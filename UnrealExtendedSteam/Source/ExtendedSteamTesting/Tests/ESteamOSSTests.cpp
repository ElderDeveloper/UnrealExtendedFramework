// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "ExtendedSteamSharedModule.h"
#include "Shared/ESteamTypes.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineIdentityInterface.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamOSSRegistrationTest,
	"UnrealExtendedSteam.OSS.Registration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamOSSRegistrationTest::RunTest(const FString& Parameters)
{
	// The enabled query must be safe whatever the Steam/config state (no crash, no fatal log).
	const bool bEnabledByConfig = IOnlineSubsystem::IsEnabled(ESTEAM_SUBSYSTEM);

	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get(ESTEAM_SUBSYSTEM);

	// Sampled AFTER Get: FOnlineSubsystemExtendedSteam::Init attempts a one-shot Steam client
	// initialization through the shared module, so Get itself can bring the client up.
	const bool bSteamClientUp = FExtendedSteamSharedModule::IsModuleAvailable()
		&& FExtendedSteamSharedModule::Get().IsSteamClientInitialized();

	if (!bSteamClientUp)
	{
		// Without an initialized Steam client, Init must fail cleanly: the factory destroys the
		// instance and IOnlineSubsystem::Get yields null instead of a half-initialized subsystem.
		// (Editor test context is never a dedicated server, so the game-server path doesn't apply.)
		TestNull(TEXT("Subsystem is null while the Steam client is not initialized"), Subsystem);
		return true;
	}

	if (!bEnabledByConfig)
	{
		TestNull(TEXT("Subsystem is null when disabled by config"), Subsystem);
		return true;
	}

	TestNotNull(TEXT("Subsystem resolves while the Steam client is initialized"), Subsystem);
	if (Subsystem == nullptr)
	{
		return false;
	}

	TestEqual(TEXT("Subsystem name"), Subsystem->GetSubsystemName(), FName(ESTEAM_SUBSYSTEM));
	TestFalse(TEXT("App id is exposed"), Subsystem->GetAppId().IsEmpty());

	const IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface();
	TestTrue(TEXT("Identity interface is implemented"), Identity.IsValid());
	if (Identity.IsValid())
	{
		const FUniqueNetIdPtr LocalUserId = Identity->GetUniquePlayerId(0);
		TestTrue(TEXT("Local unique net id resolves"), LocalUserId.IsValid());
		if (LocalUserId.IsValid())
		{
			TestTrue(TEXT("Local unique net id is valid"), LocalUserId->IsValid());
			TestEqual(TEXT("Unique net id type matches the service name"), LocalUserId->GetType(), FName(ESTEAM_SUBSYSTEM));

			// String round trip through CreateUniquePlayerId (decimal SteamID64).
			const FUniqueNetIdPtr RoundTripped = Identity->CreateUniquePlayerId(LocalUserId->ToString());
			TestTrue(TEXT("Id round trips through its string form"),
				RoundTripped.IsValid() && *RoundTripped == *LocalUserId);
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
