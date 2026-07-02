#pragma once

#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "CoreMinimal.h"

#include "EGASAttributeSet.generated.h"

#define EGAS_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS(Abstract, BlueprintType)
class UNREALEXTENDEDGAS_API UEGASAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UEGASAttributeSet();

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

protected:
	virtual void HandlePreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue);

	virtual void HandlePostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data);
};
