// Copyright Devil of the Plague. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "EGQuestFactsSubsystem.h"
#include "EGQuestFactsReplicationRelay.generated.h"

class AEGQuestFactsReplicationRelay;

USTRUCT()
struct FEGQuestReplicatedFactItem : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTag Tag;

	UPROPERTY()
	int32 Value = 0;

	UPROPERTY()
	EEGQuestFactScope Scope = EEGQuestFactScope::World;

	UPROPERTY()
	FString PlayerId;
};

USTRUCT()
struct FEGQuestReplicatedFactArray : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FEGQuestReplicatedFactItem> Items;

	AEGQuestFactsReplicationRelay* Owner = nullptr;

	void PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize);
	void PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FastArrayDeltaSerialize<FEGQuestReplicatedFactItem, FEGQuestReplicatedFactArray>(Items, DeltaParms, *this);
	}
};

template<>
struct TStructOpsTypeTraits<FEGQuestReplicatedFactArray> : public TStructOpsTypeTraitsBase2<FEGQuestReplicatedFactArray>
{
	enum { WithNetDeltaSerializer = true };
};

/** Always-relevant transport for explicitly allowlisted presentation facts. */
UCLASS(NotPlaceable, Transient)
class UNREALEXTENDEDQUEST_API AEGQuestFactsReplicationRelay : public AActor
{
	GENERATED_BODY()

public:
	AEGQuestFactsReplicationRelay();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void SetReplicatedFact(FGameplayTag Tag, int32 Value, EEGQuestFactScope Scope, const FString& PlayerId);
	void HandleReplicatedItem(const FEGQuestReplicatedFactItem& Item);

private:
	UPROPERTY(Replicated)
	FEGQuestReplicatedFactArray ReplicatedFacts;
};
