// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EDbgMenu.h"

#include "Analytics/EFAnalyticsSubsystem.h"
#include "EDbgSettings.h"
#include "EDbgSubsystem.h"
#include "Engine/GameInstance.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Messages/EFMessageSubsystem.h"
#include "Status/EFServiceStatus.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ExtendedDebugMenu"

namespace EDbgMenuPrivate
{
	const TCHAR* const TabNames[] = { TEXT("Cheats"), TEXT("Status"), TEXT("Messages"), TEXT("Analytics"), TEXT("Watches") };
	constexpr int32 MaxLines = 40;

	FSlateFontInfo Font(float Size, const TCHAR* Typeface = TEXT("Regular"))
	{
		return FCoreStyle::GetDefaultFontStyle(Typeface, Size);
	}

	TSharedRef<SWidget> MakeTextPage(const TAttribute<FText>& Text)
	{
		return SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SNew(STextBlock)
				.Text(Text)
				.Font(Font(9.f, TEXT("Mono")))
				.AutoWrapText(true)
			];
	}

	const UGameInstance* GetGameInstance(const TWeakObjectPtr<UEDbgSubsystem>& Subsystem)
	{
		const UEDbgSubsystem* Debug = Subsystem.Get();
		return Debug ? Debug->GetGameInstance() : nullptr;
	}
}

void SEDbgMenu::Construct(const FArguments& InArgs)
{
	Subsystem = InArgs._Subsystem;

	TSharedRef<SHorizontalBox> Tabs = SNew(SHorizontalBox);
	for (int32 TabIndex = 0; TabIndex < UE_ARRAY_COUNT(EDbgMenuPrivate::TabNames); ++TabIndex)
	{
		Tabs->AddSlot()
			.AutoWidth()
			.Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(SButton)
				.Text(FText::FromString(EDbgMenuPrivate::TabNames[TabIndex]))
				.OnClicked(this, &SEDbgMenu::HandleTab, TabIndex)
			];
	}

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.92f))
		.Padding(10.f)
		[
			SNew(SBox)
			.WidthOverride(620.f)
			.HeightOverride(560.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 0.f, 0.f, 6.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Title", "Extended Debug"))
						.Font(EDbgMenuPrivate::Font(14.f, TEXT("Bold")))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("Close", "Close"))
						.OnClicked(this, &SEDbgMenu::HandleClose)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 0.f, 0.f, 8.f)
				[
					Tabs
				]
				+ SVerticalBox::Slot()
				.FillHeight(1.f)
				[
					SAssignNew(Switcher, SWidgetSwitcher)
					+ SWidgetSwitcher::Slot()
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
						[
							SAssignNew(CheatList, SVerticalBox)
						]
					]
					+ SWidgetSwitcher::Slot()
					[
						EDbgMenuPrivate::MakeTextPage(TAttribute<FText>::CreateSP(this, &SEDbgMenu::GetStatusText))
					]
					+ SWidgetSwitcher::Slot()
					[
						EDbgMenuPrivate::MakeTextPage(TAttribute<FText>::CreateSP(this, &SEDbgMenu::GetMessagesText))
					]
					+ SWidgetSwitcher::Slot()
					[
						EDbgMenuPrivate::MakeTextPage(TAttribute<FText>::CreateSP(this, &SEDbgMenu::GetAnalyticsText))
					]
					+ SWidgetSwitcher::Slot()
					[
						EDbgMenuPrivate::MakeTextPage(TAttribute<FText>::CreateSP(this, &SEDbgMenu::GetWatchesText))
					]
				]
			]
		]
	];

	RebuildCheats();
}

void SEDbgMenu::RebuildCheats()
{
	if (!CheatList.IsValid())
	{
		return;
	}
	CheatList->ClearChildren();
	ArgumentBoxes.Reset();
	FirstCheatButton.Reset();

	const UEDbgSubsystem* Debug = Subsystem.Get();
	const TArray<FEDbgCheatInfo> AllCheats = Debug ? Debug->GetCheats() : TArray<FEDbgCheatInfo>();
	if (AllCheats.Num() == 0)
	{
		CheatList->AddSlot()
			.AutoHeight()
			[
				SNew(STextBlock).Text(LOCTEXT("NoCheats", "No cheats are registered."))
			];
		return;
	}

	FName LastCategory;
	for (const FEDbgCheatInfo& Info : AllCheats)
	{
		if (Info.Category != LastCategory)
		{
			LastCategory = Info.Category;
			CheatList->AddSlot()
				.AutoHeight()
				.Padding(0.f, 8.f, 0.f, 2.f)
				[
					SNew(STextBlock)
					.Text(FText::FromName(Info.Category))
					.Font(EDbgMenuPrivate::Font(11.f, TEXT("Bold")))
				];
		}

		TSharedPtr<SButton> Button;
		TSharedPtr<SEditableTextBox> ArgumentBox;
		CheatList->AddSlot()
			.AutoHeight()
			.Padding(0.f, 1.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.MinDesiredWidth(200.f)
					[
						SAssignNew(Button, SButton)
						.Text(FText::FromName(Info.Name))
						.ToolTipText(FText::FromString(Info.Help))
						.OnClicked(this, &SEDbgMenu::HandleRunCheat, Info.Name)
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.Padding(6.f, 0.f, 0.f, 0.f)
				.VAlign(VAlign_Center)
				[
					SAssignNew(ArgumentBox, SEditableTextBox)
					.HintText(FText::FromString(Info.ArgumentHint))
					.Visibility(Info.ArgumentHint.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(6.f, 0.f, 0.f, 0.f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(Info.bRunOnServer ? LOCTEXT("Server", "server") : FText::GetEmpty())
					.Font(EDbgMenuPrivate::Font(8.f))
				]
			];

		ArgumentBoxes.Add(Info.Name, ArgumentBox);
		if (!FirstCheatButton.IsValid())
		{
			FirstCheatButton = Button;
		}
	}
}

TSharedPtr<SWidget> SEDbgMenu::GetInitialFocus() const
{
	return FirstCheatButton;
}

FReply SEDbgMenu::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
	{
		return HandleClose();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

FReply SEDbgMenu::HandleTab(int32 TabIndex)
{
	if (Switcher.IsValid())
	{
		if (TabIndex == 0)
		{
			RebuildCheats();
		}
		Switcher->SetActiveWidgetIndex(TabIndex);
	}
	return FReply::Handled();
}

FReply SEDbgMenu::HandleRunCheat(FName CheatName)
{
	UEDbgSubsystem* Debug = Subsystem.Get();
	if (!Debug)
	{
		return FReply::Handled();
	}

	TArray<FString> Arguments;
	if (const TSharedPtr<SEditableTextBox>* Box = ArgumentBoxes.Find(CheatName))
	{
		if (Box->IsValid())
		{
			(*Box)->GetText().ToString().ParseIntoArrayWS(Arguments);
		}
	}

	UGameInstance* GameInstance = Debug->GetGameInstance();
	Debug->RunCheat(GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr, CheatName, Arguments);
	return FReply::Handled();
}

FReply SEDbgMenu::HandleClose()
{
	if (UEDbgSubsystem* Debug = Subsystem.Get())
	{
		Debug->CloseMenu();
	}
	return FReply::Handled();
}

FText SEDbgMenu::GetStatusText() const
{
	const double Now = FPlatformTime::Seconds();
	if (Now - CachedStatusSeconds > 0.5)
	{
		CachedStatus = FText::FromString(FEFServiceStatusRegistry::FormatReport(/*bIncludeDetails*/ true));
		CachedStatusSeconds = Now;
	}
	return CachedStatus;
}

FText SEDbgMenu::GetMessagesText() const
{
	const UGameInstance* GameInstance = EDbgMenuPrivate::GetGameInstance(Subsystem);
	const UEFMessageSubsystem* Messages = GameInstance ? GameInstance->GetSubsystem<UEFMessageSubsystem>() : nullptr;
	if (!Messages)
	{
		return LOCTEXT("NoMessageBus", "The message bus is not available.");
	}

	const TArray<FEFMessageRecord>& Recent = Messages->GetRecentMessages();
	const double Now = FPlatformTime::Seconds();
	FString Text;
	for (int32 Index = Recent.Num() - 1; Index >= 0 && Index >= Recent.Num() - EDbgMenuPrivate::MaxLines; --Index)
	{
		const FEFMessageRecord& Record = Recent[Index];
		Text += FString::Printf(TEXT("%7.1fs ago  %s  (%s) -> %d listener(s)\n"), Now - Record.TimeSeconds, *Record.Channel.ToString(),
			Record.PayloadType.IsNone() ? TEXT("no payload") : *Record.PayloadType.ToString(), Record.ListenerCount);
	}
	return FText::FromString(Text.IsEmpty() ? FString(TEXT("No messages yet. Extended.Messages.Log 1 also writes them to the log.")) : Text);
}

FText SEDbgMenu::GetAnalyticsText() const
{
	const UGameInstance* GameInstance = EDbgMenuPrivate::GetGameInstance(Subsystem);
	const UEFAnalyticsSubsystem* Analytics = GameInstance ? GameInstance->GetSubsystem<UEFAnalyticsSubsystem>() : nullptr;
	if (!Analytics)
	{
		return LOCTEXT("NoAnalytics", "Analytics is not available.");
	}

	FString Text = FString::Printf(TEXT("Session %s, %s, %d event(s) pending\n\n"), *Analytics->GetSessionId(),
		Analytics->IsCollecting() ? TEXT("collecting") : TEXT("not collecting (disabled or no consent)"), Analytics->GetPendingEventCount());
	const TArray<FEFAnalyticsEvent>& Recent = Analytics->GetRecentEvents();
	for (int32 Index = Recent.Num() - 1; Index >= 0 && Index >= Recent.Num() - EDbgMenuPrivate::MaxLines; --Index)
	{
		const FEFAnalyticsEvent& Event = Recent[Index];
		TArray<FString> Pairs;
		for (const TPair<FString, FString>& Attribute : Event.Attributes)
		{
			Pairs.Add(Attribute.Key + TEXT("=") + Attribute.Value);
		}
		Text += FString::Printf(TEXT("#%lld %s {%s}\n"), Event.Sequence, *Event.Name, *FString::Join(Pairs, TEXT(", ")));
	}
	return FText::FromString(Text);
}

FText SEDbgMenu::GetWatchesText() const
{
	const UEDbgSubsystem* Debug = Subsystem.Get();
	if (!Debug || Debug->GetWatches().Num() == 0)
	{
		return LOCTEXT("NoWatches", "No watches. Set Debug Watch (or EDBG_WATCH in C++) adds one.");
	}

	TArray<FString> Keys;
	Debug->GetWatches().GetKeys(Keys);
	Keys.Sort();
	const double Now = FPlatformTime::Seconds();
	FString Text;
	for (const FString& Key : Keys)
	{
		const FEDbgWatch& Watch = Debug->GetWatches().FindChecked(Key);
		Text += FString::Printf(TEXT("%s = %s   (%.1fs ago)\n"), *Key, *Watch.Value, Now - Watch.UpdatedSeconds);
	}
	return FText::FromString(Text);
}

// Input processor

bool FEDbgInputProcessor::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	UEDbgSubsystem* Debug = Subsystem.Get();
	if (!Debug || InKeyEvent.IsRepeat())
	{
		return false;
	}

	const UEDbgSettings* Settings = UEDbgSettings::Get();
	const FKey Key = InKeyEvent.GetKey();
	if (Settings->MenuKey.IsValid() && Key == Settings->MenuKey)
	{
		Debug->ToggleMenu();
		return true;
	}

	if (Settings->bGamepadChord)
	{
		if (Key == EKeys::Gamepad_LeftThumbstick)
		{
			bLeftStickDown = true;
		}
		else if (Key == EKeys::Gamepad_RightThumbstick)
		{
			bRightStickDown = true;
		}
		else
		{
			return false;
		}
		if (bLeftStickDown && bRightStickDown)
		{
			Debug->ToggleMenu();
			return true;
		}
	}
	return false;
}

bool FEDbgInputProcessor::HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Gamepad_LeftThumbstick)
	{
		bLeftStickDown = false;
	}
	else if (Key == EKeys::Gamepad_RightThumbstick)
	{
		bRightStickDown = false;
	}
	return false;
}

#undef LOCTEXT_NAMESPACE
