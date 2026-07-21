// EFSubtitleStyleProfile.h - Swappable accessibility / visual style profile
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Fonts/SlateFontInfo.h"
#include "EFSubtitleData.h"
#include "EFSubtitleStyleProfile.generated.h"

/**
 * Optional visual style profile for subtitles.
 * Selectable via ModularSettings without changing the widget class.
 */
UCLASS(BlueprintType)
class UNREALEXTENDEDFRAMEWORK_API UEFSubtitleStyleProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Stable id used by ModularSettings multi-select Values. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile")
	FName ProfileId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	FSlateFontInfo Font;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	FLinearColor FontColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	FVector2D ShadowOffset = FVector2D(1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	FLinearColor ShadowColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.5f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	bool bUseBackground = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance",
		meta = (EditCondition = "bUseBackground", EditConditionHides))
	FEFSubtitleBackgroundSettings BackgroundSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	bool bUseBorder = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance",
		meta = (EditCondition = "bUseBorder", EditConditionHides))
	FEFSubtitleBorderSettings BorderSettings;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("SubtitleStyleProfile"), ProfileId.IsNone() ? GetFName() : ProfileId);
	}
};
