// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Shared/EPFLogFormatting.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEPFLogFormattingRedactTest,
	"UnrealExtendedPlayFab.LogFormatting.Redact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEPFLogFormattingRedactTest::RunTest(const FString& Parameters)
{
	// A pretty-printed LoginWithSteam request.
	const FString Request = TEXT("{\n\t\"SteamTicket\": \"14000000836D7E68\",\n\t\"CreateAccount\": true,\n\t\"TitleId\": \"1CA80E\"\n}");
	const FString RedactedRequest = EPFLogFormatting::RedactSecrets(Request);
	TestFalse(TEXT("The Steam ticket is gone"), RedactedRequest.Contains(TEXT("14000000836D7E68")));
	TestTrue(TEXT("The field stays, with a marker"), RedactedRequest.Contains(TEXT("\"SteamTicket\": \"<redacted>\"")));
	TestTrue(TEXT("Other fields and the layout stay"), RedactedRequest.Contains(TEXT("\n\t\"TitleId\": \"1CA80E\"\n}")));

	// A compact login response: a session ticket, and the entity token nested in its own wrapper.
	const FString Response = TEXT("{\"data\":{\"SessionTicket\":\"B607-6F96\",\"PlayFabId\":\"B6076E3E\",\"EntityToken\":{\"EntityToken\":\"NHx4M1cy\",\"TokenExpiration\":\"2026-09-25T11:36:56Z\"}}}");
	const FString RedactedResponse = EPFLogFormatting::RedactSecrets(Response);
	TestFalse(TEXT("The session ticket is gone"), RedactedResponse.Contains(TEXT("B607-6F96")));
	TestFalse(TEXT("The nested entity token is gone"), RedactedResponse.Contains(TEXT("NHx4M1cy")));
	TestTrue(TEXT("The player id stays"), RedactedResponse.Contains(TEXT("\"PlayFabId\":\"B6076E3E\"")));
	TestTrue(TEXT("The expiry next to the token stays"), RedactedResponse.Contains(TEXT("\"TokenExpiration\":\"2026-09-25T11:36:56Z\"")));

	// Names match exactly (ignoring case); a value that happens to spell a secret name is not a field.
	const FString Lookalikes = TEXT("{\"TicketId\":\"t1\",\"ContinuationToken\":\"c1\",\"Name\":\"SessionTicket\",\"password\":\"hunter2\"}");
	const FString RedactedLookalikes = EPFLogFormatting::RedactSecrets(Lookalikes);
	TestTrue(TEXT("TicketId stays"), RedactedLookalikes.Contains(TEXT("\"TicketId\":\"t1\"")));
	TestTrue(TEXT("ContinuationToken stays"), RedactedLookalikes.Contains(TEXT("\"ContinuationToken\":\"c1\"")));
	TestTrue(TEXT("A value spelling a secret name stays"), RedactedLookalikes.Contains(TEXT("\"Name\":\"SessionTicket\"")));
	TestFalse(TEXT("A lower-case password is still caught"), RedactedLookalikes.Contains(TEXT("hunter2")));

	// Escaped quotes inside a secret, and a secret cut off by truncation.
	const FString Escaped = TEXT("{\"Password\":\"a\\\"b\\\\\",\"Next\":1}");
	TestEqual(TEXT("An escaped quote does not end the value early"), EPFLogFormatting::RedactSecrets(Escaped), FString(TEXT("{\"Password\":\"<redacted>\",\"Next\":1}")));

	const FString Cut = TEXT("{\"SessionTicket\":\"B607-6F96-trunc");
	TestFalse(TEXT("An unterminated secret is still removed"), EPFLogFormatting::RedactSecrets(Cut).Contains(TEXT("B607")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEPFLogFormattingCapTest,
	"UnrealExtendedPlayFab.LogFormatting.Cap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEPFLogFormattingCapTest::RunTest(const FString& Parameters)
{
	const FString Body = FString::ChrN(100, TEXT('x'));

	const FString Capped = EPFLogFormatting::FormatBodyForLog(Body, 40);
	TestTrue(TEXT("The first 40 characters stay"), Capped.StartsWith(FString::ChrN(40, TEXT('x')) + TEXT("\n")));
	TestTrue(TEXT("The note says how much was left out"), Capped.Contains(TEXT("60 more characters")));

	TestEqual(TEXT("0 keeps everything"), EPFLogFormatting::FormatBodyForLog(Body, 0), Body);
	TestEqual(TEXT("A body under the cap is untouched"), EPFLogFormatting::FormatBodyForLog(Body, 100), Body);

	// Redaction happens before the cut, so a secret straddling the limit never leaks its start.
	const FString Secret = TEXT("{\"SessionTicket\":\"ABCDEFGHIJKLMNOPQRSTUVWXYZ\"}");
	TestFalse(TEXT("No part of a secret survives the cut"), EPFLogFormatting::FormatBodyForLog(Secret, 24).Contains(TEXT("ABC")));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
