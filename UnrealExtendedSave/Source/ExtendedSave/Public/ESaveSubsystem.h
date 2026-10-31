// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "ESaveGameData.h"
#include "ESaveTypes.h"
#include "GameplayTagContainer.h"
#include "Result/EFResult.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ESaveSubsystem.generated.h"

class UESaveableComponent;
struct FStreamableHandle;
class UWorld;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FESaveSlotEvent, const FString&, SlotName, bool, bSuccess);

/**
 * Save games for one game instance.
 *
 *     UESaveSubsystem::Get(this)->SaveSlot(TEXT("Slot1"), [](const FEFResult& Result) { ... }, TEXT("Chapter 3"));
 *     UESaveSubsystem::Get(this)->LoadSlot(TEXT("Slot1"), EESaveLoadMode::OpenSavedLevel);
 *
 * What is saved:
 * - Actors with a UESaveableComponent: their SaveGame properties and their components', their
 *   transform, whether a placed actor was destroyed, and runtime-spawned actors (spawned again on load).
 * - Objects that implement IESaveable and are registered with RegisterSaveable.
 * - A free key/value store (SetValue / GetValue).
 * - Slot metadata for menus: display name, time, play time, level, thumbnail, extra fields.
 *
 * How it is stored:
 * - Through Unreal's save game system (Saved/SaveGames on desktop, the platform's save storage on
 *   consoles), asynchronously.
 * - With bKeepBackup, each save goes to the older of two files, so the last good save survives a
 *   crash mid-write; loading falls back to the other copy when one cannot be read.
 * - Each save carries the schema version; older saves are upgraded by UESaveMigration steps.
 * - Saves and loads run one at a time, in the order they were requested.
 *
 * Every call that takes a callback answers it exactly once, with an FEFResult (Source "Save").
 * Results are also broadcast on OnSlotSaved / OnSlotLoaded and as Extended.Save.* messages.
 */
UCLASS()
class EXTENDEDSAVE_API UESaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	using FOnSaved = TFunction<void(const FEFResult& Result)>;
	using FOnLoaded = TFunction<void(const FEFResult& Result, bool bRestoredFromBackup)>;
	using FOnCloudSynced = TFunction<void(const FEFResult& Result, bool bDownloaded)>;

	static UESaveSubsystem* Get(const UObject* WorldContextObject);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Slots

	/** Captures the world and writes it to SlotName. DisplayName is shown in slot lists; empty keeps the previous one. */
	void SaveSlot(const FString& SlotName, FOnSaved OnDone = nullptr, const FString& DisplayName = FString());

	/**
	 * Reads SlotName, upgrades it to the current schema version and makes it the active save. With
	 * ApplyToCurrentWorld the world is restored now; with OpenSavedLevel the saved level opens and
	 * restores itself as it loads (the callback runs before the level has opened).
	 */
	void LoadSlot(const FString& SlotName, EESaveLoadMode Mode = EESaveLoadMode::ApplyToCurrentWorld, FOnLoaded OnDone = nullptr);

	/** Deletes both copies of the slot. Returns false when there was nothing to delete. */
	UFUNCTION(BlueprintCallable, Category = "Extended Save")
	bool DeleteSlot(const FString& SlotName);

	/** Every slot, newest first, without loading them. */
	UFUNCTION(BlueprintPure, Category = "Extended Save")
	TArray<FESaveSlotInfo> GetSlots() const;

	UFUNCTION(BlueprintPure, Category = "Extended Save")
	bool DoesSlotExist(const FString& SlotName) const;

	/** Forgets the active save: empty records and values, play time 0. Live objects keep their current state. */
	UFUNCTION(BlueprintCallable, Category = "Extended Save")
	void StartNewGame();

	/** The slot last saved or loaded, or empty. */
	UFUNCTION(BlueprintPure, Category = "Extended Save")
	FString GetActiveSlotName() const { return ActiveSlotName; }

	/** Seconds played since a new game, including the play time of the loaded save. */
	UFUNCTION(BlueprintPure, Category = "Extended Save")
	double GetPlayTimeSeconds() const;

	/** True while a save or load is running. Requests made meanwhile wait their turn. */
	UFUNCTION(BlueprintPure, Category = "Extended Save")
	bool IsBusy() const { return bOperationRunning; }

	/** The state the next save writes: records, values and slot metadata. */
	UESaveGameData* GetActiveData() const { return ActiveData; }

	/** Slot names may hold letters, digits, spaces, '-' and '_' (not "__"), up to 64 characters. */
	static bool IsValidSlotName(const FString& SlotName);

	// Values

	UFUNCTION(BlueprintCallable, Category = "Extended Save|Values")
	void SetValue(FName Key, const FString& Value);

	UFUNCTION(BlueprintPure, Category = "Extended Save|Values")
	bool GetValue(FName Key, FString& OutValue) const;

	UFUNCTION(BlueprintCallable, Category = "Extended Save|Values")
	void RemoveValue(FName Key);

	// Slot metadata

	/** An image (PNG or JPEG bytes) stored with the next save, for slot lists. */
	UFUNCTION(BlueprintCallable, Category = "Extended Save")
	void SetNextSaveThumbnail(const TArray<uint8>& ImageBytes);

	/** An extra field stored in the slot metadata, e.g. Difficulty = Hard. An empty Value removes it. */
	UFUNCTION(BlueprintCallable, Category = "Extended Save")
	void SetSlotExtra(const FString& Key, const FString& Value);

	// Saveable objects

	/**
	 * Includes an object that implements IESaveable (and is not an actor with a UESaveableComponent)
	 * in saves. If the active save holds its state, it is restored at once.
	 */
	UFUNCTION(BlueprintCallable, Category = "Extended Save")
	void RegisterSaveable(UObject* Object);

	/** Stops saving the object. Its current state is kept in the active save. */
	UFUNCTION(BlueprintCallable, Category = "Extended Save")
	void UnregisterSaveable(UObject* Object);

	/** Copies the state of every live saveable into the active save without writing to disk. */
	UFUNCTION(BlueprintCallable, Category = "Extended Save")
	void CaptureWorldState();

	// Autosave

	/** Saves to the autosave slot now, unless autosave is blocked or a save is running. */
	UFUNCTION(BlueprintCallable, Category = "Extended Save|Autosave")
	void RequestAutosave();

	/** Blocks autosaves while any reason is set, e.g. Cutscene, Combat, MainMenu. */
	UFUNCTION(BlueprintCallable, Category = "Extended Save|Autosave")
	void SetAutosaveBlocked(FName Reason, bool bBlocked);

	UFUNCTION(BlueprintPure, Category = "Extended Save|Autosave")
	bool IsAutosaveBlocked() const { return AutosaveBlockers.Num() > 0; }

	// Cloud (through the platform layer's cloud storage)

	/** Uploads the slot's newest copy. */
	void UploadSlotToCloud(const FString& SlotName, FOnSaved OnDone = nullptr);

	/**
	 * Downloads the slot from the cloud and keeps it when it wins under CloudConflictPolicy;
	 * bDownloaded says whether the local slot was replaced. A slot missing from the cloud succeeds
	 * with bDownloaded false. Load the slot afterwards to use it.
	 */
	void SyncSlotFromCloud(const FString& SlotName, FOnCloudSynced OnDone = nullptr);

	UPROPERTY(BlueprintAssignable, Category = "Extended Save")
	FESaveSlotEvent OnSlotSaved;

	UPROPERTY(BlueprintAssignable, Category = "Extended Save")
	FESaveSlotEvent OnSlotLoaded;

	// Called by UESaveableComponent

	void RegisterComponent(UESaveableComponent* Component);
	void UnregisterComponent(UESaveableComponent* Component);
	/** Restores the component's actor from the active save; called at BeginPlay. */
	void ApplyToComponent(UESaveableComponent* Component);
	void NotifyComponentDestroyed(UESaveableComponent* Component);
	void NotifyComponentUnloaded(UESaveableComponent* Component);

	/** The key placed actors are saved under: their level's package, or the world's for World Partition. */
	static FString GetLevelKey(const AActor* Actor);

	/** The world's package name without the editor's PIE prefix. */
	static FString GetWorldKey(const UWorld* World);

	/** The file a copy (0 or 1) of a slot is written to. */
	static FString GetCopyFileName(const FString& SlotName, int32 Copy);

private:
	struct FOperation
	{
		FString Description;
		TFunction<void()> Run;
		/** Answers the operation's caller when it never got to run. */
		TFunction<void(const FEFResult&)> Cancel;
	};

	void RunOrQueue(FOperation&& Operation);
	void FinishOperation();

	void DoSave(const FString& SlotName, const FString& DisplayName, FOnSaved OnDone);
	void HandleSaveWritten(const FString& SlotName, int32 Copy, bool bSuccess, FOnSaved OnDone);
	void DoLoad(const FString& SlotName, EESaveLoadMode Mode, FOnLoaded OnDone);
	/** Reads Copies[0]. bPickNewest: read every copy and keep the newest. bFallback: Copies[0] is the backup of a copy that could not be read. */
	void LoadCopy(const FString& SlotName, EESaveLoadMode Mode, TArray<int32> Copies, bool bPickNewest, bool bFallback, FOnLoaded OnDone);
	void FinishLoad(const FString& SlotName, EESaveLoadMode Mode, UESaveGameData* Data, bool bRestoredFromBackup, FOnLoaded OnDone);
	void DoSyncFromCloud(const FString& SlotName, FOnCloudSynced OnDone);

	void LoadIndex();
	void WriteIndexAsync(TFunction<void(bool bSuccess)> OnDone);
	bool RunMigrations(UESaveGameData* Data, FEFResult& OutFailure) const;

	void CaptureComponent(UESaveableComponent* Component, FESaveRecord& OutRecord);
	void CaptureObject(UObject* Object, FESaveRecord& OutRecord);
	void ApplyRecordToComponent(UESaveableComponent* Component, const FESaveRecord& Record);
	void ApplyRecordToObject(UObject* Object, const FESaveRecord& Record);
	void ApplyToCurrentWorld();
	void RespawnMissingActors(UWorld* World);
	TArray<UESaveableComponent*> GetLiveComponents() const;

	void HandlePostLoadMap(UWorld* LoadedWorld);
	bool TickAutosave(float DeltaTime);
	void Broadcast(const FNativeGameplayTag& Channel, const FString& SlotName, bool bSuccess, bool bRestoredFromBackup);
	FString GetCloudFileName(const FString& SlotName) const;

	UPROPERTY(Transient)
	TObjectPtr<UESaveGameData> ActiveData;

	UPROPERTY(Transient)
	TObjectPtr<UESaveIndex> SlotIndex;

	/** The first copy read while a load decides between two copies. */
	UPROPERTY(Transient)
	TObjectPtr<UESaveGameData> LoadCandidate;

	TSet<TWeakObjectPtr<UESaveableComponent>> LiveComponents;
	TArray<TWeakObjectPtr<UObject>> LiveSaveables;
	TArray<FOperation> PendingOperations;
	TSet<FName> AutosaveBlockers;
	TArray<uint8> NextThumbnail;
	FString ActiveSlotName;
	double PlayTimeBaseSeconds = 0.0;
	double PlayTimeStartSeconds = 0.0;
	FTSTicker::FDelegateHandle AutosaveTickerHandle;
	FDelegateHandle PostLoadMapHandle;
	/** Keeps the configured migration classes loaded, so migrating an old save does not hitch. */
	TSharedPtr<FStreamableHandle> MigrationPreloadHandle;
	int32 StatusHandle = 0;
	bool bOperationRunning = false;
	/** Set while a load restores the world, so destroying actors it does not need is not recorded. */
	bool bApplying = false;
	/** Set from a load that opens a level until that level has loaded: the old level's actors must not overwrite the loaded state. */
	bool bIgnoreUnloads = false;
};
