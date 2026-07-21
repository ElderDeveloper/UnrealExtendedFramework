// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "Factories/Factory.h"

#include "EGQuestEventCustomFactory.generated.h"

class UEGQuestEventCustom;

UCLASS()
class UNREALEXTENDEDQUESTEDITOR_API UEGQuestEventCustomFactory : public UFactory
{
	GENERATED_BODY()

public:
	UEGQuestEventCustomFactory(const FObjectInitializer& ObjectInitializer);

	//
	// UFactory interface
	//

	bool ConfigureProperties() override;
	UObject* FactoryCreateNew(
		UClass* Class,
		UObject* InParent,
		FName Name,
		EObjectFlags Flags,
		UObject* Context,
		FFeedbackContext* Warn
	) override;

private:
	// Holds the template of the class we are building
	UPROPERTY()
	TSubclassOf<UEGQuestEventCustom> ParentClass;
};
