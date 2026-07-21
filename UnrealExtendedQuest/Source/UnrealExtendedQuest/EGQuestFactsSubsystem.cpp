// Copyright Devil of the Plague. All Rights Reserved.
#include "EGQuestFactsSubsystem.h"

#include "EGQuestFactsReplicationRelay.h"
#include "EGQuestPluginSettings.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"

void UEGQuestFactsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEGQuestFactsSubsystem::Deinitialize()
{
	ReplicationRelay = nullptr;
	WorldFacts.Reset();
	PlayerFacts.Reset();
	Super::Deinitialize();
}

void UEGQuestFactsSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	EnsureReplicationRelay();
}

bool UEGQuestFactsSubsystem::HasAuthority() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_Client;
}

FString UEGQuestFactsSubsystem::GetPlayerScopeId(const APlayerState* Player)
{
	if (!Player) return FString();
	const FUniqueNetIdRepl& UniqueId = Player->GetUniqueId();
	if (UniqueId.IsValid()) return UniqueId.ToString();
	if (!Player->GetPlayerName().IsEmpty()) return FString::Printf(TEXT("Name:%s"), *Player->GetPlayerName());
	return FString::Printf(TEXT("PlayerId:%d"), Player->GetPlayerId());
}

int32 UEGQuestFactsSubsystem::GetFact(FGameplayTag Tag, EEGQuestFactScope Scope, APlayerState* Player) const
{
	if (!Tag.IsValid()) return 0;
	if (Scope == EEGQuestFactScope::World)
	{
		const int32* Value = WorldFacts.Find(Tag);
		return Value ? *Value : 0;
	}
	const FString PlayerId = GetPlayerScopeId(Player);
	const FEGQuestPlayerFactStore* Store = PlayerFacts.Find(PlayerId);
	const int32* Value = Store ? Store->Values.Find(Tag) : nullptr;
	return Value ? *Value : 0;
}

bool UEGQuestFactsSubsystem::HasFact(FGameplayTag Tag, EEGQuestFactScope Scope, APlayerState* Player) const
{
	return GetFact(Tag, Scope, Player) != 0;
}

bool UEGQuestFactsSubsystem::SetFact(FGameplayTag Tag, int32 Value, EEGQuestFactScope Scope,
	APlayerState* Player, AActor* Instigator)
{
	return SetFactInternal(Tag, Value, Scope, Player, FString(), Instigator, false);
}

bool UEGQuestFactsSubsystem::AddToFact(FGameplayTag Tag, int32 Delta, EEGQuestFactScope Scope,
	APlayerState* Player, AActor* Instigator)
{
	if (!HasAuthority() || Delta == 0) return false;
	const int64 Sum = static_cast<int64>(GetFact(Tag, Scope, Player)) + static_cast<int64>(Delta);
	return SetFact(Tag, static_cast<int32>(FMath::Clamp<int64>(Sum, MIN_int32, MAX_int32)), Scope, Player, Instigator);
}

bool UEGQuestFactsSubsystem::ClearFact(FGameplayTag Tag, EEGQuestFactScope Scope,
	APlayerState* Player, AActor* Instigator)
{
	return SetFact(Tag, 0, Scope, Player, Instigator);
}

bool UEGQuestFactsSubsystem::SetFactInternal(FGameplayTag Tag, int32 Value, EEGQuestFactScope Scope,
	APlayerState* Player, const FString& ExplicitPlayerId, AActor* Instigator, bool bFromReplication)
{
	if (!Tag.IsValid() || (!bFromReplication && !HasAuthority())) return false;
	const FString PlayerId = Scope == EEGQuestFactScope::Player
		? (!ExplicitPlayerId.IsEmpty() ? ExplicitPlayerId : GetPlayerScopeId(Player))
		: FString();
	if (Scope == EEGQuestFactScope::Player && PlayerId.IsEmpty()) return false;

	TMap<FGameplayTag, int32>* Store = &WorldFacts;
	if (Scope == EEGQuestFactScope::Player)
	{
		Store = &PlayerFacts.FindOrAdd(PlayerId).Values;
	}
	const int32 OldValue = Store->FindRef(Tag);
	if (OldValue == Value) return false;
	if (Value == 0)
	{
		Store->Remove(Tag);
		if (Scope == EEGQuestFactScope::Player && Store->Num() == 0) PlayerFacts.Remove(PlayerId);
	}
	else
	{
		Store->Add(Tag, Value);
	}

	APlayerState* BroadcastPlayer = Player;
	if (Scope == EEGQuestFactScope::Player && !BroadcastPlayer && !PlayerId.IsEmpty())
	{
		if (const UWorld* World = GetWorld())
		{
			if (const AGameStateBase* GameState = World->GetGameState())
			{
				for (APlayerState* Candidate : GameState->PlayerArray)
				{
					if (Candidate && GetPlayerScopeId(Candidate) == PlayerId)
					{
						BroadcastPlayer = Candidate;
						break;
					}
				}
			}
		}
	}
	OnFactChanged.Broadcast(Tag, OldValue, Value, Scope, BroadcastPlayer, Instigator);
	if (!bFromReplication && ShouldReplicateFact(Tag))
	{
		PushFactToRelay(Tag, Value, Scope, PlayerId);
	}
	return true;
}

FEGQuestFactsSaveData UEGQuestFactsSubsystem::ExtractFactsSaveData() const
{
	FEGQuestFactsSaveData SaveData;
	if (!HasAuthority()) return SaveData;
	for (const TPair<FGameplayTag, int32>& Pair : WorldFacts)
	{
		FEGQuestFactSaveEntry& Entry = SaveData.Entries.AddDefaulted_GetRef();
		Entry.Tag = Pair.Key;
		Entry.Value = Pair.Value;
		Entry.Scope = EEGQuestFactScope::World;
	}
	for (const TPair<FString, FEGQuestPlayerFactStore>& PlayerPair : PlayerFacts)
	{
		for (const TPair<FGameplayTag, int32>& FactPair : PlayerPair.Value.Values)
		{
			FEGQuestFactSaveEntry& Entry = SaveData.Entries.AddDefaulted_GetRef();
			Entry.Tag = FactPair.Key;
			Entry.Value = FactPair.Value;
			Entry.Scope = EEGQuestFactScope::Player;
			Entry.PlayerId = PlayerPair.Key;
		}
	}
	SaveData.Entries.Sort([](const FEGQuestFactSaveEntry& A, const FEGQuestFactSaveEntry& B)
	{
		if (A.Scope != B.Scope) return static_cast<uint8>(A.Scope) < static_cast<uint8>(B.Scope);
		if (A.PlayerId != B.PlayerId) return A.PlayerId < B.PlayerId;
		return A.Tag.ToString() < B.Tag.ToString();
	});
	return SaveData;
}

bool UEGQuestFactsSubsystem::RestoreFactsSaveData(const FEGQuestFactsSaveData& SaveData, bool bReplaceExisting)
{
	if (!HasAuthority() || SaveData.SchemaVersion != 1) return false;
	if (bReplaceExisting)
	{
		for (const TPair<FGameplayTag, int32>& Pair : WorldFacts)
		{
			if (ShouldReplicateFact(Pair.Key)) PushFactToRelay(Pair.Key, 0, EEGQuestFactScope::World, FString());
		}
		for (const TPair<FString, FEGQuestPlayerFactStore>& PlayerPair : PlayerFacts)
		{
			for (const TPair<FGameplayTag, int32>& Pair : PlayerPair.Value.Values)
			{
				if (ShouldReplicateFact(Pair.Key)) PushFactToRelay(Pair.Key, 0, EEGQuestFactScope::Player, PlayerPair.Key);
			}
		}
		WorldFacts.Reset();
		PlayerFacts.Reset();
	}
	for (const FEGQuestFactSaveEntry& Entry : SaveData.Entries)
	{
		if (!Entry.Tag.IsValid() || Entry.Value == 0) continue;
		SetFactInternal(Entry.Tag, Entry.Value, Entry.Scope, nullptr, Entry.PlayerId, nullptr, false);
	}
	return true;
}

bool UEGQuestFactsSubsystem::ShouldReplicateFact(FGameplayTag Tag) const
{
	if (!Tag.IsValid()) return false;
	const UEGQuestPluginSettings* Settings = GetDefault<UEGQuestPluginSettings>();
	for (const FGameplayTag& Root : Settings->ReplicatedFactRootTags)
	{
		if (Tag.MatchesTag(Root)) return true;
	}
	return false;
}

void UEGQuestFactsSubsystem::ApplyReplicatedFact(FGameplayTag Tag, int32 Value,
	EEGQuestFactScope Scope, const FString& PlayerId)
{
	if (HasAuthority()) return;
	SetFactInternal(Tag, Value, Scope, nullptr, PlayerId, nullptr, true);
}

void UEGQuestFactsSubsystem::EnsureReplicationRelay()
{
	if (!HasAuthority() || ReplicationRelay || !GetWorld()) return;
	FActorSpawnParameters Params;
	Params.Name = TEXT("EGQuestFactsReplicationRelay");
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	ReplicationRelay = GetWorld()->SpawnActor<AEGQuestFactsReplicationRelay>(Params);
}

void UEGQuestFactsSubsystem::PushFactToRelay(FGameplayTag Tag, int32 Value,
	EEGQuestFactScope Scope, const FString& PlayerId)
{
	EnsureReplicationRelay();
	if (ReplicationRelay) ReplicationRelay->SetReplicatedFact(Tag, Value, Scope, PlayerId);
}
