// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "UserAuth/ESteamWebUserAuthSubsystem.h"
#include "Core/ESteamWebSettings.h"

void UESteamWebUserAuthSubsystem::AuthenticateUserTicket(int32 AppId, FString HexTicket, FString Identity, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamUserAuth"), TEXT("AuthenticateUserTicket"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("ticket"), HexTicket);
	if (!Identity.IsEmpty())
	{
		Request.AddParam(TEXT("identity"), Identity);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebUserAuthSubsystem::AuthenticateUser(FString SteamId, FString SessionKeyHex, FString EncryptedLoginKeyHex, const FOnSteamWebResponse& OnResponse)
{
	// The encrypted session key is the credential — this endpoint documents no "key" param.
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamUserAuth"), TEXT("AuthenticateUser"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true, /*bRequiresApiKey*/ false);

	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	Request.AddParam(TEXT("sessionkey"), SessionKeyHex);
	Request.AddParam(TEXT("encrypted_loginkey"), EncryptedLoginKeyHex);

	SendWebRequest(MoveTemp(Request), OnResponse);
}
