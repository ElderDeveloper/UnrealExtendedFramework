// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAccountLinkingSubsystem.h"
#include "UnrealExtendedPlayFab.h"
#include "Dom/JsonObject.h"
#include "Misc/Guid.h"

void UEPFAccountLinkingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEPFAccountLinkingSubsystem::Deinitialize()
{
	Super::Deinitialize();
}


// ── Link Steam ───────────────────────────────────────────────────────────────

void UEPFAccountLinkingSubsystem::LinkSteamAccount(const FString& SteamTicket, bool bForceLink)
{
	if (SteamTicket.IsEmpty()) { OnAccountLinked.Broadcast(FEPFResult::Failure(TEXT("SteamTicket cannot be empty")), TEXT("Steam")); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("SteamTicket"), SteamTicket);
	Body->SetBoolField(TEXT("ForceLink"), bForceLink);

	SendPlayFabRequestDetailed(TEXT("/Client/LinkSteamAccount"), Body, true,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			if (Result.bSuccess) UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFAccountLinking — Steam linked"));
			OnAccountLinked.Broadcast(Result, TEXT("Steam"));
		}));
}


// ── Link Custom ID ───────────────────────────────────────────────────────────

void UEPFAccountLinkingSubsystem::LinkCustomId(const FString& CustomId, bool bForceLink)
{
	if (CustomId.IsEmpty()) { OnAccountLinked.Broadcast(FEPFResult::Failure(TEXT("CustomId cannot be empty")), TEXT("CustomId")); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("CustomId"), CustomId);
	Body->SetBoolField(TEXT("ForceLink"), bForceLink);

	SendPlayFabRequestDetailed(TEXT("/Client/LinkCustomID"), Body, true,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			if (Result.bSuccess) UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFAccountLinking — CustomId linked"));
			OnAccountLinked.Broadcast(Result, TEXT("CustomId"));
		}));
}


// ── Link Device ID ───────────────────────────────────────────────────────────

void UEPFAccountLinkingSubsystem::LinkDeviceId(bool bForceLink)
{
	FString DeviceId = FPlatformMisc::GetDeviceId();
	if (DeviceId.IsEmpty()) DeviceId = FGuid::NewGuid().ToString();

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("CustomId"), DeviceId);
	Body->SetBoolField(TEXT("ForceLink"), bForceLink);

	SendPlayFabRequestDetailed(TEXT("/Client/LinkCustomID"), Body, true,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			if (Result.bSuccess) UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFAccountLinking — DeviceId linked"));
			OnAccountLinked.Broadcast(Result, TEXT("DeviceId"));
		}));
}


// ── Unlink Steam ─────────────────────────────────────────────────────────────

void UEPFAccountLinkingSubsystem::UnlinkSteamAccount()
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();

	SendPlayFabRequestDetailed(TEXT("/Client/UnlinkSteamAccount"), Body, true,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			if (Result.bSuccess) UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFAccountLinking — Steam unlinked"));
			OnAccountUnlinked.Broadcast(Result, TEXT("Steam"));
		}));
}


// ── Unlink Custom ID ─────────────────────────────────────────────────────────

void UEPFAccountLinkingSubsystem::UnlinkCustomId(const FString& CustomId)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	if (!CustomId.IsEmpty()) Body->SetStringField(TEXT("CustomId"), CustomId);

	SendPlayFabRequestDetailed(TEXT("/Client/UnlinkCustomID"), Body, true,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			if (Result.bSuccess) UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFAccountLinking — CustomId unlinked"));
			OnAccountUnlinked.Broadcast(Result, TEXT("CustomId"));
		}));
}


// ── Unlink Device ID ─────────────────────────────────────────────────────────

void UEPFAccountLinkingSubsystem::UnlinkDeviceId()
{
	UnlinkCustomId(FPlatformMisc::GetDeviceId());
}
