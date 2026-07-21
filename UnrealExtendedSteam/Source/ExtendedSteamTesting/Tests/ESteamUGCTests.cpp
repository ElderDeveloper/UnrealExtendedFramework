// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "ExtendedSteamSharedModule.h"
#include "UGC/ESteamUGCSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamUGCOfflineStateTest,
	"UnrealExtendedSteam.UGC.OfflineState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamUGCOfflineStateTest::RunTest(const FString& Parameters)
{
	// Constructed directly (Initialize is intentionally not called): the subsystem's
	// availability guards must make every entry point safe and offline-correct anyway.
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	UESteamUGCSubsystem* Subsystem = NewObject<UESteamUGCSubsystem>(GameInstance);
	TestNotNull(TEXT("Subsystem constructed"), Subsystem);
	if (!Subsystem)
	{
		return false;
	}

	const bool bSteamUp = FExtendedSteamSharedModule::IsModuleAvailable()
		&& FExtendedSteamSharedModule::Get().IsSteamClientInitialized();

	if (bSteamUp)
	{
		// Steam is live on this machine/run: only assert weak, non-flaky invariants.
		const int32 NumSubscribed = Subsystem->GetNumSubscribedItems();
		TestTrue(TEXT("GetNumSubscribedItems is non-negative"), NumSubscribed >= 0);

		TArray<int64> SubscribedItems;
		Subsystem->GetSubscribedItems(SubscribedItems);
		TestEqual(TEXT("GetSubscribedItems matches GetNumSubscribedItems"), SubscribedItems.Num(), NumSubscribed);
		return true;
	}

	// The unavailable guards log a standard warning; that is the expected behavior under test.
	AddExpectedMessage(TEXT("Steam is not available"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 0, false);

	TestFalse(TEXT("CreateItem fails offline"), Subsystem->CreateItem(EESteamWorkshopFileType::Community));
	TestEqual(TEXT("StartItemUpdate returns 0 offline"), Subsystem->StartItemUpdate(1), static_cast<int64>(0));
	TestFalse(TEXT("SubmitItemUpdate fails offline"), Subsystem->SubmitItemUpdate(1, TEXT("note")));
	TestFalse(TEXT("SetItemTitle fails offline"), Subsystem->SetItemTitle(1, TEXT("Title")));

	int64 BytesProcessed = 42;
	int64 BytesTotal = 42;
	TestEqual(TEXT("GetItemUpdateProgress is Invalid offline"),
		Subsystem->GetItemUpdateProgress(1, BytesProcessed, BytesTotal), EESteamItemUpdateStatus::Invalid);
	TestEqual(TEXT("GetItemUpdateProgress zeroes BytesProcessed offline"), BytesProcessed, static_cast<int64>(0));
	TestEqual(TEXT("GetItemUpdateProgress zeroes BytesTotal offline"), BytesTotal, static_cast<int64>(0));

	TestFalse(TEXT("SubscribeItem fails offline"), Subsystem->SubscribeItem(1));
	TestFalse(TEXT("UnsubscribeItem fails offline"), Subsystem->UnsubscribeItem(1));
	TestEqual(TEXT("GetNumSubscribedItems returns 0 offline"), Subsystem->GetNumSubscribedItems(), 0);

	TArray<int64> SubscribedItems;
	Subsystem->GetSubscribedItems(SubscribedItems);
	TestEqual(TEXT("GetSubscribedItems returns empty offline"), SubscribedItems.Num(), 0);

	const FESteamUGCItemState State = Subsystem->GetItemState(1);
	TestFalse(TEXT("GetItemState: not subscribed offline"), State.bSubscribed);
	TestFalse(TEXT("GetItemState: not installed offline"), State.bInstalled);
	TestFalse(TEXT("GetItemState: no update needed offline"), State.bNeedsUpdate);
	TestFalse(TEXT("GetItemState: not downloading offline"), State.bDownloading);
	TestFalse(TEXT("GetItemState: no download pending offline"), State.bDownloadPending);

	FString Folder;
	int64 SizeOnDisk = 42;
	int64 Timestamp = 42;
	TestFalse(TEXT("GetItemInstallInfo fails offline"), Subsystem->GetItemInstallInfo(1, Folder, SizeOnDisk, Timestamp));
	TestTrue(TEXT("GetItemInstallInfo outputs an empty folder offline"), Folder.IsEmpty());
	TestEqual(TEXT("GetItemInstallInfo zeroes size offline"), SizeOnDisk, static_cast<int64>(0));
	TestEqual(TEXT("GetItemInstallInfo zeroes timestamp offline"), Timestamp, static_cast<int64>(0));

	TestFalse(TEXT("QueryAllItems fails offline"),
		Subsystem->QueryAllItems(EESteamUGCQueryType::RankedByPublicationDate, EESteamUGCMatchingType::Items, 1));
	TestFalse(TEXT("QueryItemsByIds fails offline"), Subsystem->QueryItemsByIds({ 1 }));
	TestFalse(TEXT("DownloadItem fails offline"), Subsystem->DownloadItem(1, false));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
