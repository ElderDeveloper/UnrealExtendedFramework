// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "EFGlobalLibrary.generated.h"


static TMap<FGameplayTag , TSubclassOf<UObject>> EFGlobalClass;
static TMap<FGameplayTag , bool> EFGlobalBool;
static TMap<FGameplayTag , uint8 > EFGlobalByte;
static TMap<FGameplayTag , float> EFGlobalFloat;
static TMap<FGameplayTag , int32> EFGlobalInt;
static TMap<FGameplayTag , int64> EFGlobalInt64;
static TMap<FGameplayTag , FName> EFGlobalName;
static TMap<FGameplayTag , FString> EFGlobalString;
static TMap<FGameplayTag , FText> EFGlobalText;
static TMap<FGameplayTag , FVector> EFGlobalVector;
static TMap<FGameplayTag , FRotator> EFGlobalRotator;
static TMap<FGameplayTag , FTransform> EFGlobalTransform;

	




UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFGlobalLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure , Category="Global Library|Get")
	static TSubclassOf<UObject> GetGlobalClass(FGameplayTag Tag)
	{
		return ReturnGlobalVariable<FGameplayTag , TSubclassOf<UObject>>(EFGlobalClass , Tag,nullptr);
	}
	
	
	UFUNCTION(BlueprintPure , Category="Global Library|Get")
	static bool GetGlobalBool(FGameplayTag Tag)
	{
		return ReturnGlobalVariable<FGameplayTag , bool>(EFGlobalBool , Tag,false);
	}

	
	UFUNCTION(BlueprintPure , Category="Global Library|Get")
	static uint8 GetGlobalByte(FGameplayTag Tag)
	{
		return ReturnGlobalVariable<FGameplayTag , uint8>(EFGlobalByte , Tag,0);
	}

	
	UFUNCTION(BlueprintPure , Category="Global Library|Get")
	static float GetGlobalFloat(FGameplayTag Tag)
	{
		return ReturnGlobalVariable<FGameplayTag , float>(EFGlobalFloat , Tag,0);
	}

	UFUNCTION(BlueprintPure  , Category="Global Library|Get")
	static int32 GetGlobalInt(FGameplayTag Tag)
	{
		return ReturnGlobalVariable<FGameplayTag , int32>(EFGlobalInt , Tag,0);
	}

	
	UFUNCTION(BlueprintPure  , Category="Global Library|Get")
	static int64 GetGlobalInt64(FGameplayTag Tag)
	{
		return ReturnGlobalVariable<FGameplayTag , int64>(EFGlobalInt64 , Tag,0);
	}
	
	
	UFUNCTION(BlueprintPure  , Category="Global Library|Get")
	static FName GetGlobalName(FGameplayTag Tag)
	{
		return ReturnGlobalVariable<FGameplayTag , FName>(EFGlobalName , Tag,FName());
	}
	

	UFUNCTION(BlueprintPure , Category="Global Library|Get")
	static FString GetGlobalString(FGameplayTag Tag)
	{
		return ReturnGlobalVariable<FGameplayTag , FString>(EFGlobalString , Tag,FString());
	}

	
	UFUNCTION(BlueprintPure , Category="Global Library|Get")
	static FText GetGlobalText(FGameplayTag Tag)
	{
		return ReturnGlobalVariable<FGameplayTag , FText>(EFGlobalText , Tag,FText());
	}


	UFUNCTION(BlueprintPure  , Category="Global Library|Get")
	static FVector GetGlobalVector(FGameplayTag Tag)
	{
		return ReturnGlobalVariable<FGameplayTag , FVector>(EFGlobalVector , Tag,FVector::ZeroVector);
	}

	
	UFUNCTION(BlueprintPure  , Category="Global Library|Get")
	static FRotator GetGlobalRotator(FGameplayTag Tag)
	{
		return ReturnGlobalVariable<FGameplayTag , FRotator>(EFGlobalRotator , Tag,FRotator::ZeroRotator);
	}

	
	UFUNCTION(BlueprintPure , Category="Global Library|Get")
	static FTransform GetGlobalTransform(FGameplayTag Tag)
	{
		return ReturnGlobalVariable<FGameplayTag , FTransform>(EFGlobalTransform , Tag,FTransform());
	}


	

	UFUNCTION(BlueprintCallable , Category="Global Library|Set")
	static TSubclassOf<UObject> SetGlobalClass(FGameplayTag Tag , TSubclassOf<UObject> Value)
	{
		return CreateNewGlobalVariable<FGameplayTag , TSubclassOf<UObject>>(EFGlobalClass , Tag,Value);
	}


	UFUNCTION(BlueprintCallable , Category="Global Library|Set")
	static bool SetGlobalBool(FGameplayTag Tag , bool Value)
	{
		return CreateNewGlobalVariable<FGameplayTag , bool>(EFGlobalBool , Tag,Value);
	}

	
	UFUNCTION(BlueprintCallable , Category="Global Library|Set")
	static uint8 SetGlobalByte(FGameplayTag Tag , uint8 Value)
	{
		return CreateNewGlobalVariable<FGameplayTag , uint8>(EFGlobalByte , Tag,Value);
	}

	
	UFUNCTION(BlueprintCallable , Category="Global Library|Set")
	static float SetGlobalFloat(FGameplayTag Tag , float Value)
	{
		return CreateNewGlobalVariable<FGameplayTag , float>(EFGlobalFloat , Tag,Value);
	}

	UFUNCTION(BlueprintCallable , Category="Global Library|Set")
	static int32 SetGlobalInt(FGameplayTag Tag , int32 Value)
	{
		return CreateNewGlobalVariable<FGameplayTag , int32>(EFGlobalInt , Tag,Value);
	}

	
	UFUNCTION(BlueprintCallable , Category="Global Library|Set")
	static int64 SetGlobalInt64(FGameplayTag Tag , int64 Value)
	{
		return CreateNewGlobalVariable<FGameplayTag , int64>(EFGlobalInt64 , Tag,Value);
	}
	
	
	UFUNCTION(BlueprintCallable , Category="Global Library|Set")
	static FName SetGlobalName(FGameplayTag Tag , FName Value)
	{
		return CreateNewGlobalVariable<FGameplayTag , FName>(EFGlobalName , Tag,Value);
	}
	

	UFUNCTION(BlueprintCallable , Category="Global Library|Set")
	static FString SetGlobalString(FGameplayTag Tag , FString Value)
	{
		return CreateNewGlobalVariable<FGameplayTag , FString>(EFGlobalString , Tag,Value);
	}

	
	UFUNCTION(BlueprintCallable , Category="Global Library|Set")
	static FText SetGlobalText(FGameplayTag Tag , FText Value)
	{
		return CreateNewGlobalVariable<FGameplayTag , FText>(EFGlobalText , Tag,Value);
	}


	UFUNCTION(BlueprintCallable , Category="Global Library|Set")
	static FVector SetGlobalVector(FGameplayTag Tag , FVector Value)
	{
		return CreateNewGlobalVariable<FGameplayTag , FVector>(EFGlobalVector , Tag,Value);
	}

	
	UFUNCTION(BlueprintCallable , Category="Global Library|Set")
	static FRotator SetGlobalRotator(FGameplayTag Tag , FRotator Value)
	{
		return CreateNewGlobalVariable<FGameplayTag , FRotator>(EFGlobalRotator , Tag,Value);
	}

	
	UFUNCTION(BlueprintCallable , Category="Global Library|Set")
	static FTransform SetGlobalTransform(FGameplayTag Tag , FTransform Value)
	{
		return CreateNewGlobalVariable<FGameplayTag , FTransform>(EFGlobalTransform , Tag,Value);
	}

	

	UFUNCTION(BlueprintCallable , Category="Global Library|All")
	static void GetAllGlobalClass(TArray<FGameplayTag>& Tags , TArray<TSubclassOf<UObject>>& Values)
	{
		Tags = ReturnGlobalVariableArrayKeys<TSubclassOf<UObject>>(EFGlobalClass);
		Values = ReturnGlobalVariableArrayValues<TSubclassOf<UObject>>(EFGlobalClass);
	}
	
	
	UFUNCTION(BlueprintCallable , Category="Global Library|All")
	static void GetAllGlobalBool(TArray<FGameplayTag>& Tags , TArray<bool>& Values)
	{
		Tags = ReturnGlobalVariableArrayKeys<bool>(EFGlobalBool);
		Values = ReturnGlobalVariableArrayValues<bool>(EFGlobalBool);
	}

	UFUNCTION(BlueprintCallable , Category="Global Library|All")
	static void GetAllGlobalByte(TArray<FGameplayTag>& Tags , TArray<uint8>& Values)
	{
		Tags = ReturnGlobalVariableArrayKeys<uint8>(EFGlobalByte);
		Values = ReturnGlobalVariableArrayValues<uint8>(EFGlobalByte);
	}

	UFUNCTION(BlueprintCallable , Category="Global Library|All")
	static void GetAllGlobalFloat(TArray<FGameplayTag>& Tags , TArray<float>& Values)
	{
		Tags = ReturnGlobalVariableArrayKeys<float>(EFGlobalFloat);
		Values = ReturnGlobalVariableArrayValues<float>(EFGlobalFloat);
	}

	UFUNCTION(BlueprintCallable , Category="Global Library|All")
	static void GetAllGlobalInt(TArray<FGameplayTag>& Tags , TArray<int32>& Values)
	{
		Tags = ReturnGlobalVariableArrayKeys<int32>(EFGlobalInt);
		Values = ReturnGlobalVariableArrayValues<int32>(EFGlobalInt);
	}

	UFUNCTION(BlueprintCallable , Category="Global Library|All")
	static void GetAllGlobalInt64(TArray<FGameplayTag>& Tags , TArray<int64>& Values)
	{
		Tags = ReturnGlobalVariableArrayKeys<int64>(EFGlobalInt64);
		Values = ReturnGlobalVariableArrayValues<int64>(EFGlobalInt64);
	}

	UFUNCTION(BlueprintCallable , Category="Global Library|All")
	static void GetAllGlobalName(TArray<FGameplayTag>& Tags , TArray<FName>& Values)
	{
		Tags = ReturnGlobalVariableArrayKeys<FName>(EFGlobalName);
		Values = ReturnGlobalVariableArrayValues<FName>(EFGlobalName);
	}

	UFUNCTION(BlueprintCallable , Category="Global Library|All")
	static void GetAllGlobalString(TArray<FGameplayTag>& Tags , TArray<FString>& Values)
	{
		Tags = ReturnGlobalVariableArrayKeys<FString>(EFGlobalString);
		Values = ReturnGlobalVariableArrayValues<FString>(EFGlobalString);
	}

	UFUNCTION(BlueprintCallable , Category="Global Library|All")
	static void GetAllGlobalText(TArray<FGameplayTag>& Tags , TArray<FText>& Values)
	{
		Tags = ReturnGlobalVariableArrayKeys<FText>(EFGlobalText);
		Values = ReturnGlobalVariableArrayValues<FText>(EFGlobalText);
	}

	UFUNCTION(BlueprintCallable , Category="Global Library|All")
	static void GetAllGlobalVector(TArray<FGameplayTag>& Tags , TArray<FVector>& Values)
	{
		Tags = ReturnGlobalVariableArrayKeys<FVector>(EFGlobalVector);
		Values = ReturnGlobalVariableArrayValues<FVector>(EFGlobalVector);
	}

	UFUNCTION(BlueprintCallable , Category="Global Library|All")
	static void GetAllGlobalRotator(TArray<FGameplayTag>& Tags , TArray<FRotator>& Values)
	{
		Tags = ReturnGlobalVariableArrayKeys<FRotator>(EFGlobalRotator);
		Values = ReturnGlobalVariableArrayValues<FRotator>(EFGlobalRotator);
	}

	UFUNCTION(BlueprintCallable , Category="Global Library|All")
	static void GetAllGlobalTransform(TArray<FGameplayTag>& Tags , TArray<FTransform>& Values)
	{
		Tags = ReturnGlobalVariableArrayKeys<FTransform>(EFGlobalTransform);
		Values = ReturnGlobalVariableArrayValues<FTransform>(EFGlobalTransform);
	}






	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject" , ExpandBoolAsExecs = "Valid"))
	static AActor* GetGlobalActor(const UObject* WorldContextObject , FGameplayTag Tag , bool& Valid);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void SetGlobalActor(const UObject* WorldContextObject , FGameplayTag Tag , AActor* Value);
	

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject" , ExpandBoolAsExecs = "Valid"))
	static UObject* GetGlobalObject(const UObject* WorldContextObject , FGameplayTag Tag , bool& Valid);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void SetGlobalObject(const UObject* WorldContextObject , FGameplayTag Tag , UObject* Value);


private:
	
	template< typename  T , typename V >
	static V CreateNewGlobalVariable(TMap<FGameplayTag, V >& Type ,T Tag , V Value)
	{
		Type.Add(Tag ,Value);
		return Type[Tag];
	}

	template< typename  T , typename V >
	static V ReturnGlobalVariable(TMap<FGameplayTag, V >& Type ,T Tag , V CustomValue)
	{
		if (Type.Contains(Tag))
		{
			return Type[Tag];
		}
		Type.Add(Tag , CustomValue);
		return Type[Tag];
	}

	template<typename T >
	static TArray<T> ReturnGlobalVariableArrayValues(const TMap<FGameplayTag,T>& Type)
	{
		TArray<T> Array;
		for (const auto i : Type)
			Array .Add(i.Value);
		return Array;
	}

	template<typename T >
	static TArray<FGameplayTag> ReturnGlobalVariableArrayKeys(const TMap<FGameplayTag,T>& Type)
		{
			TArray<FGameplayTag> Array;
			for (const auto i : Type)
				Array .Add(i.Key);
			return Array;
		}
	
};
