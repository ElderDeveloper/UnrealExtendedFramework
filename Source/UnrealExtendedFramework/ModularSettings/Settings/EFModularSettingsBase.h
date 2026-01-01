// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/Engine.h"
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
	
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Settings")
	bool Validate(const FString& Value) const;
	virtual bool Validate_Implementation(const FString& Value) const { return true; }

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Settings")
	void RefreshValues();
	virtual void RefreshValues_Implementation() {}

	UFUNCTION(BlueprintCallable,BlueprintNativeEvent, Category = "Settings")
	void Apply();
	virtual void Apply_Implementation() {}

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

	// Called to sync setting value from engine state (e.g., GameUserSettings)
	// Override this for settings that use GameUserSettings APIs
	virtual void SyncFromEngine() {}

	// Reference to the subsystem managing this setting
	UPROPERTY(Transient,BlueprintReadOnly)
	TObjectPtr<UEFModularSettingsSubsystem> ModularSettingsSubsystem;
	
	virtual UWorld* GetWorld() const override;

	void NotifyValueChanged();

protected:

	FString PreviousValue;
	
	UPROPERTY(Transient)
	bool bIsDirty = false;
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

	UFUNCTION(BlueprintCallable,BlueprintNativeEvent, Category = "Settings")
	void SetValue(float NewValue);
	virtual void SetValue_Implementation(float NewValue) 
	{ 
		float ClampedValue = FMath::Clamp(NewValue, Min, Max);
		if (!FMath::IsNearlyEqual(Value, ClampedValue))
		{
			Value = ClampedValue;
			MarkDirty();
		}
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

protected:
	float SavedValue = 1.f;
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
		if (SelectedIndex != Index)
		{
			// Optional: Check if the NEW index is valid and not locked?
			if (Values.IsValidIndex(Index))
			{
				FString NewValue = Values[Index];
				if (IsOptionLocked(NewValue))
				{
					// Rejected because it's locked
					UE_LOG(LogTemp, Warning, TEXT("[UEFModularSettingsMultiSelect] Attempted to select locked option: %s"), *NewValue);
					return;
				}
			}

			SelectedIndex = Index;
			MarkDirty();
		}
	}

	virtual void Apply_Implementation() override { }
	virtual void SaveCurrentValue() override { SavedValue = SelectedIndex; Super::SaveCurrentValue(); }
	virtual void RevertToSavedValue() override { SelectedIndex = SavedValue; Apply_Implementation(); Super::RevertToSavedValue(); }
	
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
	}

	virtual FString GetSavedValueAsString() const override
	{
		return Values.IsValidIndex(SavedValue) ? Values[SavedValue] : TEXT("");
	}
	
	virtual void ResetToDefault() override
	{
		int32 Index = Values.Find(DefaultValue);
		SetSelectedIndex(Index != INDEX_NONE ? Index : 0);
	}

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

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SetOptionLocked(const FString& OptionValue, bool bLocked)
	{
		if (bLocked)
		{
			LockedOptions.AddUnique(OptionValue);
		}
		else
		{
			LockedOptions.Remove(OptionValue);
		}
		
		OnOptionLockChanged();
	}

	void OnOptionLockChanged();

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

		return true;
	}

protected:
	UPROPERTY(Transient, ReplicatedUsing = OnRep_LockedOptions, BlueprintReadOnly, Category = "Settings")
	TArray<FString> LockedOptions;

	UFUNCTION()
	void OnRep_LockedOptions() { OnOptionLockChanged(); }

	int32 SavedValue = 0;
};