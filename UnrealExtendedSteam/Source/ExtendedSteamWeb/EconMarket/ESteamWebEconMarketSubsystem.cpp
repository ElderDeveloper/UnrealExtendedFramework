// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EconMarket/ESteamWebEconMarketSubsystem.h"
#include "Core/ESteamWebSettings.h"

void UESteamWebEconMarketSubsystem::GetMarketEligibility(FString SteamId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IEconMarketService"), TEXT("GetMarketEligibility"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebEconMarketSubsystem::CancelAppListingsForUser(int32 AppId, FString SteamId, bool bSynchronous, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IEconMarketService"), TEXT("CancelAppListingsForUser"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	Request.AddParam(TEXT("synchronous"), bSynchronous);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebEconMarketSubsystem::GetAssetID(int32 AppId, int64 ListingId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IEconMarketService"), TEXT("GetAssetID"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("listingid"), ListingId);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebEconMarketSubsystem::GetPopular(FString Language, int32 Rows, int32 Start, int32 FilterAppId, int32 ECurrency, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IEconMarketService"), TEXT("GetPopular"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	if (!Language.IsEmpty())
	{
		Request.AddParam(TEXT("language"), Language);
	}
	if (Rows > 0)
	{
		Request.AddParam(TEXT("rows"), Rows);
	}
	Request.AddParam(TEXT("start"), Start > 0 ? Start : 0);
	if (FilterAppId > 0)
	{
		Request.AddParam(TEXT("filter_appid"), FilterAppId);
	}
	if (ECurrency > 0)
	{
		Request.AddParam(TEXT("ecurrency"), ECurrency);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}
