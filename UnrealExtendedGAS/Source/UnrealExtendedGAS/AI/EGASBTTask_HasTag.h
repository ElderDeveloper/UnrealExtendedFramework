#pragma once

#include "AI/EGASBTTask_Base.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "EGASBTTask_HasTag.generated.h"

/**
 * Succeeds when the AI itself, or the actor in TargetActorKey, owns a matching
 * gameplay tag; fails when it does not.
 *
 * This is the task form, for use inside a sequence. To gate a whole branch — and
 * to re-evaluate while it runs — use the Has Gameplay Tag decorator instead.
 */
UCLASS(meta = (DisplayName = "Has Gameplay Tag"))
class UNREALEXTENDEDGAS_API UEGASBTTask_HasTag : public UEGASBTTask_Base
{
	GENERATED_BODY()

public:
	UEGASBTTask_HasTag(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Tags to look for on the target. */
	UPROPERTY(EditAnywhere, Category = "Tags")
	FGameplayTagContainer SearchTags;

	/** Require every tag in SearchTags rather than any one of them. */
	UPROPERTY(EditAnywhere, Category = "Tags")
	bool bRequireAllTags = false;

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;
};
