// Copyright © W2.Wizard 2020 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/ConsoleManager.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EEConsoleLibrary.generated.h"

UENUM()
enum EConsoleVariableType
{
	ConsoleFloat,
	ConsoleInt,
	ConsoleBool,
	ConsoleString,

	MAX		 UMETA(Hidden)
};


DECLARE_DYNAMIC_DELEGATE(FConsoleVarMulticastDelegate);

UCLASS()
class UNREALEXTENDEDEDITOR_API UEEConsoleLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// If the Console manager supports its conversion, it should work with this template.
	template<typename T>
	static void SetVar(const FString& VarName, T Value, const FString& HelpText)
	{
		if (VarName.IsEmpty())
		{
			UE_LOG(LogBlueprintUserMessages, Error, TEXT("Cannot register ConsoleVariable with no Name!!!"));
			return;
		}
		// If variable does not exist register it.
		if (!IConsoleManager::Get().IsNameRegistered(*VarName))
		{
			FConsoleManager::Get().RegisterConsoleVariable(*VarName, Value , *HelpText);
			return;
		}
		// MA
		// Update the value instead of destroying it.
		auto ConsoleVariable = IConsoleManager::Get().FindConsoleVariable(*VarName);
		
		ConsoleVariable->Set(Value);
		ConsoleVariable->SetHelp(*HelpText);
		return;
	}
	
	

	
	
	/**
	 * Deletes a console variable
	 * @param VarName		The name of the variable
	 * @param KeepState		If the current state is kept in memory until a ConsoleVariable with the same name is registered again.
	 */
	UFUNCTION(BlueprintCallable, Category = "Console", meta=(DisplayName="Delete Console Variable (Any)", Keywords ="delete console ConsoleVariable"))
	static void DeleteConsoleVariable(const FString& VarName, bool KeepState);

	UFUNCTION(BlueprintPure, Category = "Console", meta=(DisplayName="Get Variable Help Text", Keywords ="get console help text"))
	static FString GetHelpText(const FString& VarName);
	
	UFUNCTION(BlueprintCallable, Category = "Console", meta=(DisplayName="On Variable Value Change ", Keywords ="value variable change delegate"))
	static void OnVarChangeDelegate(const FString& VarName, FConsoleVarMulticastDelegate OnChange);

	UFUNCTION(BlueprintPure, Category = "Console", meta=(DisplayName="Is Variable Type", Keywords ="get console help text"))
	static bool IsVarType(const FString& VarName, TEnumAsByte<EConsoleVariableType> Type);
	
	/*//////////// Get Functions ////////////*/

	/** Gets the specified float console variable */
	UFUNCTION(BlueprintPure, Category = "Console|Get", meta=(DisplayName="Get Console Variable (Float)", Keywords = "get add float ConsoleVariable"))
	static float GetFloatConsoleVariable(const FString& VarName);

	/** Gets the specified int32 console variable */
	UFUNCTION(BlueprintPure, Category = "Console|Get", meta=(DisplayName="Get Console Variable (Int)", Keywords = "get add int ConsoleVariable"))
    static int32 GetInt32ConsoleVariable(const FString& VarName);

	/** Gets the specified bool console variable */
	UFUNCTION(BlueprintPure, Category = "Console|Get", meta=(DisplayName="Get Console Variable (Bool)", Keywords = "get add bool ConsoleVariable"))
    static bool GetBoolConsoleVariable(const FString& VarName);

	/** Gets the specified string console variable */
	UFUNCTION(BlueprintPure, Category = "Console|Get", meta=(DisplayName="Get Console Variable (String)", Keywords = "get add string ConsoleVariable"))
    static FString GetStringConsoleVariable(const FString& VarName);
	
	/*//////////// Set Functions ////////////*/

	/** Sets the specified float console variable, if it does not exists, it will be created then. */
	UFUNCTION(BlueprintCallable, Category = "Console|Set", meta=(DisplayName="Set Console Variable (Float)", Keywords = "set add float ConsoleVariable"))
	static void SetFloatConsoleVariable(const FString& VarName, float Value, const FString& HelpText);

	/** Sets the specified int32 console variable, if it does not exists, it will be created then. */
	UFUNCTION(BlueprintCallable, Category = "Console|Set", meta=(DisplayName="Set Console Variable (Int)", Keywords = "set add int ConsoleVariable"))
    static void SetInt32ConsoleVariable(const FString& VarName, int32 Value, const FString& HelpText);

	/** Sets the specified bool console variable, if it does not exists, it will be created then. */
	UFUNCTION(BlueprintCallable, Category = "Console|Set", meta=(DisplayName="Set Console Variable (Bool)", Keywords = "set add bool ConsoleVariable"))
    static void SetBoolConsoleVariable(const FString& VarName, bool Value, const FString& HelpText);

	/** Sets the specified string console variable, if it does not exists, it will be created then. */
	UFUNCTION(BlueprintCallable, Category = "Console|Set", meta=(DisplayName="Set Console Variable (String)", Keywords = "set add string ConsoleVariable"))
    static void SetStringConsoleVariable(const FString& VarName, const FString& Value, const FString& HelpText);
};
