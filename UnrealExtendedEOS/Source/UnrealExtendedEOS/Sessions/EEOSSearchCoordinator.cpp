// Copyright Kemal Erdem YILMAZ. All Rights Reserved.
#include "EEOSSearchCoordinator.h"
#include "UnrealExtendedEOS.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Containers/Ticker.h"
#include "IOnlineSubsystemEOS.h"
#include "IEOSSDKManager.h"

namespace
{
	struct FRetiredSearch : TSharedFromThis<FRetiredSearch>
	{
		IOnlineSessionPtr Sessions;
		TSharedPtr<IEOSPlatformHandle, ESPMode::ThreadSafe> Platform;
		FEEOSNativeOperationLease Lease;
		FDelegateHandle Completion;
		FTSTicker::FDelegateHandle Watcher;
		FName Instance;
		double Started = FPlatformTime::Seconds();
		bool bWarned = false;
		void Finish(const TCHAR* Reason)
		{
			if (!Lease.IsValid()) return;
			UE_LOG(LogExtendedEOS, Verbose, TEXT("EOSCallback Instance=%s Op=FindRetirement Request=%lld Generation=0 Disposition=ShutdownInternalOnly Attribution=PluginLeaseOnly ExternalNativeCallerIdentifiable=0"),
				*FEEOSNativeOperationLease::SafeField(Instance.ToString()), Lease.GetRequestId());
			UE_LOG(LogExtendedEOS, Log, TEXT("EOSSearch Request=%lld Phase=Retired Reason=%s ElapsedMs=%.0f"), Lease.GetRequestId(), Reason, (FPlatformTime::Seconds() - Started) * 1000);
			const auto KeepAlive = AsShared();
			if (Sessions.IsValid()) Sessions->ClearOnFindSessionsCompleteDelegate_Handle(Completion);
			Completion.Reset();
			if (Watcher.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(Watcher);
			Watcher.Reset();
			Lease.Reset();
			Sessions.Reset();
		}
		void Arm()
		{
			const auto Self = AsShared();
			Completion = Sessions->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateLambda(
				[Self](bool) { Self->Finish(TEXT("NativeCompletionDrained")); }));
			Watcher = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self](float)
			{
				IOnlineSubsystem* OSS = IOnlineSubsystem::DoesInstanceExist(Self->Instance) ? IOnlineSubsystem::Get(Self->Instance) : nullptr;
				if (!OSS || OSS->GetSessionInterface() != Self->Sessions) { Self->Finish(TEXT("OriginalInterfaceRetired")); return false; }
				if (!Self->bWarned && FPlatformTime::Seconds() - Self->Started > 15)
				{
					Self->bWarned = true;
					UE_LOG(LogExtendedEOS, Warning, TEXT("EOSSearch Request=%lld Phase=RetirementWaiting; original native search still owns the interface"), Self->Lease.GetRequestId());
				}
				return true;
			}), 1.0f);
		}
	};
}
bool UEEOSSearchCoordinator::TryAcquire(FName OwnerTag)
{
	// Kept for legacy local-only callers. Runtime consumers use the native-interface overload.
	if (OwnerTag.IsNone() || !CurrentOwner.IsNone()) return false;
	CurrentOwner = OwnerTag;
	return true;
}
bool UEEOSSearchCoordinator::TryAcquire(FName OwnerTag, const IOnlineSessionPtr& Sessions, UObject* OperationOwner, FName Operation)
{
	if (OwnerTag.IsNone() || !CurrentOwner.IsNone() || !Sessions.IsValid()) return false;
	if (!SearchLease.TryAcquire(Sessions.Get(), TEXT("Search"), OperationOwner ? OperationOwner : this, Operation.IsNone() ? OwnerTag : Operation)) return false;
	HeldSessions = Sessions;
	CurrentOwner = OwnerTag;
	return true;
}
void UEEOSSearchCoordinator::Release(FName OwnerTag)
{
	if (OwnerTag != CurrentOwner) return;
	CurrentOwner = NAME_None;
	SearchLease.Reset();
	HeldSessions.Reset();
}
void UEEOSSearchCoordinator::Retire(FName OwnerTag)
{
	if (OwnerTag != CurrentOwner) return;
	if (SearchLease.IsValid() && HeldSessions.IsValid())
	{
		const auto Retired = MakeShared<FRetiredSearch>();
		Retired->Sessions = HeldSessions;
		Retired->Lease = SearchLease;
		Retired->Instance = Online::GetUtils() ? Online::GetUtils()->GetOnlineIdentifier(GetWorld(), EOS_SUBSYSTEM) : EOS_SUBSYSTEM;
		IOnlineSubsystem* OSS = IOnlineSubsystem::DoesInstanceExist(Retired->Instance) ? IOnlineSubsystem::Get(Retired->Instance) : nullptr;
		if (OSS && OSS->GetSubsystemName() == EOS_SUBSYSTEM && OSS->GetSessionInterface() == HeldSessions)
			Retired->Platform = static_cast<IOnlineSubsystemEOS*>(OSS)->GetEOSPlatformHandle();
		// UE 5.8 CancelFindSessions nulls CurrentSessionSearch and reports acceptance on the next tick.
		// That callback is not proof that the outstanding SDK Find has retired. Drain its original
		// Find completion instead, retaining this interface-wide lease without accepting new searches.
		UE_LOG(LogExtendedEOS, Log, TEXT("EOSSearch Request=%lld Phase=CancellationAccepted NativeCancelSubmitted=0 Policy=DrainOriginalFindCompletion"), SearchLease.GetRequestId());
		Retired->Arm();
	}
	Release(OwnerTag);
}
void UEEOSSearchCoordinator::Deinitialize()
{
	Retire(CurrentOwner);
	Super::Deinitialize();
}
