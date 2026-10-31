// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncAuth.h"
#include "Auth/EPFAuthSubsystem.h"


// ── Login ────────────────────────────────────────────────────────────────────

UEPFAsyncLogin* UEPFAsyncLogin::LoginWithSteam(UObject* WorldContext, const FString& SteamTicket)
{
	auto* Action = NewObject<UEPFAsyncLogin>();
	Action->Method = ELogin_Steam;
	Action->Credential = SteamTicket;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

UEPFAsyncLogin* UEPFAsyncLogin::LoginWithCustomId(UObject* WorldContext, const FString& CustomId)
{
	auto* Action = NewObject<UEPFAsyncLogin>();
	Action->Method = ELogin_CustomID;
	Action->Credential = CustomId;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

UEPFAsyncLogin* UEPFAsyncLogin::LoginWithDeviceId(UObject* WorldContext)
{
	auto* Action = NewObject<UEPFAsyncLogin>();
	Action->Method = ELogin_DeviceID;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

UEPFAsyncLogin* UEPFAsyncLogin::LoginWithEmail(UObject* WorldContext, const FString& Email, const FString& Password)
{
	auto* Action = NewObject<UEPFAsyncLogin>();
	Action->Method = ELogin_Email;
	Action->Credential = Email;
	Action->Credential2 = Password;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

UEPFAsyncLogin* UEPFAsyncLogin::LoginWithPlayFabAccount(UObject* WorldContext, const FString& Username, const FString& Password)
{
	auto* Action = NewObject<UEPFAsyncLogin>();
	Action->Method = ELogin_PlayFab;
	Action->Credential = Username;
	Action->Credential2 = Password;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEPFAsyncLogin::ExecuteAction()
{
	auto* Auth = RequireGameInstanceSubsystem<UEPFAuthSubsystem>();
	if (!Auth) return;

	switch (Method)
	{
	case ELogin_Steam:
		if (Credential.IsEmpty()) { FailInvalidArgument(TEXT("Steam ticket cannot be empty")); return; }
		break;
	case ELogin_CustomID:
		if (Credential.IsEmpty()) { FailInvalidArgument(TEXT("Custom ID cannot be empty")); return; }
		break;
	case ELogin_Email:
		if (Credential.IsEmpty() || Credential2.IsEmpty()) { FailInvalidArgument(TEXT("Email and password cannot be empty")); return; }
		break;
	case ELogin_PlayFab:
		if (Credential.IsEmpty() || Credential2.IsEmpty()) { FailInvalidArgument(TEXT("Username and password cannot be empty")); return; }
		break;
	default:
		break;
	}

	Auth->OnLoginComplete.AddDynamic(this, &UEPFAsyncLogin::HandleComplete);

	// Every LoginWith* call below reaches PlayFab through SendPlayFabRequestDetailed (Device ID via
	// LoginWithCustomId), so the OnLoginComplete broadcast it causes carries this node's context.
	CallWithContext([&]
	{
		switch (Method)
		{
		case ELogin_Steam: Auth->LoginWithSteam(Credential); break;
		case ELogin_CustomID: Auth->LoginWithCustomId(Credential); break;
		case ELogin_DeviceID: Auth->LoginWithDeviceId(); break;
		case ELogin_Email: Auth->LoginWithEmail(Credential, Credential2); break;
		case ELogin_PlayFab: Auth->LoginWithPlayFab(Credential, Credential2); break;
		}
	});
}

void UEPFAsyncLogin::CancelWork()
{
	if (auto* Auth = GetGameInstanceSubsystem<UEPFAuthSubsystem>())
		Auth->OnLoginComplete.RemoveDynamic(this, &UEPFAsyncLogin::HandleComplete);
}

void UEPFAsyncLogin::HandleComplete(const FEPFResult& Result, const FString& PlayFabId)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(PlayFabId) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Login failed")));
}


// ── Display Name ─────────────────────────────────────────────────────────────

UEPFAsyncUpdateDisplayName* UEPFAsyncUpdateDisplayName::UpdateDisplayName(UObject* WorldContext, const FString& DisplayName)
{
	auto* Action = NewObject<UEPFAsyncUpdateDisplayName>();
	Action->RequestedDisplayName = DisplayName;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEPFAsyncUpdateDisplayName::ExecuteAction()
{
	if (RequestedDisplayName.Len() < 3 || RequestedDisplayName.Len() > 25) { FailInvalidArgument(TEXT("Display name must be 3-25 characters")); return; }
	auto* Auth = RequireGameInstanceSubsystem<UEPFAuthSubsystem>();
	if (!Auth) return;
	Auth->OnDisplayNameUpdated.AddDynamic(this, &UEPFAsyncUpdateDisplayName::HandleComplete);
	CallWithContext([&] { Auth->UpdateDisplayName(RequestedDisplayName); });
}

void UEPFAsyncUpdateDisplayName::CancelWork()
{
	if (auto* Auth = GetGameInstanceSubsystem<UEPFAuthSubsystem>())
		Auth->OnDisplayNameUpdated.RemoveDynamic(this, &UEPFAsyncUpdateDisplayName::HandleComplete);
}

void UEPFAsyncUpdateDisplayName::HandleComplete(const FEPFResult& Result, const FString& NewDisplayName)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(NewDisplayName) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to update display name")));
}


// ── Register User ────────────────────────────────────────────────────────────

UEPFAsyncRegisterUser* UEPFAsyncRegisterUser::RegisterUser(UObject* WorldContext, const FString& Username, const FString& Email, const FString& Password)
{
	auto* Action = NewObject<UEPFAsyncRegisterUser>();
	Action->AccountUsername = Username;
	Action->AccountEmail = Email;
	Action->AccountPassword = Password;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEPFAsyncRegisterUser::ExecuteAction()
{
	auto* Auth = RequireGameInstanceSubsystem<UEPFAuthSubsystem>();
	if (!Auth) return;
	Auth->OnRegistrationComplete.AddDynamic(this, &UEPFAsyncRegisterUser::HandleComplete);
	CallWithContext([&] { Auth->RegisterUser(AccountUsername, AccountEmail, AccountPassword); });
}

void UEPFAsyncRegisterUser::CancelWork()
{
	if (auto* Auth = GetGameInstanceSubsystem<UEPFAuthSubsystem>())
		Auth->OnRegistrationComplete.RemoveDynamic(this, &UEPFAsyncRegisterUser::HandleComplete);
}

void UEPFAsyncRegisterUser::HandleComplete(const FEPFResult& Result, const FString& PlayFabId)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(PlayFabId) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Registration failed")));
}


// ── Add Username Password ────────────────────────────────────────────────────

UEPFAsyncAddUsernamePassword* UEPFAsyncAddUsernamePassword::AddUsernamePassword(UObject* WorldContext, const FString& Username, const FString& Email, const FString& Password)
{
	auto* Action = NewObject<UEPFAsyncAddUsernamePassword>();
	Action->AccountUsername = Username;
	Action->AccountEmail = Email;
	Action->AccountPassword = Password;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEPFAsyncAddUsernamePassword::ExecuteAction()
{
	auto* Auth = RequireGameInstanceSubsystem<UEPFAuthSubsystem>();
	if (!Auth) return;
	Auth->OnUsernamePasswordAdded.AddDynamic(this, &UEPFAsyncAddUsernamePassword::HandleComplete);
	CallWithContext([&] { Auth->AddUsernamePassword(AccountUsername, AccountEmail, AccountPassword); });
}

void UEPFAsyncAddUsernamePassword::CancelWork()
{
	if (auto* Auth = GetGameInstanceSubsystem<UEPFAuthSubsystem>())
		Auth->OnUsernamePasswordAdded.RemoveDynamic(this, &UEPFAsyncAddUsernamePassword::HandleComplete);
}

void UEPFAsyncAddUsernamePassword::HandleComplete(const FEPFResult& Result)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to add username/password")));
}


// ── Account Recovery Email ───────────────────────────────────────────────────

UEPFAsyncAccountRecovery* UEPFAsyncAccountRecovery::SendAccountRecoveryEmail(UObject* WorldContext, const FString& Email)
{
	auto* Action = NewObject<UEPFAsyncAccountRecovery>();
	Action->RecoveryEmail = Email;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEPFAsyncAccountRecovery::ExecuteAction()
{
	auto* Auth = RequireGameInstanceSubsystem<UEPFAuthSubsystem>();
	if (!Auth) return;
	Auth->OnAccountRecoveryEmailSent.AddDynamic(this, &UEPFAsyncAccountRecovery::HandleComplete);
	CallWithContext([&] { Auth->SendAccountRecoveryEmail(RecoveryEmail); });
}

void UEPFAsyncAccountRecovery::CancelWork()
{
	if (auto* Auth = GetGameInstanceSubsystem<UEPFAuthSubsystem>())
		Auth->OnAccountRecoveryEmailSent.RemoveDynamic(this, &UEPFAsyncAccountRecovery::HandleComplete);
}

void UEPFAsyncAccountRecovery::HandleComplete(const FEPFResult& Result)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to send recovery email")));
}
