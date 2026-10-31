// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGInteractionPromptWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"

void UEGInteractionPromptWidget::NativeConstruct()
{
	// The Designer's static icon, before any prompt (a key glyph, a StaticIcon) can have replaced it.
	if (InteractionIcon && !bHasAuthoredStaticBrush)
	{
		AuthoredStaticBrush = InteractionIcon->GetBrush();
		bHasAuthoredStaticBrush = true;
	}

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

	bShowingStaticIcon = bUseStaticIcon && !bPresentationOverridesStaticIcon;

	if (InteractionText)
	{
		InteractionText->SetText(bShowingStaticIcon ? FText::GetEmpty() : CurrentPrompt);
		InteractionText->SetVisibility(bShowingStaticIcon ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}

	if (bShowingStaticIcon)
	{
		// Keep the brush authored in the Designer: never write the interactable's ordinary icon here. Only
		// a presentation that asked for a static icon of its own swaps the picture, keeping the brush's
		// size and tint; the next prompt without one puts the authored picture back.
		if (InteractionIcon)
		{
			if (!bHasAuthoredStaticBrush)
			{
				AuthoredStaticBrush = InteractionIcon->GetBrush();
				bHasAuthoredStaticBrush = true;
			}
			FSlateBrush Brush = AuthoredStaticBrush;
			if (PresentationStaticIcon)
			{
				Brush.SetResourceObject(PresentationStaticIcon);
			}
			InteractionIcon->SetBrush(Brush);
			InteractionIcon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		return;
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

void UEGInteractionPromptWidget::ShowPresentation(const FEGInteractionPresentation& Presentation)
{
	TGuardValue<bool> OverrideGuard(bPresentationOverridesStaticIcon, Presentation.bOverrideStaticIcon);
	TGuardValue<TObjectPtr<UTexture2D>> StaticIconGuard(PresentationStaticIcon, Presentation.StaticIcon);
	TGuardValue<FText> HintGuard(PresentationHintText, Presentation.HintText);
	TGuardValue<bool> HintKeyGuard(bPresentationHintShowsKey, Presentation.bHintShowsKey);
	ShowPrompt(Presentation.Text, Presentation.Icon);
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
