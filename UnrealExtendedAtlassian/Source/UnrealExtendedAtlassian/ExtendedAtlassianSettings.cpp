// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianSettings.h"

#include "ExtendedAtlassianClient.h"
#include "ExtendedAtlassianConfluence.h"
#include "ExtendedAtlassianCredentials.h"
#include "ExtendedAtlassianJira.h"
#include "ExtendedAtlassianLog.h"
#include "UnrealExtendedAtlassian.h"

#include "HAL/PlatformProcess.h"

#if WITH_EDITOR
#include "Framework/Notifications/NotificationManager.h"
#include "Misc/Paths.h"
#include "Widgets/Notifications/SNotificationList.h"
#endif

#define LOCTEXT_NAMESPACE "ExtendedAtlassianSettings"

#if WITH_EDITOR
namespace ExtendedAtlassianSettingsPrivate
{
	const TCHAR* TokenPageUrl = TEXT("https://id.atlassian.com/manage-profile/security/api-tokens");

	/**
	 * The ConnectionStatus property only repaints when the details panel next refreshes, which is
	 * not guaranteed to be soon. A notification gives the immediate confirmation a button press
	 * needs; the property is the record you can still read afterwards.
	 */
	void Report(const FText& Message, bool bSuccess)
	{
		FNotificationInfo Info(Message);
		Info.bFireAndForget = true;
		Info.ExpireDuration = bSuccess ? 4.0f : 8.0f;

		if (const TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		}
	}

	void SetStatus(const FText& Message, bool bSuccess)
	{
		if (UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::GetMutable())
		{
			Settings->ConnectionStatus = Message.ToString();
		}

		// Always log. A toast is easy to miss and ConnectionStatus only repaints on the panel's next
		// redraw, which together made a correctly-refusing button look completely dead.
		if (bSuccess)
		{
			UE_LOG(LogExtendedAtlassian, Log, TEXT("%s"), *Message.ToString());
		}
		else
		{
			UE_LOG(LogExtendedAtlassian, Warning, TEXT("%s"), *Message.ToString());
		}

		Report(Message, bSuccess);
	}
}
#endif

UExtendedAtlassianSettings::UExtendedAtlassianSettings()
	: DefaultIssueTypeName(TEXT("Bug"))
	, bIncludePersonalSpaces(true)
	, bEnablePolling(false)
	, PollIntervalSeconds(300)
	, bCaptureScreenshot(true)
	, bCaptureLogTail(true)
	, LogTailKilobytes(64)
	, bCaptureSelectedActors(true)
	, bCaptureCameraTransform(true)
	, bCaptureSourceControlRevision(true)
	, RequestTimeoutSeconds(30)
	, MaxRetries(3)
{
	JqlPresets.Add({ TEXT("My open issues"), TEXT("assignee = currentUser() AND resolution = Unresolved ORDER BY updated DESC") });
	JqlPresets.Add({ TEXT("Reported by me"), TEXT("reporter = currentUser() ORDER BY created DESC") });
	JqlPresets.Add({ TEXT("Updated this week"), TEXT("updated >= -7d ORDER BY updated DESC") });
}

FName UExtendedAtlassianSettings::GetCategoryName() const
{
	return TEXT("Extended Framework");
}

FName UExtendedAtlassianSettings::GetSectionName() const
{
	return TEXT("ExtendedAtlassian");
}

#if WITH_EDITOR
FText UExtendedAtlassianSettings::GetSectionText() const
{
	return LOCTEXT("SectionText", "Extended Atlassian");
}

FText UExtendedAtlassianSettings::GetSectionDescription() const
{
	return LOCTEXT("SectionDescription",
		"Jira and Confluence integration. Site and project settings here are shared with the team through "
		"DefaultGame.ini; your API token is stored per-user outside the repository.");
}
#endif

const UExtendedAtlassianSettings* UExtendedAtlassianSettings::Get()
{
	return GetDefault<UExtendedAtlassianSettings>();
}

void UExtendedAtlassianSettings::PostInitProperties()
{
	Super::PostInitProperties();

#if WITH_EDITOR
	// Mirror the per-user store into the transient fields so the panel opens showing the real state
	// rather than blank boxes over stored credentials. This runs on the CDO, which is exactly the
	// object the settings panel edits.
	FExtendedAtlassianCredentials Credentials;
	if (FExtendedAtlassianCredentialStore::Load(Credentials))
	{
		AccountEmail = Credentials.Email;
		ApiToken = Credentials.ApiToken;
		ConnectionStatus = TEXT("Credentials stored but not yet verified.");
	}
	else
	{
		ConnectionStatus = FExtendedAtlassianCredentialStore::HasStoredCredentials()
			? TEXT("A credential file exists but could not be read. Re-enter your token.")
			: TEXT("No credentials stored on this machine.");
	}
#endif
}

#if WITH_EDITOR

void UExtendedAtlassianSettings::SaveCredentials()
{
	using namespace ExtendedAtlassianSettingsPrivate;

	FExtendedAtlassianCredentials Credentials;
	Credentials.Email = AccountEmail.TrimStartAndEnd();
	Credentials.ApiToken = ApiToken.TrimStartAndEnd();

	if (!Credentials.IsValid())
	{
		SetStatus(LOCTEXT("NeedBoth", "Enter both an e-mail address and an API token."), false);
		return;
	}

	const TSharedPtr<FExtendedAtlassianClient> Client = FUnrealExtendedAtlassianModule::GetClient();
	if (!Client.IsValid())
	{
		SetStatus(LOCTEXT("NoClient", "The Atlassian transport module is not loaded."), false);
		return;
	}

	if (!Client->SetCredentials(Credentials))
	{
		SetStatus(LOCTEXT("SaveFailed", "Could not write the credential file. See the Output Log."), false);
		return;
	}

	SetStatus(FText::Format(
		LOCTEXT("Saved", "Saved for {0}. Now press Test Connection."),
		FText::FromString(Credentials.Email)), true);
}

void UExtendedAtlassianSettings::TestConnection()
{
	using namespace ExtendedAtlassianSettingsPrivate;

	if (!IsConfigured())
	{
		SetStatus(LOCTEXT("NoSite", "Set the Atlassian Site URL above first."), false);
		return;
	}

	const TSharedPtr<FExtendedAtlassianClient> Client = FUnrealExtendedAtlassianModule::GetClient();
	if (!Client.IsValid())
	{
		SetStatus(LOCTEXT("NoClient", "The Atlassian transport module is not loaded."), false);
		return;
	}

	// Pressing Test without pressing Save first used to be a silent refusal. If the fields hold a
	// complete credential that differs from what is stored, just save it and carry on.
	const FString PendingEmail = AccountEmail.TrimStartAndEnd();
	const FString PendingToken = ApiToken.TrimStartAndEnd();

	if (!PendingEmail.IsEmpty() && !PendingToken.IsEmpty())
	{
		const FExtendedAtlassianCredentials& Stored = Client->GetCredentials();
		if (Stored.Email != PendingEmail || Stored.ApiToken != PendingToken)
		{
			FExtendedAtlassianCredentials Pending;
			Pending.Email = PendingEmail;
			Pending.ApiToken = PendingToken;
			Client->SetCredentials(Pending);
		}
	}

	if (!Client->HasCredentials())
	{
		SetStatus(LOCTEXT("NoCredentials", "Enter an e-mail address and API token above first."), false);
		return;
	}

	UE_LOG(LogExtendedAtlassian, Log, TEXT("Testing connection to %s ..."), *GetNormalizedSiteUrl());
	ConnectionStatus = TEXT("Contacting Atlassian...");

	// The CDO outlives any request, so the callback needs no lifetime guard.
	Client->TestConnection([](bool bSuccess, const FExtendedAtlassianUser& User, const FExtendedAtlassianError& Error)
	{
		if (bSuccess)
		{
			SetStatus(FText::Format(
				LOCTEXT("Connected", "Connected as {0}. Loading projects and spaces..."),
				FText::FromString(User.DisplayName)), true);

			// Fill the dropdowns immediately, so the intended flow is just: paste token, test, pick.
			if (UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::GetMutable())
			{
				Settings->RefreshAtlassianLists();
			}
			return;
		}

		SetStatus(FText::Format(
			LOCTEXT("ConnectFailed", "Connection failed: {0}"),
			FText::FromString(Error.Message)), false);
	});
}

void UExtendedAtlassianSettings::OpenApiTokenPage()
{
	FPlatformProcess::LaunchURL(ExtendedAtlassianSettingsPrivate::TokenPageUrl, nullptr, nullptr);
}

void UExtendedAtlassianSettings::ClearStoredCredentials()
{
	using namespace ExtendedAtlassianSettingsPrivate;

	if (const TSharedPtr<FExtendedAtlassianClient> Client = FUnrealExtendedAtlassianModule::GetClient())
	{
		Client->ClearCredentials();
	}
	else
	{
		FExtendedAtlassianCredentialStore::Clear();
	}

	AccountEmail.Reset();
	ApiToken.Reset();

	SetStatus(LOCTEXT("Cleared", "Stored credentials deleted from this machine."), true);
}

void UExtendedAtlassianSettings::RefreshAtlassianLists()
{
	using namespace ExtendedAtlassianSettingsPrivate;

	const TSharedPtr<FExtendedAtlassianClient> Client = FUnrealExtendedAtlassianModule::GetClient();
	if (!Client.IsValid() || !Client->IsReady())
	{
		SetStatus(LOCTEXT("RefreshNotReady", "Set the site URL and save your credentials before refreshing the lists."), false);
		return;
	}

	UE_LOG(LogExtendedAtlassian, Log, TEXT("Refreshing Atlassian dropdown lists..."));

	FExtendedAtlassianJira::GetProjects(FExtendedAtlassianProjectsDelegate::CreateLambda(
		[](bool bSuccess, const TArray<FExtendedAtlassianProject>& Projects, const FExtendedAtlassianError& Error)
		{
			UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::GetMutable();
			if (!Settings)
			{
				return;
			}

			if (!bSuccess)
			{
				SetStatus(FText::Format(
					LOCTEXT("ProjectsFailed", "Could not list projects: {0}"),
					FText::FromString(Error.Message)), false);
				return;
			}

			Settings->CachedProjectKeys.Reset();
			for (const FExtendedAtlassianProject& Project : Projects)
			{
				Settings->CachedProjectKeys.Add(Project.Key);
			}

			SetStatus(FText::Format(
				LOCTEXT("ProjectsLoaded", "Loaded {0} project(s). Pick one in the Jira section."),
				FText::AsNumber(Settings->CachedProjectKeys.Num())), true);

			// Issue types are per-project, so they can only be fetched once a project is known.
			const FString BugProject = Settings->GetEffectiveBugProjectKey();
			if (BugProject.IsEmpty())
			{
				return;
			}

			FExtendedAtlassianJira::GetIssueTypes(BugProject, FExtendedAtlassianIssueTypesDelegate::CreateLambda(
				[](bool bTypesOk, const TArray<FExtendedAtlassianIssueType>& IssueTypes, const FExtendedAtlassianError&)
				{
					UExtendedAtlassianSettings* Inner = UExtendedAtlassianSettings::GetMutable();
					if (!Inner || !bTypesOk)
					{
						return;
					}

					Inner->CachedIssueTypeNames.Reset();
					for (const FExtendedAtlassianIssueType& IssueType : IssueTypes)
					{
						Inner->CachedIssueTypeNames.Add(IssueType.Name);
					}
				}));
		}));

	FExtendedAtlassianJira::GetPriorities(FExtendedAtlassianPrioritiesDelegate::CreateLambda(
		[](bool bSuccess, const TArray<FExtendedAtlassianPriority>& Priorities, const FExtendedAtlassianError&)
		{
			UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::GetMutable();
			if (!Settings || !bSuccess)
			{
				return;
			}

			Settings->CachedPriorityNames.Reset();
			for (const FExtendedAtlassianPriority& Priority : Priorities)
			{
				Settings->CachedPriorityNames.Add(Priority.Name);
			}
		}));

	FExtendedAtlassianConfluence::ListSpaces(FExtendedAtlassianSpacesDelegate::CreateLambda(
		[](bool bSuccess, const TArray<FExtendedAtlassianSpace>& Spaces, const FExtendedAtlassianError&)
		{
			UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::GetMutable();
			if (!Settings || !bSuccess)
			{
				return;
			}

			Settings->CachedSpaceKeys.Reset();
			for (const FExtendedAtlassianSpace& Space : Spaces)
			{
				Settings->CachedSpaceKeys.Add(Space.Key);
			}
		}));
}

void UExtendedAtlassianSettings::ShowCredentialFileLocation()
{
	const FString Path = FExtendedAtlassianCredentialStore::GetStorePath();
	FPlatformProcess::ExploreFolder(*FPaths::GetPath(Path));

	UE_LOG(LogExtendedAtlassian, Log, TEXT("Credentials are stored at %s"), *Path);
}

#endif // WITH_EDITOR

namespace ExtendedAtlassianOptionsPrivate
{
	/**
	 * GetOptions turns the property into a combo box, so a value missing from the list becomes
	 * unselectable. Always offering the current value means an offline editor or a project you have
	 * lost access to never silently wipes a configured setting.
	 */
	TArray<FString> WithCurrentValue(const TArray<FString>& Cached, const FString& Current)
	{
		TArray<FString> Options = Cached;

		const FString Trimmed = Current.TrimStartAndEnd();
		if (!Trimmed.IsEmpty() && !Options.Contains(Trimmed))
		{
			Options.Insert(Trimmed, 0);
		}

		return Options;
	}
}

TArray<FString> UExtendedAtlassianSettings::GetProjectKeyOptions() const
{
	using namespace ExtendedAtlassianOptionsPrivate;

	// Both project fields share this list, so offer whichever of them is set.
	TArray<FString> Options = WithCurrentValue(CachedProjectKeys, ProjectKey);
	const FString Bug = BugProjectKey.TrimStartAndEnd();
	if (!Bug.IsEmpty() && !Options.Contains(Bug))
	{
		Options.Insert(Bug, 0);
	}

	return Options;
}

TArray<FString> UExtendedAtlassianSettings::GetIssueTypeOptions() const
{
	return ExtendedAtlassianOptionsPrivate::WithCurrentValue(CachedIssueTypeNames, DefaultIssueTypeName);
}

TArray<FString> UExtendedAtlassianSettings::GetPriorityOptions() const
{
	return ExtendedAtlassianOptionsPrivate::WithCurrentValue(CachedPriorityNames, DefaultPriorityName);
}

TArray<FString> UExtendedAtlassianSettings::GetSpaceKeyOptions() const
{
	TArray<FString> Options = CachedSpaceKeys;

	// Keep every already-configured space selectable even before a refresh.
	for (const FString& Key : SpaceKeys)
	{
		const FString Trimmed = Key.TrimStartAndEnd();
		if (!Trimmed.IsEmpty() && !Options.Contains(Trimmed))
		{
			Options.Insert(Trimmed, 0);
		}
	}

	return Options;
}

FString UExtendedAtlassianSettings::GetNormalizedSiteUrl() const
{
	FString Url = SiteUrl.TrimStartAndEnd();
	if (Url.IsEmpty())
	{
		return FString();
	}

	if (!Url.StartsWith(TEXT("http://")) && !Url.StartsWith(TEXT("https://")))
	{
		Url = TEXT("https://") + Url;
	}

	while (Url.EndsWith(TEXT("/")))
	{
		Url.LeftChopInline(1);
	}

	return Url;
}

FString UExtendedAtlassianSettings::GetJiraApiBaseUrl() const
{
	const FString Base = GetNormalizedSiteUrl();
	return Base.IsEmpty() ? FString() : Base + TEXT("/rest/api/3");
}

FString UExtendedAtlassianSettings::GetConfluenceApiBaseUrl() const
{
	const FString Base = GetNormalizedSiteUrl();
	return Base.IsEmpty() ? FString() : Base + TEXT("/wiki/api/v2");
}

FString UExtendedAtlassianSettings::GetConfluenceV1ApiBaseUrl() const
{
	const FString Base = GetNormalizedSiteUrl();
	return Base.IsEmpty() ? FString() : Base + TEXT("/wiki/rest/api");
}

FString UExtendedAtlassianSettings::GetEffectiveBugProjectKey() const
{
	const FString Trimmed = BugProjectKey.TrimStartAndEnd();
	return Trimmed.IsEmpty() ? ProjectKey.TrimStartAndEnd() : Trimmed;
}

bool UExtendedAtlassianSettings::IsConfigured() const
{
	return !GetNormalizedSiteUrl().IsEmpty();
}

#undef LOCTEXT_NAMESPACE
