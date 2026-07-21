// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "MicroTxn/ESteamWebMicroTxnSubsystem.h"
#include "Core/ESteamWebSettings.h"

FString UESteamWebMicroTxnSubsystem::GetMicroTxnInterface() const
{
	return GetWebSettings()->bMicroTxnSandbox ? TEXT("ISteamMicroTxnSandbox") : TEXT("ISteamMicroTxn");
}

void UESteamWebMicroTxnSubsystem::GetUserInfo(FString SteamId, FString IpAddress, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(GetMicroTxnInterface(), TEXT("GetUserInfo"), 2, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	if (!IpAddress.IsEmpty())
	{
		Request.AddParam(TEXT("ipaddress"), IpAddress);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebMicroTxnSubsystem::InitTxn(FString OrderId, FString SteamId, int32 AppId, FString Language, FString Currency, FString UserSession, FString IpAddress, const TArray<int32>& ItemIds, const TArray<int32>& Quantities, const TArray<FString>& Amounts, const TArray<FString>& Descriptions, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(GetMicroTxnInterface(), TEXT("InitTxn"), 3, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("orderid"), OrderId);
	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("itemcount"), ItemIds.Num());
	Request.AddParam(TEXT("language"), Language);
	Request.AddParam(TEXT("currency"), Currency);
	if (!UserSession.IsEmpty())
	{
		Request.AddParam(TEXT("usersession"), UserSession);
	}
	if (!IpAddress.IsEmpty())
	{
		Request.AddParam(TEXT("ipaddress"), IpAddress);
	}

	// Parallel line-item arrays -> indexed itemid[N]/qty[N]/amount[N]/description[N] params.
	for (int32 Index = 0; Index < ItemIds.Num(); ++Index)
	{
		Request.AddParam(FString::Printf(TEXT("itemid[%d]"), Index), ItemIds[Index]);
		Request.AddParam(FString::Printf(TEXT("qty[%d]"), Index), Quantities.IsValidIndex(Index) ? Quantities[Index] : 1);
		Request.AddParam(FString::Printf(TEXT("amount[%d]"), Index), Amounts.IsValidIndex(Index) ? Amounts[Index] : FString());
		Request.AddParam(FString::Printf(TEXT("description[%d]"), Index), Descriptions.IsValidIndex(Index) ? Descriptions[Index] : FString());
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebMicroTxnSubsystem::FinalizeTxn(FString OrderId, int32 AppId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(GetMicroTxnInterface(), TEXT("FinalizeTxn"), 2, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("orderid"), OrderId);
	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebMicroTxnSubsystem::QueryTxn(int32 AppId, FString OrderId, FString TransId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(GetMicroTxnInterface(), TEXT("QueryTxn"), 3, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	if (!OrderId.IsEmpty())
	{
		Request.AddParam(TEXT("orderid"), OrderId);
	}
	if (!TransId.IsEmpty())
	{
		Request.AddParam(TEXT("transid"), TransId);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebMicroTxnSubsystem::RefundTxn(FString OrderId, int32 AppId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(GetMicroTxnInterface(), TEXT("RefundTxn"), 2, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("orderid"), OrderId);
	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebMicroTxnSubsystem::GetReport(int32 AppId, FString Time, FString Type, int32 MaxResults, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(GetMicroTxnInterface(), TEXT("GetReport"), 5, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("time"), Time);
	if (!Type.IsEmpty())
	{
		Request.AddParam(TEXT("type"), Type);
	}
	if (MaxResults > 0)
	{
		Request.AddParam(TEXT("maxresults"), MaxResults);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebMicroTxnSubsystem::GetUserAgreementInfo(int32 AppId, FString SteamId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(GetMicroTxnInterface(), TEXT("GetUserAgreementInfo"), 2, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebMicroTxnSubsystem::ProcessAgreement(int32 AppId, FString SteamId, FString AgreementId, FString OrderId, FString Amount, FString Currency, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(GetMicroTxnInterface(), TEXT("ProcessAgreement"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("orderid"), OrderId);
	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	Request.AddParam(TEXT("agreementid"), AgreementId);
	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("amount"), Amount);
	Request.AddParam(TEXT("currency"), Currency);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebMicroTxnSubsystem::AdjustAgreement(int32 AppId, FString SteamId, FString AgreementId, FString NextProcessDate, FString OldNextProcessDate, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(GetMicroTxnInterface(), TEXT("AdjustAgreement"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	Request.AddParam(TEXT("agreementid"), AgreementId);
	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("nextprocessdate"), NextProcessDate);
	if (!OldNextProcessDate.IsEmpty())
	{
		Request.AddParam(TEXT("oldnextprocessdate"), OldNextProcessDate);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebMicroTxnSubsystem::CancelAgreement(int32 AppId, FString SteamId, FString AgreementId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(GetMicroTxnInterface(), TEXT("CancelAgreement"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	Request.AddParam(TEXT("agreementid"), AgreementId);
	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));

	SendWebRequest(MoveTemp(Request), OnResponse);
}
