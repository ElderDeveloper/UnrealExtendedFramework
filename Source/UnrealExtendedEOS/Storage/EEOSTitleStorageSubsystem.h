// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EEOSSubsystem.h"
#include "EEOSTitleStorageSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEOSTitleFileRead, bool, bSuccess, const FString&, FileName, const TArray<uint8>&, Data);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSTitleFilesQueried, const TArray<FString>&, FileNames);

/**
 * Manages game-wide read-only cloud data through EOS Title Storage.
 * Title storage files are configured in the EOS DevPortal and are read-only at runtime.
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSTitleStorageSubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/** Read a title storage file by name */
	UFUNCTION(BlueprintCallable, Category = "EOS|TitleStorage")
	void ReadTitleFile(const FString& FileName);

	/** Read a title file and return contents as a string */
	UFUNCTION(BlueprintCallable, Category = "EOS|TitleStorage")
	void ReadTitleFileAsString(const FString& FileName);

	/** Query available title storage files */
	UFUNCTION(BlueprintCallable, Category = "EOS|TitleStorage")
	void QueryTitleFiles();

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Get the cached title file list */
	UFUNCTION(BlueprintPure, Category = "EOS|TitleStorage")
	TArray<FString> GetTitleFileList() const;

	/** Check if a title file exists in the cached list */
	UFUNCTION(BlueprintPure, Category = "EOS|TitleStorage")
	bool HasTitleFile(const FString& FileName) const;

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "EOS|TitleStorage")
	FOnEOSTitleFileRead OnTitleFileRead;

	UPROPERTY(BlueprintAssignable, Category = "EOS|TitleStorage")
	FOnEOSTitleFilesQueried OnTitleFilesQueried;

private:

	TArray<FString> CachedTitleFiles;

	void HandleEnumerateTitleFilesComplete(bool bWasSuccessful, const FString& Error);
	void HandleReadTitleFileComplete(bool bWasSuccessful, const FString& FileName);
};
