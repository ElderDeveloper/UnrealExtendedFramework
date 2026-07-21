// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "UnrealExtendedQuest/Nodes/EGQuestNode.h"

#include "EGQuestNode_Custom.generated.h"


/**
 * Abstract quest node, can be extended outside of the plugin to add custom data and implement custom behavior
 */
UCLASS(BlueprintType, Abstract, ClassGroup = "Quest")
class UNREALEXTENDEDQUEST_API UEGQuestNode_Custom : public UEGQuestNode
{
	GENERATED_BODY()

public:

	//
	// Check the virtual functions in UEGQuestNode to see what you need to override
	// in your own childclass to implement custom runtime behavior
	//


public:

	//
	// Editor customization
	//

	/** @return a one line description of an object. */
	virtual FString GetDesc() override { return TEXT("My Custom Desc"); }


#if WITH_EDITOR
	// Name of the node in the add node context menu
	virtual FString GetNodeTypeString() const override { return TEXT("My custom Node"); }

	/** Node parameter category name in details panel */
	virtual FName GetCategoryName() const { return TEXT("Custom"); }

	/** override and return true to not have a warning on the node if it is orphan without targeted by a proxy */
	virtual bool CanBeOrphan() const { return false; }

	virtual FLinearColor GetNodeColor() const { return FLinearColor::Gray; }

	/** Override and return to false if the node should not be able to have parents (like UEGQuestNode_Start) */
	virtual bool CanHaveInputConnections() const { return true; }

	/** Override and return to false if the node should not be able to have children (like UEGQuestNode_Proxy) */
	virtual bool CanHaveOutputConnections() const { return true; }

	/** Option to override the node title (to have a custom one instead of the node owner). Return true if you want to use OutTitle */
	virtual bool GetNodeTitleOverride(FString& OutTitle) const { return false; }
#endif
};
