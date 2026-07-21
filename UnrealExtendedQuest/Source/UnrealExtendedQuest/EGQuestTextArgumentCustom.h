// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once
#include "EGQuestObject.h"

#include "EGQuestTextArgumentCustom.generated.h"

class UEGQuestContext;


// Abstract base class for a custom text argument
// Extend this class to define additional data you want to store
//
// 1. Override GetText
// 2. Return the new Text for the specified text argument
UCLASS(Blueprintable, BlueprintType, Abstract, EditInlineNew)
class UNREALEXTENDEDQUEST_API UEGQuestTextArgumentCustom : public UEGQuestObject
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Quest")
	FText GetText(const UEGQuestContext* Context, const FString& DisplayStringParam);
	virtual FText GetText_Implementation(const UEGQuestContext* Context, const FString& DisplayStringParam)
	{
		return FText::GetEmpty();
	}
};

// This is the same as UEGQuestTextArgumentCustom but it does NOT show the categories
UCLASS(Blueprintable, BlueprintType, Abstract, EditInlineNew, CollapseCategories)
class UNREALEXTENDEDQUEST_API UEGQuestTextArgumentCustomHideCategories : public UEGQuestTextArgumentCustom
{
	GENERATED_BODY()
};
