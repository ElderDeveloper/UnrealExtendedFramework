// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EconService/ESteamWebEconServiceSubsystem.h"
#include "Core/ESteamWebSettings.h"

void UESteamWebEconServiceSubsystem::GetTradeHistory(int32 MaxTrades, int64 StartAfterTime, FString StartAfterTradeId, bool bNavigatingBack, bool bGetDescriptions, FString Language, bool bIncludeFailed, bool bIncludeTotal, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IEconService"), TEXT("GetTradeHistory"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	if (MaxTrades > 0)
	{
		Request.AddParam(TEXT("max_trades"), MaxTrades);
	}
	if (StartAfterTime > 0)
	{
		Request.AddParam(TEXT("start_after_time"), StartAfterTime);
	}
	if (!StartAfterTradeId.IsEmpty())
	{
		Request.AddParam(TEXT("start_after_tradeid"), StartAfterTradeId);
	}
	if (bNavigatingBack)
	{
		Request.AddParam(TEXT("navigating_back"), true);
	}
	if (bGetDescriptions)
	{
		Request.AddParam(TEXT("get_descriptions"), true);
	}
	if (!Language.IsEmpty())
	{
		Request.AddParam(TEXT("language"), Language);
	}
	if (bIncludeFailed)
	{
		Request.AddParam(TEXT("include_failed"), true);
	}
	if (bIncludeTotal)
	{
		Request.AddParam(TEXT("include_total"), true);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebEconServiceSubsystem::GetTradeOffers(bool bGetSentOffers, bool bGetReceivedOffers, bool bGetDescriptions, FString Language, bool bActiveOnly, bool bHistoricalOnly, int64 TimeHistoricalCutoff, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IEconService"), TEXT("GetTradeOffers"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	if (bGetSentOffers)
	{
		Request.AddParam(TEXT("get_sent_offers"), true);
	}
	if (bGetReceivedOffers)
	{
		Request.AddParam(TEXT("get_received_offers"), true);
	}
	if (bGetDescriptions)
	{
		Request.AddParam(TEXT("get_descriptions"), true);
	}
	if (!Language.IsEmpty())
	{
		Request.AddParam(TEXT("language"), Language);
	}
	if (bActiveOnly)
	{
		Request.AddParam(TEXT("active_only"), true);
	}
	if (bHistoricalOnly)
	{
		Request.AddParam(TEXT("historical_only"), true);
	}
	if (TimeHistoricalCutoff > 0)
	{
		Request.AddParam(TEXT("time_historical_cutoff"), TimeHistoricalCutoff);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebEconServiceSubsystem::GetTradeOffer(FString TradeOfferId, FString Language, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IEconService"), TEXT("GetTradeOffer"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("tradeofferid"), TradeOfferId);
	if (!Language.IsEmpty())
	{
		Request.AddParam(TEXT("language"), Language);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebEconServiceSubsystem::GetTradeOffersSummary(int64 TimeLastVisit, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IEconService"), TEXT("GetTradeOffersSummary"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	if (TimeLastVisit > 0)
	{
		Request.AddParam(TEXT("time_last_visit"), TimeLastVisit);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebEconServiceSubsystem::DeclineTradeOffer(FString TradeOfferId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IEconService"), TEXT("DeclineTradeOffer"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("tradeofferid"), TradeOfferId);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebEconServiceSubsystem::CancelTradeOffer(FString TradeOfferId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IEconService"), TEXT("CancelTradeOffer"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("tradeofferid"), TradeOfferId);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebEconServiceSubsystem::GetTradeStatus(FString TradeId, bool bGetDescriptions, FString Language, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IEconService"), TEXT("GetTradeStatus"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("tradeid"), TradeId);
	if (bGetDescriptions)
	{
		Request.AddParam(TEXT("get_descriptions"), true);
	}
	if (!Language.IsEmpty())
	{
		Request.AddParam(TEXT("language"), Language);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebEconServiceSubsystem::FlushInventoryCache(FString SteamId, int32 AppId, int32 ContextId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IEconService"), TEXT("FlushInventoryCache"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("contextid"), ContextId);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebEconServiceSubsystem::FlushContextCache(int32 AppId, int32 ContextId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IEconService"), TEXT("FlushContextCache"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("contextid"), ContextId);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebEconServiceSubsystem::FlushAssetAppearanceCache(int32 AppId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IEconService"), TEXT("FlushAssetAppearanceCache"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));

	SendWebRequest(MoveTemp(Request), OnResponse);
}
