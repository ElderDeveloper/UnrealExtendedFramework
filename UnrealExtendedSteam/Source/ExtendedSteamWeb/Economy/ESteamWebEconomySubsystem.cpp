// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Economy/ESteamWebEconomySubsystem.h"
#include "Core/ESteamWebSettings.h"

void UESteamWebEconomySubsystem::GetAssetClassInfo(int32 AppId, FString Language, const TArray<FString>& ClassIds, const TArray<FString>& InstanceIds, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamEconomy"), TEXT("GetAssetClassInfo"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	if (!Language.IsEmpty())
	{
		Request.AddParam(TEXT("language"), Language);
	}
	Request.AddParam(TEXT("class_count"), ClassIds.Num());

	// Parallel arrays -> numeric-suffixed classid0/instanceid0 params.
	for (int32 Index = 0; Index < ClassIds.Num(); ++Index)
	{
		Request.AddParam(FString::Printf(TEXT("classid%d"), Index), ClassIds[Index]);
		if (InstanceIds.IsValidIndex(Index) && !InstanceIds[Index].IsEmpty())
		{
			Request.AddParam(FString::Printf(TEXT("instanceid%d"), Index), InstanceIds[Index]);
		}
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebEconomySubsystem::GetAssetPrices(int32 AppId, FString Currency, FString Language, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamEconomy"), TEXT("GetAssetPrices"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	if (!Currency.IsEmpty())
	{
		Request.AddParam(TEXT("currency"), Currency);
	}
	if (!Language.IsEmpty())
	{
		Request.AddParam(TEXT("language"), Language);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebEconomySubsystem::GetExportedAssetsForUser(FString SteamId, int32 AppId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamEconomy"), TEXT("GetExportedAssetsForUser"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebEconomySubsystem::GetMarketPrices(int32 AppId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamEconomy"), TEXT("GetMarketPrices"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebEconomySubsystem::CanTrade(int32 AppId, FString SteamId, FString TargetSteamId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamEconomy"), TEXT("CanTrade"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	Request.AddParam(TEXT("targetid"), TargetSteamId);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebEconomySubsystem::StartTrade(int32 AppId, FString PartyA, FString PartyB, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamEconomy"), TEXT("StartTrade"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("partya"), PartyA);
	Request.AddParam(TEXT("partyb"), PartyB);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebEconomySubsystem::StartAssetTransaction(int32 AppId, FString SteamId, const TArray<FString>& AssetIds, const TArray<int32>& AssetQuantities, FString Currency, FString Language, FString IpAddress, FString Referrer, bool bClientAuth, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamEconomy"), TEXT("StartAssetTransaction"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));

	// Parallel arrays -> numeric-suffixed assetid0/assetquantity0 params.
	for (int32 Index = 0; Index < AssetIds.Num(); ++Index)
	{
		Request.AddParam(FString::Printf(TEXT("assetid%d"), Index), AssetIds[Index]);
		Request.AddParam(FString::Printf(TEXT("assetquantity%d"), Index), AssetQuantities.IsValidIndex(Index) ? AssetQuantities[Index] : 1);
	}

	Request.AddParam(TEXT("currency"), Currency);
	Request.AddParam(TEXT("language"), Language);
	Request.AddParam(TEXT("ipaddress"), IpAddress);
	if (!Referrer.IsEmpty())
	{
		Request.AddParam(TEXT("referrer"), Referrer);
	}
	if (bClientAuth)
	{
		Request.AddParam(TEXT("clientauth"), true);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebEconomySubsystem::FinalizeAssetTransaction(int32 AppId, FString SteamId, FString TxnId, FString Language, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamEconomy"), TEXT("FinalizeAssetTransaction"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	Request.AddParam(TEXT("txnid"), TxnId);
	Request.AddParam(TEXT("language"), Language);

	SendWebRequest(MoveTemp(Request), OnResponse);
}
