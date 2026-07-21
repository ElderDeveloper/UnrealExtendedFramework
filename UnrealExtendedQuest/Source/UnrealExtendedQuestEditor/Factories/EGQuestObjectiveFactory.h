// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "Factories/Factory.h"

#include "EGQuestObjectiveFactory.generated.h"

class UEGQuestNode_Objective;

/** Creates Blueprint subclasses of UEGQuestNode_Objective: objectives with custom evaluation. */
UCLASS()
class UNREALEXTENDEDQUESTEDITOR_API UEGQuestObjectiveFactory : public UFactory
{
	GENERATED_BODY()

public:
	UEGQuestObjectiveFactory(const FObjectInitializer& ObjectInitializer);

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
	TSubclassOf<UEGQuestNode_Objective> ParentClass;
};
