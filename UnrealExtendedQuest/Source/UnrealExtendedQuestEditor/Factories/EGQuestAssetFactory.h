// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "Factories/Factory.h"

#include "EGQuestAssetFactory.generated.h"

/**
 * Factory for quests. Editor does the magic here, without this class,
 * you won't have the right click "Dialog System" -> "Quest"
 */
UCLASS()
class UNREALEXTENDEDQUESTEDITOR_API UEGQuestAssetFactory : public UFactory
{
	GENERATED_BODY()

public:
	UEGQuestAssetFactory(const FObjectInitializer& ObjectInitializer);

	//
	// UFactory interface
	//
	UObject* FactoryCreateNew(
		UClass* Class,
		UObject* InParent,
		FName Name,
		EObjectFlags Flags,
		UObject* Context,
		FFeedbackContext* Warn
	) override;
};
