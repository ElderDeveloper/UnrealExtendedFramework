// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Shared/EEOSTypes.h"

// ── FEEOSResult Factory Methods ─────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSResultSuccessTest,
	"UnrealExtendedEOS.Types.FEEOSResult.Success",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSResultSuccessTest::RunTest(const FString& Parameters)
{
	FEEOSResult Result = FEEOSResult::Success();

	TestTrue(TEXT("Success result should have bSuccess = true"), Result.bSuccess);
	TestTrue(TEXT("Success result should have empty ErrorMessage"), Result.ErrorMessage.IsEmpty());
	TestTrue(TEXT("Success result should have empty ErrorCode"), Result.ErrorCode.IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSResultFailureTest,
	"UnrealExtendedEOS.Types.FEEOSResult.Failure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSResultFailureTest::RunTest(const FString& Parameters)
{
	FEEOSResult Result = FEEOSResult::Failure(TEXT("Something went wrong"), TEXT("ERR_TEST"));

	TestFalse(TEXT("Failure result should have bSuccess = false"), Result.bSuccess);
	TestEqual(TEXT("ErrorMessage should match"), Result.ErrorMessage, FString(TEXT("Something went wrong")));
	TestEqual(TEXT("ErrorCode should match"), Result.ErrorCode, FString(TEXT("ERR_TEST")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSResultFailureNoCodeTest,
	"UnrealExtendedEOS.Types.FEEOSResult.FailureNoCode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSResultFailureNoCodeTest::RunTest(const FString& Parameters)
{
	FEEOSResult Result = FEEOSResult::Failure(TEXT("Error only"));

	TestFalse(TEXT("Failure result should have bSuccess = false"), Result.bSuccess);
	TestEqual(TEXT("ErrorMessage should match"), Result.ErrorMessage, FString(TEXT("Error only")));
	TestTrue(TEXT("ErrorCode should be empty when not provided"), Result.ErrorCode.IsEmpty());

	return true;
}


// ── Struct Default Values ───────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSSessionSettingsDefaultsTest,
	"UnrealExtendedEOS.Types.SessionSettings.Defaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSSessionSettingsDefaultsTest::RunTest(const FString& Parameters)
{
	FEEOSSessionSettings Settings;

	TestEqual(TEXT("MaxPlayers default"), Settings.MaxPlayers, 4);
	TestEqual(TEXT("NumPrivateConnections default"), Settings.NumPrivateConnections, 0);
	TestFalse(TEXT("bIsDedicatedServer default"), Settings.bIsDedicatedServer);
	TestFalse(TEXT("bIsLANMatch default"), Settings.bIsLANMatch);
	TestTrue(TEXT("bShouldAdvertise default"), Settings.bShouldAdvertise);
	TestTrue(TEXT("bAllowJoinInProgress default"), Settings.bAllowJoinInProgress);
	TestTrue(TEXT("bUsesPresence default"), Settings.bUsesPresence);
	TestTrue(TEXT("bAllowJoinViaPresence default"), Settings.bAllowJoinViaPresence);
	TestFalse(TEXT("bAllowJoinViaPresenceFriendsOnly default"), Settings.bAllowJoinViaPresenceFriendsOnly);
	TestTrue(TEXT("bAllowInvites default"), Settings.bAllowInvites);
	TestFalse(TEXT("bUseLobbiesIfAvailable default"), Settings.bUseLobbiesIfAvailable);
	TestFalse(TEXT("bUseLobbiesVoiceChatIfAvailable default"), Settings.bUseLobbiesVoiceChatIfAvailable);
	TestFalse(TEXT("bUsesStats default"), Settings.bUsesStats);
	TestEqual(TEXT("Region default"), Settings.Region, EEOSRegionInfo::NoSelection);
	TestEqual(TEXT("CustomSettings should be empty"), Settings.CustomSettings.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSSessionSearchResultDefaultsTest,
	"UnrealExtendedEOS.Types.SessionSearchResult.Defaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSSessionSearchResultDefaultsTest::RunTest(const FString& Parameters)
{
	FEEOSSessionSearchResult Result;

	TestTrue(TEXT("SessionId default empty"), Result.SessionId.IsEmpty());
	TestTrue(TEXT("OwnerName default empty"), Result.OwnerName.IsEmpty());
	TestEqual(TEXT("CurrentPlayers default"), Result.CurrentPlayers, 0);
	TestEqual(TEXT("MaxPlayers default"), Result.MaxPlayers, 0);
	TestEqual(TEXT("Ping default"), Result.Ping, 0);
	TestFalse(TEXT("bIsDedicatedServer default"), Result.bIsDedicatedServer);
	TestEqual(TEXT("Settings map should be empty"), Result.Settings.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSLeaderboardEntryDefaultsTest,
	"UnrealExtendedEOS.Types.LeaderboardEntry.Defaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSLeaderboardEntryDefaultsTest::RunTest(const FString& Parameters)
{
	FEEOSLeaderboardEntry Entry;

	TestEqual(TEXT("Rank default"), Entry.Rank, 0);
	TestTrue(TEXT("UserId default empty"), Entry.UserId.IsEmpty());
	TestTrue(TEXT("DisplayName default empty"), Entry.DisplayName.IsEmpty());
	TestEqual(TEXT("Score default"), Entry.Score, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSAchievementDefaultsTest,
	"UnrealExtendedEOS.Types.Achievement.Defaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSAchievementDefaultsTest::RunTest(const FString& Parameters)
{
	FEEOSAchievement Achievement;

	TestTrue(TEXT("AchievementId default empty"), Achievement.AchievementId.IsEmpty());
	TestTrue(TEXT("DisplayName default empty"), Achievement.DisplayName.IsEmpty());
	TestTrue(TEXT("Description default empty"), Achievement.Description.IsEmpty());
	TestEqual(TEXT("Progress default"), Achievement.Progress, 0.f);
	TestFalse(TEXT("bUnlocked default"), Achievement.bUnlocked);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSFriendInfoDefaultsTest,
	"UnrealExtendedEOS.Types.FriendInfo.Defaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSFriendInfoDefaultsTest::RunTest(const FString& Parameters)
{
	FEEOSFriendInfo Info;

	TestTrue(TEXT("UserId default empty"), Info.UserId.IsEmpty());
	TestTrue(TEXT("DisplayName default empty"), Info.DisplayName.IsEmpty());
	TestTrue(TEXT("PresenceStatus default empty"), Info.PresenceStatus.IsEmpty());
	TestFalse(TEXT("bIsOnline default"), Info.bIsOnline);
	TestFalse(TEXT("bIsPlayingThisGame default"), Info.bIsPlayingThisGame);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSStatDefaultsTest,
	"UnrealExtendedEOS.Types.Stat.Defaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSStatDefaultsTest::RunTest(const FString& Parameters)
{
	FEEOSStat Stat;

	TestTrue(TEXT("StatName default empty"), Stat.StatName.IsEmpty());
	TestEqual(TEXT("Value default"), Stat.Value, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSSanctionDefaultsTest,
	"UnrealExtendedEOS.Types.Sanction.Defaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSSanctionDefaultsTest::RunTest(const FString& Parameters)
{
	FEEOSSanction Sanction;

	TestTrue(TEXT("SanctionId default empty"), Sanction.SanctionId.IsEmpty());
	TestTrue(TEXT("Action default empty"), Sanction.Action.IsEmpty());
	TestFalse(TEXT("bIsPermanent default"), Sanction.bIsPermanent);
	TestTrue(TEXT("Reason default empty"), Sanction.Reason.IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSCatalogOfferDefaultsTest,
	"UnrealExtendedEOS.Types.CatalogOffer.Defaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSCatalogOfferDefaultsTest::RunTest(const FString& Parameters)
{
	FEEOSCatalogOffer Offer;

	TestTrue(TEXT("OfferId default empty"), Offer.OfferId.IsEmpty());
	TestTrue(TEXT("Title default empty"), Offer.Title.IsEmpty());
	TestTrue(TEXT("Description default empty"), Offer.Description.IsEmpty());
	TestTrue(TEXT("FormattedPrice default empty"), Offer.FormattedPrice.IsEmpty());
	TestEqual(TEXT("PriceInSmallestUnit default"), Offer.PriceInSmallestUnit, (int64)0);
	TestEqual(TEXT("OriginalPriceInSmallestUnit default"), Offer.OriginalPriceInSmallestUnit, (int64)0);
	TestTrue(TEXT("CurrencyCode default empty"), Offer.CurrencyCode.IsEmpty());
	TestEqual(TEXT("ItemCount default"), Offer.ItemCount, 0);
	TestFalse(TEXT("bAvailableForPurchase default"), Offer.bAvailableForPurchase);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSEntitlementDefaultsTest,
	"UnrealExtendedEOS.Types.Entitlement.Defaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSEntitlementDefaultsTest::RunTest(const FString& Parameters)
{
	FEEOSEntitlement Entitlement;

	TestTrue(TEXT("EntitlementId default empty"), Entitlement.EntitlementId.IsEmpty());
	TestTrue(TEXT("EntitlementName default empty"), Entitlement.EntitlementName.IsEmpty());
	TestTrue(TEXT("CatalogItemId default empty"), Entitlement.CatalogItemId.IsEmpty());
	TestFalse(TEXT("bRedeemed default"), Entitlement.bRedeemed);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSOwnershipDefaultsTest,
	"UnrealExtendedEOS.Types.Ownership.Defaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSOwnershipDefaultsTest::RunTest(const FString& Parameters)
{
	FEEOSOwnership Ownership;

	TestTrue(TEXT("ItemId default empty"), Ownership.ItemId.IsEmpty());
	TestFalse(TEXT("bOwned default"), Ownership.bOwned);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSUserInfoDefaultsTest,
	"UnrealExtendedEOS.Types.UserInfo.Defaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSUserInfoDefaultsTest::RunTest(const FString& Parameters)
{
	FEEOSUserInfo UserInfo;

	TestTrue(TEXT("EpicAccountId default empty"), UserInfo.EpicAccountId.IsEmpty());
	TestTrue(TEXT("DisplayName default empty"), UserInfo.DisplayName.IsEmpty());
	TestTrue(TEXT("Nickname default empty"), UserInfo.Nickname.IsEmpty());
	TestTrue(TEXT("Country default empty"), UserInfo.Country.IsEmpty());
	TestTrue(TEXT("PreferredLanguage default empty"), UserInfo.PreferredLanguage.IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSExternalAccountMappingDefaultsTest,
	"UnrealExtendedEOS.Types.ExternalAccountMapping.Defaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSExternalAccountMappingDefaultsTest::RunTest(const FString& Parameters)
{
	FEEOSExternalAccountMapping Mapping;

	TestTrue(TEXT("ProductUserId default empty"), Mapping.ProductUserId.IsEmpty());
	TestTrue(TEXT("ExternalAccountId default empty"), Mapping.ExternalAccountId.IsEmpty());
	TestTrue(TEXT("ExternalAccountType default empty"), Mapping.ExternalAccountType.IsEmpty());
	TestTrue(TEXT("DisplayName default empty"), Mapping.DisplayName.IsEmpty());

	return true;
}
