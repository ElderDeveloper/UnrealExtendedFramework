// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Sessions/EEOSLobbyJoinRequest.h"
#include "Sessions/EEOSLobbySubsystem.h"
#include "Engine/GameInstance.h"
#include "OnlineSubsystemTypes.h"
#include "Shared/EEOSSettings.h"
#include "Misc/ScopeExit.h"
#include "EEOSLobbyLifecycleTestObserver.h"

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
	static FEEOSLobbyExitRequest& Exit(UEEOSLobbySubsystem* S) { return S->ExitRequest; }
	static bool LocalRemovalAllowed(UEEOSLobbySubsystem* S) { return S->ShouldHandleLocalMemberRemoval(); }
	static void RemoteExit(UEEOSLobbySubsystem* S, const FNamedOnlineSession* Native, bool bSuccess) { S->ReconcileRemoteLobbyExit(Native, bSuccess); }
	static bool Owner(UEEOSLobbySubsystem* S, const FString& Id) { return S->UpdateCachedLobbyOwner(Id); }
	static void Promotion(UEEOSLobbySubsystem* S, uint64 Token, const FString& LobbyId, const FString& Member, bool bSuccess)
	{ S->HandlePromotionComplete(Token, LobbyId, Member, bSuccess); }
	static void BeginPromotion(UEEOSLobbySubsystem* S, uint64 Token, const FString& LobbyId)
	{ S->bPromotionPending = true; S->PromotionToken = Token; S->PromotionLobbyId = LobbyId; }
	static void ExpectNativeExits(UEEOSLobbySubsystem* S, int32 Count) { S->ExpectedNativeExitCompletions = Count; S->NativeExitCompletions = 0; }
	static void NativeExit(UEEOSLobbySubsystem* S, bool bSuccess) { S->HandleDestroySessionComplete(S->GetLobbySessionName(), bSuccess); }
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
		TestFalse(TEXT("Transfer rejected during membership transition"), Lobby->PromoteMember(TEXT("member")));
		TestFalse(TEXT("Kick rejected during membership transition"), Lobby->KickMember(TEXT("member")));
		TestFalse(TEXT("Attribute update rejected during membership transition"), Lobby->SetLobbyAttribute(TEXT("name"), TEXT("value")));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEEOSLobbyDeleteOrderingTest,
	"UnrealExtendedEOS.Lobbies.Lifecycle.DeleteAndNativeCleanupOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FEEOSLobbyDeleteOrderingTest::RunTest(const FString&)
{
	for (const bool bNativeFirst : {false, true})
	{
		FEEOSLobbyExitRequest Exit;
		TestTrue(TEXT("Delete accepted"), Exit.Begin(TEXT("hosted"), true, FEEOSLobbyExitRequest::EContinuation::None));
		TestFalse(TEXT("Another close cannot overlap"), Exit.Begin(TEXT("other"), true, FEEOSLobbyExitRequest::EContinuation::None));
		TestFalse(TEXT("Native closure alone cannot complete backend deletion"), Exit.Succeeded(true, !bNativeFirst));
		TestTrue(TEXT("Backend result accepted for exact request"), Exit.CompleteBackend(Exit.GetToken(), TEXT("hosted"), true));
		TestFalse(TEXT("Duplicate SDK result ignored"), Exit.CompleteBackend(Exit.GetToken(), TEXT("hosted"), true));
		TestFalse(TEXT("Deleted backend still requires native session/voice cleanup"), Exit.Succeeded(true, true));
		TestTrue(TEXT("Failed native leave of an already deleted room still completes successfully"), Exit.Succeeded(false, false));
		Exit.Reset();
		TestFalse(TEXT("Duplicate native completion cannot finish an idle request"), Exit.Succeeded(true, false));
	}
	FEEOSLobbyExitRequest Leave;
	Leave.Begin(TEXT("member"), false, FEEOSLobbyExitRequest::EContinuation::None);
	TestFalse(TEXT("Ordinary failed leave is not reported as successful"), Leave.Succeeded(false, false));
	TestTrue(TEXT("Successful leave with local removal succeeds"), Leave.Succeeded(true, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEEOSLobbyDeleteFailureTest,
	"UnrealExtendedEOS.Lobbies.Lifecycle.DeleteFailureAndStaleCompletion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FEEOSLobbyDeleteFailureTest::RunTest(const FString&)
{
	FEEOSLobbyExitRequest Exit;
	Exit.Begin(TEXT("old"), true, FEEOSLobbyExitRequest::EContinuation::None);
	const uint64 OldToken = Exit.GetToken();
	Exit.CompleteBackend(OldToken, TEXT("old"), false);
	TestFalse(TEXT("Backend deletion failure cannot erase remaining membership"), Exit.Succeeded(true, true));
	TestFalse(TEXT("Native leave success cannot conceal backend deletion failure"), Exit.Succeeded(true, false));
	Exit.Reset();
	Exit.Begin(TEXT("new"), true, FEEOSLobbyExitRequest::EContinuation::None);
	TestFalse(TEXT("Stale SDK result cannot delete a replacement lobby"), Exit.CompleteBackend(OldToken, TEXT("old"), true));
	TestFalse(TEXT("Correct token with wrong lobby cannot complete"), Exit.CompleteBackend(Exit.GetToken(), TEXT("old"), true));
	TestTrue(TEXT("Replacement is still waiting for its own result"), Exit.IsBackendPending());
	Exit.Reset();
	Exit.Begin(TEXT("old"), true, FEEOSLobbyExitRequest::EContinuation::None);
	TestFalse(TEXT("Reusing a lobby id does not reuse its operation token"), Exit.CompleteBackend(OldToken, TEXT("old"), true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEEOSLobbyRemoteClosureTest,
	"UnrealExtendedEOS.Lobbies.Lifecycle.RemoteClosureReconcilesFailureOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FEEOSLobbyRemoteClosureTest::RunTest(const FString&)
{
	UGameInstance* GI = NewObject<UGameInstance>();
	UEEOSLobbySubsystem* Lobby = NewObject<UEEOSLobbySubsystem>(GI);
	UEEOSLobbyLifecycleTestObserver* Observer = NewObject<UEEOSLobbyLifecycleTestObserver>(GI);
	Lobby->OnLobbyDestroyed.AddDynamic(Observer, &UEEOSLobbyLifecycleTestObserver::Destroyed);
	FNamedOnlineSession Native(Lobby->GetLobbySessionName(), MakeLobbyResult(TEXT("remote")).Session);
	Native.SessionState = EOnlineSessionState::Pending;
	FEEOSLobbyTestAccess::Refresh(Lobby, &Native);
	FEEOSLobbyTestAccess::RemoteExit(Lobby, &Native, false);
	TestTrue(TEXT("Failure retains a surviving native lobby"), Lobby->IsInLobby());
	TestEqual(TEXT("No departure notification while membership remains"), Observer->DestroyCount, 0);
	FEEOSLobbyTestAccess::RemoteExit(Lobby, nullptr, false);
	TestFalse(TEXT("Native removal clears membership even with failed EOS leave"), Lobby->IsInLobby());
	TestEqual(TEXT("One remote departure event"), Observer->DestroyCount, 1);
	TestFalse(TEXT("SDK failure remains available to callers"), Observer->bLastSuccess);
	TestEqual(TEXT("Event identifies the departed lobby"), Observer->LastId, FString(TEXT("remote")));
	FEEOSLobbyTestAccess::RemoteExit(Lobby, nullptr, true);
	TestEqual(TEXT("Duplicate native callback cannot emit a second departure"), Observer->DestroyCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEEOSLobbyRemovalSwitchTest,
	"UnrealExtendedEOS.Lobbies.Lifecycle.SwitchSuppressesLocalRemoval",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FEEOSLobbyRemovalSwitchTest::RunTest(const FString&)
{
	UGameInstance* GI = NewObject<UGameInstance>();
	UEEOSLobbySubsystem* Lobby = NewObject<UEEOSLobbySubsystem>(GI);
	UEEOSLobbyLifecycleTestObserver* Observer = NewObject<UEEOSLobbyLifecycleTestObserver>(GI);
	Lobby->OnLobbyDestroyed.AddDynamic(Observer, &UEEOSLobbyLifecycleTestObserver::Destroyed);
	FNamedOnlineSession Native(Lobby->GetLobbySessionName(), MakeLobbyResult(TEXT("old")).Session);
	Native.SessionState = EOnlineSessionState::Pending;
	for (bool bLeaving : {false, true})
	{
		FEEOSLobbyTestAccess::Refresh(Lobby, &Native);
		FEEOSLobbyTestAccess::Request(Lobby).Begin(MakeLobbyResult(TEXT("target")), bLeaving);
		TestFalse(TEXT("Local left callback cannot clear admission during either switch phase"), FEEOSLobbyTestAccess::LocalRemovalAllowed(Lobby));
		FEEOSLobbyTestAccess::RemoteExit(Lobby, nullptr, false);
		TestTrue(TEXT("Scoped join completion owns the cache during a switch"), Lobby->IsInLobby());
		TestEqual(TEXT("No unrelated destroy completion leaks into the new join"), Observer->DestroyCount, 0);
		FEEOSLobbyTestAccess::Request(Lobby).Reset();
	}
	TestTrue(TEXT("Unsolicited local removal is handled when idle"), FEEOSLobbyTestAccess::LocalRemovalAllowed(Lobby));
	FEEOSLobbyTestAccess::Exit(Lobby).Begin(TEXT("old"), true, FEEOSLobbyExitRequest::EContinuation::Create);
	TestFalse(TEXT("Create's backend-delete phase also owns local removal"), FEEOSLobbyTestAccess::LocalRemovalAllowed(Lobby));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEEOSLobbyPromotionTest,
	"UnrealExtendedEOS.Lobbies.Lifecycle.PromotionSerializationAndOwnerDeduplication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FEEOSLobbyPromotionTest::RunTest(const FString&)
{
	UGameInstance* GI = NewObject<UGameInstance>();
	UEEOSLobbySubsystem* Lobby = NewObject<UEEOSLobbySubsystem>(GI);
	UEEOSLobbyLifecycleTestObserver* Observer = NewObject<UEEOSLobbyLifecycleTestObserver>(GI);
	Lobby->OnLobbyPromotionComplete.AddDynamic(Observer, &UEEOSLobbyLifecycleTestObserver::Promoted);
	FNamedOnlineSession Native(Lobby->GetLobbySessionName(), MakeLobbyResult(TEXT("room")).Session);
	Native.SessionState = EOnlineSessionState::Pending;
	FEEOSLobbyTestAccess::Refresh(Lobby, &Native);
	TestFalse(TEXT("Initial owner is a baseline, not a transfer"), FEEOSLobbyTestAccess::Owner(Lobby, TEXT("host")));
	TestTrue(TEXT("Remote/automatic promotion changes owner"), FEEOSLobbyTestAccess::Owner(Lobby, TEXT("promoted")));
	TestFalse(TEXT("SDK/update/ticker duplicate owner is suppressed"), FEEOSLobbyTestAccess::Owner(Lobby, TEXT("promoted")));
	TestFalse(TEXT("Incomplete owner resolution cannot replace cached owner"), FEEOSLobbyTestAccess::Owner(Lobby, TEXT("")));
	FEEOSLobbyTestAccess::BeginPromotion(Lobby, 7, TEXT("room"));
	TestTrue(TEXT("Promotion holds the membership operation slot"), Lobby->IsLobbyOperationInFlight());
	TestFalse(TEXT("Cannot transfer twice concurrently"), Lobby->PromoteMember(TEXT("next")));
	TestFalse(TEXT("Cannot switch during transfer"), Lobby->JoinLobbyResult(MakeLobbyResult(TEXT("other"))));
	FEEOSLobbyTestAccess::Promotion(Lobby, 6, TEXT("room"), TEXT("promoted"), true);
	TestEqual(TEXT("Stale promotion token cannot notify"), Observer->PromotionCount, 0);
	TestTrue(TEXT("Stale callback cannot release the current operation"), Lobby->IsLobbyOperationInFlight());
	FEEOSLobbyTestAccess::Promotion(Lobby, 7, TEXT("room"), TEXT("promoted"), false);
	TestEqual(TEXT("Transfer failure gets an explicit completion"), Observer->PromotionCount, 1);
	TestFalse(TEXT("Failure reported accurately"), Observer->bLastSuccess);
	TestFalse(TEXT("Failed transfer releases the guard"), Lobby->IsLobbyOperationInFlight());
	FEEOSLobbyTestAccess::BeginPromotion(Lobby, 8, TEXT("room"));
	FEEOSLobbyTestAccess::Refresh(Lobby, nullptr);
	FEEOSLobbyTestAccess::Promotion(Lobby, 8, TEXT("room"), TEXT("promoted"), true);
	TestEqual(TEXT("Old-room completion cannot notify after remote closure"), Observer->PromotionCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEEOSLobbyShutdownContinuationTest,
	"UnrealExtendedEOS.Lobbies.Lifecycle.ShutdownCancelsContinuations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FEEOSLobbyShutdownContinuationTest::RunTest(const FString&)
{
	UGameInstance* GI = NewObject<UGameInstance>();
	UEEOSLobbySubsystem* Lobby = NewObject<UEEOSLobbySubsystem>(GI);
	UEEOSLobbyLifecycleTestObserver* Observer = NewObject<UEEOSLobbyLifecycleTestObserver>(GI);
	Lobby->OnLobbyCreated.AddDynamic(Observer, &UEEOSLobbyLifecycleTestObserver::Created);
	Lobby->OnLobbyJoined.AddDynamic(Observer, &UEEOSLobbyLifecycleTestObserver::Joined);
	Lobby->OnLobbyDestroyed.AddDynamic(Observer, &UEEOSLobbyLifecycleTestObserver::Destroyed);
	FEEOSLobbyTestAccess::SetShuttingDown(Lobby, true);
	for (auto Continuation : {FEEOSLobbyExitRequest::EContinuation::Create, FEEOSLobbyExitRequest::EContinuation::Join})
	{
		FEEOSLobbyTestAccess::Request(Lobby).Begin(MakeLobbyResult(TEXT("target")), true);
		auto& Exit = FEEOSLobbyTestAccess::Exit(Lobby);
		Exit.Begin(TEXT("old"), true, Continuation);
		Exit.CompleteBackend(Exit.GetToken(), TEXT("old"), true);
		FEEOSLobbyTestAccess::NativeExit(Lobby, false);
		TestFalse(TEXT("Shutdown consumes the exit without starting the staged operation"), Lobby->IsLobbyOperationInFlight());
		TestEqual(TEXT("No create gameplay callback during teardown"), Observer->CreateCount, 0);
		TestEqual(TEXT("No join travel callback during teardown"), Observer->JoinCount, 0);
		TestEqual(TEXT("No close UI callback during teardown"), Observer->DestroyCount, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEEOSLobbyDuplicateNativeCleanupTest,
	"UnrealExtendedEOS.Lobbies.Lifecycle.DuplicateNativeCleanupCannotConsumeTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FEEOSLobbyDuplicateNativeCleanupTest::RunTest(const FString&)
{
	UGameInstance* GI = NewObject<UGameInstance>();
	UEEOSLobbySubsystem* Lobby = NewObject<UEEOSLobbySubsystem>(GI);
	UEEOSLobbyLifecycleTestObserver* Observer = NewObject<UEEOSLobbyLifecycleTestObserver>(GI);
	Lobby->OnLobbyJoined.AddDynamic(Observer, &UEEOSLobbyLifecycleTestObserver::Joined);
	FEEOSLobbyTestAccess::SetShuttingDown(Lobby, true); // Finish without entering a real OSS.
	FEEOSLobbyTestAccess::Request(Lobby).Begin(MakeLobbyResult(TEXT("target")), true);
	auto& Exit = FEEOSLobbyTestAccess::Exit(Lobby);
	Exit.Begin(TEXT("old"), true, FEEOSLobbyExitRequest::EContinuation::Join);
	Exit.CompleteBackend(Exit.GetToken(), TEXT("old"), true);
	FEEOSLobbyTestAccess::ExpectNativeExits(Lobby, 2); // local cleanup + racing CLOSED cleanup
	FEEOSLobbyTestAccess::NativeExit(Lobby, false);
	TestTrue(TEXT("First leave cannot release the old lobby's operation guard"), Lobby->IsLobbyOperationInFlight());
	TestTrue(TEXT("Target remains staged until both old callbacks drain"), FEEOSLobbyTestAccess::Request(Lobby).IsLeaving());
	TestEqual(TEXT("No target completion after the first callback"), Observer->JoinCount, 0);
	FEEOSLobbyTestAccess::NativeExit(Lobby, false);
	TestFalse(TEXT("Second old callback permits terminal cleanup"), Lobby->IsLobbyOperationInFlight());
	TestEqual(TEXT("Shutdown still cannot dispatch target travel"), Observer->JoinCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEEOSLobbyTransferPolicyTest,
	"UnrealExtendedEOS.Lobbies.Lifecycle.DisabledTransferDoesNotStartOrComplete",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FEEOSLobbyTransferPolicyTest::RunTest(const FString&)
{
	UEEOSSettings* Settings = GetMutableDefault<UEEOSSettings>();
	const bool bPrevious = Settings->bAllowLobbyOwnerTransfer;
	ON_SCOPE_EXIT { Settings->bAllowLobbyOwnerTransfer = bPrevious; };
	Settings->bAllowLobbyOwnerTransfer = false;
	UGameInstance* GI = NewObject<UGameInstance>();
	UEEOSLobbySubsystem* Lobby = NewObject<UEEOSLobbySubsystem>(GI);
	UEEOSLobbyLifecycleTestObserver* Observer = NewObject<UEEOSLobbyLifecycleTestObserver>(GI);
	Lobby->OnLobbyPromotionComplete.AddDynamic(Observer, &UEEOSLobbyLifecycleTestObserver::Promoted);
	TestFalse(TEXT("Policy rejects ownership transfer before native/SDK calls"), Lobby->PromoteMember(TEXT("member")));
	TestFalse(TEXT("Policy rejection leaves no pending operation"), Lobby->IsLobbyOperationInFlight());
	TestEqual(TEXT("Rejected transfer cannot consume another caller's completion"), Observer->PromotionCount, 0);
	return true;
}
