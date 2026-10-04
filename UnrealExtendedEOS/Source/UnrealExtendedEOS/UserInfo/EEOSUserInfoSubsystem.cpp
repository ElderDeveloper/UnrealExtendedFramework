// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSUserInfoSubsystem.h"
#include "Shared/EEOSNativeOperation.h"
#include "Shared/EEOSIdentityUtils.h"
#include "OnlineSubsystemUtils.h"
#include "Interfaces/OnlineUserInterface.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Shared/EEOSBlueprintLibrary.h"
#include "UnrealExtendedEOS.h"

#include "eos_sdk.h"
#include "eos_connect.h"
#include "eos_userinfo.h"

namespace
{
	/** Same-user test for EOS net ids. Instance compare first: the EOS net-id registry hands
	 *  out ONE shared instance per user and mutates it in place when a missing half becomes
	 *  known (FUniqueNetIdEOSRegistry::FindOrAddImpl, OnlineSubsystemEOSTypes.cpp:246-260), so
	 *  ToString() snapshots taken at request time can stop matching by completion time while
	 *  pointer identity survives. Fall back to comparing the Epic-account half only — the half
	 *  user-info queries are keyed by; the PUID half may be present on one side only. */
	bool IsSameEOSUser(const FUniqueNetId& A, const FUniqueNetId& B)
	{
		if (&A == &B)
		{
			return true;
		}
		const FString EasA = UEEOSBlueprintLibrary::ExtractEpicAccountId(A.ToString());
		const FString EasB = UEEOSBlueprintLibrary::ExtractEpicAccountId(B.ToString());
		return !EasA.IsEmpty() && EasA == EasB;
	}

	/** Read a user's info from what the engine already has. IOnlineUser::GetUserInfo on the
	 *  5.8 EOS OSS serves LOCAL users only (UserManagerEOS.cpp:3730-3748); queried remote
	 *  users are reachable through IOnlineIdentity::GetPlayerNickname(const FUniqueNetId&)
	 *  (:2320-2335), which loses country/language but keeps the fields games actually use. */
	bool GetKnownUserInfo(const IOnlineUserPtr& UserInterface, const IOnlineIdentityPtr& Identity, const FUniqueNetId& UserId, FEEOSUserInfo& OutInfo)
	{
		if (UserInterface.IsValid())
		{
			TSharedPtr<FOnlineUser> UserData = UserInterface->GetUserInfo(0, UserId);
			if (UserData.IsValid())
			{
				OutInfo.EpicAccountId = UserData->GetUserId()->ToString();
				OutInfo.DisplayName = UserData->GetDisplayName();
				UserData->GetUserAttribute(TEXT("country"), OutInfo.Country);
				UserData->GetUserAttribute(TEXT("preferredLanguage"), OutInfo.PreferredLanguage);
				return true;
			}
		}
		if (Identity.IsValid())
		{
			const FString Nickname = EEOSIdentity::SafeNickname(Identity, UserId);
			if (!Nickname.IsEmpty())
			{
				OutInfo.EpicAccountId = UserId.ToString();
				OutInfo.DisplayName = Nickname;
				return true;
			}
		}
		return false;
	}

	/** Bare-id acceptance: the EOS net-id registry only parses the composite
	 *  "<EpicAccountId>|<ProductUserId>" (Epic half BEFORE the pipe) — appending the separator
	 *  turns a bare EAS id into a valid composite. Malformed ids still fail the registry's
	 *  checks and are caught by the EmptyId guard at the call site. */
	FString MakeCompositeId(const FString& EpicAccountId)
	{
		FString CompositeId = EpicAccountId;
		if (!CompositeId.Contains(TEXT("|")))
		{
			CompositeId += TEXT("|");
		}
		return CompositeId;
	}
}

void UEEOSUserInfoSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEEOSUserInfoSubsystem::Deinitialize()
{
	BeginEOSShutdown(); bShuttingDown = true;
	if (PendingUserInterface.IsValid())
	{
		PendingUserInterface->OnQueryUserInfoCompleteDelegates[0].Remove(QueryUserInfoDelegateHandle);
		PendingUserInterface->OnQueryUserInfoCompleteDelegates[0].Remove(QueryBatchDelegateHandle);
	}
	PendingUserInterface.Reset();
	// Clear any in-flight query handlers — the lambdas self-remove on completion,
	// but an operation still in flight would leave its handle registered
	QueryUserInfoDelegateHandle.Reset(); QueryBatchDelegateHandle.Reset();

	bFindUserByDisplayNameInFlight = false;
	PendingQueryUserInfoId.Reset();
	PendingBatchQueryIds.Empty();
	CachedSearchResults.Empty();
	CachedMappings.Empty();
	Super::Deinitialize();
}

// ── User Lookup ──────────────────────────────────────────────────────────────

bool UEEOSUserInfoSubsystem::QueryUserInfo(const FString& EpicAccountId)
{
	if (bShuttingDown || QueryUserInfoDelegateHandle.IsValid() || QueryBatchDelegateHandle.IsValid() || bFindUserByDisplayNameInFlight)
	{
		RejectOperation(TEXT("QueryUserInfo"), EEOSOperationCode::Busy, TEXT("A user lookup sharing the native query or completion channel is pending.")); return false;
	}

	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("QueryUserInfo"));
		OnUserInfoQueried.Broadcast(false, FEEOSUserInfo());
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineUserPtr UserInterface = EOSSub->GetUserInterface();
	IOnlineIdentityPtr Identity = EOSSub->GetIdentityInterface();
	if (!UserInterface.IsValid() || !Identity.IsValid())
	{
		FEEOSNativeOperationLease::ReportRepeated(GetOwningEOSInstanceName(), TEXT("QueryUserInfo"), TEXT("CapabilityUnavailable"));
		OnUserInfoQueried.Broadcast(false, FEEOSUserInfo());
		return false;
	}

	FUniqueNetIdPtr LocalUserId = Identity->GetUniquePlayerId(0);
	if (!LocalUserId.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSUserInfoSubsystem::QueryUserInfo — No logged-in user"));
		OnUserInfoQueried.Broadcast(false, FEEOSUserInfo());
		return false;
	}

	// In-flight guard: a second single-user query while one is pending would clobber the
	// stored request id and handle. Rejections must NOT broadcast on the shared completion
	// delegate — the legitimate in-flight caller is waiting on it — so: log + return false.
	if (QueryUserInfoDelegateHandle.IsValid())
	{
		RejectOperation(TEXT("QueryUserInfo"), EEOSOperationCode::Busy, TEXT("Another admitted request already owns this operation."));
		return false;
	}

	// CreateUniquePlayerId on the EOS OSS returns the non-null registry EmptyId on parse
	// failure — Ptr.IsValid() alone is not a validity check, ask the id itself too
	FUniqueNetIdPtr ParsedId = Identity->CreateUniquePlayerId(MakeCompositeId(EpicAccountId));
	if (!ParsedId.IsValid() || !ParsedId->IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSUserInfoSubsystem::QueryUserInfo — Invalid EpicAccountId '%s'"), *FEEOSNativeOperationLease::SafeField(EpicAccountId));
		OnUserInfoQueried.Broadcast(false, FEEOSUserInfo());
		return false;
	}

	// Local user? The engine SKIPS local users (UserManagerEOS.cpp:3629-3632) and, with
	// nothing else in the request, fires NO completion at all — the guard would arm forever.
	// Its cache already holds the data: serve it synchronously and never call the engine.
	if (IsSameEOSUser(*ParsedId, *LocalUserId))
	{
		FEEOSUserInfo Info;
		if (GetKnownUserInfo(UserInterface, Identity, *ParsedId, Info))
		{
			CachedUserInfo = Info;
			UE_LOG(LogExtendedEOS, Log, TEXT("EEOSUserInfoSubsystem: Served local user '%s' from cache — %s"),
				*FEEOSNativeOperationLease::SafeField(CachedUserInfo.EpicAccountId), *FEEOSNativeOperationLease::SafeField(CachedUserInfo.DisplayName));
			OnUserInfoQueried.Broadcast(true, CachedUserInfo);
			return true;
		}
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSUserInfoSubsystem::QueryUserInfo — Local user '%s' has no cached account info"), *FEEOSNativeOperationLease::SafeField(EpicAccountId));
		OnUserInfoQueried.Broadcast(false, FEEOSUserInfo());
		return false;
	}

	// The engine also skips ids without a valid Epic Account half (:3634-3636) — again with
	// no echo and possibly no completion. A Connect-only id can never complete: fail now.
	if (UEEOSBlueprintLibrary::ExtractEpicAccountId(ParsedId->ToString()).IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSUserInfoSubsystem::QueryUserInfo — '%s' has no Epic Account half; the engine cannot query it"), *FEEOSNativeOperationLease::SafeField(EpicAccountId));
		OnUserInfoQueried.Broadcast(false, FEEOSUserInfo());
		return false;
	}

	if (!EEOSIdentity::HasLocalEpicAccount(Identity))
	{
		OnUserInfoQueried.Broadcast(false, FEEOSUserInfo()); return false;
	}
	PendingUserInterface = UserInterface;
	// Remember the requested id as the registry INSTANCE (not a string snapshot — see the
	// member comment); the completion list is interface-wide and any concurrent user-info
	// query (incl. engine-internal friends-name resolution) lands there too.
	PendingQueryUserInfoId = ParsedId;

	const auto Context = CaptureEOSContext();
	const int64 Request = BeginOperation(TEXT("QueryUserInfo"), ParsedId->ToString());
	QueryUserInfoDelegateHandle = UserInterface->OnQueryUserInfoCompleteDelegates[0].AddWeakLambda(this,
		[this, EpicAccountId, UserInterface, Identity, Context, Request](int32 LocalUserNum, bool bWasSuccessful, const TArray<FUniqueNetIdRef>& QueriedIds, const FString& ErrorStr)
		{
			// Only consume a completion that echoes the id we asked for; anything else is
			// someone else's query — ignore WITHOUT unbinding and keep waiting for ours.
			// (The engine's completion accumulator can also carry foreign ids alongside ours,
			// so we search the echo rather than requiring an exact match.)
			if (bShuttingDown || LocalUserNum != 0 || !PendingQueryUserInfoId.IsValid() || GetActiveOperationOutcome(TEXT("QueryUserInfo")).RequestId != Request)
			{
				return;
			}
			FUniqueNetIdPtr OurQueriedId;
			for (const FUniqueNetIdRef& QueriedId : QueriedIds)
			{
				if (IsSameEOSUser(*QueriedId, *PendingQueryUserInfoId))
				{
					OurQueriedId = QueriedId;
					break;
				}
			}
			if (!OurQueriedId.IsValid())
			{
				return;
			}

			// Our completion — release our binding and the pending request BEFORE broadcasting
			// so a listener can start the next query from inside the completion event
			UserInterface->OnQueryUserInfoCompleteDelegates[0].Remove(QueryUserInfoDelegateHandle);
			QueryUserInfoDelegateHandle.Reset();
			PendingQueryUserInfoId.Reset();

			// Success is decided by whether OUR user actually resolved: the completion's
			// bWasSuccessful reflects only the LAST read the engine processed, which can
			// belong to a different id sharing the same accumulated completion.
			FEEOSUserInfo Info;
			const bool bCurrent = IsEOSContextCurrent(Context);
			const bool bResolved = bCurrent && GetKnownUserInfo(UserInterface, Identity, *OurQueriedId, Info);
			const auto Outcome = CompleteOperation(TEXT("QueryUserInfo"), bResolved, !bCurrent ? EEOSOperationCode::Canceled : bResolved ? EEOSOperationCode::Succeeded : EEOSOperationCode::NativeFailure,
				bCurrent ? TEXT("User query completed.") : TEXT("Original query identity/platform retired."), FString(), bWasSuccessful ? TEXT("Success") : TEXT("Failure"), EEOSResultSource::NativeCallback);
			FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
			if (bResolved)
			{
				CachedUserInfo = Info;
				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSUserInfoSubsystem: Queried user '%s' — %s"),
					*FEEOSNativeOperationLease::SafeField(CachedUserInfo.EpicAccountId), *FEEOSNativeOperationLease::SafeField(CachedUserInfo.DisplayName));
				OnUserInfoQueried.Broadcast(true, CachedUserInfo); OnOperationCompleted.Broadcast(Outcome);
				return;
			}

			UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSUserInfoSubsystem: QueryUserInfo failed for '%s' (engine success flag: %d) — %s"),
				*FEEOSNativeOperationLease::SafeField(EpicAccountId), bWasSuccessful ? 1 : 0, *FEEOSNativeOperationLease::SafeField(ErrorStr));
			OnUserInfoQueried.Broadcast(false, FEEOSUserInfo()); OnOperationCompleted.Broadcast(Outcome);
		});

	TArray<FUniqueNetIdRef> UserIds;
	UserIds.Add(ParsedId->AsShared());
	const FDelegateHandle SubmittedHandle = QueryUserInfoDelegateHandle;
	if (!UserInterface->QueryUserInfo(0, UserIds) && QueryUserInfoDelegateHandle == SubmittedHandle)
	{
		UserInterface->OnQueryUserInfoCompleteDelegates[0].Remove(QueryUserInfoDelegateHandle);
		QueryUserInfoDelegateHandle.Reset(); PendingQueryUserInfoId.Reset();
		const auto Outcome = CompleteOperation(TEXT("QueryUserInfo"), false, EEOSOperationCode::NativeStartRejected, TEXT("Native user query refused submission."));
		FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
		OnUserInfoQueried.Broadcast(false, FEEOSUserInfo()); OnOperationCompleted.Broadcast(Outcome); return false;
	}
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSUserInfoSubsystem::QueryUserInfo — Querying '%s'..."), *FEEOSNativeOperationLease::SafeField(EpicAccountId));
	return true;
}

bool UEEOSUserInfoSubsystem::FindUserByDisplayName(const FString& DisplayName)
{
	if (bShuttingDown || QueryUserInfoDelegateHandle.IsValid() || QueryBatchDelegateHandle.IsValid() || bFindUserByDisplayNameInFlight)
	{
		RejectOperation(TEXT("FindUserByDisplayName"), EEOSOperationCode::Busy, TEXT("A user lookup sharing the native query or completion channel is pending.")); return false;
	}

	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("FindUserByDisplayName"));
		OnUserSearchComplete.Broadcast(false, TArray<FEEOSUserInfo>());
		return false;
	}

	// In-flight guard: this operation broadcasts on the SHARED OnUserSearchComplete delegate,
	// so a concurrent search would clobber CachedSearchResults and confuse the pending
	// caller's waiters. Rejections do not broadcast (the pending caller owns the delegate).
	if (bFindUserByDisplayNameInFlight)
	{
		RejectOperation(TEXT("FindUserByDisplayName"), EEOSOperationCode::Busy, TEXT("Another admitted request already owns this operation."));
		return false;
	}

	// NOTE: the engine's IOnlineUser::QueryUserIdMapping wrapper cannot provide exactly-once
	// completion semantics on 5.8: its internal callback always executes the caller's delegate
	// once with (false, EmptyId, empty error) — even on success, BEFORE the real result — and
	// when the found user is already registered it never delivers the success at all
	// (UserManagerEOS.cpp:3765-3817). Query the SDK directly instead, matching this file's
	// existing raw-SDK pattern (QueryExternalAccountMappings).
	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	EOS_HUserInfo UserInfoHandle = PlatformHandle ? EOS_Platform_GetUserInfoInterface(PlatformHandle) : nullptr;
	if (!UserInfoHandle)
	{
		FEEOSNativeOperationLease::ReportRepeated(GetOwningEOSInstanceName(), TEXT("FindUserByDisplayName"), TEXT("CapabilityUnavailable"));
		OnUserSearchComplete.Broadcast(false, TArray<FEEOSUserInfo>());
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	FUniqueNetIdPtr LocalUserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
	const FString LocalEasStr = LocalUserId.IsValid() ? UEEOSBlueprintLibrary::ExtractEpicAccountId(LocalUserId->ToString()) : FString();
	if (LocalEasStr.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSUserInfoSubsystem::FindUserByDisplayName — Requires an Epic account login (Connect-only sessions cannot search)"));
		OnUserSearchComplete.Broadcast(false, TArray<FEEOSUserInfo>());
		return false;
	}
	const EOS_EpicAccountId LocalEas = EOS_EpicAccountId_FromString(TCHAR_TO_UTF8(*LocalEasStr));

	struct FFindUserContext
	{
		TWeakObjectPtr<UEEOSUserInfoSubsystem> Self;
		FString DisplayName;
		EOS_HUserInfo UserInfoHandle;
		TSharedPtr<IEOSPlatformHandle, ESPMode::ThreadSafe> Platform;
		FEEOSRequestContext Context;
		int64 Request;
	};
	const int64 Request = BeginOperation(TEXT("FindUserByDisplayName"));
	FFindUserContext* Ctx = new FFindUserContext{ this, DisplayName, UserInfoHandle, GetOwningEOSPlatform(), CaptureEOSContext(), Request };

	// The UTF-8 conversion outlives the synchronous SDK call, which copies the string
	FTCHARToUTF8 NameUtf8(*DisplayName);
	EOS_UserInfo_QueryUserInfoByDisplayNameOptions Options = {};
	Options.ApiVersion = EOS_USERINFO_QUERYUSERINFOBYDISPLAYNAME_API_LATEST;
	Options.LocalUserId = LocalEas;
	Options.DisplayName = NameUtf8.Get();

	bFindUserByDisplayNameInFlight = true;

	EOS_UserInfo_QueryUserInfoByDisplayName(UserInfoHandle, &Options, Ctx,
		[](const EOS_UserInfo_QueryUserInfoByDisplayNameCallbackInfo* Data)
		{
			if (!Data || !Data->ClientData || !EOS_EResult_IsOperationComplete(Data->ResultCode)) return;
			TUniquePtr<FFindUserContext> Ctx(static_cast<FFindUserContext*>(Data->ClientData));

			UEEOSUserInfoSubsystem* Self = Ctx->Self.Get();
			const FString SearchedName = MoveTemp(Ctx->DisplayName);
			const EOS_HUserInfo UserInfoHandle = Ctx->UserInfoHandle;

			if (!Self || Self->bShuttingDown) return;
			if (Self->GetActiveOperationOutcome(TEXT("FindUserByDisplayName")).RequestId != Ctx->Request)
			{ Self->LogCallbackDisposition(TEXT("FindUserByDisplayName"), Ctx->Request, TEXT("DifferentOwner"), Ctx->Context.Generation); return; }

			const bool bCurrent = Self->IsEOSContextCurrent(Ctx->Context);
			const bool bSuccess = bCurrent && (Data->ResultCode == EOS_EResult::EOS_Success);
			FEEOSUserInfo Info;
			if (bSuccess)
			{
				char EasBuffer[EOS_EPICACCOUNTID_MAX_LENGTH + 1];
				int32_t BufferSize = sizeof(EasBuffer);
				if (EOS_EpicAccountId_ToString(Data->TargetUserId, EasBuffer, &BufferSize) == EOS_EResult::EOS_Success)
				{
					Info.EpicAccountId = UTF8_TO_TCHAR(EasBuffer);
				}
				Info.DisplayName = Data->DisplayName ? UTF8_TO_TCHAR(Data->DisplayName) : SearchedName;

				// The query populated the SDK-side cache — pull the rich fields while we're here
				EOS_UserInfo_CopyUserInfoOptions CopyOptions = {};
				CopyOptions.ApiVersion = EOS_USERINFO_COPYUSERINFO_API_LATEST;
				CopyOptions.LocalUserId = Data->LocalUserId;
				CopyOptions.TargetUserId = Data->TargetUserId;

				EOS_UserInfo* SdkInfo = nullptr;
				if (EOS_UserInfo_CopyUserInfo(UserInfoHandle, &CopyOptions, &SdkInfo) == EOS_EResult::EOS_Success && SdkInfo)
				{
					if (SdkInfo->DisplayName)       { Info.DisplayName = UTF8_TO_TCHAR(SdkInfo->DisplayName); }
					if (SdkInfo->Nickname)          { Info.Nickname = UTF8_TO_TCHAR(SdkInfo->Nickname); }
					if (SdkInfo->Country)           { Info.Country = UTF8_TO_TCHAR(SdkInfo->Country); }
					if (SdkInfo->PreferredLanguage) { Info.PreferredLanguage = UTF8_TO_TCHAR(SdkInfo->PreferredLanguage); }
					EOS_UserInfo_Release(SdkInfo);
				}
			}

			const auto Outcome = Self->CompleteOperation(TEXT("FindUserByDisplayName"), bSuccess, !bCurrent ? EEOSOperationCode::Canceled : bSuccess ? EEOSOperationCode::Succeeded : EEOSOperationCode::NativeFailure,
				bCurrent ? TEXT("Display-name query completed.") : TEXT("Original display-name query identity/platform retired."), FString(), FString(), EEOSResultSource::SDKCallback, ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
			FEEOSOutcomeDispatchScope Dispatch(Self, Outcome);
			Self->HandleFindUserByDisplayNameComplete(bSuccess, Info, SearchedName); Self->OnOperationCompleted.Broadcast(Outcome);
		});

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSUserInfoSubsystem::FindUserByDisplayName — Searching '%s'..."), *FEEOSNativeOperationLease::SafeField(DisplayName));
	return true;
}

void UEEOSUserInfoSubsystem::HandleFindUserByDisplayNameComplete(bool bSuccess, const FEEOSUserInfo& FoundUser, const FString& SearchedName)
{
	// Release the guard BEFORE broadcasting so a listener can start the next search from
	// inside the completion event without being rejected
	bFindUserByDisplayNameInFlight = false;

	CachedSearchResults.Empty();
	if (bSuccess)
	{
		CachedSearchResults.Add(FoundUser);
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSUserInfoSubsystem: Found user '%s' — ID=%s"),
			*FEEOSNativeOperationLease::SafeField(FoundUser.DisplayName), *FEEOSNativeOperationLease::SafeField(FoundUser.EpicAccountId));
	}
	else
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSUserInfoSubsystem: FindUserByDisplayName failed for '%s'"), *FEEOSNativeOperationLease::SafeField(SearchedName));
	}

	OnUserSearchComplete.Broadcast(bSuccess, CachedSearchResults);
}

bool UEEOSUserInfoSubsystem::QueryUserInfoBatch(const TArray<FString>& EpicAccountIds)
{
	if (bShuttingDown || QueryUserInfoDelegateHandle.IsValid() || QueryBatchDelegateHandle.IsValid() || bFindUserByDisplayNameInFlight)
	{
		RejectOperation(TEXT("QueryUserInfoBatch"), EEOSOperationCode::Busy, TEXT("A user lookup sharing the native query or completion channel is pending.")); return false;
	}

	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("QueryUserInfoBatch"));
		OnUserSearchComplete.Broadcast(false, TArray<FEEOSUserInfo>());
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineUserPtr UserInterface = EOSSub->GetUserInterface();
	IOnlineIdentityPtr Identity = EOSSub->GetIdentityInterface();
	if (!UserInterface.IsValid() || !Identity.IsValid())
	{
		OnUserSearchComplete.Broadcast(false, TArray<FEEOSUserInfo>());
		return false;
	}

	FUniqueNetIdPtr LocalUserId = Identity->GetUniquePlayerId(0);
	if (!LocalUserId.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSUserInfoSubsystem::QueryUserInfoBatch — No logged-in user"));
		OnUserSearchComplete.Broadcast(false, TArray<FEEOSUserInfo>());
		return false;
	}

	// In-flight guard: rejections must NOT broadcast on the shared completion delegate —
	// the legitimate in-flight caller is waiting on it — so: log + return false.
	if (QueryBatchDelegateHandle.IsValid())
	{
		RejectOperation(TEXT("QueryUserInfoBatch"), EEOSOperationCode::Busy, TEXT("Another admitted request already owns this operation."));
		return false;
	}

	// Parse and partition. The engine silently SKIPS local users and ids without a valid
	// Epic Account half (UserManagerEOS.cpp:3629-3636): skipped ids are never echoed, and if
	// EVERYTHING is skipped no completion fires at all. Serve local users from the engine
	// cache up front and drop unqueryable ids, so the engine only ever sees ids it will answer.
	TArray<FUniqueNetIdRef> QueryIds;
	TArray<FEEOSUserInfo> PreServedResults;
	for (const FString& Id : EpicAccountIds)
	{
		// CreateUniquePlayerId on the EOS OSS returns the non-null registry EmptyId on parse
		// failure — Ptr.IsValid() alone is not a validity check, ask the id itself too
		FUniqueNetIdPtr ParsedId = Identity->CreateUniquePlayerId(MakeCompositeId(Id));
		if (!ParsedId.IsValid() || !ParsedId->IsValid())
		{
			UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSUserInfoSubsystem::QueryUserInfoBatch — Skipping invalid ID '%s'"), *FEEOSNativeOperationLease::SafeField(Id));
			continue;
		}

		if (IsSameEOSUser(*ParsedId, *LocalUserId))
		{
			FEEOSUserInfo Info;
			if (GetKnownUserInfo(UserInterface, Identity, *ParsedId, Info))
			{
				PreServedResults.Add(Info);
			}
			else
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSUserInfoSubsystem::QueryUserInfoBatch — Local user '%s' has no cached account info, skipping"), *FEEOSNativeOperationLease::SafeField(Id));
			}
		}
		else if (UEEOSBlueprintLibrary::ExtractEpicAccountId(ParsedId->ToString()).IsEmpty())
		{
			UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSUserInfoSubsystem::QueryUserInfoBatch — '%s' has no Epic Account half; the engine cannot query it, skipping"), *FEEOSNativeOperationLease::SafeField(Id));
		}
		else
		{
			QueryIds.Add(ParsedId->AsShared());
		}
	}

	if (QueryIds.Num() == 0)
	{
		// No native request is submitted for users already resolved locally.
		BeginOperation(TEXT("QueryUserInfoBatch"));
		CachedSearchResults = PreServedResults;
		const bool bAnyResult = CachedSearchResults.Num() > 0;
		const auto Outcome = CompleteOperation(TEXT("QueryUserInfoBatch"), bAnyResult,
			bAnyResult ? EEOSOperationCode::Succeeded : EEOSOperationCode::InvalidTarget, TEXT("Batch resolved from local information; no native query submitted."));
		FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
		OnUserSearchComplete.Broadcast(bAnyResult, CachedSearchResults);
		if (!bShuttingDown) OnOperationCompleted.Broadcast(Outcome);
		return bAnyResult;
	}

	if (!EEOSIdentity::HasLocalEpicAccount(Identity))
	{
		CachedSearchResults = PreServedResults;
		OnUserSearchComplete.Broadcast(!CachedSearchResults.IsEmpty(), CachedSearchResults);
		return !CachedSearchResults.IsEmpty();
	}
	PendingUserInterface = UserInterface;
	// Remember the ids we actually sent as registry INSTANCES (not string snapshots — see
	// the member comment); the completion list is interface-wide and any concurrent
	// user-info query (incl. engine-internal friends-name resolution) lands there too.
	PendingBatchQueryIds = QueryIds;

	const auto Context = CaptureEOSContext();
	const int64 Request = BeginOperation(TEXT("QueryUserInfoBatch"));
	QueryBatchDelegateHandle = UserInterface->OnQueryUserInfoCompleteDelegates[0].AddWeakLambda(this,
		[this, UserInterface, Identity, PreServedResults, Context, Request](int32 LocalUserNum, bool bWasSuccessful, const TArray<FUniqueNetIdRef>& QueriedIds, const FString& ErrorStr)
		{
			if (bShuttingDown || LocalUserNum != 0 || PendingBatchQueryIds.Num() == 0 || GetActiveOperationOutcome(TEXT("QueryUserInfoBatch")).RequestId != Request)
			{
				return;
			}

			// The engine may legitimately echo a strict SUBSET of our request (it skips users
			// it decides not to query even after our pre-filter, e.g. secondary local users),
			// and its completion accumulator can carry foreign ids alongside ours. So: consume
			// any completion whose NON-EMPTY echo intersects our request, then read results
			// per REQUESTED id below. An empty echo (the engine's own duplicate-rejection
			// signal) never matches and is ignored.
			bool bIsOurCompletion = false;
			for (const FUniqueNetIdRef& QueriedId : QueriedIds)
			{
				for (const FUniqueNetIdRef& RequestedId : PendingBatchQueryIds)
				{
					if (IsSameEOSUser(*QueriedId, *RequestedId))
					{
						bIsOurCompletion = true;
						break;
					}
				}
				if (bIsOurCompletion)
				{
					break;
				}
			}
			if (!bIsOurCompletion)
			{
				return;
			}

			// Our completion — release our binding and the pending set BEFORE broadcasting
			// so a listener can start the next batch from inside the completion event
			const TArray<FUniqueNetIdRef> RequestedIds = MoveTemp(PendingBatchQueryIds);
			PendingBatchQueryIds.Empty();
			UserInterface->OnQueryUserInfoCompleteDelegates[0].Remove(QueryBatchDelegateHandle);
			QueryBatchDelegateHandle.Reset();

			// Results = locally pre-served users + whatever the engine now knows about the
			// ids we requested. Read per REQUEST (not per echo) and regardless of the
			// engine's success flag: subset echoes mean some ids may be missing, and the
			// flag only reflects the LAST read the engine processed — already-cached or
			// earlier-resolved users still surface. Success = we produced any results.
			const bool bCurrent = IsEOSContextCurrent(Context);
			CachedSearchResults = bCurrent ? PreServedResults : TArray<FEEOSUserInfo>();
			if (bCurrent) for (const FUniqueNetIdRef& RequestedId : RequestedIds)
			{
				FEEOSUserInfo Info;
				if (GetKnownUserInfo(UserInterface, Identity, *RequestedId, Info)) CachedSearchResults.Add(Info);
			}
			const bool bAnyResult = CachedSearchResults.Num() > 0;
			const auto Outcome = CompleteOperation(TEXT("QueryUserInfoBatch"), bAnyResult,
				!bCurrent ? EEOSOperationCode::Canceled : bAnyResult ? EEOSOperationCode::Succeeded : EEOSOperationCode::NativeFailure,
				bCurrent ? TEXT("Batch query completed.") : TEXT("Original batch identity/platform retired."), FString(),
				bWasSuccessful ? TEXT("Success") : TEXT("Failure"), EEOSResultSource::NativeCallback);
			FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
			OnUserSearchComplete.Broadcast(bAnyResult, CachedSearchResults);
			if (!bShuttingDown) OnOperationCompleted.Broadcast(Outcome);
		});

	const FDelegateHandle SubmittedHandle = QueryBatchDelegateHandle;
	const auto SubmittedIds = PendingBatchQueryIds;
	if (!UserInterface->QueryUserInfo(0, SubmittedIds) && QueryBatchDelegateHandle == SubmittedHandle)
	{
		UserInterface->OnQueryUserInfoCompleteDelegates[0].Remove(QueryBatchDelegateHandle);
		QueryBatchDelegateHandle.Reset(); PendingBatchQueryIds.Empty();
		const auto Outcome = CompleteOperation(TEXT("QueryUserInfoBatch"), false, EEOSOperationCode::NativeStartRejected, TEXT("Native batch query refused submission."));
		FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
		OnUserSearchComplete.Broadcast(false, TArray<FEEOSUserInfo>()); OnOperationCompleted.Broadcast(Outcome); return false;
	}
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSUserInfoSubsystem::QueryUserInfoBatch — Querying %d of %d users (%d served locally)..."),
		PendingBatchQueryIds.Num(), EpicAccountIds.Num(), PreServedResults.Num());
	return true;
}

// ── External Account Mapping ─────────────────────────────────────────────────

bool UEEOSUserInfoSubsystem::QueryExternalAccountMappings(const TArray<FString>& ExternalAccountIds, EEOSExternalCredentialType AccountType)
{
	if (bShuttingDown || bMappingsQueryInFlight)
	{
		RejectOperation(TEXT("QueryExternalAccountMappings"), EEOSOperationCode::Busy, TEXT("External account mapping query is unavailable or pending.")); return false;
	}
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("QueryExternalAccountMappings"));
		OnExternalMappingsQueried.Broadcast(false, TArray<FEEOSExternalAccountMapping>());
		return false;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle)
	{
		FEEOSNativeOperationLease::ReportRepeated(GetOwningEOSInstanceName(), TEXT("QueryExternalAccountMappings"), TEXT("CapabilityUnavailable"));
		OnExternalMappingsQueried.Broadcast(false, TArray<FEEOSExternalAccountMapping>());
		return false;
	}

	EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(PlatformHandle);
	if (!ConnectHandle)
	{
		FEEOSNativeOperationLease::ReportRepeated(GetOwningEOSInstanceName(), TEXT("QueryExternalAccountMappings"), TEXT("CapabilityUnavailable"));
		OnExternalMappingsQueried.Broadcast(false, TArray<FEEOSExternalAccountMapping>());
		return false;
	}

	// Get the logged-in Product User ID
	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	FUniqueNetIdPtr LocalUserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
	if (!LocalUserId.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSUserInfoSubsystem::QueryExternalAccountMappings — No logged-in user"));
		OnExternalMappingsQueried.Broadcast(false, TArray<FEEOSExternalAccountMapping>());
		return false;
	}

	// ToString() is the composite "<EpicAccountId>|<ProductUserId>" — extract the PUID half.
	// EOS_ProductUserId_FromString performs NO validation, so an empty extracted half is the
	// only reliable failure signal here.
	const FString LocalPUIDStr = UEEOSBlueprintLibrary::ExtractProductUserId(LocalUserId->ToString());
	if (LocalPUIDStr.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSUserInfoSubsystem::QueryExternalAccountMappings — Logged-in user has no Product User ID (no Connect session)"));
		OnExternalMappingsQueried.Broadcast(false, TArray<FEEOSExternalAccountMapping>());
		return false;
	}
	EOS_ProductUserId LocalPUID = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*LocalPUIDStr));

	// Map our credential type to EOS external account type
	EOS_EExternalAccountType EosAccountType = EOS_EExternalAccountType::EOS_EAT_EPIC;
	switch (AccountType)
	{
	case EEOSExternalCredentialType::Steam:     EosAccountType = EOS_EExternalAccountType::EOS_EAT_STEAM; break;
	case EEOSExternalCredentialType::PSN:        EosAccountType = EOS_EExternalAccountType::EOS_EAT_PSN; break;
	case EEOSExternalCredentialType::XboxLive:   EosAccountType = EOS_EExternalAccountType::EOS_EAT_XBL; break;
	case EEOSExternalCredentialType::Nintendo:   EosAccountType = EOS_EExternalAccountType::EOS_EAT_NINTENDO; break;
	case EEOSExternalCredentialType::Discord:    EosAccountType = EOS_EExternalAccountType::EOS_EAT_DISCORD; break;
	case EEOSExternalCredentialType::Apple:      EosAccountType = EOS_EExternalAccountType::EOS_EAT_APPLE; break;
	default:                                    EosAccountType = EOS_EExternalAccountType::EOS_EAT_EPIC; break;
	}

	// Build the array of external account ID strings for the EOS SDK (const char**).
	// Two-pass: first convert every id into stable heap storage, THEN take the pointers.
	// (Storing FTCHARToUTF8 elements in a growing TArray relocates them past the inline
	// slack and dangles every previously-taken char* — verified bug for >4 ids.)
	TArray<TArray<ANSICHAR>> ConvertedIds;
	ConvertedIds.Reserve(ExternalAccountIds.Num());
	for (const FString& ExtId : ExternalAccountIds)
	{
		FTCHARToUTF8 Converted(*ExtId);
		TArray<ANSICHAR>& Stored = ConvertedIds.Emplace_GetRef();
		Stored.Append(Converted.Get(), Converted.Length());
		Stored.Add('\0');
	}

	// The inner arrays' heap buffers are stable now — safe to take raw pointers. Both
	// containers stay alive until this function returns, which is after the SDK call
	// below has copied the strings.
	TArray<const char*> RawIdPtrs;
	RawIdPtrs.Reserve(ConvertedIds.Num());
	for (const TArray<ANSICHAR>& Stored : ConvertedIds)
	{
		RawIdPtrs.Add(Stored.GetData());
	}

	EOS_Connect_QueryExternalAccountMappingsOptions Options = {};
	Options.ApiVersion = EOS_CONNECT_QUERYEXTERNALACCOUNTMAPPINGS_API_LATEST;
	Options.LocalUserId = LocalPUID;
	Options.AccountIdType = EosAccountType;
	Options.ExternalAccountIds = RawIdPtrs.GetData();
	Options.ExternalAccountIdCount = static_cast<uint32_t>(RawIdPtrs.Num());

	// We need to keep the input IDs and account type for the callback's GetExternalAccountMapping calls
	struct FQueryMappingsContext
	{
		TWeakObjectPtr<UEEOSUserInfoSubsystem> Self;
		TArray<FString> QueriedIds;
		EOS_EExternalAccountType AccountType;
		EOS_HConnect ConnectHandle = nullptr;
		FEEOSRequestContext Ownership;
		TSharedPtr<IEOSPlatformHandle, ESPMode::ThreadSafe> Platform;
	};

	FQueryMappingsContext* Ctx = new FQueryMappingsContext();
	Ctx->Self = this;
	Ctx->QueriedIds = ExternalAccountIds;
	Ctx->AccountType = EosAccountType;
	Ctx->ConnectHandle = ConnectHandle;
	Ctx->Platform = GetOwningEOSPlatform(); Ctx->Ownership = CaptureEOSContext();
	bMappingsQueryInFlight = true;

	EOS_Connect_QueryExternalAccountMappings(ConnectHandle, &Options, Ctx,
		[](const EOS_Connect_QueryExternalAccountMappingsCallbackInfo* Data)
		{
			if (!Data || !Data->ClientData || !EOS_EResult_IsOperationComplete(Data->ResultCode)) return;
			TUniquePtr<FQueryMappingsContext> Ctx(static_cast<FQueryMappingsContext*>(Data->ClientData));

			UEEOSUserInfoSubsystem* Self = Ctx->Self.Get();
			TArray<FString> QueriedIds = MoveTemp(Ctx->QueriedIds);
			EOS_EExternalAccountType AccountType = Ctx->AccountType;

			if (!Self || Self->bShuttingDown) return;
			const bool bCurrent = Self->IsEOSContextCurrent(Ctx->Ownership);

			Self->bMappingsQueryInFlight = false;
			Self->CachedMappings.Empty();

			if (bCurrent && Data->ResultCode == EOS_EResult::EOS_Success)
			{
				const EOS_HConnect ConnectHandle = Ctx->ConnectHandle;

				// Now retrieve mappings for each queried external ID
				for (const FString& ExtId : QueriedIds)
				{
					FTCHARToUTF8 ExtIdAnsi(*ExtId);

					EOS_Connect_GetExternalAccountMappingsOptions GetOptions = {};
					GetOptions.ApiVersion = EOS_CONNECT_GETEXTERNALACCOUNTMAPPING_API_LATEST;
					GetOptions.LocalUserId = Data->LocalUserId;
					GetOptions.AccountIdType = AccountType;
					GetOptions.TargetExternalUserId = ExtIdAnsi.Get();

					EOS_ProductUserId MappedPUID = EOS_Connect_GetExternalAccountMapping(ConnectHandle, &GetOptions);
					if (EOS_ProductUserId_IsValid(MappedPUID))
					{
						char PUIDBuffer[EOS_PRODUCTUSERID_MAX_LENGTH + 1];
						int32_t BufferSize = sizeof(PUIDBuffer);
						FString PUIDStr;
						if (EOS_ProductUserId_ToString(MappedPUID, PUIDBuffer, &BufferSize) == EOS_EResult::EOS_Success)
						{
							PUIDStr = ANSI_TO_TCHAR(PUIDBuffer);
						}

						FEEOSExternalAccountMapping Mapping;
						Mapping.ExternalAccountId = ExtId;
						Mapping.ProductUserId = PUIDStr;
						// ExternalAccountType and DisplayName would come from a separate query
						Self->CachedMappings.Add(Mapping);
					}
				}

				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSUserInfoSubsystem: External account mapping query succeeded — %d mappings found"),
					Self->CachedMappings.Num());
				Self->OnExternalMappingsQueried.Broadcast(true, Self->CachedMappings);
			}
			else
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSUserInfoSubsystem::QueryExternalAccountMappings — Failed: %s"),
					ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
				Self->OnExternalMappingsQueried.Broadcast(false, Self->CachedMappings);
			}
		});

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSUserInfoSubsystem::QueryExternalAccountMappings — Querying %d external IDs via EOS Connect..."), ExternalAccountIds.Num());
	return true;
}

// ── Queries ──────────────────────────────────────────────────────────────────

FEEOSUserInfo UEEOSUserInfoSubsystem::GetCachedUserInfo() const
{
	return CachedUserInfo;
}

TArray<FEEOSUserInfo> UEEOSUserInfoSubsystem::GetCachedSearchResults() const
{
	return CachedSearchResults;
}

TArray<FEEOSExternalAccountMapping> UEEOSUserInfoSubsystem::GetCachedMappings() const
{
	return CachedMappings;
}
