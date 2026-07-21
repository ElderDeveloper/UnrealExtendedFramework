// Copyright Devil of the Plague. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "EGQuestTargetRegistry.generated.h"

USTRUCT(BlueprintType)
struct UNREALEXTENDEDQUEST_API FEGQuestEntityHandle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Roles")
	FName StableId = NAME_None;

	bool IsValid() const { return !StableId.IsNone(); }
	friend bool operator==(const FEGQuestEntityHandle& A, const FEGQuestEntityHandle& B) { return A.StableId == B.StableId; }
	friend uint32 GetTypeHash(const FEGQuestEntityHandle& Handle) { return GetTypeHash(Handle.StableId); }
};

UCLASS(ClassGroup = "Quest", BlueprintType, Blueprintable, Meta = (BlueprintSpawnableComponent))
class UNREALEXTENDEDQUEST_API UEGQuestTargetComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEGQuestTargetComponent();
	void OnRegister() override;
	void OnUnregister() override;

	FEGQuestEntityHandle GetEntityHandle() const;
	const FGameplayTagContainer& GetTargetTags() const { return TargetTags; }
	FText GetTargetDisplayName() const;

	void SetStableId(FName InId) { StableId = InId; }
	void SetTargetTags(const FGameplayTagContainer& InTags) { TargetTags = InTags; }
	void SetDisplayName(const FText& InName) { DisplayName = InName; }

protected:
	UPROPERTY(EditAnywhere, SaveGame, Category = "Quest|Roles") FName StableId = NAME_None;
	UPROPERTY(EditAnywhere, Category = "Quest|Roles") FGameplayTagContainer TargetTags;
	UPROPERTY(EditAnywhere, Category = "Quest|Roles") FText DisplayName;
};

UCLASS()
class UNREALEXTENDEDQUEST_API UEGQuestTargetRegistrySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void RegisterTarget(UEGQuestTargetComponent& Target);
	void UnregisterTarget(UEGQuestTargetComponent& Target);
	FEGQuestEntityHandle GetOrCreateHandle(AActor& Actor);
	void BindExternalHandle(const FEGQuestEntityHandle& Handle, AActor* Actor);
	AActor* ResolveActor(const FEGQuestEntityHandle& Handle) const;
	FText ResolveDisplayName(const FEGQuestEntityHandle& Handle) const;
	FTransform ResolveTransform(const FEGQuestEntityHandle& Handle, bool& bOutResolved) const;
	TArray<FEGQuestEntityHandle> FindTargets(const FGameplayTagQuery& Query) const;

private:
	UPROPERTY(Transient) TMap<FName, TWeakObjectPtr<UEGQuestTargetComponent>> RegisteredTargets;
	UPROPERTY(Transient) TMap<FName, TWeakObjectPtr<AActor>> ExternalActors;
};

