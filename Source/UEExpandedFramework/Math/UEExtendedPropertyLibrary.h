// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UEExtendedPropertyLibrary.generated.h"

/**
 * 
 */

// Declare General Log Category, header file .h
DECLARE_LOG_CATEGORY_EXTERN(LogParserPropertyLibrary, Log, All);



UCLASS()
class UEEXPANDEDFRAMEWORK_API UUEExtendedPropertyLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:



	UFUNCTION(BlueprintCallable, CustomThunk, Category = "Utilities|Variables", meta = (CustomStructureParam = "Value", AutoCreateRefTerm = "Value", DisplayName = "GET Property" , CompactNodeTitle = "GET"))
	static void GetPropertyByName(UObject* Object, FName PropertyName, const int32& Value);
	static void Generic_GetPropertyByName(UObject* OwnerObject, void* SrcPropertyAddr, FProperty* SrcProperty,FName PropertyName);
	DECLARE_FUNCTION(execGetPropertyByName)
	{
		P_GET_OBJECT(UObject, OwnerObject);
		P_GET_PROPERTY(UNameProperty, PropertyName);

		Stack.StepCompiledIn<UStructProperty>(NULL);
		if (const auto SrcPropertyAddr =  Stack.MostRecentPropertyAddress)
		{
			if (const auto SrcProperty = CastField<UProperty>(Stack.MostRecentProperty))
			{
				P_FINISH;
				P_NATIVE_BEGIN;
				Generic_GetPropertyByName(OwnerObject, SrcPropertyAddr, SrcProperty, PropertyName);
				P_NATIVE_END;
			}
		}
	}

	/**
	* Get or Set object PROPERTY value by property name
	*
	* @param  Object, object that owns this PROPERTY
	* @param  PropertyName, property name
	* @param  Value(return), save returned object property(Get Operation) as well as indicate property type
	* @param  bSetter, If true, write Value to object property(Set operation). Otherwise, read object property and assign it to Value(Get operation)
	*/
	
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "Utilities|Variables", meta = (CustomStructureParam = "Value", AutoCreateRefTerm = "Value", DisplayName = "GET/SET (Property)", CompactNodeTitle = "GET/SET"))
	static void AccessPropertyByName(UObject* Object, FName PropertyName, const int32& Value, bool bSetter = true);
	static void Generic_AccessPropertyByName(UObject* OwnerObject, FName PropertyName, void* SrcPropertyAddr, UProperty* SrcProperty, bool bSetter = true);
	DECLARE_FUNCTION(execAccessPropertyByName)
	{
		P_GET_OBJECT(UObject, OwnerObject);
		P_GET_PROPERTY(UNameProperty, PropertyName);

		Stack.StepCompiledIn<UStructProperty>(NULL);
		void* SrcPropertyAddr = Stack.MostRecentPropertyAddress;
		/// Reference: Plugins\Experimental\StructBox\Source\StructBox\Classes\StructBoxLibrary.h -> execSetStructInBox
		UProperty* SrcProperty = CastField<UProperty>(Stack.MostRecentProperty);

		P_GET_UBOOL(bSetter);
		P_FINISH;

		P_NATIVE_BEGIN;
		Generic_AccessPropertyByName(OwnerObject, PropertyName, SrcPropertyAddr, SrcProperty, bSetter);
		P_NATIVE_END;
	}
	

	
	/**
	 * Get or Set object ARRAY value by property name
	 *
	 * @param  Object, object that owns this ARRAY
	 * @param  PropertyName, array property name
	 * @param  Value, save returned object array value(Get Operation) as well as indicate array type
	 * @param  bSetter, If true, write Value to object array(Set operation). Otherwise, read object array and assign it to Value(Get operation)
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "Utilities|Array", meta = (ArrayParm = "Value", ArrayTypeDependentParams = "Value", DisplayName = "GET/SET (array)", CompactNodeTitle = "GET/SET"))
	static void AccessArrayByName(UObject* Object, FName PropertyName, const TArray<int32>& Value, bool bSetter = true);
	static void Generic_AccessArrayByName(UObject* OwnerObject, FName ArrayPropertyName, void* SrcArrayAddr, const UArrayProperty* SrcArrayProperty, bool bSetter = true);
	DECLARE_FUNCTION(execAccessArrayByName)
	{
		P_GET_OBJECT(UObject, OwnerObject);
		P_GET_PROPERTY(UNameProperty, ArrayPropertyName);

		Stack.StepCompiledIn<UArrayProperty>(NULL);
		void* SrcArrayAddr = Stack.MostRecentPropertyAddress;
		UArrayProperty* SrcArrayProperty = CastField<UArrayProperty>(Stack.MostRecentProperty);

		P_GET_UBOOL(bSetter);
		P_FINISH;

		P_NATIVE_BEGIN;
		Generic_AccessArrayByName(OwnerObject, ArrayPropertyName, SrcArrayAddr, SrcArrayProperty, bSetter);
		P_NATIVE_END;
	}

	
	/**
	 * Get or Set object SET value by property name
	 *
	 * @param  Object, object that owns this SET
	 * @param  PropertyName, set property name
	 * @param  Value(return), save returned set value(Get Operation) as well as indicate set type
	 * @param  bSetter, If true, write Value to object set(Set operation). Otherwise, read object set and assign it to Value(Get operation)
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "Utilities|Set", meta = (SetParam = "Value", DisplayName = "GET/SET (set)", CompactNodeTitle = "GET/SET"))
	static void AccessSetByName(UObject* Object, FName PropertyName, const TSet<int32>& Value, bool bSetter = true);
	static void Generic_AccessSetByName(UObject* OwnerObject, FName SetPropertyName, void* SrcSetAddr, const USetProperty* SrcSetProperty, bool bSetter = true);
	DECLARE_FUNCTION(execAccessSetByName)
	{
		P_GET_OBJECT(UObject, OwnerObject);
		P_GET_PROPERTY(UNameProperty, SetPropertyName);

		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<USetProperty>(NULL);
		void* SetAddr = Stack.MostRecentPropertyAddress;
		USetProperty* SetProperty = CastField<USetProperty>(Stack.MostRecentProperty);
		if (!SetProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		P_GET_UBOOL(bSetter);
		P_FINISH;

		P_NATIVE_BEGIN;
		Generic_AccessSetByName(OwnerObject, SetPropertyName, SetAddr, SetProperty, bSetter);
		P_NATIVE_END;
	}
	
	/**
	 * Get or Set object MAP value by property name
	 *
	 * @param  Object, object that owns this MAP
	 * @param  PropertyName, map property name
	 * @param  Value(return), save returned map value(Get Operation) as well as indicate map type
	 * @param  bSetter, If true, write Value to object map(Set operation). Otherwise, read object map and assign it to Value(Get operation)
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "Utilities|Map", meta = (MapParam = "Value", DisplayName = "GET/SET (map)", CompactNodeTitle = "GET/SET"))
	static void AccessMapByName(UObject* Object, FName PropertyName, const TMap<int32, int32>& Value, bool bSetter = true);
	static void Generic_AccessMapByName(UObject* OwnerObject, FName MapPropertyName, void* SrcMapAddr, const UMapProperty* SrcMapProperty, bool bSetter = true);
	DECLARE_FUNCTION(execAccessMapByName)
	{
		P_GET_OBJECT(UObject, OwnerObject);
		P_GET_PROPERTY(UNameProperty, MapPropertyName);

		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<UMapProperty>(NULL);
		void* SrcMapAddr = Stack.MostRecentPropertyAddress;
		UMapProperty* SrcMapProperty = CastField<UMapProperty>(Stack.MostRecentProperty);
		if (!SrcMapProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		P_GET_UBOOL(bSetter);
		P_FINISH;

		P_NATIVE_BEGIN;
		Generic_AccessMapByName(OwnerObject, MapPropertyName, SrcMapAddr, SrcMapProperty, bSetter);
		P_NATIVE_END;
	}


	/*
	UFUNCTION(BlueprintCallable , meta = (BlueprintInternalUseOnly = "true") , Category="Variable|Access")
	static void SetIntPropertyByName(UObject* Object , FName PropertyName , int32 Value)	{	AccessPropertyByName(Object,PropertyName,Value,true);	}
	//DECLARE_FUNCTION(execSetIntPropertyByName) {	}
	
	UFUNCTION(BlueprintCallable , meta = (BlueprintInternalUseOnly = "true") , Category="Variable|Access")
	static void SetInt64PropertyByName(UObject* Object , FName PropertyName , int64 Value)	{	AccessPropertyByName(Object,PropertyName,Value,true);	}
	//DECLARE_FUNCTION(execSetInt64PropertyByName) {	}

	UFUNCTION(BlueprintCallable , meta = (BlueprintInternalUseOnly = "true") , Category="Variable|Access")
	static void SetBytePropertyByName(UObject* Object , FName PropertyName , uint8 Value)	{	AccessPropertyByName(Object,PropertyName,Value,true);	}
	//DECLARE_FUNCTION(execSetBytePropertyByName) {	}
	
	UFUNCTION(BlueprintCallable , meta = (BlueprintInternalUseOnly = "true") , Category="Variable|Access")
	static void SetFloatPropertyByName(UObject* Object , FName PropertyName , float Value)	{	AccessPropertyByName(Object,PropertyName,Value,true);	}
	//DECLARE_FUNCTION(execSetFloatPropertyByName) {	}
	
	UFUNCTION(BlueprintCallable , meta = (BlueprintInternalUseOnly = "true") , Category="Variable|Access")
	static void SetBoolPropertyByName(UObject* Object , FName PropertyName , bool Value)	{	AccessPropertyByName(Object,PropertyName,Value,true);	}
	//DECLARE_FUNCTION(execSetBoolPropertyByName) {	}

	UFUNCTION(BlueprintCallable , meta = (BlueprintInternalUseOnly = "true") , Category="Variable|Access")
	static void SetObjectPropertyByName(UObject* Object , FName PropertyName , UObject* Value)	{	AccessPropertyByName(Object,PropertyName,Value,true);	}
	DECLARE_FUNCTION(execSetObjectPropertyByName) {	}

	UFUNCTION(BlueprintCallable , meta = (BlueprintInternalUseOnly = "true") , Category="Variable|Access")
	static void SetClassPropertyByName(UObject* Object , FName PropertyName , UClass* Value)	{	AccessPropertyByName(Object,PropertyName,Value,true);	}
	DECLARE_FUNCTION(execSetClassPropertyByName) {	}

	UFUNCTION(BlueprintCallable , meta = (BlueprintInternalUseOnly = "true") , Category="Variable|Access")
	static void SetInterfacePropertyByName(UObject* Object , FName PropertyName , UInterface* Value)	{	AccessPropertyByName(Object,PropertyName,Value,true);	}
	DECLARE_FUNCTION(execSetInterfacePropertyByName) {	}

	UFUNCTION(BlueprintCallable , meta = (BlueprintInternalUseOnly = "true") , Category="Variable|Access")
	static void SetNamePropertyByName(UObject* Object , FName PropertyName , FName Value)	{	AccessPropertyByName(Object,PropertyName,Value,true);	}
	DECLARE_FUNCTION(execSetNamePropertyByName) {	}

	UFUNCTION(BlueprintCallable , meta = (BlueprintInternalUseOnly = "true") , Category="Variable|Access")
	static void SetStringPropertyByName(UObject* Object , FName PropertyName , FString Value)	{	AccessPropertyByName(Object,PropertyName,Value,true);	}
	DECLARE_FUNCTION(execSetStringPropertyByName) {	}

	/*
	UFUNCTION(BlueprintCallable , meta = (BlueprintInternalUseOnly = "true") , Category="Variable|Access")
	static void SetSoftObjectPropertyByName(UObject* Object , FName PropertyName , int32 Value);
	DECLARE_FUNCTION(execSetSoftObjectPropertyByName) {	}

	UFUNCTION(BlueprintCallable , meta = (BlueprintInternalUseOnly = "true") , Category="Variable|Access")
	static void SetSoftClassPropertyByName(UObject* Object , FName PropertyName , int32 Value);
	DECLARE_FUNCTION(execSetSoftClassPropertyByName) {	}
	*/
	



	
	/*
	UFUNCTION(BlueprintCallable , meta = (BlueprintInternalUseOnly = "true" , CompactNodeTitle = "Get Int") , Category="Variable|Access")
	static void GetIntPropertyByName(UObject* Object , FName PropertyName , int32& Value)	{	AccessPropertyByName(Object,PropertyName,Value,false);	}
	//DECLARE_FUNCTION(execGetIntPropertyByName) {	}
	
	UFUNCTION(BlueprintCallable , meta = (BlueprintInternalUseOnly = "true" , CompactNodeTitle = "Get Int64") , Category="Variable|Access")
	static void GetInt64PropertyByName(UObject* Object , FName PropertyName , int64& Value)	{	AccessPropertyByName(Object,PropertyName,Value,false);	}
	//DECLARE_FUNCTION(execGetInt64PropertyByName) {	}
    	
	UFUNCTION(BlueprintCallable , meta = (BlueprintInternalUseOnly = "true" , CompactNodeTitle = "Get Byte") , Category="Variable|Access")
	static void GetBytePropertyByName(UObject* Object , FName PropertyName , uint8& Value)	{	AccessPropertyByName(Object,PropertyName,Value,false);	}
	//DECLARE_FUNCTION(execGetBytePropertyByName) {	}

	UFUNCTION(BlueprintCallable , meta = (BlueprintInternalUseOnly = "true" , CompactNodeTitle = "Get Float") , Category="Variable|Access")
	static void GetFloatPropertyByName(UObject* Object , FName PropertyName , float& Value)	{	AccessPropertyByName(Object,PropertyName,Value,false);	}
	//DECLARE_FUNCTION(execGetFloatPropertyByName) {	}
	
	UFUNCTION(BlueprintCallable , meta = (BlueprintInternalUseOnly = "true" , CompactNodeTitle = "Get Bool") , Category="Variable|Access")
	static bool GetBoolPropertyByName(UObject* Object , FName PropertyName) {  bool a = false ; AccessPropertyByName(Object,PropertyName,a,false); return a; }
	//DECLARE_FUNCTION(execGetBoolPropertyByName) {	}
	*/

	/*
	UFUNCTION(BlueprintCallable , meta = (BlueprintInternalUseOnly = "true") , Category="Variable|Access")
	static void GetObjectPropertyByName(UObject* Object , FName PropertyName , UObject*& Value);
	DECLARE_FUNCTION(execGetObjectPropertyByName) {	}

	UFUNCTION(BlueprintCallable , meta = (BlueprintInternalUseOnly = "true") , Category="Variable|Access")
	static void GetClassPropertyByName(UObject* Object , FName PropertyName , UClass*& Value);
	DECLARE_FUNCTION(execGetClassPropertyByName) {	}

	UFUNCTION(BlueprintCallable , meta = (BlueprintInternalUseOnly = "true") , Category="Variable|Access")
	static void GetInterfacePropertyByName(UObject* Object , FName PropertyName , UInterface*& Value);
	DECLARE_FUNCTION(execGetInterfacePropertyByName) {	}

	UFUNCTION(BlueprintCallable , meta = (BlueprintInternalUseOnly = "true") , Category="Variable|Access")
	static void GetNamePropertyByName(UObject* Object , FName PropertyName , FName& Value);
	DECLARE_FUNCTION(execGetNamePropertyByName) {	}
	
	UFUNCTION(BlueprintCallable , meta = (BlueprintInternalUseOnly = "true") , Category="Variable|Access")
	static void GetStringPropertyByName(UObject* Object , FName PropertyName , FString& Value );
	DECLARE_FUNCTION(execGetStringPropertyByName) {	}
	*/
	/*
	UFUNCTION(BlueprintCallable , meta = (BlueprintInternalUseOnly = "true") , Category="Variable|Access")
	static void GetSoftObjectPropertyByName(UObject* Object , FName PropertyName , TWeakObjectPtr<UObject>& Value);
	DECLARE_FUNCTION(execGetSoftObjectPropertyByName) {	}

	UFUNCTION(BlueprintCallable , meta = (BlueprintInternalUseOnly = "true") , Category="Variable|Access")
	static void GetSoftClassPropertyByName(UObject* Object , FName PropertyName , int32& Value);
	DECLARE_FUNCTION(execGetSoftClassPropertyByName) {	}
	*/
};
