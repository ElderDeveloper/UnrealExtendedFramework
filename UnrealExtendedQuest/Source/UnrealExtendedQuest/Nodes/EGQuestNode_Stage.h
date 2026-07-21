// Copyright Devil of the Plague. All Rights Reserved.
#pragma once

#include "UnrealExtendedQuest/Nodes/EGQuestNode.h"
#include "UnrealExtendedQuest/EGQuestTextArgument.h"
#include "UnrealExtendedQuest/EGQuestRole.h"
#include "UnrealExtendedQuest/EGQuestDirective.h"

#include "EGQuestNode_Stage.generated.h"

/**
 * The scope, and the only thing that is ever active.
 *
 * A stage carries the journal entry - Title and Description - and owns the objectives the player is
 * working on, as its children. It has no conditions of its own and no completion rule: it is not a
 * join. It is simply left when a destination fires, and every objective still pending is cancelled.
 */
UCLASS(BlueprintType, ClassGroup = "Quest")
class UNREALEXTENDEDQUEST_API UEGQuestNode_Stage : public UEGQuestNode
{
	GENERATED_BODY()

public:
	// Begin UObject Interface.
	FString GetDesc() override
	{
		return TEXT("A stage of the quest: a journal entry owning the objectives active with it.");
	}

#if WITH_EDITOR
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	FString GetNodeTypeString() const override { return TEXT("Stage"); }
#endif

	//
	// Begin UEGQuestNode Interface.
	//

	/**
	 * A stage's Children are ownership edges to the objective rows it owns, built from GetObjectives()
	 * and never from pins - the routing arrows leave the objectives, not the card. So a stage never
	 * arbitrates between destinations and never carries a route priority.
	 */
	bool EmitsRoutes() const override { return false; }

	void UpdateTextsValuesFromDefaultsAndRemappings(const UEGQuestPluginSettings& Settings, bool bUpdateGraphNode = true) override;
	void UpdateTextsNamespacesAndKeys(const UEGQuestPluginSettings& Settings, bool bUpdateGraphNode = true) override;
	void RebuildConstructedText(UEGQuestContext& Context) const override;
	void RebuildTextArguments(bool bUpdateGraphNode = true) override
	{
		Super::RebuildTextArguments(bUpdateGraphNode);
		FEGQuestTextArgument::UpdateTextArgumentArray(Description, TextArguments);
	}
	void RebuildTextArgumentsFromPreview(const FText& Preview) override { FEGQuestTextArgument::UpdateTextArgumentArray(Preview, TextArguments); }
	const TArray<FEGQuestTextArgument>& GetTextArguments() const override { return TextArguments; }

	// The stage's node text is its title: it is what names the stage everywhere a node is labelled.
	const FText& GetNodeText() const override { return Title; }

	//
	// Begin own functions.
	//

	const FText& GetTitle() const { return Title; }
	void SetTitle(const FText& InTitle) { Title = InTitle; }

	FName GetStageId() const { return StageId; }
	void SetStageId(FName InStageId) { StageId = InStageId; }

	const FText& GetDescription() const { return Description; }
	void SetDescription(const FText& InDescription)
	{
		Description = InDescription;
		RebuildTextArguments(/* bUpdateGraphNode */ false);
	}

	TArray<FEGQuestTextArgument>& GetMutableTextArguments() { return TextArguments; }
	const TArray<FEGQuestRoleDefinition>& GetRoleDefinitions() const { return RoleDefinitions; }
	void SetRoleDefinitions(const TArray<FEGQuestRoleDefinition>& InDefinitions) { RoleDefinitions = InDefinitions; }
	const TArray<FEGQuestDirective>& GetActivateDirectives() const { return ActivateDirectives; }
	const TArray<FEGQuestDirective>& GetDeactivateDirectives() const { return DeactivateDirectives; }
	bool ShouldAutoRevertDirectives() const { return bAutoRevert; }
	void SetActivateDirectives(const TArray<FEGQuestDirective>& InDirectives) { ActivateDirectives = InDirectives; }
	void SetDeactivateDirectives(const TArray<FEGQuestDirective>& InDirectives) { DeactivateDirectives = InDirectives; }
	void SetAutoRevertDirectives(bool bInAutoRevert) { bAutoRevert = bInAutoRevert; }

	// Helper functions to get the names of some properties. Used by the QuestPluginEditor module.
	static FName GetMemberNameTitle() { return GET_MEMBER_NAME_CHECKED(UEGQuestNode_Stage, Title); }
	static FName GetMemberNameStageId() { return GET_MEMBER_NAME_CHECKED(UEGQuestNode_Stage, StageId); }
	static FName GetMemberNameDescription() { return GET_MEMBER_NAME_CHECKED(UEGQuestNode_Stage, Description); }
	static FName GetMemberNameTextArguments() { return GET_MEMBER_NAME_CHECKED(UEGQuestNode_Stage, TextArguments); }

protected:
	/** The journal headline for this stage. */
	UPROPERTY(EditAnywhere, Category = "Stage")
	FText Title;

	/**
	 * Optional script-facing name for this stage. The quest script's OnStageEntered/OnStageExited
	 * receive it, so scripts can switch on a readable identifier instead of a GUID or a localized
	 * title. Never shown to players and never localized.
	 */
	UPROPERTY(EditAnywhere, Category = "Stage")
	FName StageId;

	/** The journal body. Supports {identifier} placeholders resolved through the text arguments. */
	UPROPERTY(EditAnywhere, Category = "Stage", Meta = (MultiLine = true))
	FText Description;

	// If you want replaceable portions inside the Description just add {identifier} inside it and set
	// the value it should have at runtime.
	UPROPERTY(EditAnywhere, EditFixedSize, Category = "Stage")
	TArray<FEGQuestTextArgument> TextArguments;

	/** Resolved on entry and released on every exit, including abandon and quest-global termination. */
	UPROPERTY(EditAnywhere, Category = "Stage|Roles")
	TArray<FEGQuestRoleDefinition> RoleDefinitions;

	UPROPERTY(EditAnywhere, Category = "Stage|Directives") TArray<FEGQuestDirective> ActivateDirectives;
	UPROPERTY(EditAnywhere, Category = "Stage|Directives") TArray<FEGQuestDirective> DeactivateDirectives;
	UPROPERTY(EditAnywhere, Category = "Stage|Directives") bool bAutoRevert = true;

	// NOTE: no runtime state may live on this class. Formatted text goes through the context
	// (RebuildConstructedText) - node objects are shared by every quest instance in the process.
};
