// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "PerfSentinelReportView.h"

#include "PerfSentinelSettings.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PerfSentinelReportView"

const FName PerfSentinelReportTabName(TEXT("PerfSentinel.Report"));

namespace
{
FString ReadString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, const FString& Default = TEXT("-"))
{
	FString Value;
	return Object.IsValid() && Object->TryGetStringField(Field, Value) ? Value : Default;
}

double ReadNumber(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, double Default = 0.0)
{
	double Value = Default;
	return Object.IsValid() && Object->TryGetNumberField(Field, Value) ? Value : Default;
}

FLinearColor SeverityColor(const FString& Severity)
{
	if (Severity.Equals(TEXT("error"), ESearchCase::IgnoreCase))
	{
		return FLinearColor(0.36f, 0.04f, 0.04f, 0.9f);
	}
	if (Severity.Equals(TEXT("warning"), ESearchCase::IgnoreCase))
	{
		return FLinearColor(0.34f, 0.20f, 0.03f, 0.9f);
	}
	return FLinearColor(0.04f, 0.16f, 0.27f, 0.9f);
}
}

void SPerfSentinelReportView::Construct(const FArguments& InArgs)
{
	(void)InArgs;
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(6.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton).Text(LOCTEXT("Refresh", "Refresh")).OnClicked(this, &SPerfSentinelReportView::HandleRefresh)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton).Text(LOCTEXT("OpenReport", "Open JSON")).OnClicked(this, &SPerfSentinelReportView::HandleOpenReport)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton).Text(LOCTEXT("OpenFolder", "Open Folder")).OnClicked(this, &SPerfSentinelReportView::HandleOpenFolder)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 2.0f)
		[
			SAssignNew(StatusText, STextBlock).AutoWrapText(true)
		]
		+ SVerticalBox::Slot().FillHeight(1.0f).Padding(6.0f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SAssignNew(ResultsBox, SVerticalBox)
			]
		]
	];

	RefreshReport();
}

FReply SPerfSentinelReportView::HandleRefresh()
{
	RefreshReport();
	return FReply::Handled();
}

FReply SPerfSentinelReportView::HandleOpenReport()
{
	if (FPaths::FileExists(ReportPath))
	{
		FPlatformProcess::LaunchFileInDefaultExternalApplication(*ReportPath);
	}
	return FReply::Handled();
}

FReply SPerfSentinelReportView::HandleOpenFolder()
{
	const FString Directory = ReportPath.IsEmpty()
		? UPerfSentinelSettings::Get()->GetResolvedReportOutputDirectory()
		: FPaths::GetPath(ReportPath);
	IFileManager::Get().MakeDirectory(*Directory, true);
	FPlatformProcess::ExploreFolder(*Directory);
	return FReply::Handled();
}

FString SPerfSentinelReportView::FindNewestReport() const
{
	const UPerfSentinelSettings* Settings = UPerfSentinelSettings::Get();
	if (!Settings)
	{
		return FString();
	}

	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *Settings->GetResolvedReportOutputDirectory(), TEXT("findings.json"), true, false, false);
	FString Newest;
	FDateTime NewestTime = FDateTime::MinValue();
	for (const FString& File : Files)
	{
		const FDateTime Timestamp = IFileManager::Get().GetTimeStamp(*File);
		if (Newest.IsEmpty() || Timestamp > NewestTime)
		{
			Newest = File;
			NewestTime = Timestamp;
		}
	}
	return Newest;
}

void SPerfSentinelReportView::AddMessage(const FText& Message)
{
	ResultsBox->AddSlot().AutoHeight().Padding(4.0f)
	[
		SNew(STextBlock).Text(Message).AutoWrapText(true)
	];
}

void SPerfSentinelReportView::RefreshReport()
{
	if (!ResultsBox.IsValid() || !StatusText.IsValid())
	{
		return;
	}
	ResultsBox->ClearChildren();
	ReportPath = FindNewestReport();
	if (ReportPath.IsEmpty())
	{
		StatusText->SetText(LOCTEXT("NoReport", "No PerfSentinel report has been generated yet."));
		return;
	}

	FString Contents;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Contents, *ReportPath)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Contents), Root)
		|| !Root.IsValid())
	{
		StatusText->SetText(FText::Format(LOCTEXT("InvalidReport", "Could not read report: {0}"), FText::FromString(ReportPath)));
		return;
	}

	const TSharedPtr<FJsonObject>* SummaryPtr = nullptr;
	const TSharedPtr<FJsonObject> Summary = Root->TryGetObjectField(TEXT("summary"), SummaryPtr) && SummaryPtr ? *SummaryPtr : nullptr;
	const FString Scenario = ReadString(Root, TEXT("scenario"));
	const FString Status = ReadString(Summary, TEXT("status"), TEXT("not_available"));
	const int32 FindingCount = static_cast<int32>(ReadNumber(Summary, TEXT("finding_count")));
	const double P99Ms = ReadNumber(Summary, TEXT("p99_frame_ms"));
	const double HitchesPerMinute = ReadNumber(Summary, TEXT("hitches_per_minute"));
	StatusText->SetText(FText::Format(
		LOCTEXT("Summary", "{0}  |  {1}  |  {2} findings  |  p99 {3} ms  |  {4} hitches/min\n{5}"),
		FText::FromString(Scenario), FText::FromString(Status), FText::AsNumber(FindingCount),
		FText::AsNumber(P99Ms), FText::AsNumber(HitchesPerMinute), FText::FromString(ReportPath)));

	const TSharedPtr<FJsonObject>* CiPtr = nullptr;
	if (Root->TryGetObjectField(TEXT("ci"), CiPtr) && CiPtr && CiPtr->IsValid())
	{
		bool bEnabled = false;
		bool bPassed = true;
		(*CiPtr)->TryGetBoolField(TEXT("enabled"), bEnabled);
		(*CiPtr)->TryGetBoolField(TEXT("passed"), bPassed);
		if (bEnabled)
		{
			AddMessage(FText::FromString(bPassed ? TEXT("CI gates: PASS") : TEXT("CI gates: FAIL")));
		}
	}

	const TSharedPtr<FJsonObject>* CoveragePtr = nullptr;
	if (Root->TryGetObjectField(TEXT("coverage"), CoveragePtr) && CoveragePtr && CoveragePtr->IsValid())
	{
		bool bLaunchRequirementsSatisfied = true;
		(*CoveragePtr)->TryGetBoolField(TEXT("launch_requirements_satisfied"), bLaunchRequirementsSatisfied);
		if (!bLaunchRequirementsSatisfied)
		{
			AddMessage(LOCTEXT("LaunchCoverageWarning", "Coverage warning: this process was not launched with the selected profile's required startup arguments."));
		}
		const TArray<TSharedPtr<FJsonValue>>* Missing = nullptr;
		if ((*CoveragePtr)->TryGetArrayField(TEXT("missing_requested_providers"), Missing) && Missing && Missing->Num() > 0)
		{
			TArray<FString> Names;
			for (const TSharedPtr<FJsonValue>& Value : *Missing)
			{
				Names.Add(Value->AsString());
			}
			AddMessage(FText::FromString(FString::Printf(TEXT("Coverage warning: missing %s"), *FString::Join(Names, TEXT(", ")))));
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* Findings = nullptr;
	if (!Root->TryGetArrayField(TEXT("findings"), Findings) || !Findings)
	{
		AddMessage(LOCTEXT("NoFindingsField", "The report contains no findings array."));
		return;
	}

	for (const TSharedPtr<FJsonValue>& Value : *Findings)
	{
		const TSharedPtr<FJsonObject> Finding = Value.IsValid() ? Value->AsObject() : nullptr;
		if (!Finding.IsValid())
		{
			continue;
		}
		const FString Severity = ReadString(Finding, TEXT("severity"), TEXT("info"));
		const FString Title = ReadString(Finding, TEXT("title"));
		const FString Category = ReadString(Finding, TEXT("category"));
		const double Confidence = ReadNumber(Finding, TEXT("confidence"));
		FString EvidenceText;
		const TArray<TSharedPtr<FJsonValue>>* Evidence = nullptr;
		if (Finding->TryGetArrayField(TEXT("evidence"), Evidence) && Evidence)
		{
			TArray<FString> Lines;
			for (const TSharedPtr<FJsonValue>& Item : *Evidence)
			{
				Lines.Add(FString::Printf(TEXT("• %s"), *Item->AsString()));
			}
			EvidenceText = FString::Join(Lines, TEXT("\n"));
		}
		const FString NextStep = ReadString(Finding, TEXT("suggested_next_step"), TEXT(""));

		ResultsBox->AddSlot().AutoHeight().Padding(3.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
			.BorderBackgroundColor(SeverityColor(Severity))
			.Padding(8.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("[%s] %s"), *Severity.ToUpper(), *Title)))
					.Font(FAppStyle::GetFontStyle(TEXT("BoldFont")))
					.AutoWrapText(true)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
				[
					SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("%s | confidence %.0f%%"), *Category, Confidence * 100.0))).AutoWrapText(true)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
				[
					SNew(STextBlock).Text(FText::FromString(EvidenceText)).AutoWrapText(true)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
				[
					SNew(STextBlock).Text(FText::FromString(NextStep.IsEmpty() ? TEXT("") : FString::Printf(TEXT("Next: %s"), *NextStep))).AutoWrapText(true)
				]
			]
		];
	}
}

#undef LOCTEXT_NAMESPACE
