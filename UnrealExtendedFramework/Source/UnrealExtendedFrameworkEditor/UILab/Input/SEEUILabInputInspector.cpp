// Copyright Moon Punch Games. All Rights Reserved.

#include "SEEUILabInputInspector.h"

#if WITH_EDITOR

#include "Framework/Application/SlateApplication.h"
#include "Styling/AppStyle.h"
#include "UILab/Input/EEUILabInputLog.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SEEUILabInputInspector"

void SEEUILabInputInspector::Construct(const FArguments& InArgs, const TSharedRef<FEEUILabInputLog>& InLog)
{
	Log = InLog;
	LogChangedHandle = InLog->OnChanged.AddSP(this, &SEEUILabInputInspector::HandleLogChanged);

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight().Padding(4.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SEEUILabInputInspector::GetFocusPathText)
				.AutoWrapText(true)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Clear", "Clear"))
				.OnClicked_Lambda([this]() { Log->Clear(); return FReply::Handled(); })
			]
		]

		+ SVerticalBox::Slot().FillHeight(1.0f).Padding(4.0f)
		[
			SAssignNew(EventList, SListView<TSharedPtr<FEEUILabInputEvent>>)
			.ListItemsSource(&InLog->GetEvents())
			.OnGenerateRow(this, &SEEUILabInputInspector::MakeEventRow)
		]
	];
}

SEEUILabInputInspector::~SEEUILabInputInspector()
{
	if (Log.IsValid() && LogChangedHandle.IsValid())
	{
		Log->OnChanged.Remove(LogChangedHandle);
	}
}

void SEEUILabInputInspector::HandleLogChanged()
{
	if (EventList.IsValid())
	{
		EventList->RequestListRefresh();
		if (Log->GetEvents().Num() > 0)
		{
			EventList->RequestScrollIntoView(Log->GetEvents().Last());
		}
	}
}

TSharedRef<ITableRow> SEEUILabInputInspector::MakeEventRow(TSharedPtr<FEEUILabInputEvent> Event, const TSharedRef<STableViewBase>& OwnerTable)
{
	FString Description;
	if (Event->EventType == TEXT("Capture"))
	{
		Description = Event->Detail;
	}
	else if (Event->EventType == TEXT("Analog"))
	{
		Description = FString::Printf(TEXT("Analog  %s = %.2f"), *Event->Key.ToString(), Event->AnalogValue);
	}
	else
	{
		Description = FString::Printf(TEXT("%s  %s  —  %s"),
			*Event->EventType.ToString(),
			*Event->Key.ToString(),
			Event->bHandledByWidget ? TEXT("handled by widget") : TEXT("NOT handled"));
	}

	const FLinearColor Color = Event->bHandledByWidget ? FLinearColor::White : FLinearColor(1.0f, 0.6f, 0.3f);

	return SNew(STableRow<TSharedPtr<FEEUILabInputEvent>>, OwnerTable)
	[
		SNew(STextBlock)
		.Text(FText::FromString(Description))
		.ColorAndOpacity(FSlateColor(Color))
	];
}

FText SEEUILabInputInspector::GetFocusPathText() const
{
	const TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetUserFocusedWidget(0);
	if (!Focused.IsValid())
	{
		return LOCTEXT("NoFocus", "Focus: (none)");
	}

	// Short path: bottom-up widget types, capped for readability.
	FString Path = Focused->GetTypeAsString();
	int32 Depth = 0;
	for (TSharedPtr<SWidget> Walker = Focused->GetParentWidget(); Walker.IsValid() && Depth < 4; Walker = Walker->GetParentWidget(), ++Depth)
	{
		Path = Walker->GetTypeAsString() + TEXT(" > ") + Path;
	}

	return FText::Format(LOCTEXT("FocusFmt", "Focus: {0}"), FText::FromString(Path));
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
