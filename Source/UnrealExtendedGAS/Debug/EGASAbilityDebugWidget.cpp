#include "Debug/EGASAbilityDebugWidget.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Actor.h"

void UEGASAbilityDebugWidget::SetObservedActor(AActor* InObservedActor)
{
	ObservedActor = InObservedActor;
}

AActor* UEGASAbilityDebugWidget::GetObservedActor() const
{
	return ObservedActor;
}

TArray<FString> UEGASAbilityDebugWidget::GetDebugLines() const
{
	TArray<FString> Lines;

	if (!ObservedActor)
	{
		Lines.Add(TEXT("No observed actor."));
		return Lines;
	}

	UAbilitySystemComponent* AbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(ObservedActor);
	if (!AbilitySystemComponent)
	{
		Lines.Add(FString::Printf(TEXT("%s has no AbilitySystemComponent."), *ObservedActor->GetName()));
		return Lines;
	}

	Lines.Add(FString::Printf(TEXT("Actor: %s"), *ObservedActor->GetName()));
	Lines.Add(FString::Printf(TEXT("ASC: %s"), *AbilitySystemComponent->GetName()));

	FGameplayTagContainer OwnedTags;
	AbilitySystemComponent->GetOwnedGameplayTags(OwnedTags);
	Lines.Add(FString::Printf(TEXT("Tags: %s"), *OwnedTags.ToStringSimple()));

	for (const UAttributeSet* AttributeSet : AbilitySystemComponent->GetSpawnedAttributes())
	{
		if (!AttributeSet)
		{
			continue;
		}

		Lines.Add(FString::Printf(TEXT("AttributeSet: %s"), *AttributeSet->GetClass()->GetName()));

		for (TFieldIterator<FProperty> It(AttributeSet->GetClass()); It; ++It)
		{
			FProperty* Property = *It;
			if (!FGameplayAttribute::IsGameplayAttributeDataProperty(Property) && !CastField<FFloatProperty>(Property))
			{
				continue;
			}

			const FGameplayAttribute Attribute(Property);
			Lines.Add(FString::Printf(TEXT("  %s = %.2f"), *Attribute.GetName(), AbilitySystemComponent->GetNumericAttribute(Attribute)));
		}
	}

	return Lines;
}
