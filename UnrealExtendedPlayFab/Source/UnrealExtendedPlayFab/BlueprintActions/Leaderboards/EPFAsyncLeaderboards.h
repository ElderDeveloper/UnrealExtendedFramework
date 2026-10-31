// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActions/EPFAsyncTypes.h"
#include "Shared/EPFTypes.h"
#include "EPFAsyncLeaderboards.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAsyncLeaderboardReceived, const TArray<FEPFLeaderboardEntry>&, Entries);

// ── Global ───────────────────────────────────────────────────────────────────

UCLASS(meta = (DisplayName = "PlayFab: Get Leaderboard"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncGetLeaderboard : public UEPFAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Get Leaderboard"), Category = "PlayFab|Async|Leaderboards")
	static UEPFAsyncGetLeaderboard* GetLeaderboard(UObject* WorldContext, const FString& StatisticName, int32 StartPosition = 0, int32 MaxResultsCount = 10);

	UPROPERTY(BlueprintAssignable) FOnEPFAsyncLeaderboardReceived OnSuccess;

protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;

private:
	FString RequestedStatisticName;
	int32 RequestedStartPosition = 0;
	int32 RequestedMaxResults = 10;
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const TArray<FEPFLeaderboardEntry>& Entries);
};

// ── Around Player ────────────────────────────────────────────────────────────

UCLASS(meta = (DisplayName = "PlayFab: Get Leaderboard Around Player"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncGetLeaderboardAroundPlayer : public UEPFAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Get Leaderboard Around Player"), Category = "PlayFab|Async|Leaderboards")
	static UEPFAsyncGetLeaderboardAroundPlayer* GetLeaderboardAroundPlayer(UObject* WorldContext, const FString& StatisticName, int32 MaxResultsCount = 10);

	UPROPERTY(BlueprintAssignable) FOnEPFAsyncLeaderboardReceived OnSuccess;

protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;

private:
	FString RequestedStatisticName;
	int32 RequestedMaxResults = 10;
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const TArray<FEPFLeaderboardEntry>& Entries);
};

// ── Friends ──────────────────────────────────────────────────────────────────

UCLASS(meta = (DisplayName = "PlayFab: Get Friends Leaderboard"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncGetFriendsLeaderboard : public UEPFAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Get Friends Leaderboard"), Category = "PlayFab|Async|Leaderboards")
	static UEPFAsyncGetFriendsLeaderboard* GetFriendsLeaderboard(UObject* WorldContext, const FString& StatisticName, int32 MaxResultsCount = 10);

	UPROPERTY(BlueprintAssignable) FOnEPFAsyncLeaderboardReceived OnSuccess;

protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;

private:
	FString RequestedStatisticName;
	int32 RequestedMaxResults = 10;
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const TArray<FEPFLeaderboardEntry>& Entries);
};
