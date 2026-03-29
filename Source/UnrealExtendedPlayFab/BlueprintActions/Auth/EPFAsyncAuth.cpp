// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncAuth.h"
#include "Auth/EPFAuthSubsystem.h"
#include "Engine/GameInstance.h"


// ── Login ────────────────────────────────────────────────────────────────────

UEPFAsyncLogin* UEPFAsyncLogin::LoginWithSteam(UObject* WorldContext, const FString& SteamTicket)
{
	auto* Action = NewObject<UEPFAsyncLogin>();
	Action->Method = ELogin_Steam;
	Action->Credential = SteamTicket;
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

UEPFAsyncLogin* UEPFAsyncLogin::LoginWithCustomId(UObject* WorldContext, const FString& CustomId)
{
	auto* Action = NewObject<UEPFAsyncLogin>();
	Action->Method = ELogin_CustomID;
	Action->Credential = CustomId;
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

UEPFAsyncLogin* UEPFAsyncLogin::LoginWithDeviceId(UObject* WorldContext)
{
	auto* Action = NewObject<UEPFAsyncLogin>();
	Action->Method = ELogin_DeviceID;
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

UEPFAsyncLogin* UEPFAsyncLogin::LoginWithEmail(UObject* WorldContext, const FString& Email, const FString& Password)
{
	auto* Action = NewObject<UEPFAsyncLogin>();
	Action->Method = ELogin_Email;
	Action->Credential = Email;
	Action->Credential2 = Password;
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

UEPFAsyncLogin* UEPFAsyncLogin::LoginWithPlayFabAccount(UObject* WorldContext, const FString& Username, const FString& Password)
{
	auto* Action = NewObject<UEPFAsyncLogin>();
	Action->Method = ELogin_PlayFab;
	Action->Credential = Username;
	Action->Credential2 = Password;
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEPFAsyncLogin::Activate()
{
	auto* Auth = GetEPFSubsystemFromContext<UEPFAuthSubsystem>(WorldContext.Get());
	if (!Auth) { BroadcastEPFFailure(OnFailure, TEXT("Auth subsystem not available")); SetReadyToDestroy(); return; }

	Auth->OnLoginComplete.AddDynamic(this, &UEPFAsyncLogin::HandleComplete);

	switch (Method)
	{
	case ELogin_Steam:
		if (Credential.IsEmpty()) { Auth->OnLoginComplete.RemoveDynamic(this, &UEPFAsyncLogin::HandleComplete); BroadcastEPFFailure(OnFailure, TEXT("Steam ticket cannot be empty")); SetReadyToDestroy(); return; }
		Auth->LoginWithSteam(Credential);
		break;
	case ELogin_CustomID:
		if (Credential.IsEmpty()) { Auth->OnLoginComplete.RemoveDynamic(this, &UEPFAsyncLogin::HandleComplete); BroadcastEPFFailure(OnFailure, TEXT("Custom ID cannot be empty")); SetReadyToDestroy(); return; }
		Auth->LoginWithCustomId(Credential);
		break;
	case ELogin_DeviceID:
		Auth->LoginWithDeviceId();
		break;
	case ELogin_Email:
		if (Credential.IsEmpty() || Credential2.IsEmpty()) { Auth->OnLoginComplete.RemoveDynamic(this, &UEPFAsyncLogin::HandleComplete); BroadcastEPFFailure(OnFailure, TEXT("Email and password cannot be empty")); SetReadyToDestroy(); return; }
		Auth->LoginWithEmail(Credential, Credential2);
		break;
	case ELogin_PlayFab:
		if (Credential.IsEmpty() || Credential2.IsEmpty()) { Auth->OnLoginComplete.RemoveDynamic(this, &UEPFAsyncLogin::HandleComplete); BroadcastEPFFailure(OnFailure, TEXT("Username and password cannot be empty")); SetReadyToDestroy(); return; }
		Auth->LoginWithPlayFab(Credential, Credential2);
		break;
	}
}

void UEPFAsyncLogin::HandleComplete(const FEPFResult& Result, const FString& PlayFabId)
{
	if (auto* Auth = GetEPFSubsystemFromContext<UEPFAuthSubsystem>(WorldContext.Get()))
		Auth->OnLoginComplete.RemoveDynamic(this, &UEPFAsyncLogin::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(PlayFabId) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Login failed")));
	SetReadyToDestroy();
}


// ── Display Name ─────────────────────────────────────────────────────────────

UEPFAsyncUpdateDisplayName* UEPFAsyncUpdateDisplayName::UpdateDisplayName(UObject* WorldContext, const FString& DisplayName)
{
	auto* Action = NewObject<UEPFAsyncUpdateDisplayName>();
	Action->DisplayName = DisplayName;
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEPFAsyncUpdateDisplayName::Activate()
{
	if (DisplayName.Len() < 3 || DisplayName.Len() > 25) { BroadcastEPFFailure(OnFailure, TEXT("Display name must be 3-25 characters")); SetReadyToDestroy(); return; }
	auto* Auth = GetEPFSubsystemFromContext<UEPFAuthSubsystem>(WorldContext.Get());
	if (!Auth) { BroadcastEPFFailure(OnFailure, TEXT("Auth subsystem not available")); SetReadyToDestroy(); return; }
	Auth->OnDisplayNameUpdated.AddDynamic(this, &UEPFAsyncUpdateDisplayName::HandleComplete);
	Auth->UpdateDisplayName(DisplayName);
}

void UEPFAsyncUpdateDisplayName::HandleComplete(const FEPFResult& Result, const FString& NewDisplayName)
{
	if (auto* Auth = GetEPFSubsystemFromContext<UEPFAuthSubsystem>(WorldContext.Get()))
		Auth->OnDisplayNameUpdated.RemoveDynamic(this, &UEPFAsyncUpdateDisplayName::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(NewDisplayName) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to update display name")));
	SetReadyToDestroy();
}


// ── Register User ────────────────────────────────────────────────────────────

UEPFAsyncRegisterUser* UEPFAsyncRegisterUser::RegisterUser(UObject* WorldContext, const FString& Username, const FString& Email, const FString& Password)
{
	auto* Action = NewObject<UEPFAsyncRegisterUser>();
	Action->Username = Username;
	Action->Email = Email;
	Action->Password = Password;
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEPFAsyncRegisterUser::Activate()
{
	auto* Auth = GetEPFSubsystemFromContext<UEPFAuthSubsystem>(WorldContext.Get());
	if (!Auth) { BroadcastEPFFailure(OnFailure, TEXT("Auth subsystem not available")); SetReadyToDestroy(); return; }
	Auth->OnRegistrationComplete.AddDynamic(this, &UEPFAsyncRegisterUser::HandleComplete);
	Auth->RegisterUser(Username, Email, Password);
}

void UEPFAsyncRegisterUser::HandleComplete(const FEPFResult& Result, const FString& PlayFabId)
{
	if (auto* Auth = GetEPFSubsystemFromContext<UEPFAuthSubsystem>(WorldContext.Get()))
		Auth->OnRegistrationComplete.RemoveDynamic(this, &UEPFAsyncRegisterUser::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(PlayFabId) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Registration failed")));
	SetReadyToDestroy();
}


// ── Add Username Password ────────────────────────────────────────────────────

UEPFAsyncAddUsernamePassword* UEPFAsyncAddUsernamePassword::AddUsernamePassword(UObject* WorldContext, const FString& Username, const FString& Email, const FString& Password)
{
	auto* Action = NewObject<UEPFAsyncAddUsernamePassword>();
	Action->Username = Username;
	Action->Email = Email;
	Action->Password = Password;
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEPFAsyncAddUsernamePassword::Activate()
{
	auto* Auth = GetEPFSubsystemFromContext<UEPFAuthSubsystem>(WorldContext.Get());
	if (!Auth) { BroadcastEPFFailure(OnFailure, TEXT("Auth subsystem not available")); SetReadyToDestroy(); return; }
	Auth->OnUsernamePasswordAdded.AddDynamic(this, &UEPFAsyncAddUsernamePassword::HandleComplete);
	Auth->AddUsernamePassword(Username, Email, Password);
}

void UEPFAsyncAddUsernamePassword::HandleComplete(const FEPFResult& Result)
{
	if (auto* Auth = GetEPFSubsystemFromContext<UEPFAuthSubsystem>(WorldContext.Get()))
		Auth->OnUsernamePasswordAdded.RemoveDynamic(this, &UEPFAsyncAddUsernamePassword::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to add username/password")));
	SetReadyToDestroy();
}


// ── Account Recovery Email ───────────────────────────────────────────────────

UEPFAsyncAccountRecovery* UEPFAsyncAccountRecovery::SendAccountRecoveryEmail(UObject* WorldContext, const FString& Email)
{
	auto* Action = NewObject<UEPFAsyncAccountRecovery>();
	Action->Email = Email;
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEPFAsyncAccountRecovery::Activate()
{
	auto* Auth = GetEPFSubsystemFromContext<UEPFAuthSubsystem>(WorldContext.Get());
	if (!Auth) { BroadcastEPFFailure(OnFailure, TEXT("Auth subsystem not available")); SetReadyToDestroy(); return; }
	Auth->OnAccountRecoveryEmailSent.AddDynamic(this, &UEPFAsyncAccountRecovery::HandleComplete);
	Auth->SendAccountRecoveryEmail(Email);
}

void UEPFAsyncAccountRecovery::HandleComplete(const FEPFResult& Result)
{
	if (auto* Auth = GetEPFSubsystemFromContext<UEPFAuthSubsystem>(WorldContext.Get()))
		Auth->OnAccountRecoveryEmailSent.RemoveDynamic(this, &UEPFAsyncAccountRecovery::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to send recovery email")));
	SetReadyToDestroy();
}


