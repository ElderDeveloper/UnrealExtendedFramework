// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "ExtendedSteamSharedModule.h"
#include "Storage/ESteamRemoteStorageSubsystem.h"
#include "Screenshots/ESteamScreenshotsSubsystem.h"
#include "Music/ESteamMusicSubsystem.h"
#include "Video/ESteamVideoSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

// Steam may genuinely be initialized when tests run interactively (auto-init in the editor),
// so each test branches: strict offline defaults when Steam is down, weak invariants when up.
// Subsystems are constructed directly without Initialize — every method is guarded by
// IsSteamAvailable(), which only consults the shared module.
namespace ESteamStorageMediaTestHelpers
{
	bool IsSteamClientUp()
	{
		return FExtendedSteamSharedModule::IsModuleAvailable()
			&& FExtendedSteamSharedModule::Get().IsSteamClientInitialized();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamStorageOfflineStateTest,
	"UnrealExtendedSteam.Storage.OfflineState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamStorageOfflineStateTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	UESteamRemoteStorageSubsystem* Storage = NewObject<UESteamRemoteStorageSubsystem>(GameInstance);
	TestNotNull(TEXT("Storage subsystem constructed"), Storage);

	const bool bSteamUp = ESteamStorageMediaTestHelpers::IsSteamClientUp();

	if (bSteamUp)
	{
		TestTrue(TEXT("File count is non-negative while Steam is up"), Storage->GetFileCount() >= 0);

		int64 TotalBytes = 0;
		int64 AvailableBytes = 0;
		if (Storage->GetQuota(TotalBytes, AvailableBytes))
		{
			TestTrue(TEXT("Quota total is non-negative while Steam is up"), TotalBytes >= 0);
			TestTrue(TEXT("Quota available does not exceed total"), AvailableBytes <= TotalBytes);
		}
		return true;
	}

	// The unavailable guards log a standard warning; that is the expected behavior under test.
	AddExpectedMessage(TEXT("Steam is not available"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 0, false);

	TestFalse(TEXT("FileExists is false offline"), Storage->FileExists(TEXT("estest_nonexistent.bin")));
	TestFalse(TEXT("FilePersisted is false offline"), Storage->FilePersisted(TEXT("estest_nonexistent.bin")));

	TArray<uint8> Data;
	TestFalse(TEXT("FileRead fails offline"), Storage->FileRead(TEXT("estest_nonexistent.bin"), Data));
	TestEqual(TEXT("FileRead outputs an empty array offline"), Data.Num(), 0);

	FString Contents;
	TestFalse(TEXT("FileReadString fails offline"), Storage->FileReadString(TEXT("estest_nonexistent.bin"), Contents));
	TestTrue(TEXT("FileReadString outputs an empty string offline"), Contents.IsEmpty());

	TestEqual(TEXT("File count is 0 offline"), Storage->GetFileCount(), 0);
	TestEqual(TEXT("File size is 0 offline"), Storage->GetFileSize(TEXT("estest_nonexistent.bin")), 0);
	TestEqual(TEXT("File timestamp is 0 offline"), Storage->GetFileTimestamp(TEXT("estest_nonexistent.bin")), static_cast<int64>(0));

	FString Name;
	int32 Size = 0;
	TestFalse(TEXT("GetFileNameAndSize fails offline"), Storage->GetFileNameAndSize(0, Name, Size));

	int64 TotalBytes = 0;
	int64 AvailableBytes = 0;
	TestFalse(TEXT("GetQuota fails offline"), Storage->GetQuota(TotalBytes, AvailableBytes));
	TestEqual(TEXT("Quota total is 0 offline"), TotalBytes, static_cast<int64>(0));
	TestEqual(TEXT("Quota available is 0 offline"), AvailableBytes, static_cast<int64>(0));

	TestFalse(TEXT("Cloud is disabled for account offline"), Storage->IsCloudEnabledForAccount());
	TestFalse(TEXT("Cloud is disabled for app offline"), Storage->IsCloudEnabledForApp());

	TestFalse(TEXT("FileWriteAsync fails offline"), Storage->FileWriteAsync(TEXT("estest_nonexistent.bin"), Data));
	TestFalse(TEXT("FileReadAsync fails offline"), Storage->FileReadAsync(TEXT("estest_nonexistent.bin")));
	TestFalse(TEXT("FileShare fails offline"), Storage->FileShare(TEXT("estest_nonexistent.bin")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamScreenshotsOfflineStateTest,
	"UnrealExtendedSteam.Screenshots.OfflineState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamScreenshotsOfflineStateTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	UESteamScreenshotsSubsystem* Screenshots = NewObject<UESteamScreenshotsSubsystem>(GameInstance);
	TestNotNull(TEXT("Screenshots subsystem constructed"), Screenshots);

	const bool bSteamUp = ESteamStorageMediaTestHelpers::IsSteamClientUp();

	if (bSteamUp)
	{
		// IsScreenshotsHooked reflects real client state; just verify it answers without crashing.
		Screenshots->IsScreenshotsHooked();
		return true;
	}

	AddExpectedMessage(TEXT("Steam is not available"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 0, false);

	TestFalse(TEXT("Screenshots are not hooked offline"), Screenshots->IsScreenshotsHooked());
	Screenshots->TriggerScreenshot(); // Must be a safe no-op offline.
	Screenshots->HookScreenshots(true); // Must be a safe no-op offline.
	TestFalse(TEXT("Screenshots remain unhooked after offline HookScreenshots"), Screenshots->IsScreenshotsHooked());
	TestEqual(TEXT("AddScreenshotToLibrary returns 0 offline"),
		Screenshots->AddScreenshotToLibrary(TEXT("estest_nonexistent.png"), FString(), 1920, 1080), 0);
	TestEqual(TEXT("WriteScreenshot returns 0 offline"),
		Screenshots->WriteScreenshot(TArray<uint8>(), 0, 0), 0);
	TestEqual(TEXT("AddVRScreenshotToLibrary returns 0 offline"),
		Screenshots->AddVRScreenshotToLibrary(EESteamVRScreenshotType::Mono, TEXT("estest_2d.png"), TEXT("estest_vr.png")), 0);
	TestFalse(TEXT("SetLocation fails offline"), Screenshots->SetLocation(0, TEXT("Nowhere")));
	TestFalse(TEXT("TagUser fails offline"), Screenshots->TagUser(0, FESteamId(1)));
	TestFalse(TEXT("TagPublishedFile fails offline"), Screenshots->TagPublishedFile(0, 1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamMusicOfflineStateTest,
	"UnrealExtendedSteam.Music.OfflineState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamMusicOfflineStateTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	UESteamMusicSubsystem* Music = NewObject<UESteamMusicSubsystem>(GameInstance);
	TestNotNull(TEXT("Music subsystem constructed"), Music);

	const bool bSteamUp = ESteamStorageMediaTestHelpers::IsSteamClientUp();

	if (bSteamUp)
	{
		const float Volume = Music->GetVolume();
		TestTrue(TEXT("Volume is within 0..1 while Steam is up"), Volume >= 0.0f && Volume <= 1.0f);
		return true;
	}

	// Music's offline methods are pure getters / silent no-ops (they do not log the
	// "Steam is not available" warning that the write-path subsystems emit), so no
	// AddExpectedMessage is needed here.
	TestFalse(TEXT("Music player is disabled offline"), Music->IsEnabled());
	TestFalse(TEXT("Music player is not playing offline"), Music->IsPlaying());
	TestEqual(TEXT("Playback status is Undefined offline"), Music->GetPlaybackStatus(), EESteamMusicStatus::Undefined);
	TestEqual(TEXT("Volume is 0 offline"), Music->GetVolume(), 0.0f);

	// Transport controls must be safe no-ops offline.
	Music->Play();
	Music->Pause();
	Music->PlayPrevious();
	Music->PlayNext();
	Music->SetVolume(0.5f);
	TestEqual(TEXT("Volume stays 0 after offline SetVolume"), Music->GetVolume(), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamVideoOfflineStateTest,
	"UnrealExtendedSteam.Video.OfflineState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamVideoOfflineStateTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	UESteamVideoSubsystem* Video = NewObject<UESteamVideoSubsystem>(GameInstance);
	TestNotNull(TEXT("Video subsystem constructed"), Video);

	const bool bSteamUp = ESteamStorageMediaTestHelpers::IsSteamClientUp();

	int32 NumViewers = -1;
	if (bSteamUp)
	{
		if (!Video->IsBroadcasting(NumViewers))
		{
			TestEqual(TEXT("Viewer count is 0 while not broadcasting"), NumViewers, 0);
		}
		return true;
	}

	AddExpectedMessage(TEXT("Steam is not available"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 0, false);

	TestFalse(TEXT("Not broadcasting offline"), Video->IsBroadcasting(NumViewers));
	TestEqual(TEXT("Viewer count is 0 offline"), NumViewers, 0);

	Video->GetOPFSettings(480); // no-op offline, broadcasts failure

	FString OPF = TEXT("stale");
	TestFalse(TEXT("GetOPFStringForApp fails offline"), Video->GetOPFStringForApp(480, OPF));
	TestTrue(TEXT("GetOPFStringForApp outputs an empty string offline"), OPF.IsEmpty());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
