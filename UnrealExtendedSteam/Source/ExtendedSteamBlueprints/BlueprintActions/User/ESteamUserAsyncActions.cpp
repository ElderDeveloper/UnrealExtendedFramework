// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "BlueprintActions/User/ESteamUserAsyncActions.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "User/ESteamUserSubsystem.h"
#include "Apps/ESteamAppsSubsystem.h"

namespace ESteamAsyncActionHelpers
{
	UESteamUserSubsystem* GetUserSubsystem(const UObject* WorldContextObject)
	{
		if (const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr)
		{
			if (const UGameInstance* GameInstance = World->GetGameInstance())
			{
				return GameInstance->GetSubsystem<UESteamUserSubsystem>();
			}
		}
		return nullptr;
	}

	UESteamAppsSubsystem* GetAppsSubsystem(const UObject* WorldContextObject)
	{
		if (const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr)
		{
			if (const UGameInstance* GameInstance = World->GetGameInstance())
			{
				return GameInstance->GetSubsystem<UESteamAppsSubsystem>();
			}
		}
		return nullptr;
	}
}

USteamAsyncGetWebApiAuthTicket* USteamAsyncGetWebApiAuthTicket::GetWebApiAuthTicket(UObject* WorldContext, const FString& RemoteServiceIdentity, float Timeout)
{
	USteamAsyncGetWebApiAuthTicket* Action = NewObject<USteamAsyncGetWebApiAuthTicket>();
	Action->WorldContextObject = WorldContext;
	Action->RemoteServiceIdentity = RemoteServiceIdentity;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncGetWebApiAuthTicket::Activate()
{
	UESteamUserSubsystem* Subsystem = ESteamAsyncActionHelpers::GetUserSubsystem(WorldContextObject);
	if (!Subsystem)
	{
		Complete(false, FString());
		return;
	}

	UserSubsystem = Subsystem;
	Subsystem->OnWebApiAuthTicketReady.AddDynamic(this, &USteamAsyncGetWebApiAuthTicket::HandleWebApiAuthTicketReady);

	RequestHandle = Subsystem->RequestWebApiAuthTicket(RemoteServiceIdentity);
	if (RequestHandle == 0)
	{
		Complete(false, FString());
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncGetWebApiAuthTicket::HandleWebApiAuthTicketReady(bool bSuccess, int32 TicketHandle, const FString& HexTicket)
{
	// Other requests may be in flight on the shared subsystem delegate; only react to ours.
	if (TicketHandle != RequestHandle)
	{
		return;
	}
	Complete(bSuccess, HexTicket);
}

void USteamAsyncGetWebApiAuthTicket::OnTimeoutFailure()
{
	Complete(false, FString());
}

void USteamAsyncGetWebApiAuthTicket::Complete(bool bSuccess, const FString& HexTicket)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamUserSubsystem* Subsystem = UserSubsystem.Get())
	{
		Subsystem->OnWebApiAuthTicketReady.RemoveDynamic(this, &USteamAsyncGetWebApiAuthTicket::HandleWebApiAuthTicketReady);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(HexTicket);
	}
	else
	{
		OnFailure.Broadcast(HexTicket);
	}

	SetReadyToDestroy();
}

USteamAsyncRequestEncryptedAppTicket* USteamAsyncRequestEncryptedAppTicket::RequestEncryptedAppTicket(UObject* WorldContext, float Timeout)
{
	USteamAsyncRequestEncryptedAppTicket* Action = NewObject<USteamAsyncRequestEncryptedAppTicket>();
	Action->WorldContextObject = WorldContext;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncRequestEncryptedAppTicket::Activate()
{
	UESteamUserSubsystem* Subsystem = ESteamAsyncActionHelpers::GetUserSubsystem(WorldContextObject);
	if (!Subsystem)
	{
		Complete(false, FString());
		return;
	}

	UserSubsystem = Subsystem;
	Subsystem->OnEncryptedAppTicketReady.AddDynamic(this, &USteamAsyncRequestEncryptedAppTicket::HandleEncryptedAppTicketReady);

	if (!Subsystem->RequestEncryptedAppTicket())
	{
		Complete(false, FString());
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncRequestEncryptedAppTicket::HandleEncryptedAppTicketReady(bool bSuccess, const FString& HexTicket)
{
	// NOTE: shared delegate carries no request id; relies on Timeout + one-in-flight subsystem limit.
	Complete(bSuccess, HexTicket);
}

void USteamAsyncRequestEncryptedAppTicket::OnTimeoutFailure()
{
	Complete(false, FString());
}

void USteamAsyncRequestEncryptedAppTicket::Complete(bool bSuccess, const FString& HexTicket)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamUserSubsystem* Subsystem = UserSubsystem.Get())
	{
		Subsystem->OnEncryptedAppTicketReady.RemoveDynamic(this, &USteamAsyncRequestEncryptedAppTicket::HandleEncryptedAppTicketReady);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(HexTicket);
	}
	else
	{
		OnFailure.Broadcast(HexTicket);
	}

	SetReadyToDestroy();
}

USteamAsyncRequestStoreAuthURL* USteamAsyncRequestStoreAuthURL::RequestStoreAuthURL(UObject* WorldContext, const FString& RedirectUrl, float Timeout)
{
	USteamAsyncRequestStoreAuthURL* Action = NewObject<USteamAsyncRequestStoreAuthURL>();
	Action->WorldContextObject = WorldContext;
	Action->RedirectUrl = RedirectUrl;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncRequestStoreAuthURL::Activate()
{
	UESteamUserSubsystem* Subsystem = ESteamAsyncActionHelpers::GetUserSubsystem(WorldContextObject);
	if (!Subsystem)
	{
		Complete(false, FString());
		return;
	}

	UserSubsystem = Subsystem;
	Subsystem->OnStoreAuthURL.AddDynamic(this, &USteamAsyncRequestStoreAuthURL::HandleStoreAuthURL);

	if (!Subsystem->RequestStoreAuthURL(RedirectUrl))
	{
		Complete(false, FString());
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncRequestStoreAuthURL::HandleStoreAuthURL(bool bSuccess, const FString& Url)
{
	// NOTE: shared delegate carries no request id; relies on Timeout + one-in-flight subsystem usage.
	Complete(bSuccess, Url);
}

void USteamAsyncRequestStoreAuthURL::OnTimeoutFailure()
{
	Complete(false, FString());
}

void USteamAsyncRequestStoreAuthURL::Complete(bool bSuccess, const FString& Url)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamUserSubsystem* Subsystem = UserSubsystem.Get())
	{
		Subsystem->OnStoreAuthURL.RemoveDynamic(this, &USteamAsyncRequestStoreAuthURL::HandleStoreAuthURL);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(Url);
	}
	else
	{
		OnFailure.Broadcast(Url);
	}

	SetReadyToDestroy();
}

USteamAsyncGetMarketEligibility* USteamAsyncGetMarketEligibility::GetMarketEligibility(UObject* WorldContext, float Timeout)
{
	USteamAsyncGetMarketEligibility* Action = NewObject<USteamAsyncGetMarketEligibility>();
	Action->WorldContextObject = WorldContext;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncGetMarketEligibility::Activate()
{
	UESteamUserSubsystem* Subsystem = ESteamAsyncActionHelpers::GetUserSubsystem(WorldContextObject);
	if (!Subsystem)
	{
		Complete(false, false);
		return;
	}

	UserSubsystem = Subsystem;
	Subsystem->OnMarketEligibility.AddDynamic(this, &USteamAsyncGetMarketEligibility::HandleMarketEligibility);

	if (!Subsystem->GetMarketEligibility())
	{
		Complete(false, false);
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncGetMarketEligibility::HandleMarketEligibility(bool bAllowed)
{
	// NOTE: shared delegate carries no request id; relies on Timeout + one-in-flight subsystem usage.
	Complete(true, bAllowed);
}

void USteamAsyncGetMarketEligibility::OnTimeoutFailure()
{
	Complete(false, false);
}

void USteamAsyncGetMarketEligibility::Complete(bool bResultArrived, bool bAllowed)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamUserSubsystem* Subsystem = UserSubsystem.Get())
	{
		Subsystem->OnMarketEligibility.RemoveDynamic(this, &USteamAsyncGetMarketEligibility::HandleMarketEligibility);
	}

	if (bResultArrived)
	{
		OnSuccess.Broadcast(bAllowed);
	}
	else
	{
		OnFailure.Broadcast(false);
	}

	SetReadyToDestroy();
}

USteamAsyncGetDurationControl* USteamAsyncGetDurationControl::GetDurationControl(UObject* WorldContext, float Timeout)
{
	USteamAsyncGetDurationControl* Action = NewObject<USteamAsyncGetDurationControl>();
	Action->WorldContextObject = WorldContext;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncGetDurationControl::Activate()
{
	UESteamUserSubsystem* Subsystem = ESteamAsyncActionHelpers::GetUserSubsystem(WorldContextObject);
	if (!Subsystem)
	{
		Complete(false, false, 0, 0, 0);
		return;
	}

	UserSubsystem = Subsystem;
	Subsystem->OnDurationControl.AddDynamic(this, &USteamAsyncGetDurationControl::HandleDurationControl);

	if (!Subsystem->GetDurationControl())
	{
		Complete(false, false, 0, 0, 0);
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncGetDurationControl::HandleDurationControl(bool bApplicable, int32 SecondsLast5h, int32 SecondsToday, int32 SecondsRemaining)
{
	// NOTE: DurationControl_t also fires on Steam timers; the first event completes the node.
	Complete(true, bApplicable, SecondsLast5h, SecondsToday, SecondsRemaining);
}

void USteamAsyncGetDurationControl::OnTimeoutFailure()
{
	Complete(false, false, 0, 0, 0);
}

void USteamAsyncGetDurationControl::Complete(bool bResultArrived, bool bApplicable, int32 SecondsLast5h, int32 SecondsToday, int32 SecondsRemaining)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamUserSubsystem* Subsystem = UserSubsystem.Get())
	{
		Subsystem->OnDurationControl.RemoveDynamic(this, &USteamAsyncGetDurationControl::HandleDurationControl);
	}

	if (bResultArrived)
	{
		OnSuccess.Broadcast(bApplicable, SecondsLast5h, SecondsToday, SecondsRemaining);
	}
	else
	{
		OnFailure.Broadcast(false, 0, 0, 0);
	}

	SetReadyToDestroy();
}

USteamAsyncGetFileDetails* USteamAsyncGetFileDetails::GetFileDetails(UObject* WorldContext, const FString& FileName, float Timeout)
{
	USteamAsyncGetFileDetails* Action = NewObject<USteamAsyncGetFileDetails>();
	Action->WorldContextObject = WorldContext;
	Action->FileName = FileName;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncGetFileDetails::Activate()
{
	UESteamAppsSubsystem* Subsystem = ESteamAsyncActionHelpers::GetAppsSubsystem(WorldContextObject);
	if (!Subsystem)
	{
		Complete(false, 0, FString(), 0);
		return;
	}

	AppsSubsystem = Subsystem;
	Subsystem->OnFileDetails.AddDynamic(this, &USteamAsyncGetFileDetails::HandleFileDetails);

	if (!Subsystem->GetFileDetails(FileName))
	{
		Complete(false, 0, FString(), 0);
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncGetFileDetails::HandleFileDetails(bool bSuccess, int64 FileSize, const FString& Sha1Hex, int32 Flags)
{
	// NOTE: shared delegate carries no request id; relies on Timeout + one-in-flight subsystem usage.
	Complete(bSuccess, FileSize, Sha1Hex, Flags);
}

void USteamAsyncGetFileDetails::OnTimeoutFailure()
{
	Complete(false, 0, FString(), 0);
}

void USteamAsyncGetFileDetails::Complete(bool bSuccess, int64 FileSize, const FString& Sha1Hex, int32 Flags)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamAppsSubsystem* Subsystem = AppsSubsystem.Get())
	{
		Subsystem->OnFileDetails.RemoveDynamic(this, &USteamAsyncGetFileDetails::HandleFileDetails);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(FileSize, Sha1Hex, Flags);
	}
	else
	{
		OnFailure.Broadcast(FileSize, Sha1Hex, Flags);
	}

	SetReadyToDestroy();
}
