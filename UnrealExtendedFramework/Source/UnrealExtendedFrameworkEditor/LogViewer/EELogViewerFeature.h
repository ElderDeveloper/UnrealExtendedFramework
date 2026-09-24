// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

class FSpawnTabArgs;
class FWorkspaceItem;
class SDockTab;
class SWindow;

/**
 * Extended Log editor feature.
 *
 * Registers the Extended Log nomad tab in Window > Log (beside Output Log and Message Log), and
 * marks every PIE / Simulate session in the EF_LOG files with a start and end separator, which is
 * what the window's "Current PIE run only" filter keys on.
 * Plan: Documents/ExtendedFrameworkLogSystemPlan.md (DOP repository).
 */
class FEELogViewerFeature
{
public:
	static const FName TabId;

	void Register(const TSharedRef<FWorkspaceItem>& InGroup);
	void Unregister();

private:
	TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs& Args);

	/**
	 * The run's markers bracket everything it logs: the start goes in at BeginPIE, before any PIE
	 * GameInstance (and its subsystems) exists, and the end at ShutdownPIE, after the worlds are torn
	 * down. PostPIEStarted and EndPIE would leave initialization and teardown lines outside the run.
	 */
	void HandleBeginPIE(const bool bIsSimulating);
	void HandleEndPIE(const bool bIsSimulating);
	void HandleShutdownPIE(const bool bIsSimulating);
	void CloseRun();

	/** EF_LOG could not create its folder: say so once the main frame can show a notification. */
	void HandleMainFrameCreated(TSharedPtr<SWindow> MainWindow, bool bIsRunningStartupDialog);
	static void ShowOutputOffNotification();

	/** "L_CastleTest | listen server + 1 client", from the settings the session actually started with. */
	static FString DescribePlaySession(bool bIsSimulating);

	FDelegateHandle BeginPIEHandle;
	FDelegateHandle EndPIEHandle;
	FDelegateHandle ShutdownPIEHandle;
	FDelegateHandle MainFrameHandle;
	int32 RunCount = 0;
	bool bRunOpen = false;
	bool bRunIsSimulation = false;
	bool bRegistered = false;
};

#endif // WITH_EDITOR
