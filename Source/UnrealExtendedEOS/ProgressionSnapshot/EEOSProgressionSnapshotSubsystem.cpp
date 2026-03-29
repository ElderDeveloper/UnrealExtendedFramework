// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSProgressionSnapshotSubsystem.h"
#include "UnrealExtendedEOS.h"
#include "OnlineSubsystemUtils.h"

#include "eos_progressionsnapshot.h"
#include "eos_progressionsnapshot_types.h"
#include "eos_sdk.h"

void UEEOSProgressionSnapshotSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSProgressionSnapshotSubsystem initialized"));
}

void UEEOSProgressionSnapshotSubsystem::Deinitialize()
{
	// End any active snapshots before cleanup
	TArray<int32> ActiveIds;
	ActiveSnapshots.GetKeys(ActiveIds);
	for (int32 Id : ActiveIds)
	{
		// Clean up the SDK resources
		EOS_HPlatform PlatformHandle = GetPlatformHandle();
		if (PlatformHandle)
		{
			EOS_HProgressionSnapshot PSHandle = EOS_Platform_GetProgressionSnapshotInterface(PlatformHandle);
			if (PSHandle)
			{
				EOS_ProgressionSnapshot_EndSnapshotOptions EndOpts = {};
				EndOpts.ApiVersion = EOS_PROGRESSIONSNAPSHOT_ENDSNAPSHOT_API_LATEST;
				EndOpts.SnapshotId = static_cast<uint32_t>(Id);
				EOS_ProgressionSnapshot_EndSnapshot(PSHandle, &EndOpts);
			}
		}
	}
	ActiveSnapshots.Empty();
	SnapshotIdMapping.Empty();

	Super::Deinitialize();
}

// ── Snapshot Lifecycle ───────────────────────────────────────────────────────

int32 UEEOSProgressionSnapshotSubsystem::BeginSnapshot()
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("BeginSnapshot"));
		return -1;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSProgressionSnapshotSubsystem::BeginSnapshot — Platform handle not available"));
		return -1;
	}

	EOS_HProgressionSnapshot PSHandle = EOS_Platform_GetProgressionSnapshotInterface(PlatformHandle);
	if (!PSHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSProgressionSnapshotSubsystem::BeginSnapshot — ProgressionSnapshot interface not available"));
		return -1;
	}

	// Get the logged-in Product User ID
	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	FUniqueNetIdPtr LocalUserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
	if (!LocalUserId.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSProgressionSnapshotSubsystem::BeginSnapshot — No logged-in user"));
		return -1;
	}

	FString UserIdStr = LocalUserId->ToString();
	EOS_ProductUserId LocalPUID = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*UserIdStr));

	EOS_ProgressionSnapshot_BeginSnapshotOptions Options = {};
	Options.ApiVersion = EOS_PROGRESSIONSNAPSHOT_BEGINSNAPSHOT_API_LATEST;
	Options.LocalUserId = LocalPUID;

	uint32_t OutSnapshotId = EOS_INVALID_PROGRESSIONSNAPSHOTID;
	EOS_EResult Result = EOS_ProgressionSnapshot_BeginSnapshot(PSHandle, &Options, &OutSnapshotId);

	if (Result == EOS_EResult::EOS_Success && OutSnapshotId != EOS_INVALID_PROGRESSIONSNAPSHOTID)
	{
		int32 InternalId = NextSnapshotId++;
		ActiveSnapshots.Add(InternalId, TMap<FString, FString>());
		SnapshotIdMapping.Add(InternalId, static_cast<int32>(OutSnapshotId));

		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSProgressionSnapshotSubsystem: BeginSnapshot — Internal ID=%d, EOS ID=%u"), InternalId, OutSnapshotId);
		return InternalId;
	}
	else
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSProgressionSnapshotSubsystem::BeginSnapshot — Failed: %s"), ANSI_TO_TCHAR(EOS_EResult_ToString(Result)));
		return -1;
	}
}

bool UEEOSProgressionSnapshotSubsystem::AddProgressData(int32 SnapshotId, const FString& Key, const FString& Value)
{
	TMap<FString, FString>* Data = ActiveSnapshots.Find(SnapshotId);
	if (!Data)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSProgressionSnapshotSubsystem::AddProgressData — Snapshot %d not found"), SnapshotId);
		return false;
	}

	int32* EosIdPtr = SnapshotIdMapping.Find(SnapshotId);
	if (!EosIdPtr)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSProgressionSnapshotSubsystem::AddProgressData — No EOS mapping for snapshot %d"), SnapshotId);
		return false;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSProgressionSnapshotSubsystem::AddProgressData — Platform handle not available"));
		return false;
	}

	EOS_HProgressionSnapshot PSHandle = EOS_Platform_GetProgressionSnapshotInterface(PlatformHandle);
	if (!PSHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSProgressionSnapshotSubsystem::AddProgressData — Interface not available"));
		return false;
	}

	FTCHARToUTF8 KeyAnsi(*Key);
	FTCHARToUTF8 ValueAnsi(*Value);

	EOS_ProgressionSnapshot_AddProgressionOptions AddOptions = {};
	AddOptions.ApiVersion = EOS_PROGRESSIONSNAPSHOT_ADDPROGRESSION_API_LATEST;
	AddOptions.SnapshotId = static_cast<uint32_t>(*EosIdPtr);
	AddOptions.Key = KeyAnsi.Get();
	AddOptions.Value = ValueAnsi.Get();

	EOS_EResult Result = EOS_ProgressionSnapshot_AddProgression(PSHandle, &AddOptions);
	if (Result == EOS_EResult::EOS_Success)
	{
		Data->Add(Key, Value);
		UE_LOG(LogExtendedEOS, Verbose, TEXT("EEOSProgressionSnapshotSubsystem: AddProgressData — ID=%d Key='%s' Value='%s'"), SnapshotId, *Key, *Value);
		return true;
	}
	else
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSProgressionSnapshotSubsystem::AddProgressData — Failed: %s"), ANSI_TO_TCHAR(EOS_EResult_ToString(Result)));
		return false;
	}
}

void UEEOSProgressionSnapshotSubsystem::EndSnapshot(int32 SnapshotId)
{
	if (!ActiveSnapshots.Contains(SnapshotId))
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSProgressionSnapshotSubsystem::EndSnapshot — Snapshot %d not found"), SnapshotId);
		OnSnapshotComplete.Broadcast(false, SnapshotId);
		return;
	}

	int32* EosIdPtr = SnapshotIdMapping.Find(SnapshotId);
	if (!EosIdPtr)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSProgressionSnapshotSubsystem::EndSnapshot — No EOS mapping for snapshot %d"), SnapshotId);
		OnSnapshotComplete.Broadcast(false, SnapshotId);
		return;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSProgressionSnapshotSubsystem::EndSnapshot — Platform handle not available"));
		OnSnapshotComplete.Broadcast(false, SnapshotId);
		return;
	}

	EOS_HProgressionSnapshot PSHandle = EOS_Platform_GetProgressionSnapshotInterface(PlatformHandle);
	if (!PSHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSProgressionSnapshotSubsystem::EndSnapshot — Interface not available"));
		OnSnapshotComplete.Broadcast(false, SnapshotId);
		return;
	}

	uint32_t EosSnapshotId = static_cast<uint32_t>(*EosIdPtr);

	// Step 1: Submit the snapshot to the EOS cloud (async)
	EOS_ProgressionSnapshot_SubmitSnapshotOptions SubmitOpts = {};
	SubmitOpts.ApiVersion = EOS_PROGRESSIONSNAPSHOT_SUBMITSNAPSHOT_API_LATEST;
	SubmitOpts.SnapshotId = EosSnapshotId;

	struct FEndSnapshotContext
	{
		UEEOSProgressionSnapshotSubsystem* Self;
		int32 InternalId;
		uint32_t EosId;
	};

	FEndSnapshotContext* Context = new FEndSnapshotContext();
	Context->Self = this;
	Context->InternalId = SnapshotId;
	Context->EosId = EosSnapshotId;

	EOS_ProgressionSnapshot_SubmitSnapshot(PSHandle, &SubmitOpts, Context,
		[](const EOS_ProgressionSnapshot_SubmitSnapshotCallbackInfo* Data)
		{
			FEndSnapshotContext* Ctx = static_cast<FEndSnapshotContext*>(Data->ClientData);
			if (!Ctx) return;

			UEEOSProgressionSnapshotSubsystem* Self = Ctx->Self;
			int32 InternalId = Ctx->InternalId;
			uint32_t EosId = Ctx->EosId;
			delete Ctx;

			if (!Self) return;

			bool bSuccess = (Data->ResultCode == EOS_EResult::EOS_Success);

			if (bSuccess)
			{
				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSProgressionSnapshotSubsystem: Snapshot %d submitted to EOS cloud"), InternalId);
			}
			else
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSProgressionSnapshotSubsystem::EndSnapshot — Submit failed: %s"),
					ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
			}

			// Step 2: End the snapshot to release SDK resources (regardless of submit result)
			EOS_HPlatform PlatformHandle = Self->GetPlatformHandle();
			if (PlatformHandle)
			{
				EOS_HProgressionSnapshot PSHandle = EOS_Platform_GetProgressionSnapshotInterface(PlatformHandle);
				if (PSHandle)
				{
					EOS_ProgressionSnapshot_EndSnapshotOptions EndOpts = {};
					EndOpts.ApiVersion = EOS_PROGRESSIONSNAPSHOT_ENDSNAPSHOT_API_LATEST;
					EndOpts.SnapshotId = EosId;

					EOS_EResult EndResult = EOS_ProgressionSnapshot_EndSnapshot(PSHandle, &EndOpts);
					if (EndResult != EOS_EResult::EOS_Success)
					{
						UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSProgressionSnapshotSubsystem::EndSnapshot — EndSnapshot SDK call failed: %s"),
							ANSI_TO_TCHAR(EOS_EResult_ToString(EndResult)));
					}
				}
			}

			// Step 3: Clean up local state — fixes the state bug where snapshot stays "in progress"
			Self->ActiveSnapshots.Remove(InternalId);
			Self->SnapshotIdMapping.Remove(InternalId);

			Self->OnSnapshotComplete.Broadcast(bSuccess, InternalId);
		});

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSProgressionSnapshotSubsystem: EndSnapshot — Submitting ID=%d (%d entries) to EOS cloud..."),
		SnapshotId, ActiveSnapshots[SnapshotId].Num());
}

void UEEOSProgressionSnapshotSubsystem::DeleteSnapshot(int32 SnapshotId)
{
	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSProgressionSnapshotSubsystem::DeleteSnapshot — Platform handle not available"));
		OnSnapshotDeleted.Broadcast(false);
		return;
	}

	EOS_HProgressionSnapshot PSHandle = EOS_Platform_GetProgressionSnapshotInterface(PlatformHandle);
	if (!PSHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSProgressionSnapshotSubsystem::DeleteSnapshot — Interface not available"));
		OnSnapshotDeleted.Broadcast(false);
		return;
	}

	// Get the logged-in Product User ID
	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	FUniqueNetIdPtr LocalUserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
	if (!LocalUserId.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSProgressionSnapshotSubsystem::DeleteSnapshot — No logged-in user"));
		OnSnapshotDeleted.Broadcast(false);
		return;
	}

	FString UserIdStr = LocalUserId->ToString();
	EOS_ProductUserId LocalPUID = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*UserIdStr));

	// EOS ProgressionSnapshot DeleteSnapshot only takes LocalUserId — it clears all
	// submitted snapshot data for the user. The SnapshotId is local-only and used to
	// clean up our local tracking on success.

	EOS_ProgressionSnapshot_DeleteSnapshotOptions Options = {};
	Options.ApiVersion = EOS_PROGRESSIONSNAPSHOT_DELETESNAPSHOT_API_LATEST;
	Options.LocalUserId = LocalPUID;

	struct FDeleteContext
	{
		UEEOSProgressionSnapshotSubsystem* Self;
		int32 SnapshotId;
	};

	FDeleteContext* Ctx = new FDeleteContext();
	Ctx->Self = this;
	Ctx->SnapshotId = SnapshotId;

	EOS_ProgressionSnapshot_DeleteSnapshot(PSHandle, &Options, Ctx,
		[](const EOS_ProgressionSnapshot_DeleteSnapshotCallbackInfo* Data)
		{
			FDeleteContext* Ctx = static_cast<FDeleteContext*>(Data->ClientData);
			if (!Ctx) return;

			UEEOSProgressionSnapshotSubsystem* Self = Ctx->Self;
			int32 SnapId = Ctx->SnapshotId;
			delete Ctx;

			if (!Self) return;

			bool bSuccess = (Data->ResultCode == EOS_EResult::EOS_Success);
			if (bSuccess)
			{
				// Only clean up local state on confirmed backend deletion
				Self->ActiveSnapshots.Remove(SnapId);
				Self->SnapshotIdMapping.Remove(SnapId);
				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSProgressionSnapshotSubsystem: Snapshot ID=%d deleted from EOS cloud and local state cleaned"), SnapId);
			}
			else
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSProgressionSnapshotSubsystem::DeleteSnapshot — Failed: %s (local state preserved)"),
					ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
			}

			Self->OnSnapshotDeleted.Broadcast(bSuccess);
		});

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSProgressionSnapshotSubsystem: DeleteSnapshot — ID=%d, requesting EOS cloud deletion..."), SnapshotId);
}

// ── Queries ──────────────────────────────────────────────────────────────────

bool UEEOSProgressionSnapshotSubsystem::IsSnapshotInProgress() const
{
	return ActiveSnapshots.Num() > 0;
}

int32 UEEOSProgressionSnapshotSubsystem::GetActiveSnapshotId() const
{
	if (ActiveSnapshots.Num() == 0) return -1;

	// Return the most recent snapshot
	int32 LatestId = -1;
	for (const auto& Pair : ActiveSnapshots)
	{
		if (Pair.Key > LatestId) LatestId = Pair.Key;
	}
	return LatestId;
}

TMap<FString, FString> UEEOSProgressionSnapshotSubsystem::GetSnapshotData(int32 SnapshotId) const
{
	const TMap<FString, FString>* Data = ActiveSnapshots.Find(SnapshotId);
	if (Data) return *Data;
	return TMap<FString, FString>();
}
