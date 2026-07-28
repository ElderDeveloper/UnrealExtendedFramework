// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "UnrealExtendedAtlassian.h"

#include "ExtendedAtlassianClient.h"
#include "ExtendedAtlassianLog.h"

#define LOCTEXT_NAMESPACE "FUnrealExtendedAtlassianModule"

FUnrealExtendedAtlassianModule* FUnrealExtendedAtlassianModule::Instance = nullptr;

FUnrealExtendedAtlassianModule::~FUnrealExtendedAtlassianModule() = default;

void FUnrealExtendedAtlassianModule::StartupModule()
{
	Instance = this;

	Client = MakeShared<FExtendedAtlassianClient>();
	Client->ReloadCredentials();

	UE_LOG(LogExtendedAtlassian, Verbose, TEXT("Extended Atlassian transport module started."));
}

void FUnrealExtendedAtlassianModule::ShutdownModule()
{
	Client.Reset();
	Instance = nullptr;
}

FUnrealExtendedAtlassianModule* FUnrealExtendedAtlassianModule::Get()
{
	return Instance;
}

TSharedPtr<FExtendedAtlassianClient> FUnrealExtendedAtlassianModule::GetClient()
{
	return Instance ? Instance->Client : nullptr;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUnrealExtendedAtlassianModule, UnrealExtendedAtlassian)
