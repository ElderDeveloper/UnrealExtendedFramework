// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "ExtendedSteamSharedModule.h"
#include "Shared/ESteamTypes.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineFriendsInterface.h"
#include "Interfaces/OnlinePresenceInterface.h"
#include "Interfaces/OnlineExternalUIInterface.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamOSSSocialOfflineStateTest,
	"UnrealExtendedSteam.OSS.Social.OfflineState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamOSSSocialOfflineStateTest::RunTest(const FString& Parameters)
{
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get(ESTEAM_SUBSYSTEM);

	// Sampled AFTER Get: FOnlineSubsystemExtendedSteam::Init attempts a one-shot Steam client
	// initialization through the shared module, so Get itself can bring the client up.
	const bool bSteamClientUp = FExtendedSteamSharedModule::IsModuleAvailable()
		&& FExtendedSteamSharedModule::Get().IsSteamClientInitialized();

	if (!bSteamClientUp || Subsystem == nullptr)
	{
		// Steam down (or subsystem disabled by config): Init fails cleanly and the factory yields
		// null — there are no social interfaces to probe. Early-out success by design.
		return true;
	}

	// Steam up: the three social interfaces must be constructed alongside the subsystem.
	const IOnlineFriendsPtr Friends = Subsystem->GetFriendsInterface();
	const IOnlinePresencePtr Presence = Subsystem->GetPresenceInterface();
	const IOnlineExternalUIPtr ExternalUI = Subsystem->GetExternalUIInterface();

	TestTrue(TEXT("Friends interface is implemented"), Friends.IsValid());
	TestTrue(TEXT("Presence interface is implemented"), Presence.IsValid());
	TestTrue(TEXT("External UI interface is implemented"), ExternalUI.IsValid());

	if (Friends.IsValid())
	{
		// Contract: the friends cache is only valid after a successful ReadFriendsList; before
		// that, GetFriendsList must report failure and hand back an empty array. (No test in this
		// suite calls ReadFriendsList, so the shared subsystem instance is still unread here.)
		TArray<TSharedRef<FOnlineFriend>> FriendsList;
		TestFalse(TEXT("GetFriendsList before ReadFriendsList reports failure"),
			Friends->GetFriendsList(0, EFriendsLists::ToString(EFriendsLists::Default), FriendsList));
		TestEqual(TEXT("GetFriendsList before ReadFriendsList yields an empty list"), FriendsList.Num(), 0);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
