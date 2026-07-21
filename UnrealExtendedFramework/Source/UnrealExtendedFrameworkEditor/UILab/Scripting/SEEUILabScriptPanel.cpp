// Copyright Moon Punch Games. All Rights Reserved.

#include "SEEUILabScriptPanel.h"

#if WITH_EDITOR

#include "Blueprint/UserWidget.h"
#include "ContentBrowserModule.h"
#include "Framework/Application/SlateApplication.h"
#include "IContentBrowserSingleton.h"
#include "Styling/AppStyle.h"
#include "UILab/EEUILabUtils.h"
#include "UILab/Host/SEEUILabHostPanel.h"
#include "UILab/Input/EEUILabInputLog.h"
#include "UILab/Scripting/EEUIScriptRunner.h"
#include "UILab/Scripting/EFUIInteractionScript.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SEEUILabScriptPanel"

namespace
{
	FString DescribeStep(const FEFUIScriptStep& Step)
	{
		const UEnum* OpEnum = StaticEnum<EEFUIScriptOp>();
		FString Text = OpEnum->GetNameStringByValue(static_cast<int64>(Step.Op));
		if (!Step.Target.IsNone())
		{
			Text += FString::Printf(TEXT("  %s"), *Step.Target.ToString());
		}
		if (!Step.Value.IsEmpty())
		{
			Text += FString::Printf(TEXT("  \"%s\""), *Step.Value);
		}
		if (Step.RepeatCount > 1)
		{
			Text += FString::Printf(TEXT("  x%d"), Step.RepeatCount);
		}
		return Text;
	}
}

void SEEUILabScriptPanel::Construct(const FArguments& InArgs, const TSharedRef<SEEUILabHostPanel>& InHostPanel)
{
	HostPanel = InHostPanel;

	if (const TSharedPtr<FEEUILabInputLog> Log = InHostPanel->GetInputLog())
	{
		LogChangedHandle = Log->OnChanged.AddSP(this, &SEEUILabScriptPanel::HandleInputLogChanged);
	}

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight().Padding(4.0f)
		[
			MakeScriptPickerButton()
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(4.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Run", "Run"))
				.OnClicked(this, &SEEUILabScriptPanel::OnRunClicked)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Stop", "Stop"))
				.OnClicked(this, &SEEUILabScriptPanel::OnStopClicked)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f, 2.0f, 0.0f)
			[
				SNew(SCheckBox)
				.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
				.IsChecked_Lambda([this]() { return bRecording ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState) { OnRecordToggled(NewState == ECheckBoxState::Checked); })
				[
					SNew(STextBlock).Text(LOCTEXT("Record", "Record"))
					.ToolTipText(LOCTEXT("RecordTooltip", "Append key presses in the viewport as semantic steps."))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("ExpectFocus", "+ Expect Focus"))
				.ToolTipText(LOCTEXT("ExpectFocusTooltip", "Append an ExpectFocus assertion for the currently focused widget."))
				.OnClicked(this, &SEEUILabScriptPanel::OnExpectFocusClicked)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Save", "Save"))
				.ToolTipText(LOCTEXT("SaveTooltip", "Mark the script asset dirty so recorded steps can be saved."))
				.OnClicked(this, &SEEUILabScriptPanel::OnSaveClicked)
			]
		]

		+ SVerticalBox::Slot().FillHeight(1.0f).Padding(4.0f)
		[
			SAssignNew(StepList, SListView<TSharedPtr<FStepRow>>)
			.ListItemsSource(&StepRows)
			.OnGenerateRow(this, &SEEUILabScriptPanel::MakeStepRow)
		]
	];
}

SEEUILabScriptPanel::~SEEUILabScriptPanel()
{
	if (Runner.IsValid())
	{
		Runner->Stop();
	}

	const TSharedPtr<SEEUILabHostPanel> Panel = HostPanel.Pin();
	if (Panel.IsValid() && LogChangedHandle.IsValid())
	{
		if (const TSharedPtr<FEEUILabInputLog> Log = Panel->GetInputLog())
		{
			Log->OnChanged.Remove(LogChangedHandle);
		}
	}
}

void SEEUILabScriptPanel::SetActiveScript(UEFUIInteractionScript* Script)
{
	if (Script)
	{
		ActiveScript = TStrongObjectPtr<UEFUIInteractionScript>(Script);
		RebuildStepRows();
	}
}

TSharedRef<SWidget> SEEUILabScriptPanel::MakeScriptPickerButton()
{
	return SNew(SComboButton)
		.OnGetMenuContent(FOnGetContent::CreateSP(this, &SEEUILabScriptPanel::MakeScriptPickerMenu))
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text_Lambda([this]()
			{
				return ActiveScript.IsValid()
					? FText::FromString(ActiveScript->GetName())
					: LOCTEXT("PickScript", "Pick Interaction Script...");
			})
		];
}

TSharedRef<SWidget> SEEUILabScriptPanel::MakeScriptPickerMenu()
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	FAssetPickerConfig PickerConfig;
	PickerConfig.Filter.ClassPaths.Add(UEFUIInteractionScript::StaticClass()->GetClassPathName());
	PickerConfig.Filter.bRecursiveClasses = true;
	PickerConfig.SelectionMode = ESelectionMode::Single;
	PickerConfig.InitialAssetViewType = EAssetViewType::List;
	PickerConfig.OnAssetSelected = FOnAssetSelected::CreateLambda([this](const FAssetData& AssetData)
	{
		FSlateApplication::Get().DismissAllMenus();
		if (UEFUIInteractionScript* Script = Cast<UEFUIInteractionScript>(AssetData.GetAsset()))
		{
			ActiveScript = TStrongObjectPtr<UEFUIInteractionScript>(Script);
			RebuildStepRows();
		}
	});

	return SNew(SBox).WidthOverride(350.0f).HeightOverride(400.0f)
	[
		ContentBrowserModule.Get().CreateAssetPicker(PickerConfig)
	];
}

void SEEUILabScriptPanel::RebuildStepRows()
{
	StepRows.Reset();

	if (ActiveScript.IsValid())
	{
		for (int32 i = 0; i < ActiveScript->Steps.Num(); ++i)
		{
			TSharedPtr<FStepRow> Row = MakeShared<FStepRow>();
			Row->StepIndex = i;
			Row->Description = DescribeStep(ActiveScript->Steps[i]);
			StepRows.Add(Row);
		}
	}

	if (Runner.IsValid())
	{
		for (const FEEUIScriptStepResult& Result : Runner->GetResults())
		{
			if (StepRows.IsValidIndex(Result.StepIndex))
			{
				StepRows[Result.StepIndex]->Status = Result.bPassed ? 1 : 0;
				StepRows[Result.StepIndex]->Message = Result.Message;
			}
		}
	}

	if (StepList.IsValid())
	{
		StepList->RequestListRefresh();
	}
}

TSharedRef<ITableRow> SEEUILabScriptPanel::MakeStepRow(TSharedPtr<FStepRow> Row, const TSharedRef<STableViewBase>& OwnerTable)
{
	const FLinearColor Color = Row->Status == 1 ? FLinearColor(0.3f, 1.0f, 0.3f)
		: Row->Status == 0 ? FLinearColor(1.0f, 0.35f, 0.35f)
		: FLinearColor::White;

	const FString Text = Row->Message.IsEmpty()
		? FString::Printf(TEXT("%d. %s"), Row->StepIndex + 1, *Row->Description)
		: FString::Printf(TEXT("%d. %s  —  %s"), Row->StepIndex + 1, *Row->Description, *Row->Message);

	return SNew(STableRow<TSharedPtr<FStepRow>>, OwnerTable)
	[
		SNew(STextBlock)
		.Text(FText::FromString(Text))
		.ColorAndOpacity(FSlateColor(Color))
		.AutoWrapText(true)
	];
}

FReply SEEUILabScriptPanel::OnRunClicked()
{
	const TSharedPtr<SEEUILabHostPanel> Panel = HostPanel.Pin();
	if (!ActiveScript.IsValid() || !Panel.IsValid() || (Runner.IsValid() && Runner->IsRunning()))
	{
		return FReply::Handled();
	}

	bRecording = false;
	Runner = MakeShared<FEEUIScriptRunner>();
	Runner->OnFinished.AddSP(this, &SEEUILabScriptPanel::HandleRunFinished);
	Runner->Start(ActiveScript.Get(), Panel.ToSharedRef());
	RebuildStepRows();
	return FReply::Handled();
}

FReply SEEUILabScriptPanel::OnStopClicked()
{
	if (Runner.IsValid())
	{
		Runner->Stop();
		RebuildStepRows();
	}
	return FReply::Handled();
}

void SEEUILabScriptPanel::HandleRunFinished()
{
	RebuildStepRows();
}

void SEEUILabScriptPanel::OnRecordToggled(const bool bEnabled)
{
	bRecording = bEnabled;

	const TSharedPtr<SEEUILabHostPanel> Panel = HostPanel.Pin();
	if (Panel.IsValid())
	{
		if (const TSharedPtr<FEEUILabInputLog> Log = Panel->GetInputLog())
		{
			LastSeenLogCount = Log->GetEvents().Num();
		}
	}
}

void SEEUILabScriptPanel::HandleInputLogChanged()
{
	if (!bRecording || !ActiveScript.IsValid())
	{
		return;
	}

	const TSharedPtr<SEEUILabHostPanel> Panel = HostPanel.Pin();
	if (!Panel.IsValid())
	{
		return;
	}

	const TSharedPtr<FEEUILabInputLog> Log = Panel->GetInputLog();
	if (!Log.IsValid())
	{
		return;
	}

	const TArray<TSharedPtr<FEEUILabInputEvent>>& Events = Log->GetEvents();
	for (int32 i = LastSeenLogCount; i < Events.Num(); ++i)
	{
		const FEEUILabInputEvent& Event = *Events[i];
		// A-3: only key-downs, and never bare modifier keys.
		if (Event.EventType != TEXT("KeyDown") || Event.Key.IsModifierKey())
		{
			continue;
		}

		FEFUIScriptStep Step;
		if (Event.Key == EKeys::Gamepad_DPad_Up) { Step.Op = EEFUIScriptOp::Navigate; Step.Value = TEXT("Up"); }
		else if (Event.Key == EKeys::Gamepad_DPad_Down) { Step.Op = EEFUIScriptOp::Navigate; Step.Value = TEXT("Down"); }
		else if (Event.Key == EKeys::Gamepad_DPad_Left) { Step.Op = EEFUIScriptOp::Navigate; Step.Value = TEXT("Left"); }
		else if (Event.Key == EKeys::Gamepad_DPad_Right) { Step.Op = EEFUIScriptOp::Navigate; Step.Value = TEXT("Right"); }
		else
		{
			Step.Op = EEFUIScriptOp::PressKey;
			Step.Value = Event.Key.ToString();
		}

		ActiveScript->Modify();

		// A-3: collapse repeated presses into one step with a repeat count.
		if (ActiveScript->Steps.Num() > 0)
		{
			FEFUIScriptStep& LastStep = ActiveScript->Steps.Last();
			if (LastStep.Op == Step.Op && LastStep.Value == Step.Value && LastStep.Target == Step.Target)
			{
				++LastStep.RepeatCount;
				continue;
			}
		}

		ActiveScript->Steps.Add(MoveTemp(Step));
	}
	LastSeenLogCount = Events.Num();

	RebuildStepRows();
}

FReply SEEUILabScriptPanel::OnExpectFocusClicked()
{
	const TSharedPtr<SEEUILabHostPanel> Panel = HostPanel.Pin();
	if (!ActiveScript.IsValid() || !Panel.IsValid())
	{
		return FReply::Handled();
	}

	UUserWidget* Hosted = Panel->GetHostedWidget();
	const TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetUserFocusedWidget(0);
	if (!Hosted || !Focused.IsValid())
	{
		return FReply::Handled();
	}

	const FName Identity = EEUILabUtils::FindAutomationIdentityForSlateWidget(*Hosted, Focused);
	if (Identity.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("UI Lab: focused widget has no automation identity; cannot record ExpectFocus."));
		return FReply::Handled();
	}

	FEFUIScriptStep Step;
	Step.Op = EEFUIScriptOp::ExpectFocus;
	Step.Target = Identity;

	ActiveScript->Modify();
	ActiveScript->Steps.Add(MoveTemp(Step));
	RebuildStepRows();
	return FReply::Handled();
}

FReply SEEUILabScriptPanel::OnSaveClicked()
{
	if (ActiveScript.IsValid())
	{
		ActiveScript->MarkPackageDirty();
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
