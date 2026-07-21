// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Shared/EEOSBlueprintLibrary.h"
#include "Shared/EEOSTypes.h"


// ═══════════════════════════════════════════════════════════════════════════
// ID Validation
// ═══════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSValidEpicAccountIdTest,
	"UnrealExtendedEOS.BlueprintLibrary.IDValidation.ValidEpicAccountId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSValidEpicAccountIdTest::RunTest(const FString& Parameters)
{
	// Valid: exactly 32 hex characters
	TestTrue(TEXT("32 hex chars should be valid"), UEEOSBlueprintLibrary::IsValidEpicAccountId(TEXT("0123456789abcdef0123456789abcdef")));
	TestTrue(TEXT("32 uppercase hex chars should be valid"), UEEOSBlueprintLibrary::IsValidEpicAccountId(TEXT("0123456789ABCDEF0123456789ABCDEF")));

	// Invalid cases
	TestFalse(TEXT("Empty string should be invalid"), UEEOSBlueprintLibrary::IsValidEpicAccountId(TEXT("")));
	TestFalse(TEXT("31 chars should be invalid (too short)"), UEEOSBlueprintLibrary::IsValidEpicAccountId(TEXT("0123456789abcdef0123456789abcde")));
	TestFalse(TEXT("33 chars should be invalid (too long)"), UEEOSBlueprintLibrary::IsValidEpicAccountId(TEXT("0123456789abcdef0123456789abcdef0")));
	TestFalse(TEXT("Non-hex chars should be invalid"), UEEOSBlueprintLibrary::IsValidEpicAccountId(TEXT("0123456789abcdef0123456789abcdeg")));
	TestFalse(TEXT("Spaces should be invalid"), UEEOSBlueprintLibrary::IsValidEpicAccountId(TEXT("0123456789abcdef 123456789abcdef")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSValidProductUserIdTest,
	"UnrealExtendedEOS.BlueprintLibrary.IDValidation.ValidProductUserId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSValidProductUserIdTest::RunTest(const FString& Parameters)
{
	// Valid: 16–64 hex characters
	TestTrue(TEXT("16 hex chars should be valid"), UEEOSBlueprintLibrary::IsValidProductUserId(TEXT("0123456789abcdef")));
	TestTrue(TEXT("32 hex chars should be valid"), UEEOSBlueprintLibrary::IsValidProductUserId(TEXT("0123456789abcdef0123456789abcdef")));
	TestTrue(TEXT("64 hex chars should be valid"), UEEOSBlueprintLibrary::IsValidProductUserId(TEXT("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef")));

	// Invalid cases
	TestFalse(TEXT("Empty string should be invalid"), UEEOSBlueprintLibrary::IsValidProductUserId(TEXT("")));
	TestFalse(TEXT("15 chars should be invalid (too short)"), UEEOSBlueprintLibrary::IsValidProductUserId(TEXT("0123456789abcde")));
	TestFalse(TEXT("65 chars should be invalid (too long)"), UEEOSBlueprintLibrary::IsValidProductUserId(TEXT("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0")));
	TestFalse(TEXT("Non-hex chars should be invalid"), UEEOSBlueprintLibrary::IsValidProductUserId(TEXT("0123456789abcdeg")));

	return true;
}


// ═══════════════════════════════════════════════════════════════════════════
// Byte / String Conversions
// ═══════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSByteHexRoundTripTest,
	"UnrealExtendedEOS.BlueprintLibrary.Conversion.ByteHexRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSByteHexRoundTripTest::RunTest(const FString& Parameters)
{
	TArray<uint8> Original = { 0x00, 0x11, 0xAA, 0xFF, 0x42 };

	FString HexStr = UEEOSBlueprintLibrary::ByteArrayToHexString(Original);
	TestFalse(TEXT("Hex string should not be empty"), HexStr.IsEmpty());

	TArray<uint8> Recovered = UEEOSBlueprintLibrary::HexStringToByteArray(HexStr);
	TestEqual(TEXT("Round-trip should preserve array length"), Recovered.Num(), Original.Num());

	for (int32 i = 0; i < Original.Num(); ++i)
	{
		TestEqual(*FString::Printf(TEXT("Byte[%d] should match"), i), Recovered[i], Original[i]);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSByteHexEmptyTest,
	"UnrealExtendedEOS.BlueprintLibrary.Conversion.ByteHexEmpty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSByteHexEmptyTest::RunTest(const FString& Parameters)
{
	TArray<uint8> Empty;
	FString HexStr = UEEOSBlueprintLibrary::ByteArrayToHexString(Empty);
	TestTrue(TEXT("Empty byte array should produce empty hex string"), HexStr.IsEmpty());

	TArray<uint8> Result = UEEOSBlueprintLibrary::HexStringToByteArray(TEXT(""));
	TestEqual(TEXT("Empty hex string should produce empty byte array"), Result.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSHexInvalidInputTest,
	"UnrealExtendedEOS.BlueprintLibrary.Conversion.HexInvalidInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSHexInvalidInputTest::RunTest(const FString& Parameters)
{
	// Contract: invalid hex input returns an EMPTY array — never a partial/garbage decode
	TestEqual(TEXT("Odd-length input should return empty array"),
		UEEOSBlueprintLibrary::HexStringToByteArray(TEXT("ABC")).Num(), 0);
	TestEqual(TEXT("Non-hex characters should return empty array"),
		UEEOSBlueprintLibrary::HexStringToByteArray(TEXT("GGGG")).Num(), 0);
	TestEqual(TEXT("Mixed valid/invalid characters should return empty array"),
		UEEOSBlueprintLibrary::HexStringToByteArray(TEXT("FFzz")).Num(), 0);
	TestEqual(TEXT("Whitespace should return empty array"),
		UEEOSBlueprintLibrary::HexStringToByteArray(TEXT("FF 00")).Num(), 0);

	// Valid input still decodes, case-insensitively
	TArray<uint8> Decoded = UEEOSBlueprintLibrary::HexStringToByteArray(TEXT("ff00Aa"));
	if (TestEqual(TEXT("Valid lowercase/uppercase hex should decode to 3 bytes"), Decoded.Num(), 3))
	{
		TestEqual(TEXT("Byte[0]"), Decoded[0], (uint8)0xFF);
		TestEqual(TEXT("Byte[1]"), Decoded[1], (uint8)0x00);
		TestEqual(TEXT("Byte[2]"), Decoded[2], (uint8)0xAA);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSStringBytesRoundTripTest,
	"UnrealExtendedEOS.BlueprintLibrary.Conversion.StringBytesRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSStringBytesRoundTripTest::RunTest(const FString& Parameters)
{
	FString Original = TEXT("Hello, EOS World!");

	TArray<uint8> Bytes = UEEOSBlueprintLibrary::StringToBytes(Original);
	TestTrue(TEXT("Bytes should not be empty"), Bytes.Num() > 0);

	FString Recovered = UEEOSBlueprintLibrary::BytesToString(Bytes);
	TestEqual(TEXT("Round-trip should preserve the string"), Recovered, Original);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSStringBytesEmptyTest,
	"UnrealExtendedEOS.BlueprintLibrary.Conversion.StringBytesEmpty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSStringBytesEmptyTest::RunTest(const FString& Parameters)
{
	TArray<uint8> Bytes = UEEOSBlueprintLibrary::StringToBytes(TEXT(""));
	TestEqual(TEXT("Empty string should produce empty byte array"), Bytes.Num(), 0);

	TArray<uint8> EmptyBytes;
	FString Result = UEEOSBlueprintLibrary::BytesToString(EmptyBytes);
	TestTrue(TEXT("Empty byte array should produce empty string"), Result.IsEmpty());

	return true;
}


// ═══════════════════════════════════════════════════════════════════════════
// Stat Helpers
// ═══════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSFindStatValueTest,
	"UnrealExtendedEOS.BlueprintLibrary.Stats.FindStatValue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSFindStatValueTest::RunTest(const FString& Parameters)
{
	TArray<FEEOSStat> Stats;

	FEEOSStat Kills;
	Kills.StatName = TEXT("Kills");
	Kills.Value = 42;
	Stats.Add(Kills);

	FEEOSStat Deaths;
	Deaths.StatName = TEXT("Deaths");
	Deaths.Value = 7;
	Stats.Add(Deaths);

	FEEOSStat ZeroStat;
	ZeroStat.StatName = TEXT("Assists");
	ZeroStat.Value = 0;
	Stats.Add(ZeroStat);

	TestEqual(TEXT("Kills should be 42"), UEEOSBlueprintLibrary::FindStatValue(Stats, TEXT("Kills")), 42);
	TestEqual(TEXT("Deaths should be 7"), UEEOSBlueprintLibrary::FindStatValue(Stats, TEXT("Deaths")), 7);
	TestEqual(TEXT("Assists should be 0"), UEEOSBlueprintLibrary::FindStatValue(Stats, TEXT("Assists")), 0);
	TestEqual(TEXT("Missing stat should return -1"), UEEOSBlueprintLibrary::FindStatValue(Stats, TEXT("Headshots")), -1);

	// Empty array
	TArray<FEEOSStat> EmptyStats;
	TestEqual(TEXT("Empty array should return -1"), UEEOSBlueprintLibrary::FindStatValue(EmptyStats, TEXT("Kills")), -1);

	return true;
}


// ═══════════════════════════════════════════════════════════════════════════
// Achievement Helpers
// ═══════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSIsAchievementUnlockedTest,
	"UnrealExtendedEOS.BlueprintLibrary.Achievements.IsAchievementUnlocked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSIsAchievementUnlockedTest::RunTest(const FString& Parameters)
{
	TArray<FEEOSAchievement> Achievements;

	FEEOSAchievement Unlocked;
	Unlocked.AchievementId = TEXT("ACH_FIRST_WIN");
	Unlocked.Progress = 1.0f;
	Unlocked.bUnlocked = true;
	Achievements.Add(Unlocked);

	FEEOSAchievement InProgress;
	InProgress.AchievementId = TEXT("ACH_100_KILLS");
	InProgress.Progress = 0.5f;
	InProgress.bUnlocked = false;
	Achievements.Add(InProgress);

	FEEOSAchievement NotStarted;
	NotStarted.AchievementId = TEXT("ACH_LEGENDARY");
	NotStarted.Progress = 0.0f;
	NotStarted.bUnlocked = false;
	Achievements.Add(NotStarted);

	// IsAchievementUnlocked checks Progress >= 1.0f
	TestTrue(TEXT("Fully completed achievement should be unlocked"), UEEOSBlueprintLibrary::IsAchievementUnlocked(Achievements, TEXT("ACH_FIRST_WIN")));
	TestFalse(TEXT("Half-completed achievement should not be unlocked"), UEEOSBlueprintLibrary::IsAchievementUnlocked(Achievements, TEXT("ACH_100_KILLS")));
	TestFalse(TEXT("Not started achievement should not be unlocked"), UEEOSBlueprintLibrary::IsAchievementUnlocked(Achievements, TEXT("ACH_LEGENDARY")));
	TestFalse(TEXT("Missing achievement should return false"), UEEOSBlueprintLibrary::IsAchievementUnlocked(Achievements, TEXT("ACH_NONEXISTENT")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSGetAchievementByIdTest,
	"UnrealExtendedEOS.BlueprintLibrary.Achievements.GetAchievementById",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSGetAchievementByIdTest::RunTest(const FString& Parameters)
{
	TArray<FEEOSAchievement> Achievements;

	FEEOSAchievement Achievement;
	Achievement.AchievementId = TEXT("ACH_FIRST_WIN");
	Achievement.Progress = 1.0f;
	Achievement.bUnlocked = true;
	Achievements.Add(Achievement);

	FEEOSAchievement OutAchievement;

	// Found case
	bool bFound = UEEOSBlueprintLibrary::GetAchievementById(Achievements, TEXT("ACH_FIRST_WIN"), OutAchievement);
	TestTrue(TEXT("Should find existing achievement"), bFound);
	TestEqual(TEXT("Found achievement ID should match"), OutAchievement.AchievementId, FString(TEXT("ACH_FIRST_WIN")));
	TestEqual(TEXT("Found achievement progress should match"), OutAchievement.Progress, 1.0f);

	// Not found case
	FEEOSAchievement OutMissing;
	bool bNotFound = UEEOSBlueprintLibrary::GetAchievementById(Achievements, TEXT("ACH_NONEXISTENT"), OutMissing);
	TestFalse(TEXT("Should not find missing achievement"), bNotFound);

	return true;
}


// ═══════════════════════════════════════════════════════════════════════════
// Time Helpers
// ═══════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSUnixTimestampToStringTest,
	"UnrealExtendedEOS.BlueprintLibrary.Time.UnixTimestampToString",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSUnixTimestampToStringTest::RunTest(const FString& Parameters)
{
	// Unix epoch 0 = 1970-01-01 00:00:00
	FString EpochStr = UEEOSBlueprintLibrary::UnixTimestampToString(0);
	TestTrue(TEXT("Epoch string should contain 1970"), EpochStr.Contains(TEXT("1970")));

	// Known timestamp: 1700000000 = 2023-11-14 22:13:20 UTC
	FString KnownStr = UEEOSBlueprintLibrary::UnixTimestampToString(1700000000);
	TestTrue(TEXT("Known timestamp should contain 2023"), KnownStr.Contains(TEXT("2023")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSGetCurrentUnixTimestampTest,
	"UnrealExtendedEOS.BlueprintLibrary.Time.GetCurrentUnixTimestamp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSGetCurrentUnixTimestampTest::RunTest(const FString& Parameters)
{
	int64 Timestamp = UEEOSBlueprintLibrary::GetCurrentUnixTimestamp();

	// Should be a reasonable value (after 2020-01-01 = 1577836800)
	TestTrue(TEXT("Timestamp should be after 2020"), Timestamp > 1577836800);

	return true;
}
