// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActions/EPFAsyncTypes.h"
#include "Characters/EPFCharacterSubsystem.h"
#include "EPFAsyncCharacters.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAsyncCharListSuccess, const TArray<FEPFCharacter>&, Characters);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAsyncCharGrantedSuccess, const FString&, CharacterId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEPFAsyncCharAction);

// ── Get Characters ───────────────────────────────────────────────────────────
UCLASS(meta = (DisplayName = "PlayFab: Get All Characters"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncGetCharacters : public UEPFAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Get All Characters"), Category = "PlayFab|Async|Characters")
	static UEPFAsyncGetCharacters* GetAllCharacters(UObject* WorldContext);
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncCharListSuccess OnSuccess;
protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;
private:
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const TArray<FEPFCharacter>& Characters);
};

// ── Grant Character ──────────────────────────────────────────────────────────
UCLASS(meta = (DisplayName = "PlayFab: Grant Character"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncGrantCharacter : public UEPFAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Grant Character"), Category = "PlayFab|Async|Characters")
	static UEPFAsyncGrantCharacter* GrantCharacter(UObject* WorldContext, const FString& CharacterName, const FString& ItemId);
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncCharGrantedSuccess OnSuccess;
protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;
private:
	FString Name; FString Item;
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const FString& CharId);
};

// ── Delete Character ─────────────────────────────────────────────────────────
UCLASS(meta = (DisplayName = "PlayFab: Delete Character"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncDeleteCharacter : public UEPFAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Delete Character"), Category = "PlayFab|Async|Characters")
	static UEPFAsyncDeleteCharacter* DeleteCharacter(UObject* WorldContext, const FString& CharacterId);
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncCharAction OnSuccess;
protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;
private:
	FString CharId;
	UFUNCTION() void HandleComplete(const FEPFResult& Result);
};

// ── Get Character Data ───────────────────────────────────────────────────────
UCLASS(meta = (DisplayName = "PlayFab: Get Character Data"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncGetCharData : public UEPFAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Get Character Data"), Category = "PlayFab|Async|Characters")
	static UEPFAsyncGetCharData* GetCharacterData(UObject* WorldContext, const FString& CharacterId, const TArray<FString>& Keys);
	/** Fires on success — data was received, use subsystem getters to retrieve it */
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncCharAction OnSuccess;
protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;
private:
	FString CharId; TArray<FString> RequestedKeys;
	UFUNCTION() void HandleComplete(const FEPFResult& Result);
};

// ── Get Character Stats ──────────────────────────────────────────────────────
UCLASS(meta = (DisplayName = "PlayFab: Get Character Stats"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncGetCharStats : public UEPFAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Get Character Stats"), Category = "PlayFab|Async|Characters")
	static UEPFAsyncGetCharStats* GetCharacterStatistics(UObject* WorldContext, const FString& CharacterId);
	/** Fires on success — stats were received, use subsystem getters to retrieve them */
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncCharAction OnSuccess;
protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;
private:
	FString CharId;
	UFUNCTION() void HandleComplete(const FEPFResult& Result);
};
