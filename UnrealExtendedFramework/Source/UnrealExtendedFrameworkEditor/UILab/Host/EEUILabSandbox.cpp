// Copyright Moon Punch Games. All Rights Reserved.

#include "EEUILabSandbox.h"

#if WITH_EDITOR

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "InputMappingContext.h"
#include "Subsystems/GameInstanceSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogEEUILabSandbox, Log, All);

FEEUILabSandbox::~FEEUILabSandbox()
{
	Shutdown();
}

bool FEEUILabSandbox::Initialize(const FEEUILabSandboxParams& Params, FString& OutError)
{
	if (bInitialized)
	{
		OutError = TEXT("Sandbox already initialized.");
		return false;
	}

	if (!GEngine)
	{
		OutError = TEXT("No engine.");
		return false;
	}

	UClass* GameInstanceClass = Params.GameInstanceClass ? *Params.GameInstanceClass : UGameInstance::StaticClass();
	UClass* GameModeClass = Params.GameModeClass ? *Params.GameModeClass : AGameModeBase::StaticClass();

	// Standalone game instance with a dummy game world — the engine's automation-test pattern.
	UGameInstance* NewGameInstance = NewObject<UGameInstance>(GEngine, GameInstanceClass);
	if (!NewGameInstance)
	{
		OutError = TEXT("Failed to create the sandbox GameInstance.");
		return false;
	}
	GameInstance = TStrongObjectPtr<UGameInstance>(NewGameInstance);
	NewGameInstance->InitializeStandalone(TEXT("EEUILabSandboxWorld"));

	UWorld* World = NewGameInstance->GetWorld();
	if (!World)
	{
		OutError = TEXT("The standalone GameInstance produced no world.");
		Shutdown();
		return false;
	}

	// Explicit game mode: AGameModeBase by default so the sandbox never drags project startup code
	// in unless the fixture explicitly requests it.
	FURL URL;
	URL.AddOption(*FString::Printf(TEXT("game=%s"), *GameModeClass->GetPathName()));
	World->SetGameMode(URL);
	World->InitializeActorsForPlay(URL);
	World->BeginPlay();

	FString PlayerError;
	ULocalPlayer* NewLocalPlayer = NewGameInstance->CreateLocalPlayer(0, PlayerError, /*bSpawnPlayerController*/ true);
	if (!NewLocalPlayer)
	{
		OutError = FString::Printf(TEXT("Failed to create the sandbox LocalPlayer: %s"), *PlayerError);
		Shutdown();
		return false;
	}
	LocalPlayer = NewLocalPlayer;
	PlayerController = NewLocalPlayer->GetPlayerController(World);

	if (!PlayerController.IsValid())
	{
		OutError = TEXT("The sandbox LocalPlayer produced no PlayerController.");
		Shutdown();
		return false;
	}

	// Report fixture-declared subsystems that did not initialize in this standalone environment.
	for (const TSoftClassPtr<UGameInstanceSubsystem>& SubsystemClass : Params.RequiredSubsystems)
	{
		UClass* LoadedClass = SubsystemClass.LoadSynchronous();
		if (!LoadedClass)
		{
			MissingSubsystems.Add(SubsystemClass.ToString());
			continue;
		}
		if (!NewGameInstance->GetSubsystemBase(TSubclassOf<UGameInstanceSubsystem>(LoadedClass)))
		{
			MissingSubsystems.Add(LoadedClass->GetName());
		}
	}
	for (const FString& Missing : MissingSubsystems)
	{
		UE_LOG(LogEEUILabSandbox, Warning, TEXT("Required subsystem '%s' is not available in the sandbox."), *Missing);
	}

	// The editor does not tick standalone game worlds; drive it from the core ticker.
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateRaw(this, &FEEUILabSandbox::TickWorld));

	bInitialized = true;
	return true;
}

void FEEUILabSandbox::Shutdown()
{
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}

	if (UGameInstance* GI = GameInstance.Get())
	{
		UWorld* World = GI->GetWorld();

		if (World)
		{
			World->BeginTearingDown();
		}

		// Removes local players and deinitializes subsystems.
		GI->Shutdown();

		if (World)
		{
			World->DestroyWorld(/*bInformEngineOfWorld*/ true);
			if (GEngine)
			{
				GEngine->DestroyWorldContext(World);
			}
		}
	}

	PlayerController.Reset();
	LocalPlayer.Reset();
	GameInstance.Reset();
	MissingSubsystems.Reset();
	bInitialized = false;
}

UWorld* FEEUILabSandbox::GetWorld() const
{
	return GameInstance.IsValid() ? GameInstance->GetWorld() : nullptr;
}

void FEEUILabSandbox::ApplyInputMappingContexts(const TArray<TSoftObjectPtr<UInputMappingContext>>& Contexts)
{
	ULocalPlayer* Player = LocalPlayer.Get();
	if (!Player)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = Player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!InputSubsystem)
	{
		UE_LOG(LogEEUILabSandbox, Warning, TEXT("Enhanced Input local-player subsystem unavailable in the sandbox."));
		return;
	}

	int32 Priority = 0;
	for (const TSoftObjectPtr<UInputMappingContext>& ContextRef : Contexts)
	{
		if (UInputMappingContext* Context = ContextRef.LoadSynchronous())
		{
			InputSubsystem->AddMappingContext(Context, Priority++);
		}
	}
}

bool FEEUILabSandbox::TickWorld(float DeltaTime)
{
	if (UWorld* World = GetWorld())
	{
		// Clamp so breakpoints/hitches don't produce huge simulation steps.
		World->Tick(LEVELTICK_All, FMath::Min(DeltaTime, 0.1f));
	}
	return true;
}

#endif // WITH_EDITOR
