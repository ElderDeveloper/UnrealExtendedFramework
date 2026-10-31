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
  // A Locked or Hidden setting holds its device default; nothing may change it at runtime.
  if (!IsEditableOnThisDevice())
  {
    UE_LOG(LogTemp, Verbose, TEXT("[UEFModularSettingsBase] Rejected change of %s to '%s': not editable on this device"), *SettingTag.ToString(), *NewValue);
    return false;
  }

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

FName UEFModularSettingsBase::GetEffectiveScalabilityGroup() const
{
  if (!ScalabilityGroup.IsNone())
  {
    return ScalabilityGroup;
  }

  // The plugin owns these tags, so a Blueprint setting that reuses one (a project's own shadow row)
  // still gets its group without anything set on the asset.
  static const TMap<FName, FName> KnownGroups = {
    { TEXT("Settings.Graphics.ViewDistance"), TEXT("sg.ViewDistanceQuality") },
    { TEXT("Settings.Graphics.AntiAliasingQuality"), TEXT("sg.AntiAliasingQuality") },
    { TEXT("Settings.Graphics.ShadowQuality"), TEXT("sg.ShadowQuality") },
    { TEXT("Settings.Graphics.PostProcessQuality"), TEXT("sg.PostProcessQuality") },
    { TEXT("Settings.Graphics.TextureQuality"), TEXT("sg.TextureQuality") },
    { TEXT("Settings.Graphics.VFXQuality"), TEXT("sg.EffectsQuality") },
    { TEXT("Settings.Graphics.FoliageQuality"), TEXT("sg.FoliageQuality") },
    { TEXT("Settings.Graphics.ShadingQuality"), TEXT("sg.ShadingQuality") },
    { TEXT("Settings.Graphics.GlobalIlluminationQuality"), TEXT("sg.GlobalIlluminationQuality") },
    { TEXT("Settings.Graphics.ReflectionQuality"), TEXT("sg.ReflectionQuality") },
    { TEXT("Settings.Graphics.LandscapeQuality"), TEXT("sg.LandscapeQuality") },
  };

  const FName* Known = SettingTag.IsValid() ? KnownGroups.Find(SettingTag.GetTagName()) : nullptr;
  return Known ? *Known : NAME_None;
}

void UEFModularSettingsBase::CaptureAuthoredDefault()
{
  if (!bAuthoredDefaultCaptured)
  {
    AuthoredDefaultValue = GetDefaultValueAsString();
    bAuthoredDefaultCaptured = true;
  }
}

void UEFModularSettingsBase::ApplyDeviceRule(const FEFModularSettingDeviceRule* Rule, TArray<FString>& OutWarnings)
{
  DeviceAvailability = Rule ? Rule->Availability : EEFSettingAvailability::Visible;

  // Capability outranks config: a rule cannot make visible what this machine cannot run.
  if (!IsSupportedOnThisMachine())
  {
    DeviceAvailability = EEFSettingAvailability::Hidden;
  }
}

/* Bool */

bool UEFModularSettingsBool::SetDefaultValueFromString(const FString& NewDefault)
{
  if (!IsKnownValue(NewDefault))
  {
    return false;
  }
  DefaultValue = NewDefault.TrimStartAndEnd().ToBool();
  return true;
}

bool UEFModularSettingsBool::AreValuesEqual(const FString& A, const FString& B) const
{
  return A.TrimStartAndEnd().ToBool() == B.TrimStartAndEnd().ToBool();
}

bool UEFModularSettingsBool::IsKnownValue(const FString& InValue) const
{
  const FString Trimmed = InValue.TrimStartAndEnd();
  return Trimmed.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Trimmed.Equals(TEXT("false"), ESearchCase::IgnoreCase)
    || Trimmed == TEXT("1") || Trimmed == TEXT("0")
    || Trimmed.Equals(TEXT("yes"), ESearchCase::IgnoreCase) || Trimmed.Equals(TEXT("no"), ESearchCase::IgnoreCase)
    || Trimmed.Equals(TEXT("on"), ESearchCase::IgnoreCase) || Trimmed.Equals(TEXT("off"), ESearchCase::IgnoreCase);
}

/* Float */

bool UEFModularSettingsFloat::IsKnownValue(const FString& InValue) const
{
  return InValue.TrimStartAndEnd().IsNumeric();
}

bool UEFModularSettingsFloat::SetDefaultValueFromString(const FString& NewDefault)
{
  if (!IsKnownValue(NewDefault))
  {
    return false;
  }
  DefaultValue = FMath::Clamp(FCString::Atof(*NewDefault.TrimStartAndEnd()), Min, Max);
  return true;
}

bool UEFModularSettingsFloat::AreValuesEqual(const FString& A, const FString& B) const
{
  // "0.5" and "0.500000" are the same value; only text that is not a number compares as text.
  if (IsKnownValue(A) && IsKnownValue(B))
  {
    return FMath::IsNearlyEqual(FCString::Atof(*A.TrimStartAndEnd()), FCString::Atof(*B.TrimStartAndEnd()), 1.e-4f);
  }
  return A == B;
}

bool UEFModularSettingsFloat::CanTakeValueOnThisDevice(const FString& InValue) const
{
  if (!IsKnownValue(InValue))
  {
    return false;
  }
  const float Parsed = FCString::Atof(*InValue.TrimStartAndEnd());
  return Parsed >= Min && Parsed <= Max;
}

FString UEFModularSettingsFloat::AdjustValueToDevice(const FString& InValue) const
{
  // A value outside this device's range is clamped, not reset: the nearest value is what the player
  // would have picked here.
  return IsKnownValue(InValue)
    ? FString::SanitizeFloat(FMath::Clamp(FCString::Atof(*InValue.TrimStartAndEnd()), Min, Max))
    : FString();
}

void UEFModularSettingsFloat::ApplyDeviceRule(const FEFModularSettingDeviceRule* Rule, TArray<FString>& OutWarnings)
{
  Super::ApplyDeviceRule(Rule, OutWarnings);

  if (!bAuthoredRangeCaptured)
  {
    AuthoredMin = Min;
    AuthoredMax = Max;
    AuthoredDisplayMin = DisplayMin;
    AuthoredDisplayMax = DisplayMax;
    bAuthoredRangeCaptured = true;
  }

  float NewMin = AuthoredMin;
  float NewMax = AuthoredMax;
  if (Rule)
  {
    if (!Rule->Min.IsEmpty())
    {
      if (Rule->Min.IsNumeric())
      {
        NewMin = FMath::Clamp(FCString::Atof(*Rule->Min), AuthoredMin, AuthoredMax);
      }
      else
      {
        OutWarnings.Add(FString::Printf(TEXT("%s: Min '%s' is not a number"), *SettingTag.ToString(), *Rule->Min));
      }
    }
    if (!Rule->Max.IsEmpty())
    {
      if (Rule->Max.IsNumeric())
      {
        NewMax = FMath::Clamp(FCString::Atof(*Rule->Max), AuthoredMin, AuthoredMax);
      }
      else
      {
        OutWarnings.Add(FString::Printf(TEXT("%s: Max '%s' is not a number"), *SettingTag.ToString(), *Rule->Max));
      }
    }
    if (NewMin > NewMax)
    {
      OutWarnings.Add(FString::Printf(TEXT("%s: Min %s is above Max %s; range left as authored"), *SettingTag.ToString(), *Rule->Min, *Rule->Max));
      NewMin = AuthoredMin;
      NewMax = AuthoredMax;
    }
  }

  // The display range keeps its proportion to the real range, so a 0-1 value shown as 0-100 still
  // reads correctly once narrowed.
  const FVector2D Authored(AuthoredMin, AuthoredMax);
  const FVector2D AuthoredDisplay(AuthoredDisplayMin, AuthoredDisplayMax);
  Min = NewMin;
  Max = NewMax;
  DisplayMin = FMath::IsNearlyEqual(AuthoredMin, AuthoredMax) ? AuthoredDisplayMin : FMath::GetMappedRangeValueUnclamped(Authored, AuthoredDisplay, NewMin);
  DisplayMax = FMath::IsNearlyEqual(AuthoredMin, AuthoredMax) ? AuthoredDisplayMax : FMath::GetMappedRangeValueUnclamped(Authored, AuthoredDisplay, NewMax);
}

/* Multi-select */

bool UEFModularSettingsMultiSelect::IsOptionOffered(const FString& OptionValue) const
{
  return !DeviceHiddenOptions.ContainsByPredicate([&OptionValue](const FString& Hidden) { return Hidden.Equals(OptionValue, ESearchCase::IgnoreCase); })
    && IsOptionSupported(OptionValue);
}

TArray<int32> UEFModularSettingsMultiSelect::GetOfferedIndices() const
{
  TArray<int32> Offered;
  for (int32 Index = 0; Index < Values.Num(); ++Index)
  {
    if (IsIndexOffered(Index))
    {
      Offered.Add(Index);
    }
  }
  return Offered;
}

int32 UEFModularSettingsMultiSelect::FindNearestOfferedIndex(int32 Index) const
{
  if (IsIndexOffered(Index))
  {
    return Index;
  }

  // Lower first on a tie: the cheaper neighbour is the safer guess for a device that hides an option.
  for (int32 Distance = 1; Distance < Values.Num(); ++Distance)
  {
    if (IsIndexOffered(Index - Distance))
    {
      return Index - Distance;
    }
    if (IsIndexOffered(Index + Distance))
    {
      return Index + Distance;
    }
  }
  return INDEX_NONE;
}

bool UEFModularSettingsMultiSelect::IsKnownValue(const FString& InValue) const
{
  // An option list filled later (audio devices are enumerated asynchronously) cannot judge yet.
  return Values.IsEmpty()
    || Values.ContainsByPredicate([&InValue](const FString& Candidate) { return Candidate.Equals(InValue, ESearchCase::IgnoreCase); });
}

bool UEFModularSettingsMultiSelect::SetDefaultValueFromString(const FString& NewDefault)
{
  const int32 Index = Values.IndexOfByPredicate([&NewDefault](const FString& Candidate) { return Candidate.Equals(NewDefault, ESearchCase::IgnoreCase); });
  if (Index != INDEX_NONE)
  {
    DefaultValue = Values[Index];
    return true;
  }
  if (Values.IsEmpty())
  {
    DefaultValue = NewDefault;
    return true;
  }
  return false;
}

bool UEFModularSettingsMultiSelect::CanTakeValueOnThisDevice(const FString& InValue) const
{
  if (Values.IsEmpty())
  {
    return true;
  }
  const int32 Index = Values.IndexOfByPredicate([&InValue](const FString& Candidate) { return Candidate.Equals(InValue, ESearchCase::IgnoreCase); });
  return Index != INDEX_NONE && IsIndexOffered(Index);
}

FString UEFModularSettingsMultiSelect::AdjustValueToDevice(const FString& InValue) const
{
  // A hidden option falls back to the device default (empty), not to a neighbour: the neighbour of a
  // hidden "Ultra" is a choice the player never made, while the default is what this device recommends.
  return FString();
}

void UEFModularSettingsMultiSelect::ApplyDeviceRule(const FEFModularSettingDeviceRule* Rule, TArray<FString>& OutWarnings)
{
  Super::ApplyDeviceRule(Rule, OutWarnings);

  DeviceHiddenOptions.Reset();
  if (!Rule)
  {
    return;
  }

  auto WarnUnknown = [this, &OutWarnings](const FString& Option, const TCHAR* ListName)
  {
    // A dynamic list (devices, resolutions) may not hold the option yet, so it is not an error there.
    if (!Values.IsEmpty() && !IsKnownValue(Option))
    {
      OutWarnings.Add(FString::Printf(TEXT("%s: %s names '%s', which is not an option (%s)"),
        *SettingTag.ToString(), ListName, *Option, *FString::Join(Values, TEXT(", "))));
    }
  };

  if (!Rule->AllowedOptions.IsEmpty())
  {
    for (const FString& Allowed : Rule->AllowedOptions)
    {
      WarnUnknown(Allowed, TEXT("AllowedOptions"));
    }
    for (const FString& Option : Values)
    {
      if (!Rule->AllowedOptions.ContainsByPredicate([&Option](const FString& Allowed) { return Allowed.Equals(Option, ESearchCase::IgnoreCase); }))
      {
        DeviceHiddenOptions.AddUnique(Option);
      }
    }
  }

  for (const FString& Hidden : Rule->HiddenOptions)
  {
    WarnUnknown(Hidden, TEXT("HiddenOptions"));
    DeviceHiddenOptions.AddUnique(Hidden);
  }

  if (!Values.IsEmpty() && GetOfferedIndices().IsEmpty())
  {
    OutWarnings.Add(FString::Printf(TEXT("%s: the rule hides every option; showing all of them instead"), *SettingTag.ToString()));
    DeviceHiddenOptions.Reset();
  }
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
  for (UObject* Outer = GetOuter(); Outer; Outer = Outer->GetOuter())
  {
    if (const UEFPlayerSettingsComponent* PlayerComp = Cast<UEFPlayerSettingsComponent>(Outer))
    {
      if (PlayerComp->GetOwnerRole() != ROLE_Authority && !PlayerComp->IsLocalPlayerComponent())
      {
        return;
      }
      break;
    }
  }
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
      if (PlayerComp->GetOwnerRole() != ROLE_Authority && PlayerComp->IsLocalPlayerComponent())
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

      // A saved value held back by a lock (CanApplyLoadedValue) may be applicable now.
      PlayerComp->RetryPendingLoadedValuesNextTick();
      break;
    }
    CurrentOuter = CurrentOuter->GetOuter();
  }

  NotifyValueChanged();
}
