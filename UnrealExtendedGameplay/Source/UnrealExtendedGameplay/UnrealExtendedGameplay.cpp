// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealExtendedGameplay.h"

#if WITH_GAMEPLAY_DEBUGGER
#include "GameplayDebugger.h"
#include "AI/StateMachine/GameplayDebugger/EGStateMachineGameplayDebuggerCategory.h"
#endif

#define LOCTEXT_NAMESPACE "FUnrealExtendedGameplayModule"

#if WITH_GAMEPLAY_DEBUGGER
static const FName EGStateMachineDebuggerCategoryName(TEXT("EGStateMachine"));
#endif

void FUnrealExtendedGameplayModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

#if WITH_GAMEPLAY_DEBUGGER
	IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();
	GameplayDebuggerModule.RegisterCategory(
		EGStateMachineDebuggerCategoryName,
		IGameplayDebugger::FOnGetCategory::CreateStatic(&FEGStateMachineGameplayDebuggerCategory::MakeInstance),
		EGameplayDebuggerCategoryState::EnabledInGameAndSimulate);
	GameplayDebuggerModule.NotifyCategoriesChanged();
#endif
}

void FUnrealExtendedGameplayModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

#if WITH_GAMEPLAY_DEBUGGER
	if (IGameplayDebugger::IsAvailable())
	{
		IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();
		GameplayDebuggerModule.UnregisterCategory(EGStateMachineDebuggerCategoryName);
		GameplayDebuggerModule.NotifyCategoriesChanged();
	}
#endif
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUnrealExtendedGameplayModule, UnrealExtendedGameplay)
