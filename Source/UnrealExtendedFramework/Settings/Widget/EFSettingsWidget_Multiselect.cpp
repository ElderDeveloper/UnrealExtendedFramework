// Fill out your copyright notice in the Description page of Project Settings.


#include "EFSettingsWidget_Multiselect.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"


void UEFSettingsWidget_Multiselect::OnRootButtonClicked()
{
}


void UEFSettingsWidget_Multiselect::OnRootButtonHovered()
{
}


void UEFSettingsWidget_Multiselect::OnRootButtonUnhovered()
{
}


void UEFSettingsWidget_Multiselect::OnNextOptionClicked()
{
	if (Options.Num() > 0)
	{
		SelectedOptionIndex = (SelectedOptionIndex + 1) % Options.Num();
		SelectedOptionText = Options[SelectedOptionIndex];
		UpdateWidgetVisuals();
		OnMultiselectOptionChanged.Broadcast(SelectedOptionIndex, SelectedOptionText);
	}
}


void UEFSettingsWidget_Multiselect::OnPreviousOptionClicked()
{
	if (Options.Num() > 0)
	{
		SelectedOptionIndex = (SelectedOptionIndex - 1 + Options.Num()) % Options.Num();
		SelectedOptionText = Options[SelectedOptionIndex];
		UpdateWidgetVisuals();
		OnMultiselectOptionChanged.Broadcast(SelectedOptionIndex, SelectedOptionText);
	}
}


void UEFSettingsWidget_Multiselect::UpdateWidgetVisuals()
{
	if (SelectedOption)
	{
		SelectedOption->SetText(SelectedOptionText);
	}

	if (SettingsText)
	{
		SettingsText->SetText(OptionText);
	}
}


void UEFSettingsWidget_Multiselect::SetSelectedOption(FText Text)
{
	SelectedOptionText = Text;
	SelectedOptionIndex = Options.IndexOfByPredicate([&](const FText& Option) {
		return SelectedOptionText.EqualTo(Option);
	});
	
	if (SelectedOptionIndex == INDEX_NONE)
	{
		SelectedOptionIndex = 0;
		SelectedOptionText = Options[0];
	}
	UpdateWidgetVisuals();
	OnMultiselectOptionChanged.Broadcast(SelectedOptionIndex, SelectedOptionText);
}


void UEFSettingsWidget_Multiselect::SetSelectedOptionIndex(int32 Index)
{
	if (Index >= 0 && Index < Options.Num())
	{
		SelectedOptionIndex = Index;
		SelectedOptionText = Options[SelectedOptionIndex];
		UpdateWidgetVisuals();
		OnMultiselectOptionChanged.Broadcast(SelectedOptionIndex, SelectedOptionText);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid index for multiselect option: %d"), Index);
	}
}


void UEFSettingsWidget_Multiselect::NativeConstruct()
{
	Super::NativeConstruct();

	if (RootButton)
	{
		RootButton->OnClicked.AddDynamic(this, &UEFSettingsWidget_Multiselect::OnRootButtonClicked);
		RootButton->OnHovered.AddDynamic(this, &UEFSettingsWidget_Multiselect::OnRootButtonHovered);
		RootButton->OnUnhovered.AddDynamic(this, &UEFSettingsWidget_Multiselect::OnRootButtonUnhovered);
	}

	if (ButtonNextOption)
	{
		ButtonNextOption->OnClicked.AddDynamic(this, &UEFSettingsWidget_Multiselect::OnNextOptionClicked);
	}

	if (ButtonPreviousOption)
	{
		ButtonPreviousOption->OnClicked.AddDynamic(this, &UEFSettingsWidget_Multiselect::OnPreviousOptionClicked);
	}

	if (SelectedOption)
	{
		if (SelectedOptionText.IsEmpty())
		{
			SelectedOptionText = Options[0];
			SelectedOptionIndex = 0;
		}
		else
		{
			SelectedOptionIndex = Options.IndexOfByPredicate([&](const FText& Option) {
				return SelectedOptionText.EqualTo(Option);
			});
			
			if (SelectedOptionIndex == INDEX_NONE)
			{
				SelectedOptionIndex = 0;
				SelectedOptionText = Options[0];
			}
		}
		SelectedOption->SetText(SelectedOptionText);
	}
	if (SettingsText)
	{
		SettingsText->SetText(OptionText);
	}
}
