// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Async/EFAsyncActionBase.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Platform/EFPlatformTypes.h"
#include "EFPlatformAsyncActions.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FEFPlatformSuccessPin);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEFPlatformAchievementsPin, const TArray<FEFAchievementState>&, Achievements);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEFPlatformStatsPin, const TArray<FEFStatValue>&, Stats);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEFPlatformLeaderboardPin, const TArray<FEFLeaderboardEntry>&, Entries);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEFPlatformCloudFilePin, const TArray<uint8>&, Data);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEFPlatformCloudFilesPin, const TArray<FEFCloudFileInfo>&, Files);

/** Base for platform nodes that report only success or failure. */
UCLASS(Abstract)
class EXTENDEDFOUNDATION_API UEFPlatformAsyncWriteBase : public UEFAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FEFPlatformSuccessPin OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FEFAsyncFailurePin OnFailure;

protected:
	virtual void BroadcastFailure(const FEFResult& Result) override { OnFailure.Broadcast(Result); }
	virtual FName GetResultSource() const override { return TEXT("Platform"); }

	/** Finishes the node with Result. */
	void FinishWithResult(const FEFResult& Result);

	/** A per-call callback that finishes this node. */
	TFunction<void(const FEFResult&)> MakeResultCallback();
};

UCLASS()
class EXTENDEDFOUNDATION_API UEFAsyncUnlockAchievement : public UEFPlatformAsyncWriteBase
{
	GENERATED_BODY()

public:
	/**
	 * Unlocks an achievement on the store that serves achievements. Written while no store can take
	 * it (not signed in yet), the unlock waits and the node finishes when it is sent. Timeout 0 waits
	 * as long as it takes.
	 */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Platform", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Unlock Achievement"))
	static UEFAsyncUnlockAchievement* UnlockAchievement(UObject* WorldContextObject, FName AchievementId, float TimeoutSeconds = 0.f);

protected:
	virtual void ExecuteAction() override;

private:
	FName RequestedAchievementId;
};

UCLASS()
class EXTENDEDFOUNDATION_API UEFAsyncSetAchievementProgress : public UEFPlatformAsyncWriteBase
{
	GENERATED_BODY()

public:
	/** Reports progress from 0 to 1; 1 unlocks. Stores without partial progress fail below 1 with NotAvailable. */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Platform", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Set Achievement Progress"))
	static UEFAsyncSetAchievementProgress* SetAchievementProgress(UObject* WorldContextObject, FName AchievementId, float Progress, float TimeoutSeconds = 0.f);

protected:
	virtual void ExecuteAction() override;

private:
	FName RequestedAchievementId;
	float RequestedProgress = 0.f;
};

UCLASS()
class EXTENDEDFOUNDATION_API UEFAsyncUpdateStat : public UEFPlatformAsyncWriteBase
{
	GENERATED_BODY()

public:
	/** Sets or increments a stat. Queued like achievements while no store can take it. */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Platform", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Update Stat"))
	static UEFAsyncUpdateStat* UpdateStat(UObject* WorldContextObject, FName StatId, int64 Value, EEFStatUpdateMode Mode = EEFStatUpdateMode::Set, float TimeoutSeconds = 0.f);

protected:
	virtual void ExecuteAction() override;

private:
	FName RequestedStatId;
	int64 RequestedValue = 0;
	EEFStatUpdateMode RequestedMode = EEFStatUpdateMode::Set;
};

UCLASS()
class EXTENDEDFOUNDATION_API UEFAsyncSubmitScore : public UEFPlatformAsyncWriteBase
{
	GENERATED_BODY()

public:
	/** Submits a score for the local user. Queued like achievements while no store can take it. */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Platform", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Submit Score"))
	static UEFAsyncSubmitScore* SubmitScore(UObject* WorldContextObject, FName LeaderboardId, int64 Score, float TimeoutSeconds = 0.f);

protected:
	virtual void ExecuteAction() override;

private:
	FName RequestedLeaderboardId;
	int64 RequestedScore = 0;
};

UCLASS()
class EXTENDEDFOUNDATION_API UEFAsyncWriteCloudFile : public UEFPlatformAsyncWriteBase
{
	GENERATED_BODY()

public:
	/** Writes a per-user cloud file. Fails at once with NotAvailable when no store serves cloud storage. */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Platform", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Write Cloud File"))
	static UEFAsyncWriteCloudFile* WriteCloudFile(UObject* WorldContextObject, const FString& FileName, const TArray<uint8>& Data, float TimeoutSeconds = 60.f);

protected:
	virtual void ExecuteAction() override;

private:
	FString RequestedFileName;
	TArray<uint8> RequestedData;
};

UCLASS()
class EXTENDEDFOUNDATION_API UEFAsyncDeleteCloudFile : public UEFPlatformAsyncWriteBase
{
	GENERATED_BODY()

public:
	/** Deletes a per-user cloud file. Succeeds when it did not exist. */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Platform", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Delete Cloud File"))
	static UEFAsyncDeleteCloudFile* DeleteCloudFile(UObject* WorldContextObject, const FString& FileName, float TimeoutSeconds = 30.f);

protected:
	virtual void ExecuteAction() override;

private:
	FString RequestedFileName;
};

/** Base for platform nodes that return data. */
UCLASS(Abstract)
class EXTENDEDFOUNDATION_API UEFPlatformAsyncQueryBase : public UEFAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FEFAsyncFailurePin OnFailure;

protected:
	virtual void BroadcastFailure(const FEFResult& Result) override { OnFailure.Broadcast(Result); }
	virtual FName GetResultSource() const override { return TEXT("Platform"); }
};

UCLASS()
class EXTENDEDFOUNDATION_API UEFAsyncQueryAchievements : public UEFPlatformAsyncQueryBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FEFPlatformAchievementsPin OnSuccess;

	/** Every achievement the store knows for the local user, with the game's ids. */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Platform", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Query Achievements"))
	static UEFAsyncQueryAchievements* QueryAchievements(UObject* WorldContextObject, float TimeoutSeconds = 30.f);

protected:
	virtual void ExecuteAction() override;
};

UCLASS()
class EXTENDEDFOUNDATION_API UEFAsyncQueryStats : public UEFPlatformAsyncQueryBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FEFPlatformStatsPin OnSuccess;

	/** Current values of the stats for the local user. */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Platform", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Query Stats"))
	static UEFAsyncQueryStats* QueryStats(UObject* WorldContextObject, const TArray<FName>& StatIds, float TimeoutSeconds = 30.f);

protected:
	virtual void ExecuteAction() override;

private:
	TArray<FName> RequestedStatIds;
};

UCLASS()
class EXTENDEDFOUNDATION_API UEFAsyncQueryLeaderboard : public UEFPlatformAsyncQueryBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FEFPlatformLeaderboardPin OnSuccess;

	/** Up to Count entries (1 to 100) in Range, best first. */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Platform", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Query Leaderboard"))
	static UEFAsyncQueryLeaderboard* QueryLeaderboard(UObject* WorldContextObject, FName LeaderboardId, EEFLeaderboardRange Range = EEFLeaderboardRange::Top, int32 Count = 10, float TimeoutSeconds = 30.f);

protected:
	virtual void ExecuteAction() override;

private:
	FName RequestedLeaderboardId;
	EEFLeaderboardRange RequestedRange = EEFLeaderboardRange::Top;
	int32 RequestedCount = 10;
};

UCLASS()
class EXTENDEDFOUNDATION_API UEFAsyncReadCloudFile : public UEFPlatformAsyncQueryBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FEFPlatformCloudFilePin OnSuccess;

	/** Reads a per-user cloud file. Fails with code NotFound when it does not exist. */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Platform", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Read Cloud File"))
	static UEFAsyncReadCloudFile* ReadCloudFile(UObject* WorldContextObject, const FString& FileName, float TimeoutSeconds = 60.f);

protected:
	virtual void ExecuteAction() override;

private:
	FString RequestedFileName;
};

UCLASS()
class EXTENDEDFOUNDATION_API UEFAsyncListCloudFiles : public UEFPlatformAsyncQueryBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FEFPlatformCloudFilesPin OnSuccess;

	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Platform", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "List Cloud Files"))
	static UEFAsyncListCloudFiles* ListCloudFiles(UObject* WorldContextObject, float TimeoutSeconds = 30.f);

protected:
	virtual void ExecuteAction() override;
};

UCLASS(meta = (ScriptName = "EFPlatformLibrary"))
class EXTENDEDFOUNDATION_API UEFPlatformLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** The signed-in user of the first provider that has one. */
	UFUNCTION(BlueprintPure, Category = "Extended Framework|Platform", meta = (WorldContext = "WorldContextObject"))
	static FEFPlatformUser GetPlatformUser(const UObject* WorldContextObject);

	/** The provider serving Feature now (Steam, EOS, PlayFab, Local), or None. */
	UFUNCTION(BlueprintPure, Category = "Extended Framework|Platform", meta = (WorldContext = "WorldContextObject"))
	static FName GetPlatformProviderName(const UObject* WorldContextObject, EEFPlatformFeature Feature);

	UFUNCTION(BlueprintPure, Category = "Extended Framework|Platform", meta = (WorldContext = "WorldContextObject"))
	static bool IsPlatformFeatureAvailable(const UObject* WorldContextObject, EEFPlatformFeature Feature);

	/** Sets the text friends see, plus provider-specific properties. Fire and forget. */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Platform", meta = (WorldContext = "WorldContextObject", AutoCreateRefTerm = "Properties"))
	static void SetPlatformPresence(const UObject* WorldContextObject, const FString& Status, const TMap<FString, FString>& Properties);

	/** Achievement, stat and score writes waiting for a store. */
	UFUNCTION(BlueprintPure, Category = "Extended Framework|Platform", meta = (WorldContext = "WorldContextObject"))
	static int32 GetQueuedPlatformWriteCount(const UObject* WorldContextObject);
};
