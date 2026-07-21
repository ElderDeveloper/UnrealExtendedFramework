// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "ExtendedSteamSharedModule.h"
#include "Shared/ESteamTypes.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemTypes.h"
#include "Interfaces/OnlineSessionInterface.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamOSSSessionOfflineStateTest,
	"UnrealExtendedSteam.OSS.Sessions.OfflineState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamOSSSessionOfflineStateTest::RunTest(const FString& Parameters)
{
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get(ESTEAM_SUBSYSTEM);

	// Sampled AFTER Get: FOnlineSubsystemExtendedSteam::Init attempts a one-shot Steam client
	// initialization through the shared module, so Get itself can bring the client up.
	const bool bSteamClientUp = FExtendedSteamSharedModule::IsModuleAvailable()
		&& FExtendedSteamSharedModule::Get().IsSteamClientInitialized();

	if (!bSteamClientUp || Subsystem == nullptr)
	{
		// Steam down (or subsystem disabled by config): Init fails cleanly, the factory destroys
		// the instance and there is no session interface to exercise. Early-out success (the
		// null-subsystem contract itself is covered by UnrealExtendedSteam.OSS.Registration).
		return true;
	}

	// Steam up: the session interface must be implemented.
	const IOnlineSessionPtr Session = Subsystem->GetSessionInterface();
	TestTrue(TEXT("Session interface is implemented"), Session.IsValid());
	if (!Session.IsValid())
	{
		return false;
	}

	// A name that was never created has no named session and reports the NoSession state.
	const FName NonexistentSession(TEXT("ESteamNonexistentSession"));
	TestNull(TEXT("GetNamedSession for a name never created is null"), Session->GetNamedSession(NonexistentSession));
	TestEqual(TEXT("GetSessionState for a name never created is NoSession"),
		Session->GetSessionState(NonexistentSession), EOnlineSessionState::NoSession);

	// Session ids are decimal lobby SteamID64s: a well-formed id round-trips through its string form.
	const FString LobbyIdString(TEXT("109775240984933659"));
	const FUniqueNetIdPtr SessionId = Session->CreateSessionIdFromString(LobbyIdString);
	TestTrue(TEXT("CreateSessionIdFromString parses a decimal SteamID64"), SessionId.IsValid());
	if (SessionId.IsValid())
	{
		TestTrue(TEXT("Parsed session id is valid"), SessionId->IsValid());
		TestEqual(TEXT("Parsed session id type matches the service name"), SessionId->GetType(), FName(ESTEAM_SUBSYSTEM));
		TestEqual(TEXT("Parsed session id round-trips to its string form"), SessionId->ToString(), LobbyIdString);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
