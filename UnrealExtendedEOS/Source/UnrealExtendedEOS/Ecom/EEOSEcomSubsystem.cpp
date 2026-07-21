// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSEcomSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Interfaces/OnlineStoreInterfaceV2.h"
#include "Interfaces/OnlinePurchaseInterface.h"
#include "Interfaces/OnlineEntitlementsInterface.h"
#include "UnrealExtendedEOS.h"

void UEEOSEcomSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEEOSEcomSubsystem::Deinitialize()
{
	// Remove any still-registered per-operation bindings from the interface-wide list
	if (EntitlementsQueryHandle.IsValid() || OwnershipQueryHandle.IsValid())
	{
		if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
		{
			IOnlineEntitlementsPtr EntitlementsInterface = EOSSub->GetEntitlementsInterface();
			if (EntitlementsInterface.IsValid())
			{
				EntitlementsInterface->OnQueryEntitlementsCompleteDelegates.Remove(EntitlementsQueryHandle);
				EntitlementsInterface->OnQueryEntitlementsCompleteDelegates.Remove(OwnershipQueryHandle);
			}
		}
		EntitlementsQueryHandle.Reset();
		OwnershipQueryHandle.Reset();
	}

	CachedOffers.Empty();
	CachedEntitlements.Empty();
	CachedOwnership.Empty();
	Super::Deinitialize();
}

// ── Offers / Catalog ─────────────────────────────────────────────────────────

bool UEEOSEcomSubsystem::QueryOffers()
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("QueryOffers"));
		OnOffersQueried.Broadcast(false, TArray<FEEOSCatalogOffer>());
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineStoreV2Ptr StoreInterface = EOSSub->GetStoreV2Interface();
	if (!StoreInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSEcomSubsystem::QueryOffers — StoreV2 interface not available"));
		OnOffersQueried.Broadcast(false, TArray<FEEOSCatalogOffer>());
		return false;
	}

	FUniqueNetIdPtr UserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
	if (!UserId.IsValid())
	{
		OnOffersQueried.Broadcast(false, TArray<FEEOSCatalogOffer>());
		return false;
	}

	// FOnQueryOnlineStoreOffersComplete is a regular delegate — weak-bind so the callback
	// is skipped if this subsystem is destroyed while the query is in flight
	StoreInterface->QueryOffersById(*UserId, TArray<FUniqueOfferId>(),
		FOnQueryOnlineStoreOffersComplete::CreateWeakLambda(this,
			[this, StoreInterface](bool bWasSuccessful, const TArray<FUniqueOfferId>& OfferIds, const FString& Error)
			{
				CachedOffers.Empty();

				if (bWasSuccessful)
				{
					TArray<FOnlineStoreOfferRef> Offers;
					StoreInterface->GetOffers(Offers);

					for (const auto& Offer : Offers)
					{
						FEEOSCatalogOffer EInfo;
						EInfo.OfferId = Offer->OfferId;
						EInfo.Title = Offer->Title.ToString();
						EInfo.Description = Offer->Description.ToString();
						EInfo.LongDescription = Offer->LongDescription.ToString();
						EInfo.PriceInSmallestUnit = Offer->NumericPrice;
						EInfo.OriginalPriceInSmallestUnit = Offer->RegularPrice;
						EInfo.FormattedPrice = Offer->GetDisplayPrice().ToString();
						EInfo.CurrencyCode = Offer->CurrencyCode;
						EInfo.bAvailableForPurchase = Offer->IsPurchaseable();
						CachedOffers.Add(EInfo);
					}

					UE_LOG(LogExtendedEOS, Log, TEXT("EEOSEcomSubsystem: Queried %d offers"), CachedOffers.Num());
				}
				else
				{
					UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSEcomSubsystem: QueryOffers failed — %s"), *Error);
				}

				OnOffersQueried.Broadcast(bWasSuccessful, CachedOffers);
			}));

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSEcomSubsystem::QueryOffers — Querying catalog..."));
	return true;
}

TArray<FEEOSCatalogOffer> UEEOSEcomSubsystem::GetCachedOffers() const
{
	return CachedOffers;
}

FEEOSCatalogOffer UEEOSEcomSubsystem::GetOfferById(const FString& OfferId) const
{
	for (const auto& Offer : CachedOffers)
	{
		if (Offer.OfferId == OfferId) return Offer;
	}
	return FEEOSCatalogOffer();
}

int32 UEEOSEcomSubsystem::GetOfferCount() const
{
	return CachedOffers.Num();
}

// ── Entitlements ─────────────────────────────────────────────────────────────

bool UEEOSEcomSubsystem::QueryEntitlements()
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("QueryEntitlements"));
		OnEntitlementsQueried.Broadcast(false, TArray<FEEOSEntitlement>());
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineEntitlementsPtr EntitlementsInterface = EOSSub->GetEntitlementsInterface();
	if (!EntitlementsInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSEcomSubsystem::QueryEntitlements — Entitlements interface not available"));
		OnEntitlementsQueried.Broadcast(false, TArray<FEEOSEntitlement>());
		return false;
	}

	FUniqueNetIdPtr UserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
	if (!UserId.IsValid())
	{
		OnEntitlementsQueried.Broadcast(false, TArray<FEEOSEntitlement>());
		return false;
	}

	// In-flight guard: a second QueryEntitlements while one is pending would overwrite the
	// stored handle and leak the first binding. Log-only rejection — broadcasting a failure
	// here would be indistinguishable from the in-flight query's real completion for its
	// waiters (R1).
	if (EntitlementsQueryHandle.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSEcomSubsystem::QueryEntitlements — An entitlements query is already in flight, rejecting (no broadcast)"));
		return false;
	}

	// FOnQueryEntitlementsComplete is a MULTICAST delegate — bind via the interface
	// Store the handle so we can remove ONLY our own binding in the callback
	EntitlementsQueryHandle = EntitlementsInterface->OnQueryEntitlementsCompleteDelegates.AddWeakLambda(this,
		[this, EntitlementsInterface, UserId](bool bWasSuccessful, const FUniqueNetId& InUserId, const FString& Namespace, const FString& Error)
		{
			// The list is interface-wide — ignore completions for other users (another
			// system's query) and keep our binding until our completion arrives
			if (UserId.IsValid() && InUserId != *UserId)
			{
				return;
			}

			// Remove only our own binding — QueryOwnership may have its own pending handler
			EntitlementsInterface->OnQueryEntitlementsCompleteDelegates.Remove(EntitlementsQueryHandle);
			EntitlementsQueryHandle.Reset();

			// Replace the cache only on SUCCESS — a transient failure must keep the
			// last-known-good entitlements instead of turning into "you own nothing"
			if (bWasSuccessful)
			{
				CachedEntitlements.Empty();

				TArray<TSharedRef<FOnlineEntitlement>> AllEntitlements;
				EntitlementsInterface->GetAllEntitlements(InUserId, Namespace, AllEntitlements);

				for (const auto& Ent : AllEntitlements)
				{
					FEEOSEntitlement EInfo;
					EInfo.EntitlementId = Ent->Id;
					EInfo.EntitlementName = Ent->Name;
					EInfo.CatalogItemId = Ent->ItemId;
					EInfo.bRedeemed = (Ent->ConsumedCount > 0);
					CachedEntitlements.Add(EInfo);
				}

				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSEcomSubsystem: Queried %d entitlements"), CachedEntitlements.Num());
			}
			else
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSEcomSubsystem: QueryEntitlements failed — %s (keeping last-known-good cache)"), *Error);
			}

			OnEntitlementsQueried.Broadcast(bWasSuccessful, CachedEntitlements);
		});

	// The engine returns false WITHOUT firing any delegate when the local user has no Epic
	// account (Connect/DeviceId-only login) — unbind and fail immediately so callers don't hang
	if (!EntitlementsInterface->QueryEntitlements(*UserId, TEXT("")))
	{
		EntitlementsInterface->OnQueryEntitlementsCompleteDelegates.Remove(EntitlementsQueryHandle);
		EntitlementsQueryHandle.Reset();
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSEcomSubsystem::QueryEntitlements — QueryEntitlements was refused (no Epic account for the local user?)"));
		OnEntitlementsQueried.Broadcast(false, TArray<FEEOSEntitlement>());
		return false;
	}

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSEcomSubsystem::QueryEntitlements — Querying..."));
	return true;
}

TArray<FEEOSEntitlement> UEEOSEcomSubsystem::GetCachedEntitlements() const
{
	return CachedEntitlements;
}

bool UEEOSEcomSubsystem::HasEntitlement(const FString& EntitlementNameOrItemId) const
{
	for (const auto& Ent : CachedEntitlements)
	{
		// Check both EntitlementName AND CatalogItemId — QueryOwnership passes catalog item IDs,
		// while other callers may pass entitlement names. Matching either prevents false negatives.
		if (Ent.EntitlementName == EntitlementNameOrItemId || Ent.CatalogItemId == EntitlementNameOrItemId)
		{
			return true;
		}
	}
	return false;
}

int32 UEEOSEcomSubsystem::GetEntitlementCount() const
{
	return CachedEntitlements.Num();
}

// ── Ownership ────────────────────────────────────────────────────────────────

bool UEEOSEcomSubsystem::QueryOwnership(const TArray<FString>& CatalogItemIds)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("QueryOwnership"));
		OnOwnershipQueried.Broadcast(false, TArray<FEEOSOwnership>());
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
	if (!IdentityInterface.IsValid())
	{
		OnOwnershipQueried.Broadcast(false, TArray<FEEOSOwnership>());
		return false;
	}

	FUniqueNetIdPtr UserId = IdentityInterface->GetUniquePlayerId(0);
	if (!UserId.IsValid())
	{
		OnOwnershipQueried.Broadcast(false, TArray<FEEOSOwnership>());
		return false;
	}

	IOnlineEntitlementsPtr EntitlementsInterface = EOSSub->GetEntitlementsInterface();
	if (!EntitlementsInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSEcomSubsystem::QueryOwnership — Entitlements interface not available, falling back to cached data"));
		// Degraded path: answer from the last-known-good entitlements cache, but report
		// FAILURE — cache-derived results must never masquerade as a fresh backend answer
		// (an empty cache would otherwise read as an authoritative "you own nothing")
		CachedOwnership.Empty();
		for (const FString& ItemId : CatalogItemIds)
		{
			FEEOSOwnership Ownership;
			Ownership.ItemId = ItemId;
			Ownership.bOwned = HasEntitlement(ItemId);
			CachedOwnership.Add(Ownership);
		}
		OnOwnershipQueried.Broadcast(false, CachedOwnership);
		return false;
	}

	// In-flight guard: a second QueryOwnership while one is pending would overwrite the stored
	// handle and leak the first binding. Log-only rejection — broadcasting a failure here would
	// be indistinguishable from the in-flight query's real completion for its waiters (R1).
	if (OwnershipQueryHandle.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSEcomSubsystem::QueryOwnership — An ownership query is already in flight, rejecting (no broadcast)"));
		return false;
	}

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSEcomSubsystem::QueryOwnership — Refreshing entitlements before checking %d items"), CatalogItemIds.Num());

	// Query fresh entitlements from EOS, then check ownership in the callback
	TArray<FString> ItemIdsCopy = CatalogItemIds;

	// Same interface-wide list as QueryEntitlements, but a SEPARATE member handle so the two
	// operations never remove each other's binding
	OwnershipQueryHandle = EntitlementsInterface->OnQueryEntitlementsCompleteDelegates.AddWeakLambda(this,
		[this, ItemIdsCopy, EntitlementsInterface, UserId](bool bWasSuccessful, const FUniqueNetId& EntUserId, const FString& Namespace, const FString& Error)
		{
			// Ignore completions for other users — keep our binding until ours arrives
			if (UserId.IsValid() && EntUserId != *UserId)
			{
				return;
			}

			// Remove only our own binding — a pending QueryEntitlements keeps its own handle
			EntitlementsInterface->OnQueryEntitlementsCompleteDelegates.Remove(OwnershipQueryHandle);
			OwnershipQueryHandle.Reset();

			if (bWasSuccessful)
			{
				// Refresh cached entitlements from the fresh query
				CachedEntitlements.Empty();
				IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
				if (EOSSub)
				{
					IOnlineEntitlementsPtr EntIf = EOSSub->GetEntitlementsInterface();
					if (EntIf.IsValid())
					{
						TArray<TSharedRef<FOnlineEntitlement>> Entitlements;
						EntIf->GetAllEntitlements(EntUserId, Namespace, Entitlements);
						for (const auto& Ent : Entitlements)
						{
							FEEOSEntitlement CachedEnt;
							CachedEnt.EntitlementId = Ent->Id;
							CachedEnt.EntitlementName = Ent->Name;
							CachedEnt.CatalogItemId = Ent->ItemId;
							CachedEnt.bRedeemed = (Ent->ConsumedCount > 0);
							CachedEntitlements.Add(CachedEnt);
						}
					}
				}
			}
			else
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSEcomSubsystem::QueryOwnership — Entitlement refresh failed (%s), serving last-known-good cache with bSuccess=false"), *Error);
			}

			// Build ownership results from the (refreshed or last-known-good) entitlements.
			// The success flag mirrors the refresh result: a failed refresh serves cache-derived
			// answers as FAILURE so listeners never mistake them for an authoritative
			// "all unowned" — especially when the entitlements cache was never populated.
			CachedOwnership.Empty();
			for (const FString& ItemId : ItemIdsCopy)
			{
				FEEOSOwnership Ownership;
				Ownership.ItemId = ItemId;
				Ownership.bOwned = HasEntitlement(ItemId);
				CachedOwnership.Add(Ownership);
			}

			OnOwnershipQueried.Broadcast(bWasSuccessful, CachedOwnership);
		});

	// The engine returns false WITHOUT firing any delegate when the local user has no Epic
	// account (Connect/DeviceId-only login) — unbind and fail immediately so callers don't hang
	if (!EntitlementsInterface->QueryEntitlements(*UserId, TEXT("")))
	{
		EntitlementsInterface->OnQueryEntitlementsCompleteDelegates.Remove(OwnershipQueryHandle);
		OwnershipQueryHandle.Reset();
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSEcomSubsystem::QueryOwnership — QueryEntitlements was refused (no Epic account for the local user?)"));
		OnOwnershipQueried.Broadcast(false, TArray<FEEOSOwnership>());
		return false;
	}
	return true;
}

bool UEEOSEcomSubsystem::IsItemOwned(const FString& CatalogItemId) const
{
	for (const auto& Ownership : CachedOwnership)
	{
		if (Ownership.ItemId == CatalogItemId) return Ownership.bOwned;
	}
	return false;
}

// ── Checkout / Purchase ──────────────────────────────────────────────────────

bool UEEOSEcomSubsystem::Checkout(const FString& OfferId)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("Checkout"));
		OnCheckoutComplete.Broadcast(false, TEXT(""));
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlinePurchasePtr PurchaseInterface = EOSSub->GetPurchaseInterface();
	if (!PurchaseInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSEcomSubsystem::Checkout — Purchase interface not available"));
		OnCheckoutComplete.Broadcast(false, TEXT(""));
		return false;
	}

	FUniqueNetIdPtr UserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
	if (!UserId.IsValid())
	{
		OnCheckoutComplete.Broadcast(false, TEXT(""));
		return false;
	}

	FPurchaseCheckoutRequest Request;
	Request.AddPurchaseOffer(TEXT(""), OfferId, 1);

	// FOnPurchaseCheckoutComplete is a regular delegate — weak-bind so the callback
	// is skipped if this subsystem is destroyed while checkout is in flight
	PurchaseInterface->Checkout(*UserId, Request,
		FOnPurchaseCheckoutComplete::CreateWeakLambda(this,
			[this, OfferId](const FOnlineError& Result, const TSharedRef<FPurchaseReceipt>& Receipt)
			{
				if (Result.bSucceeded)
				{
					FString TransactionId = Receipt->TransactionId;
					UE_LOG(LogExtendedEOS, Log, TEXT("EEOSEcomSubsystem: Checkout succeeded — TransactionId=%s"), *TransactionId);
					OnCheckoutComplete.Broadcast(true, TransactionId);
				}
				else
				{
					UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSEcomSubsystem: Checkout failed — %s"), *Result.ErrorMessage.ToString());
					OnCheckoutComplete.Broadcast(false, TEXT(""));
				}
			}));

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSEcomSubsystem::Checkout — Purchasing offer '%s'..."), *OfferId);
	return true;
}
