#include "EFModularSettingsBase.h"

#include "UnrealExtendedFramework/ModularSettings/EFModularSettingsSubsystem.h"
#include "UnrealExtendedFramework/ModularSettings/Components/EFWorldSettingsComponent.h"
#include "UnrealExtendedFramework/ModularSettings/Components/EFPlayerSettingsComponent.h"
#include "Net/UnrealNetwork.h"


UWorld* UEFModularSettingsBase::GetWorld() const
{
    if (IsTemplate())
    {
        return nullptr;
    }

    if (ModularSettingsSubsystem)
    {
         return ModularSettingsSubsystem->GetWorld();
    }
    if (GetOuter())
    {
        return GetOuter()->GetWorld();
    }
    return nullptr;
}

void UEFModularSettingsBase::NotifyValueChanged()
{
    UObject* CurrentOuter = GetOuter();
    while (CurrentOuter)
    {
        if (UEFWorldSettingsComponent* WorldComp = Cast<UEFWorldSettingsComponent>(CurrentOuter))
        {
            WorldComp->OnSettingChanged.Broadcast(this);
            return;
        }
        if (UEFPlayerSettingsComponent* PlayerComp = Cast<UEFPlayerSettingsComponent>(CurrentOuter))
        {
            PlayerComp->OnSettingChanged.Broadcast(this);
            return;
        }
        CurrentOuter = CurrentOuter->GetOuter();
    }

    if (ModularSettingsSubsystem)
    {
        ModularSettingsSubsystem->OnSettingsChanged.Broadcast(this);
    }
}

void UEFModularSettingsBool::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UEFModularSettingsBool, Value);
}

void UEFModularSettingsBool::OnRep_Value()
{
    NotifyValueChanged();
}

void UEFModularSettingsFloat::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UEFModularSettingsFloat, Value);
}

void UEFModularSettingsFloat::OnRep_Value()
{
    NotifyValueChanged();
}

void UEFModularSettingsMultiSelect::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UEFModularSettingsMultiSelect, SelectedIndex);
	DOREPLIFETIME(UEFModularSettingsMultiSelect, LockedOptions);
	DOREPLIFETIME(UEFModularSettingsMultiSelect, Values);
	DOREPLIFETIME(UEFModularSettingsMultiSelect, DisplayNames);
}

void UEFModularSettingsMultiSelect::OnRep_SelectedIndex()
{
    NotifyValueChanged();
}

void UEFModularSettingsMultiSelect::OnOptionLockChanged()
{
    NotifyValueChanged();
}
