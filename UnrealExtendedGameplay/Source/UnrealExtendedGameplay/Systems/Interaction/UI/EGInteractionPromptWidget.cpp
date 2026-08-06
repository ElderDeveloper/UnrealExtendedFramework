// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGInteractionPromptWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"

void UEGInteractionPromptWidget::NativeConstruct()
{
	Super::NativeConstruct();
	HidePrompt();
}

void UEGInteractionPromptWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bCursorFollowEnabled || GetVisibility() == ESlateVisibility::Collapsed)
	{
		return;
	}

	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		float MouseX = 0.0f;
		float MouseY = 0.0f;
		if (PlayerController->GetMousePosition(MouseX, MouseY))
		{
			SetPositionInViewport(FVector2D(MouseX, MouseY) + CursorFollowOffset);
		}
	}
}

void UEGInteractionPromptWidget::ShowPrompt_Implementation(const FText& InPrompt, UTexture2D* InIcon)
{
	CurrentPrompt = InPrompt;
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	if (InteractionText)
	{
		InteractionText->SetText(CurrentPrompt);
	}

	if (InteractionIcon)
	{
		InteractionIcon->SetVisibility(InIcon ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		if (InIcon)
		{
			InteractionIcon->SetBrushFromTexture(InIcon);
		}
	}
}

void UEGInteractionPromptWidget::SetHoldProgress_Implementation(float InProgress)
{
	HoldProgress = FMath::Clamp(InProgress, 0.0f, 1.0f);
	bHoldInteractionActive = HoldProgress > 0.0f;

	if (HoldProgressImage)
	{
		HoldProgressImage->SetVisibility(bHoldInteractionActive ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		if (UMaterialInstanceDynamic* Material = HoldProgressImage->GetDynamicMaterial())
		{
			Material->SetScalarParameterValue(TEXT("Progress"), HoldProgress);
		}
	}
}

void UEGInteractionPromptWidget::HidePrompt_Implementation()
{
	CurrentPrompt = FText::GetEmpty();
	SetHoldProgress(0.0f);
	SetVisibility(ESlateVisibility::Collapsed);
}

void UEGInteractionPromptWidget::SetCursorFollowMode(bool bEnable)
{
	bCursorFollowEnabled = bEnable;
}
