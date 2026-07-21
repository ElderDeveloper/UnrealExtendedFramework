// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

extern const FName PerfSentinelReportTabName;

/** Lightweight in-editor reader for the newest deterministic PerfSentinel report. */
class SPerfSentinelReportView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPerfSentinelReportView) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void RefreshReport();

private:
	FReply HandleRefresh();
	FReply HandleOpenReport();
	FReply HandleOpenFolder();
	FString FindNewestReport() const;
	void AddMessage(const FText& Message);

	TSharedPtr<class STextBlock> StatusText;
	TSharedPtr<class SVerticalBox> ResultsBox;
	FString ReportPath;
};
