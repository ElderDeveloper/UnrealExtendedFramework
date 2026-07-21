// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "ExtendedSteamSharedModule.h"
#include "Shared/ESteamTypes.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Core/OnlineSubsystemExtendedSteam.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ExtendedSteamAuthTest
{
	/**
	 * Shared entry: resolves the concrete Extended Steam subsystem when the Steam client is up.
	 * Returns nullptr (and asks the caller to early-out success) whenever Steam is down or the
	 * subsystem is disabled by config — matching the sibling OSS offline-state tests. Casting is
	 * safe because Get only returns an instance registered under ESTEAM_SUBSYSTEM, which this
	 * module always constructs as FOnlineSubsystemExtendedSteam.
	 */
	static FOnlineSubsystemExtendedSteam* GetExtendedSteamSubsystem()
	{
		IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get(ESTEAM_SUBSYSTEM);

		// Sampled AFTER Get: FOnlineSubsystemExtendedSteam::Init attempts a one-shot Steam client
		// initialization through the shared module, so Get itself can bring the client up.
		const bool bSteamClientUp = FExtendedSteamSharedModule::IsModuleAvailable()
			&& FExtendedSteamSharedModule::Get().IsSteamClientInitialized();

		if (!bSteamClientUp || Subsystem == nullptr)
		{
			return nullptr;
		}

		return static_cast<FOnlineSubsystemExtendedSteam*>(Subsystem);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamOSSAuthOfflineStateTest,
	"UnrealExtendedSteam.OSS.Auth.OfflineState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamOSSAuthOfflineStateTest::RunTest(const FString& Parameters)
{
	FOnlineSubsystemExtendedSteam* Subsystem = ExtendedSteamAuthTest::GetExtendedSteamSubsystem();
	if (Subsystem == nullptr)
	{
		// Steam down (or disabled): Init fails cleanly and there is no Auth interface to exercise.
		// The null-subsystem contract itself is covered by UnrealExtendedSteam.OSS.Registration.
		return true;
	}

	// Steam up: the plugin-specific Auth interface must be present and stable across calls.
	const FOnlineAuthExtendedSteamPtr Auth = Subsystem->GetAuthInterfaceExtended();
	TestTrue(TEXT("Auth interface accessor returns non-null while the subsystem is up"), Auth.IsValid());
	TestTrue(TEXT("Auth interface accessor is stable across calls"), Subsystem->GetAuthInterfaceExtended().Get() == Auth.Get());

	// GetAuthToken routes its synchronous session ticket through the Auth interface. Calling it must
	// be safe regardless of login state; when it does return a ticket it must be a valid hex string.
	const IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface();
	TestTrue(TEXT("Identity interface is implemented"), Identity.IsValid());
	if (Identity.IsValid())
	{
		const FString AuthToken = Identity->GetAuthToken(0);
		if (!AuthToken.IsEmpty())
		{
			bool bIsHex = AuthToken.Len() % 2 == 0;
			for (int32 Index = 0; bIsHex && Index < AuthToken.Len(); ++Index)
			{
				bIsHex = FChar::IsHexDigit(AuthToken[Index]);
			}
			TestTrue(TEXT("GetAuthToken returns a hex-encoded session ticket when available"), bIsHex);
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamOSSEncryptedAppTicketOfflineStateTest,
	"UnrealExtendedSteam.OSS.EncryptedAppTicket.OfflineState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamOSSEncryptedAppTicketOfflineStateTest::RunTest(const FString& Parameters)
{
	FOnlineSubsystemExtendedSteam* Subsystem = ExtendedSteamAuthTest::GetExtendedSteamSubsystem();
	if (Subsystem == nullptr)
	{
		return true;
	}

	// Steam up: the plugin-specific encrypted app ticket interface must be present and stable.
	const FOnlineEncryptedAppTicketExtendedSteamPtr Tickets = Subsystem->GetEncryptedAppTicketInterfaceExtended();
	TestTrue(TEXT("EncryptedAppTicket interface accessor returns non-null while the subsystem is up"), Tickets.IsValid());
	TestTrue(TEXT("EncryptedAppTicket interface accessor is stable across calls"),
		Subsystem->GetEncryptedAppTicketInterfaceExtended().Get() == Tickets.Get());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamOSSPingOfflineStateTest,
	"UnrealExtendedSteam.OSS.Ping.OfflineState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamOSSPingOfflineStateTest::RunTest(const FString& Parameters)
{
	FOnlineSubsystemExtendedSteam* Subsystem = ExtendedSteamAuthTest::GetExtendedSteamSubsystem();
	if (Subsystem == nullptr)
	{
		return true;
	}

	// Steam up: the plugin-specific Ping interface must be present and stable across calls.
	const FOnlinePingExtendedSteamPtr Ping = Subsystem->GetPingInterfaceExtended();
	TestTrue(TEXT("Ping interface accessor returns non-null while the subsystem is up"), Ping.IsValid());
	TestTrue(TEXT("Ping interface accessor is stable across calls"), Subsystem->GetPingInterfaceExtended().Get() == Ping.Get());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
