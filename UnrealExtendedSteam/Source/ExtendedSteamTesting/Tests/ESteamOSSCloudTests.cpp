// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "ExtendedSteamSharedModule.h"
#include "Shared/ESteamTypes.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemTypes.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineUserCloudInterface.h"
#include "Interfaces/OnlineSharedCloudInterface.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamOSSCloudOfflineStateTest,
	"UnrealExtendedSteam.OSS.Cloud.OfflineState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamOSSCloudOfflineStateTest::RunTest(const FString& Parameters)
{
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get(ESTEAM_SUBSYSTEM);

	// Sampled AFTER Get: FOnlineSubsystemExtendedSteam::Init attempts a one-shot Steam client
	// initialization through the shared module, so Get itself can bring the client up.
	const bool bSteamClientUp = FExtendedSteamSharedModule::IsModuleAvailable()
		&& FExtendedSteamSharedModule::Get().IsSteamClientInitialized();

	if (!bSteamClientUp || Subsystem == nullptr)
	{
		// Steam down (or subsystem disabled by config): Init fails cleanly, the factory destroys
		// the instance and there are no cloud interfaces to exercise. Early-out success.
		return true;
	}

	// Steam up: both cloud interfaces must be implemented.
	const IOnlineUserCloudPtr UserCloud = Subsystem->GetUserCloudInterface();
	const IOnlineSharedCloudPtr SharedCloud = Subsystem->GetSharedCloudInterface();
	TestTrue(TEXT("User cloud interface is implemented"), UserCloud.IsValid());
	TestTrue(TEXT("Shared cloud interface is implemented"), SharedCloud.IsValid());

	if (UserCloud.IsValid())
	{
		const IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface();
		const FUniqueNetIdPtr LocalUserId = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : nullptr;
		TestTrue(TEXT("Local unique net id resolves while Steam is up"), LocalUserId.IsValid());

		if (LocalUserId.IsValid())
		{
			// Contract: GetUserFileList returns what the last EnumerateUserFiles produced — before
			// any enumeration it must be empty (this test never calls EnumerateUserFiles).
			TArray<FCloudFileHeader> UserFiles;
			UserCloud->GetUserFileList(*LocalUserId, UserFiles);
			TestEqual(TEXT("GetUserFileList before EnumerateUserFiles is empty"), UserFiles.Num(), 0);
		}
	}

	if (SharedCloud.IsValid())
	{
		// The dummy test handle is a well-formed zero handle: never valid, never cached.
		TArray<TSharedRef<FSharedContentHandle>> DummyHandles;
		SharedCloud->GetDummySharedHandlesForTest(DummyHandles);
		TestTrue(TEXT("A dummy shared handle is provided"), DummyHandles.Num() > 0);

		if (DummyHandles.Num() > 0)
		{
			TestFalse(TEXT("Dummy shared handle is not valid"), DummyHandles[0]->IsValid());

			TArray<uint8> Contents;
			TestFalse(TEXT("Dummy shared handle has no cached contents"),
				SharedCloud->GetSharedFileContents(*DummyHandles[0], Contents));
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
