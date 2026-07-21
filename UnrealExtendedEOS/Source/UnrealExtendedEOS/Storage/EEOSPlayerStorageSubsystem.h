// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EEOSSubsystem.h"
#include "EEOSPlayerStorageSubsystem.generated.h"

class USaveGame;
class IOnlineUserCloud;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSPlayerDataWritten, bool, bSuccess, const FString&, FileName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEOSPlayerDataRead, bool, bSuccess, const FString&, FileName, const TArray<uint8>&, Data);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSPlayerFilesQueried, const TArray<FString>&, FileNames);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEOSSaveGameRead, bool, bSuccess, const FString&, FileName, USaveGame*, SaveGame);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSPlayerDataDeleted, bool, bSuccess, const FString&, FileName);

/**
 * Manages per-player cloud storage through EOS Player Data Storage.
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSPlayerStorageSubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────
	// All async actions return true when the request was started (the result arrives on
	// the corresponding delegate) and false when it could not be. Pre-flight failures
	// broadcast a failure; a duplicate request for a file whose same-kind operation is
	// already in flight is rejected with a log ONLY — no broadcast, because the rejection
	// would carry the same file name as the in-flight operation and be indistinguishable
	// from its real completion (e.g. a duplicate raw read must not fail an in-flight
	// ReadSaveGame of the same file).

	/** Write data to a player-scoped cloud file */
	UFUNCTION(BlueprintCallable, Category = "EOS|Storage")
	bool WritePlayerData(const FString& FileName, const TArray<uint8>& Data);

	/** Write a string to a player-scoped cloud file */
	UFUNCTION(BlueprintCallable, Category = "EOS|Storage")
	bool WritePlayerString(const FString& FileName, const FString& Content);

	/** Write a USaveGame object to cloud (serializes automatically) */
	UFUNCTION(BlueprintCallable, Category = "EOS|Storage")
	bool WriteSaveGame(const FString& FileName, USaveGame* SaveGameObject);

	/** Read data from a player-scoped cloud file */
	UFUNCTION(BlueprintCallable, Category = "EOS|Storage")
	bool ReadPlayerData(const FString& FileName);

	/** Read a cloud file and deserialize as USaveGame (fires OnSaveGameRead; the underlying
	 *  byte transfer also fires OnPlayerDataRead) */
	UFUNCTION(BlueprintCallable, Category = "EOS|Storage")
	bool ReadSaveGame(const FString& FileName);

	/** Delete a player-scoped cloud file (fires OnPlayerDataDeleted) */
	UFUNCTION(BlueprintCallable, Category = "EOS|Storage")
	bool DeletePlayerData(const FString& FileName);

	/**
	 * Query the list of available player files. Returns true when an enumeration is
	 * running — a call while one is already in flight coalesces onto it (its completion
	 * broadcasts to all listeners) rather than being rejected.
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|Storage")
	bool QueryPlayerFiles();

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Get the cached file list from the last query */
	UFUNCTION(BlueprintPure, Category = "EOS|Storage")
	TArray<FString> GetPlayerFileList() const;

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "EOS|Storage")
	FOnEOSPlayerDataWritten OnPlayerDataWritten;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Storage")
	FOnEOSPlayerDataRead OnPlayerDataRead;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Storage")
	FOnEOSPlayerFilesQueried OnPlayerFilesQueried;

	/** Fired when ReadSaveGame completes (provides deserialized USaveGame) */
	UPROPERTY(BlueprintAssignable, Category = "EOS|Storage")
	FOnEOSSaveGameRead OnSaveGameRead;

	/** Fired when DeletePlayerData completes (success or failure) */
	UPROPERTY(BlueprintAssignable, Category = "EOS|Storage")
	FOnEOSPlayerDataDeleted OnPlayerDataDeleted;

private:

	TArray<FString> CachedFileList;

	/** Interface-wide cloud delegates are bound once (lazily) and stay bound until Deinitialize */
	bool bCloudDelegatesBound = false;

	/** File names with an operation currently in flight — completions are filtered against these */
	TSet<FString> PendingWriteFiles;
	TSet<FString> PendingReadFiles;
	TSet<FString> PendingDeleteFiles;
	TSet<FString> PendingSaveGameReads;

	/** Whether an EnumerateUserFiles request is in flight */
	bool bEnumerateInFlight = false;

	/** Bind the four interface-wide cloud completion delegates exactly once */
	void EnsureCloudDelegatesBound(IOnlineUserCloud& CloudInterface);

	void HandleWriteUserFileComplete(bool bWasSuccessful, const FUniqueNetId& UserId, const FString& FileName);
	void HandleReadUserFileComplete(bool bWasSuccessful, const FUniqueNetId& UserId, const FString& FileName);
	void HandleEnumerateUserFilesComplete(bool bWasSuccessful, const FUniqueNetId& UserId);
	void HandleDeleteUserFileComplete(bool bWasSuccessful, const FUniqueNetId& UserId, const FString& FileName);

	UFUNCTION()
	void HandleSaveGameDataRead(bool bSuccess, const FString& FileName, const TArray<uint8>& Data);
};
