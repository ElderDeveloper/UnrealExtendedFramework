// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once
#include "UObject/Object.h"

#include "EGQuestObject.generated.h"

// Our Quest base object
UCLASS(Abstract, ClassGroup = "Quest", HideCategories = ("DoNotShow"), AutoExpandCategories = ("Default"))
class UNREALEXTENDEDQUEST_API UEGQuestObject : public UObject
{
	GENERATED_BODY()
public:
	// UObject interface
	void PostInitProperties() override;
	UWorld* GetWorld() const override;
};
