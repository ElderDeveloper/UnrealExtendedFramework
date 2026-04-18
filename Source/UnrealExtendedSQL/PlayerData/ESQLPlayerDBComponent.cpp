// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ESQLPlayerDBComponent.h"
#include "Subsystem/ESQLSubsystem.h"
#include "Shared/ESQLSettings.h"
#include "UnrealExtendedSQL.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"


// ── Construction ─────────────────────────────────────────────────────────────

UESQLPlayerDBComponent::UESQLPlayerDBComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsInitializeComponent = false;

	// Server-side only — not replicated to clients
	SetIsReplicatedByDefault(false);
}


// ── Lifecycle ────────────────────────────────────────────────────────────────

void UESQLPlayerDBComponent::BeginPlay()
{
	Super::BeginPlay();

	// Only operate on the server
	UWorld* World = GetWorld();
	if (!World) return;

	const ENetMode NetMode = World->GetNetMode();
	if (NetMode == NM_Client)
	{
		UE_LOG(LogExtendedSQL, Verbose, TEXT("PlayerDBComponent: Skipping on client (inert)"));
		return;
	}

	// Resolve player identity
	CachedPlayerId = ResolvePlayerId();
	if (CachedPlayerId.IsEmpty())
	{
		UE_LOG(LogExtendedSQL, Warning, TEXT("PlayerDBComponent: Could not resolve player identity on BeginPlay. Will retry on first query."));
	}

	// Get subsystem
	UGameInstance* GI = World->GetGameInstance();
	if (GI)
	{
		SQLSubsystem = GI->GetSubsystem<UESQLSubsystem>();
	}

	if (!SQLSubsystem.IsValid())
	{
		UE_LOG(LogExtendedSQL, Warning, TEXT("PlayerDBComponent: UESQLSubsystem not available"));
		return;
	}

	// Auto-open the player database
	if (!CachedPlayerId.IsEmpty())
	{
		// Get the owning PlayerState's controller for the subsystem API
		APlayerState* PS = GetOwningPlayerState();
		APlayerController* PC = PS ? PS->GetPlayerController() : nullptr;

		if (PC)
		{
			FESQLQueryResult Result = SQLSubsystem->OpenPlayerDatabase(PlayerDatabaseName, PC);
			if (Result.bSuccess)
			{
				UE_LOG(LogExtendedSQL, Log, TEXT("PlayerDBComponent: Opened database '%s' for player '%s'"),
					*PlayerDatabaseName, *CachedPlayerId);
			}
			else
			{
				UE_LOG(LogExtendedSQL, Warning, TEXT("PlayerDBComponent: Failed to open database: %s"), *Result.ErrorMessage);
			}
		}
	}
}

void UESQLPlayerDBComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Close the player database
	if (SQLSubsystem.IsValid() && !CachedPlayerId.IsEmpty())
	{
		APlayerState* PS = GetOwningPlayerState();
		APlayerController* PC = PS ? PS->GetPlayerController() : nullptr;

		if (PC)
		{
			const UESQLSettings* Settings = UESQLSettings::Get();
			const bool bDeleteFile = Settings ? Settings->bDeletePlayerDBOnDisconnect : false;

			SQLSubsystem->ClosePlayerDatabase(PlayerDatabaseName, PC, bDeleteFile);

			UE_LOG(LogExtendedSQL, Log, TEXT("PlayerDBComponent: Closed database '%s' for player '%s'%s"),
				*PlayerDatabaseName, *CachedPlayerId,
				bDeleteFile ? TEXT(" (file deleted)") : TEXT(""));
		}
	}

	Super::EndPlay(EndPlayReason);
}
// ── State ────────────────────────────────────────────────────────────────

FString UESQLPlayerDBComponent::GetPlayerId() const
{
	if (!CachedPlayerId.IsEmpty())
	{
		return CachedPlayerId;
	}
	return ResolvePlayerId();
}

bool UESQLPlayerDBComponent::IsPlayerDatabaseOpen() const
{
	if (!SQLSubsystem.IsValid())
	{
		return false;
	}

	APlayerState* PS = GetOwningPlayerState();
	APlayerController* PC = PS ? PS->GetPlayerController() : nullptr;
	return PC && SQLSubsystem->IsPlayerDatabaseOpen(PlayerDatabaseName, PC);
}

APlayerState* UESQLPlayerDBComponent::GetOwningPlayerState() const
{
	return Cast<APlayerState>(GetOwner());
}


// ── Internal ─────────────────────────────────────────────────────────────────

FString UESQLPlayerDBComponent::ResolvePlayerId() const
{
	APlayerState* PS = GetOwningPlayerState();
	if (!PS) return FString();

	// Try UniqueNetId
	FUniqueNetIdRepl UniqueNetIdRepl = PS->GetUniqueId();
	if (UniqueNetIdRepl.IsValid())
	{
		return UniqueNetIdRepl->ToString();
	}

	// PIE fallback
#if WITH_EDITOR
	return FString::Printf(TEXT("LOCAL_%d"), UE::GetPlayInEditorID());
#else
	// Non-editor fallback using player index
	APlayerController* PC = PS->GetPlayerController();
	if (PC && PC->GetLocalPlayer())
	{
		return FString::Printf(TEXT("LOCAL_%d"), PC->GetLocalPlayer()->GetControllerId());
	}
	return FString::Printf(TEXT("LOCAL_0"));
#endif
}

FString UESQLPlayerDBComponent::GetPlayerDBKey() const
{
	// The subsystem stores player databases under PlayerId → DatabaseName
	// We can't query that directly via public API, so check if the subsystem
	// reports the database as open using a heuristic
	return PlayerDatabaseName;
}
