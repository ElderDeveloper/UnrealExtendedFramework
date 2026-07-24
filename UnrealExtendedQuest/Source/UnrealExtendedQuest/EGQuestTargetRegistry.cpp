// Copyright Devil of the Plague. All Rights Reserved.
#include "EGQuestTargetRegistry.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace
{
	/** Prefer ActorGuid in editor and a cooked-safe object path for placed runtime actors. */
	FName MakeActorStableId(const AActor& Actor)
	{
#if WITH_EDITOR
		const FGuid ActorGuid = Actor.GetActorGuid();
		if (ActorGuid.IsValid())
		{
			return FName(*ActorGuid.ToString(EGuidFormats::DigitsWithHyphensLower));
		}
#endif
		if (Actor.HasAnyFlags(RF_WasLoaded))
		{
			return FName(*Actor.GetPathName(Actor.GetWorld()));
		}
		return FName(*FString::Printf(TEXT("Dyn.%s"), *FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower)));
	}
}

UEGQuestTargetComponent::UEGQuestTargetComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEGQuestTargetComponent::OnRegister()
{
	Super::OnRegister();
	if (StableId.IsNone() && GetOwner()) StableId = MakeActorStableId(*GetOwner());
	if (UWorld* World = GetWorld())
		if (UEGQuestTargetRegistrySubsystem* Registry = World->GetSubsystem<UEGQuestTargetRegistrySubsystem>())
			Registry->RegisterTarget(*this);
}

void UEGQuestTargetComponent::OnUnregister()
{
	if (UWorld* World = GetWorld())
		if (UEGQuestTargetRegistrySubsystem* Registry = World->GetSubsystem<UEGQuestTargetRegistrySubsystem>())
			Registry->UnregisterTarget(*this);
	Super::OnUnregister();
}

FEGQuestEntityHandle UEGQuestTargetComponent::GetEntityHandle() const
{
	FEGQuestEntityHandle Result;
	Result.StableId = StableId;
	return Result;
}

FText UEGQuestTargetComponent::GetTargetDisplayName() const
{
	if (!DisplayName.IsEmpty()) return DisplayName;
	return GetOwner() ? FText::FromString(GetOwner()->GetActorNameOrLabel()) : FText::GetEmpty();
}

void UEGQuestTargetRegistrySubsystem::RegisterTarget(UEGQuestTargetComponent& Target)
{
	const FEGQuestEntityHandle Handle = Target.GetEntityHandle();
	if (Handle.IsValid()) RegisteredTargets.Add(Handle.StableId, &Target);
}

void UEGQuestTargetRegistrySubsystem::UnregisterTarget(UEGQuestTargetComponent& Target)
{
	const FEGQuestEntityHandle Handle = Target.GetEntityHandle();
	if (const TWeakObjectPtr<UEGQuestTargetComponent>* Existing = RegisteredTargets.Find(Handle.StableId))
		if (Existing->Get() == &Target) RegisteredTargets.Remove(Handle.StableId);
}

FEGQuestEntityHandle UEGQuestTargetRegistrySubsystem::GetOrCreateHandle(AActor& Actor)
{
	if (UEGQuestTargetComponent* Target = Actor.FindComponentByClass<UEGQuestTargetComponent>())
	{
		RegisterTarget(*Target);
		return Target->GetEntityHandle();
	}
	for (const TPair<FName, TWeakObjectPtr<AActor>>& Pair : ExternalActors)
	{
		if (Pair.Value.Get() == &Actor)
		{
			FEGQuestEntityHandle Existing;
			Existing.StableId = Pair.Key;
			return Existing;
		}
	}
	FEGQuestEntityHandle Handle;
	Handle.StableId = MakeActorStableId(Actor);
	ExternalActors.Add(Handle.StableId, &Actor);
	return Handle;
}

void UEGQuestTargetRegistrySubsystem::BindExternalHandle(const FEGQuestEntityHandle& Handle, AActor* Actor)
{
	if (Handle.IsValid() && Actor) ExternalActors.Add(Handle.StableId, Actor);
}

AActor* UEGQuestTargetRegistrySubsystem::ResolveActor(const FEGQuestEntityHandle& Handle) const
{
	if (!Handle.IsValid()) return nullptr;
	if (const TWeakObjectPtr<UEGQuestTargetComponent>* Target = RegisteredTargets.Find(Handle.StableId))
		if (Target->IsValid()) return Target->Get()->GetOwner();
	if (const TWeakObjectPtr<AActor>* Actor = ExternalActors.Find(Handle.StableId)) return Actor->Get();
	return nullptr;
}

FText UEGQuestTargetRegistrySubsystem::ResolveDisplayName(const FEGQuestEntityHandle& Handle) const
{
	if (const TWeakObjectPtr<UEGQuestTargetComponent>* Target = RegisteredTargets.Find(Handle.StableId))
		if (Target->IsValid()) return Target->Get()->GetTargetDisplayName();
	if (AActor* Actor = ResolveActor(Handle)) return FText::FromString(Actor->GetActorNameOrLabel());
	return FText::FromName(Handle.StableId);
}

FTransform UEGQuestTargetRegistrySubsystem::ResolveTransform(const FEGQuestEntityHandle& Handle, bool& bOutResolved) const
{
	if (AActor* Actor = ResolveActor(Handle))
	{
		bOutResolved = true;
		return Actor->GetActorTransform();
	}
	bOutResolved = false;
	return FTransform::Identity;
}

TArray<FEGQuestEntityHandle> UEGQuestTargetRegistrySubsystem::FindTargets(const FGameplayTagQuery& Query) const
{
	TArray<FEGQuestEntityHandle> Result;
	for (const TPair<FName, TWeakObjectPtr<UEGQuestTargetComponent>>& Pair : RegisteredTargets)
	{
		const UEGQuestTargetComponent* Target = Pair.Value.Get();
		if (!Target || (!Query.IsEmpty() && !Query.Matches(Target->GetTargetTags()))) continue;
		FEGQuestEntityHandle& Handle = Result.AddDefaulted_GetRef();
		Handle.StableId = Pair.Key;
	}
	Result.Sort([](const FEGQuestEntityHandle& A, const FEGQuestEntityHandle& B)
	{
		return A.StableId.LexicalLess(B.StableId);
	});
	return Result;
}

