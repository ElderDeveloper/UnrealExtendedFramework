// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Core/ESteamWebSubsystem.h"
#include "Core/ESteamWebSettings.h"

void UESteamWebSubsystem::SendWebRequest(FESteamWebRequest&& Request, const FOnSteamWebResponse& Callback)
{
	FESteamWebClient::Send(Request, FOnSteamWebResponseNative::CreateLambda(
		[Callback](const FString& Json, bool bSuccess, int32 HttpCode)
		{
			Callback.ExecuteIfBound(Json, bSuccess, HttpCode);
		}));
}

FESteamWebRequest UESteamWebSubsystem::MakeRequest(const FString& Interface, const FString& Method, int32 Version,
	EESteamWebVerb Verb, bool bUsePartnerHost, bool bRequiresApiKey) const
{
	FESteamWebRequest Request;
	Request.Interface = Interface;
	Request.Method = Method;
	Request.Version = Version;
	Request.Verb = Verb;
	Request.bUsePartnerHost = bUsePartnerHost;
	Request.bRequiresApiKey = bRequiresApiKey;
	return Request;
}

int32 UESteamWebSubsystem::ResolveAppId(int32 AppId) const
{
	return AppId > 0 ? AppId : GetWebSettings()->AppId;
}

FString UESteamWebSubsystem::ResolveSteamId(const FString& SteamId) const
{
	return SteamId.IsEmpty() ? GetWebSettings()->DevSteamId : SteamId;
}

const UESteamWebSettings* UESteamWebSubsystem::GetWebSettings() const
{
	return UESteamWebSettings::Get();
}
