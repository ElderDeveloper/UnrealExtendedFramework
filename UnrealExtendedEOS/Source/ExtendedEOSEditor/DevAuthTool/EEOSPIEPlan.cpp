// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSPIEPlan.h"

#include "EEOSDevAuthTool.h"
#include "EEOSPIELoginSettings.h"
#include "EEOSPIELoginSync.h"

#include "Settings/LevelEditorPlaySettings.h"

#define LOCTEXT_NAMESPACE "ExtendedEOSPIEPlan"

FEEOSPIEPlan FEEOSPIEPlan::Build(bool bProbeTool)
{
	FEEOSPIEPlan Plan;

	const UEEOSPIELoginSettings* Settings = GetDefault<UEEOSPIELoginSettings>();
	const FEEOSDevAuthTool::EEnvironment Environment = Settings->bUseGameDevEnvironment
		? FEEOSDevAuthTool::EEnvironment::GameDev
		: FEEOSDevAuthTool::EEnvironment::Prod;

	Plan.ToolAddress = UEEOSPIELoginSettings::GetDevAuthToolAddress();
	Plan.AccountsInTool = FEEOSDevAuthTool::ReadCredentialNames(Environment);
	Plan.bToolRunning = bProbeTool && FEEOSDevAuthTool::IsToolListening(UEEOSPIELoginSettings::GetDevAuthToolPort());

	// ── Engine play settings ─────────────────────────────────────────────────
	if (const ULevelEditorPlaySettings* PlaySettings = GetDefault<ULevelEditorPlaySettings>())
	{
		PlaySettings->GetPlayNumberOfClients(Plan.NumClients);
		PlaySettings->GetRunUnderOneProcess(Plan.bRunUnderOneProcess);
	}

	// ── Engine PIE credential list ───────────────────────────────────────────
	TArray<FEEOSPIELoginSync::FLoginEntry> Logins;
	Plan.bSettingsAvailable = FEEOSPIELoginSync::ReadAllLogins(Logins, Plan.bLoginsEnabled);
	Plan.NumConfiguredLogins = Logins.Num();
	for (const FEEOSPIELoginSync::FLoginEntry& Login : Logins)
	{
		Plan.NumValidLogins += Login.bValid ? 1 : 0;
	}

	if (!Plan.bSettingsAvailable)
	{
		Plan.Problems.Add(TEXT("Could not read the engine's Play Credentials (UOnlinePIESettings) — per-instance sign-in cannot be verified."));
		return Plan;
	}

	// This mirrors UEditorEngine's own gate. Both halves matter and they fail differently.
	Plan.bWillApplyPerInstanceLogins = Plan.bLoginsEnabled && Plan.NumClients <= Plan.NumValidLogins;

	// ── The sign-in order ────────────────────────────────────────────────────
	for (int32 Client = 0; Client < Plan.NumClients; ++Client)
	{
		if (!Logins.IsValidIndex(Client))
		{
			Plan.SignInOrder.Add(FString::Printf(TEXT("Client %d  ->  (no credential configured)"), Client));
			continue;
		}

		const FEEOSPIELoginSync::FLoginEntry& Login = Logins[Client];
		if (!Login.bValid)
		{
			Plan.SignInOrder.Add(FString::Printf(TEXT("Client %d  ->  (entry %d is incomplete — Type/Id/Password must all be set)"), Client, Client));
		}
		else if (Login.IsDeveloperType())
		{
			Plan.SignInOrder.Add(FString::Printf(TEXT("Client %d  ->  %s   [dev auth @ %s]"), Client, *Login.Token, *Login.Id));
		}
		else
		{
			Plan.SignInOrder.Add(FString::Printf(TEXT("Client %d  ->  %s   [%s]"), Client, *Login.Id, *Login.Type));
		}
	}

	// ── Problems, most-decisive first ────────────────────────────────────────

	// THE silent killer: too few valid logins disables per-instance sign-in for EVERY client,
	// not just the unconfigured ones, and the engine only says so at Verbose.
	if (Plan.bLoginsEnabled && Plan.NumClients > Plan.NumValidLogins)
	{
		Plan.Problems.Add(FString::Printf(
			TEXT("%d client(s) but only %d valid credential(s). UE requires Clients <= Credentials or it turns per-instance sign-in OFF ENTIRELY — ")
			TEXT("all clients then fall back to the project's auto-login and share ONE account. Add credentials or lower the client count."),
			Plan.NumClients, Plan.NumValidLogins));
	}

	if (!Plan.bLoginsEnabled)
	{
		Plan.Problems.Add(TEXT("\"Enable Logins\" is off in Project Settings > Level Editor > Play Credentials — no client will use a per-instance account."));
	}

	if (Plan.NumConfiguredLogins > Plan.NumValidLogins)
	{
		Plan.Problems.Add(FString::Printf(
			TEXT("%d credential entry/entries are incomplete and are not counted. An entry needs Type, Id and Password all set; ")
			TEXT("re-run \"Sync PIE Logins from Dev Auth Tool\" to rewrite the list."),
			Plan.NumConfiguredLogins - Plan.NumValidLogins));
	}

	// Credential names the tool does not actually have → the SDK's exchange_code request 404s
	// and that one client fails to sign in.
	if (Plan.AccountsInTool.Num() > 0)
	{
		TArray<FString> Unknown;
		for (const FEEOSPIELoginSync::FLoginEntry& Login : Logins)
		{
			if (Login.IsDeveloperType() && Login.bValid && !Plan.AccountsInTool.Contains(Login.Token))
			{
				Unknown.Add(FString::Printf(TEXT("client %d: '%s'"), Login.InstanceIndex, *Login.Token));
			}
		}
		if (Unknown.Num() > 0)
		{
			Plan.Problems.Add(FString::Printf(
				TEXT("The Dev Auth Tool has no account named — %s. Those clients will fail to sign in. Known accounts: %s."),
				*FString::Join(Unknown, TEXT("; ")), *FString::Join(Plan.AccountsInTool, TEXT(", "))));
		}
	}
	else
	{
		bool bAnyDeveloperLogin = false;
		for (const FEEOSPIELoginSync::FLoginEntry& Login : Logins)
		{
			bAnyDeveloperLogin |= Login.IsDeveloperType();
		}
		if (bAnyDeveloperLogin)
		{
			Plan.Problems.Add(TEXT("The Dev Auth Tool has no saved accounts, but PIE is configured to sign in with Developer credentials. Add one account per client in the tool."));
		}
	}

	if (bProbeTool && !Plan.bToolRunning)
	{
		bool bAnyDeveloperLogin = false;
		for (const FEEOSPIELoginSync::FLoginEntry& Login : Logins)
		{
			bAnyDeveloperLogin |= Login.IsDeveloperType();
		}
		if (bAnyDeveloperLogin)
		{
			Plan.Problems.Add(FString::Printf(TEXT("Nothing is listening on %s — the Dev Auth Tool is not running."), *Plan.ToolAddress));
		}
	}

	return Plan;
}

FString FEEOSPIEPlan::ToLogString() const
{
	TArray<FString> Lines;
	Lines.Add(TEXT("──────── Extended EOS: PIE sign-in plan ────────"));
	Lines.Add(FString::Printf(TEXT("  Clients: %d   Valid credentials: %d   Dev Auth Tool: %s (%s)"),
		NumClients, NumValidLogins,
		bToolRunning ? TEXT("running") : TEXT("not running"), *ToolAddress));
	Lines.Add(FString::Printf(TEXT("  Per-instance sign-in will be applied: %s%s"),
		bWillApplyPerInstanceLogins ? TEXT("YES") : TEXT("NO"),
		bRunUnderOneProcess ? TEXT("   (clients run under one process)") : TEXT("   (clients run as separate processes)")));

	if (SignInOrder.Num() > 0)
	{
		Lines.Add(TEXT("  Order:"));
		for (const FString& Line : SignInOrder)
		{
			Lines.Add(FString::Printf(TEXT("    %s"), *Line));
		}
	}

	if (Problems.Num() > 0)
	{
		Lines.Add(TEXT("  Problems:"));
		for (const FString& Problem : Problems)
		{
			Lines.Add(FString::Printf(TEXT("    ! %s"), *Problem));
		}
	}

	Lines.Add(TEXT("────────────────────────────────────────────────"));
	return FString::Join(Lines, LINE_TERMINATOR);
}

#undef LOCTEXT_NAMESPACE
