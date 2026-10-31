// Copyright Moon Punch Games. All Rights Reserved.

// EFSubtitleSubsystem.h — Authority subtitle subsystem (GameInstanceSubsystem)
#pragma once

#include "CoreMinimal.h"
#include "Engine/StreamableManager.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/ObjectKey.h"
#include "UnrealExtendedFramework/Systems/Subtitle/Data/EFSubtitleData.h"
#include "EFSubtitleSubsystem.generated.h"

class UEFSubtitleDataAsset;
class UEFSubtitleReceiverComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSubtitleExecuted, const FEFSubtitleEntry&, Entry, int32, RequestId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSubtitleSequenceFinished, int32, SequenceId);

DECLARE_LOG_CATEGORY_EXTERN(LogEFSubtitle, Log, All);

/**
 * Authority-side subtitle subsystem. Resolves subtitle data, filters by distance,
 * and dispatches to relevant clients via RPC (or directly in standalone).
 *
 * Simple usage:  SubtitleSubsystem->ExecuteSubtitle(KEY);
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFSubtitleSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }

	// ── One-Liner API ──

	// Play subtitle by key — searches all registered data sources
	UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject"), Category = "Subtitle")
	void ExecuteSubtitle(const UObject* WorldContextObject, FName SubtitleKey);

	// Play subtitle at a world location (3D audio + distance filtering)
	UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject"), Category = "Subtitle")
	void ExecuteSubtitleAtLocation(const UObject* WorldContextObject,
		FName SubtitleKey, FVector Location, float MaxDistance = 3000.0f);

	// Play subtitle attached to an actor
	UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject"), Category = "Subtitle")
	void ExecuteSubtitleAttached(const UObject* WorldContextObject,
		FName SubtitleKey, AActor* AttachActor, float MaxDistance = 3000.0f);

	// Play subtitle only for a specific player
	UFUNCTION(BlueprintCallable, Category = "Subtitle")
	void ExecuteSubtitleForPlayer(APlayerController* Player, FName SubtitleKey);

	// ── Advanced API ──

	// Full control over the request
	UFUNCTION(BlueprintCallable, Category = "Subtitle")
	int32 RequestSubtitle(const FEFSubtitleRequest& Request);

	// Cancel an active/queued subtitle by ID
	UFUNCTION(BlueprintCallable, Category = "Subtitle")
	void CancelSubtitle(int32 RequestId);

	// Execute a sequence of subtitles
	UFUNCTION(BlueprintCallable, Category = "Subtitle")
	int32 ExecuteSubtitleSequence(const FEFSubtitleSequence& Sequence);

	// Cancel a subtitle sequence
	UFUNCTION(BlueprintCallable, Category = "Subtitle")
	void CancelSubtitleSequence(int32 SequenceId);

	// ── Data Source Management ──

	UFUNCTION(BlueprintCallable, Category = "Subtitle|Data")
	void RegisterSubtitleSource(UEFSubtitleDataAsset* DataAsset);

	UFUNCTION(BlueprintCallable, Category = "Subtitle|Data")
	void UnregisterSubtitleSource(UEFSubtitleDataAsset* DataAsset);

	UFUNCTION(BlueprintCallable, Category = "Subtitle|Data")
	void SetActiveDataTable(UDataTable* DataTable);

	// ── Delegates ──

	UPROPERTY(BlueprintAssignable, Category = "Subtitle")
	FOnSubtitleExecuted OnSubtitleExecuted;

	UPROPERTY(BlueprintAssignable, Category = "Subtitle")
	FOnSubtitleSequenceFinished OnSubtitleSequenceFinished;

	// ── DEPRECATED (backward compatibility) ──

	UE_DEPRECATED(5.0, "Use ExecuteSubtitle instead.")
	UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject", DeprecatedFunction,
		DeprecationMessage="Use ExecuteSubtitle instead"), Category = "Subtitle")
	void ExecuteExtendedSubtitle(const UObject* WorldContextObject, const FString SubtitleKey);

private:
	// ── Data Resolution ──

	// Resolve a subtitle key across all data sources. Registers the default data assets first
	// when their background load has not finished yet.
	bool ResolveSubtitleEntry(FName SubtitleKey, FEFSubtitleEntry& OutEntry);

	// ── Preloading ──

	// Registers the project's default data assets, ahead of any source registered at runtime.
	// Runs once: when their background load finishes, or earlier (loading them) if they are needed first.
	void RegisterDefaultDataAssets();

	// Starts loading the voice sounds of a data source that just became active (bPreloadVoiceSounds).
	void PreloadDataAssetVoices(const UEFSubtitleDataAsset* DataAsset);
	void PreloadActiveDataTableVoices();

	// ── Dispatch ──

	// Send subtitle to all relevant clients
	void DispatchToClients(const FEFSubtitleEntry& Entry, const FEFSubtitleRequest& Request);

	// Send subtitle to a specific client
	void DispatchToClient(APlayerController* PC, const FEFSubtitleEntry& Entry, const FEFSubtitleRequest& Request);

	// Get or add receiver component on a PlayerController
	UEFSubtitleReceiverComponent* GetOrAddReceiverComponent(APlayerController* PC);

	// ── Data Sources ──

	UPROPERTY()
	TArray<UEFSubtitleDataAsset*> RegisteredDataAssets;

	UPROPERTY()
	TSoftObjectPtr<UDataTable> ActiveDataTable;

	// Default data assets and data table from project settings, loading in the background.
	TSharedPtr<FStreamableHandle> DefaultSourcesHandle;

	// Style profiles from project settings, for the style setting and the display widget.
	TSharedPtr<FStreamableHandle> StyleProfilesHandle;

	// Voice sounds of each registered data asset and of the active data table.
	TMap<FObjectKey, TSharedPtr<FStreamableHandle>> DataAssetVoiceHandles;
	TSharedPtr<FStreamableHandle> DataTableVoiceHandle;

	bool bDefaultDataAssetsRegistered = false;

	// ── Request ID tracking ──
	int32 NextRequestId = 1;

	// ── Sequence tracking ──
	struct FActiveSequence
	{
		FEFSubtitleSequence Sequence;
		int32 SequenceId;
		int32 CurrentIndex;
		FTimerHandle TimerHandle;
	};

	TArray<FActiveSequence> ActiveSequences;
	int32 NextSequenceId = 1;
	
	void AdvanceSequence(int32 SequenceId);
};
