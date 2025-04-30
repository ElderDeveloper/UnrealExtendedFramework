#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Styling/SlateTypes.h"
#include "Sound/SoundBase.h"
#include "EFButtonStyle.generated.h"

/**
 * Custom button style class that can be created in the editor
 * Similar to CommonUIButtonStyle but simplified for the Horror Plugin
 */
UCLASS(BlueprintType, Blueprintable)
class UNREALEXTENDEDFRAMEWORK_API UEFButtonStyle : public UObject
{
	GENERATED_BODY()

public:
	UEFButtonStyle()
	{}

	// Normal state appearance
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button Style")
	FButtonStyle ButtonStyle;
};