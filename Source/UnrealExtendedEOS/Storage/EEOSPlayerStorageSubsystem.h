// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EEOSSubsystem.h"
#include "EEOSPlayerStorageSubsystem.generated.h"

class USaveGame;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSPlayerDataWritten, bool, bSuccess, const FString&, FileName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEOSPlayerDataRead, bool, bSuccess, const FString&, FileName, const TArray<uint8>&, Data);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSPlayerFilesQueried, const TArray<FString>&, FileNames);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEOSSaveGameRead, bool, bSuccess, const FString&, FileName, USaveGame*, SaveGame);

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

	/** Write data to a player-scoped cloud file */
	UFUNCTION(BlueprintCallable, Category = "EOS|Storage")
	void WritePlayerData(const FString& FileName, const TArray<uint8>& Data);

	/** Write a string to a player-scoped cloud file */
	UFUNCTION(BlueprintCallable, Category = "EOS|Storage")
	void WritePlayerString(const FString& FileName, const FString& Content);

	/** Write a USaveGame object to cloud (serializes automatically) */
	UFUNCTION(BlueprintCallable, Category = "EOS|Storage")
	void WriteSaveGame(const FString& FileName, USaveGame* SaveGameObject);

	/** Read data from a player-scoped cloud file */
	UFUNCTION(BlueprintCallable, Category = "EOS|Storage")
	void ReadPlayerData(const FString& FileName);

	/** Read a cloud file and deserialize as USaveGame (fires OnSaveGameRead) */
	UFUNCTION(BlueprintCallable, Category = "EOS|Storage")
	void ReadSaveGame(const FString& FileName);

	/** Delete a player-scoped cloud file */
	UFUNCTION(BlueprintCallable, Category = "EOS|Storage")
	void DeletePlayerData(const FString& FileName);

	/** Query the list of available player files */
	UFUNCTION(BlueprintCallable, Category = "EOS|Storage")
	void QueryPlayerFiles();

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

private:

	TArray<FString> CachedFileList;
	bool bPendingSaveGameRead = false;
	FString PendingSaveGameFileName;

	void HandleWriteUserFileComplete(bool bWasSuccessful, const FUniqueNetId& UserId, const FString& FileName);
	void HandleReadUserFileComplete(bool bWasSuccessful, const FUniqueNetId& UserId, const FString& FileName);
	void HandleEnumerateUserFilesComplete(bool bWasSuccessful, const FUniqueNetId& UserId);
	void HandleDeleteUserFileComplete(bool bWasSuccessful, const FUniqueNetId& UserId, const FString& FileName);

	UFUNCTION()
	void HandleSaveGameDataRead(bool bSuccess, const FString& FileName, const TArray<uint8>& Data);
};
