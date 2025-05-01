// Fill out your copyright notice in the Description page of Project Settings.


#include "EFSubtitleSubsystem.h"
#include "UnrealExtendedFramework/Subtitle/Widget/EFSubtitleWidget.h"
#include "Blueprint/UserWidget.h"
#include "UnrealExtendedFramework/Subtitle/Data/EFSubtitlePluginSettings.h"

DEFINE_LOG_CATEGORY(LogExtendedSubtitleSubsystemError);
DEFINE_LOG_CATEGORY(LogExtendedSubtitleSubsystemWarning);
DEFINE_LOG_CATEGORY(LogExtendedSubtitleSubsystem);

void UEFSubtitleSubsystem::ExecuteExtendedSubtitle(const UObject* WorldContextObject, const FString SubtitleKey)
{
	CheckInitializeExtendedSubtitleWidget();
	CheckCurrentSubtitleDataTable();
	FExtendedSubtitle Subtitle = GetSubtitle(SubtitleKey);
	SubtitleWidget->ExecuteSubtitle(Subtitle, ESubtitleExecutionType::Boundless);
}


void UEFSubtitleSubsystem::ExecuteExtendedSubtitleLocation(const UObject* WorldContextObject, const FString SubtitleKey,const FVector Location)
{
	ExecuteExtendedSubtitle(WorldContextObject, SubtitleKey);
}


void UEFSubtitleSubsystem::CheckInitializeExtendedSubtitleWidget()
{
	if (SubtitleWidget == nullptr)
	{
		const auto SubtitleAppearance = GetDefault<UEFSubtitleSettings>();
		SubtitleWidget = Cast<UEFSubtitleWidget>(CreateWidget(GetWorld()->GetFirstPlayerController() ,SubtitleAppearance->SubtitleWidgetClass));
		SubtitleWidget->AddToViewport();
		SubtitleWidget->ApplyVisualSettings();
		return;
	}
	
	if (!SubtitleWidget->IsInViewport())
	{
		SubtitleWidget->AddToViewport();
	}
}


bool UEFSubtitleSubsystem::CheckCurrentSubtitleDataTable()
{
	if (CurrentSubtitleDataTable.IsValid() == false)
	{
		const auto SubtitleData = GetDefault<UEFSubtitleSettings>();
		if (SubtitleData->SubtitleDataTable.IsValid())
		{
			CurrentSubtitleDataTable = SubtitleData->SubtitleDataTable;
			return true;
		}
		UE_LOG(LogExtendedSubtitleSubsystemError, Error, TEXT("Current Subtitle Data Table is Invalid"));
		return false;
	}
	return true;
}


void UEFSubtitleSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}


void UEFSubtitleSubsystem::Deinitialize()
{
	Super::Deinitialize();
	if (SubtitleWidget)
	{
		SubtitleWidget->RemoveFromParent();
		SubtitleWidget = nullptr;
	}
} 


FExtendedSubtitle UEFSubtitleSubsystem::GetSubtitle(const FString& SubtitleKey) const
{
	FExtendedSubtitle* Subtitle = CurrentSubtitleDataTable->FindRow<FExtendedSubtitle>(FName(*SubtitleKey), "");
	if (Subtitle != nullptr)
	{
		return *Subtitle;
	}
	return FExtendedSubtitle();
}


