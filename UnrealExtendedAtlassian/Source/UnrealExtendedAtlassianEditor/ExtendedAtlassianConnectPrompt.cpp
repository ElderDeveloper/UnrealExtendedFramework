// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianConnectPrompt.h"

#include "ExtendedAtlassianClient.h"
#include "ExtendedAtlassianSettings.h"
#include "UnrealExtendedAtlassian.h"

#include "HAL/PlatformProcess.h"
#include "ISettingsModule.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ExtendedAtlassianConnectPrompt"

namespace ExtendedAtlassianConnectPromptPrivate
{
	const TCHAR* ApiTokenPageUrl = TEXT("https://id.atlassian.com/manage-profile/security/api-tokens");
}

bool SExtendedAtlassianConnectPrompt::IsConnected()
{
	const TSharedPtr<FExtendedAtlassianClient> Client = FUnrealExtendedAtlassianModule::GetClient();
	return Client.IsValid() && Client->IsReady();
}

void SExtendedAtlassianConnectPrompt::Construct(const FArguments& InArgs)
{
	ChildSlot
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		SNew(SBox)
		.MaxDesiredWidth(460.0f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Title", "Connect to Atlassian"))
				.Font(FAppStyle::GetFontStyle(TEXT("BoldFont")))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 10.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Justification(ETextJustify::Center)
				.Text_Lambda([]()
				{
					const UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::Get();
					const bool bHasSite = Settings && Settings->IsConfigured();

					const TSharedPtr<FExtendedAtlassianClient> Client = FUnrealExtendedAtlassianModule::GetClient();
					const bool bHasCredentials = Client.IsValid() && Client->HasCredentials();

					// Name the specific thing that is missing rather than a generic prompt.
					if (!bHasSite && !bHasCredentials)
					{
						return LOCTEXT("NeedBoth",
							"Set your Atlassian site URL and enter an API token to start browsing issues and documentation.");
					}
					if (!bHasSite)
					{
						return LOCTEXT("NeedSite",
							"An API token is stored, but no Atlassian site URL is set yet.");
					}
					return LOCTEXT("NeedCredentials",
						"The site URL is set, but no API token is stored on this machine yet.");
				})
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(0.0f, 16.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("OpenSettings", "Open Atlassian Settings"))
					.OnClicked_Lambda([]()
					{
						if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>(TEXT("Settings")))
						{
							SettingsModule->ShowViewer(TEXT("Project"), TEXT("Extended Framework"), TEXT("ExtendedAtlassian"));
						}
						return FReply::Handled();
					})
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("GetToken", "Get an API Token"))
					.OnClicked_Lambda([]()
					{
						FPlatformProcess::LaunchURL(ExtendedAtlassianConnectPromptPrivate::ApiTokenPageUrl, nullptr, nullptr);
						return FReply::Handled();
					})
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 14.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Justification(ETextJustify::Center)
				.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
				.Text(LOCTEXT("PrivacyNote",
					"Your token is stored per-user outside the project, never in source control. "
					"The site URL and project settings are shared with the team."))
			]
		]
	];
}

#undef LOCTEXT_NAMESPACE
