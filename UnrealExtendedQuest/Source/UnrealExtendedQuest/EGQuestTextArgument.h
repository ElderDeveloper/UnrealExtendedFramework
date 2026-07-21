// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "EGQuestTextArgumentCustom.h"
#include "EGQuestTextArgument.generated.h"

class UEGQuestContext;

USTRUCT(BlueprintType)
struct UNREALEXTENDEDQUEST_API FEGQuestTextArgument
{
	GENERATED_BODY()

	bool operator==(const FEGQuestTextArgument& Other) const
	{
		return DisplayString == Other.DisplayString && CustomTextArgument == Other.CustomTextArgument;
	}

	FFormatArgumentValue ConstructFormatArgumentValue(const UEGQuestContext& Context) const;
	static void UpdateTextArgumentArray(const FText& Text, TArray<FEGQuestTextArgument>& InOutArgumentArray);

	// The {placeholder} in the owning text this argument resolves.
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Text Argument")
	FString DisplayString;

	// Supplies the text for this placeholder. An unset argument resolves to empty text.
	UPROPERTY(Instanced, EditAnywhere, BlueprintReadWrite, Category = "Text Argument")
	TObjectPtr<UEGQuestTextArgumentCustom> CustomTextArgument = nullptr;
};

template<> struct TStructOpsTypeTraits<FEGQuestTextArgument> : TStructOpsTypeTraitsBase2<FEGQuestTextArgument>
{
	enum { WithIdenticalViaEquality = true };
};
