// EFSubtitleDisplayWidget.cpp
#include "EFSubtitleDisplayWidget.h"

#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "UnrealExtendedFramework/Systems/Subtitle/Data/EFSubtitleProjectSettings.h"
#include "UnrealExtendedFramework/Systems/Subtitle/Data/EFSubtitleStyleProfile.h"

void UEFSubtitleDisplayWidget::ApplyVisualSettings()
{
	ApplyPresentationState(FEFSubtitlePresentationState(), nullptr);
}

void UEFSubtitleDisplayWidget::ApplyPresentationState(const FEFSubtitlePresentationState& State, UEFSubtitleStyleProfile* StyleProfile)
{
	CachedPresentationState = State;
	CachedStyleProfile = StyleProfile;

	const UEFSubtitleProjectSettings* ProjectSettings = GetDefault<UEFSubtitleProjectSettings>();
	if (!ProjectSettings)
	{
		return;
	}

	FSlateFontInfo Font = ProjectSettings->DefaultFont;
	FLinearColor FontColor = ProjectSettings->DefaultFontColor;
	FVector2D ShadowOffset = ProjectSettings->ShadowOffset;
	FLinearColor ShadowColor = ProjectSettings->ShadowColor;
	bUseBackground = ProjectSettings->bUseBackground;
	FLinearColor BackgroundColor = ProjectSettings->BackgroundSettings.BackgroundColor;
	float BackgroundOpacity = ProjectSettings->BackgroundSettings.BackgroundOpacity;

	if (StyleProfile)
	{
		Font = StyleProfile->Font;
		FontColor = StyleProfile->FontColor;
		ShadowOffset = StyleProfile->ShadowOffset;
		ShadowColor = StyleProfile->ShadowColor;
		bUseBackground = StyleProfile->bUseBackground;
		BackgroundColor = StyleProfile->BackgroundSettings.BackgroundColor;
		BackgroundOpacity = StyleProfile->BackgroundSettings.BackgroundOpacity;
	}

	// User opacity preference overrides profile/project baseline.
	BackgroundOpacity = State.BackgroundOpacity;

	if (Font.Size > 0)
	{
		Font.Size = FMath::RoundToInt(static_cast<float>(Font.Size) * FMath::Max(State.TextScale, 0.1f));
	}

	if (SubtitleText)
	{
		SubtitleText->SetFont(Font);
		SubtitleText->SetColorAndOpacity(FontColor);
		SubtitleText->SetShadowOffset(ShadowOffset);
		SubtitleText->SetShadowColorAndOpacity(ShadowColor);
	}

	if (SpeakerLabel)
	{
		SpeakerLabel->SetFont(Font);
		if (!CachedPresentationState.bShowSpeakerLabels)
		{
			SpeakerLabel->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (BackgroundBorder)
	{
		if (bUseBackground)
		{
			FLinearColor BrushColor = BackgroundColor;
			BrushColor.A = BackgroundOpacity;
			BackgroundBorder->SetBrushColor(BrushColor);
			BackgroundBorder->SetRenderOpacity(1.0f);
		}
		else
		{
			BackgroundBorder->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UEFSubtitleDisplayWidget::ShowSubtitle_Implementation(const FEFSubtitleEntry& Entry, const FEFSubtitleRequest& Request)
{
	if (SpeakerLabel)
	{
		if (CachedPresentationState.bShowSpeakerLabels && !Entry.SpeakerName.IsEmpty())
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

	if (SubtitleText)
	{
		SubtitleText->SetText(Entry.Text);
		SubtitleText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	if (BackgroundBorder && bUseBackground)
	{
		BackgroundBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
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
}

void UEFSubtitleDisplayWidget::AddStackedSubtitle_Implementation(const FEFSubtitleEntry& Entry, int32 RequestId)
{
	if (!StackContainer)
	{
		FEFSubtitleRequest DummyRequest;
		DummyRequest.RequestId = RequestId;
		ShowSubtitle(Entry, DummyRequest);
		return;
	}

	UTextBlock* NewTextBlock = NewObject<UTextBlock>(this);
	if (!NewTextBlock)
	{
		return;
	}

	const UEFSubtitleProjectSettings* Settings = GetDefault<UEFSubtitleProjectSettings>();
	FSlateFontInfo Font = Settings ? Settings->DefaultFont : FSlateFontInfo();
	FLinearColor FontColor = Settings ? Settings->DefaultFontColor : FLinearColor::White;
	FVector2D ShadowOffset = Settings ? Settings->ShadowOffset : FVector2D(1.f, 1.f);
	FLinearColor ShadowColor = Settings ? Settings->ShadowColor : FLinearColor(0.f, 0.f, 0.f, 0.5f);

	if (CachedStyleProfile)
	{
		Font = CachedStyleProfile->Font;
		FontColor = CachedStyleProfile->FontColor;
		ShadowOffset = CachedStyleProfile->ShadowOffset;
		ShadowColor = CachedStyleProfile->ShadowColor;
	}

	if (Font.Size > 0)
	{
		Font.Size = FMath::RoundToInt(static_cast<float>(Font.Size) * FMath::Max(CachedPresentationState.TextScale, 0.1f));
	}

	NewTextBlock->SetFont(Font);
	NewTextBlock->SetColorAndOpacity(FontColor);
	NewTextBlock->SetShadowOffset(ShadowOffset);
	NewTextBlock->SetShadowColorAndOpacity(ShadowColor);

	FString DisplayText;
	if (CachedPresentationState.bShowSpeakerLabels && !Entry.SpeakerName.IsEmpty())
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

	if (BackgroundBorder && bUseBackground)
	{
		BackgroundBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
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

	if (StackedEntries.Num() == 0 && BackgroundBorder)
	{
		BackgroundBorder->SetVisibility(ESlateVisibility::Collapsed);
	}
}
