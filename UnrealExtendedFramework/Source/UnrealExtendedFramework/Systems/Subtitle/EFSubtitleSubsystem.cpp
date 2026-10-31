// Copyright Moon Punch Games. All Rights Reserved.

// EFSubtitleSubsystem.cpp

#include "EFSubtitleSubsystem.h"
#include "Assets/EFAssetLoading.h"
#include "UnrealExtendedFramework/Systems/Subtitle/Data/EFSubtitleProjectSettings.h"
#include "UnrealExtendedFramework/Systems/Subtitle/Data/EFSubtitleDataAsset.h"
#include "UnrealExtendedFramework/Systems/Subtitle/Policy/EFSubtitleAudioPlayer.h"
#include "UnrealExtendedFramework/Systems/Subtitle/Subsystem/EFSubtitleReceiverComponent.h"
#include "UnrealExtendedFramework/Systems/Subtitle/Subsystem/EFSubtitleLocalSubsystem.h"
#include "UnrealExtendedFramework/Systems/Subtitle/UserSettings/EFSubtitleSettingsHelpers.h"
#include "Engine/DataTable.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY(LogEFSubtitle);


void UEFSubtitleSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Load default data sources from project settings, in the background. The data assets are
	// registered when they arrive, or at once if a subtitle is requested before that.
	const auto* Settings = GetDefault<UEFSubtitleProjectSettings>();
	if (Settings)
	{
		ActiveDataTable = Settings->DefaultDataTable;

		TArray<FSoftObjectPath> Paths;
		Paths.Reserve(Settings->DefaultDataAssets.Num() + 1);
		for (const auto& AssetPtr : Settings->DefaultDataAssets)
		{
			Paths.Add(AssetPtr.ToSoftObjectPath());
		}
		Paths.Add(Settings->DefaultDataTable.ToSoftObjectPath());

		TWeakObjectPtr<UEFSubtitleSubsystem> WeakSubsystem(this);
		DefaultSourcesHandle = EFAssets::Preload(MoveTemp(Paths), [WeakSubsystem]()
		{
			if (UEFSubtitleSubsystem* Subtitles = WeakSubsystem.Get())
			{
				Subtitles->RegisterDefaultDataAssets();
				Subtitles->PreloadActiveDataTableVoices();
			}
		});
	}

	// Style profiles feed the style setting's options and the display widget; nothing draws them on a dedicated server.
	if (!IsRunningDedicatedServer())
	{
		StyleProfilesHandle = EFSubtitleSettingsHelpers::PreloadStyleProfiles();
	}
}


void UEFSubtitleSubsystem::Deinitialize()
{
	// Clear all sequences
	for (auto& Seq : ActiveSequences)
	{
		if (GetWorld())
		{
			GetWorld()->GetTimerManager().ClearTimer(Seq.TimerHandle);
		}
	}
	ActiveSequences.Empty();
	RegisteredDataAssets.Empty();

	// Lets the preloaded data sources, voices and style profiles unload with the game instance.
	// The default sources are cancelled rather than released, so their completion callback
	// cannot register anything on a deinitialized subsystem.
	if (DefaultSourcesHandle.IsValid())
	{
		DefaultSourcesHandle->CancelHandle();
		DefaultSourcesHandle.Reset();
	}
	StyleProfilesHandle.Reset();
	DataAssetVoiceHandles.Reset();
	DataTableVoiceHandle.Reset();

	Super::Deinitialize();
}


// ── One-Liner API ──

void UEFSubtitleSubsystem::ExecuteSubtitle(const UObject* WorldContextObject, FName SubtitleKey)
{
	FEFSubtitleRequest Request;
	Request.SubtitleKey = SubtitleKey;
	Request.ExecutionType = EEFSubtitleExecutionType::Boundless;
	RequestSubtitle(Request);
}


void UEFSubtitleSubsystem::ExecuteSubtitleAtLocation(const UObject* WorldContextObject,
	FName SubtitleKey, FVector Location, float MaxDistance)
{
	FEFSubtitleRequest Request;
	Request.SubtitleKey = SubtitleKey;
	Request.ExecutionType = EEFSubtitleExecutionType::Location;
	Request.WorldLocation = Location;
	Request.MaxDistance = MaxDistance;
	RequestSubtitle(Request);
}


void UEFSubtitleSubsystem::ExecuteSubtitleAttached(const UObject* WorldContextObject,
	FName SubtitleKey, AActor* AttachActor, float MaxDistance)
{
	FEFSubtitleRequest Request;
	Request.SubtitleKey = SubtitleKey;
	Request.ExecutionType = EEFSubtitleExecutionType::AttachedToActor;
	Request.AttachActor = AttachActor;
	Request.MaxDistance = MaxDistance;
	if (AttachActor)
	{
		Request.WorldLocation = AttachActor->GetActorLocation();
	}
	RequestSubtitle(Request);
}


void UEFSubtitleSubsystem::ExecuteSubtitleForPlayer(APlayerController* Player, FName SubtitleKey)
{
	FEFSubtitleEntry Entry;
	if (!ResolveSubtitleEntry(SubtitleKey, Entry))
	{
		UE_LOG(LogEFSubtitle, Warning, TEXT("Subtitle key '%s' not found in any data source"), *SubtitleKey.ToString());
		return;
	}

	FEFSubtitleRequest Request;
	Request.SubtitleKey = SubtitleKey;
	Request.ExecutionType = EEFSubtitleExecutionType::PlayerOnly;
	Request.RequestId = NextRequestId++;

	DispatchToClient(Player, Entry, Request);
	OnSubtitleExecuted.Broadcast(Entry, Request.RequestId);
}


// ── Advanced API ──

int32 UEFSubtitleSubsystem::RequestSubtitle(const FEFSubtitleRequest& InRequest)
{
	FEFSubtitleEntry Entry;
	if (!ResolveSubtitleEntry(InRequest.SubtitleKey, Entry))
	{
		UE_LOG(LogEFSubtitle, Warning, TEXT("Subtitle key '%s' not found in any data source"), *InRequest.SubtitleKey.ToString());
		return 0;
	}

	FEFSubtitleRequest Request = InRequest;
	Request.RequestId = NextRequestId++;

	DispatchToClients(Entry, Request);
	OnSubtitleExecuted.Broadcast(Entry, Request.RequestId);

	return Request.RequestId;
}


void UEFSubtitleSubsystem::CancelSubtitle(int32 RequestId)
{
	// Broadcast cancel to all clients
	UWorld* World = GetWorld();
	if (!World) return;

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			if (PC->IsLocalController())
			{
				if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
				{
					if (UEFSubtitleLocalSubsystem* LocalSubsystem =
						LocalPlayer->GetSubsystem<UEFSubtitleLocalSubsystem>())
					{
						LocalSubsystem->CancelSubtitle(RequestId);
						continue;
					}
				}
			}

			if (UEFSubtitleReceiverComponent* Receiver = PC->FindComponentByClass<UEFSubtitleReceiverComponent>())
			{
				Receiver->ClientCancelSubtitle(RequestId);
			}
		}
	}
}


int32 UEFSubtitleSubsystem::ExecuteSubtitleSequence(const FEFSubtitleSequence& Sequence)
{
	if (Sequence.SubtitleKeys.Num() == 0)
	{
		return 0;
	}

	FActiveSequence NewSeq;
	NewSeq.Sequence = Sequence;
	NewSeq.SequenceId = NextSequenceId++;
	NewSeq.CurrentIndex = 0;

	ActiveSequences.Add(NewSeq);

	// Start the first subtitle
	AdvanceSequence(NewSeq.SequenceId);

	return NewSeq.SequenceId;
}


void UEFSubtitleSubsystem::CancelSubtitleSequence(int32 SequenceId)
{
	for (int32 i = ActiveSequences.Num() - 1; i >= 0; --i)
	{
		if (ActiveSequences[i].SequenceId == SequenceId)
		{
			if (GetWorld())
			{
				GetWorld()->GetTimerManager().ClearTimer(ActiveSequences[i].TimerHandle);
			}
			ActiveSequences.RemoveAt(i);
			return;
		}
	}
}


void UEFSubtitleSubsystem::AdvanceSequence(int32 SequenceId)
{
	FActiveSequence* Seq = nullptr;
	for (auto& S : ActiveSequences)
	{
		if (S.SequenceId == SequenceId)
		{
			Seq = &S;
			break;
		}
	}

	if (!Seq)
	{
		return;
	}

	if (Seq->CurrentIndex >= Seq->Sequence.SubtitleKeys.Num())
	{
		// Sequence complete
		OnSubtitleSequenceFinished.Broadcast(SequenceId);
		CancelSubtitleSequence(SequenceId);
		return;
	}

	// Execute current subtitle
	FName Key = Seq->Sequence.SubtitleKeys[Seq->CurrentIndex];
	FEFSubtitleEntry Entry;
	if (ResolveSubtitleEntry(Key, Entry))
	{
		FEFSubtitleRequest Request;
		Request.SubtitleKey = Key;
		Request.ExecutionType = Seq->Sequence.ExecutionType;
		Request.WorldLocation = Seq->Sequence.WorldLocation;
		Request.MaxDistance = Seq->Sequence.MaxDistance;
		Request.RequestId = NextRequestId++;

		DispatchToClients(Entry, Request);

		// Calculate delay for next subtitle
		float Delay = Entry.Duration;
		if (Delay <= 0.0f)
		{
			// Auto-calculate
			const auto* Settings = GetDefault<UEFSubtitleProjectSettings>();
			const FEFSubtitleDurationSettings& DurSettings = Settings ? Settings->DurationSettings : FEFSubtitleDurationSettings();
			Delay = (Entry.Text.ToString().Len() * DurSettings.TimePerLetter) + DurSettings.TimeAfterComplete;
			Delay = FMath::Max(Delay, 1.0f);
		}

		// Schedule next subtitle
		Seq->CurrentIndex++;
		if (GetWorld())
		{
			GetWorld()->GetTimerManager().SetTimer(
				Seq->TimerHandle,
				FTimerDelegate::CreateUObject(this, &UEFSubtitleSubsystem::AdvanceSequence, SequenceId),
				Delay,
				false
			);
		}
	}
	else
	{
		UE_LOG(LogEFSubtitle, Warning, TEXT("Sequence: subtitle key '%s' not found, skipping"), *Key.ToString());
		Seq->CurrentIndex++;
		AdvanceSequence(SequenceId);
	}
}


// ── Data Source Management ──

void UEFSubtitleSubsystem::RegisterSubtitleSource(UEFSubtitleDataAsset* DataAsset)
{
	if (DataAsset)
	{
		RegisteredDataAssets.AddUnique(DataAsset);
		PreloadDataAssetVoices(DataAsset);
		UE_LOG(LogEFSubtitle, Log, TEXT("Registered subtitle data asset: %s"), *DataAsset->GetName());
	}
}


void UEFSubtitleSubsystem::UnregisterSubtitleSource(UEFSubtitleDataAsset* DataAsset)
{
	// A default asset still loading would otherwise come back when its load finishes.
	RegisterDefaultDataAssets();

	RegisteredDataAssets.Remove(DataAsset);
	DataAssetVoiceHandles.Remove(FObjectKey(DataAsset));
}


void UEFSubtitleSubsystem::SetActiveDataTable(UDataTable* DataTable)
{
	ActiveDataTable = DataTable;
	PreloadActiveDataTableVoices();
}


// ── Preloading ──

void UEFSubtitleSubsystem::RegisterDefaultDataAssets()
{
	if (bDefaultDataAssetsRegistered)
	{
		return;
	}
	bDefaultDataAssetsRegistered = true;

	const auto* Settings = GetDefault<UEFSubtitleProjectSettings>();
	if (!Settings)
	{
		return;
	}

	TArray<UEFSubtitleDataAsset*> DefaultAssets;
	for (const auto& AssetPtr : Settings->DefaultDataAssets)
	{
		if (UEFSubtitleDataAsset* Asset = EFAssets::Resolve(AssetPtr, TEXT("UEFSubtitleSubsystem")))
		{
			DefaultAssets.AddUnique(Asset);
		}
	}

	// Searched first, as when they were registered in Initialize, even if the game registered its
	// own sources while these were still loading.
	for (UEFSubtitleDataAsset* Asset : DefaultAssets)
	{
		RegisteredDataAssets.Remove(Asset);
	}
	RegisteredDataAssets.Insert(DefaultAssets, 0);

	for (const UEFSubtitleDataAsset* Asset : DefaultAssets)
	{
		PreloadDataAssetVoices(Asset);
	}
}

void UEFSubtitleSubsystem::PreloadDataAssetVoices(const UEFSubtitleDataAsset* DataAsset)
{
	const auto* Settings = GetDefault<UEFSubtitleProjectSettings>();
	if (!DataAsset || !Settings || !Settings->bPreloadVoiceSounds || IsRunningDedicatedServer())
	{
		return;
	}

	TArray<FSoftObjectPath> Paths;
	for (const TPair<FName, FEFSubtitleEntry>& Pair : DataAsset->Entries)
	{
		UEFSubtitleAudioPlayer::GatherSubtitleSoundPaths(Pair.Value, Paths);
	}

	if (TSharedPtr<FStreamableHandle> VoiceHandle = EFAssets::Preload(MoveTemp(Paths)))
	{
		DataAssetVoiceHandles.Add(FObjectKey(DataAsset), VoiceHandle);
	}
}

void UEFSubtitleSubsystem::PreloadActiveDataTableVoices()
{
	DataTableVoiceHandle.Reset();

	const auto* Settings = GetDefault<UEFSubtitleProjectSettings>();
	const UDataTable* DataTable = ActiveDataTable.Get();
	if (!DataTable || !Settings || !Settings->bPreloadVoiceSounds || IsRunningDedicatedServer())
	{
		return;
	}

	const UScriptStruct* RowStruct = DataTable->GetRowStruct();
	if (!RowStruct || !RowStruct->IsChildOf(FEFSubtitleEntry::StaticStruct()))
	{
		return;
	}

	TArray<FSoftObjectPath> Paths;
	for (const TPair<FName, uint8*>& Row : DataTable->GetRowMap())
	{
		if (const FEFSubtitleEntry* Entry = reinterpret_cast<const FEFSubtitleEntry*>(Row.Value))
		{
			UEFSubtitleAudioPlayer::GatherSubtitleSoundPaths(*Entry, Paths);
		}
	}

	DataTableVoiceHandle = EFAssets::Preload(MoveTemp(Paths));
}


// ── Data Resolution ──

bool UEFSubtitleSubsystem::ResolveSubtitleEntry(FName SubtitleKey, FEFSubtitleEntry& OutEntry)
{
	// The defaults normally finished loading long before the first subtitle; if not, load them now.
	RegisterDefaultDataAssets();

	// 1. Search registered DataAssets first
	for (const auto* Asset : RegisteredDataAssets)
	{
		if (Asset)
		{
			if (const FEFSubtitleEntry* Found = Asset->FindEntry(SubtitleKey))
			{
				OutEntry = *Found;
				return true;
			}
		}
	}

	// 2. Search active DataTable (preloaded in Initialize; resolved here if that has not finished)
	if (!ActiveDataTable.IsNull())
	{
		if (UDataTable* DT = EFAssets::Resolve(ActiveDataTable, TEXT("UEFSubtitleSubsystem")))
		{
			if (FEFSubtitleEntry* Found = DT->FindRow<FEFSubtitleEntry>(SubtitleKey, TEXT("")))
			{
				OutEntry = *Found;
				return true;
			}
		}
	}

	// 3. Not found
	return false;
}


// ── Dispatch ──

void UEFSubtitleSubsystem::DispatchToClients(const FEFSubtitleEntry& Entry, const FEFSubtitleRequest& Request)
{
	UWorld* World = GetWorld();
	if (!World) return;

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) continue;

		// Distance filtering for spatial subtitles
		if (Request.MaxDistance > 0.0f &&
			(Request.ExecutionType == EEFSubtitleExecutionType::Location ||
			 Request.ExecutionType == EEFSubtitleExecutionType::AttachedToActor))
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				const float Distance = FVector::Dist(Pawn->GetActorLocation(), Request.WorldLocation);
				if (Distance > Request.MaxDistance)
				{
					continue; // Too far, skip this client
				}
			}
		}

		DispatchToClient(PC, Entry, Request);
	}
}


void UEFSubtitleSubsystem::DispatchToClient(APlayerController* PC, const FEFSubtitleEntry& Entry, const FEFSubtitleRequest& Request)
{
	if (!PC) return;

	// Check if this is a local player (standalone or listen server host)
	if (PC->IsLocalController())
	{
		// Direct call — no RPC needed
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (UEFSubtitleLocalSubsystem* LocalSub = LP->GetSubsystem<UEFSubtitleLocalSubsystem>())
			{
				LocalSub->ReceiveSubtitle(Entry, Request);
				return;
			}
		}
	}

	// Remote client — use RPC via receiver component
	UEFSubtitleReceiverComponent* Receiver = GetOrAddReceiverComponent(PC);
	if (Receiver)
	{
		Receiver->ClientReceiveSubtitle(Entry, Request);
	}
}


UEFSubtitleReceiverComponent* UEFSubtitleSubsystem::GetOrAddReceiverComponent(APlayerController* PC)
{
	if (!PC) return nullptr;

	UEFSubtitleReceiverComponent* Receiver = PC->FindComponentByClass<UEFSubtitleReceiverComponent>();
	if (!Receiver)
	{
		Receiver = NewObject<UEFSubtitleReceiverComponent>(PC, UEFSubtitleReceiverComponent::StaticClass());
		Receiver->RegisterComponent();
	}
	return Receiver;
}


// ── Deprecated ──

void UEFSubtitleSubsystem::ExecuteExtendedSubtitle(const UObject* WorldContextObject, const FString SubtitleKey)
{
	ExecuteSubtitle(WorldContextObject, FName(*SubtitleKey));
}
