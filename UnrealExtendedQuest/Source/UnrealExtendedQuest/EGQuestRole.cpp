// Copyright Devil of the Plague. All Rights Reserved.
#include "EGQuestRole.h"

#include "EGQuestComponent.h"
#include "EGQuestFactsSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"

bool UEGQuestRoleResolver::Resolve(const FEGQuestRoleResolveContext&, FName, TArray<FEGQuestEntityHandle>& OutHandles) const
{
	OutHandles.Reset();
	return false;
}

bool UEGQuestRoleResolver_RegistryQuery::Resolve(const FEGQuestRoleResolveContext& Context, FName, TArray<FEGQuestEntityHandle>& OutHandles) const
{
	OutHandles.Reset();
	UEGQuestTargetRegistrySubsystem* Registry = Context.World ? Context.World->GetSubsystem<UEGQuestTargetRegistrySubsystem>() : nullptr;
	if (!Registry) return false;
	OutHandles = Registry->FindTargets(TargetQuery);
	if (Selection == EEGQuestRoleSelection::Nearest)
	{
		OutHandles.Sort([Registry, Origin = Context.Origin.GetLocation()](const FEGQuestEntityHandle& A, const FEGQuestEntityHandle& B)
		{
			const AActor* ActorA = Registry->ResolveActor(A);
			const AActor* ActorB = Registry->ResolveActor(B);
			const double DistA = ActorA ? FVector::DistSquared(Origin, ActorA->GetActorLocation()) : TNumericLimits<double>::Max();
			const double DistB = ActorB ? FVector::DistSquared(Origin, ActorB->GetActorLocation()) : TNumericLimits<double>::Max();
			return DistA == DistB ? A.StableId.LexicalLess(B.StableId) : DistA < DistB;
		});
	}
	else if (Selection == EEGQuestRoleSelection::Random && OutHandles.Num() > 1)
	{
		const int32 Index = static_cast<int32>(GetTypeHash(Context.RunId) % static_cast<uint32>(OutHandles.Num()));
		Swap(OutHandles[0], OutHandles[Index]);
	}
	if (Selection != EEGQuestRoleSelection::All && OutHandles.Num() > 1) OutHandles.SetNum(1);
	else if (MaxResults > 0 && OutHandles.Num() > MaxResults) OutHandles.SetNum(MaxResults);
	return !OutHandles.IsEmpty();
}

bool UEGQuestRoleResolver_Players::Resolve(const FEGQuestRoleResolveContext& Context, FName, TArray<FEGQuestEntityHandle>& OutHandles) const
{
	OutHandles.Reset();
	if (!Context.World) return false;
	UEGQuestTargetRegistrySubsystem* Registry = Context.World->GetSubsystem<UEGQuestTargetRegistrySubsystem>();
	const AGameStateBase* GameState = Context.World->GetGameState();
	if (!Registry || !GameState) return false;
	for (APlayerState* Player : GameState->PlayerArray)
		if (Player) OutHandles.Add(Registry->GetOrCreateHandle(*Player));
	return !OutHandles.IsEmpty();
}

bool UEGQuestRoleResolver_Explicit::Resolve(const FEGQuestRoleResolveContext& Context, FName, TArray<FEGQuestEntityHandle>& OutHandles) const
{
	OutHandles.Reset();
	if (Actor.IsNull()) return false;
	FEGQuestEntityHandle Handle;
	Handle.StableId = FName(*Actor.ToSoftObjectPath().ToString());
	OutHandles.Add(Handle);
	if (Context.World)
		if (UEGQuestTargetRegistrySubsystem* Registry = Context.World->GetSubsystem<UEGQuestTargetRegistrySubsystem>())
			Registry->BindExternalHandle(Handle, Actor.Get());
	return true;
}

bool UEGQuestRoleResolver_FromFact::Resolve(const FEGQuestRoleResolveContext& Context, FName, TArray<FEGQuestEntityHandle>& OutHandles) const
{
	OutHandles.Reset();
	if (!SourceRole.IsNone() && Context.ExistingBindings)
		if (const FEGQuestRoleBinding* Binding = Context.ExistingBindings->FindByPredicate(
			[this](const FEGQuestRoleBinding& Item){ return Item.RoleName == SourceRole; }))
		{
			OutHandles = Binding->Handles;
			return !OutHandles.IsEmpty();
		}
	if (!Context.World || !FactTag.IsValid()) return false;
	const UEGQuestFactsSubsystem* Facts = Context.World->GetSubsystem<UEGQuestFactsSubsystem>();
	UEGQuestTargetRegistrySubsystem* Registry = Context.World->GetSubsystem<UEGQuestTargetRegistrySubsystem>();
	if (!Facts || !Registry) return false;
	const FGameplayTag* TargetTag = FactValueTargetTags.Find(Facts->GetFact(FactTag));
	if (!TargetTag) return false;
	OutHandles = Registry->FindTargets(FGameplayTagQuery::MakeQuery_MatchTag(*TargetTag));
	return !OutHandles.IsEmpty();
}

bool UEGQuestRoleResolver_Scripted::Resolve(const FEGQuestRoleResolveContext& Context, FName RoleName, TArray<FEGQuestEntityHandle>& OutHandles) const
{
	OutHandles.Reset();
	return ResolveRole(Context.QuestComponent, Context.RunId, RoleName, OutHandles);
}

bool UEGQuestRoleResolver_Scripted::ResolveRole_Implementation(UEGQuestComponent*, FGuid, FName, TArray<FEGQuestEntityHandle>& OutHandles) const
{
	OutHandles.Reset();
	return false;
}

