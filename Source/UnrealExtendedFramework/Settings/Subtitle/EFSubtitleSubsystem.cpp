// EFSubtitleSubsystem.cpp

#include "EFSubtitleSubsystem.h"
#include "UnrealExtendedFramework/Settings/Subtitle/Data/EFSubtitleProjectSettings.h"
#include "UnrealExtendedFramework/Settings/Subtitle/Data/EFSubtitleDataAsset.h"
#include "UnrealExtendedFramework/Settings/Subtitle/Subsystem/EFSubtitleReceiverComponent.h"
#include "UnrealExtendedFramework/Settings/Subtitle/Subsystem/EFSubtitleLocalSubsystem.h"
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

	// Load default data sources from project settings
	const auto* Settings = GetDefault<UEFSubtitleProjectSettings>();
	if (Settings)
	{
		ActiveDataTable = Settings->DefaultDataTable;

		for (const auto& AssetPtr : Settings->DefaultDataAssets)
		{
			if (UEFSubtitleDataAsset* Asset = AssetPtr.LoadSynchronous())
			{
				RegisteredDataAssets.AddUnique(Asset);
			}
		}
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
		UE_LOG(LogEFSubtitle, Log, TEXT("Registered subtitle data asset: %s"), *DataAsset->GetName());
	}
}


void UEFSubtitleSubsystem::UnregisterSubtitleSource(UEFSubtitleDataAsset* DataAsset)
{
	RegisteredDataAssets.Remove(DataAsset);
}


void UEFSubtitleSubsystem::SetActiveDataTable(UDataTable* DataTable)
{
	ActiveDataTable = DataTable;
}


// ── Data Resolution ──

bool UEFSubtitleSubsystem::ResolveSubtitleEntry(FName SubtitleKey, FEFSubtitleEntry& OutEntry) const
{
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

	// 2. Search active DataTable
	if (ActiveDataTable.IsValid())
	{
		if (UDataTable* DT = ActiveDataTable.LoadSynchronous())
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
