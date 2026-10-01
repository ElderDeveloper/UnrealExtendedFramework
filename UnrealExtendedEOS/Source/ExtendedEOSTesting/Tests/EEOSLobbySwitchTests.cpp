// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Sessions/EEOSLobbyJoinRequest.h"
#include "Sessions/EEOSLobbySubsystem.h"
#include "Engine/GameInstance.h"
#include "OnlineSubsystemTypes.h"
#include "Shared/EEOSSettings.h"
#include "Misc/ScopeExit.h"

// Session fixtures never enter the OSS or EOS SDK; no live account or temporary world.
namespace
{
class FEEOSTestLobbyInfo final : public FOnlineSessionInfo
{
public:
	explicit FEEOSTestLobbyInfo(const FString& Id)
		: SessionId(FUniqueNetIdString::Create(Id, FName(TEXT("EOS")))) {}
	virtual const uint8* GetBytes() const override { return SessionId->GetBytes(); }
	virtual int32 GetSize() const override { return SessionId->GetSize(); }
	virtual bool IsValid() const override { return SessionId->IsValid(); }
	virtual FString ToString() const override { return SessionId->ToString(); }
	virtual FString ToDebugString() const override { return SessionId->ToDebugString(); }
	virtual const FUniqueNetId& GetSessionId() const override { return *SessionId; }
private:
	FUniqueNetIdStringRef SessionId;
};

FOnlineSessionSearchResult MakeLobbyResult(const TCHAR* Id)
{
	FOnlineSessionSearchResult Result;
	Result.Session.OwningUserId = FUniqueNetIdString::Create(TEXT("host"), FName(TEXT("EOS")));
	Result.Session.SessionInfo = MakeShared<FEEOSTestLobbyInfo>(Id);
	Result.Session.SessionSettings.bUseLobbiesIfAvailable = true;
	Result.Session.SessionSettings.Set(FName(TEXT("TEST_LABEL")), FString(TEXT("original")), EOnlineDataAdvertisementType::ViaOnlineService);
	return Result;
}
}

struct FEEOSLobbyTestAccess
{
	static void Refresh(UEEOSLobbySubsystem* Subsystem, const FNamedOnlineSession* Session)
	{
		Subsystem->RefreshLobbyState(Session);
	}
	static FEEOSLobbyJoinRequest& Request(UEEOSLobbySubsystem* Subsystem) { return Subsystem->JoinRequest; }
	static bool ShouldRecover(UEEOSLobbySubsystem* Subsystem, const UGameInstance* GameInstance)
	{
		return Subsystem->ShouldLeaveAfterConnectionFailure(GameInstance);
	}
	static void SetShuttingDown(UEEOSLobbySubsystem* Subsystem, bool bShuttingDown) { Subsystem->bShuttingDown = bShuttingDown; }
	static void NotifyLifetimeDestroy(UEEOSLobbySubsystem* Subsystem) { Subsystem->HandleLifetimeSessionDestroyed(Subsystem->GetLobbySessionName(), true); }
	static void SetCreatePending(UEEOSLobbySubsystem* Subsystem, bool bPending)
	{
		Subsystem->CreateLobbyCompleteHandle = bPending ? FDelegateHandle(FDelegateHandle::GenerateNewHandle) : FDelegateHandle();
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEEOSLobbySwitchWaitTest,
	"UnrealExtendedEOS.Lobbies.Switch.WaitForLeaveAndKeepTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEEOSLobbySwitchWaitTest::RunTest(const FString& Parameters)
{
	FEEOSLobbyJoinRequest Request;
	FOnlineSessionSearchResult Target = MakeLobbyResult(TEXT("target"));
	TestTrue(TEXT("Request accepted with existing lobby"), Request.Begin(Target, true));
	TestTrue(TEXT("Existing membership requires a leave"), Request.IsLeaving());
	TestFalse(TEXT("Joining cannot begin while leave is pending"), Request.IsJoining());
	TestFalse(TEXT("Another request cannot replace a pending target"), Request.Begin(MakeLobbyResult(TEXT("other")), false));

	Target.Session.SessionSettings.Set(FName(TEXT("TEST_LABEL")), FString(TEXT("changed")), EOnlineDataAdvertisementType::ViaOnlineService);
	Target = FOnlineSessionSearchResult(); // Search refresh must not discard the staged target.
	FString Label;
	Request.GetResult().Session.SessionSettings.Get(FName(TEXT("TEST_LABEL")), Label);
	TestEqual(TEXT("Target settings copied across asynchronous leave"), Label, FString(TEXT("original")));
	TestEqual(TEXT("Target survives replacing search results"), Request.GetResult().GetSessionIdStr(), FString(TEXT("target")));
	TestTrue(TEXT("Successful leave with no remaining named session permits join"), Request.CompleteLeave(true, false));
	TestTrue(TEXT("Next phase is joining"), Request.IsJoining());
	TestFalse(TEXT("Duplicate leave callback cannot change joining phase"), Request.CompleteLeave(true, false));
	TestFalse(TEXT("A new request is rejected while joining"), Request.Begin(MakeLobbyResult(TEXT("other")), false));
	Request.Reset();
	TestFalse(TEXT("Completed join releases operation guard"), Request.IsActive());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEEOSLobbySwitchFailureTest,
	"UnrealExtendedEOS.Lobbies.Switch.LeaveFailureStopsJoinAndAllowsRetry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEEOSLobbySwitchFailureTest::RunTest(const FString& Parameters)
{
	for (const bool bNamedSessionRemains : { false, true })
	{
		FEEOSLobbyJoinRequest Request;
		Request.Begin(MakeLobbyResult(TEXT("target")), true);
		TestFalse(TEXT("Failed EOS leave must never start join"), Request.CompleteLeave(false, bNamedSessionRemains));
		TestFalse(TEXT("Join blocked whether failed leave removed local session or not"), Request.IsJoining());
		TestEqual(TEXT("Failed target remains available for diagnostics"), Request.GetResult().GetSessionIdStr(), FString(TEXT("target")));
		Request.Reset(); // Terminal failure releases the guard.
		TestTrue(TEXT("Next user retry can start without restarting editor"), Request.Begin(MakeLobbyResult(TEXT("retry")), bNamedSessionRemains));
		TestEqual(TEXT("Retry leaves only if native session remains"), Request.IsLeaving(), bNamedSessionRemains);
	}
	FEEOSLobbyJoinRequest Request;
	Request.Begin(MakeLobbyResult(TEXT("target")), true);
	TestFalse(TEXT("Success callback is insufficient while native session remains"), Request.CompleteLeave(true, true));
	TestFalse(TEXT("No concurrent join on an occupied session name"), Request.IsJoining());
	Request.Reset();
	Request.Begin(MakeLobbyResult(TEXT("retry")), false);
	TestTrue(TEXT("Clean retry joins directly"), Request.IsJoining());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEEOSLobbyCacheRecoveryTest,
	"UnrealExtendedEOS.Lobbies.Switch.NativeSessionControlsCachedMembership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEEOSLobbyCacheRecoveryTest::RunTest(const FString& Parameters)
{
	UGameInstance* GI = NewObject<UGameInstance>();
	UEEOSLobbySubsystem* Lobby = NewObject<UEEOSLobbySubsystem>(GI);
	FNamedOnlineSession Existing(UEEOSLobbySubsystem::GetLobbySessionName(), MakeLobbyResult(TEXT("old")).Session);
	Existing.SessionState = EOnlineSessionState::Pending;
	FEEOSLobbyTestAccess::Refresh(Lobby, &Existing);
	TestTrue(TEXT("Native lobby restores an initially false membership cache"), Lobby->IsInLobby());
	TestEqual(TEXT("Native id restores cached id"), Lobby->GetCurrentLobbyId(), FString(TEXT("old")));
	TestEqual(TEXT("Native attributes restored"), Lobby->GetLobbyAttribute(TEXT("TEST_LABEL")), FString(TEXT("original")));

	FEEOSLobbyTestAccess::Request(Lobby).Begin(MakeLobbyResult(TEXT("target")), true);
	FEEOSLobbyTestAccess::Request(Lobby).CompleteLeave(false, true);
	FEEOSLobbyTestAccess::Refresh(Lobby, &Existing);
	FEEOSLobbyTestAccess::Request(Lobby).Reset();
	TestTrue(TEXT("Failed switch cannot erase surviving native membership"), Lobby->IsInLobby());
	TestEqual(TEXT("Surviving old lobby remains available for LeaveLobby"), Lobby->GetCurrentLobbyId(), FString(TEXT("old")));

	FEEOSLobbyTestAccess::Refresh(Lobby, nullptr);
	TestFalse(TEXT("No native session clears membership even after failed leave"), Lobby->IsInLobby());
	TestTrue(TEXT("No stale id after native session removal"), Lobby->GetCurrentLobbyId().IsEmpty());
	TestTrue(TEXT("No stale attributes after native session removal"), Lobby->GetLobbyAttribute(TEXT("TEST_LABEL")).IsEmpty());

	Existing.SessionState = EOnlineSessionState::Creating;
	Existing.SessionInfo.Reset();
	FEEOSLobbyTestAccess::Refresh(Lobby, &Existing);
	TestFalse(TEXT("Session placeholder without info is not membership"), Lobby->IsInLobby());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEEOSLobbyMembershipGuardTest,
	"UnrealExtendedEOS.Lobbies.Switch.MembershipOperationsCannotOverlap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEEOSLobbyMembershipGuardTest::RunTest(const FString& Parameters)
{
	UGameInstance* GI = NewObject<UGameInstance>();
	UEEOSLobbySubsystem* Lobby = NewObject<UEEOSLobbySubsystem>(GI);
	const FOnlineSessionSearchResult Target = MakeLobbyResult(TEXT("target"));
	for (const bool bLeaving : { true, false })
	{
		FEEOSLobbyTestAccess::Request(Lobby).Begin(Target, bLeaving);
		TestFalse(TEXT("Search join rejected during membership transition"), Lobby->JoinLobby(0));
		TestFalse(TEXT("Invite join rejected during membership transition"), Lobby->JoinLobbyResult(Target));
		TestFalse(TEXT("Create rejected during membership transition"), Lobby->CreateLobby());
		TestFalse(TEXT("Leave rejected during membership transition"), Lobby->LeaveLobby());
		TestFalse(TEXT("Destroy rejected during membership transition"), Lobby->DestroyLobby());
		FNamedOnlineSession Existing(UEEOSLobbySubsystem::GetLobbySessionName(), MakeLobbyResult(TEXT("old")).Session);
		Existing.SessionState = EOnlineSessionState::Pending;
		FEEOSLobbyTestAccess::Refresh(Lobby, &Existing);
		FEEOSLobbyTestAccess::NotifyLifetimeDestroy(Lobby);
		TestEqual(TEXT("Lifetime destroy notification cannot clear an active switch"), Lobby->GetCurrentLobbyId(), FString(TEXT("old")));
		TestTrue(TEXT("Rejections do not complete or replace existing request"), FEEOSLobbyTestAccess::Request(Lobby).IsActive());
		TestTrue(TEXT("Rejected overlapping calls do not overwrite current error"), Lobby->GetLastLobbyJoinError().IsEmpty());
		FEEOSLobbyTestAccess::Request(Lobby).Reset();
	}
	FEEOSLobbyTestAccess::SetCreatePending(Lobby, true);
	TestFalse(TEXT("Join rejected while create is pending"), Lobby->JoinLobbyResult(Target));
	FEEOSLobbyTestAccess::SetCreatePending(Lobby, false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEEOSLobbyConnectionRecoveryScopeTest,
	"UnrealExtendedEOS.Lobbies.Switch.ConnectionRecoveryScope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEEOSLobbyConnectionRecoveryScopeTest::RunTest(const FString& Parameters)
{
	UGameInstance* GI = NewObject<UGameInstance>();
	UEEOSLobbySubsystem* Lobby = NewObject<UEEOSLobbySubsystem>(GI);
	UEEOSSettings* Settings = GetMutableDefault<UEEOSSettings>();
	const bool bPreviousRecovery = Settings->bLeaveLobbyOnConnectionFailure;
	ON_SCOPE_EXIT { Settings->bLeaveLobbyOnConnectionFailure = bPreviousRecovery; };
	Settings->bLeaveLobbyOnConnectionFailure = true; // In-memory only; never SaveConfig.

	TestTrue(TEXT("Idle member can recover a failure in its own GameInstance"), FEEOSLobbyTestAccess::ShouldRecover(Lobby, GI));
	TestFalse(TEXT("Editor/world with no GameInstance cannot affect membership"), FEEOSLobbyTestAccess::ShouldRecover(Lobby, nullptr));
	TestFalse(TEXT("Another PIE GameInstance cannot affect membership"), FEEOSLobbyTestAccess::ShouldRecover(Lobby, NewObject<UGameInstance>()));
	Settings->bLeaveLobbyOnConnectionFailure = false;
	TestFalse(TEXT("Persistent social lobby opt-out is respected"), FEEOSLobbyTestAccess::ShouldRecover(Lobby, GI));
	Settings->bLeaveLobbyOnConnectionFailure = true;
	FEEOSLobbyTestAccess::Request(Lobby).Begin(MakeLobbyResult(TEXT("target")), true);
	TestFalse(TEXT("Recovery cannot interrupt an active switch"), FEEOSLobbyTestAccess::ShouldRecover(Lobby, GI));
	FEEOSLobbyTestAccess::Request(Lobby).Reset();
	FEEOSLobbyTestAccess::SetShuttingDown(Lobby, true);
	TestFalse(TEXT("No recovery operations during shutdown"), FEEOSLobbyTestAccess::ShouldRecover(Lobby, GI));
	return true;
}
