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
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly, Category="Settings")
	FGameplayTag SettingTag;

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly, Category="Settings")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly, Category="Settings")
	FName ConfigCategory = TEXT("Settings");

	bool Validate(const FString& Value) const;
	virtual bool Validate_Implementation(const FString& Value) const { return true; }

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

	// Reference to the subsystem managing this setting
	UPROPERTY(Transient,BlueprintReadOnly)
	TObjectPtr<UEFModularSettingsSubsystem> ModularSettingsSubsystem;
	
	virtual UWorld* GetWorld() const override;

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
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings Value")
	bool Value = false;

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
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings Value")
	float Value = 1.f;

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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings Value")
	int32 SelectedIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings Value")
	FString DefaultValue = TEXT("");

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Settings Value")
	TArray<FString> Values;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Settings Value")
	TArray<FText> DisplayNames;

	UFUNCTION(BlueprintCallable,BlueprintNativeEvent, Category = "Settings")
	void SetSelectedIndex(int32 Index);
	virtual void SetSelectedIndex_Implementation(int32 Index) 
	{ 
		if (SelectedIndex != Index)
		{
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

protected:
	int32 SavedValue = 0;
};