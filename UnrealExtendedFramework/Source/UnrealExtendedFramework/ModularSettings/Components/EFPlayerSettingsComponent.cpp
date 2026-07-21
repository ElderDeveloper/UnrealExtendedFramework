// Fill out your copyright notice in the Description page of Project Settings.


#include "EFPlayerSettingsComponent.h"

#include "Engine/ActorChannel.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UnrealExtendedFramework/ModularSettings/EFModularSettingsSubsystem.h"
#include "UnrealExtendedFramework/ModularSettings/SaveGame/EFSettingsSaveGame.h"

const FString UEFPlayerSettingsComponent::OldSaveSlotName = TEXT("PlayerSettingsSave");
const FString UEFPlayerSettingsComponent::NewSaveSlotName = TEXT("EFPlayerSettings");
const int32 UEFPlayerSettingsComponent::SaveUserIndex = 0;

UEFPlayerSettingsComponent::UEFPlayerSettingsComponent() 
{
  PrimaryComponentTick.bCanEverTick = false;
  SetIsReplicatedByDefault(true);
}

UEFModularSettingsBase* UEFPlayerSettingsComponent::AddSettingFromTemplate_Local(UEFModularSettingsBase* Template) 
{
  if (!Template)
  {
    return nullptr;
  }

  if (!Template->SettingTag.IsValid()) 
  {
    UE_LOG(LogTemp, Warning,TEXT("[UEFPlayerSettingsComponent] Template has invalid SettingTag. ""Outer=%s"),*GetNameSafe(Template->GetOuter()));
    return nullptr;
  }

  if (UEFModularSettingsBase* ExistingSetting = GetSettingByTag(Template->SettingTag)) 
  {
    // Already exists
    return ExistingSetting;
  }

  UEFModularSettingsBase* NewSetting = DuplicateObject<UEFModularSettingsBase>(Template, this);
  if (!NewSetting) 
  {
    return nullptr;
  }

  Settings.Add(NewSetting);

  // Only register as replicated subobject on server
  if (GetOwnerRole() == ROLE_Authority) 
  {
    if (AActor *Owner = GetOwner()) 
    {
      Owner->AddReplicatedSubObject(NewSetting);
    }
  }

  OnSettingChanged.Broadcast(NewSetting);
  return NewSetting;
}

void UEFPlayerSettingsComponent::BeginPlay() 
{
  Super::BeginPlay();

  auto ConsumeTemplate = [this](UEFModularSettingsBase*Template) 
  {
    if (!Template || !Template->SettingTag.IsValid()) 
    {
      return;
    }

    // Track the definition on the server so clients/late-joiners also create
    // the object.
    if (GetOwnerRole() == ROLE_Authority) 
    {
      bool bExists = false;
      for (const FEFPlayerSettingDefinition &Def : SettingDefinitions) 
      {
        if (Def.Tag == Template->SettingTag) 
        {
          bExists = true;
          break;
        }
      }

      if (!bExists) 
      {
        FEFPlayerSettingDefinition NewDef;
        NewDef.Tag = Template->SettingTag;
        NewDef.Template = Template;
        NewDef.CurrentValue = Template->GetValueAsString();
        SettingDefinitions.Add(NewDef);
      }
    }

    AddSettingFromTemplate_Local(Template);
  };

  // Only create settings from templates on the server (authority).
  // On clients, OTHER players' settings arrive via replication (OnRep_SettingDefinitions,
  // OnRep_RuntimeSettingDefinitions, and OnRep_Settings). Creating local copies here
  // would produce default-value objects that interfere with the replicated subobjects,
  // causing late-joiners to see stale defaults instead of the server's current values.
  if (GetOwnerRole() == ROLE_Authority)
  {
    // Project containers
    if (const UEFModularProjectSettings* ProjectSettings = GetDefault<UEFModularProjectSettings>()) 
    {
      for (const TSoftObjectPtr<UEFModularSettingsContainer>& ContainerPtr : ProjectSettings->PlayerSettingsContainers) 
      {
        if (UEFModularSettingsContainer* Container = ContainerPtr.LoadSynchronous()) 
        {
          for (UEFModularSettingsBase* Template : Container->Settings) 
          {
            ConsumeTemplate(Template);
          }
        }
      }
    }

    for (UEFModularSettingsBase* Template : DefaultSettings) 
    {
      ConsumeTemplate(Template);
    }
  }

  // --- Deferred load: check if we can identify the local player now ---
  if (IsLocalPlayerComponent())
  {
    LoadState = EPlayerSettingsLoadState::LoadingFromDisk;
    LoadPlayerSettings();
  }
  else
  {
    // PlayerState may not have replicated yet. Retry on a timer.
    LoadState = EPlayerSettingsLoadState::WaitingForOwner;
    OwnerRetryCount = 0;
    
    if (UWorld* World = GetWorld())
    {
      World->GetTimerManager().SetTimer(
        OwnerRetryTimer, this, &UEFPlayerSettingsComponent::RetryIdentifyOwner,
        0.1f, true); // Every 100ms
    }
  }
}

void UEFPlayerSettingsComponent::RetryIdentifyOwner()
{
  OwnerRetryCount++;
  
  if (IsLocalPlayerComponent())
  {
    // Found the local player — stop retrying and load
    if (UWorld* World = GetWorld())
    {
      World->GetTimerManager().ClearTimer(OwnerRetryTimer);
    }
    
    LoadState = EPlayerSettingsLoadState::LoadingFromDisk;
    LoadPlayerSettings();
    return;
  }
  
  if (OwnerRetryCount >= MaxOwnerRetries)
  {
    // Give up — this component is not for the local player
    if (UWorld* World = GetWorld())
    {
      World->GetTimerManager().ClearTimer(OwnerRetryTimer);
    }
    
    LoadState = EPlayerSettingsLoadState::Ready;
    UE_LOG(LogTemp, Log, TEXT("[UEFPlayerSettingsComponent] Not local player component after %d retries. Skipping load."), OwnerRetryCount);
  }
}

void UEFPlayerSettingsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const 
{
  Super::GetLifetimeReplicatedProps(OutLifetimeProps);
  DOREPLIFETIME(UEFPlayerSettingsComponent, Settings);
  DOREPLIFETIME(UEFPlayerSettingsComponent, SettingDefinitions);
  DOREPLIFETIME(UEFPlayerSettingsComponent, RuntimeSettingDefinitions);
}

bool UEFPlayerSettingsComponent::ReplicateSubobjects(UActorChannel *Channel, FOutBunch *Bunch, FReplicationFlags *RepFlags) 
{
  bool bWroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);

  for (UEFModularSettingsBase* Setting : Settings) 
  {
    if (Setting) 
    {
      bWroteSomething |= Channel->ReplicateSubobject(Setting, *Bunch, *RepFlags);
    }
  }

  return bWroteSomething;
}

UEFModularSettingsBase* UEFPlayerSettingsComponent::GetSettingByTag(FGameplayTag Tag) const 
{
  for (auto Setting : Settings) 
  {
    if (Setting && Setting->SettingTag == Tag) 
    {
      return Setting;
    }
  }
  return nullptr;
}

TArray<UEFModularSettingsBase*> UEFPlayerSettingsComponent::GetSettingsByCategory(FName Category) const 
{
  TArray<UEFModularSettingsBase*> FoundSettings;
  for (auto Setting : Settings) 
  {
    if (Setting && Setting->ConfigCategory == Category) 
    {
      FoundSettings.Add(Setting);
    }
  }
  return FoundSettings;
}

void UEFPlayerSettingsComponent::RequestUpdateSetting(FGameplayTag Tag, const FString &NewValue, bool bFromLoad) 
{
  if (GetOwnerRole() == ROLE_Authority) 
  {
    // Authority path (listen-server host or dedicated server)
    if (UEFModularSettingsBase *Setting = GetSettingByTag(Tag)) 
    {
      if (Setting->Validate(NewValue)) 
      {
        Setting->SetValueFromString(NewValue);
        Setting->Apply();
        OnSettingChanged.Broadcast(Setting);

        // Keep the definition's CurrentValue in sync so late-joiners get it.
        UpdateDefinitionCurrentValue(Tag, Setting->GetValueAsString());

        // Only save on explicit user changes, NOT during load
        if (!bFromLoad)
        {
          RequestSave();
        }
      }
    }
  } 
  else 
  {
    // Client: apply value locally first, then tell server
    if (UEFModularSettingsBase *Setting = GetSettingByTag(Tag)) 
    {
      if (Setting->Validate(NewValue)) 
      {
        Setting->SetValueFromString(NewValue);
        Setting->Apply();
        OnSettingChanged.Broadcast(Setting);
        
        // Only save on explicit user changes, NOT during load
        if (!bFromLoad)
        {
          RequestSave();
        }
      }
    }
    
    // Send to server
    ServerUpdateSetting(Tag, NewValue);
  }
}

bool UEFPlayerSettingsComponent::ServerUpdateSetting_Validate(FGameplayTag Tag, const FString &NewValue) 
{
  // Reject obviously invalid tags; value validation is done against the
  // setting.
  return Tag.IsValid();
}

void UEFPlayerSettingsComponent::ServerUpdateSetting_Implementation(FGameplayTag Tag, const FString &NewValue) 
{
  if (UEFModularSettingsBase *Setting = GetSettingByTag(Tag)) 
  {
    if (Setting->Validate(NewValue)) 
    {
      Setting->SetValueFromString(NewValue);
      Setting->Apply();
      OnSettingChanged.Broadcast(Setting);

      // Keep the definition's CurrentValue in sync so late-joiners get it.
      UpdateDefinitionCurrentValue(Tag, Setting->GetValueAsString());

      // For server/listen-server player: OnRep doesn't fire, so save here.
      // Uses coalescing to batch multiple updates.
      RequestSave();
    }
  }
}

void UEFPlayerSettingsComponent::OnRep_Settings() 
{
  // If we haven't loaded our saved values yet, don't broadcast server defaults
  // to the UI — they would be overwritten shortly anyway.
  if (LoadState < EPlayerSettingsLoadState::SyncingWithServer)
  {
    return;
  }

  for (auto Setting : Settings) 
  {
    if (Setting) 
    {
      OnSettingChanged.Broadcast(Setting);
    }
  }

  // Try to apply any pending values for newly replicated settings
  TryApplyPendingValues();
}

void UEFPlayerSettingsComponent::OnRep_SettingDefinitions() 
{
  // Build any missing settings from replicated definitions.
  for (const FEFPlayerSettingDefinition &Def : SettingDefinitions) 
  {
    if (!Def.Tag.IsValid()) 
    {
      continue;
    }

    if (GetSettingByTag(Def.Tag)) 
    {
      continue;
    }

    UEFModularSettingsBase*Template = Def.Template.LoadSynchronous();
    if (!Template) 
    {
      UE_LOG(LogTemp, Warning,TEXT("[UEFPlayerSettingsComponent] Failed to load template for ""tag %s"),*Def.Tag.ToString());
      continue;
    }

    // Safety: ensure the loaded template tag matches the replicated tag.
    if (Template->SettingTag != Def.Tag) 
    {
      UE_LOG(LogTemp, Warning,TEXT("[UEFPlayerSettingsComponent] Template tag mismatch. Def=%s ""Template=%s"),*Def.Tag.ToString(), *Template->SettingTag.ToString());
    }

    AddSettingFromTemplate_Local(Template);
  }

  // Apply the server's current values from the replicated definitions.
  // This ensures late-joiners see the correct state even if subobject
  // property replication hasn't delivered the values yet.
  for (const FEFPlayerSettingDefinition &Def : SettingDefinitions) 
  {
    if (!Def.Tag.IsValid() || Def.CurrentValue.IsEmpty()) 
    {
      continue;
    }

    if (UEFModularSettingsBase* Setting = GetSettingByTag(Def.Tag)) 
    {
      if (Setting->GetValueAsString() != Def.CurrentValue)
      {
        Setting->SetValueFromString(Def.CurrentValue);
        Setting->Apply();
        OnSettingChanged.Broadcast(Setting);
      }
    }
  }

  // Try to apply pending values to any newly created settings
  TryApplyPendingValues();
}

UEFModularSettingsBool *UEFPlayerSettingsComponent::AddBoolSetting(FGameplayTag Tag, FText DisplayName, FName ConfigCategory,bool DefaultValue, bool InitialValue) 
{
  if (!Tag.IsValid()) 
  {
    UE_LOG(LogTemp, Warning,TEXT("[UEFPlayerSettingsComponent::AddBoolSetting] Invalid tag ""provided."));
    return nullptr;
  }

  // Check if setting already exists
  if (UEFModularSettingsBool*Existing = Cast<UEFModularSettingsBool>(GetSettingByTag(Tag))) 
  {
    return Existing;
  }

  // Only server can create new settings
  if (GetOwnerRole() != ROLE_Authority) 
  {
    UE_LOG(LogTemp, Warning,TEXT("[UEFPlayerSettingsComponent::AddBoolSetting] Only server can ""create settings."));
    return nullptr;
  }

  // Create the runtime definition that will be replicated to clients
  FEFRuntimeSettingDefinition Def;
  Def.Tag = Tag;
  Def.DisplayName = DisplayName;
  Def.ConfigCategory = ConfigCategory;
  Def.SettingType = 0; // Bool
  Def.BoolDefaultValue = DefaultValue;
  Def.BoolValue = InitialValue;
  RuntimeSettingDefinitions.Add(Def);

  // Create the setting locally on server
  UEFModularSettingsBool* NewSetting = Cast<UEFModularSettingsBool>(CreateSettingFromRuntimeDefinition(Def));

  return NewSetting;
}

UEFModularSettingsFloat *UEFPlayerSettingsComponent::AddFloatSetting(FGameplayTag Tag, FText DisplayName, FName ConfigCategory,float DefaultValue, float InitialValue, float Min, float Max) 
{
  if (!Tag.IsValid()) 
  {
    UE_LOG(LogTemp, Warning,TEXT("[UEFPlayerSettingsComponent::AddFloatSetting] Invalid tag ""provided."));
    return nullptr;
  }

  // Check if setting already exists
  if (UEFModularSettingsFloat*Existing = Cast<UEFModularSettingsFloat>(GetSettingByTag(Tag))) 
  {
    return Existing;
  }

  // Only server can create new settings
  if (GetOwnerRole() != ROLE_Authority) 
  {
    UE_LOG(LogTemp, Warning,TEXT("[UEFPlayerSettingsComponent::AddFloatSetting] Only server can ""create settings."));
    return nullptr;
  }

  // Create the runtime definition that will be replicated to clients
  FEFRuntimeSettingDefinition Def;
  Def.Tag = Tag;
  Def.DisplayName = DisplayName;
  Def.ConfigCategory = ConfigCategory;
  Def.SettingType = 1; // Float
  Def.FloatDefaultValue = DefaultValue;
  Def.FloatMin = Min;
  Def.FloatMax = Max;
  Def.FloatValue = FMath::Clamp(InitialValue, Min, Max);
  RuntimeSettingDefinitions.Add(Def);

  // Create the setting locally on server
  UEFModularSettingsFloat*NewSetting = Cast<UEFModularSettingsFloat>(CreateSettingFromRuntimeDefinition(Def));

  return NewSetting;
}

UEFModularSettingsMultiSelect* UEFPlayerSettingsComponent::AddMultiSelectSetting(FGameplayTag Tag, FText DisplayName, FName ConfigCategory,const TArray<FString> &Values, const TArray<FText> &ValueDisplayNames,int32 DefaultIndex, int32 InitialIndex) 
{
  if (!Tag.IsValid()) 
  {
    UE_LOG(LogTemp, Warning,TEXT("[UEFPlayerSettingsComponent::AddMultiSelectSetting] Invalid ""tag provided."));
    return nullptr;
  }

  // Check if setting already exists
  if (UEFModularSettingsMultiSelect *Existing = Cast<UEFModularSettingsMultiSelect>(GetSettingByTag(Tag))) 
  {
    return Existing;
  }

  // Only server can create new settings
  if (GetOwnerRole() != ROLE_Authority) 
  {
    UE_LOG(LogTemp, Warning,TEXT("[UEFPlayerSettingsComponent::AddMultiSelectSetting] Only ""server can create settings."));
    return nullptr;
  }

  // Create the runtime definition that will be replicated to clients
  FEFRuntimeSettingDefinition Def;
  Def.Tag = Tag;
  Def.DisplayName = DisplayName;
  Def.ConfigCategory = ConfigCategory;
  Def.SettingType = 2; // MultiSelect
  Def.MultiSelectValues = Values;
  Def.MultiSelectDisplayNames = ValueDisplayNames;
  Def.MultiSelectDefaultValue = Values.IsValidIndex(DefaultIndex) ? Values[DefaultIndex] : TEXT("");
  Def.MultiSelectIndex = Values.IsValidIndex(InitialIndex) ? InitialIndex : 0;
  RuntimeSettingDefinitions.Add(Def);

  // Create the setting locally on server
  UEFModularSettingsMultiSelect* NewSetting = Cast<UEFModularSettingsMultiSelect>(CreateSettingFromRuntimeDefinition(Def));

  return NewSetting;
}

UEFModularSettingsBase* UEFPlayerSettingsComponent::CreateSettingFromRuntimeDefinition(const FEFRuntimeSettingDefinition &Def) 
{
  if (!Def.Tag.IsValid()) 
  {
    return nullptr;
  }

  // Already exists?
  if (UEFModularSettingsBase *ExistingSetting = GetSettingByTag(Def.Tag)) 
  {
    return ExistingSetting;
  }

  UEFModularSettingsBase* NewSetting = nullptr;

  switch (Def.SettingType) 
  {
  case 0: // Bool
  {
    UEFModularSettingsBool* BoolSetting = NewObject<UEFModularSettingsBool>(this);
    BoolSetting->SettingTag = Def.Tag;
    BoolSetting->DisplayName = Def.DisplayName;
    BoolSetting->ConfigCategory = Def.ConfigCategory;
    BoolSetting->DefaultValue = Def.BoolDefaultValue;
    BoolSetting->Value = Def.BoolValue;
    NewSetting = BoolSetting;
  } break;

  case 1: // Float
  {
    UEFModularSettingsFloat* FloatSetting = NewObject<UEFModularSettingsFloat>(this);
    FloatSetting->SettingTag = Def.Tag;
    FloatSetting->DisplayName = Def.DisplayName;
    FloatSetting->ConfigCategory = Def.ConfigCategory;
    FloatSetting->DefaultValue = Def.FloatDefaultValue;
    FloatSetting->Min = Def.FloatMin;
    FloatSetting->Max = Def.FloatMax;
    FloatSetting->Value = Def.FloatValue;
    NewSetting = FloatSetting;
  } break;

  case 2: // MultiSelect
  {
    UEFModularSettingsMultiSelect* MultiSetting = NewObject<UEFModularSettingsMultiSelect>(this);
    MultiSetting->SettingTag = Def.Tag;
    MultiSetting->DisplayName = Def.DisplayName;
    MultiSetting->ConfigCategory = Def.ConfigCategory;
    MultiSetting->Values = Def.MultiSelectValues;
    MultiSetting->DisplayNames = Def.MultiSelectDisplayNames;
    MultiSetting->DefaultValue = Def.MultiSelectDefaultValue;
    MultiSetting->SelectedIndex = Def.MultiSelectIndex;
    NewSetting = MultiSetting;
  } break;
  }

  if (NewSetting) 
  {
    Settings.Add(NewSetting);

    // Only register as replicated subobject on server
    if (GetOwnerRole() == ROLE_Authority) 
    {
      if (AActor *Owner = GetOwner()) 
      {
        Owner->AddReplicatedSubObject(NewSetting);
      }
    }

    OnSettingChanged.Broadcast(NewSetting);
  }

  return NewSetting;
}

void UEFPlayerSettingsComponent::OnRep_RuntimeSettingDefinitions() 
{
  // Build any missing settings from replicated runtime definitions.
  for (const FEFRuntimeSettingDefinition& Def : RuntimeSettingDefinitions) 
  {
    if (!Def.Tag.IsValid())
    {
      continue;
    }

    if (GetSettingByTag(Def.Tag)) 
    {
      continue;
    }

    CreateSettingFromRuntimeDefinition(Def);
  }

  // Try to apply pending values to any newly created settings
  TryApplyPendingValues();
}

// ============================================================================
// Persistence Methods
// ============================================================================

bool UEFPlayerSettingsComponent::IsLocalPlayerComponent() const 
{
  // Check if this component is attached to the local player's PlayerState
  AActor* Owner = GetOwner();
  if (!Owner) 
  {
    return false;
  }

  // Check if owner is a PlayerState
  if (APlayerState* PS = Cast<APlayerState>(Owner)) 
  {
    // Get the local player controller
    if (UWorld* World = GetWorld()) 
    {
      APlayerController* LocalPC = World->GetFirstPlayerController();
      if (LocalPC && LocalPC->PlayerState == PS) 
      {
        return true;
      }
    }
  }

  // Check if owner is a PlayerController
  if (APlayerController* PC = Cast<APlayerController>(Owner)) 
  {
    return PC->IsLocalController();
  }

  // Check if owner is a Pawn with a local controller
  if (APawn* Pawn = Cast<APawn>(Owner)) 
  {
    AController* Controller = Pawn->GetController();
    if (APlayerController* PC = Cast<APlayerController>(Controller)) 
    {
      return PC->IsLocalController();
    }
  }

  return false;
}

FString UEFPlayerSettingsComponent::GetPlayerSettingsSaveSlotName() const 
{
  return NewSaveSlotName;
}

// ============================================================================
// Save Coalescing
// ============================================================================

void UEFPlayerSettingsComponent::RequestSave()
{
  // Only save for local player
  if (!IsLocalPlayerComponent())
  {
    return;
  }

  bSavePending = true;

  // Reset the coalescing timer — actual save happens after 0.5s of inactivity
  if (UWorld* World = GetWorld())
  {
    World->GetTimerManager().ClearTimer(SaveCoalesceTimer);
    World->GetTimerManager().SetTimer(
      SaveCoalesceTimer, this, &UEFPlayerSettingsComponent::ExecutePendingSave,
      0.5f, false);
  }
}

void UEFPlayerSettingsComponent::ExecutePendingSave()
{
  if (!bSavePending)
  {
    return;
  }

  bSavePending = false;
  SavePlayerSettings();
}

void UEFPlayerSettingsComponent::SavePlayerSettings() 
{
  // Only save for local player
  if (!IsLocalPlayerComponent()) 
  {
    return;
  }

  UEFSettingsSaveGame* SaveGameInstance = Cast<UEFSettingsSaveGame>(UGameplayStatics::CreateSaveGameObject(UEFSettingsSaveGame::StaticClass()));

  if (!SaveGameInstance) 
  {
    UE_LOG(LogTemp, Warning,TEXT("[UEFPlayerSettingsComponent] Failed to create save game object."));
    return;
  }

  // Write ONLY player settings — no read-modify-write of other data
  SaveGameInstance->PlayerSettings.Empty();
  for (UEFModularSettingsBase* Setting : Settings) 
  {
    if (Setting && Setting->SettingTag.IsValid()) 
    {
      SaveGameInstance->PlayerSettings.Add(Setting->SettingTag,Setting->GetValueAsString());
    }
  }

  // Write to disk using the dedicated player settings slot
  if (UGameplayStatics::SaveGameToSlot(SaveGameInstance,GetPlayerSettingsSaveSlotName(),SaveUserIndex)) 
  {
    UE_LOG(LogTemp, Log,TEXT("[UEFPlayerSettingsComponent] Saved %d player settings to ""slot: %s"),SaveGameInstance->PlayerSettings.Num(),*GetPlayerSettingsSaveSlotName());
  } 
  else 
  {
    UE_LOG(LogTemp, Warning,TEXT("[UEFPlayerSettingsComponent] Failed to save player settings."));
  }
}

// ============================================================================
// Load + Deferred Application
// ============================================================================

bool UEFPlayerSettingsComponent::TryMigrateFromOldSaveFormat()
{
  // Check if old shared save slot exists and has player data
  if (!UGameplayStatics::DoesSaveGameExist(OldSaveSlotName, SaveUserIndex))
  {
    return false;
  }

  UEFSettingsSaveGame* OldSave = Cast<UEFSettingsSaveGame>(
    UGameplayStatics::LoadGameFromSlot(OldSaveSlotName, SaveUserIndex));

  if (!OldSave || OldSave->PlayerSettings.Num() == 0)
  {
    return false;
  }

  UE_LOG(LogTemp, Log, TEXT("[UEFPlayerSettingsComponent] Migrating %d settings from old save format (%s -> %s)"),
    OldSave->PlayerSettings.Num(), *OldSaveSlotName, *NewSaveSlotName);

  // Copy player settings into pending values map
  PendingLoadedValues = OldSave->PlayerSettings;

  return true;
}

void UEFPlayerSettingsComponent::LoadPlayerSettings() 
{
  // Only load for local player
  if (!IsLocalPlayerComponent()) 
  {
    return;
  }

  LoadState = EPlayerSettingsLoadState::LoadingFromDisk;

  // Try new save slot first
  if (UGameplayStatics::DoesSaveGameExist(GetPlayerSettingsSaveSlotName(), SaveUserIndex))
  {
    UEFSettingsSaveGame* LoadedGame = Cast<UEFSettingsSaveGame>(
      UGameplayStatics::LoadGameFromSlot(GetPlayerSettingsSaveSlotName(), SaveUserIndex));

    if (LoadedGame)
    {
      PendingLoadedValues = LoadedGame->PlayerSettings;
      UE_LOG(LogTemp, Log, TEXT("[UEFPlayerSettingsComponent] Loaded %d pending values from slot: %s"),
        PendingLoadedValues.Num(), *GetPlayerSettingsSaveSlotName());
    }
    else
    {
      UE_LOG(LogTemp, Warning, TEXT("[UEFPlayerSettingsComponent] Failed to load player settings from slot: %s"),
        *GetPlayerSettingsSaveSlotName());
    }
  }
  else if (!TryMigrateFromOldSaveFormat())
  {
    UE_LOG(LogTemp, Log, TEXT("[UEFPlayerSettingsComponent] No saved player settings found. Using defaults."));
  }

  // Try to apply whatever we can right now
  LoadState = EPlayerSettingsLoadState::SyncingWithServer;
  TryApplyPendingValues();

  // If all pending values were applied, we're done
  if (PendingLoadedValues.Num() == 0)
  {
    LoadState = EPlayerSettingsLoadState::Ready;
  }
  // Otherwise, remaining values will be applied as settings arrive via OnRep
}

void UEFPlayerSettingsComponent::TryApplyPendingValues()
{
  if (PendingLoadedValues.Num() == 0)
  {
    return;
  }

  if (!IsLocalPlayerComponent())
  {
    return;
  }

  TArray<FGameplayTag> AppliedTags;

  for (const auto& Pair : PendingLoadedValues)
  {
    if (UEFModularSettingsBase* Setting = GetSettingByTag(Pair.Key))
    {
      // Apply the saved value locally
      Setting->SetValueFromString(Pair.Value);
      Setting->SaveCurrentValue();
      Setting->ClearDirty();

      // Send RPC to server with bFromLoad=true to prevent circular save
      RequestUpdateSetting(Pair.Key, Pair.Value, /*bFromLoad=*/true);

      AppliedTags.Add(Pair.Key);
    }
  }

  // Remove successfully applied entries from the pending map
  for (const FGameplayTag& Tag : AppliedTags)
  {
    PendingLoadedValues.Remove(Tag);
  }

  if (AppliedTags.Num() > 0)
  {
    UE_LOG(LogTemp, Log, TEXT("[UEFPlayerSettingsComponent] Applied %d pending values. %d remaining."),
      AppliedTags.Num(), PendingLoadedValues.Num());
  }

  // Transition to Ready once all pending values are applied
  if (PendingLoadedValues.Num() == 0 && LoadState == EPlayerSettingsLoadState::SyncingWithServer)
  {
    LoadState = EPlayerSettingsLoadState::Ready;
    
    // Save to the new slot (ensures migration is persisted)
    RequestSave();
  }
}

void UEFPlayerSettingsComponent::UpdateDefinitionCurrentValue(FGameplayTag Tag, const FString& NewValue)
{
  for (FEFPlayerSettingDefinition& Def : SettingDefinitions)
  {
    if (Def.Tag == Tag)
    {
      Def.CurrentValue = NewValue;
      return;
    }
  }
}
