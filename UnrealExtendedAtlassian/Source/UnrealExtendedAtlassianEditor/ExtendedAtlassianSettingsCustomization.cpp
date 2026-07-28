// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianSettingsCustomization.h"

#include "ExtendedAtlassianClient.h"
#include "ExtendedAtlassianCredentials.h"
#include "ExtendedAtlassianLog.h"
#include "ExtendedAtlassianSettings.h"
#include "UnrealExtendedAtlassian.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ExtendedAtlassianSettingsCustomization"

namespace ExtendedAtlassianSettingsCustomizationPrivate
{
	const FLinearColor NeutralColor(0.75f, 0.75f, 0.75f);
	const FLinearColor GoodColor(0.30f, 0.80f, 0.40f);
	const FLinearColor BadColor(0.90f, 0.35f, 0.30f);
}

TSharedRef<IDetailCustomization> FExtendedAtlassianSettingsCustomization::MakeInstance()
{
	return MakeShared<FExtendedAtlassianSettingsCustomization>();
}

FName FExtendedAtlassianSettingsCustomization::GetCustomizedClassName()
{
	// Resolved from the class itself on first call — during startup, while the UObject system is
	// alive — and cached so shutdown never touches StaticClass(). A hardcoded literal here would
	// fail silently if the class were ever renamed, which is exactly the failure mode to avoid.
	static const FName CachedName = UExtendedAtlassianSettings::StaticClass()->GetFName();
	return CachedName;
}

void FExtendedAtlassianSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// The Slate status row below replaces this property; showing both would be redundant.
	DetailBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(UExtendedAtlassianSettings, ConnectionStatus));

	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(
		TEXT("Credentials"),
		LOCTEXT("CredentialsCategory", "Credentials"),
		ECategoryPriority::Important);

	// --- Live status -------------------------------------------------------
	Category.AddCustomRow(LOCTEXT("StatusFilter", "Connection status"))
		.WholeRowContent()
		[
			SNew(SBox)
			.Padding(FMargin(0.0f, 6.0f))
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SThrobber)
					.Visibility(this, &FExtendedAtlassianSettingsCustomization::GetBusyVisibility)
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(this, &FExtendedAtlassianSettingsCustomization::GetStatusText)
						.ColorAndOpacity(this, &FExtendedAtlassianSettingsCustomization::GetStatusColor)
						.Font(FAppStyle::GetFontStyle(TEXT("BoldFont")))
						.AutoWrapText(true)
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 2.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(this, &FExtendedAtlassianSettingsCustomization::GetLastMessageText)
						.ColorAndOpacity(FSlateColor(ExtendedAtlassianSettingsCustomizationPrivate::NeutralColor))
						.AutoWrapText(true)
					]
				]
			]
		];

	// --- Actions -----------------------------------------------------------
	Category.AddCustomRow(LOCTEXT("ActionsFilter", "Connection actions"))
		.WholeRowContent()
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 4.0f, 6.0f, 4.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("SaveAndTest", "Save && Test Connection"))
					.ToolTipText(LOCTEXT("SaveAndTestTip", "Store the e-mail and token, verify them, then load your projects and spaces."))
					.IsEnabled(this, &FExtendedAtlassianSettingsCustomization::IsReadyToTest)
					.OnClicked(this, &FExtendedAtlassianSettingsCustomization::OnSaveAndTestClicked)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 4.0f, 6.0f, 4.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("RefreshLists", "Refresh Lists"))
					.ToolTipText(LOCTEXT("RefreshListsTip", "Re-read projects, issue types, priorities and Confluence spaces."))
					.OnClicked(this, &FExtendedAtlassianSettingsCustomization::OnRefreshListsClicked)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 4.0f, 0.0f, 4.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("GetToken", "Get an API Token"))
					.ToolTipText(LOCTEXT("GetTokenTip", "Opens id.atlassian.com, where API tokens are created."))
					.OnClicked(this, &FExtendedAtlassianSettingsCustomization::OnGetTokenClicked)
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 6.0f, 4.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("ClearCredentials", "Clear Stored Credentials"))
					.OnClicked(this, &FExtendedAtlassianSettingsCustomization::OnClearClicked)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 0.0f, 4.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("ShowFile", "Show Credential File"))
					.OnClicked(this, &FExtendedAtlassianSettingsCustomization::OnShowFileClicked)
				]
			]
		];

	// --- Storage note ------------------------------------------------------
	Category.AddCustomRow(LOCTEXT("StorageFilter", "Credential storage"))
		.WholeRowContent()
		[
			SNew(STextBlock)
			.Text(FText::Format(
				FExtendedAtlassianCredentialStore::IsEncryptionAvailable()
					? LOCTEXT("StorageEncrypted", "Stored DPAPI-encrypted at {0}, bound to this Windows account. Never written to project config.")
					: LOCTEXT("StoragePlain", "Warning: no encryption on this platform. The token is stored as plain text at {0}."),
				FText::FromString(FExtendedAtlassianCredentialStore::GetStorePath())))
			.ColorAndOpacity(FSlateColor(FExtendedAtlassianCredentialStore::IsEncryptionAvailable()
				? ExtendedAtlassianSettingsCustomizationPrivate::NeutralColor
				: ExtendedAtlassianSettingsCustomizationPrivate::BadColor))
			.AutoWrapText(true)
		];
}

FText FExtendedAtlassianSettingsCustomization::GetStatusText() const
{
	const TSharedPtr<FExtendedAtlassianClient> Client = FUnrealExtendedAtlassianModule::GetClient();
	if (!Client.IsValid())
	{
		return LOCTEXT("NoModule", "The Atlassian transport module is not loaded.");
	}

	const UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::Get();
	if (!Settings || !Settings->IsConfigured())
	{
		return LOCTEXT("NoSite", "No site URL set.");
	}

	switch (Client->GetAuthState())
	{
	case EExtendedAtlassianAuthState::NotConfigured:
		return LOCTEXT("NoCreds", "No credentials stored on this machine.");

	case EExtendedAtlassianAuthState::Unverified:
		return LOCTEXT("Unverified", "Credentials stored but not yet verified.");

	case EExtendedAtlassianAuthState::Verified:
		return FText::Format(
			LOCTEXT("Verified", "Connected as {0}."),
			FText::FromString(Client->GetVerifiedUser().DisplayName));

	case EExtendedAtlassianAuthState::Failed:
		return LOCTEXT("Failed", "Connection failed.");
	}

	return FText::GetEmpty();
}

FSlateColor FExtendedAtlassianSettingsCustomization::GetStatusColor() const
{
	using namespace ExtendedAtlassianSettingsCustomizationPrivate;

	const TSharedPtr<FExtendedAtlassianClient> Client = FUnrealExtendedAtlassianModule::GetClient();
	if (!Client.IsValid())
	{
		return FSlateColor(BadColor);
	}

	switch (Client->GetAuthState())
	{
	case EExtendedAtlassianAuthState::Verified: return FSlateColor(GoodColor);
	case EExtendedAtlassianAuthState::Failed:   return FSlateColor(BadColor);
	default:                                    return FSlateColor(NeutralColor);
	}
}

FText FExtendedAtlassianSettingsCustomization::GetLastMessageText() const
{
	// Bound through Slate rather than shown as a property, so it repaints as soon as it changes.
	const UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::Get();
	return Settings ? FText::FromString(Settings->ConnectionStatus) : FText::GetEmpty();
}

EVisibility FExtendedAtlassianSettingsCustomization::GetBusyVisibility() const
{
	const UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::Get();
	const bool bBusy = Settings && Settings->ConnectionStatus.Contains(TEXT("..."));
	return bBusy ? EVisibility::Visible : EVisibility::Collapsed;
}

bool FExtendedAtlassianSettingsCustomization::IsReadyToTest() const
{
	const UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::Get();
	return Settings && Settings->IsConfigured();
}

FReply FExtendedAtlassianSettingsCustomization::OnSaveAndTestClicked()
{
	UE_LOG(LogExtendedAtlassian, Log, TEXT("Save & Test Connection pressed."));

	if (UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::GetMutable())
	{
		Settings->SaveCredentials();
		Settings->TestConnection();
	}

	return FReply::Handled();
}

FReply FExtendedAtlassianSettingsCustomization::OnRefreshListsClicked()
{
	UE_LOG(LogExtendedAtlassian, Log, TEXT("Refresh Lists pressed."));

	if (UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::GetMutable())
	{
		Settings->RefreshAtlassianLists();
	}

	return FReply::Handled();
}

FReply FExtendedAtlassianSettingsCustomization::OnClearClicked()
{
	UE_LOG(LogExtendedAtlassian, Log, TEXT("Clear Stored Credentials pressed."));

	if (UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::GetMutable())
	{
		Settings->ClearStoredCredentials();
	}

	return FReply::Handled();
}

FReply FExtendedAtlassianSettingsCustomization::OnGetTokenClicked()
{
	if (UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::GetMutable())
	{
		Settings->OpenApiTokenPage();
	}

	return FReply::Handled();
}

FReply FExtendedAtlassianSettingsCustomization::OnShowFileClicked()
{
	if (UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::GetMutable())
	{
		Settings->ShowCredentialFileLocation();
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
