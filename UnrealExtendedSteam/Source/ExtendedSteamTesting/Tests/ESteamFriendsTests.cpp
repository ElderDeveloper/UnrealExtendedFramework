// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "ExtendedSteamSharedModule.h"
#include "Social/ESteamFriendsSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamFriendsOfflineStateTest,
	"UnrealExtendedSteam.Friends.OfflineState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamFriendsOfflineStateTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	TestNotNull(TEXT("Game instance created"), GameInstance);

	// Not initialized as a subsystem on purpose: every query must still be safe.
	UESteamFriendsSubsystem* Friends = NewObject<UESteamFriendsSubsystem>(GameInstance);
	TestNotNull(TEXT("Friends subsystem created"), Friends);

	const bool bSteamUp = FExtendedSteamSharedModule::IsModuleAvailable()
		&& FExtendedSteamSharedModule::Get().IsSteamClientInitialized();

	if (!bSteamUp)
	{
		// Steam is unavailable: everything must degrade to inert, offline-safe defaults.
		TestTrue(TEXT("Persona name empty when Steam is unavailable"), Friends->GetPersonaName().IsEmpty());
		TestEqual(TEXT("Friend count is 0 when Steam is unavailable"), Friends->GetFriendCount(), 0);

		TArray<FESteamFriend> FriendList;
		Friends->GetFriends(FriendList);
		TestEqual(TEXT("Friends list is empty when Steam is unavailable"), FriendList.Num(), 0);

		TestEqual(TEXT("Persona state is Offline when Steam is unavailable"),
			Friends->GetPersonaState(), EESteamPersonaState::Offline);

		AddExpectedError(TEXT("Steam is not available"), EAutomationExpectedErrorFlags::Contains, 0);
		TestFalse(TEXT("SetRichPresence fails when Steam is unavailable"),
			Friends->SetRichPresence(TEXT("status"), TEXT("Testing")));

		TestNull(TEXT("Avatar texture is null when Steam is unavailable"),
			Friends->GetFriendAvatarTexture(FESteamId(), EESteamAvatarSize::Small));

		TestFalse(TEXT("IsFriend is false when Steam is unavailable"), Friends->IsFriend(FESteamId()));

		const FESteamId FakeUser(76561197960265729ull);

		// Rich-presence iteration, nicknames and clan/group queries degrade to inert defaults.
		TestTrue(TEXT("Player nickname empty when Steam is unavailable"), Friends->GetPlayerNickname(FakeUser).IsEmpty());
		TestFalse(TEXT("RequestUserInformation false when Steam is unavailable"), Friends->RequestUserInformation(FakeUser, true));
		TestEqual(TEXT("Rich-presence key count is 0 when Steam is unavailable"), Friends->GetFriendRichPresenceKeyCount(FakeUser), 0);
		TestTrue(TEXT("Rich-presence key by index empty when Steam is unavailable"), Friends->GetFriendRichPresenceKeyByIndex(FakeUser, 0).IsEmpty());
		TestEqual(TEXT("Clan count is 0 when Steam is unavailable"), Friends->GetClanCount(), 0);
		TestFalse(TEXT("Clan by index invalid when Steam is unavailable"), Friends->GetClanByIndex(0).IsValid());
		TestTrue(TEXT("Clan name empty when Steam is unavailable"), Friends->GetClanName(FakeUser).IsEmpty());
		TestEqual(TEXT("Friend count from source is 0 when Steam is unavailable"), Friends->GetFriendCountFromSource(FakeUser), 0);

		int32 Online = 7;
		int32 InGame = 7;
		int32 Chatting = 7;
		TestFalse(TEXT("GetClanActivityCounts false when Steam is unavailable"),
			Friends->GetClanActivityCounts(FakeUser, Online, InGame, Chatting));
		TestEqual(TEXT("GetClanActivityCounts resets its out values"), Online + InGame + Chatting, 0);

		// Async request ops fail to issue offline (they log the standard warning, matched above).
		TestFalse(TEXT("DownloadClanActivityCounts fails when Steam is unavailable"), Friends->DownloadClanActivityCounts(FakeUser));
		TestFalse(TEXT("RequestClanOfficerList fails when Steam is unavailable"), Friends->RequestClanOfficerList(FakeUser));
		TestFalse(TEXT("GetFollowerCount fails when Steam is unavailable"), Friends->GetFollowerCount(FakeUser));
		TestFalse(TEXT("IsFollowing fails when Steam is unavailable"), Friends->IsFollowing(FakeUser));
		TestFalse(TEXT("EnumerateFollowingList fails when Steam is unavailable"), Friends->EnumerateFollowingList(0));
	}
	else
	{
		// Steam is up: only assert weak invariants (this runs against a live account).
		TestFalse(TEXT("Persona name is non-empty when Steam is up"), Friends->GetPersonaName().IsEmpty());
		TestTrue(TEXT("Friend count is non-negative when Steam is up"), Friends->GetFriendCount() >= 0);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
