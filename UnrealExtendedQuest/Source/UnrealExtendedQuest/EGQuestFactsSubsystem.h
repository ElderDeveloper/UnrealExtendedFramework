// Copyright Devil of the Plague. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "EGQuestFactsSubsystem.generated.h"

class AActor;
class AEGQuestFactsReplicationRelay;
class APlayerState;

UENUM(BlueprintType)
enum class EEGQuestFactScope : uint8
{
	World,
	Player
};

USTRUCT(BlueprintType)
struct UNREALEXTENDEDQUEST_API FEGQuestFactSaveEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Facts")
	FGameplayTag Tag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Facts")
	int32 Value = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Facts")
	EEGQuestFactScope Scope = EEGQuestFactScope::World;

	/** Stable player identity used to reconnect player-scoped facts after travel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Facts")
	FString PlayerId;
};

USTRUCT(BlueprintType)
struct UNREALEXTENDEDQUEST_API FEGQuestFactsSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Facts")
	int32 SchemaVersion = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Facts")
	TArray<FEGQuestFactSaveEntry> Entries;
};

USTRUCT()
struct FEGQuestPlayerFactStore
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<FGameplayTag, int32> Values;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_SixParams(FEGQuestFactChanged, FGameplayTag, Tag, int32, OldValue,
	int32, NewValue, EEGQuestFactScope, Scope, APlayerState*, Player, AActor*, Instigator);

/**
 * Authority-owned, persistent world truth used by trackers and predicates. Facts are integer
 * summaries keyed by gameplay tag; booleans use 0/1 and HasFact means value != 0.
 */
UCLASS()
class UNREALEXTENDEDQUEST_API UEGQuestFactsSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	UFUNCTION(BlueprintPure, Category = "Quest|Facts")
	int32 GetFact(FGameplayTag Tag, EEGQuestFactScope Scope = EEGQuestFactScope::World,
		APlayerState* Player = nullptr) const;

	UFUNCTION(BlueprintPure, Category = "Quest|Facts")
	bool HasFact(FGameplayTag Tag, EEGQuestFactScope Scope = EEGQuestFactScope::World,
		APlayerState* Player = nullptr) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Quest|Facts")
	bool SetFact(FGameplayTag Tag, int32 Value, EEGQuestFactScope Scope = EEGQuestFactScope::World,
		APlayerState* Player = nullptr, AActor* Instigator = nullptr);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Quest|Facts")
	bool AddToFact(FGameplayTag Tag, int32 Delta, EEGQuestFactScope Scope = EEGQuestFactScope::World,
		APlayerState* Player = nullptr, AActor* Instigator = nullptr);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Quest|Facts")
	bool ClearFact(FGameplayTag Tag, EEGQuestFactScope Scope = EEGQuestFactScope::World,
		APlayerState* Player = nullptr, AActor* Instigator = nullptr);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Quest|Facts|Save")
	FEGQuestFactsSaveData ExtractFactsSaveData() const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Quest|Facts|Save")
	bool RestoreFactsSaveData(const FEGQuestFactsSaveData& SaveData, bool bReplaceExisting = true);

	/** Prefix allowlist policy used by the relay. Public for presentation and policy tests. */
	UFUNCTION(BlueprintPure, Category = "Quest|Facts|Replication")
	bool ShouldReplicateFact(FGameplayTag Tag) const;

	UPROPERTY(BlueprintAssignable, Category = "Quest|Facts")
	FEGQuestFactChanged OnFactChanged;

	/** Called by the replicated relay on clients; not a gameplay mutation API. */
	void ApplyReplicatedFact(FGameplayTag Tag, int32 Value, EEGQuestFactScope Scope, const FString& PlayerId);

	static FString GetPlayerScopeId(const APlayerState* Player);

private:
	bool HasAuthority() const;
	bool SetFactInternal(FGameplayTag Tag, int32 Value, EEGQuestFactScope Scope, APlayerState* Player,
		const FString& ExplicitPlayerId, AActor* Instigator, bool bFromReplication);
	void EnsureReplicationRelay();
	void PushFactToRelay(FGameplayTag Tag, int32 Value, EEGQuestFactScope Scope, const FString& PlayerId);

	UPROPERTY(Transient)
	TMap<FGameplayTag, int32> WorldFacts;

	UPROPERTY(Transient)
	TMap<FString, FEGQuestPlayerFactStore> PlayerFacts;

	UPROPERTY(Transient)
	TObjectPtr<AEGQuestFactsReplicationRelay> ReplicationRelay;
};
