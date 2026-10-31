// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EDbgLibrary.h"

#include "EDbgSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Subsystems/EFSubsystems.h"

bool UEDbgLibrary::AreDebugToolsEnabled(const UObject* WorldContextObject)
{
#if EXTENDED_DEBUG_ENABLED
	return UEDbgSubsystem::Get(WorldContextObject) != nullptr;
#else
	return false;
#endif
}

void UEDbgLibrary::RegisterCheat(const UObject* WorldContextObject, const FEDbgCheatInfo& Info, FEDbgCheatDelegate Handler)
{
	if (UEDbgSubsystem* Debug = UEDbgSubsystem::Get(WorldContextObject))
	{
		Debug->RegisterBlueprintCheat(Info, Handler);
	}
}

void UEDbgLibrary::UnregisterCheat(const UObject* WorldContextObject, FName CheatName)
{
	if (UEDbgSubsystem* Debug = UEDbgSubsystem::Get(WorldContextObject))
	{
		Debug->UnregisterCheat(CheatName);
	}
}

bool UEDbgLibrary::RunCheat(const UObject* WorldContextObject, FName CheatName, const TArray<FString>& Arguments)
{
	UEDbgSubsystem* Debug = UEDbgSubsystem::Get(WorldContextObject);
	if (!Debug)
	{
		return false;
	}
	UGameInstance* GameInstance = EF::GetGameInstance(WorldContextObject);
	return Debug->RunCheat(GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr, CheatName, Arguments);
}

TArray<FEDbgCheatInfo> UEDbgLibrary::GetCheats(const UObject* WorldContextObject)
{
	const UEDbgSubsystem* Debug = UEDbgSubsystem::Get(WorldContextObject);
	return Debug ? Debug->GetCheats() : TArray<FEDbgCheatInfo>();
}

void UEDbgLibrary::SetDebugWatch(const UObject* WorldContextObject, const FString& Key, const FString& Value)
{
	if (UEDbgSubsystem* Debug = UEDbgSubsystem::Get(WorldContextObject))
	{
		Debug->SetWatch(Key, Value);
	}
}

void UEDbgLibrary::ClearDebugWatch(const UObject* WorldContextObject, const FString& Key)
{
	if (UEDbgSubsystem* Debug = UEDbgSubsystem::Get(WorldContextObject))
	{
		Debug->ClearWatch(Key);
	}
}

void UEDbgLibrary::ToggleDebugMenu(const UObject* WorldContextObject)
{
	if (UEDbgSubsystem* Debug = UEDbgSubsystem::Get(WorldContextObject))
	{
		Debug->ToggleMenu();
	}
}
