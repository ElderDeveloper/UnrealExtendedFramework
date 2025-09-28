// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/Engine.h"

#include "EFModularSettingBase.generated.h"

UCLASS(Abstract,BlueprintType, Blueprintable, EditInlineNew)
class UNREALEXTENDEDFRAMEWORK_API UEFModularSettingBase : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, Category="Meta")
	FGameplayTag SettingTag;

	UPROPERTY(EditDefaultsOnly, Category="Meta")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, Category="Meta")
	FName ConfigCategory = TEXT("Settings");

	UPROPERTY(EditDefaultsOnly, Category="Meta")
	FString DefaultValue;
	
	virtual bool CanApply(const FString& Value) const { return true; }
	virtual void Apply() {}

	UFUNCTION(BlueprintCallable, Category = "Settings")
	virtual FString GetValueAsString() const { return TEXT(""); }

	UFUNCTION(BlueprintCallable, Category = "Settings")
	virtual void SetValueFromString(const FString& Value) {}

	UFUNCTION(BlueprintCallable, Category = "Settings")
	virtual void ResetToDefault() {}
};

/* Bool */
UCLASS(Blueprintable,BlueprintType, EditInlineNew)
class UNREALEXTENDEDFRAMEWORK_API UEFModularSettingBool : public UEFModularSettingBase
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bValue = false;

	virtual void Apply() override {}
	
	virtual FString GetValueAsString() const override
	{
		return bValue ? TEXT("true") : TEXT("false");
	}
	
	virtual void SetValueFromString(const FString& Value) override
	{
		bValue = Value.ToBool();
	}
	
	virtual void ResetToDefault() override
	{
		bValue = DefaultValue.ToBool();
	}
};

/* Float */
UCLASS(Blueprintable,BlueprintType, EditInlineNew)
class UNREALEXTENDEDFRAMEWORK_API UEFModularSettingFloat : public UEFModularSettingBase
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Value = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float Min = 0.f, Max = 1.f;

	virtual void Apply() override {}
	
	virtual FString GetValueAsString() const override
	{
		return FString::SanitizeFloat(Value);
	}
	
	virtual void SetValueFromString(const FString& ValueString) override
	{
		Value = FMath::Clamp(FCString::Atof(*ValueString), Min, Max);
	}
	
	virtual void ResetToDefault() override
	{
		Value = FMath::Clamp(FCString::Atof(*DefaultValue), Min, Max);
	}
};

/* Multi-select */
UCLASS(Blueprintable,BlueprintType, EditInlineNew)
class UNREALEXTENDEDFRAMEWORK_API UEFModularSettingMultiSelect : public UEFModularSettingBase
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 SelectedIndex = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TArray<FString> Values;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TArray<FText> DisplayNames;

	virtual void Apply() override {}
	
	virtual FString GetValueAsString() const override
	{
		return Values.IsValidIndex(SelectedIndex) ? Values[SelectedIndex] : TEXT("");
	}
	
	virtual void SetValueFromString(const FString& Value) override
	{
		int32 Index = Values.Find(Value);
		if (Index != INDEX_NONE)
		{
			SelectedIndex = Index;
		}
	}
	
	virtual void ResetToDefault() override
	{
		int32 Index = Values.Find(DefaultValue);
		SelectedIndex = Index != INDEX_NONE ? Index : 0;
	}
};