// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "EFWaitModularSettingsPendingChanges.generated.h"

class UEFModularSettingsBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPendingSettingsStateChanged, bool, bHasPendingChanges);

/**
 * Async blueprint node to listen for changes to the global "Pending Changes" state.
 * Useful for showing/hiding a global "Apply" button as safely as possible.
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFWaitModularSettingsPendingChanges : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	// Delegate to notify Blueprint when pending state changes
	UPROPERTY(BlueprintAssignable)
	FOnPendingSettingsStateChanged OnStateChanged;

	// Factory function for Blueprint
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"), Category = "Modular Settings|Async")
	static UEFWaitModularSettingsPendingChanges* WaitForModularSettingsPendingChanges(UObject* WorldContextObject);

	virtual void Activate() override;

protected:
	UPROPERTY()
	UObject* WorldContextObject;

	UPROPERTY()
	bool bLastKnownPendingState = false;

	UFUNCTION()
	void OnSettingChanged(UEFModularSettingsBase* ChangedSetting);
	
	UFUNCTION()
	void CheckPendingState();
};
