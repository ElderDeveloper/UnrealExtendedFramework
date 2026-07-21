#include "EGASAttributeSet.h"

UEGASAttributeSet::UEGASAttributeSet()
{
}

void UEGASAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	HandlePreAttributeChange(Attribute, NewValue);
}

void UEGASAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	HandlePostGameplayEffectExecute(Data);
}

void UEGASAttributeSet::HandlePreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
}

void UEGASAttributeSet::HandlePostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
}
