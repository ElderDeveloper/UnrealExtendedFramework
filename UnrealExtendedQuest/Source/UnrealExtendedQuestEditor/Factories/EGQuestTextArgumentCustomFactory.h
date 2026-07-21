// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "Factories/Factory.h"

#include "EGQuestTextArgumentCustomFactory.generated.h"

class UEGQuestTextArgumentCustom;

UCLASS()
class UNREALEXTENDEDQUESTEDITOR_API UEGQuestTextArgumentCustomFactory : public UFactory
{
	GENERATED_BODY()

public:
	UEGQuestTextArgumentCustomFactory(const FObjectInitializer& ObjectInitializer);

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
	TSubclassOf<UEGQuestTextArgumentCustom> ParentClass;
};
