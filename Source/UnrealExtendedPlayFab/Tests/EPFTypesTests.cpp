// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Shared/EPFTypes.h"


// ═══════════════════════════════════════════════════════════════════════════════
// FEPFError — Factory Methods
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEPFErrorNoneTest,
	"UnrealExtendedPlayFab.Types.FEPFError.None",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEPFErrorNoneTest::RunTest(const FString& Parameters)
{
	FEPFError Error = FEPFError::None();

	TestFalse(TEXT("None error should have bHasError = false"), Error.bHasError);
	TestTrue(TEXT("None error should have empty ErrorMessage"), Error.ErrorMessage.IsEmpty());
	TestTrue(TEXT("None error should have empty ErrorCode"), Error.ErrorCode.IsEmpty());
	TestEqual(TEXT("None error should have HttpStatusCode = 0"), Error.HttpStatusCode, 0);
	TestTrue(TEXT("None error should have empty ErrorDetails"), Error.ErrorDetails.IsEmpty());
	TestTrue(TEXT("None error should have empty RawResponse"), Error.RawResponse.IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEPFErrorFailureTest,
	"UnrealExtendedPlayFab.Types.FEPFError.Failure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEPFErrorFailureTest::RunTest(const FString& Parameters)
{
	FEPFError Error = FEPFError::Failure(TEXT("Connection timeout"), TEXT("TIMEOUT"), 503);

	TestTrue(TEXT("Failure error should have bHasError = true"), Error.bHasError);
	TestEqual(TEXT("ErrorMessage should match"), Error.ErrorMessage, FString(TEXT("Connection timeout")));
	TestEqual(TEXT("ErrorCode should match"), Error.ErrorCode, FString(TEXT("TIMEOUT")));
	TestEqual(TEXT("HttpStatusCode should match"), Error.HttpStatusCode, 503);
	TestEqual(TEXT("ErrorName should match ErrorCode"), Error.ErrorName, FString(TEXT("TIMEOUT")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEPFErrorFailureDefaultsTest,
	"UnrealExtendedPlayFab.Types.FEPFError.FailureDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEPFErrorFailureDefaultsTest::RunTest(const FString& Parameters)
{
	FEPFError Error = FEPFError::Failure(TEXT("Something broke"));

	TestTrue(TEXT("Should have error"), Error.bHasError);
	TestEqual(TEXT("ErrorMessage should match"), Error.ErrorMessage, FString(TEXT("Something broke")));
	TestTrue(TEXT("ErrorCode should be empty when not provided"), Error.ErrorCode.IsEmpty());
	TestEqual(TEXT("HttpStatusCode should default to 0"), Error.HttpStatusCode, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEPFErrorStringConstructorTest,
	"UnrealExtendedPlayFab.Types.FEPFError.StringConstructor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEPFErrorStringConstructorTest::RunTest(const FString& Parameters)
{
	FEPFError Error(TEXT("Constructed from string"));

	TestTrue(TEXT("Should have error"), Error.bHasError);
	TestEqual(TEXT("ErrorMessage should match"), Error.ErrorMessage, FString(TEXT("Constructed from string")));

	FEPFError EmptyError{FString()};
	TestFalse(TEXT("Empty string should not flag error"), EmptyError.bHasError);
	TestTrue(TEXT("ErrorMessage should be empty"), EmptyError.ErrorMessage.IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEPFErrorDefaultConstructorTest,
	"UnrealExtendedPlayFab.Types.FEPFError.DefaultConstructor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEPFErrorDefaultConstructorTest::RunTest(const FString& Parameters)
{
	FEPFError Error;

	TestFalse(TEXT("Default bHasError should be false"), Error.bHasError);
	TestEqual(TEXT("Default HttpStatusCode should be 0"), Error.HttpStatusCode, 0);
	TestTrue(TEXT("Default ErrorCode should be empty"), Error.ErrorCode.IsEmpty());
	TestTrue(TEXT("Default ErrorName should be empty"), Error.ErrorName.IsEmpty());
	TestTrue(TEXT("Default ErrorMessage should be empty"), Error.ErrorMessage.IsEmpty());
	TestTrue(TEXT("Default ErrorDetails should be empty"), Error.ErrorDetails.IsEmpty());
	TestTrue(TEXT("Default RawResponse should be empty"), Error.RawResponse.IsEmpty());

	return true;
}


// ═══════════════════════════════════════════════════════════════════════════════
// FEPFResult — Factory Methods
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEPFResultSuccessTest,
	"UnrealExtendedPlayFab.Types.FEPFResult.Success",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEPFResultSuccessTest::RunTest(const FString& Parameters)
{
	FEPFResult Result = FEPFResult::Success();

	TestTrue(TEXT("Success result should have bSuccess = true"), Result.bSuccess);
	TestTrue(TEXT("Success result should have empty ErrorMessage"), Result.ErrorMessage.IsEmpty());
	TestTrue(TEXT("Success result should have empty ErrorCode"), Result.ErrorCode.IsEmpty());
	TestEqual(TEXT("Success result should have HttpStatusCode = 0"), Result.HttpStatusCode, 0);
	TestTrue(TEXT("Success result should have empty ErrorDetails"), Result.ErrorDetails.IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEPFResultFailureStringTest,
	"UnrealExtendedPlayFab.Types.FEPFResult.FailureString",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEPFResultFailureStringTest::RunTest(const FString& Parameters)
{
	FEPFResult Result = FEPFResult::Failure(TEXT("Not authenticated"), TEXT("AUTH_REQUIRED"));

	TestFalse(TEXT("Failure result should have bSuccess = false"), Result.bSuccess);
	TestEqual(TEXT("ErrorMessage should match"), Result.ErrorMessage, FString(TEXT("Not authenticated")));
	TestEqual(TEXT("ErrorCode should match"), Result.ErrorCode, FString(TEXT("AUTH_REQUIRED")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEPFResultFailureNoCodeTest,
	"UnrealExtendedPlayFab.Types.FEPFResult.FailureNoCode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEPFResultFailureNoCodeTest::RunTest(const FString& Parameters)
{
	FEPFResult Result = FEPFResult::Failure(TEXT("Generic error"));

	TestFalse(TEXT("Failure result should have bSuccess = false"), Result.bSuccess);
	TestEqual(TEXT("ErrorMessage should match"), Result.ErrorMessage, FString(TEXT("Generic error")));
	TestTrue(TEXT("ErrorCode should be empty"), Result.ErrorCode.IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEPFResultFailureTCHARTest,
	"UnrealExtendedPlayFab.Types.FEPFResult.FailureTCHAR",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEPFResultFailureTCHARTest::RunTest(const FString& Parameters)
{
	// This specifically tests the TCHAR* overload that resolves TEXT("...") ambiguity
	FEPFResult Result = FEPFResult::Failure(TEXT("TEXT literal error"));

	TestFalse(TEXT("Should fail"), Result.bSuccess);
	TestEqual(TEXT("ErrorMessage should match"), Result.ErrorMessage, FString(TEXT("TEXT literal error")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEPFResultFailureFromErrorTest,
	"UnrealExtendedPlayFab.Types.FEPFResult.FailureFromError",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEPFResultFailureFromErrorTest::RunTest(const FString& Parameters)
{
	FEPFError Error = FEPFError::Failure(TEXT("Rate limited"), TEXT("RateLimited"), 429);
	Error.ErrorDetails = TEXT("{\"retryAfter\": 5}");

	FEPFResult Result = FEPFResult::Failure(Error);

	TestFalse(TEXT("Should fail"), Result.bSuccess);
	TestEqual(TEXT("ErrorMessage preserved from FEPFError"), Result.ErrorMessage, FString(TEXT("Rate limited")));
	TestEqual(TEXT("ErrorCode preserved from FEPFError"), Result.ErrorCode, FString(TEXT("RateLimited")));
	TestEqual(TEXT("HttpStatusCode preserved from FEPFError"), Result.HttpStatusCode, 429);
	TestEqual(TEXT("ErrorDetails preserved from FEPFError"), Result.ErrorDetails, FString(TEXT("{\"retryAfter\": 5}")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEPFResultDefaultConstructorTest,
	"UnrealExtendedPlayFab.Types.FEPFResult.DefaultConstructor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEPFResultDefaultConstructorTest::RunTest(const FString& Parameters)
{
	FEPFResult Result;

	TestFalse(TEXT("Default bSuccess should be false"), Result.bSuccess);
	TestEqual(TEXT("Default HttpStatusCode should be 0"), Result.HttpStatusCode, 0);
	TestTrue(TEXT("Default ErrorMessage should be empty"), Result.ErrorMessage.IsEmpty());
	TestTrue(TEXT("Default ErrorCode should be empty"), Result.ErrorCode.IsEmpty());
	TestTrue(TEXT("Default ErrorDetails should be empty"), Result.ErrorDetails.IsEmpty());

	return true;
}


// ═══════════════════════════════════════════════════════════════════════════════
// FEPFAuthContext
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEPFAuthContextDefaultsTest,
	"UnrealExtendedPlayFab.Types.FEPFAuthContext.Defaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEPFAuthContextDefaultsTest::RunTest(const FString& Parameters)
{
	FEPFAuthContext Ctx;

	TestTrue(TEXT("Default SessionTicket empty"), Ctx.SessionTicket.IsEmpty());
	TestTrue(TEXT("Default EntityToken empty"), Ctx.EntityToken.IsEmpty());
	TestTrue(TEXT("Default PlayFabId empty"), Ctx.PlayFabId.IsEmpty());
	TestTrue(TEXT("Default EntityId empty"), Ctx.EntityId.IsEmpty());
	TestTrue(TEXT("Default EntityType empty"), Ctx.EntityType.IsEmpty());
	TestTrue(TEXT("Default DeveloperSecretKey empty"), Ctx.DeveloperSecretKey.IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEPFAuthContextHasTokenTest,
	"UnrealExtendedPlayFab.Types.FEPFAuthContext.HasTokenChecks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEPFAuthContextHasTokenTest::RunTest(const FString& Parameters)
{
	FEPFAuthContext Ctx;

	TestFalse(TEXT("Empty context should not have session ticket"), Ctx.HasSessionTicket());
	TestFalse(TEXT("Empty context should not have entity token"), Ctx.HasEntityToken());

	Ctx.SessionTicket = TEXT("session-abc-123");
	TestTrue(TEXT("Should have session ticket after setting it"), Ctx.HasSessionTicket());
	TestFalse(TEXT("Should still not have entity token"), Ctx.HasEntityToken());

	Ctx.EntityToken = TEXT("entity-def-456");
	TestTrue(TEXT("Should have entity token after setting it"), Ctx.HasEntityToken());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEPFAuthContextResetTest,
	"UnrealExtendedPlayFab.Types.FEPFAuthContext.Reset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEPFAuthContextResetTest::RunTest(const FString& Parameters)
{
	FEPFAuthContext Ctx;
	Ctx.SessionTicket = TEXT("ticket");
	Ctx.EntityToken = TEXT("token");
	Ctx.PlayFabId = TEXT("pfid");
	Ctx.EntityId = TEXT("eid");
	Ctx.EntityType = TEXT("title_player_account");
	Ctx.DeveloperSecretKey = TEXT("secret");

	Ctx.Reset();

	TestTrue(TEXT("SessionTicket should be empty after reset"), Ctx.SessionTicket.IsEmpty());
	TestTrue(TEXT("EntityToken should be empty after reset"), Ctx.EntityToken.IsEmpty());
	TestTrue(TEXT("PlayFabId should be empty after reset"), Ctx.PlayFabId.IsEmpty());
	TestTrue(TEXT("EntityId should be empty after reset"), Ctx.EntityId.IsEmpty());
	TestTrue(TEXT("EntityType should be empty after reset"), Ctx.EntityType.IsEmpty());
	TestTrue(TEXT("DeveloperSecretKey should be empty after reset"), Ctx.DeveloperSecretKey.IsEmpty());
	TestFalse(TEXT("HasSessionTicket should be false after reset"), Ctx.HasSessionTicket());
	TestFalse(TEXT("HasEntityToken should be false after reset"), Ctx.HasEntityToken());

	return true;
}


// ═══════════════════════════════════════════════════════════════════════════════
// Data Struct Default Values
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEPFStatisticDefaultsTest,
	"UnrealExtendedPlayFab.Types.FEPFStatistic.Defaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEPFStatisticDefaultsTest::RunTest(const FString& Parameters)
{
	FEPFStatistic Stat;

	TestTrue(TEXT("StatName default empty"), Stat.StatName.IsEmpty());
	TestEqual(TEXT("Value default 0"), Stat.Value, 0);
	TestEqual(TEXT("Version default 0"), Stat.Version, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEPFLeaderboardEntryDefaultsTest,
	"UnrealExtendedPlayFab.Types.FEPFLeaderboardEntry.Defaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEPFLeaderboardEntryDefaultsTest::RunTest(const FString& Parameters)
{
	FEPFLeaderboardEntry Entry;

	TestEqual(TEXT("Position default 0"), Entry.Position, 0);
	TestTrue(TEXT("PlayFabId default empty"), Entry.PlayFabId.IsEmpty());
	TestTrue(TEXT("DisplayName default empty"), Entry.DisplayName.IsEmpty());
	TestEqual(TEXT("StatValue default 0"), Entry.StatValue, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEPFPlayerDataDefaultsTest,
	"UnrealExtendedPlayFab.Types.FEPFPlayerData.Defaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEPFPlayerDataDefaultsTest::RunTest(const FString& Parameters)
{
	FEPFPlayerData Data;

	TestTrue(TEXT("Key default empty"), Data.Key.IsEmpty());
	TestTrue(TEXT("Value default empty"), Data.Value.IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEPFCloudScriptResultDefaultsTest,
	"UnrealExtendedPlayFab.Types.FEPFCloudScriptResult.Defaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEPFCloudScriptResultDefaultsTest::RunTest(const FString& Parameters)
{
	FEPFCloudScriptResult Result;

	TestTrue(TEXT("FunctionName default empty"), Result.FunctionName.IsEmpty());
	TestTrue(TEXT("ResultJson default empty"), Result.ResultJson.IsEmpty());
	TestFalse(TEXT("bError default false"), Result.bError);
	TestTrue(TEXT("ErrorMessage default empty"), Result.ErrorMessage.IsEmpty());
	TestEqual(TEXT("ExecutionTimeMs default 0"), Result.ExecutionTimeMs, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEPFAnalyticsEventDefaultsTest,
	"UnrealExtendedPlayFab.Types.FEPFAnalyticsEvent.Defaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEPFAnalyticsEventDefaultsTest::RunTest(const FString& Parameters)
{
	FEPFAnalyticsEvent Event;

	TestTrue(TEXT("EventName default empty"), Event.EventName.IsEmpty());
	TestEqual(TEXT("Body should be empty"), Event.Body.Num(), 0);

	return true;
}
