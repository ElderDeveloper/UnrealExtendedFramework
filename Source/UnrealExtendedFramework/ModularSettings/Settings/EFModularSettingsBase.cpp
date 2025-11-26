#include "EFModularSettingsBase.h"

#include "UnrealExtendedFramework/ModularSettings/EFModularSettingsSubsystem.h"


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
