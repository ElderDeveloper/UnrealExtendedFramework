// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Blueprint/ESQLBlueprintLibrary.h"
#include "CoreMinimal.h"
#include "K2Node_CallFunction.h"
#include "Subsystem/ESQLSubsystem.h"
#include "Subsystems/SubsystemBlueprintLibrary.h"
#include "UObject/UObjectGlobals.h"

namespace ESQLRuntimeReflectionUtils
{
	inline UClass* LoadNativeClass(const TCHAR* ClassPath)
	{
		return LoadObject<UClass>(nullptr, ClassPath);
	}

	inline UClass* GetAsyncActionClass(const TCHAR* ClassName)
	{
		static TMap<FString, TWeakObjectPtr<UClass>> CachedClasses;
		const FString Key(ClassName);
		if (const TWeakObjectPtr<UClass>* CachedClass = CachedClasses.Find(Key))
		{
			if (CachedClass->IsValid())
			{
				return CachedClass->Get();
			}
		}

		const FString ClassPath = FString::Printf(TEXT("/Script/UnrealExtendedSQL.%s"), ClassName);
		UClass* LoadedClass = LoadNativeClass(*ClassPath);
		CachedClasses.FindOrAdd(Key) = LoadedClass;
		return LoadedClass;
	}

	inline bool BindSQLSubsystemFunction(UK2Node_CallFunction& FunctionNode, const FName FunctionName)
	{
		FunctionNode.FunctionReference.SetExternalMember(FunctionName, UESQLSubsystem::StaticClass());
		return true;
	}

	inline bool BindSQLBlueprintLibraryFunction(UK2Node_CallFunction& FunctionNode, const FName FunctionName)
	{
		FunctionNode.FunctionReference.SetExternalMember(FunctionName, UESQLBlueprintLibrary::StaticClass());
		return true;
	}

	inline bool BindGameInstanceSubsystemGetter(UK2Node_CallFunction& FunctionNode)
	{
		FunctionNode.FunctionReference.SetExternalMember(
			GET_FUNCTION_NAME_CHECKED(USubsystemBlueprintLibrary, GetGameInstanceSubsystem),
			USubsystemBlueprintLibrary::StaticClass());
		return true;
	}
}