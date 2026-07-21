// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActions/EPFAsyncTypes.h"
#include "EPFAsyncAuth.generated.h"


// ── Login ────────────────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAsyncLoginSuccess, const FString&, PlayFabId);

UCLASS(meta = (DisplayName = "PlayFab: Login"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncLogin : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Login With Steam"), Category = "PlayFab|Async|Auth")
	static UEPFAsyncLogin* LoginWithSteam(UObject* WorldContext, const FString& SteamTicket);

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Login With Custom ID"), Category = "PlayFab|Async|Auth")
	static UEPFAsyncLogin* LoginWithCustomId(UObject* WorldContext, const FString& CustomId);

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Login With Device ID"), Category = "PlayFab|Async|Auth")
	static UEPFAsyncLogin* LoginWithDeviceId(UObject* WorldContext);

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Login With Email"), Category = "PlayFab|Async|Auth")
	static UEPFAsyncLogin* LoginWithEmail(UObject* WorldContext, const FString& Email, const FString& Password);

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Login With PlayFab"), Category = "PlayFab|Async|Auth")
	static UEPFAsyncLogin* LoginWithPlayFabAccount(UObject* WorldContext, const FString& Username, const FString& Password);

	UPROPERTY(BlueprintAssignable) FOnEPFAsyncLoginSuccess OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;

	virtual void Activate() override;

private:
	enum ELoginMethod { ELogin_Steam, ELogin_CustomID, ELogin_DeviceID, ELogin_Email, ELogin_PlayFab };
	ELoginMethod Method;
	FString Credential;
	FString Credential2;
	TWeakObjectPtr<UObject> WorldContext;

	UFUNCTION() void HandleComplete(const FEPFResult& Result, const FString& PlayFabId);
};


// ── Display Name ─────────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAsyncDisplayNameSuccess, const FString&, DisplayName);

UCLASS(meta = (DisplayName = "PlayFab: Update Display Name"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncUpdateDisplayName : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Update Display Name"), Category = "PlayFab|Async|Auth")
	static UEPFAsyncUpdateDisplayName* UpdateDisplayName(UObject* WorldContext, const FString& DisplayName);

	UPROPERTY(BlueprintAssignable) FOnEPFAsyncDisplayNameSuccess OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;

	virtual void Activate() override;

private:
	FString DisplayName;
	TWeakObjectPtr<UObject> WorldContext;

	UFUNCTION() void HandleComplete(const FEPFResult& Result, const FString& NewDisplayName);
};


// ── Register User ────────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAsyncRegisterSuccess, const FString&, PlayFabId);

UCLASS(meta = (DisplayName = "PlayFab: Register User"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncRegisterUser : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Register User"), Category = "PlayFab|Async|Auth")
	static UEPFAsyncRegisterUser* RegisterUser(UObject* WorldContext, const FString& Username, const FString& Email, const FString& Password);

	UPROPERTY(BlueprintAssignable) FOnEPFAsyncRegisterSuccess OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;

	virtual void Activate() override;

private:
	FString Username, Email, Password;
	TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const FString& PlayFabId);
};


// ── Add Username Password ────────────────────────────────────────────────────

UCLASS(meta = (DisplayName = "PlayFab: Add Username Password"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncAddUsernamePassword : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Add Username Password"), Category = "PlayFab|Async|Auth")
	static UEPFAsyncAddUsernamePassword* AddUsernamePassword(UObject* WorldContext, const FString& Username, const FString& Email, const FString& Password);

	UPROPERTY(BlueprintAssignable) FOnEPFAsyncSimpleSuccess OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;

	virtual void Activate() override;

private:
	FString Username, Email, Password;
	TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result);
};


// ── Account Recovery Email ───────────────────────────────────────────────────

UCLASS(meta = (DisplayName = "PlayFab: Send Account Recovery Email"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncAccountRecovery : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Send Account Recovery Email"), Category = "PlayFab|Async|Auth")
	static UEPFAsyncAccountRecovery* SendAccountRecoveryEmail(UObject* WorldContext, const FString& Email);

	UPROPERTY(BlueprintAssignable) FOnEPFAsyncSimpleSuccess OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;

	virtual void Activate() override;

private:
	FString Email;
	TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result);
};
