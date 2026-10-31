// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EDbgTypes.generated.h"

class APlayerController;
class UWorld;

/** Describes a cheat for the console, the debug menu and Get Cheats. */
USTRUCT(BlueprintType)
struct EXTENDEDDEBUG_API FEDbgCheatInfo
{
	GENERATED_BODY()

	/** Typed after Extended.Cheat, e.g. Extended.Cheat GiveGold 500. Unique. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cheat")
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cheat")
	FString Help;

	/** Groups cheats in the debug menu and in Extended.Cheat.List. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cheat")
	FName Category = TEXT("Game");

	/** Shown in the menu's argument box, e.g. "<amount>". Empty: the cheat takes no arguments. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cheat")
	FString ArgumentHint;

	/**
	 * In multiplayer, runs on the server for the player who asked instead of on that player's
	 * machine. Register such a cheat on the server too.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cheat")
	bool bRunOnServer = false;
};

/** What a C++ cheat handler receives. */
struct EXTENDEDDEBUG_API FEDbgCheatContext
{
	/** The player who ran the cheat; null when it came from a console without a player. */
	APlayerController* Player = nullptr;
	UWorld* World = nullptr;
	TArray<FString> Arguments;

	/** Arguments[Index] as text, or Default. */
	FString GetString(int32 Index, const FString& Default = FString()) const;
	float GetFloat(int32 Index, float Default = 0.f) const;
	int32 GetInt(int32 Index, int32 Default = 0) const;
};

/** Blueprint cheat handler: the player who ran it (may be null) and the words typed after its name. */
DECLARE_DYNAMIC_DELEGATE_TwoParams(FEDbgCheatDelegate, APlayerController*, Player, const TArray<FString>&, Arguments);

/** A value shown on screen by the watch overlay. */
USTRUCT(BlueprintType)
struct EXTENDEDDEBUG_API FEDbgWatch
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	FString Value;

	/** FPlatformTime::Seconds() of the last update. */
	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	double UpdatedSeconds = 0.0;
};
