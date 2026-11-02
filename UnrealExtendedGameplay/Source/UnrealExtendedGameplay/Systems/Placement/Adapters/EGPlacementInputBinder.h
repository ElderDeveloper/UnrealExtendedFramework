// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "EGPlacementInputBinder.generated.h"

class APlayerController;
class UEGPlacementSessionComponent;
class UEnhancedInputComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionInstance;

/**
 * Binds Enhanced Input actions to a placement session: adds the mapping context at the priority
 * the project chooses and binds the configured actions to the session's semantic calls. It removes
 * only its own context and bindings, and never touches the project's own context stack or modal
 * rules. Bind when a session starts and Unbind when it ends (or set bUnbindWhenSessionEnds).
 *
 * Held modifiers (free position, free rotation) are cleared on release.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, CollapseCategories)
class UNREALEXTENDEDGAMEPLAY_API UEGPlacementInputBinder : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputMappingContext> MappingContext;

	/** Above the gameplay contexts that map the same keys. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	int32 Priority = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> ConfirmAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> CancelAction;

	/** Axis1D; its sign picks the direction, one spin step per trigger. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> RotateAction;

	/** Axis1D; the value is passed to AdjustDistance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> AdjustDistanceAction;

	/** Held. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> FreePositionAction;

	/** Held. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> FreeRotationAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bUnbindWhenSessionEnds = true;

	/** Binds to Session through Player's Enhanced Input component and local player subsystem. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Placement")
	bool Bind(UEGPlacementSessionComponent* Session, APlayerController* Player);

	UFUNCTION(BlueprintCallable, Category = "Extended|Placement")
	void Unbind();

	UFUNCTION(BlueprintPure, Category = "Extended|Placement")
	bool IsBound() const { return BoundSession.IsValid(); }

	virtual void BeginDestroy() override;

private:
	void HandleConfirm(const FInputActionInstance& Instance);
	void HandleCancel(const FInputActionInstance& Instance);
	void HandleRotate(const FInputActionInstance& Instance);
	void HandleAdjustDistance(const FInputActionInstance& Instance);
	void HandleFreePosition(const FInputActionInstance& Instance);
	void HandleFreePositionReleased(const FInputActionInstance& Instance);
	void HandleFreeRotation(const FInputActionInstance& Instance);
	void HandleFreeRotationReleased(const FInputActionInstance& Instance);

	UFUNCTION()
	void HandleSessionEnded(bool bCancelled);

	TWeakObjectPtr<UEGPlacementSessionComponent> BoundSession;
	TWeakObjectPtr<APlayerController> BoundPlayer;
	TWeakObjectPtr<UEnhancedInputComponent> BoundInput;
	TArray<uint32> Handles;
	bool bAddedContext = false;
};
