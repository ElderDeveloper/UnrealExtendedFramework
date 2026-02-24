// EFSubtitleDisplayWidget.cpp

#include "EFSubtitleDisplayWidget.h"
#include "UnrealExtendedFramework/Settings/Subtitle/Data/EFSubtitleProjectSettings.h"
#include "Components/TextBlock.h"
#include "Components/RichTextBlock.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"


void UEFSubtitleDisplayWidget::ApplyVisualSettings()
{
	const auto* Settings = GetDefault<UEFSubtitleProjectSettings>();
	if (!Settings) return;

	if (SubtitleText)
	{
		SubtitleText->SetFont(Settings->DefaultFont);
		SubtitleText->SetColorAndOpacity(Settings->DefaultFontColor);
		SubtitleText->SetShadowOffset(Settings->ShadowOffset);
		SubtitleText->SetShadowColorAndOpacity(Settings->ShadowColor);
		SubtitleText->SetText(FText::GetEmpty());
	}

	if (SpeakerLabel)
	{
		SpeakerLabel->SetFont(Settings->DefaultFont);
		SpeakerLabel->SetText(FText::GetEmpty());
		SpeakerLabel->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (BackgroundBorder)
	{
		if (Settings->bUseBackground)
		{
			BackgroundBorder->SetBrushColor(Settings->BackgroundSettings.BackgroundColor);
			BackgroundBorder->SetRenderOpacity(Settings->BackgroundSettings.BackgroundOpacity);
			BackgroundBorder->SetVisibility(ESlateVisibility::Collapsed); // Hidden until subtitle shows
		}
		else
		{
			BackgroundBorder->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}


void UEFSubtitleDisplayWidget::ShowSubtitle_Implementation(const FEFSubtitleEntry& Entry, const FEFSubtitleRequest& Request)
{
	// Show speaker label if available
	if (SpeakerLabel)
	{
		if (!Entry.SpeakerName.IsEmpty())
		{
			SpeakerLabel->SetText(Entry.SpeakerName);
			SpeakerLabel->SetColorAndOpacity(Entry.SpeakerColor);
			SpeakerLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			SpeakerLabel->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Show subtitle text
	if (SubtitleText)
	{
		SubtitleText->SetText(Entry.Text);
		SubtitleText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	// Show background
	if (BackgroundBorder)
	{
		const auto* Settings = GetDefault<UEFSubtitleProjectSettings>();
		if (Settings && Settings->bUseBackground)
		{
			BackgroundBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}
}


void UEFSubtitleDisplayWidget::HideSubtitle_Implementation()
{
	if (SubtitleText)
	{
		SubtitleText->SetText(FText::GetEmpty());
		SubtitleText->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (SpeakerLabel)
	{
		SpeakerLabel->SetText(FText::GetEmpty());
		SpeakerLabel->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (BackgroundBorder)
	{
		BackgroundBorder->SetVisibility(ESlateVisibility::Collapsed);
	}
}


void UEFSubtitleDisplayWidget::UpdateSubtitle_Implementation(float DeltaTime, float Progress)
{
	// Base implementation does nothing — override in Blueprint for typewriter, fade, etc.
}


void UEFSubtitleDisplayWidget::AddStackedSubtitle_Implementation(const FEFSubtitleEntry& Entry, int32 RequestId)
{
	if (!StackContainer)
	{
		// Fallback: just show as normal subtitle
		FEFSubtitleRequest DummyRequest;
		DummyRequest.RequestId = RequestId;
		ShowSubtitle(Entry, DummyRequest);
		return;
	}

	// Create a new text block for this stacked entry
	UTextBlock* NewTextBlock = NewObject<UTextBlock>(this);
	if (NewTextBlock)
	{
		const auto* Settings = GetDefault<UEFSubtitleProjectSettings>();
		if (Settings)
		{
			NewTextBlock->SetFont(Settings->DefaultFont);
			NewTextBlock->SetColorAndOpacity(Settings->DefaultFontColor);
			NewTextBlock->SetShadowOffset(Settings->ShadowOffset);
			NewTextBlock->SetShadowColorAndOpacity(Settings->ShadowColor);
		}

		// Build display text: "[Speaker] Text" or just "Text"
		FString DisplayText;
		if (!Entry.SpeakerName.IsEmpty())
		{
			DisplayText = FString::Printf(TEXT("[%s] %s"), *Entry.SpeakerName.ToString(), *Entry.Text.ToString());
		}
		else
		{
			DisplayText = Entry.Text.ToString();
		}
		NewTextBlock->SetText(FText::FromString(DisplayText));

		StackContainer->AddChildToVerticalBox(NewTextBlock);
		StackedEntries.Add(RequestId, NewTextBlock);

		// Show background if configured
		if (BackgroundBorder)
		{
			const auto* BgSettings = GetDefault<UEFSubtitleProjectSettings>();
			if (BgSettings && BgSettings->bUseBackground)
			{
				BackgroundBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
		}
	}
}


void UEFSubtitleDisplayWidget::RemoveStackedSubtitle_Implementation(int32 RequestId)
{
	if (UTextBlock** FoundBlock = StackedEntries.Find(RequestId))
	{
		if (*FoundBlock)
		{
			(*FoundBlock)->RemoveFromParent();
		}
		StackedEntries.Remove(RequestId);
	}

	// Hide background if no more stacked entries
	if (StackedEntries.Num() == 0 && BackgroundBorder)
	{
		BackgroundBorder->SetVisibility(ESlateVisibility::Collapsed);
	}
}
