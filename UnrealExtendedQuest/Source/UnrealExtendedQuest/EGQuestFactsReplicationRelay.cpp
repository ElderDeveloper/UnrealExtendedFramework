// Copyright Devil of the Plague. All Rights Reserved.
#include "EGQuestFactsReplicationRelay.h"

#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

void FEGQuestReplicatedFactArray::PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize)
{
	if (!Owner) return;
	for (const int32 Index : AddedIndices) if (Items.IsValidIndex(Index)) Owner->HandleReplicatedItem(Items[Index]);
}

void FEGQuestReplicatedFactArray::PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize)
{
	if (!Owner) return;
	for (const int32 Index : ChangedIndices) if (Items.IsValidIndex(Index)) Owner->HandleReplicatedItem(Items[Index]);
}

void FEGQuestReplicatedFactArray::PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize)
{
	if (!Owner) return;
	for (const int32 Index : RemovedIndices)
	{
		if (!Items.IsValidIndex(Index)) continue;
		FEGQuestReplicatedFactItem Removed = Items[Index];
		Removed.Value = 0;
		Owner->HandleReplicatedItem(Removed);
	}
}

AEGQuestFactsReplicationRelay::AEGQuestFactsReplicationRelay()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	bNetLoadOnClient = false;
	SetReplicateMovement(false);
	ReplicatedFacts.Owner = this;
}

void AEGQuestFactsReplicationRelay::BeginPlay()
{
	Super::BeginPlay();
	ReplicatedFacts.Owner = this;
	if (!HasAuthority())
	{
		for (const FEGQuestReplicatedFactItem& Item : ReplicatedFacts.Items) HandleReplicatedItem(Item);
	}
}

void AEGQuestFactsReplicationRelay::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AEGQuestFactsReplicationRelay, ReplicatedFacts);
}

void AEGQuestFactsReplicationRelay::SetReplicatedFact(FGameplayTag Tag, int32 Value,
	EEGQuestFactScope Scope, const FString& PlayerId)
{
	if (!HasAuthority() || !Tag.IsValid()) return;
	FEGQuestReplicatedFactItem* Item = ReplicatedFacts.Items.FindByPredicate(
		[Tag, Scope, &PlayerId](const FEGQuestReplicatedFactItem& Candidate)
		{
			return Candidate.Tag == Tag && Candidate.Scope == Scope && Candidate.PlayerId == PlayerId;
		});
	if (!Item)
	{
		Item = &ReplicatedFacts.Items.AddDefaulted_GetRef();
		Item->Tag = Tag;
		Item->Scope = Scope;
		Item->PlayerId = PlayerId;
	}
	Item->Value = Value;
	ReplicatedFacts.MarkItemDirty(*Item);
}

void AEGQuestFactsReplicationRelay::HandleReplicatedItem(const FEGQuestReplicatedFactItem& Item)
{
	if (HasAuthority() || !GetWorld()) return;
	if (UEGQuestFactsSubsystem* Facts = GetWorld()->GetSubsystem<UEGQuestFactsSubsystem>())
	{
		Facts->ApplyReplicatedFact(Item.Tag, Item.Value, Item.Scope, Item.PlayerId);
	}
}
