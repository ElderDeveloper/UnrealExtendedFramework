// Copyright © W2.Wizard 2020 All Rights Reserved.

#include "EEConsoleLibrary.h"

/*/////////// Helper Functions ///////////*/

#define ConsoleManager IConsoleManager::Get()

void UEEConsoleLibrary::DeleteConsoleVariable(const FString& VarName, bool KeepState)
{
	if (ConsoleManager.IsNameRegistered(*VarName))
	{
		// Unregister it fully.
		ConsoleManager.UnregisterConsoleObject(*VarName, KeepState);
	}
}

FString UEEConsoleLibrary::GetHelpText(const FString& VarName)
{
	if (ConsoleManager.IsNameRegistered(*VarName))
		return FString(ConsoleManager.FindConsoleVariable(*VarName)->GetHelp());
	return "";
}

void UEEConsoleLibrary::OnVarChangeDelegate(const FString& VarName, FConsoleVarMulticastDelegate OnChange)
{
	if (ConsoleManager.IsNameRegistered(*VarName))
	{
		ConsoleManager.FindConsoleVariable(*VarName)->SetOnChangedCallback(FConsoleVariableDelegate::CreateLambda([OnChange](IConsoleVariable* Var)
		{
			OnChange.ExecuteIfBound();
		}));
	}
}

bool UEEConsoleLibrary::IsVarType(const FString& VarName, TEnumAsByte<EConsoleVariableType> Type)
{
	auto ConsoleVariable = ConsoleManager.FindConsoleVariable(*VarName);
	
	switch (Type)
	{
		case ConsoleFloat:
			return ConsoleVariable->IsVariableFloat();

		case ConsoleBool:
			return ConsoleVariable->IsVariableBool();

		case ConsoleString:
			return ConsoleVariable->IsVariableString();

		case ConsoleInt:
			return ConsoleVariable->IsVariableInt();

		default:
			return false;
	}
}


float UEEConsoleLibrary::GetFloatConsoleVariable(const FString& VarName)
{
	IConsoleVariable* Var = ConsoleManager.FindConsoleVariable(*VarName);
	
	if (Var && Var->IsVariableFloat()) return Var->GetFloat();

	UE_LOG(LogBlueprintUserMessages, Warning, TEXT("Failed to retrieve correct format for ConsoleVariable: '%s' | Expected float!"), *VarName);
	return 0;
}

int32 UEEConsoleLibrary::GetInt32ConsoleVariable(const FString& VarName)
{
	IConsoleVariable* Var = ConsoleManager.FindConsoleVariable(*VarName);
	
	if (Var && Var->IsVariableInt()) return Var->GetInt();

	UE_LOG(LogBlueprintUserMessages, Warning, TEXT("Failed to retrieve correct format for ConsoleVariable: '%s' | Expected int!"), *VarName);
	return 0;
}

bool UEEConsoleLibrary::GetBoolConsoleVariable(const FString& VarName)
{
	IConsoleVariable* Var = ConsoleManager.FindConsoleVariable(*VarName);
	
	if (Var && Var->IsVariableBool()) return Var->GetBool();

	UE_LOG(LogBlueprintUserMessages, Warning, TEXT("Failed to retrieve correct format for ConsoleVariable: '%s' | Expected bool!"), *VarName);
	return 0;
}

FString UEEConsoleLibrary::GetStringConsoleVariable(const FString& VarName)
{
	IConsoleVariable* Var = ConsoleManager.FindConsoleVariable(*VarName);
	
	if (Var && Var->IsVariableString()) return Var->GetString();

	UE_LOG(LogBlueprintUserMessages, Warning, TEXT("Failed to retrieve correct format for ConsoleVariable: '%s' | Expected string!"), *VarName);
	return "";
}



void UEEConsoleLibrary::SetFloatConsoleVariable(const FString& VarName, float Value, const FString& HelpText)
{
	SetVar(VarName, Value, HelpText);
}

void UEEConsoleLibrary::SetInt32ConsoleVariable(const FString& VarName, int32 Value, const FString& HelpText)
{
	SetVar(VarName, Value, HelpText);
}

void UEEConsoleLibrary::SetBoolConsoleVariable(const FString& VarName, bool Value, const FString& HelpText)
{
	SetVar(VarName, Value, HelpText);
}

void UEEConsoleLibrary::SetStringConsoleVariable(const FString& VarName, const FString& Value, const FString& HelpText)
{
	SetVar(VarName, *Value, HelpText);
}
