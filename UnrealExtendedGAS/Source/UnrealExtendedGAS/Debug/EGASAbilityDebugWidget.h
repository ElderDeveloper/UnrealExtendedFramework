#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "EGASAbilityDebugWidget.generated.h"

UCLASS()
class UNREALEXTENDEDGAS_API UEGASAbilityDebugWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Unreal Extended GAS|Debug")
	void SetObservedActor(AActor* InObservedActor);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Unreal Extended GAS|Debug")
	AActor* GetObservedActor() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Unreal Extended GAS|Debug")
	TArray<FString> GetDebugLines() const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug")
	TObjectPtr<AActor> ObservedActor;
};
