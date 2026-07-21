// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "InventoryService/ESteamWebInventoryServiceSubsystem.h"
#include "Core/ESteamWebSettings.h"

void UESteamWebInventoryServiceSubsystem::AddItem(int32 AppId, FString SteamId, const TArray<int32>& ItemDefIds, FString ItemPropsJson, bool bNotify, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IInventoryService"), TEXT("AddItem"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	for (int32 Index = 0; Index < ItemDefIds.Num(); ++Index)
	{
		Request.AddParam(FString::Printf(TEXT("itemdefid[%d]"), Index), ItemDefIds[Index]);
	}
	if (!ItemPropsJson.IsEmpty())
	{
		Request.AddParam(TEXT("itempropsjson"), ItemPropsJson);
	}
	if (bNotify)
	{
		Request.AddParam(TEXT("notify"), true);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebInventoryServiceSubsystem::AddPromoItem(int32 AppId, FString SteamId, int32 ItemDefId, FString ItemPropsJson, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IInventoryService"), TEXT("AddPromoItem"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	Request.AddParam(TEXT("itemdefid"), ItemDefId);
	if (!ItemPropsJson.IsEmpty())
	{
		Request.AddParam(TEXT("itempropsjson"), ItemPropsJson);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebInventoryServiceSubsystem::ConsumeItem(int32 AppId, FString SteamId, FString ItemId, FString Quantity, FString RequestId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IInventoryService"), TEXT("ConsumeItem"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	Request.AddParam(TEXT("itemid"), ItemId);
	Request.AddParam(TEXT("quantity"), Quantity);
	if (!RequestId.IsEmpty())
	{
		Request.AddParam(TEXT("requestid"), RequestId);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebInventoryServiceSubsystem::ExchangeItem(int32 AppId, FString SteamId, const TArray<FString>& MaterialItemIds, const TArray<int32>& MaterialQuantities, int32 OutputItemDefId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IInventoryService"), TEXT("ExchangeItem"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	for (int32 Index = 0; Index < MaterialItemIds.Num(); ++Index)
	{
		Request.AddParam(FString::Printf(TEXT("materialsitemid[%d]"), Index), MaterialItemIds[Index]);
	}
	for (int32 Index = 0; Index < MaterialQuantities.Num(); ++Index)
	{
		Request.AddParam(FString::Printf(TEXT("materialsquantity[%d]"), Index), MaterialQuantities[Index]);
	}
	Request.AddParam(TEXT("outputitemdefid"), OutputItemDefId);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebInventoryServiceSubsystem::GetInventory(int32 AppId, FString SteamId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IInventoryService"), TEXT("GetInventory"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebInventoryServiceSubsystem::GetItemDefs(int32 AppId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IInventoryService"), TEXT("GetItemDefs"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebInventoryServiceSubsystem::GetPriceSheet(int32 Ecurrency, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IInventoryService"), TEXT("GetPriceSheet"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	if (Ecurrency >= 0)
	{
		Request.AddParam(TEXT("ecurrency"), Ecurrency);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebInventoryServiceSubsystem::ModifyItems(int32 AppId, FString SteamId, FString UpdatesJson, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IInventoryService"), TEXT("ModifyItems"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	// Raw JSON array of update objects — see the header comment for the encoding caveat.
	Request.AddParam(TEXT("updates"), UpdatesJson);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebInventoryServiceSubsystem::Consolidate(int32 AppId, FString SteamId, const TArray<int32>& ItemDefIds, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IInventoryService"), TEXT("Consolidate"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	for (int32 Index = 0; Index < ItemDefIds.Num(); ++Index)
	{
		Request.AddParam(FString::Printf(TEXT("itemdefid[%d]"), Index), ItemDefIds[Index]);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebInventoryServiceSubsystem::GetQuantity(int32 AppId, FString SteamId, const TArray<int32>& ItemDefIds, bool bForce, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IInventoryService"), TEXT("GetQuantity"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	for (int32 Index = 0; Index < ItemDefIds.Num(); ++Index)
	{
		Request.AddParam(FString::Printf(TEXT("itemdefid[%d]"), Index), ItemDefIds[Index]);
	}
	if (bForce)
	{
		Request.AddParam(TEXT("force"), true);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}
