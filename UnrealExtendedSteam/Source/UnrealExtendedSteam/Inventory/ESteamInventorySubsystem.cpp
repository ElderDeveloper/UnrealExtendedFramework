// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Inventory/ESteamInventorySubsystem.h"
#include "Shared/ESteamLog.h"
#include "Shared/ESteamSDK.h"
#include "Containers/Queue.h"

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END

/** Marks a queued RequestPrices call (the call takes no parameters). */
struct FESteamPendingPricesRequest
{
};

/** Parameters captured to (re-)issue a queued StartPurchase call. */
struct FESteamPendingInventoryPurchase
{
	TArray<SteamItemDef_t> Definitions;
	TArray<uint32> Quantities;
};

/** Parameters captured to (re-)issue a queued RequestEligiblePromoItemDefinitionsIDs call. */
struct FESteamPendingEligiblePromoRequest
{
	CSteamID SteamID;
};

/**
 * Native Steam callback listeners; alive only while the Steam client API is initialized.
 *
 * Result handles issued by the subsystem are tracked in PendingResults until their
 * SteamInventoryResultReady_t arrives. Every ResultReady is consumed here: the result is
 * parsed, broadcast on OnInventoryItemsReceived and ALWAYS DestroyResult'd — including
 * Steam-internal results (purchase completion, web-store purchases, out-of-band grants/trades)
 * which Steam spawns itself and are therefore never in PendingResults. Destroying every result
 * honors the SDK's mandatory contract and avoids leaking result handles. Handles still pending
 * when the holder dies are dropped without DestroyResult — the SDK may already be shut down at
 * that point and Steam reclaims result memory on SteamAPI_Shutdown anyway.
 *
 * The two call-result operations (RequestPrices, StartPurchase) each own a single CCallResult, so
 * same-type requests are serialized via an internal FIFO queue: at most one of each is in flight
 * and further requests are enqueued and issued as the previous one completes — in order, none
 * dropped. Queued-but-unissued requests hold no Steam handles, so tearing down this holder
 * abandons them cleanly.
 */
class FESteamInventoryCallbacks
{
public:
	explicit FESteamInventoryCallbacks(UESteamInventorySubsystem* InOwner)
		: Owner(InOwner)
		, ResultReadyCallback(this, &FESteamInventoryCallbacks::HandleResultReady)
		, FullUpdateCallback(this, &FESteamInventoryCallbacks::HandleFullUpdate)
		, DefinitionUpdateCallback(this, &FESteamInventoryCallbacks::HandleDefinitionUpdate)
	{
	}

	void TrackResultHandle(SteamInventoryResult_t Handle)
	{
		PendingResults.Add(Handle);
	}

	// Each Enqueue* issues the Steam call immediately when its operation is idle, otherwise it
	// queues the parameters. Returns true when the request was issued or queued; false only on the
	// immediate path when the Steam call could not be issued (preserves the public methods'
	// historical "return false when the request could not be issued" contract).

	bool EnqueuePricesRequest(const FESteamPendingPricesRequest& Request)
	{
		if (bPricesBusy)
		{
			PricesRequestQueue.Enqueue(Request);
			return true;
		}
		return IssuePricesRequest(Request);
	}

	bool EnqueuePurchase(const FESteamPendingInventoryPurchase& Request)
	{
		if (bPurchaseBusy)
		{
			PurchaseQueue.Enqueue(Request);
			return true;
		}
		return IssuePurchase(Request);
	}

	bool EnqueueEligiblePromoRequest(const FESteamPendingEligiblePromoRequest& Request)
	{
		if (bEligiblePromoBusy)
		{
			EligiblePromoQueue.Enqueue(Request);
			return true;
		}
		return IssueEligiblePromoRequest(Request);
	}

	// ---- Result serialization cache (see SerializeLastResult / CheckResultSteamID) ----

	/** Serializes the given (still-alive) result handle and stores the signed bytes as the cache. */
	void CacheSerializedResult(ISteamInventory* Inventory, SteamInventoryResult_t Handle)
	{
		uint32 BufferSize = 0;
		if (!Inventory->SerializeResult(Handle, nullptr, &BufferSize) || BufferSize == 0)
		{
			return;
		}

		TArray<uint8> Serialized;
		Serialized.SetNumUninitialized(static_cast<int32>(BufferSize));
		if (Inventory->SerializeResult(Handle, Serialized.GetData(), &BufferSize))
		{
			Serialized.SetNum(static_cast<int32>(BufferSize), EAllowShrinking::No);
			LastSerializedResult = MoveTemp(Serialized);
		}
	}

	/** Overwrites the serialized-result cache directly (used by the public DeserializeResult path). */
	void SetLastSerializedResult(const TArray<uint8>& Bytes)
	{
		LastSerializedResult = Bytes;
	}

	/** Copies the cached serialized result bytes out; false when nothing has been cached yet. */
	bool GetLastSerializedResult(TArray<uint8>& OutBuffer) const
	{
		if (LastSerializedResult.Num() == 0)
		{
			return false;
		}
		OutBuffer = LastSerializedResult;
		return true;
	}

	/**
	 * Verifies the cached serialized result belongs to Expected by deserializing it into a throwaway
	 * handle and calling CheckResultSteamID. The handle is registered in InternalResults, so the
	 * SteamInventoryResultReady_t it posts is swallowed (and the handle destroyed) by HandleResultReady
	 * WITHOUT broadcasting on OnInventoryItemsReceived. False when no result is cached or Steam is down.
	 */
	bool CheckLastResultSteamID(CSteamID Expected)
	{
		ISteamInventory* Inventory = SteamInventory();
		if (!Inventory || LastSerializedResult.Num() == 0)
		{
			return false;
		}

		SteamInventoryResult_t TempHandle = k_SteamInventoryResultInvalid;
		if (!Inventory->DeserializeResult(&TempHandle, LastSerializedResult.GetData(), static_cast<uint32>(LastSerializedResult.Num()), false))
		{
			return false;
		}

		// Mark as internal so its ResultReady is consumed silently (not broadcast) and destroyed there.
		InternalResults.Add(TempHandle);
		return Inventory->CheckResultSteamID(TempHandle, Expected);
	}

private:
	// ---- RequestPrices (serialized) ----

	bool IssuePricesRequest(const FESteamPendingPricesRequest& /*Request*/)
	{
		UESteamInventorySubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsSteamAvailable() || !SteamInventory())
		{
			return false;
		}

		const SteamAPICall_t Call = SteamInventory()->RequestPrices();
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		PricesResult.Set(Call, this, &FESteamInventoryCallbacks::HandlePricesResult);
		bPricesBusy = true;
		return true;
	}

	void DrainPricesQueue()
	{
		while (!bPricesBusy)
		{
			FESteamPendingPricesRequest Request;
			if (!PricesRequestQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssuePricesRequest(Request))
			{
				// Steam went away while draining: fail this queued request instead of dropping it.
				if (UESteamInventorySubsystem* Subsystem = Owner.Get())
				{
					Subsystem->OnPricesReceived.Broadcast(false, FString());
				}
			}
		}
	}

	// ---- StartPurchase (serialized) ----

	bool IssuePurchase(const FESteamPendingInventoryPurchase& Request)
	{
		UESteamInventorySubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsSteamAvailable() || !SteamInventory() || Request.Definitions.Num() == 0)
		{
			return false;
		}

		const SteamAPICall_t Call = SteamInventory()->StartPurchase(
			Request.Definitions.GetData(), Request.Quantities.GetData(), static_cast<uint32>(Request.Definitions.Num()));
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		PurchaseResult.Set(Call, this, &FESteamInventoryCallbacks::HandlePurchaseResult);
		bPurchaseBusy = true;
		return true;
	}

	void DrainPurchaseQueue()
	{
		while (!bPurchaseBusy)
		{
			FESteamPendingInventoryPurchase Request;
			if (!PurchaseQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssuePurchase(Request))
			{
				if (UESteamInventorySubsystem* Subsystem = Owner.Get())
				{
					Subsystem->OnPurchaseStarted.Broadcast(false, 0, 0);
				}
			}
		}
	}

	// ---- RequestEligiblePromoItemDefinitionsIDs (serialized) ----

	bool IssueEligiblePromoRequest(const FESteamPendingEligiblePromoRequest& Request)
	{
		UESteamInventorySubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsSteamAvailable() || !SteamInventory())
		{
			return false;
		}

		const SteamAPICall_t Call = SteamInventory()->RequestEligiblePromoItemDefinitionsIDs(Request.SteamID);
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		EligiblePromoResult.Set(Call, this, &FESteamInventoryCallbacks::HandleEligiblePromoResult);
		bEligiblePromoBusy = true;
		return true;
	}

	void DrainEligiblePromoQueue()
	{
		while (!bEligiblePromoBusy)
		{
			FESteamPendingEligiblePromoRequest Request;
			if (!EligiblePromoQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssueEligiblePromoRequest(Request))
			{
				if (UESteamInventorySubsystem* Subsystem = Owner.Get())
				{
					Subsystem->OnEligiblePromoItemDefsReceived.Broadcast(false, TArray<int32>());
				}
			}
		}
	}

	/**
	 * Reads one dynamic string property (by name) for the item at ItemIndex in a result set, via the
	 * two-call size pattern. Returns the value, or empty when the property is absent/unavailable.
	 * A null property name asks Steam for the comma-separated list of available names.
	 */
	static FString ReadResultItemProperty(ISteamInventory* Inventory, SteamInventoryResult_t Handle, uint32 ItemIndex, const char* PropertyNameAnsi)
	{
		uint32 ValueSize = 0;
		if (!Inventory->GetResultItemProperty(Handle, ItemIndex, PropertyNameAnsi, nullptr, &ValueSize) || ValueSize == 0)
		{
			return FString();
		}

		TArray<char> Buffer;
		Buffer.SetNumZeroed(static_cast<int32>(ValueSize) + 1);
		if (!Inventory->GetResultItemProperty(Handle, ItemIndex, PropertyNameAnsi, Buffer.GetData(), &ValueSize))
		{
			return FString();
		}
		return FString(UTF8_TO_TCHAR(Buffer.GetData()));
	}

	/**
	 * Fills OutProperties with every dynamic property of the item at ItemIndex. The available names
	 * are enumerated first (a null property name returns the comma-separated list), then each value
	 * is read individually via ReadResultItemProperty.
	 */
	static void ReadResultItemProperties(ISteamInventory* Inventory, SteamInventoryResult_t Handle, uint32 ItemIndex, TMap<FString, FString>& OutProperties)
	{
		const FString NameList = ReadResultItemProperty(Inventory, Handle, ItemIndex, nullptr);
		if (NameList.IsEmpty())
		{
			return;
		}

		TArray<FString> Names;
		NameList.ParseIntoArray(Names, TEXT(","), true);
		for (const FString& Name : Names)
		{
			const FString Trimmed = Name.TrimStartAndEnd();
			if (Trimmed.IsEmpty())
			{
				continue;
			}
			const FTCHARToUTF8 NameUtf8(*Trimmed);
			OutProperties.Add(Trimmed, ReadResultItemProperty(Inventory, Handle, ItemIndex, reinterpret_cast<const char*>(NameUtf8.Get())));
		}
	}

	/**
	 * Copies a ready result set into Items via the two-call size pattern. Returns false only if an
	 * SDK query failed; a zero-item result is a valid success (Items stays empty). Shared by the
	 * tracked and untracked (Steam-internal) ResultReady paths so both parse identically. Each item's
	 * dynamic properties are read into FESteamInventoryItem::Properties.
	 */
	static bool ParseResultItems(ISteamInventory* Inventory, SteamInventoryResult_t Handle, TArray<FESteamInventoryItem>& Items)
	{
		// Two-call size pattern: first query the item count, then fill the array.
		uint32 ItemCount = 0;
		if (!Inventory->GetResultItems(Handle, nullptr, &ItemCount))
		{
			return false;
		}
		if (ItemCount == 0)
		{
			return true;
		}

		TArray<SteamItemDetails_t> Details;
		Details.SetNumUninitialized(static_cast<int32>(ItemCount));
		if (!Inventory->GetResultItems(Handle, Details.GetData(), &ItemCount))
		{
			return false;
		}

		Items.Reserve(static_cast<int32>(ItemCount));
		for (uint32 Index = 0; Index < ItemCount; ++Index)
		{
			FESteamInventoryItem& Item = Items.AddDefaulted_GetRef();
			// SteamItemInstanceID_t is uint64; Blueprint only has int64, so the bits are
			// reinterpreted. The value round-trips through GetItemsById.
			Item.ItemId = static_cast<int64>(Details[Index].m_itemId);
			Item.Definition = Details[Index].m_iDefinition;
			Item.Quantity = static_cast<int32>(Details[Index].m_unQuantity);
			// Raw status/action bitmask; test against ESteamItemFlags (no-trade/removed/consumed).
			Item.Flags = static_cast<int32>(Details[Index].m_unFlags);
			// Dynamic string properties for this item instance (empty when it carries none).
			ReadResultItemProperties(Inventory, Handle, Index, Item.Properties);
		}
		return true;
	}

	void HandleResultReady(SteamInventoryResultReady_t* Data)
	{
		// Internal check handles (created by CheckLastResultSteamID) carry no user-facing payload:
		// swallow their callback silently and destroy the handle without broadcasting.
		if (InternalResults.Remove(Data->m_handle) > 0)
		{
			if (ISteamInventory* Inventory = SteamInventory())
			{
				Inventory->DestroyResult(Data->m_handle);
			}
			return;
		}

		// A handle this subsystem issued (GetAllItems, GetItemsById, TriggerItemDrop, ConsumeItem)
		// is tracked here; drop it from the set. A handle we did NOT issue is a Steam-internal
		// result — purchase completion (StartPurchase), web-store purchase, or an out-of-band
		// grant/trade — which Steam spawns itself and never puts in PendingResults. These arrive
		// untracked and are now consumed too: previously they were early-returned, so they were
		// never parsed, never broadcast, and never DestroyResult'd, leaking the handle and
		// violating the SDK's mandatory destroy contract (isteaminventory.h).
		const bool bTracked = PendingResults.Remove(Data->m_handle) > 0;

		ISteamInventory* Inventory = SteamInventory();

		// Tracked handles keep their existing behavior (status from the callback). Untracked
		// handles are queried via GetResultStatus. Both are equivalent per the SDK.
		const EResult Status = bTracked
			? Data->m_result
			: (Inventory ? Inventory->GetResultStatus(Data->m_handle) : k_EResultFail);

		TArray<FESteamInventoryItem> Items;
		const bool bSuccess = Inventory != nullptr && Status == k_EResultOK
			&& ParseResultItems(Inventory, Data->m_handle, Items);

		// Cache the signed serialization of tracked results while the handle is still alive, so
		// SerializeLastResult / CheckResultSteamID can work after the handle is destroyed below.
		// Only tracked results (ones this subsystem issued) are serialized; Steam-internal results
		// are left alone. A serialize failure just leaves the previous cache untouched here — it is
		// overwritten by CacheSerializedResult only on success.
		if (bTracked && Inventory && Status == k_EResultOK)
		{
			CacheSerializedResult(Inventory, Data->m_handle);
		}

		// Honor the SDK contract: ALWAYS destroy any result Steam handed us — tracked or not, on
		// success, failure or non-OK status — so its memory is freed and the handle does not leak.
		if (Inventory)
		{
			Inventory->DestroyResult(Data->m_handle);
		}

		if (UESteamInventorySubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnInventoryItemsReceived.Broadcast(bSuccess, Items);
		}
	}

	void HandleFullUpdate(SteamInventoryFullUpdate_t* /*Data*/)
	{
		// Steam reports the inventory changed out-of-band. Per the SDK a full ResultReady with the
		// fresh set is posted immediately afterwards (handled above); this is an additional
		// convenience notification. Consumers can react by re-querying or consume that ResultReady.
		if (UESteamInventorySubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnInventoryUpdated.Broadcast();
		}
	}

	void HandleDefinitionUpdate(SteamInventoryDefinitionUpdate_t* /*Data*/)
	{
		if (UESteamInventorySubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnItemDefinitionsUpdated.Broadcast();
		}
	}

	void HandlePricesResult(SteamInventoryRequestPricesResult_t* Data, bool bIOFailure)
	{
		if (UESteamInventorySubsystem* Subsystem = Owner.Get())
		{
			const bool bSuccess = !bIOFailure && Data->m_result == k_EResultOK;
			// m_rgchCurrency is a null-terminated 3-letter ISO 4217 code.
			const FString Currency = bSuccess ? FString(UTF8_TO_TCHAR(Data->m_rgchCurrency)) : FString();
			Subsystem->OnPricesReceived.Broadcast(bSuccess, Currency);
		}
		bPricesBusy = false;
		DrainPricesQueue();
	}

	void HandlePurchaseResult(SteamInventoryStartPurchaseResult_t* Data, bool bIOFailure)
	{
		if (UESteamInventorySubsystem* Subsystem = Owner.Get())
		{
			const bool bSuccess = !bIOFailure && Data->m_result == k_EResultOK;
			Subsystem->OnPurchaseStarted.Broadcast(bSuccess,
				static_cast<int64>(Data->m_ulOrderID), static_cast<int64>(Data->m_ulTransID));
		}
		bPurchaseBusy = false;
		DrainPurchaseQueue();
	}

	void HandleEligiblePromoResult(SteamInventoryEligiblePromoItemDefIDs_t* Data, bool bIOFailure)
	{
		if (UESteamInventorySubsystem* Subsystem = Owner.Get())
		{
			TArray<int32> DefIds;
			const bool bSuccess = !bIOFailure && Data->m_result == k_EResultOK;
			if (bSuccess && Data->m_numEligiblePromoItemDefs > 0 && SteamInventory())
			{
				// Pull the ids Steam cached for this user (the callback only carries the count).
				uint32 Count = static_cast<uint32>(Data->m_numEligiblePromoItemDefs);
				TArray<SteamItemDef_t> Ids;
				Ids.SetNumUninitialized(static_cast<int32>(Count));
				if (SteamInventory()->GetEligiblePromoItemDefinitionIDs(Data->m_steamID, Ids.GetData(), &Count))
				{
					DefIds.Reserve(static_cast<int32>(Count));
					for (uint32 Index = 0; Index < Count; ++Index)
					{
						DefIds.Add(static_cast<int32>(Ids[Index]));
					}
				}
			}
			Subsystem->OnEligiblePromoItemDefsReceived.Broadcast(bSuccess, DefIds);
		}
		bEligiblePromoBusy = false;
		DrainEligiblePromoQueue();
	}

	TWeakObjectPtr<UESteamInventorySubsystem> Owner;
	TSet<SteamInventoryResult_t> PendingResults;

	// Throwaway handles created by CheckLastResultSteamID; their ResultReady is consumed silently.
	TSet<SteamInventoryResult_t> InternalResults;

	// Signed serialization of the most recent tracked result (see CacheSerializedResult), kept so
	// SerializeLastResult / CheckResultSteamID keep working after the source handle is destroyed.
	TArray<uint8> LastSerializedResult;

	CCallback<FESteamInventoryCallbacks, SteamInventoryResultReady_t> ResultReadyCallback;
	CCallback<FESteamInventoryCallbacks, SteamInventoryFullUpdate_t> FullUpdateCallback;
	CCallback<FESteamInventoryCallbacks, SteamInventoryDefinitionUpdate_t> DefinitionUpdateCallback;
	CCallResult<FESteamInventoryCallbacks, SteamInventoryRequestPricesResult_t> PricesResult;
	CCallResult<FESteamInventoryCallbacks, SteamInventoryStartPurchaseResult_t> PurchaseResult;
	CCallResult<FESteamInventoryCallbacks, SteamInventoryEligiblePromoItemDefIDs_t> EligiblePromoResult;

	// In-flight flags + FIFO queues, one per serialized call-result operation type.
	bool bPricesBusy = false;
	bool bPurchaseBusy = false;
	bool bEligiblePromoBusy = false;

	TQueue<FESteamPendingPricesRequest> PricesRequestQueue;
	TQueue<FESteamPendingInventoryPurchase> PurchaseQueue;
	TQueue<FESteamPendingEligiblePromoRequest> EligiblePromoQueue;
};
#else
class FESteamInventoryCallbacks
{
};
#endif // WITH_EXTENDEDSTEAM_SDK

void UESteamInventorySubsystem::Deinitialize()
{
	Super::Deinitialize();
	Callbacks.Reset();
}

void UESteamInventorySubsystem::HandleSteamClientInitialized()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!Callbacks)
	{
		Callbacks = MakeShared<FESteamInventoryCallbacks>(this);
	}
#endif
}

void UESteamInventorySubsystem::HandleSteamClientShutdown()
{
	Callbacks.Reset();
}

bool UESteamInventorySubsystem::GetAllItems()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamInventory() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("GetAllItems"));
		return false;
	}

	SteamInventoryResult_t ResultHandle = k_SteamInventoryResultInvalid;
	if (!SteamInventory()->GetAllItems(&ResultHandle))
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("GetAllItems failed (inventory unavailable)"));
		return false;
	}

	Callbacks->TrackResultHandle(ResultHandle);
	return true;
#else
	LogSteamUnavailable(TEXT("GetAllItems"));
	return false;
#endif
}

bool UESteamInventorySubsystem::GetItemsById(const TArray<int64>& ItemIds)
{
	if (ItemIds.Num() == 0)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("GetItemsById: no item ids provided"));
		return false;
	}

#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamInventory() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("GetItemsById"));
		return false;
	}

	// int64 -> SteamItemInstanceID_t (uint64): reinterprets the bits of ids that round-tripped
	// through Blueprint; see FESteamInventoryItem::ItemId.
	TArray<SteamItemInstanceID_t> InstanceIds;
	InstanceIds.Reserve(ItemIds.Num());
	for (const int64 ItemId : ItemIds)
	{
		InstanceIds.Add(static_cast<SteamItemInstanceID_t>(ItemId));
	}

	SteamInventoryResult_t ResultHandle = k_SteamInventoryResultInvalid;
	if (!SteamInventory()->GetItemsByID(&ResultHandle, InstanceIds.GetData(), static_cast<uint32>(InstanceIds.Num())))
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("GetItemsById failed (inventory unavailable)"));
		return false;
	}

	Callbacks->TrackResultHandle(ResultHandle);
	return true;
#else
	LogSteamUnavailable(TEXT("GetItemsById"));
	return false;
#endif
}

bool UESteamInventorySubsystem::TriggerItemDrop(int32 ItemDefinition)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamInventory() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("TriggerItemDrop"));
		return false;
	}

	SteamInventoryResult_t ResultHandle = k_SteamInventoryResultInvalid;
	if (!SteamInventory()->TriggerItemDrop(&ResultHandle, static_cast<SteamItemDef_t>(ItemDefinition)))
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("TriggerItemDrop failed for definition %d"), ItemDefinition);
		return false;
	}

	Callbacks->TrackResultHandle(ResultHandle);
	return true;
#else
	LogSteamUnavailable(TEXT("TriggerItemDrop"));
	return false;
#endif
}

bool UESteamInventorySubsystem::ConsumeItem(int64 ItemId, int32 Quantity)
{
	if (Quantity <= 0)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("ConsumeItem: Quantity must be positive (got %d)"), Quantity);
		return false;
	}

#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamInventory() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("ConsumeItem"));
		return false;
	}

	SteamInventoryResult_t ResultHandle = k_SteamInventoryResultInvalid;
	// int64 -> SteamItemInstanceID_t (uint64): see FESteamInventoryItem::ItemId.
	if (!SteamInventory()->ConsumeItem(&ResultHandle, static_cast<SteamItemInstanceID_t>(ItemId), static_cast<uint32>(Quantity)))
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("ConsumeItem failed for item %lld"), ItemId);
		return false;
	}

	Callbacks->TrackResultHandle(ResultHandle);
	return true;
#else
	LogSteamUnavailable(TEXT("ConsumeItem"));
	return false;
#endif
}

bool UESteamInventorySubsystem::LoadItemDefinitions()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamInventory())
	{
		LogSteamUnavailable(TEXT("LoadItemDefinitions"));
		return false;
	}
	return SteamInventory()->LoadItemDefinitions();
#else
	LogSteamUnavailable(TEXT("LoadItemDefinitions"));
	return false;
#endif
}

FString UESteamInventorySubsystem::GetItemDefinitionProperty(int32 Definition, const FString& PropertyName) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamInventory())
	{
		return FString();
	}

	// A null property name asks Steam for the comma-separated list of available names.
	// Named converter: the UTF-8 buffer must outlive both GetItemDefinitionProperty calls.
	const FTCHARToUTF8 PropertyNameUtf8(*PropertyName);
	const char* PropertyNameAnsi = PropertyName.IsEmpty() ? nullptr : reinterpret_cast<const char*>(PropertyNameUtf8.Get());

	// Two-call size pattern: first query the required buffer size, then fetch the value.
	uint32 ValueSize = 0;
	if (!SteamInventory()->GetItemDefinitionProperty(static_cast<SteamItemDef_t>(Definition), PropertyNameAnsi, nullptr, &ValueSize) || ValueSize == 0)
	{
		return FString();
	}

	TArray<char> Buffer;
	Buffer.SetNumZeroed(static_cast<int32>(ValueSize) + 1);
	if (!SteamInventory()->GetItemDefinitionProperty(static_cast<SteamItemDef_t>(Definition), PropertyNameAnsi, Buffer.GetData(), &ValueSize))
	{
		return FString();
	}

	return FString(UTF8_TO_TCHAR(Buffer.GetData()));
#else
	return FString();
#endif
}

bool UESteamInventorySubsystem::RequestPrices()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamInventory() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("RequestPrices"));
		return false;
	}

	// Issued now if idle, otherwise queued behind the in-flight prices request (never dropped).
	return Callbacks->EnqueuePricesRequest(FESteamPendingPricesRequest{});
#else
	LogSteamUnavailable(TEXT("RequestPrices"));
	return false;
#endif
}

bool UESteamInventorySubsystem::GetItemPrice(int32 Definition, int64& OutCurrentPrice, int64& OutBasePrice) const
{
	OutCurrentPrice = 0;
	OutBasePrice = 0;
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamInventory())
	{
		return false;
	}

	uint64 CurrentPrice = 0;
	uint64 BasePrice = 0;
	if (!SteamInventory()->GetItemPrice(static_cast<SteamItemDef_t>(Definition), &CurrentPrice, &BasePrice))
	{
		return false;
	}

	OutCurrentPrice = static_cast<int64>(CurrentPrice);
	OutBasePrice = static_cast<int64>(BasePrice);
	return true;
#else
	return false;
#endif
}

bool UESteamInventorySubsystem::StartPurchase(const TArray<int32>& Definitions, const TArray<int32>& Quantities)
{
	if (Definitions.Num() == 0 || Definitions.Num() != Quantities.Num())
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("StartPurchase: Definitions and Quantities must be non-empty arrays of equal length (%d vs %d)"),
			Definitions.Num(), Quantities.Num());
		return false;
	}

#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamInventory() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("StartPurchase"));
		return false;
	}

	FESteamPendingInventoryPurchase Request;
	Request.Definitions.Reserve(Definitions.Num());
	Request.Quantities.Reserve(Quantities.Num());
	for (int32 Index = 0; Index < Definitions.Num(); ++Index)
	{
		Request.Definitions.Add(static_cast<SteamItemDef_t>(Definitions[Index]));
		Request.Quantities.Add(static_cast<uint32>(FMath::Max(Quantities[Index], 0)));
	}

	// Issued now if idle, otherwise queued behind the in-flight purchase (never dropped).
	return Callbacks->EnqueuePurchase(Request);
#else
	LogSteamUnavailable(TEXT("StartPurchase"));
	return false;
#endif
}

bool UESteamInventorySubsystem::ExchangeItems(const TArray<int32>& GenerateDefs, const TArray<int32>& GenerateQty, const TArray<int64>& DestroyItemIds, const TArray<int32>& DestroyQty)
{
	if (GenerateDefs.Num() != GenerateQty.Num() || DestroyItemIds.Num() != DestroyQty.Num())
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("ExchangeItems: generate arrays (%d/%d) and destroy arrays (%d/%d) must each be equal length"),
			GenerateDefs.Num(), GenerateQty.Num(), DestroyItemIds.Num(), DestroyQty.Num());
		return false;
	}

#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamInventory() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("ExchangeItems"));
		return false;
	}

	TArray<SteamItemDef_t> Generate;
	TArray<uint32> GenerateQuantities;
	Generate.Reserve(GenerateDefs.Num());
	GenerateQuantities.Reserve(GenerateQty.Num());
	for (int32 Index = 0; Index < GenerateDefs.Num(); ++Index)
	{
		Generate.Add(static_cast<SteamItemDef_t>(GenerateDefs[Index]));
		GenerateQuantities.Add(static_cast<uint32>(FMath::Max(GenerateQty[Index], 0)));
	}

	TArray<SteamItemInstanceID_t> Destroy;
	TArray<uint32> DestroyQuantities;
	Destroy.Reserve(DestroyItemIds.Num());
	DestroyQuantities.Reserve(DestroyQty.Num());
	for (int32 Index = 0; Index < DestroyItemIds.Num(); ++Index)
	{
		// int64 -> SteamItemInstanceID_t (uint64): see FESteamInventoryItem::ItemId.
		Destroy.Add(static_cast<SteamItemInstanceID_t>(DestroyItemIds[Index]));
		DestroyQuantities.Add(static_cast<uint32>(FMath::Max(DestroyQty[Index], 0)));
	}

	SteamInventoryResult_t ResultHandle = k_SteamInventoryResultInvalid;
	if (!SteamInventory()->ExchangeItems(&ResultHandle,
		Generate.GetData(), GenerateQuantities.GetData(), static_cast<uint32>(Generate.Num()),
		Destroy.GetData(), DestroyQuantities.GetData(), static_cast<uint32>(Destroy.Num())))
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("ExchangeItems failed (inventory unavailable or invalid recipe)"));
		return false;
	}

	Callbacks->TrackResultHandle(ResultHandle);
	return true;
#else
	LogSteamUnavailable(TEXT("ExchangeItems"));
	return false;
#endif
}

bool UESteamInventorySubsystem::TransferItemQuantity(int64 SourceItemId, int32 Quantity, int64 DestItemId)
{
	if (Quantity <= 0)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("TransferItemQuantity: Quantity must be positive (got %d)"), Quantity);
		return false;
	}

#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamInventory() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("TransferItemQuantity"));
		return false;
	}

	// A DestItemId of 0 means "split into a new stack" (k_SteamItemInstanceIDInvalid).
	const SteamItemInstanceID_t Destination = DestItemId == 0
		? k_SteamItemInstanceIDInvalid
		: static_cast<SteamItemInstanceID_t>(DestItemId);

	SteamInventoryResult_t ResultHandle = k_SteamInventoryResultInvalid;
	if (!SteamInventory()->TransferItemQuantity(&ResultHandle,
		static_cast<SteamItemInstanceID_t>(SourceItemId), static_cast<uint32>(Quantity), Destination))
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("TransferItemQuantity failed for item %lld"), SourceItemId);
		return false;
	}

	Callbacks->TrackResultHandle(ResultHandle);
	return true;
#else
	LogSteamUnavailable(TEXT("TransferItemQuantity"));
	return false;
#endif
}

bool UESteamInventorySubsystem::GenerateItems(const TArray<int32>& Defs, const TArray<int32>& Quantities)
{
	if (Defs.Num() == 0 || Defs.Num() != Quantities.Num())
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("GenerateItems: Defs and Quantities must be non-empty arrays of equal length (%d vs %d)"),
			Defs.Num(), Quantities.Num());
		return false;
	}

#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamInventory() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("GenerateItems"));
		return false;
	}

	TArray<SteamItemDef_t> ItemDefs;
	TArray<uint32> ItemQuantities;
	ItemDefs.Reserve(Defs.Num());
	ItemQuantities.Reserve(Quantities.Num());
	for (int32 Index = 0; Index < Defs.Num(); ++Index)
	{
		ItemDefs.Add(static_cast<SteamItemDef_t>(Defs[Index]));
		ItemQuantities.Add(static_cast<uint32>(FMath::Max(Quantities[Index], 0)));
	}

	SteamInventoryResult_t ResultHandle = k_SteamInventoryResultInvalid;
	if (!SteamInventory()->GenerateItems(&ResultHandle, ItemDefs.GetData(), ItemQuantities.GetData(), static_cast<uint32>(ItemDefs.Num())))
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("GenerateItems failed (requires a publisher-group account; prototyping only)"));
		return false;
	}

	Callbacks->TrackResultHandle(ResultHandle);
	return true;
#else
	LogSteamUnavailable(TEXT("GenerateItems"));
	return false;
#endif
}

bool UESteamInventorySubsystem::GrantPromoItems()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamInventory() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("GrantPromoItems"));
		return false;
	}

	SteamInventoryResult_t ResultHandle = k_SteamInventoryResultInvalid;
	if (!SteamInventory()->GrantPromoItems(&ResultHandle))
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("GrantPromoItems failed (inventory unavailable)"));
		return false;
	}

	Callbacks->TrackResultHandle(ResultHandle);
	return true;
#else
	LogSteamUnavailable(TEXT("GrantPromoItems"));
	return false;
#endif
}

bool UESteamInventorySubsystem::AddPromoItem(int32 Def)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamInventory() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("AddPromoItem"));
		return false;
	}

	SteamInventoryResult_t ResultHandle = k_SteamInventoryResultInvalid;
	if (!SteamInventory()->AddPromoItem(&ResultHandle, static_cast<SteamItemDef_t>(Def)))
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("AddPromoItem failed for definition %d"), Def);
		return false;
	}

	Callbacks->TrackResultHandle(ResultHandle);
	return true;
#else
	LogSteamUnavailable(TEXT("AddPromoItem"));
	return false;
#endif
}

bool UESteamInventorySubsystem::AddPromoItems(const TArray<int32>& Defs)
{
	if (Defs.Num() == 0)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("AddPromoItems: no definitions provided"));
		return false;
	}

#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamInventory() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("AddPromoItems"));
		return false;
	}

	TArray<SteamItemDef_t> ItemDefs;
	ItemDefs.Reserve(Defs.Num());
	for (const int32 Def : Defs)
	{
		ItemDefs.Add(static_cast<SteamItemDef_t>(Def));
	}

	SteamInventoryResult_t ResultHandle = k_SteamInventoryResultInvalid;
	if (!SteamInventory()->AddPromoItems(&ResultHandle, ItemDefs.GetData(), static_cast<uint32>(ItemDefs.Num())))
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("AddPromoItems failed (inventory unavailable)"));
		return false;
	}

	Callbacks->TrackResultHandle(ResultHandle);
	return true;
#else
	LogSteamUnavailable(TEXT("AddPromoItems"));
	return false;
#endif
}

bool UESteamInventorySubsystem::RequestEligiblePromoItemDefinitionsIDs(FESteamId SteamId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamInventory() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("RequestEligiblePromoItemDefinitionsIDs"));
		return false;
	}

	FESteamPendingEligiblePromoRequest Request;
	Request.SteamID = CSteamID(SteamId.Value);
	// Issued now if idle, otherwise queued behind the in-flight request (never dropped).
	return Callbacks->EnqueueEligiblePromoRequest(Request);
#else
	LogSteamUnavailable(TEXT("RequestEligiblePromoItemDefinitionsIDs"));
	return false;
#endif
}

bool UESteamInventorySubsystem::GetEligiblePromoItemDefinitionIDs(FESteamId SteamId, TArray<int32>& OutDefIds) const
{
	OutDefIds.Reset();
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamInventory())
	{
		return false;
	}

	const CSteamID Id(SteamId.Value);

	// Two-call size pattern: first query the count, then fill the array.
	uint32 Count = 0;
	if (!SteamInventory()->GetEligiblePromoItemDefinitionIDs(Id, nullptr, &Count))
	{
		return false;
	}
	if (Count == 0)
	{
		return true;
	}

	TArray<SteamItemDef_t> Ids;
	Ids.SetNumUninitialized(static_cast<int32>(Count));
	if (!SteamInventory()->GetEligiblePromoItemDefinitionIDs(Id, Ids.GetData(), &Count))
	{
		return false;
	}

	OutDefIds.Reserve(static_cast<int32>(Count));
	for (uint32 Index = 0; Index < Count; ++Index)
	{
		OutDefIds.Add(static_cast<int32>(Ids[Index]));
	}
	return true;
#else
	return false;
#endif
}

int64 UESteamInventorySubsystem::StartUpdateProperties()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamInventory())
	{
		LogSteamUnavailable(TEXT("StartUpdateProperties"));
		return static_cast<int64>(k_SteamInventoryUpdateHandleInvalid);
	}
	// SteamInventoryUpdateHandle_t (uint64) -> int64: opaque bits, fed back to the SetProperty* calls.
	return static_cast<int64>(SteamInventory()->StartUpdateProperties());
#else
	LogSteamUnavailable(TEXT("StartUpdateProperties"));
	return static_cast<int64>(0xffffffffffffffffull);
#endif
}

bool UESteamInventorySubsystem::SetPropertyBool(int64 UpdateHandle, int64 ItemId, const FString& PropertyName, bool bValue)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamInventory())
	{
		LogSteamUnavailable(TEXT("SetPropertyBool"));
		return false;
	}
	return SteamInventory()->SetProperty(static_cast<SteamInventoryUpdateHandle_t>(UpdateHandle),
		static_cast<SteamItemInstanceID_t>(ItemId), TCHAR_TO_UTF8(*PropertyName), bValue);
#else
	LogSteamUnavailable(TEXT("SetPropertyBool"));
	return false;
#endif
}

bool UESteamInventorySubsystem::SetPropertyInt64(int64 UpdateHandle, int64 ItemId, const FString& PropertyName, int64 Value)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamInventory())
	{
		LogSteamUnavailable(TEXT("SetPropertyInt64"));
		return false;
	}
	return SteamInventory()->SetProperty(static_cast<SteamInventoryUpdateHandle_t>(UpdateHandle),
		static_cast<SteamItemInstanceID_t>(ItemId), TCHAR_TO_UTF8(*PropertyName), static_cast<int64>(Value));
#else
	LogSteamUnavailable(TEXT("SetPropertyInt64"));
	return false;
#endif
}

bool UESteamInventorySubsystem::SetPropertyFloat(int64 UpdateHandle, int64 ItemId, const FString& PropertyName, float Value)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamInventory())
	{
		LogSteamUnavailable(TEXT("SetPropertyFloat"));
		return false;
	}
	return SteamInventory()->SetProperty(static_cast<SteamInventoryUpdateHandle_t>(UpdateHandle),
		static_cast<SteamItemInstanceID_t>(ItemId), TCHAR_TO_UTF8(*PropertyName), Value);
#else
	LogSteamUnavailable(TEXT("SetPropertyFloat"));
	return false;
#endif
}

bool UESteamInventorySubsystem::SetPropertyString(int64 UpdateHandle, int64 ItemId, const FString& PropertyName, const FString& Value)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamInventory())
	{
		LogSteamUnavailable(TEXT("SetPropertyString"));
		return false;
	}
	// Named converters: both UTF-8 buffers must outlive the SetProperty call.
	const FTCHARToUTF8 PropertyNameUtf8(*PropertyName);
	const FTCHARToUTF8 ValueUtf8(*Value);
	return SteamInventory()->SetProperty(static_cast<SteamInventoryUpdateHandle_t>(UpdateHandle),
		static_cast<SteamItemInstanceID_t>(ItemId),
		reinterpret_cast<const char*>(PropertyNameUtf8.Get()), reinterpret_cast<const char*>(ValueUtf8.Get()));
#else
	LogSteamUnavailable(TEXT("SetPropertyString"));
	return false;
#endif
}

bool UESteamInventorySubsystem::RemoveProperty(int64 UpdateHandle, int64 ItemId, const FString& PropertyName)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamInventory())
	{
		LogSteamUnavailable(TEXT("RemoveProperty"));
		return false;
	}
	return SteamInventory()->RemoveProperty(static_cast<SteamInventoryUpdateHandle_t>(UpdateHandle),
		static_cast<SteamItemInstanceID_t>(ItemId), TCHAR_TO_UTF8(*PropertyName));
#else
	LogSteamUnavailable(TEXT("RemoveProperty"));
	return false;
#endif
}

bool UESteamInventorySubsystem::SubmitUpdateProperties(int64 UpdateHandle)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamInventory() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("SubmitUpdateProperties"));
		return false;
	}

	SteamInventoryResult_t ResultHandle = k_SteamInventoryResultInvalid;
	if (!SteamInventory()->SubmitUpdateProperties(static_cast<SteamInventoryUpdateHandle_t>(UpdateHandle), &ResultHandle))
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("SubmitUpdateProperties failed (invalid update handle?)"));
		return false;
	}

	Callbacks->TrackResultHandle(ResultHandle);
	return true;
#else
	LogSteamUnavailable(TEXT("SubmitUpdateProperties"));
	return false;
#endif
}

bool UESteamInventorySubsystem::SerializeLastResult(TArray<uint8>& OutBuffer) const
{
	OutBuffer.Reset();
#if WITH_EXTENDEDSTEAM_SDK
	if (!Callbacks)
	{
		return false;
	}
	return Callbacks->GetLastSerializedResult(OutBuffer);
#else
	return false;
#endif
}

bool UESteamInventorySubsystem::DeserializeResult(const TArray<uint8>& Buffer)
{
	if (Buffer.Num() == 0)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("DeserializeResult: empty buffer"));
		return false;
	}

#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamInventory() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("DeserializeResult"));
		return false;
	}

	SteamInventoryResult_t ResultHandle = k_SteamInventoryResultInvalid;
	if (!SteamInventory()->DeserializeResult(&ResultHandle, Buffer.GetData(), static_cast<uint32>(Buffer.Num()), false))
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("DeserializeResult failed (bad signature or Steam offline)"));
		return false;
	}

	// The input buffer IS the serialization of "the last result" now, so cache it for a subsequent
	// CheckResultSteamID (which cannot use the handle below — it is destroyed once its ResultReady
	// completes). Tracked like any other handle: its ResultReady parses + broadcasts on
	// OnInventoryItemsReceived and destroys the handle.
	Callbacks->SetLastSerializedResult(Buffer);
	Callbacks->TrackResultHandle(ResultHandle);
	return true;
#else
	LogSteamUnavailable(TEXT("DeserializeResult"));
	return false;
#endif
}

bool UESteamInventorySubsystem::CheckResultSteamID(FESteamId ExpectedSteamId) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamInventory() || !Callbacks)
	{
		return false;
	}
	return Callbacks->CheckLastResultSteamID(CSteamID(ExpectedSteamId.Value));
#else
	return false;
#endif
}

bool UESteamInventorySubsystem::GetItemDefinitionIDs(TArray<int32>& OutDefIds) const
{
	OutDefIds.Reset();
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamInventory())
	{
		return false;
	}

	// Two-call size pattern: first query the count, then fill the array.
	uint32 Count = 0;
	if (!SteamInventory()->GetItemDefinitionIDs(nullptr, &Count))
	{
		return false;
	}
	if (Count == 0)
	{
		return true;
	}

	TArray<SteamItemDef_t> Ids;
	Ids.SetNumUninitialized(static_cast<int32>(Count));
	if (!SteamInventory()->GetItemDefinitionIDs(Ids.GetData(), &Count))
	{
		return false;
	}

	OutDefIds.Reserve(static_cast<int32>(Count));
	for (uint32 Index = 0; Index < Count; ++Index)
	{
		OutDefIds.Add(static_cast<int32>(Ids[Index]));
	}
	return true;
#else
	return false;
#endif
}

int32 UESteamInventorySubsystem::GetNumItemsWithPrices() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamInventory())
	{
		return static_cast<int32>(SteamInventory()->GetNumItemsWithPrices());
	}
#endif
	return 0;
}

bool UESteamInventorySubsystem::GetItemsWithPrices(TArray<int32>& OutDefs, TArray<int64>& OutPrices, TArray<int64>& OutBasePrices) const
{
	OutDefs.Reset();
	OutPrices.Reset();
	OutBasePrices.Reset();
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamInventory())
	{
		return false;
	}

	const uint32 Count = SteamInventory()->GetNumItemsWithPrices();
	if (Count == 0)
	{
		// No definitions are currently priced (a valid, empty success).
		return true;
	}

	TArray<SteamItemDef_t> Defs;
	TArray<uint64> CurrentPrices;
	TArray<uint64> BasePrices;
	Defs.SetNumUninitialized(static_cast<int32>(Count));
	CurrentPrices.SetNumUninitialized(static_cast<int32>(Count));
	BasePrices.SetNumUninitialized(static_cast<int32>(Count));
	if (!SteamInventory()->GetItemsWithPrices(Defs.GetData(), CurrentPrices.GetData(), BasePrices.GetData(), Count))
	{
		return false;
	}

	OutDefs.Reserve(static_cast<int32>(Count));
	OutPrices.Reserve(static_cast<int32>(Count));
	OutBasePrices.Reserve(static_cast<int32>(Count));
	for (uint32 Index = 0; Index < Count; ++Index)
	{
		OutDefs.Add(static_cast<int32>(Defs[Index]));
		OutPrices.Add(static_cast<int64>(CurrentPrices[Index]));
		OutBasePrices.Add(static_cast<int64>(BasePrices[Index]));
	}
	return true;
#else
	return false;
#endif
}
