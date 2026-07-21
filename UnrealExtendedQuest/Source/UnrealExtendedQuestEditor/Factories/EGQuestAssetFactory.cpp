// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "EGQuestAssetFactory.h"

#include "UnrealExtendedQuest/EGQuestGraph.h"

#define LOCTEXT_NAMESPACE "QuestPlugin"

/////////////////////////////////////////////////////
// UEGQuestAssetFactory
UEGQuestAssetFactory::UEGQuestAssetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;

	// true if the associated editor should be opened after creating a new object.
	bEditAfterNew = true;
	SupportedClass = UEGQuestGraph::StaticClass();
}

UObject* UEGQuestAssetFactory::FactoryCreateNew(
	UClass* Class,
	UObject* InParent,
	FName Name,
	EObjectFlags Flags,
	UObject* Context,
	FFeedbackContext* Warn
)
{
	UEGQuestGraph* NewQuest = NewObject<UEGQuestGraph>(InParent, Class, Name, Flags | RF_Transactional);
	return NewQuest;
}

#undef LOCTEXT_NAMESPACE
