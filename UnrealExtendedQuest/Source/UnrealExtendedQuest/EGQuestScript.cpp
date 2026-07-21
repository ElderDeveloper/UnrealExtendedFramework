// Copyright Devil of the Plague. All Rights Reserved.
#include "EGQuestScript.h"

#include "EGQuestComponent.h"
#include "GameFramework/Actor.h"

UWorld* UEGQuestScript::GetWorld() const
{
	// The CDO answers nullptr so the Blueprint editor does not think this is a world-bound class.
	if (HasAnyFlags(RF_ClassDefaultObject) || !OwnerComponent.IsValid())
	{
		return nullptr;
	}
	return OwnerComponent->GetWorld();
}

void UEGQuestScript::Initialize(UEGQuestComponent* InOwnerComponent, const FGuid& InQuestInstanceGuid)
{
	OwnerComponent = InOwnerComponent;
	QuestInstanceGuid = InQuestInstanceGuid;
}

void UEGQuestScript::HandleQuestStarted()
{
	OnQuestStarted();
}

void UEGQuestScript::HandleQuestResumed()
{
	OnQuestResumed();
}

void UEGQuestScript::HandleStageEntered(const FGuid& StageGuid, FName StageId, const FText& StageTitle)
{
	OnStageEntered(StageGuid, StageId, StageTitle);
}

void UEGQuestScript::HandleStageExited(const FGuid& StageGuid, FName StageId, EEGQuestStageExitReason Reason)
{
	OnStageExited(StageGuid, StageId, Reason);
}

void UEGQuestScript::HandleObjectiveProgress(const FEGQuestSnapshotObjective& Objective)
{
	OnObjectiveProgress(Objective);
}

void UEGQuestScript::HandleObjectiveResolved(const FEGQuestSnapshotObjective& Objective)
{
	OnObjectiveResolved(Objective);
}

void UEGQuestScript::HandleRoleLost(FName RoleName)
{
	OnRoleLost(RoleName);
}

void UEGQuestScript::HandleQuestEnded(EEGQuestResult Result)
{
	OnQuestEnded(Result);
}

UEGQuestComponent* UEGQuestScript::GetQuestComponent() const
{
	return OwnerComponent.Get();
}

bool UEGQuestScript::GetQuestSnapshot(FEGQuestViewSnapshot& OutSnapshot) const
{
	return OwnerComponent.IsValid() && OwnerComponent->FindQuestSnapshot(QuestInstanceGuid, OutSnapshot);
}

UEGQuestFactsSubsystem* UEGQuestScript::GetFactsSubsystem() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UEGQuestFactsSubsystem>() : nullptr;
}

int32 UEGQuestScript::GetFact(FGameplayTag Tag, EEGQuestFactScope Scope, APlayerState* Player) const
{
	const UEGQuestFactsSubsystem* Facts = GetFactsSubsystem();
	return Facts ? Facts->GetFact(Tag, Scope, Player) : 0;
}

bool UEGQuestScript::HasFact(FGameplayTag Tag, EEGQuestFactScope Scope, APlayerState* Player) const
{
	const UEGQuestFactsSubsystem* Facts = GetFactsSubsystem();
	return Facts && Facts->HasFact(Tag, Scope, Player);
}

bool UEGQuestScript::SetFact(FGameplayTag Tag, int32 Value, EEGQuestFactScope Scope, APlayerState* Player)
{
	UEGQuestFactsSubsystem* Facts = GetFactsSubsystem();
	return Facts && Facts->SetFact(Tag, Value, Scope, Player, OwnerComponent.IsValid() ? OwnerComponent->GetOwner() : nullptr);
}

bool UEGQuestScript::AddToFact(FGameplayTag Tag, int32 Delta, EEGQuestFactScope Scope, APlayerState* Player)
{
	UEGQuestFactsSubsystem* Facts = GetFactsSubsystem();
	return Facts && Facts->AddToFact(Tag, Delta, Scope, Player, OwnerComponent.IsValid() ? OwnerComponent->GetOwner() : nullptr);
}

TArray<AActor*> UEGQuestScript::ResolveRoleActors(FName RoleName) const
{
	return OwnerComponent.IsValid()
		? OwnerComponent->ResolveRoleActors(QuestInstanceGuid, RoleName)
		: TArray<AActor*>();
}

bool UEGQuestScript::GetRoleTransform(FName RoleName, FTransform& OutTransform) const
{
	return OwnerComponent.IsValid() && OwnerComponent->GetRoleTransform(QuestInstanceGuid, RoleName, OutTransform);
}

FText UEGQuestScript::GetRoleDisplayText(FName RoleName) const
{
	return OwnerComponent.IsValid() ? OwnerComponent->GetRoleDisplayText(QuestInstanceGuid, RoleName) : FText::GetEmpty();
}

bool UEGQuestScript::EmitQuestEvent(FGameplayTag EventTag, float Magnitude, FName ContextName)
{
	if (!OwnerComponent.IsValid() || !EventTag.IsValid())
	{
		return false;
	}

	FEGQuestGameplayEvent Event;
	Event.EventTag = EventTag;
	Event.Magnitude = Magnitude;
	Event.ContextName = ContextName;
	return OwnerComponent->NotifyGameplayEvent(Event);
}

bool UEGQuestScript::CompleteObjective(FGuid ObjectiveGuid)
{
	return OwnerComponent.IsValid() && OwnerComponent->CompleteActiveObjective(QuestInstanceGuid, ObjectiveGuid);
}

bool UEGQuestScript::FailObjective(FGuid ObjectiveGuid)
{
	return OwnerComponent.IsValid() && OwnerComponent->FailActiveObjective(QuestInstanceGuid, ObjectiveGuid);
}

bool UEGQuestScript::SetObjectiveRequiredCount(FGuid ObjectiveGuid, int32 RequiredCount)
{
	return OwnerComponent.IsValid() && OwnerComponent->SetObjectiveRequiredCount(QuestInstanceGuid, ObjectiveGuid, RequiredCount);
}

TArray<FGuid> UEGQuestScript::SetObjectiveRequiredCountByEventTag(FGameplayTag EventTag, int32 RequiredCount)
{
	if (!OwnerComponent.IsValid())
	{
		return {};
	}
	return OwnerComponent->SetObjectiveRequiredCountByEventTag(QuestInstanceGuid, EventTag, RequiredCount).AffectedElementGuids;
}
