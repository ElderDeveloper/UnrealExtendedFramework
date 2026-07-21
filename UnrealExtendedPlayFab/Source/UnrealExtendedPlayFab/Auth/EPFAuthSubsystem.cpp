// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAuthSubsystem.h"
#include "Shared/EPFSettings.h"
#include "UnrealExtendedPlayFab.h"
#include "Dom/JsonObject.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "Engine/GameInstance.h"

namespace
{
	const TCHAR* DeviceLoginConfigSection = TEXT("/Script/UnrealExtendedPlayFab.EPFAuthSubsystem");
	const TCHAR* DeviceLoginIdConfigKey = TEXT("PersistentDeviceLoginId");
	const TCHAR* DeviceLoginIdSourceConfigKey = TEXT("PersistentDeviceLoginIdSource");

	FString GetDeviceLoginConfigPath()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ExtendedPlayFabAuth.ini"));
	}

	bool LoadPersistedDeviceLoginId(FString& OutLoginId, FString& OutSource)
	{
		const FString ConfigPath = GetDeviceLoginConfigPath();
		const bool bHasLoginId = GConfig->GetString(DeviceLoginConfigSection, DeviceLoginIdConfigKey, OutLoginId, ConfigPath);
		GConfig->GetString(DeviceLoginConfigSection, DeviceLoginIdSourceConfigKey, OutSource, ConfigPath);
		return bHasLoginId && !OutLoginId.IsEmpty();
	}

	void SavePersistedDeviceLoginId(const FString& LoginId, const FString& Source)
	{
		const FString ConfigPath = GetDeviceLoginConfigPath();
		GConfig->SetString(DeviceLoginConfigSection, DeviceLoginIdConfigKey, *LoginId, ConfigPath);
		GConfig->SetString(DeviceLoginConfigSection, DeviceLoginIdSourceConfigKey, *Source, ConfigPath);
		GConfig->Flush(false, ConfigPath);
	}

	FString DescribeSteamTicketForLog(const FString& SteamTicket)
	{
		if (SteamTicket.IsEmpty())
		{
			return TEXT("<empty>");
		}

		const int32 PrefixLen = FMath::Min(8, SteamTicket.Len());
		return FString::Printf(TEXT("prefix=%s..., len=%d"), *SteamTicket.Left(PrefixLen), SteamTicket.Len());
	}

	const TCHAR* GetLoginMethodName(const UEPFAuthSubsystem::ELastLoginMethod LoginMethod)
	{
		switch (LoginMethod)
		{
		case UEPFAuthSubsystem::LM_Steam:
			return TEXT("Steam");
		case UEPFAuthSubsystem::LM_CustomId:
			return TEXT("CustomId");
		case UEPFAuthSubsystem::LM_DeviceId:
			return TEXT("DeviceId");
		case UEPFAuthSubsystem::LM_Email:
			return TEXT("Email");
		case UEPFAuthSubsystem::LM_PlayFab:
			return TEXT("PlayFab");
		default:
			return TEXT("None");
		}
	}

	FString GetLoginIdentifierLabel(const UEPFAuthSubsystem::ELastLoginMethod LoginMethod)
	{
		switch (LoginMethod)
		{
		case UEPFAuthSubsystem::LM_Steam:
			return TEXT("SteamTicket");
		case UEPFAuthSubsystem::LM_CustomId:
			return TEXT("CustomId");
		case UEPFAuthSubsystem::LM_DeviceId:
			return TEXT("PersistedDeviceLoginId");
		case UEPFAuthSubsystem::LM_Email:
			return TEXT("Email");
		case UEPFAuthSubsystem::LM_PlayFab:
			return TEXT("Username");
		default:
			return TEXT("Identifier");
		}
	}

	FString GetLoginIdentifierValueForLog(const UEPFAuthSubsystem::ELastLoginMethod LoginMethod, const FString& Credential)
	{
		switch (LoginMethod)
		{
		case UEPFAuthSubsystem::LM_Steam:
			return DescribeSteamTicketForLog(Credential);
		case UEPFAuthSubsystem::LM_CustomId:
		case UEPFAuthSubsystem::LM_DeviceId:
		case UEPFAuthSubsystem::LM_Email:
		case UEPFAuthSubsystem::LM_PlayFab:
			return Credential.IsEmpty() ? TEXT("<empty>") : Credential;
		default:
			return TEXT("<none>");
		}
	}
}

void UEPFAuthSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEPFAuthSubsystem::Deinitialize()
{
	// Stop session refresh timer
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UWorld* World = GI->GetWorld())
		{
			World->GetTimerManager().ClearTimer(SessionRefreshTimer);
		}
	}

	ClearSharedAuthContext();
	CachedDisplayName.Empty();
	CachedEntityId.Empty();
	CachedEntityType.Empty();
	CachedEntityToken.Empty();
	bNewlyCreated = false;
	LastLoginMethod = LM_None;
	SavedCredential1.Empty();
	SavedCredential2.Empty();
	Super::Deinitialize();
}

void UEPFAuthSubsystem::LoginWithSteam(const FString& SteamTicket)
{
	if (!IsConfigured())
	{
		LogNotConfigured(TEXT("LoginWithSteam"));
		OnLoginComplete.Broadcast(FEPFResult::Failure(TEXT("TitleId not configured")), TEXT(""));
		return;
	}

	const UEPFSettings* Settings = GetSettings();

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("SteamTicket"), SteamTicket);
	Body->SetBoolField(TEXT("CreateAccount"), Settings->bCreateAccountOnFirstLogin);
	Body->SetStringField(TEXT("TitleId"), Settings->TitleId);

	LastLoginMethod = LM_Steam;
	SavedCredential1 = SteamTicket;
	SavedCredential2.Empty();

	SendPlayFabRequestDetailed(
		TEXT("/Client/LoginWithSteam"),
		Body,
		EEPFAuthMode::None,
		FOnPlayFabResponseDetailed::CreateUObject(this, &UEPFAuthSubsystem::HandleLoginResponse)
	);

	UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFAuthSubsystem::LoginWithSteam — Logging in..."));
}

void UEPFAuthSubsystem::LoginWithCustomId(const FString& CustomId)
{
	if (!IsConfigured())
	{
		LogNotConfigured(TEXT("LoginWithCustomId"));
		OnLoginComplete.Broadcast(FEPFResult::Failure(TEXT("TitleId not configured")), TEXT(""));
		return;
	}

	const UEPFSettings* Settings = GetSettings();

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("CustomId"), CustomId);
	Body->SetBoolField(TEXT("CreateAccount"), Settings->bCreateAccountOnFirstLogin);
	Body->SetStringField(TEXT("TitleId"), Settings->TitleId);

	LastLoginMethod = LM_CustomId;
	SavedCredential1 = CustomId;
	SavedCredential2.Empty();

	SendPlayFabRequestDetailed(
		TEXT("/Client/LoginWithCustomID"),
		Body,
		EEPFAuthMode::None,
		FOnPlayFabResponseDetailed::CreateUObject(this, &UEPFAuthSubsystem::HandleLoginResponse)
	);

	UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFAuthSubsystem::LoginWithCustomId — Logging in as '%s'..."), *CustomId);
}

void UEPFAuthSubsystem::LoginWithDeviceId()
{
	const FString PlatformDeviceId = FPlatformMisc::GetDeviceId();
	FString PersistedLoginId;
	FString PersistedLoginIdSource;
	const bool bLoadedPersistedLoginId = LoadPersistedDeviceLoginId(PersistedLoginId, PersistedLoginIdSource);

	if (!bLoadedPersistedLoginId)
	{
		PersistedLoginId = !PlatformDeviceId.IsEmpty()
			? PlatformDeviceId
			: FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
		PersistedLoginIdSource = !PlatformDeviceId.IsEmpty() ? TEXT("platform_device_id") : TEXT("generated_guid");
		SavePersistedDeviceLoginId(PersistedLoginId, PersistedLoginIdSource);
	}
	else if (PersistedLoginIdSource.IsEmpty())
	{
		PersistedLoginIdSource = TEXT("legacy_saved_value");
	}

	LoginWithCustomId(PersistedLoginId);
	LastLoginMethod = LM_DeviceId;
	SavedCredential1 = PersistedLoginId;
	SavedCredential2.Empty();

	UE_LOG(
		LogExtendedPlayFab,
		Log,
		TEXT("EPFAuthSubsystem::LoginWithDeviceId — Platform device ID: %s"),
		PlatformDeviceId.IsEmpty() ? TEXT("<empty>") : *PlatformDeviceId
	);
	UE_LOG(
		LogExtendedPlayFab,
		Log,
		TEXT("EPFAuthSubsystem::LoginWithDeviceId — Persisted login ID (%s, source=%s): %s"),
		bLoadedPersistedLoginId ? TEXT("loaded") : TEXT("created"),
		*PersistedLoginIdSource,
		*PersistedLoginId
	);

	if (PlatformDeviceId.IsEmpty())
	{
		UE_LOG(LogExtendedPlayFab, Warning, TEXT("EPFAuthSubsystem::LoginWithDeviceId — Platform device ID is empty; using persisted local ID for PlayFab login."));
	}
	else if (PlatformDeviceId == PersistedLoginId)
	{
		UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFAuthSubsystem::LoginWithDeviceId — Platform device ID matches the persisted PlayFab login ID."));
	}
	else
	{
		UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFAuthSubsystem::LoginWithDeviceId — Platform device ID differs from the persisted PlayFab login ID."));
	}
}

void UEPFAuthSubsystem::LoginWithEmail(const FString& Email, const FString& Password)
{
	if (!IsConfigured())
	{
		LogNotConfigured(TEXT("LoginWithEmail"));
		OnLoginComplete.Broadcast(FEPFResult::Failure(TEXT("TitleId not configured")), TEXT(""));
		return;
	}

	const UEPFSettings* Settings = GetSettings();

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("Email"), Email);
	Body->SetStringField(TEXT("Password"), Password);
	Body->SetStringField(TEXT("TitleId"), Settings->TitleId);

	LastLoginMethod = LM_Email;
	SavedCredential1 = Email;
	SavedCredential2 = Password;

	SendPlayFabRequestDetailed(
		TEXT("/Client/LoginWithEmailAddress"),
		Body,
		EEPFAuthMode::None,
		FOnPlayFabResponseDetailed::CreateUObject(this, &UEPFAuthSubsystem::HandleLoginResponse)
	);

	UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFAuthSubsystem::LoginWithEmail — Logging in with email..."));
}

void UEPFAuthSubsystem::LoginWithPlayFab(const FString& Username, const FString& Password)
{
	if (!IsConfigured())
	{
		LogNotConfigured(TEXT("LoginWithPlayFab"));
		OnLoginComplete.Broadcast(FEPFResult::Failure(TEXT("TitleId not configured")), TEXT(""));
		return;
	}

	const UEPFSettings* Settings = GetSettings();

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("Username"), Username);
	Body->SetStringField(TEXT("Password"), Password);
	Body->SetStringField(TEXT("TitleId"), Settings->TitleId);

	LastLoginMethod = LM_PlayFab;
	SavedCredential1 = Username;
	SavedCredential2 = Password;

	SendPlayFabRequestDetailed(
		TEXT("/Client/LoginWithPlayFab"),
		Body,
		EEPFAuthMode::None,
		FOnPlayFabResponseDetailed::CreateUObject(this, &UEPFAuthSubsystem::HandleLoginResponse)
	);

	UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFAuthSubsystem::LoginWithPlayFab — Logging in as '%s'..."), *Username);
}

void UEPFAuthSubsystem::RegisterUser(const FString& Username, const FString& Email, const FString& Password)
{
	if (!IsConfigured())
	{
		LogNotConfigured(TEXT("RegisterUser"));
		OnRegistrationComplete.Broadcast(FEPFResult::Failure(TEXT("TitleId not configured")), TEXT(""));
		return;
	}

	const UEPFSettings* Settings = GetSettings();

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("Username"), Username);
	Body->SetStringField(TEXT("Email"), Email);
	Body->SetStringField(TEXT("Password"), Password);
	Body->SetBoolField(TEXT("RequireBothUsernameAndEmail"), true);
	Body->SetStringField(TEXT("TitleId"), Settings->TitleId);

	SendPlayFabRequestDetailed(
		TEXT("/Client/RegisterPlayFabUser"),
		Body,
		EEPFAuthMode::None,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			FString PlayFabId;
			if (Result.bSuccess && Response.IsValid())
			{
				PlayFabId = Response->GetStringField(TEXT("PlayFabId"));

				FEPFAuthContext AuthContext;
				AuthContext.PlayFabId = PlayFabId;
				AuthContext.SessionTicket = Response->GetStringField(TEXT("SessionTicket"));

				// Parse Entity Token if available
				const TSharedPtr<FJsonObject>* EntityTokenObj = nullptr;
				if (Response->TryGetObjectField(TEXT("EntityToken"), EntityTokenObj) && EntityTokenObj)
				{
					CachedEntityToken = (*EntityTokenObj)->GetStringField(TEXT("EntityToken"));
					AuthContext.EntityToken = CachedEntityToken;
					const TSharedPtr<FJsonObject>* EntityObj = nullptr;
					if ((*EntityTokenObj)->TryGetObjectField(TEXT("Entity"), EntityObj) && EntityObj)
					{
						CachedEntityId = (*EntityObj)->GetStringField(TEXT("Id"));
						CachedEntityType = (*EntityObj)->GetStringField(TEXT("Type"));
						AuthContext.EntityId = CachedEntityId;
						AuthContext.EntityType = CachedEntityType;
					}
				}

				SetSharedAuthContext(AuthContext);

				UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFAuthSubsystem — Registration successful. PlayFabId: %s"), *PlayFabId);
			}
			OnRegistrationComplete.Broadcast(Result, PlayFabId);
		})
	);

	UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFAuthSubsystem::RegisterUser — Registering '%s'..."), *Username);
}

void UEPFAuthSubsystem::AddUsernamePassword(const FString& Username, const FString& Email, const FString& Password)
{
	if (Username.IsEmpty() || Password.IsEmpty()) { OnUsernamePasswordAdded.Broadcast(FEPFResult::Failure(TEXT("Username and password are required"))); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("Username"), Username);
	Body->SetStringField(TEXT("Email"), Email);
	Body->SetStringField(TEXT("Password"), Password);

	SendPlayFabRequestDetailed(TEXT("/Client/AddUsernamePassword"), Body, EEPFAuthMode::SessionTicket,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			if (Result.bSuccess) UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFAuthSubsystem — Username/password added"));
			OnUsernamePasswordAdded.Broadcast(Result);
		}));
}

void UEPFAuthSubsystem::SendAccountRecoveryEmail(const FString& Email)
{
	if (!IsConfigured())
	{
		LogNotConfigured(TEXT("SendAccountRecoveryEmail"));
		OnAccountRecoveryEmailSent.Broadcast(FEPFResult::Failure(TEXT("TitleId not configured")));
		return;
	}

	const UEPFSettings* Settings = GetSettings();

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("Email"), Email);
	Body->SetStringField(TEXT("TitleId"), Settings->TitleId);

	SendPlayFabRequestDetailed(TEXT("/Client/SendAccountRecoveryEmail"), Body, EEPFAuthMode::None,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			if (Result.bSuccess) UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFAuthSubsystem — Recovery email sent"));
			OnAccountRecoveryEmailSent.Broadcast(Result);
		}));
}

void UEPFAuthSubsystem::UpdateDisplayName(const FString& DisplayName)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("DisplayName"), DisplayName);

	SendPlayFabRequestDetailed(
		TEXT("/Client/UpdateUserTitleDisplayName"),
		Body,
		EEPFAuthMode::SessionTicket,
		FOnPlayFabResponseDetailed::CreateLambda([this, DisplayName](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			if (Result.bSuccess) 			{
				CachedDisplayName = DisplayName;
				UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFAuthSubsystem — Display name updated to '%s'"), *DisplayName);
			}
			else
			{
				UE_LOG(LogExtendedPlayFab, Warning, TEXT("EPFAuthSubsystem — Failed to update display name to '%s'"), *DisplayName);
			}
			OnDisplayNameUpdated.Broadcast(Result, Result.bSuccess ? DisplayName : TEXT(""));
		})
	);
}

void UEPFAuthSubsystem::Logout()
{
	ClearSharedAuthContext();
	CachedDisplayName.Empty();
	CachedEntityId.Empty();
	CachedEntityType.Empty();
	CachedEntityToken.Empty();
	bNewlyCreated = false;

	UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFAuthSubsystem::Logout — Session cleared"));
	OnLogoutComplete.Broadcast();
}

bool UEPFAuthSubsystem::IsLoggedIn() const
{
	return IsAuthenticated();
}

FString UEPFAuthSubsystem::GetDisplayName() const
{
	return CachedDisplayName;
}

bool UEPFAuthSubsystem::WasNewlyCreated() const
{
	return bNewlyCreated;
}

FString UEPFAuthSubsystem::GetEntityId() const
{
	return UEPFSubsystem::GetEntityId();
}

FString UEPFAuthSubsystem::GetEntityType() const
{
	return UEPFSubsystem::GetEntityType();
}

FString UEPFAuthSubsystem::GetEntityToken() const
{
	return UEPFSubsystem::GetEntityToken();
}

void UEPFAuthSubsystem::HandleLoginResponse(const FEPFResult& Result, TSharedPtr<FJsonObject> JsonResponse)
{
	if (Result.bSuccess && JsonResponse.IsValid())
	{
		FEPFAuthContext AuthContext;
		AuthContext.SessionTicket = JsonResponse->GetStringField(TEXT("SessionTicket"));
		AuthContext.PlayFabId = JsonResponse->GetStringField(TEXT("PlayFabId"));
		bNewlyCreated = JsonResponse->GetBoolField(TEXT("NewlyCreated"));

		// Try to get display name from InfoResultPayload if available
		const TSharedPtr<FJsonObject>* InfoPayload = nullptr;
		if (JsonResponse->TryGetObjectField(TEXT("InfoResultPayload"), InfoPayload) && InfoPayload)
		{
			const TSharedPtr<FJsonObject>* AccountInfo = nullptr;
			if ((*InfoPayload)->TryGetObjectField(TEXT("AccountInfo"), AccountInfo) && AccountInfo)
			{
				const TSharedPtr<FJsonObject>* TitleInfo = nullptr;
				if ((*AccountInfo)->TryGetObjectField(TEXT("TitleInfo"), TitleInfo) && TitleInfo)
				{
					CachedDisplayName = (*TitleInfo)->GetStringField(TEXT("DisplayName"));
				}
			}
		}

		UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFAuthSubsystem — Login successful. PlayFabId: %s, NewlyCreated: %s"),
			*AuthContext.PlayFabId, bNewlyCreated ? TEXT("true") : TEXT("false"));

		// Parse Entity Token for Entity API (Groups, Matchmaking)
		const TSharedPtr<FJsonObject>* EntityTokenObj = nullptr;
		if (JsonResponse->TryGetObjectField(TEXT("EntityToken"), EntityTokenObj) && EntityTokenObj)
		{
			CachedEntityToken = (*EntityTokenObj)->GetStringField(TEXT("EntityToken"));
			AuthContext.EntityToken = CachedEntityToken;
			const TSharedPtr<FJsonObject>* EntityObj = nullptr;
			if ((*EntityTokenObj)->TryGetObjectField(TEXT("Entity"), EntityObj) && EntityObj)
			{
				CachedEntityId = (*EntityObj)->GetStringField(TEXT("Id"));
				CachedEntityType = (*EntityObj)->GetStringField(TEXT("Type"));
				AuthContext.EntityId = CachedEntityId;
				AuthContext.EntityType = CachedEntityType;
			}
			UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFAuthSubsystem — Entity: %s (%s)"), *CachedEntityId, *CachedEntityType);
		}

		SetSharedAuthContext(AuthContext);

		// Start session refresh timer on successful login
		StartSessionRefreshTimer();

		OnLoginComplete.Broadcast(FEPFResult::Success(), AuthContext.PlayFabId);
	}
	else
	{
		const FString ErrorMessage = !Result.ErrorMessage.IsEmpty()
			? Result.ErrorMessage
			: (JsonResponse.IsValid() ? JsonResponse->GetStringField(TEXT("errorMessage")) : TEXT("Unknown error"));
		const FString LoginIdentifierLabel = GetLoginIdentifierLabel(LastLoginMethod);
		const FString LoginIdentifierValue = GetLoginIdentifierValueForLog(LastLoginMethod, SavedCredential1);
		UE_LOG(
			LogExtendedPlayFab,
			Warning,
			TEXT("EPFAuthSubsystem — Login failed [%s] %s=%s: %s"),
			GetLoginMethodName(LastLoginMethod),
			*LoginIdentifierLabel,
			*LoginIdentifierValue,
			*ErrorMessage
		);
		OnLoginComplete.Broadcast(Result.bSuccess ? Result : FEPFResult::Failure(ErrorMessage), TEXT(""));
	}
}


// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Session Refresh
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void UEPFAuthSubsystem::SetAutoSessionRefresh(bool bEnabled)
{
	bAutoRefreshEnabled = bEnabled;
	if (!bEnabled)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UWorld* World = GI->GetWorld())
			{
				World->GetTimerManager().ClearTimer(SessionRefreshTimer);
			}
		}
	}
	else if (IsAuthenticated() && !SessionRefreshTimer.IsValid())
	{
		StartSessionRefreshTimer();
	}
}

void UEPFAuthSubsystem::StartSessionRefreshTimer()
{
	if (!bAutoRefreshEnabled) return;

	UGameInstance* GI = GetGameInstance();
	if (!GI) return;
	UWorld* World = GI->GetWorld();
	if (!World) return;

	// PlayFab session tickets expire after 24 hours.
	// Refresh every 30 minutes to stay well within the window.
	World->GetTimerManager().SetTimer(
		SessionRefreshTimer,
		this,
		&UEPFAuthSubsystem::RefreshSession,
		1800.0f, // 30 min
		true     // looping
	);

	UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFAuthSubsystem — Session refresh timer started (30 min interval)"));
}

void UEPFAuthSubsystem::RefreshSession()
{
	if (LastLoginMethod == LM_None)
	{
		UE_LOG(LogExtendedPlayFab, Warning, TEXT("EPFAuthSubsystem::RefreshSession — No stored login method, cannot refresh"));
		return;
	}

	const FString LoginIdentifierLabel = GetLoginIdentifierLabel(LastLoginMethod);
	const FString LoginIdentifierValue = GetLoginIdentifierValueForLog(LastLoginMethod, SavedCredential1);
	UE_LOG(
		LogExtendedPlayFab,
		Log,
		TEXT("EPFAuthSubsystem::RefreshSession — Silently re-authenticating with %s [%s=%s]..."),
		GetLoginMethodName(LastLoginMethod),
		*LoginIdentifierLabel,
		*LoginIdentifierValue
	);

	switch (LastLoginMethod)
	{
	case LM_Steam:
		LoginWithSteam(SavedCredential1);
		break;
	case LM_CustomId:
	case LM_DeviceId:
		LoginWithCustomId(SavedCredential1);
		break;
	case LM_Email:
		LoginWithEmail(SavedCredential1, SavedCredential2);
		break;
	case LM_PlayFab:
		LoginWithPlayFab(SavedCredential1, SavedCredential2);
		break;
	default:
		break;
	}
}
