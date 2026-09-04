#include "EFModularSettingsBase.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "UnrealExtendedFramework/ModularSettings/Components/EFPlayerSettingsComponent.h"
#include "UnrealExtendedFramework/ModularSettings/Components/EFWorldSettingsComponent.h"
#include "UnrealExtendedFramework/ModularSettings/EFModularSettingsSubsystem.h"

UWorld *UEFModularSettingsBase::GetWorld() const 
{
  if (IsTemplate()) 
  {
    return nullptr;
  }

  if (ModularSettingsSubsystem) 
  {
    return ModularSettingsSubsystem->GetWorld();
  }

  if (GEngine)
  {
    for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
    {
      UWorld* World = WorldContext.World();
      if (World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE))
      {
        return World;
      }
    }
  }

  return nullptr;
}

void UEFModularSettingsBase::NotifyValueChanged() 
{
  UObject *CurrentOuter = GetOuter();
  while (CurrentOuter) 
  {
    if (UEFWorldSettingsComponent* WorldComp =Cast<UEFWorldSettingsComponent>(CurrentOuter)) 
    {
      WorldComp->OnSettingChanged.Broadcast(this);
      return;
    }
    if (UEFPlayerSettingsComponent* PlayerComp = Cast<UEFPlayerSettingsComponent>(CurrentOuter)) 
    {
      PlayerComp->OnSettingChanged.Broadcast(this);
      // NOTE: Do NOT auto-save here. Saving is handled explicitly in ServerUpdateSetting_Implementation
      // after the server applies the value. This prevents replication from overwriting saved settings.
      return;
    }
    CurrentOuter = CurrentOuter->GetOuter();
  }

  if (ModularSettingsSubsystem) 
  {
    ModularSettingsSubsystem->OnSettingsChanged.Broadcast(this);
  }
}

void UEFModularSettingsBase::ProcessEvent(UFunction* Function, void* Parms)
{
  const FName FunctionName = Function ? Function->GetFName() : NAME_None;

  if (FunctionName == GET_FUNCTION_NAME_CHECKED(UEFModularSettingsBase, Apply))
  {
    // Fresh at the gate: the option list / lock state Apply resolves against
    // must be current. The Apply itself always runs.
    EnsureFresh();
  }

  Super::ProcessEvent(Function, Parms);

  if (FunctionName == GET_FUNCTION_NAME_CHECKED(UEFModularSettingsBase, RefreshValues))
  {
    // Every refresh (direct call, EnsureFresh, Blueprint override) stamps the
    // frame so EnsureFresh can skip redundant work within the same frame.
    LastRefreshFrame = GFrameCounter;
  }
}

bool UEFModularSettingsBase::CanRefreshLocally() const
{
  // RefreshValues implementations read machine-local data (PlayFab inventory
  // cache, audio devices). For player settings, only the owning machine has
  // that player's data — a server refreshing a remote client's setting would
  // recompute locks from the WRONG account's cache and overwrite the state
  // the owning client pushed up. World and local settings always refresh.
  for (UObject* CurrentOuter = GetOuter(); CurrentOuter; CurrentOuter = CurrentOuter->GetOuter())
  {
    if (const UEFPlayerSettingsComponent* PlayerComp = Cast<UEFPlayerSettingsComponent>(CurrentOuter))
    {
      return PlayerComp->IsLocalPlayerComponent();
    }
  }

  return true;
}

void UEFModularSettingsBase::EnsureFresh()
{
  if (!bRefreshBeforeApply || bIsRefreshing || LastRefreshFrame == GFrameCounter || !CanRefreshLocally())
  {
    return;
  }

  bIsRefreshing = true;
  RefreshValues();
  bIsRefreshing = false;
}

bool UEFModularSettingsBase::RequestChange(const FString& NewValue)
{
  // Refresh -> validate -> set. Validation runs against fresh state, so a lock
  // or option list that went stale since the player last looked cannot reject
  // (or accept) a change it should not.
  EnsureFresh();

  if (!Validate(NewValue))
  {
    UE_LOG(LogTemp, Verbose, TEXT("[UEFModularSettingsBase] Rejected change of %s to '%s'"), *SettingTag.ToString(), *NewValue);
    return false;
  }

  SetValueFromString(NewValue);
  return true;
}

void UEFModularSettingsBool::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const 
{
  Super::GetLifetimeReplicatedProps(OutLifetimeProps);
  DOREPLIFETIME(UEFModularSettingsBool, Value);
}

void UEFModularSettingsBool::OnRep_Value() 
{
  NotifyValueChanged();
}

void UEFModularSettingsFloat::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const 
{
  Super::GetLifetimeReplicatedProps(OutLifetimeProps);
  DOREPLIFETIME(UEFModularSettingsFloat, Value);
}

void UEFModularSettingsFloat::OnRep_Value() 
{
  NotifyValueChanged();
}

void UEFModularSettingsMultiSelect::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const 
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

void UEFModularSettingsMultiSelect::SetOptionLocked(const FString& OptionValue, bool bLocked)
{
  // OnOptionLockChanged broadcasts OnSettingChanged, so firing it when the lock set is
  // unchanged is not merely wasteful: a listener that re-locks in response (a settings
  // object that recomputes ownership on change) recurses until the stack overflows.
  if (IsOptionLocked(OptionValue) == bLocked)
  {
    return;
  }

  if (bLocked)
  {
    LockedOptions.AddUnique(OptionValue);
  }
  else
  {
    LockedOptions.Remove(OptionValue);
  }

  OnOptionLockChanged();

  // Blueprints (customization UI, the setting classes themselves) call this
  // directly on whatever instance they hold. Off-authority that instance is a
  // client-side replica, so forward the change to the server; its LockedOptions
  // and definition state then replicate to every client. The server applies the
  // RPC through this same function on an authoritative instance, so it does not
  // forward again.
  UObject* CurrentOuter = GetOuter();
  while (CurrentOuter)
  {
    if (UEFPlayerSettingsComponent* PlayerComp = Cast<UEFPlayerSettingsComponent>(CurrentOuter))
    {
      if (PlayerComp->GetOwnerRole() != ROLE_Authority)
      {
        PlayerComp->ServerSetOptionLocked(SettingTag, OptionValue, bLocked);
      }
      break;
    }
    CurrentOuter = CurrentOuter->GetOuter();
  }
}

void UEFModularSettingsMultiSelect::OnOptionLockChanged()
{
  // Keep the owning component's replicated definition in sync (authority only,
  // gated inside the component). Clients build their setting instances from
  // SettingDefinitions, so lock state must travel with the definition the same
  // way CurrentValue does — subobject property replication alone does not reach
  // the definition-created instances.
  UObject* CurrentOuter = GetOuter();
  while (CurrentOuter)
  {
    if (UEFWorldSettingsComponent* WorldComp = Cast<UEFWorldSettingsComponent>(CurrentOuter))
    {
      WorldComp->UpdateDefinitionLockedOptions(SettingTag, LockedOptions);
      break;
    }
    if (UEFPlayerSettingsComponent* PlayerComp = Cast<UEFPlayerSettingsComponent>(CurrentOuter))
    {
      PlayerComp->UpdateDefinitionLockedOptions(SettingTag, LockedOptions);
      break;
    }
    CurrentOuter = CurrentOuter->GetOuter();
  }

  NotifyValueChanged();
}
