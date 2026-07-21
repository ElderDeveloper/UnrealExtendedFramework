// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EEOSSubsystem.h"
#include "EEOSTitleStorageSubsystem.generated.h"

class IOnlineTitleFile;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEOSTitleFileRead, bool, bSuccess, const FString&, FileName, const TArray<uint8>&, Data);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEOSTitleFileReadAsString, bool, bSuccess, const FString&, FileName, const FString&, Content);
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
	// All async actions return true when the request was started (the result arrives on
	// the corresponding delegate) and false when it could not be. Pre-flight failures
	// broadcast a failure; a duplicate request for a file whose read is already in flight
	// is rejected with a log ONLY — no broadcast, so the in-flight read's waiters never
	// see a foreign failure carrying their own file name.

	/** Read a title storage file by name. Completion arrives on OnTitleFileRead. */
	UFUNCTION(BlueprintCallable, Category = "EOS|TitleStorage")
	bool ReadTitleFile(const FString& FileName);

	/**
	 * Read a title file and receive its contents as a string on OnTitleFileReadAsString.
	 * The payload is decoded as UTF-8 (a leading BOM is stripped).
	 *
	 * Dual-fire contract: a read started here ALWAYS fires both delegates — the byte-level
	 * OnTitleFileRead first, then OnTitleFileReadAsString — on success AND on failure
	 * (including pre-flight failures that never reach the SDK), so byte-level listeners see
	 * a symmetric event stream regardless of which entry point started the read. Reads
	 * started via ReadTitleFile fire only OnTitleFileRead.
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|TitleStorage")
	bool ReadTitleFileAsString(const FString& FileName);

	/**
	 * Query available title storage files. Returns true when an enumeration is running —
	 * a call while one is already in flight coalesces onto it (its completion broadcasts
	 * to all listeners) rather than being rejected.
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|TitleStorage")
	bool QueryTitleFiles();

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

	/** Fires for reads started via ReadTitleFileAsString, with the payload decoded as UTF-8 */
	UPROPERTY(BlueprintAssignable, Category = "EOS|TitleStorage")
	FOnEOSTitleFileReadAsString OnTitleFileReadAsString;

	UPROPERTY(BlueprintAssignable, Category = "EOS|TitleStorage")
	FOnEOSTitleFilesQueried OnTitleFilesQueried;

private:

	TArray<FString> CachedTitleFiles;

	/** Interface-wide title-file delegates are bound once (lazily) and stay bound until Deinitialize */
	bool bTitleFileDelegatesBound = false;

	/** File names with a read currently in flight — completions are filtered against this */
	TSet<FString> PendingReadFiles;

	/** Subset of PendingReadFiles that was requested via ReadTitleFileAsString — the shared
	 *  read handler additionally decodes and broadcasts OnTitleFileReadAsString for these */
	TSet<FString> PendingAsStringFiles;

	/** Whether an EnumerateFiles request is in flight */
	bool bEnumerateInFlight = false;

	/** Bind the interface-wide title-file completion delegates exactly once */
	void EnsureTitleFileDelegatesBound(IOnlineTitleFile& TitleFileInterface);

	void HandleEnumerateTitleFilesComplete(bool bWasSuccessful, const FString& Error);
	void HandleReadTitleFileComplete(bool bWasSuccessful, const FString& FileName);
};
