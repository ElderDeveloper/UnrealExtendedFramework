// Copyright Kemal Erdem YILMAZ. All Rights Reserved.
#include "EEOSNativeOperation.h"
#include "EEOSLog.h"
#include "HAL/PlatformTime.h"
#include "EEOSSettings.h"
#include "EEOSSubsystem.h"

namespace
{
	struct FRepeatedDiagnostic { FName Instance, Operation, Reason; int32 Count = 0; double LastSummary = 0; };
	TMap<FString, FRepeatedDiagnostic> RepeatedDiagnostics;
	struct FNativeKey
	{
		const void* Interface = nullptr;
		FName Scope;
		bool operator==(const FNativeKey& Other) const { return Interface == Other.Interface && Scope == Other.Scope; }
		friend uint32 GetTypeHash(const FNativeKey& Key) { return HashCombine(PointerHash(Key.Interface), GetTypeHash(Key.Scope)); }
	};
	TMap<FNativeKey, TWeakPtr<FEEOSNativeOperationState>>& Registry()
	{
		// The last lease removes its entry; registry lifetime is independent of UObject teardown.
		static auto* Entries = new TMap<FNativeKey, TWeakPtr<FEEOSNativeOperationState>>;
		return *Entries;
	}
}
struct FEEOSNativeOperationState
{
	FNativeKey Key;
	int64 RequestId = 0;
	FName Operation;
	TWeakObjectPtr<UObject> Owner;
	double Started = 0;
	~FEEOSNativeOperationState() { Registry().Remove(Key); }
};
int64 FEEOSNativeOperationLease::NextRequestId()
{
	check(IsInGameThread());
	static int64 Sequence = 0;
	return ++Sequence;
}
bool FEEOSNativeOperationLease::TryAcquire(const void* Interface, FName Scope, UObject* Owner, FName Operation, int64 RequestId)
{
	check(IsInGameThread());
	if (State.IsValid() || !Interface || Scope.IsNone()) return false;
	const FNativeKey Key{Interface, Scope};
	if (const auto* Existing = Registry().Find(Key))
	{
		if (const auto Active = Existing->Pin())
		{
			UE_LOG(LogExtendedEOS, Verbose, TEXT("EOSAdmission Op=%s Phase=Rejected Code=Busy Scope=%s ActiveRequest=%lld ActiveOp=%s ElapsedMs=%.0f"),
				*FEEOSNativeOperationLease::SafeField(Operation.ToString()), *FEEOSNativeOperationLease::SafeField(Scope.ToString()), Active->RequestId, *FEEOSNativeOperationLease::SafeField(Active->Operation.ToString()), (FPlatformTime::Seconds() - Active->Started) * 1000);
			return false;
		}
	}
	State = MakeShared<FEEOSNativeOperationState>();
	State->Key = Key;
	State->RequestId = RequestId ? RequestId : NextRequestId();
	State->Operation = Operation;
	State->Owner = Owner;
	State->Started = FPlatformTime::Seconds();
	Registry().Add(Key, State);
	return true;
}
int64 FEEOSNativeOperationLease::GetRequestId() const { return State.IsValid() ? State->RequestId : 0; }
FName FEEOSNativeOperationLease::GetOperation() const { return State.IsValid() ? State->Operation : NAME_None; }
FEEOSOperationOutcome FEEOSNativeOperationLease::DescribeOwner(const void* Interface, FName Scope)
{
	check(IsInGameThread());
	FEEOSOperationOutcome Result;
	if (const auto* Entry = Registry().Find(FNativeKey{Interface, Scope}))
	{
		if (const auto Active = Entry->Pin())
		{
			if (const auto* Owner = Cast<UEEOSSubsystem>(Active->Owner.Get()))
				Result = Owner->GetActiveOperationOutcome(Active->Operation);
			if (Result.RequestId != Active->RequestId)
			{
				Result = FEEOSOperationOutcome();
				Result.RequestId = Active->RequestId; Result.Operation = Active->Operation;
				Result.Phase = TEXT("WaitingForNativeCallback");
			}
		}
	}
	return Result;
}
FString FEEOSNativeOperationLease::SafeField(const FString& Value)
{
	// Only bounded scalars belong here. Payloads/connection strings must never be passed in.
	for (const TCHAR* Secret : {TEXT("token="), TEXT("password="), TEXT("secret="), TEXT("authorization:"), TEXT("bearer ")})
		if (Value.Contains(Secret, ESearchCase::IgnoreCase)) return TEXT("[redacted]");
	FString Result = Value.Left(160);
	for (TCHAR& Character : Result) if (Character < TCHAR(32) || Character == TCHAR(127)) Character = TEXT(' ');
	return Result;
}
void FEEOSNativeOperationLease::ReportRepeated(FName Instance, FName Operation, FName Reason)
{
	const auto* Settings = UEEOSSettings::Get();
#if UE_BUILD_SHIPPING
	if (!Settings->bEnableShippingDiagnostics) return;
#endif
	if (!Settings->bEnableOperationLogging) return;
	const FString Key = Instance.ToString() + TEXT("/") + Operation.ToString() + TEXT("/") + Reason.ToString();
	if (!RepeatedDiagnostics.Contains(Key) && RepeatedDiagnostics.Num() >= 128) FlushDiagnosticSummaries();
	if (!RepeatedDiagnostics.Contains(Key) && RepeatedDiagnostics.Num() >= 128)
	{
		const auto Victim = RepeatedDiagnostics.CreateConstIterator();
		const auto& Pending = Victim.Value();
		if (Pending.Count > 0) UE_LOG(LogExtendedEOS, Warning, TEXT("EOSDiagnostic Instance=%s Op=%s Reason=%s RepeatedCount=%d SummaryEvicted=1"),
			*FEEOSNativeOperationLease::SafeField(Pending.Instance.ToString()), *FEEOSNativeOperationLease::SafeField(Pending.Operation.ToString()), *FEEOSNativeOperationLease::SafeField(Pending.Reason.ToString()), Pending.Count);
		RepeatedDiagnostics.Remove(Victim.Key());
	}
	auto& Entry = RepeatedDiagnostics.FindOrAdd(Key);
	Entry.Instance = Instance; Entry.Operation = Operation; Entry.Reason = Reason; ++Entry.Count;
	if (Entry.LastSummary == 0)
	{
		Entry.LastSummary = FPlatformTime::Seconds();
		UE_LOG(LogExtendedEOS, Warning, TEXT("EOSDiagnostic Instance=%s Op=%s Reason=%s Count=1"), *FEEOSNativeOperationLease::SafeField(Instance.ToString()), *FEEOSNativeOperationLease::SafeField(Operation.ToString()), *FEEOSNativeOperationLease::SafeField(Reason.ToString()));
		Entry.Count = 0;
	}
	FlushDiagnosticSummaries();
}
void FEEOSNativeOperationLease::FlushDiagnosticSummaries()
{
	const double Now = FPlatformTime::Seconds();
	const double Interval = FMath::Clamp(double(UEEOSSettings::Get()->DiagnosticSummaryIntervalSeconds), 1.0, 3600.0);
	for (auto& Pair : RepeatedDiagnostics)
	{
		auto& Entry = Pair.Value;
		if (Entry.Count > 0 && Now - Entry.LastSummary >= Interval)
		{
			UE_LOG(LogExtendedEOS, Warning, TEXT("EOSDiagnostic Instance=%s Op=%s Reason=%s RepeatedCount=%d IntervalSeconds=%.1f"),
				*FEEOSNativeOperationLease::SafeField(Entry.Instance.ToString()), *FEEOSNativeOperationLease::SafeField(Entry.Operation.ToString()), *FEEOSNativeOperationLease::SafeField(Entry.Reason.ToString()), Entry.Count, Now - Entry.LastSummary);
			Entry.Count = 0; Entry.LastSummary = Now;
		}
	}
}
