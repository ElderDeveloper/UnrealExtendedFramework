// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "BlueprintActions/User/ESteamUserAsyncActions.h"
#include "User/ESteamUserSubsystem.h"
#include "Apps/ESteamAppsSubsystem.h"

USteamAsyncGetWebApiAuthTicket* USteamAsyncGetWebApiAuthTicket::GetWebApiAuthTicket(UObject* WorldContext, const FString& RemoteServiceIdentity, float Timeout)
{
	USteamAsyncGetWebApiAuthTicket* Action = NewObject<USteamAsyncGetWebApiAuthTicket>();
	Action->RemoteServiceIdentity = RemoteServiceIdentity;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncGetWebApiAuthTicket::ExecuteAction()
{
	UESteamUserSubsystem* Subsystem = RequireGameInstanceSubsystem<UESteamUserSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	UserSubsystem = Subsystem;
	Subsystem->OnWebApiAuthTicketReady.AddDynamic(this, &USteamAsyncGetWebApiAuthTicket::HandleWebApiAuthTicketReady);

	CallWithContext([&] { RequestHandle = Subsystem->RequestWebApiAuthTicket(RemoteServiceIdentity); });
	if (RequestHandle == 0)
	{
		Complete(false, FString());
	}
}

void USteamAsyncGetWebApiAuthTicket::HandleWebApiAuthTicketReady(bool bSuccess, int32 TicketHandle, const FString& HexTicket)
{
	// Other requests may be in flight on the shared subsystem delegate; only react to ours.
	if (!IsResultForThisCall() || TicketHandle != RequestHandle)
	{
		return;
	}
	Complete(bSuccess, HexTicket);
}

void USteamAsyncGetWebApiAuthTicket::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(FString());
}

void USteamAsyncGetWebApiAuthTicket::CancelWork()
{
	if (UESteamUserSubsystem* Subsystem = UserSubsystem.Get())
	{
		Subsystem->OnWebApiAuthTicketReady.RemoveDynamic(this, &USteamAsyncGetWebApiAuthTicket::HandleWebApiAuthTicketReady);
	}
}

void USteamAsyncGetWebApiAuthTicket::Complete(bool bSuccess, const FString& HexTicket)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(HexTicket);
	}
	else
	{
		OnFailure.Broadcast(HexTicket);
	}
}

USteamAsyncRequestEncryptedAppTicket* USteamAsyncRequestEncryptedAppTicket::RequestEncryptedAppTicket(UObject* WorldContext, float Timeout)
{
	USteamAsyncRequestEncryptedAppTicket* Action = NewObject<USteamAsyncRequestEncryptedAppTicket>();
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncRequestEncryptedAppTicket::ExecuteAction()
{
	UESteamUserSubsystem* Subsystem = RequireGameInstanceSubsystem<UESteamUserSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	UserSubsystem = Subsystem;
	Subsystem->OnEncryptedAppTicketReady.AddDynamic(this, &USteamAsyncRequestEncryptedAppTicket::HandleEncryptedAppTicketReady);

	bool bStarted = false;
	CallWithContext([&] { bStarted = Subsystem->RequestEncryptedAppTicket(); });
	if (!bStarted)
	{
		Complete(false, FString());
	}
}

void USteamAsyncRequestEncryptedAppTicket::HandleEncryptedAppTicketReady(bool bSuccess, const FString& HexTicket)
{
	// The shared delegate carries no request id; the subsystem tags the result with the call
	// context of its request.
	if (!IsResultForThisCall())
	{
		return;
	}
	Complete(bSuccess, HexTicket);
}

void USteamAsyncRequestEncryptedAppTicket::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(FString());
}

void USteamAsyncRequestEncryptedAppTicket::CancelWork()
{
	if (UESteamUserSubsystem* Subsystem = UserSubsystem.Get())
	{
		Subsystem->OnEncryptedAppTicketReady.RemoveDynamic(this, &USteamAsyncRequestEncryptedAppTicket::HandleEncryptedAppTicketReady);
	}
}

void USteamAsyncRequestEncryptedAppTicket::Complete(bool bSuccess, const FString& HexTicket)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(HexTicket);
	}
	else
	{
		OnFailure.Broadcast(HexTicket);
	}
}

USteamAsyncRequestStoreAuthURL* USteamAsyncRequestStoreAuthURL::RequestStoreAuthURL(UObject* WorldContext, const FString& RedirectUrl, float Timeout)
{
	USteamAsyncRequestStoreAuthURL* Action = NewObject<USteamAsyncRequestStoreAuthURL>();
	Action->RedirectUrl = RedirectUrl;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncRequestStoreAuthURL::ExecuteAction()
{
	UESteamUserSubsystem* Subsystem = RequireGameInstanceSubsystem<UESteamUserSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	UserSubsystem = Subsystem;
	Subsystem->OnStoreAuthURL.AddDynamic(this, &USteamAsyncRequestStoreAuthURL::HandleStoreAuthURL);

	bool bStarted = false;
	CallWithContext([&] { bStarted = Subsystem->RequestStoreAuthURL(RedirectUrl); });
	if (!bStarted)
	{
		Complete(false, FString());
	}
}

void USteamAsyncRequestStoreAuthURL::HandleStoreAuthURL(bool bSuccess, const FString& Url)
{
	// The shared delegate carries no request id; the subsystem tags the result with the call
	// context of its request.
	if (!IsResultForThisCall())
	{
		return;
	}
	Complete(bSuccess, Url);
}

void USteamAsyncRequestStoreAuthURL::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(FString());
}

void USteamAsyncRequestStoreAuthURL::CancelWork()
{
	if (UESteamUserSubsystem* Subsystem = UserSubsystem.Get())
	{
		Subsystem->OnStoreAuthURL.RemoveDynamic(this, &USteamAsyncRequestStoreAuthURL::HandleStoreAuthURL);
	}
}

void USteamAsyncRequestStoreAuthURL::Complete(bool bSuccess, const FString& Url)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(Url);
	}
	else
	{
		OnFailure.Broadcast(Url);
	}
}

USteamAsyncGetMarketEligibility* USteamAsyncGetMarketEligibility::GetMarketEligibility(UObject* WorldContext, float Timeout)
{
	USteamAsyncGetMarketEligibility* Action = NewObject<USteamAsyncGetMarketEligibility>();
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncGetMarketEligibility::ExecuteAction()
{
	UESteamUserSubsystem* Subsystem = RequireGameInstanceSubsystem<UESteamUserSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	UserSubsystem = Subsystem;
	Subsystem->OnMarketEligibility.AddDynamic(this, &USteamAsyncGetMarketEligibility::HandleMarketEligibility);

	bool bStarted = false;
	CallWithContext([&] { bStarted = Subsystem->GetMarketEligibility(); });
	if (!bStarted)
	{
		Complete(false, false);
	}
}

void USteamAsyncGetMarketEligibility::HandleMarketEligibility(bool bAllowed)
{
	// The shared delegate carries no request id; the subsystem tags the result with the call
	// context of its request.
	if (!IsResultForThisCall())
	{
		return;
	}
	Complete(true, bAllowed);
}

void USteamAsyncGetMarketEligibility::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(false);
}

void USteamAsyncGetMarketEligibility::CancelWork()
{
	if (UESteamUserSubsystem* Subsystem = UserSubsystem.Get())
	{
		Subsystem->OnMarketEligibility.RemoveDynamic(this, &USteamAsyncGetMarketEligibility::HandleMarketEligibility);
	}
}

void USteamAsyncGetMarketEligibility::Complete(bool bResultArrived, bool bAllowed)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bResultArrived)
	{
		OnSuccess.Broadcast(bAllowed);
	}
	else
	{
		OnFailure.Broadcast(false);
	}
}

USteamAsyncGetDurationControl* USteamAsyncGetDurationControl::GetDurationControl(UObject* WorldContext, float Timeout)
{
	USteamAsyncGetDurationControl* Action = NewObject<USteamAsyncGetDurationControl>();
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncGetDurationControl::ExecuteAction()
{
	UESteamUserSubsystem* Subsystem = RequireGameInstanceSubsystem<UESteamUserSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	UserSubsystem = Subsystem;
	Subsystem->OnDurationControl.AddDynamic(this, &USteamAsyncGetDurationControl::HandleDurationControl);

	bool bStarted = false;
	CallWithContext([&] { bStarted = Subsystem->GetDurationControl(); });
	if (!bStarted)
	{
		Complete(false, false, 0, 0, 0);
	}
}

void USteamAsyncGetDurationControl::HandleDurationControl(bool bApplicable, int32 SecondsLast5h, int32 SecondsToday, int32 SecondsRemaining)
{
	// DurationControl_t also fires on Steam timers (no call context); the first event that is not
	// another node's result completes the node.
	if (!IsResultForThisCall())
	{
		return;
	}
	Complete(true, bApplicable, SecondsLast5h, SecondsToday, SecondsRemaining);
}

void USteamAsyncGetDurationControl::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(false, 0, 0, 0);
}

void USteamAsyncGetDurationControl::CancelWork()
{
	if (UESteamUserSubsystem* Subsystem = UserSubsystem.Get())
	{
		Subsystem->OnDurationControl.RemoveDynamic(this, &USteamAsyncGetDurationControl::HandleDurationControl);
	}
}

void USteamAsyncGetDurationControl::Complete(bool bResultArrived, bool bApplicable, int32 SecondsLast5h, int32 SecondsToday, int32 SecondsRemaining)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bResultArrived)
	{
		OnSuccess.Broadcast(bApplicable, SecondsLast5h, SecondsToday, SecondsRemaining);
	}
	else
	{
		OnFailure.Broadcast(false, 0, 0, 0);
	}
}

USteamAsyncGetFileDetails* USteamAsyncGetFileDetails::GetFileDetails(UObject* WorldContext, const FString& FileName, float Timeout)
{
	USteamAsyncGetFileDetails* Action = NewObject<USteamAsyncGetFileDetails>();
	Action->FileName = FileName;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncGetFileDetails::ExecuteAction()
{
	UESteamAppsSubsystem* Subsystem = RequireGameInstanceSubsystem<UESteamAppsSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	AppsSubsystem = Subsystem;
	Subsystem->OnFileDetails.AddDynamic(this, &USteamAsyncGetFileDetails::HandleFileDetails);

	bool bStarted = false;
	CallWithContext([&] { bStarted = Subsystem->GetFileDetails(FileName); });
	if (!bStarted)
	{
		Complete(false, 0, FString(), 0);
	}
}

void USteamAsyncGetFileDetails::HandleFileDetails(bool bSuccess, int64 FileSize, const FString& Sha1Hex, int32 Flags)
{
	// The shared delegate carries no request id; the subsystem tags the result with the call
	// context of its request.
	if (!IsResultForThisCall())
	{
		return;
	}
	Complete(bSuccess, FileSize, Sha1Hex, Flags);
}

void USteamAsyncGetFileDetails::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(0, FString(), 0);
}

void USteamAsyncGetFileDetails::CancelWork()
{
	if (UESteamAppsSubsystem* Subsystem = AppsSubsystem.Get())
	{
		Subsystem->OnFileDetails.RemoveDynamic(this, &USteamAsyncGetFileDetails::HandleFileDetails);
	}
}

void USteamAsyncGetFileDetails::Complete(bool bSuccess, int64 FileSize, const FString& Sha1Hex, int32 Flags)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(FileSize, Sha1Hex, Flags);
	}
	else
	{
		OnFailure.Broadcast(FileSize, Sha1Hex, Flags);
	}
}
