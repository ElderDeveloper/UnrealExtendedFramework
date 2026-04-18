// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Shared/ESQLTypes.h"
#include "ESQLPlayerDBComponent.generated.h"

class UESQLSubsystem;


/**
 * Attach to a PlayerState (server-side) to give that player
 * their own SQLite database scoped by UniqueNetId.
 *
 * Lives on PlayerState (not PlayerController) so the database
 * handle survives seamless travel without any special teardown/reopen logic.
 *
 * Typical setup:
 *   1. Override AGameModeBase::GetPlayerStateClass() to return your
 *      custom APlayerState subclass that has this component, OR
 *   2. In your GameMode Blueprint PostLogin, add the component to
 *      NewPlayer->PlayerState dynamically.
 *      — or configure bAutoOpenPlayerDatabases in Project Settings and
 *        the subsystem does it for you via the subsystem API (no component needed).
 *   3. Use the component as player-db ownership/state metadata while higher-level
 *      player data workflows are rebuilt on top of table/id APIs.
 *
 * The subsystem API (OpenPlayerDatabase, ExecutePlayerSQL, etc.) takes
 * APlayerController* for convenience — both pathways resolve to the
 * same UniqueNetId-keyed database file.
 *
 * NOTE: Server-side only. bReplicates is false. On clients the component is inert.
 */
UCLASS(ClassGroup=(SQL), meta=(BlueprintSpawnableComponent))
class UNREALEXTENDEDSQL_API UESQLPlayerDBComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UESQLPlayerDBComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;


	// ── State ────────────────────────────────────────────────────────────

	/** The UniqueNetId string for this player (extracted from owning PlayerState). */
	UFUNCTION(BlueprintPure, Category = "SQL|Player")
	FString GetPlayerId() const;

	/** True if this component's database is currently open. */
	UFUNCTION(BlueprintPure, Category = "SQL|Player")
	bool IsPlayerDatabaseOpen() const;

	/** Returns the owning PlayerState (convenience cast). */
	UFUNCTION(BlueprintPure, Category = "SQL|Player")
	APlayerState* GetOwningPlayerState() const;


	// ── Config ───────────────────────────────────────────────────────────

	/** The logical database name to create for this player.
	    Defaults to "PlayerData". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL|Player")
	FString PlayerDatabaseName = TEXT("PlayerData");

private:

	FString CachedPlayerId;
	TWeakObjectPtr<UESQLSubsystem> SQLSubsystem;

	/** Resolve the UniqueNetId from the owning PlayerState. */
	FString ResolvePlayerId() const;

	/** Get the internal DB key used for the player databases map. */
	FString GetPlayerDBKey() const;
};
