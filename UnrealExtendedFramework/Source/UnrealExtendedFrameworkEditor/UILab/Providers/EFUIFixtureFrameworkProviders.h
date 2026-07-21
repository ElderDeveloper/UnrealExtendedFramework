// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UILab/Fixture/EFUIFixture.h"
#include "EFUIFixtureFrameworkProviders.generated.h"

/**
 * UL-7 reference provider: pushes a sample subtitle into a hosted UEFSubtitleDisplayWidget,
 * so subtitle presentation (speaker, color, rich text, duration) can be iterated without
 * PIE or a live subtitle subsystem. The reference implementation for typed framework providers.
 */
UCLASS(meta = (DisplayName = "Subtitle Sample"))
class UNREALEXTENDEDFRAMEWORKEDITOR_API UEFUIFixtureSubtitleSampleConfig : public UEFUIFixtureProviderConfig
{
	GENERATED_BODY()

public:
	/** Subtitle text (rich text markup supported). */
	UPROPERTY(EditAnywhere, Category = "Provider")
	FText Text;

	UPROPERTY(EditAnywhere, Category = "Provider")
	FText SpeakerName;

	UPROPERTY(EditAnywhere, Category = "Provider")
	FLinearColor SpeakerColor = FLinearColor::White;

	/** 0 = auto-calculate from text length. */
	UPROPERTY(EditAnywhere, Category = "Provider", meta = (ClampMin = "0.0"))
	float Duration = 5.0f;

	virtual void ApplyToWidget(UUserWidget& Widget) const override;
};

/**
 * UL-7 reference provider for modular settings widgets: refreshes (and optionally loads)
 * the modular settings subsystem after instantiation so settings widgets populate with real
 * values. Requires the Runtime-Faithful Sandbox host mode with the settings subsystem listed
 * in the fixture's RequiredSubsystems.
 */
UCLASS(meta = (DisplayName = "Modular Settings"))
class UNREALEXTENDEDFRAMEWORKEDITOR_API UEFUIFixtureModularSettingsConfig : public UEFUIFixtureProviderConfig
{
	GENERATED_BODY()

public:
	/** Load persisted settings from disk before refreshing. */
	UPROPERTY(EditAnywhere, Category = "Provider")
	bool bLoadFromDisk = false;

	virtual void ApplyToWidget(UUserWidget& Widget) const override;
};
