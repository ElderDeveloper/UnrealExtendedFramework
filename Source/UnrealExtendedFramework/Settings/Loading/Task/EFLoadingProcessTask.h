// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/CoreUObject/Public/UObject/Object.h"
#include "UnrealExtendedFramework/Settings/Loading/EFLoadingProcessInterface.h"
#include "EFLoadingProcessTask.generated.h"

struct FFrame;

UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFLoadingProcessTask : public UObject , public IEFLoadingProcessInterface
{
	GENERATED_BODY()
	
public:
	UEFLoadingProcessTask() { }
	/*
	UFUNCTION(BlueprintCallable, meta=(WorldContext = "WorldContextObject"))
	static UEFLoadingProcessTask* CreateLoadingScreenProcessTask(UObject* WorldContextObject, const FString& ShowLoadingScreenReason);
	
	UFUNCTION(BlueprintCallable)
	void Unregister();

	UFUNCTION(BlueprintCallable)
	void SetShowLoadingScreenReason(const FString& InReason);

	virtual bool ShouldShowLoadingScreen(FString& OutReason) const override;
	
	FString Reason;
	*/
};
