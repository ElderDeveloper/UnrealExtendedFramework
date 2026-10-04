// Copyright Kemal Erdem YILMAZ. All Rights Reserved.
#include "EEOSSubsystem.h"
#include "EEOSSettings.h"
#include "EEOSNativeOperation.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "IOnlineSubsystemEOS.h"
#include "IEOSSDKManager.h"
#include "Engine/GameInstance.h"
#include "EEOSLog.h"
#include "HAL/PlatformTime.h"
#include "UObject/UnrealType.h"
#include "eos_connect.h"
#include "eos_sdk.h"

namespace
{
	struct FCreationBudget { int32 Remaining = 3; bool bReported = false; };
	TMap<FName, FCreationBudget> CreationBudgets;
}

void UEEOSSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bEOSShuttingDown = false;
	// Instance creation is bounded and scoped; pure queries below never create it.
	GetEOSOnlineSubsystem();
	CaptureEOSContext();
	ContextTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UEEOSSubsystem::TickEOSContext), 0.5f);
}
void UEEOSSubsystem::Deinitialize()
{
	BeginEOSShutdown();
	if (ContextTicker.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(ContextTicker);
	ContextTicker.Reset();
	if (ObservedContext.Identity.IsValid()) ObservedContext.Identity->ClearOnLoginStatusChangedDelegate_Handle(0, ContextStatusHandle);
	ContextStatusHandle.Reset();
	++ObservedContext.Generation; ++ObservedContext.PlatformGeneration;
	ObservedContext.Identity.Reset(); ObservedContext.Platform.Reset(); ObservedContext.LocalId.Empty();
	CachedEOSSubsystem = nullptr;
	OperationStartTimes.Empty();
	Super::Deinitialize();
}
FName UEEOSSubsystem::GetOwningEOSInstanceName() const
{
	if (const UWorld* World = GetWorld())
	{
		if (IOnlineSubsystemUtils* Utils = Online::GetUtils())
			ResolvedEOSInstance = Utils->GetOnlineIdentifier(World, EOS_SUBSYSTEM);
	}
	if (ResolvedEOSInstance.IsNone()) ResolvedEOSInstance = EOS_SUBSYSTEM;
	return ResolvedEOSInstance;
}
IOnlineSubsystem* UEEOSSubsystem::GetExistingEOSOnlineSubsystem() const
{
	const FName Instance = GetOwningEOSInstanceName();
	if (!IOnlineSubsystem::DoesInstanceExist(Instance))
	{
		CachedEOSSubsystem = nullptr;
		return nullptr;
	}
	// Re-fetch an existing instance instead of trusting a potentially retired raw pointer.
	IOnlineSubsystem* Current = IOnlineSubsystem::Get(Instance);
	if (Current != CachedEOSSubsystem)
	{
		UE_LOG(LogExtendedEOS, Log, TEXT("EOSInstance Instance=%s GameInstance=%s Phase=Resolved Available=%d"),
			*FEEOSNativeOperationLease::SafeField(Instance.ToString()), *GetNameSafe(GetGameInstance()), Current != nullptr);
		CachedEOSSubsystem = Current;
	}
	return Current;
}
IOnlineSubsystem* UEEOSSubsystem::GetEOSOnlineSubsystem() const
{
	if (IOnlineSubsystem* Existing = GetExistingEOSOnlineSubsystem()) return Existing;
	if (bEOSShuttingDown) return nullptr;
	const FName Instance = GetOwningEOSInstanceName();
	FCreationBudget& Budget = CreationBudgets.FindOrAdd(Instance);
	if (Budget.Remaining <= 0)
	{
		if (!Budget.bReported)
		{
			Budget.bReported = true;
			UE_LOG(LogExtendedEOS, Error, TEXT("EOSInstance Instance=%s Phase=Unavailable CreationAttemptsExhausted=1; inspect preceding native initialization/configuration errors"), *FEEOSNativeOperationLease::SafeField(Instance.ToString()));
		}
		return nullptr;
	}
	--Budget.Remaining;
	CachedEOSSubsystem = IOnlineSubsystem::Get(Instance);
	if (CachedEOSSubsystem) { Budget.Remaining = 3; Budget.bReported = false; }
	return CachedEOSSubsystem;
}
FEEOSRequestContext UEEOSSubsystem::CaptureEOSContext() const
{
	IOnlineSubsystem* OSS = GetExistingEOSOnlineSubsystem();
	const auto Platform = OSS && OSS->GetSubsystemName() == EOS_SUBSYSTEM
		? static_cast<IOnlineSubsystemEOS*>(OSS)->GetEOSPlatformHandle() : TSharedPtr<IEOSPlatformHandle, ESPMode::ThreadSafe>();
	const uint32 Capabilities = OSS ? (OSS->GetSessionInterface().IsValid() ? 1u : 0u)
		| (OSS->GetChatInterface().IsValid() ? 2u : 0u) | (OSS->GetUserInterface().IsValid() ? 4u : 0u)
		| (OSS->GetPresenceInterface().IsValid() ? 8u : 0u) : 0u;
	if (ObservedCapabilities != Capabilities)
	{
		UE_LOG(LogExtendedEOS, Log, TEXT("EOSCapabilities Instance=%s PreviousMask=%u NewMask=%u Bits=Session1Chat2User4Presence8"),
			*GetOwningEOSInstanceName().ToString(), ObservedCapabilities, Capabilities);
		ObservedCapabilities = Capabilities;
	}
	const auto Identity = OSS ? OSS->GetIdentityInterface() : IOnlineIdentityPtr();
	const auto Local = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : FUniqueNetIdPtr();
	const auto Status = Identity.IsValid() ? Identity->GetLoginStatus(0) : ELoginStatus::NotLoggedIn;
	const FString LocalId = Local.IsValid() && Local->IsValid() && Status == ELoginStatus::LoggedIn ? Local->ToString() : FString();
	const FName Instance = GetOwningEOSInstanceName();
	const bool bPlatformChanged = ObservedContext.Platform != Platform || ObservedContext.Instance != Instance || ObservedContext.Identity != Identity;
	const bool bChanged = bPlatformChanged || ObservedContext.LocalId != LocalId || ObservedLoginStatus != Status;
	if (bChanged)
	{
		UE_LOG(LogExtendedEOS, Log, TEXT("EOSContext Instance=%s Generation=%lld PreviousPlatform=%d NewPlatform=%d PreviousLoggedIn=%d NewLoggedIn=%d IdentityChanged=%d Shutdown=%d"),
			*FEEOSNativeOperationLease::SafeField(Instance.ToString()), ObservedContext.Generation + 1, ObservedContext.Platform.IsValid(), Platform.IsValid(),
			ObservedLoginStatus == ELoginStatus::LoggedIn, Status == ELoginStatus::LoggedIn, ObservedContext.LocalId != LocalId, bEOSShuttingDown);
		++ObservedContext.Generation;
		if (bPlatformChanged) ++ObservedContext.PlatformGeneration;
	}
	if (ObservedContext.Identity != Identity)
	{
		if (ObservedContext.Identity.IsValid()) ObservedContext.Identity->ClearOnLoginStatusChangedDelegate_Handle(0, ContextStatusHandle);
		ContextStatusHandle.Reset();
		if (Identity.IsValid() && !bEOSShuttingDown)
		{
			const auto Weak = TWeakObjectPtr<UEEOSSubsystem>(const_cast<UEEOSSubsystem*>(this));
			ContextStatusHandle = Identity->AddOnLoginStatusChangedDelegate_Handle(0, FOnLoginStatusChangedDelegate::CreateLambda(
				[Weak](int32 User, ELoginStatus::Type Previous, ELoginStatus::Type Current, const FUniqueNetId&)
				{
					if (auto* Self = Weak.Get(); Self && User == 0 && Previous != Current)
					{
						// Even a logout/login of the same ID invalidates old SDK work.
						++Self->ObservedContext.Generation;
						Self->CaptureEOSContext();
					}
				}));
		}
	}
	ObservedContext.Platform = Platform; ObservedContext.Identity = Identity; ObservedContext.Instance = Instance;
	ObservedContext.LocalId = LocalId; ObservedLoginStatus = Status;
	return ObservedContext;
}
bool UEEOSSubsystem::IsEOSContextCurrent(const FEEOSRequestContext& Context, bool bRequireSameIdentity) const
{
	if (bEOSShuttingDown) return false;
	const auto Current = CaptureEOSContext();
	return Context.PlatformGeneration == Current.PlatformGeneration && Context.Platform == Current.Platform
		&& Context.Identity == Current.Identity && Context.Instance == Current.Instance
		&& (!bRequireSameIdentity || (Context.Generation == Current.Generation && Context.LocalId == Current.LocalId));
}
FEEOSReadinessSnapshot UEEOSSubsystem::GetEOSReadiness() const
{
	const auto Context = CaptureEOSContext();
	FEEOSReadinessSnapshot Result; Result.Instance = Context.Instance; Result.Generation = Context.Generation;
	Result.bPlatformAvailable = Context.Platform.IsValid();
	Result.State = bEOSShuttingDown ? EEOSReadinessState::ShuttingDown : (CachedEOSSubsystem && Context.Platform.IsValid()) ? EEOSReadinessState::Ready
		: CachedEOSSubsystem ? EEOSReadinessState::Initializing
		: IsOwningEOSCreationExhausted() ? EEOSReadinessState::CreationExhausted
		: (CreationBudgets.Contains(Context.Instance) && CreationBudgets.FindChecked(Context.Instance).Remaining < 3) ? EEOSReadinessState::Initializing : EEOSReadinessState::InstanceAbsent;
	Result.bNativeLoggedIn = Context.Identity.IsValid() && Context.Identity->GetLoginStatus(0) == ELoginStatus::LoggedIn;
	FString Epic, Product;
	if (!Context.LocalId.Split(TEXT("|"), &Epic, &Product)) Product = Context.LocalId;
	Result.bHasEpicAccount = Result.bNativeLoggedIn && !Epic.IsEmpty() && EOS_EpicAccountId_IsValid(EOS_EpicAccountId_FromString(TCHAR_TO_UTF8(*Epic))) == EOS_TRUE;
	Result.bHasProductUserId = Result.bNativeLoggedIn && !Product.IsEmpty() && EOS_ProductUserId_IsValid(EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*Product))) == EOS_TRUE;
	if (Result.bHasProductUserId && Context.Platform.IsValid())
	{
		const EOS_HConnect Connect = EOS_Platform_GetConnectInterface(static_cast<EOS_HPlatform>(*Context.Platform));
		Result.bConnectLoggedIn = Connect && EOS_Connect_GetLoginStatus(Connect, EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*Product))) == EOS_ELoginStatus::EOS_LS_LoggedIn;
	}
	return Result;
}
bool UEEOSSubsystem::TickEOSContext(float)
{
	if (bEOSShuttingDown) return false;
	CaptureEOSContext(); FEEOSNativeOperationLease::FlushDiagnosticSummaries(); return true;
}
bool UEEOSSubsystem::IsEOSAvailable() const { return GetExistingEOSOnlineSubsystem() != nullptr; }
void UEEOSSubsystem::LogCallbackDisposition(FName Operation, int64 RequestId, const TCHAR* Disposition, int64 Generation) const
{
	UE_LOG(LogExtendedEOS, Verbose, TEXT("EOSCallback Instance=%s Op=%s Request=%lld Generation=%lld Disposition=%s Attribution=PluginLeaseOnly ExternalNativeCallerIdentifiable=0"),
		*GetOwningEOSInstanceName().ToString(), *FEEOSNativeOperationLease::SafeField(Operation.ToString()), RequestId, Generation, Disposition);
}
bool UEEOSSubsystem::IsEOSCreationExhausted()
{
	const FCreationBudget* Budget = CreationBudgets.Find(EOS_SUBSYSTEM);
	return Budget && Budget->Remaining <= 0 && !IOnlineSubsystem::DoesInstanceExist(EOS_SUBSYSTEM);
}
bool UEEOSSubsystem::IsOwningEOSCreationExhausted() const
{
	const FName Instance = GetOwningEOSInstanceName();
	const FCreationBudget* Budget = CreationBudgets.Find(Instance);
	return Budget && Budget->Remaining <= 0 && !IOnlineSubsystem::DoesInstanceExist(Instance);
}
TSharedPtr<IEOSPlatformHandle, ESPMode::ThreadSafe> UEEOSSubsystem::GetOwningEOSPlatform() const
{
	IOnlineSubsystem* OSS = GetExistingEOSOnlineSubsystem();
	if (!OSS || OSS->GetSubsystemName() != EOS_SUBSYSTEM) return nullptr;
	return static_cast<IOnlineSubsystemEOS*>(OSS)->GetEOSPlatformHandle();
}
EOS_HPlatform UEEOSSubsystem::GetPlatformHandle() const
{
	const auto Platform = GetOwningEOSPlatform();
	return Platform.IsValid() ? static_cast<EOS_HPlatform>(*Platform) : nullptr;
}
const UEEOSSettings* UEEOSSubsystem::GetEOSSettings() const { return UEEOSSettings::Get(); }
void UEEOSSubsystem::LogEOSUnavailable(const FString& FunctionName) const
{
	UE_LOG(LogExtendedEOS, Verbose, TEXT("EOSInstance Instance=%s Op=%s Phase=Unavailable Shutdown=%d"),
		*GetOwningEOSInstanceName().ToString(), *FEEOSNativeOperationLease::SafeField(FunctionName), bEOSShuttingDown);
}
FEEOSOperationOutcome UEEOSSubsystem::GetLastOperationOutcome(FName Operation) const
{
	for (int32 Index = DispatchOutcomes.Num() - 1; Index >= 0; --Index)
		if (DispatchOutcomes[Index].Operation == Operation) return DispatchOutcomes[Index];
	if (const auto* Result = TerminalOutcomes.Find(Operation)) return *Result;
	return FEEOSOperationOutcome();
}
FEEOSOperationOutcome UEEOSSubsystem::GetActiveOperationOutcome(FName Operation) const
{
	if (const auto* Started = OperationStartTimes.Find(Operation))
		if (const auto* Result = OperationOutcomes.Find(Operation))
		{
			auto Snapshot = *Result; Snapshot.ElapsedMilliseconds = (FPlatformTime::Seconds() - *Started) * 1000; return Snapshot;
		}
	return FEEOSOperationOutcome();
}
FEEOSOutcomeDispatchScope::FEEOSOutcomeDispatchScope(UEEOSSubsystem* InOwner, const FEEOSOperationOutcome& Outcome) : Owner(InOwner)
{
	Owner->DispatchOutcomes.Add(Outcome);
}
FEEOSOutcomeDispatchScope::~FEEOSOutcomeDispatchScope()
{
	Owner->DispatchOutcomes.Pop(EAllowShrinking::No);
}
int64 UEEOSSubsystem::BeginOperation(FName Operation, const FString& TargetId, int64 RequestId)
{
	FEEOSOperationOutcome Result;
	Result.Operation = Operation;
	Result.RequestId = RequestId ? RequestId : FEEOSNativeOperationLease::NextRequestId();
	Result.TargetId = TargetId;
	Result.ContextGeneration = CaptureEOSContext().Generation;
	Result.StartedUtc = FDateTime::UtcNow(); Result.LastTransitionUtc = Result.StartedUtc;
	Result.Code = EEOSOperationCode::Started;
	Result.Phase = TEXT("Started");
	OperationOutcomes.Add(Operation, Result);
	OperationStartTimes.Add(Operation, FPlatformTime::Seconds());
	if (GetEOSSettings()->bEnableOperationLogging) UE_LOG(LogExtendedEOS, Log, TEXT("EOSOperation Instance=%s GameInstance=%s Op=%s Request=%lld Phase=Started Target=%s"),
		*GetOwningEOSInstanceName().ToString(), *GetNameSafe(GetGameInstance()), *FEEOSNativeOperationLease::SafeField(Operation.ToString()), Result.RequestId,
		*FEEOSNativeOperationLease::SafeField(TargetId));
	return Result.RequestId;
}
void UEEOSSubsystem::SetOperationPhase(FName Operation, FName Phase)
{
	if (auto* Result = OperationOutcomes.Find(Operation))
	{
		Result->Phase = Phase; Result->LastTransitionUtc = FDateTime::UtcNow();
		UE_LOG(LogExtendedEOS, Verbose, TEXT("EOSOperation Op=%s Request=%lld Phase=%s"), *FEEOSNativeOperationLease::SafeField(Operation.ToString()), Result->RequestId, *FEEOSNativeOperationLease::SafeField(Phase.ToString()));
	}
}
void UEEOSSubsystem::TagOperationContext(FName Operation, const FString& OriginalId, int64 MembershipGeneration, int64 SearchGeneration, int64 ParentRequestId)
{
	if (auto* Result = OperationOutcomes.Find(Operation))
	{
		Result->OriginalId = OriginalId; Result->MembershipGeneration = MembershipGeneration;
		Result->SearchGeneration = SearchGeneration; Result->ParentRequestId = ParentRequestId;
	}
}
FEEOSOperationOutcome UEEOSSubsystem::CompleteOperation(FName Operation, bool bSuccess, EEOSOperationCode Code, const FString& Message,
	const FString& CurrentId, const FString& NativeResult, EEOSResultSource Source, const FString& SDKResult)
{
	FEEOSOperationOutcome Result = GetActiveOperationOutcome(Operation);
	if (!Result.RequestId) { Result.Operation = Operation; Result.RequestId = FEEOSNativeOperationLease::NextRequestId(); }
	Result.bSuccess = bSuccess;
	Result.Code = Code;
	Result.Source = Source;
	Result.Message = Message;
	Result.CurrentId = CurrentId;
	Result.NativeResult = NativeResult;
	Result.SDKResult = SDKResult;
	Result.Phase = TEXT("Complete");
	Result.CompletedUtc = FDateTime::UtcNow(); Result.LastTransitionUtc = Result.CompletedUtc;
	if (const double* Started = OperationStartTimes.Find(Operation))
		Result.ElapsedMilliseconds = (FPlatformTime::Seconds() - *Started) * 1000;
	OperationStartTimes.Remove(Operation);
	OperationOutcomes.Add(Operation, Result);
	TerminalOutcomes.Add(Operation, Result);
	if (GetEOSSettings()->bEnableOperationLogging) UE_LOG(LogExtendedEOS, Log, TEXT("EOSOperation Instance=%s Op=%s Request=%lld ParentRequest=%lld ContextGeneration=%lld MembershipGeneration=%lld SearchGeneration=%lld Original=%s Phase=Complete Success=%d Code=%s Source=%s Target=%s Current=%s NativeResult=%s SDKResult=%s ElapsedMs=%.0f"),
		*GetOwningEOSInstanceName().ToString(), *FEEOSNativeOperationLease::SafeField(Operation.ToString()), Result.RequestId, Result.ParentRequestId, Result.ContextGeneration, Result.MembershipGeneration, Result.SearchGeneration, *FEEOSNativeOperationLease::SafeField(Result.OriginalId), bSuccess,
		*StaticEnum<EEOSOperationCode>()->GetNameStringByValue(static_cast<int64>(Code)),
		*StaticEnum<EEOSResultSource>()->GetNameStringByValue(static_cast<int64>(Source)),
		*FEEOSNativeOperationLease::SafeField(Result.TargetId), *FEEOSNativeOperationLease::SafeField(CurrentId),
		*FEEOSNativeOperationLease::SafeField(NativeResult), SDKResult.IsEmpty() ? TEXT("Unavailable") : *FEEOSNativeOperationLease::SafeField(SDKResult), Result.ElapsedMilliseconds);
	if (!bSuccess && Code != EEOSOperationCode::Canceled && Code != EEOSOperationCode::Superseded)
		FEEOSNativeOperationLease::ReportRepeated(GetOwningEOSInstanceName(), Operation, FName(*StaticEnum<EEOSOperationCode>()->GetNameStringByValue(int64(Code))));
	// Callers publish this captured result AFTER legacy delegates, preserving reentrant callers.
	return Result;
}
void UEEOSSubsystem::RejectOperation(FName Operation, EEOSOperationCode Code, const FString& Message, const FString& TargetId)
{
	FEEOSOperationOutcome Result;
	Result.RequestId = FEEOSNativeOperationLease::NextRequestId();
	Result.Operation = Operation;
	Result.Code = Code;
	Result.Message = Message;
	Result.TargetId = TargetId;
	Result.Phase = TEXT("Rejected");
	Result.ContextGeneration = CaptureEOSContext().Generation;
	Result.StartedUtc = Result.CompletedUtc = Result.LastTransitionUtc = FDateTime::UtcNow();
	if (Code == EEOSOperationCode::Busy)
	{
		FEEOSOperationOutcome Blocking = GetActiveOperationOutcome(Operation);
		if (auto* OSS = GetExistingEOSOnlineSubsystem())
		{
			FName Scope;
			const void* Interface = nullptr;
			const FString Name = Operation.ToString();
			if (Name == TEXT("FindLobbies") || Name == TEXT("FindSessions") || Name == TEXT("FindMatch"))
			{ Scope = TEXT("Search"); Interface = OSS->GetSessionInterface().Get(); }
			else if (Name.Contains(TEXT("Lobby")))
			{ Scope = TEXT("EOS_Lobby"); Interface = OSS->GetSessionInterface().Get(); }
			else if (Name.Contains(TEXT("Session")))
			{ Scope = FName(*TargetId); Interface = OSS->GetSessionInterface().Get(); }
			else if (Name == TEXT("AcceptMatch") || Name == TEXT("CancelMatch"))
			{ Scope = NAME_GameSession; Interface = OSS->GetSessionInterface().Get(); }
			else if (Name.Contains(TEXT("Login")) || Name.Contains(TEXT("Logout")) || Name.Contains(TEXT("Account")) || Name.Contains(TEXT("Device")) || Name.Contains(TEXT("Auth")))
			{ Scope = TEXT("Identity0"); Interface = OSS->GetIdentityInterface().Get(); }
			if (Interface && !Scope.IsNone())
			{
				const auto NativeOwner = FEEOSNativeOperationLease::DescribeOwner(Interface, Scope);
				if (NativeOwner.RequestId) Blocking = NativeOwner;
			}
		}
		Result.BlockingRequestId = Blocking.RequestId;
		Result.BlockingOperation = Blocking.Operation;
		Result.BlockingPhase = Blocking.Phase;
	}
	LastRejection = Result;
	UE_LOG(LogExtendedEOS, Verbose, TEXT("EOSOperation Instance=%s Op=%s Request=%lld Phase=Rejected Code=%s"),
		*GetOwningEOSInstanceName().ToString(), *FEEOSNativeOperationLease::SafeField(Operation.ToString()), Result.RequestId,
		*StaticEnum<EEOSOperationCode>()->GetNameStringByValue(static_cast<int64>(Code)));
	if (Code != EEOSOperationCode::AlreadyLoggedIn && Code != EEOSOperationCode::AlreadyInLobby)
		FEEOSNativeOperationLease::ReportRepeated(GetOwningEOSInstanceName(), Operation, FName(*StaticEnum<EEOSOperationCode>()->GetNameStringByValue(int64(Code))));
	if (!bEOSShuttingDown) OnOperationRejected.Broadcast(Result);
}
