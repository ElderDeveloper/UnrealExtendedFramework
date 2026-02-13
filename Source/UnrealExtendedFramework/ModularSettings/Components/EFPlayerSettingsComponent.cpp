// Fill out your copyright notice in the Description page of Project Settings.

#include "EFPlayerSettingsComponent.h"

#include "Engine/ActorChannel.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "UnrealExtendedFramework/ModularSettings/EFModularSettingsSubsystem.h"
#include "UnrealExtendedFramework/ModularSettings/SaveGame/EFSettingsSaveGame.h"

static const FString PlayerSettingsSaveSlotName = TEXT("PlayerSettingsSave");
static const int32 PlayerSettingsUserIndex = 0;

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
        SettingDefinitions.Add(NewDef);
      }
    }

    AddSettingFromTemplate_Local(Template);
  };

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

  // Load saved settings for local player after all settings are created
  LoadPlayerSettings();
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

void UEFPlayerSettingsComponent::RequestUpdateSetting(FGameplayTag Tag, const FString &NewValue) 
{
  if (GetOwnerRole() == ROLE_Authority) 
  {
    ServerUpdateSetting_Implementation(Tag, NewValue);
  } 
  else 
  {
    // Client: apply value locally first, save, then tell server
    if (UEFModularSettingsBase *Setting = GetSettingByTag(Tag)) 
    {
      if (Setting->Validate(NewValue)) 
      {
        Setting->SetValueFromString(NewValue);
        Setting->Apply();
        OnSettingChanged.Broadcast(Setting);
        
        // Save locally on client
        SavePlayerSettings();
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

      // For server/listen-server player: OnRep doesn't fire, so we need to save
      // here SavePlayerSettings() internally checks IsLocalPlayerComponent()
      SavePlayerSettings();
    }
  }
}

void UEFPlayerSettingsComponent::OnRep_Settings() 
{
  for (auto Setting : Settings) 
  {
    if (Setting) 
    {
      OnSettingChanged.Broadcast(Setting);
    }
  }

  // NOTE: Do NOT save here. This fires on replication which may contain server defaults.
  // Saving is handled in ServerUpdateSetting_Implementation when user explicitly changes settings.
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

  // For clients: after settings definitions are replicated (settings may already exist from subobject replication),
  // load and apply saved settings to restore client's preferences
  // Only do this once to avoid repeatedly applying on every replication update
  if (!bHasAppliedSavedSettings && IsLocalPlayerComponent()) 
  {
    bHasAppliedSavedSettings = true;
    ApplySavedSettingsToExisting();
  }
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

  // For clients: after runtime settings definitions are replicated,
  // load and apply saved settings to restore client's preferences
  // Only do this once to avoid repeatedly applying on every replication update
  if (!bHasAppliedSavedSettings && IsLocalPlayerComponent()) 
  {
    bHasAppliedSavedSettings = true;
    ApplySavedSettingsToExisting();
  }
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
  // Single slot for local player settings
  return PlayerSettingsSaveSlotName;
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

  // First, load existing save to preserve other data
  if (UGameplayStatics::DoesSaveGameExist(GetPlayerSettingsSaveSlotName(),PlayerSettingsUserIndex)) 
  {
    if (UEFSettingsSaveGame*ExistingSave = Cast<UEFSettingsSaveGame>(UGameplayStatics::LoadGameFromSlot(GetPlayerSettingsSaveSlotName(), PlayerSettingsUserIndex))) 
    {
      // Preserve subsystem settings
      SaveGameInstance->StoredSettings = ExistingSave->StoredSettings;
    }
  }

  // Save all player settings
  SaveGameInstance->PlayerSettings.Empty();
  for (UEFModularSettingsBase* Setting : Settings) 
  {
    if (Setting && Setting->SettingTag.IsValid()) 
    {
      SaveGameInstance->PlayerSettings.Add(Setting->SettingTag,Setting->GetValueAsString());
    }
  }

  // Write to disk
  if (UGameplayStatics::SaveGameToSlot(SaveGameInstance,GetPlayerSettingsSaveSlotName(),PlayerSettingsUserIndex)) 
  {
    UE_LOG(LogTemp, Log,TEXT("[UEFPlayerSettingsComponent] Saved %d player settings to ""slot: %s"),SaveGameInstance->PlayerSettings.Num(),*GetPlayerSettingsSaveSlotName());
  } 
  else 
  {
    UE_LOG(LogTemp, Warning,TEXT("[UEFPlayerSettingsComponent] Failed to save player settings."));
  }
}

void UEFPlayerSettingsComponent::LoadPlayerSettings() 
{
  // Only load for local player
  if (!IsLocalPlayerComponent()) 
  {
    return;
  }

  if (!UGameplayStatics::DoesSaveGameExist(GetPlayerSettingsSaveSlotName(),PlayerSettingsUserIndex)) 
  {
    UE_LOG(LogTemp, Log,TEXT("[UEFPlayerSettingsComponent] No saved player settings found. ""Using defaults."));
    return;
  }

  UEFSettingsSaveGame*LoadedGame = Cast<UEFSettingsSaveGame>(UGameplayStatics::LoadGameFromSlot(GetPlayerSettingsSaveSlotName(), PlayerSettingsUserIndex));

  if (!LoadedGame) 
  {
    UE_LOG(LogTemp, Warning,TEXT("[UEFPlayerSettingsComponent] Failed to load player settings."));
    return;
  }

  int32 LoadedCount = 0;
  for (const auto &StoredPair : LoadedGame->PlayerSettings) 
  {
    if (UEFModularSettingsBase*Setting = GetSettingByTag(StoredPair.Key)) 
    {
      // Load the value locally first
      Setting->SetValueFromString(StoredPair.Value);
      Setting->SaveCurrentValue(); // Mark as saved state
      Setting->ClearDirty();

      // Request server to apply this value (for multiplayer sync)
      RequestUpdateSetting(StoredPair.Key, StoredPair.Value);
      LoadedCount++;
    }
  }


  UE_LOG(LogTemp, Log,TEXT("[UEFPlayerSettingsComponent] Loaded %d player settings from ""slot: %s"),LoadedCount, *GetPlayerSettingsSaveSlotName());
}

void UEFPlayerSettingsComponent::ApplySavedSettingsToExisting() {
  // This method is called on clients when settings are created via replication
  // It loads saved settings from disk and applies them to existing settings,
  // then syncs with the server so the server has the correct values

  if (!IsLocalPlayerComponent()) 
  {
    return;
  }

  if (!UGameplayStatics::DoesSaveGameExist(GetPlayerSettingsSaveSlotName(),PlayerSettingsUserIndex)) 
  {
    UE_LOG(LogTemp, Log,TEXT("[UEFPlayerSettingsComponent::ApplySavedSettingsToExisting] No ""saved player settings found."));
    return;
  }

  UEFSettingsSaveGame*LoadedGame = Cast<UEFSettingsSaveGame>(UGameplayStatics::LoadGameFromSlot(GetPlayerSettingsSaveSlotName(), PlayerSettingsUserIndex));

  if (!LoadedGame) 
  {
    UE_LOG(LogTemp, Warning,TEXT("[UEFPlayerSettingsComponent::ApplySavedSettingsToExisting] Failed to load saved settings."));
    return;
  }

  int32 AppliedCount = 0;
  for (const auto &StoredPair : LoadedGame->PlayerSettings) 
  {
    if (UEFModularSettingsBase *Setting = GetSettingByTag(StoredPair.Key)) 
    {
      // Apply the saved value locally
      Setting->SetValueFromString(StoredPair.Value);
      Setting->SaveCurrentValue();
      Setting->ClearDirty();

      // Send RPC to server to update the setting with saved value
      // This ensures the server has the correct value for this client
      RequestUpdateSetting(StoredPair.Key, StoredPair.Value);
      AppliedCount++;
    }
  }


  UE_LOG(LogTemp, Log,TEXT("[UEFPlayerSettingsComponent::ApplySavedSettingsToExisting] ""Applied %d saved settings after replication."),AppliedCount);
}

