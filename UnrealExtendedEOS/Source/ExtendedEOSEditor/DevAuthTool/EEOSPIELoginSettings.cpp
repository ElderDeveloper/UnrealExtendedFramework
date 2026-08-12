// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSPIELoginSettings.h"

#include "EEOSDevAuthTool.h"
#include "EEOSPIEPlan.h"
#include "EEOSPIELoginSync.h"
#include "Shared/EEOSLog.h"
#include "Shared/EEOSSettings.h"

namespace
{
	constexpr int32 GDefaultDevAuthPort = 6547;
	const TCHAR* GDefaultDevAuthAddress = TEXT("localhost:6547");
}

UEEOSPIELoginSettings::UEEOSPIELoginSettings()
{
}

FString UEEOSPIELoginSettings::GetDevAuthToolAddress()
{
	// Single source of truth: the same address a Developer login uses at runtime.
	const UEEOSSettings* EOSSettings = UEEOSSettings::Get();
	if (!EOSSettings || EOSSettings->DevAuthToolAddress.IsEmpty())
	{
		return GDefaultDevAuthAddress;
	}
	return EOSSettings->DevAuthToolAddress;
}

int32 UEEOSPIELoginSettings::GetDevAuthToolPort()
{
	const FString Address = GetDevAuthToolAddress();

	// Split on the LAST colon so an IPv6-ish or scheme-prefixed host does not confuse the parse.
	FString Host;
	FString PortStr;
	if (Address.Split(TEXT(":"), &Host, &PortStr, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
	{
		PortStr.TrimStartAndEndInline();
		if (PortStr.IsNumeric())
		{
			const int32 Port = FCString::Atoi(*PortStr);
			if (Port >= 1024 && Port <= 65535)
			{
				return Port;
			}
		}
	}

	return GDefaultDevAuthPort;
}

void UEEOSPIELoginSettings::RefreshStatusOnDefault()
{
	if (UEEOSPIELoginSettings* Settings = GetMutableDefault<UEEOSPIELoginSettings>())
	{
		Settings->RefreshStatus();
	}
}

void UEEOSPIELoginSettings::RefreshStatus()
{
	const FEEOSPIEPlan Plan = FEEOSPIEPlan::Build(/*bProbeTool*/ true);

	ToolStatus = Plan.bToolRunning
		? FString::Printf(TEXT("Running on %s"), *Plan.ToolAddress)
		: FString::Printf(TEXT("NOT running (%s) — use \"Open Dev Auth Tool\""), *Plan.ToolAddress);

	AccountsInTool = Plan.AccountsInTool;
	SignInOrder = Plan.SignInOrder;
	Problems = Plan.Problems;

	if (AccountsInTool.Num() == 0)
	{
		AccountsInTool.Add(TEXT("(none — open the tool and sign in once per account)"));
	}
	if (SignInOrder.Num() == 0)
	{
		SignInOrder.Add(TEXT("(no PIE clients configured)"));
	}
	if (Problems.Num() == 0)
	{
		Problems.Add(TEXT("None — every client has a distinct account."));
	}
}

void UEEOSPIELoginSettings::OpenDevAuthTool()
{
	const int32 Port = GetDevAuthToolPort();
	if (FEEOSDevAuthTool::IsToolListening(Port))
	{
		UE_LOG(LogExtendedEOS, Log, TEXT("DevAuthTool: already running on port %d."), Port);
	}
	else
	{
		const FEEOSDevAuthTool::EEnvironment Environment = bUseGameDevEnvironment
			? FEEOSDevAuthTool::EEnvironment::GameDev
			: FEEOSDevAuthTool::EEnvironment::Prod;

		if (!FEEOSDevAuthTool::LaunchTool(Environment))
		{
			UE_LOG(LogExtendedEOS, Error,
				TEXT("DevAuthTool: could not launch the tool. Check EOS_DevAuthTool.exe exists under Thirdparty/EOS/SDK/Tools."));
		}
	}

	RefreshStatus();
}

void UEEOSPIELoginSettings::SyncPIELoginsFromDevAuthTool()
{
	const FEEOSDevAuthTool::EEnvironment Environment = bUseGameDevEnvironment
		? FEEOSDevAuthTool::EEnvironment::GameDev
		: FEEOSDevAuthTool::EEnvironment::Prod;

	const TArray<FString> Names = FEEOSDevAuthTool::ReadCredentialNames(Environment);
	if (Names.Num() == 0)
	{
		UE_LOG(LogExtendedEOS, Warning,
			TEXT("DevAuthTool: no accounts in '%s' — open the tool, sign in once per account, then sync again."),
			*FEEOSDevAuthTool::GetCredentialsFilePath(Environment));
		RefreshStatus();
		return;
	}

	FString Error;
	if (!FEEOSPIELoginSync::WriteDeveloperLogins(Names, GetDevAuthToolAddress(), Error))
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("DevAuthTool: could not write PIE logins — %s"), *Error);
	}

	RefreshStatus();
}

#if WITH_EDITOR
void UEEOSPIELoginSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Switching environment changes which credential store is read, so the status block would
	// otherwise show the other environment's accounts until the next manual refresh.
	RefreshStatus();
}
#endif
