// Copyright Kemal Erdem YILMAZ. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "EEOSOperationTypes.h"
struct FEEOSNativeOperationState;

/** Interface-wide admission across GameInstances. Retain the native interface while holding
 * this lease; copy the lease into late cleanup before its original owner is removed.
 * Uncontrolled native callers still cannot be attributed by name-only callbacks. */
class EXTENDEDEOSSHARED_API FEEOSNativeOperationLease
{
public:
	bool TryAcquire(const void* Interface, FName Scope, UObject* Owner, FName Operation, int64 RequestId = 0);
	void Reset() { State.Reset(); }
	bool IsValid() const { return State.IsValid(); }
	int64 GetRequestId() const;
	FName GetOperation() const;
	static int64 NextRequestId();
	static FString SafeField(const FString& Value);
	/** Reasons must be stable codes, never arbitrary SDK/user text. Counts flush on the shared ticker. */
	static void ReportRepeated(FName Instance, FName Operation, FName Reason);
	static void FlushDiagnosticSummaries();
	static FEEOSOperationOutcome DescribeOwner(const void* Interface, FName Scope);
private:
	TSharedPtr<FEEOSNativeOperationState> State;
};
