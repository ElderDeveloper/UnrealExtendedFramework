// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Containers/Ticker.h"
#include "UObject/StrongObjectPtr.h"

class AGameModeBase;
class APlayerController;
class UGameInstance;
class UGameInstanceSubsystem;
class UInputMappingContext;
class ULocalPlayer;
class UUserWidget;
class UWorld;

/** Everything needed to initialize a sandbox session. */
struct FEEUILabSandboxParams
{
	/** Defaults to plain UGameInstance — never the project's game instance unless the fixture asks. */
	TSubclassOf<UGameInstance> GameInstanceClass;
	/** Defaults to AGameModeBase — never the project's game mode unless the fixture asks. */
	TSubclassOf<AGameModeBase> GameModeClass;
	/** Subsystems the fixture declares as required; missing ones are reported, not fatal. */
	TArray<TSoftClassPtr<UGameInstanceSubsystem>> RequiredSubsystems;
};

/**
 * UL-3 Runtime-Faithful Sandbox.
 *
 * A transient, editor-owned game session: standalone UGameInstance (the engine's own automation
 * pattern), dummy game world with BeginPlay, a real ULocalPlayer + APlayerController, and Enhanced
 * Input mapping contexts. Widgets created against the player controller resolve GetOwningPlayer,
 * GetWorld, and subsystem access exactly as at runtime. Never opens a gameplay map.
 *
 * Teardown is deterministic: Shutdown() ends play, tears the world down, shuts down the game
 * instance, destroys the world and its context, and stops ticking. The destructor guarantees it.
 */
class UNREALEXTENDEDFRAMEWORKEDITOR_API FEEUILabSandbox
{
public:
	~FEEUILabSandbox();

	/**
	 * Creates the sandbox session. Returns false (with OutError set) when any stage fails;
	 * a failed sandbox is safe to destroy and the host should fall back to the Fast Host.
	 */
	bool Initialize(const FEEUILabSandboxParams& Params, FString& OutError);

	void Shutdown();

	bool IsInitialized() const { return bInitialized; }
	UWorld* GetWorld() const;
	APlayerController* GetPlayerController() const { return PlayerController.Get(); }
	ULocalPlayer* GetLocalPlayer() const { return LocalPlayer.Get(); }

	/** Applies Enhanced Input mapping contexts to the sandbox local player (priority = array order). */
	void ApplyInputMappingContexts(const TArray<TSoftObjectPtr<UInputMappingContext>>& Contexts);

	/** Missing required subsystems found during Initialize (fixture-declared). */
	const TArray<FString>& GetMissingSubsystems() const { return MissingSubsystems; }

private:
	bool TickWorld(float DeltaTime);

	TStrongObjectPtr<UGameInstance> GameInstance;
	TWeakObjectPtr<APlayerController> PlayerController;
	TWeakObjectPtr<ULocalPlayer> LocalPlayer;
	FTSTicker::FDelegateHandle TickHandle;
	TArray<FString> MissingSubsystems;
	bool bInitialized = false;
};

#endif // WITH_EDITOR
