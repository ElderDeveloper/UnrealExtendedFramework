// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "BlueprintActions/EPFAsyncTypes.h"
#include "Shared/EPFTypes.h"


// ═══════════════════════════════════════════════════════════════════════════════
// MakeEPFAsyncError — From FEPFResult
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMakeEPFAsyncErrorFromSuccessTest,
	"UnrealExtendedPlayFab.Helpers.MakeEPFAsyncError.FromSuccess",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMakeEPFAsyncErrorFromSuccessTest::RunTest(const FString& Parameters)
{
	FEPFResult SuccessResult = FEPFResult::Success();
	FEPFError Error = MakeEPFAsyncError(SuccessResult);

	TestFalse(TEXT("Error from success should have bHasError = false"), Error.bHasError);
	TestTrue(TEXT("ErrorMessage should be empty"), Error.ErrorMessage.IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMakeEPFAsyncErrorFromFailureTest,
	"UnrealExtendedPlayFab.Helpers.MakeEPFAsyncError.FromFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMakeEPFAsyncErrorFromFailureTest::RunTest(const FString& Parameters)
{
	FEPFResult FailResult = FEPFResult::Failure(TEXT("Not found"), TEXT("NotFound"));
	FailResult.HttpStatusCode = 404;
	FailResult.ErrorDetails = TEXT("Resource was not found");

	FEPFError Error = MakeEPFAsyncError(FailResult);

	TestTrue(TEXT("Should have error"), Error.bHasError);
	TestEqual(TEXT("ErrorMessage preserved"), Error.ErrorMessage, FString(TEXT("Not found")));
	TestEqual(TEXT("ErrorCode preserved"), Error.ErrorCode, FString(TEXT("NotFound")));
	TestEqual(TEXT("HttpStatusCode preserved"), Error.HttpStatusCode, 404);
	TestEqual(TEXT("ErrorDetails preserved"), Error.ErrorDetails, FString(TEXT("Resource was not found")));
	TestEqual(TEXT("ErrorName should match ErrorCode"), Error.ErrorName, FString(TEXT("NotFound")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMakeEPFAsyncErrorFallbackMessageTest,
	"UnrealExtendedPlayFab.Helpers.MakeEPFAsyncError.FallbackMessage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMakeEPFAsyncErrorFallbackMessageTest::RunTest(const FString& Parameters)
{
	// FEPFResult with empty ErrorMessage but bSuccess = false
	FEPFResult FailResult;
	FailResult.bSuccess = false;

	FEPFError Error = MakeEPFAsyncError(FailResult, TEXT("Custom fallback"));

	TestTrue(TEXT("Should have error"), Error.bHasError);
	TestEqual(TEXT("Should use fallback message"), Error.ErrorMessage, FString(TEXT("Custom fallback")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMakeEPFAsyncErrorDefaultFallbackTest,
	"UnrealExtendedPlayFab.Helpers.MakeEPFAsyncError.DefaultFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMakeEPFAsyncErrorDefaultFallbackTest::RunTest(const FString& Parameters)
{
	FEPFResult FailResult;
	FailResult.bSuccess = false;

	FEPFError Error = MakeEPFAsyncError(FailResult);

	TestTrue(TEXT("Should have error"), Error.bHasError);
	TestEqual(TEXT("Should use 'Request failed' default"), Error.ErrorMessage, FString(TEXT("Request failed")));

	return true;
}


// ═══════════════════════════════════════════════════════════════════════════════
// MakeEPFAsyncError — From FString (convenience overload)
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMakeEPFAsyncErrorFromStringTest,
	"UnrealExtendedPlayFab.Helpers.MakeEPFAsyncError.FromString",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMakeEPFAsyncErrorFromStringTest::RunTest(const FString& Parameters)
{
	FEPFError Error = MakeEPFAsyncError(FString(TEXT("Subsystem not available")));

	TestTrue(TEXT("Should have error"), Error.bHasError);
	TestEqual(TEXT("ErrorMessage should match"), Error.ErrorMessage, FString(TEXT("Subsystem not available")));

	return true;
}


// ═══════════════════════════════════════════════════════════════════════════════
// Error ↔ Result Round-Trip
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEPFErrorResultRoundTripTest,
	"UnrealExtendedPlayFab.Helpers.ErrorResultRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEPFErrorResultRoundTripTest::RunTest(const FString& Parameters)
{
	// Error → Result → Error should preserve all fields
	FEPFError Original = FEPFError::Failure(TEXT("Server error"), TEXT("InternalServerError"), 500);
	Original.ErrorDetails = TEXT("{\"trace\": \"abc\"}");

	FEPFResult Result = FEPFResult::Failure(Original);

	TestFalse(TEXT("Result should fail"), Result.bSuccess);
	TestEqual(TEXT("ErrorMessage preserved"), Result.ErrorMessage, FString(TEXT("Server error")));
	TestEqual(TEXT("ErrorCode preserved"), Result.ErrorCode, FString(TEXT("InternalServerError")));
	TestEqual(TEXT("HttpStatusCode preserved"), Result.HttpStatusCode, 500);
	TestEqual(TEXT("ErrorDetails preserved"), Result.ErrorDetails, FString(TEXT("{\"trace\": \"abc\"}")));

	// Now round-trip back via MakeEPFAsyncError
	FEPFError Recovered = MakeEPFAsyncError(Result);

	TestTrue(TEXT("Recovered should have error"), Recovered.bHasError);
	TestEqual(TEXT("Recovered ErrorMessage"), Recovered.ErrorMessage, FString(TEXT("Server error")));
	TestEqual(TEXT("Recovered ErrorCode"), Recovered.ErrorCode, FString(TEXT("InternalServerError")));
	TestEqual(TEXT("Recovered HttpStatusCode"), Recovered.HttpStatusCode, 500);
	TestEqual(TEXT("Recovered ErrorDetails"), Recovered.ErrorDetails, FString(TEXT("{\"trace\": \"abc\"}")));

	return true;
}
