// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/Engine.h"
#include "UnrealExtendedFramework/ModularSettings/Device/EFModularSettingsDeviceRules.h"
#include "EFModularSettingsBase.generated.h"

class UEFModularSettingsSubsystem;

UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class UNREALEXTENDEDFRAMEWORK_API UEFModularSettingsBase : public UObject
{
	GENERATED_BODY()
public:
	virtual bool IsSupportedForNetworking() const override { return true; }
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly, Category="Settings")
	FGameplayTag SettingTag;

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly, Category="Settings")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly, Category="Settings")
	FName ConfigCategory = TEXT("Settings");

	/**
	 * The scalability group this setting drives, as its console variable (sg.ShadowQuality,
	 * sg.EffectsQuality, ...). The setting's device default is then the active device profile's level for
	 * that group, so a device profile's +CVars=sg.* lines become the menu's defaults. Empty for a setting
	 * that drives no group; the plugin's own quality tags are recognised without it.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settings|Device")
	FName ScalabilityGroup;

	/**
	 * Which save bucket this setting uses. Auto follows UEFModularProjectSettings::DeviceScopedSettings;
	 * Device keeps one value per device family (a PC and a Steam Deck sharing a cloud save each keep
	 * their own), User follows the player everywhere.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settings|Device")
	EEFSettingScope Scope = EEFSettingScope::Auto;

	/** ScalabilityGroup, or the group of a plugin quality tag when it is empty. */
	UFUNCTION(BlueprintPure, Category = "Settings|Device")
	FName GetEffectiveScalabilityGroup() const;

	/** Whether this device shows the setting and lets the player change it (device rules, capability). */
	UFUNCTION(BlueprintPure, Category = "Settings|Device")
	EEFSettingAvailability GetAvailability() const { return DeviceAvailability; }

	UFUNCTION(BlueprintPure, Category = "Settings|Device")
	bool IsVisibleOnThisDevice() const { return DeviceAvailability != EEFSettingAvailability::Hidden; }

	UFUNCTION(BlueprintPure, Category = "Settings|Device")
	bool IsEditableOnThisDevice() const { return DeviceAvailability == EEFSettingAvailability::Visible; }

	/**
	 * Whether this machine can use the setting at all. False hides it, without any config: an upscaler
	 * mode on hardware that has no such upscaler. Asked again once the PostEngineInit phase completes.
	 */
	virtual bool IsSupportedOnThisMachine() const { return true; }

	/** Sorts the startup and Apply passes: lower first. A preset that writes other settings goes first. */
	virtual int32 GetApplyPriority() const { return 0; }

	/**
	 * False for a setting that only views another one (a single "mode" row for whichever upscaler is
	 * selected): the source owns the value, so the view is neither saved nor reset to a default on load.
	 */
	virtual bool IsPersisted() const { return true; }

	/** The default as the setting saves it ("Low", "true", "0.5"). */
	UFUNCTION(BlueprintPure, Category = "Settings|Device")
	virtual FString GetDefaultValueAsString() const { return FString(); }

	/** Replaces the default. Returns false, leaving it untouched, for a value the setting cannot hold. */
	virtual bool SetDefaultValueFromString(const FString& NewDefault) { return false; }

	/** The default authored on the asset or class, before any device rule touched it. */
	const FString& GetAuthoredDefaultValue() const { return AuthoredDefaultValue; }

	/** Remembers the authored default. Called once by the subsystem when it registers the setting. */
	void CaptureAuthoredDefault();

	/**
	 * A default the setting computes from the machine (the monitor's native resolution, the first
	 * upscaler this machine supports). Empty keeps the authored one. Device rules still win over it.
	 */
	virtual FString ComputeDeviceDefault() const { return FString(); }

	/** Compares two saved values the way this setting means them (numbers as numbers, ids without case). */
	virtual bool AreValuesEqual(const FString& A, const FString& B) const { return A == B; }

	/** Whether a saved value can be taken on this device as is (an offered option, a value inside the range). */
	virtual bool CanTakeValueOnThisDevice(const FString& InValue) const { return true; }

	/**
	 * The nearest value this device can take (a float clamped into the range). Empty when there is no
	 * sensible neighbour, in which case the device default is used.
	 */
	virtual FString AdjustValueToDevice(const FString& InValue) const { return FString(); }

	/** Whether a saved value is one this setting knows at all, on any device. Used to vet old saves. */
	virtual bool IsKnownValue(const FString& InValue) const { return true; }

	/**
	 * Takes this device's rule for the setting (null when there is none): availability, option mask,
	 * range. Restores everything a previous device's rule changed first, so rules never stack.
	 */
	virtual void ApplyDeviceRule(const FEFModularSettingDeviceRule* Rule, TArray<FString>& OutWarnings);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Settings")
	bool Validate(const FString& Value) const;
	virtual bool Validate_Implementation(const FString& Value) const { return true; }

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Settings")
	void RefreshValues();
	virtual void RefreshValues_Implementation() {}

	// When enabled, this setting's derived state (option lists, device names,
	// owned-item locks, ranges) is rebuilt from its source at every gate:
	// before a requested change is validated (RequestChange) and before Apply()
	// resolves the value. At most one refresh runs per frame, so a request that
	// is applied in the same frame refreshes once. Useful for settings whose
	// backing data can go stale between the moment the player picks a value
	// and the moment it is checked or applied.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings")
	bool bRefreshBeforeApply = false;

	// The single entry point for changing a value at runtime. Refreshes derived
	// state (when bRefreshBeforeApply is set), validates the new value against
	// that fresh state, then sets it. Returns false, leaving the value untouched,
	// when the change is rejected (unknown option, locked option, out of range).
	// Apply() is a separate step so callers control when the change takes effect.
	UFUNCTION(BlueprintCallable, Category = "Settings")
	bool RequestChange(const FString& NewValue);

	// Runs RefreshValues() if bRefreshBeforeApply is set, this setting has not
	// already refreshed this frame, and this machine owns the data the refresh
	// reads (see CanRefreshLocally). Safe to call from anywhere; no-op otherwise.
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void EnsureFresh();

	// Apply() is a BlueprintNativeEvent thunk, so every caller — C++ and
	// Blueprint alike — funnels through ProcessEvent. Hooked here so
	// bRefreshBeforeApply covers all Apply call sites, including Blueprint
	// overrides, and so every RefreshValues() call stamps the frame it ran in.
	virtual void ProcessEvent(UFunction* Function, void* Parms) override;

	UFUNCTION(BlueprintCallable,BlueprintNativeEvent, Category = "Settings")
	void Apply();
	virtual void Apply_Implementation() {}

	/**
	 * Whether a value loaded from disk can be taken right now.
	 *
	 * False keeps the value pending on the owning player settings component, which tries again when
	 * this setting's state changes, instead of applying it: applying a value the setting refuses
	 * resets the setting to its default, and the save that follows the load then writes that default
	 * over the player's real choice. Return false only for a refusal that can lift later in the
	 * session (a multi-select option that is locked until ownership is known); a value that can never
	 * be valid should still be applied so the setting falls back as usual.
	 */
	virtual bool CanApplyLoadedValue(const FString& Value) const { return true; }

	/**
	 * The value written to disk. Normally the value in effect.
	 *
	 * A setting that is holding a choice it cannot honour right now (an option the session hides
	 * for a while) returns that choice instead, so the player still has it once the option is back,
	 * including in a later session.
	 */
	virtual FString GetValueToSave() const { return GetValueAsString(); }

	virtual void SaveCurrentValue()
	{
		PreviousValue = GetValueAsString();
		bIsDirty = false;
	}
	
	virtual void RevertToSavedValue() 
	{ 
		SetValueFromString(PreviousValue);
		Apply_Implementation();
		bIsDirty = false;
	}

	UFUNCTION(BlueprintCallable, Category = "Settings")
	virtual FString GetValueAsString() const { return TEXT(""); }

	UFUNCTION(BlueprintCallable, Category = "Settings")
	virtual void SetValueFromString(const FString& Value) 
	{
		// Base implementation just marks dirty if value changed
		// Concrete classes should call Super or handle MarkDirty themselves
		if (Value != GetValueAsString())
		{
			MarkDirty();
		}
	}

	UFUNCTION(BlueprintCallable, Category = "Settings")
	virtual FString GetSavedValueAsString() const { return PreviousValue; }

	UFUNCTION(BlueprintCallable, Category = "Settings")
	virtual void ResetToDefault() 
	{
		// Should be overridden, but ensure we mark dirty
		MarkDirty();
	}

	UFUNCTION(BlueprintCallable, Category = "Settings")
	bool IsDirty() const { return bIsDirty; }

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void MarkDirty() { bIsDirty = true; }

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void ClearDirty() { bIsDirty = false; }

	// Called when setting is registered with subsystem
	virtual void OnRegistered() {}

	// Reference to the subsystem managing this setting
	UPROPERTY(Transient,BlueprintReadOnly)
	TObjectPtr<UEFModularSettingsSubsystem> ModularSettingsSubsystem;
	
	virtual UWorld* GetWorld() const override;

	void NotifyValueChanged();

protected:

	FString PreviousValue;

	UPROPERTY(Transient)
	bool bIsDirty = false;

	/** Set by ApplyDeviceRule from the device rule and IsSupportedOnThisMachine. */
	UPROPERTY(Transient)
	EEFSettingAvailability DeviceAvailability = EEFSettingAvailability::Visible;

	FString AuthoredDefaultValue;
	bool bAuthoredDefaultCaptured = false;

	// RefreshValues implementations read machine-local data (inventory caches,
	// audio devices). For a player setting only the owning machine has that
	// player's data, so a server must not refresh a remote client's setting.
	bool CanRefreshLocally() const;

	// Guards against RefreshValues() re-entering EnsureFresh() (e.g. through
	// ResetToDefault -> SetSelectedIndex) while a refresh is already in flight.
	bool bIsRefreshing = false;

	// GFrameCounter value of the last RefreshValues() call; Max means never.
	uint64 LastRefreshFrame = TNumericLimits<uint64>::Max();
};

/* Bool */
UCLASS(Blueprintable,BlueprintType, EditInlineNew)
class UNREALEXTENDEDFRAMEWORK_API UEFModularSettingsBool : public UEFModularSettingsBase
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings Value")
	bool DefaultValue = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Value, Category="Settings Value")
	bool Value = false;

	UFUNCTION()
	void OnRep_Value();

	UFUNCTION(BlueprintCallable,BlueprintNativeEvent, Category = "Settings")
	void SetValue(bool NewValue);
	virtual void SetValue_Implementation(bool NewValue) 
	{ 
		if (Value != NewValue)
		{
			Value = NewValue;
			MarkDirty();
		}
	}

	virtual void SaveCurrentValue() override { SavedValue = Value; Super::SaveCurrentValue(); }
	virtual void RevertToSavedValue() override { Value = SavedValue; Apply_Implementation(); Super::RevertToSavedValue(); }
	virtual void Apply_Implementation() override {}
	
	virtual FString GetValueAsString() const override
	{
		return Value ? TEXT("true") : TEXT("false");
	}
	
	virtual void SetValueFromString(const FString& NewValue) override
	{
		SetValue(NewValue.ToBool());
	}

	virtual FString GetSavedValueAsString() const override
	{
		return SavedValue ? TEXT("true") : TEXT("false");
	}

	virtual void ResetToDefault() override
	{
		SetValue(DefaultValue);
	}

	virtual FString GetDefaultValueAsString() const override
	{
		return DefaultValue ? TEXT("true") : TEXT("false");
	}

	virtual bool SetDefaultValueFromString(const FString& NewDefault) override;
	virtual bool AreValuesEqual(const FString& A, const FString& B) const override;
	virtual bool IsKnownValue(const FString& InValue) const override;

	bool SavedValue = false;
};

/* Float */
UCLASS(Blueprintable,BlueprintType, EditInlineNew)
class UNREALEXTENDEDFRAMEWORK_API UEFModularSettingsFloat : public UEFModularSettingsBase
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings Value")
	float DefaultValue = 1.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Value, Category="Settings Value")
	float Value = 1.f;

	UFUNCTION()
	void OnRep_Value();

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Settings Value")
	float Min = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Settings Value")
	float Max = 1.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Settings Value")
	float DisplayMin = 0.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Settings Value")
	float DisplayMax = 1.f;

	UFUNCTION(BlueprintCallable,BlueprintNativeEvent, Category = "Settings")
	void SetValue(float NewValue);
	virtual void SetValue_Implementation(float NewValue) 
	{ 
		if (NewValue < Min || NewValue > Max)
		{
			UE_LOG(LogTemp, Warning, TEXT("[UEFModularSettingsFloat] Loaded value %.2f for %s is out of range [%.2f, %.2f]. Resetting to default: %.2f"), 
				NewValue, *SettingTag.ToString(), Min, Max, DefaultValue);
			ResetToDefault();
			return;
		}

		float ClampedValue = FMath::Clamp(NewValue, Min, Max);
		if (!FMath::IsNearlyEqual(Value, ClampedValue))
		{
			Value = ClampedValue;
			MarkDirty();
		}
	}
	
	UFUNCTION(BlueprintPure, Category = "Settings")
	float GetDisplayValue() const
	{
		return FMath::GetMappedRangeValueClamped(FVector2D(Min, Max), FVector2D(DisplayMin, DisplayMax), Value);
	}

	// A requested change must be numeric and inside [Min, Max]. Without this,
	// SetValue would accept the request and then silently reset to default.
	virtual bool Validate_Implementation(const FString& ValueString) const override
	{
		if (!ValueString.IsNumeric())
		{
			return false;
		}

		const float Parsed = FCString::Atof(*ValueString);
		return Parsed >= Min && Parsed <= Max;
	}
	
	virtual void Apply_Implementation() override {}
	virtual void SaveCurrentValue() override { SavedValue = Value; Super::SaveCurrentValue(); }
	virtual void RevertToSavedValue() override { Value = SavedValue; Apply_Implementation(); Super::RevertToSavedValue(); }
	
	
	virtual FString GetValueAsString() const override
	{
		return FString::SanitizeFloat(Value);
	}
	
	virtual void SetValueFromString(const FString& ValueString) override
	{
		SetValue(FCString::Atof(*ValueString));
	}
	
	virtual FString GetSavedValueAsString() const override
	{
		return FString::SanitizeFloat(SavedValue);
	}
	
	virtual void ResetToDefault() override
	{
		SetValue(DefaultValue);
	}

	virtual FString GetDefaultValueAsString() const override
	{
		return FString::SanitizeFloat(DefaultValue);
	}

	virtual bool SetDefaultValueFromString(const FString& NewDefault) override;
	virtual bool AreValuesEqual(const FString& A, const FString& B) const override;
	virtual bool CanTakeValueOnThisDevice(const FString& InValue) const override;
	virtual FString AdjustValueToDevice(const FString& InValue) const override;
	virtual bool IsKnownValue(const FString& InValue) const override;

	/** Narrows Min/Max to the rule's range; the display range follows in proportion. */
	virtual void ApplyDeviceRule(const FEFModularSettingDeviceRule* Rule, TArray<FString>& OutWarnings) override;

protected:
	float SavedValue = 1.f;

	/** The authored range, kept so a device rule can be undone. */
	bool bAuthoredRangeCaptured = false;
	float AuthoredMin = 0.f;
	float AuthoredMax = 1.f;
	float AuthoredDisplayMin = 0.f;
	float AuthoredDisplayMax = 1.f;
};

/* Multi-select */
UCLASS(Blueprintable,BlueprintType, EditInlineNew)
class UNREALEXTENDEDFRAMEWORK_API UEFModularSettingsMultiSelect : public UEFModularSettingsBase
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_SelectedIndex, Category="Settings Value")
	int32 SelectedIndex = 0;

	UFUNCTION()
	void OnRep_SelectedIndex();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings Value")
	FString DefaultValue = TEXT("");

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Replicated, Category="Settings Value")
	TArray<FString> Values;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Replicated, Category="Settings Value")
	TArray<FText> DisplayNames;

	UFUNCTION(BlueprintCallable,BlueprintNativeEvent, Category = "Settings")
	void SetSelectedIndex(int32 Index);
	virtual void SetSelectedIndex_Implementation(int32 Index) 
	{ 
		// Reject invalid indices for safety (important for dynamic option lists)
		if (!Values.IsValidIndex(Index))
		{
			UE_LOG(LogTemp, Warning, TEXT("[UEFModularSettingsMultiSelect] Loaded index %d for %s is invalid. Resetting to default."), 
				Index, *SettingTag.ToString());
			ResetToDefault();
			return;
		}

		if (SelectedIndex == Index)
		{
			return;
		}

		const FString NewValue = Values[Index];
		if (!IsOptionOffered(NewValue))
		{
			// Hidden on this device (a device rule) or unsupported by this machine. Masked rather than
			// removed from Values, so indices keep their meaning; see IsOptionOffered.
			UE_LOG(LogTemp, Warning, TEXT("[UEFModularSettingsMultiSelect] Rejected option '%s' for %s: not offered on this device"), *NewValue, *SettingTag.ToString());
			return;
		}

		if (IsOptionLocked(NewValue))
		{
			// Direct callers bypass RequestChange, so the lock state may be stale.
			// Refresh once (no-op unless bRefreshBeforeApply) and re-resolve by
			// value, since a refresh can rebuild the option list.
			EnsureFresh();
			Index = Values.Find(NewValue);
			if (Index == INDEX_NONE || IsOptionLocked(NewValue))
			{
				UE_LOG(LogTemp, Warning, TEXT("[UEFModularSettingsMultiSelect] Rejected locked or unavailable option '%s' for %s"), *NewValue, *SettingTag.ToString());
				return;
			}
		}

		if (SelectedIndex != Index)
		{
			SelectedIndex = Index;
			MarkDirty();
		}
	}

	virtual void Apply_Implementation() override { }

	virtual void SaveCurrentValue() override
	{
		// Save both representations:
		// - index for stable/static lists
		// - string for dynamic lists (audio devices etc.)
		SavedValue = SelectedIndex;
		SavedValueString = GetValueAsString();
		Super::SaveCurrentValue();
	}

	virtual void RevertToSavedValue() override
	{
		// Prefer string-based restore so dynamic lists remap correctly after RefreshValues()
		if (!SavedValueString.IsEmpty())
		{
			SetValueFromString(SavedValueString);
		}
		else if (Values.IsValidIndex(SavedValue))
		{
			SetSelectedIndex(SavedValue);
		}

		Apply_Implementation();
		Super::RevertToSavedValue();
	}
	
	virtual FString GetValueAsString() const override
	{
		return Values.IsValidIndex(SelectedIndex) ? Values[SelectedIndex] : TEXT("");
	}
	
	virtual void SetValueFromString(const FString& Value) override
	{
		int32 Index = Values.Find(Value);
		if (Index != INDEX_NONE)
		{
			SetSelectedIndex(Index);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[UEFModularSettingsMultiSelect] Loaded value '%s' for %s is not a valid option. Resetting to default."), 
				*Value, *SettingTag.ToString());
			ResetToDefault();
		}
	}

	virtual FString GetSavedValueAsString() const override
	{
		// Return the exact saved string (not value-at-saved-index) to remain stable across refreshes.
		return SavedValueString;
	}
	
	virtual void ResetToDefault() override
	{
		int32 Index = Values.Find(DefaultValue);
		SetSelectedIndex(Index != INDEX_NONE ? Index : 0);
	}

	virtual FString GetDefaultValueAsString() const override { return DefaultValue; }
	virtual bool SetDefaultValueFromString(const FString& NewDefault) override;
	virtual bool AreValuesEqual(const FString& A, const FString& B) const override { return A.Equals(B, ESearchCase::IgnoreCase); }
	virtual bool CanTakeValueOnThisDevice(const FString& InValue) const override;
	virtual FString AdjustValueToDevice(const FString& InValue) const override;
	virtual bool IsKnownValue(const FString& InValue) const override;

	/** Builds the hidden-option mask from the rule's AllowedOptions and HiddenOptions. */
	virtual void ApplyDeviceRule(const FEFModularSettingDeviceRule* Rule, TArray<FString>& OutWarnings) override;

	/**
	 * Whether this machine can use an option (an upscaler its GPU supports). False hides the option
	 * without any config. Asked again once the PostEngineInit phase completes.
	 */
	virtual bool IsOptionSupported(const FString& OptionValue) const { return true; }

	/**
	 * Whether the option is offered on this device: not hidden by a device rule and supported by the
	 * machine. Separate from locked (a padlock the player can see, such as unowned items): a hidden
	 * option is not shown at all. Hidden options stay in Values, because several settings use the
	 * option index as an engine level and removing one would shift every level above it.
	 */
	UFUNCTION(BlueprintPure, Category = "Settings|Device")
	bool IsOptionOffered(const FString& OptionValue) const;

	UFUNCTION(BlueprintPure, Category = "Settings|Device")
	bool IsIndexOffered(int32 Index) const
	{
		return Values.IsValidIndex(Index) && IsOptionOffered(Values[Index]);
	}

	/** Indices of the offered options, in order. */
	UFUNCTION(BlueprintPure, Category = "Settings|Device")
	TArray<int32> GetOfferedIndices() const;

	/** The offered index nearest to Index (the lower one on a tie); INDEX_NONE when nothing is offered. */
	UFUNCTION(BlueprintPure, Category = "Settings|Device")
	int32 FindNearestOfferedIndex(int32 Index) const;

	/** Options a device rule hides here. */
	const TArray<FString>& GetDeviceHiddenOptions() const { return DeviceHiddenOptions; }

	UFUNCTION(BlueprintCallable, Category = "Settings")
	bool IsOptionLocked(const FString& OptionValue) const
	{
		return LockedOptions.Contains(OptionValue);
	}

	UFUNCTION(BlueprintCallable, Category = "Settings")
	bool IsIndexLocked(int32 Index) const
	{
		return Values.IsValidIndex(Index) ? IsOptionLocked(Values[Index]) : false;
	}

	// Locks/unlocks an option. Safe to call from any machine: when this setting
	// belongs to a player settings component and we are not the authority, the
	// change is applied locally and forwarded to the server, which replicates it
	// to every client.
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SetOptionLocked(const FString& OptionValue, bool bLocked);

	void OnOptionLockChanged();

	const TArray<FString>& GetLockedOptions() const { return LockedOptions; }

	// Replaces the whole lock list. Used when applying replicated definition state
	// on clients; fires OnOptionLockChanged only when the list actually differs.
	void SetLockedOptions(const TArray<FString>& NewLockedOptions)
	{
		if (LockedOptions != NewLockedOptions)
		{
			LockedOptions = NewLockedOptions;
			OnOptionLockChanged();
		}
	}

	// A locked option is held back rather than applied: locks often reflect data this machine has
	// not finished loading (owned items), and they lift again once it has. An unknown option is
	// applied as before, so the setting falls back to its default.
	virtual bool CanApplyLoadedValue(const FString& Value) const override
	{
		return !IsOptionLocked(Value);
	}

	virtual bool Validate_Implementation(const FString& Value) const override
	{
		// Basic existence check
		if (!Values.Contains(Value))
		{
			return false;
		}

		// Security check: is it locked?
		if (IsOptionLocked(Value))
		{
			return false;
		}

		// Hidden on this device, or unsupported by this machine.
		if (!IsOptionOffered(Value))
		{
			return false;
		}

		return true;
	}

protected:
	UPROPERTY(Transient, ReplicatedUsing = OnRep_LockedOptions, BlueprintReadOnly, Category = "Settings")
	TArray<FString> LockedOptions;

	/** Options hidden on this device by its device rule. Local settings only; never replicated. */
	UPROPERTY(Transient)
	TArray<FString> DeviceHiddenOptions;

	UFUNCTION()
	void OnRep_LockedOptions() { OnOptionLockChanged(); }

	int32 SavedValue = 0;
	FString SavedValueString;
};