// Fill out your copyright notice in the Description page of Project Settings.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/GameInstanceSubsystem.h" 
#include "UnrealExtendedFramework/Settings/Subtitle/Data/EFSubtitleData.h"
#include "EFSubtitleSubsystem.generated.h"


class UEFSubtitleWidget;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnExecuteSubtitle, FString, Subtitle, float, Duration);

DECLARE_LOG_CATEGORY_EXTERN(LogExtendedSubtitleSubsystem, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogExtendedSubtitleSubsystemError, Error, All);
DECLARE_LOG_CATEGORY_EXTERN(LogExtendedSubtitleSubsystemWarning, Warning, All);

UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFSubtitleSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
protected:

	UPROPERTY(BlueprintReadWrite)
	UEFSubtitleWidget* SubtitleWidget;

	UPROPERTY()
	TSoftObjectPtr<UDataTable> CurrentSubtitleDataTable;

public:
	
	UPROPERTY(BlueprintAssignable)
	FOnExecuteSubtitle OnExecuteSubtitle;
	
	// Triggers the Subtitle Event , OnExecuteSubtitle will be triggered , Audio Will Be Played On 2D Sound 
	UFUNCTION(BlueprintCallable ,meta=(WorldContext = WorldContextObject) , Category="Extended Settings | Subtitle")
	void ExecuteExtendedSubtitle(const UObject* WorldContextObject , const FString SubtitleKey);

	// Triggers the Subtitle Event , OnExecuteSubtitle will be triggered , Audio Will Be Played On The Given Location
	UFUNCTION(BlueprintCallable ,meta=(WorldContext = WorldContextObject) , Category="Extended Settings | Subtitle")
	void ExecuteExtendedSubtitleLocation(const UObject* WorldContextObject ,const FString SubtitleKey , const FVector Location);

protected:

	void CheckInitializeExtendedSubtitleWidget();
	bool CheckCurrentSubtitleDataTable();
	
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }

	FExtendedSubtitle GetSubtitle(const FString& SubtitleKey) const;
	
};
