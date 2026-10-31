// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/EFEnums.h"
#include "Json/EFJsonTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EFLegacyJsonLibrary.generated.h"

/**
 * The Backend Json nodes from before the JSON library moved to ExtendedFoundation.
 *
 * Old Blueprints are redirected here (UEBJsonLibrary -> UEFLegacyJsonLibrary) so they keep loading
 * and running. Every node is deprecated; use UEFJsonLibrary for new work. These live in the core
 * rather than the foundation because they take the core's EFProjectDirectory enum.
 */
UCLASS(meta = (ScriptName = "EFLegacyJsonLibrary"))
class UNREALEXTENDEDFRAMEWORK_API UEFLegacyJsonLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Backend|Json|Deprecated", meta = (ExpandBoolAsExecs = "Success", DeprecatedFunction, DeprecationMessage = "Use Load Json Object From File."))
	static FEFJsonObject ReadJsonFile(EFProjectDirectory DirectoryType, FString FileDirectory, bool& Success, bool DebugDirectory = false);

	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Backend|Json|Deprecated", meta = (ExpandBoolAsExecs = "Success", DeprecatedFunction, DeprecationMessage = "Use Read Text from the Extended File library, or Load Json Object From File."))
	static FString ReadExtendedJsonAsString(EFProjectDirectory DirectoryType, FString FileDirectory, bool& Success);

	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Backend|Json|Deprecated", meta = (ExpandBoolAsExecs = "Success", DeprecatedFunction, DeprecationMessage = "Use Try Get String Field."))
	static void GetExtendedStringField(bool& Success, const FEFJsonObject& ExtendedJson, const FString& FieldName, FString& OutString);

	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Backend|Json|Deprecated", meta = (ExpandBoolAsExecs = "Success", DeprecatedFunction, DeprecationMessage = "Use Try Get Bool Field."))
	static void GetExtendedBoolField(bool& Success, const FEFJsonObject& ExtendedJson, const FString& FieldName, bool& OutBool);

	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Backend|Json|Deprecated", meta = (ExpandBoolAsExecs = "Success", DeprecatedFunction, DeprecationMessage = "Use Try Get Integer Field."))
	static void GetExtendedNumberField(bool& Success, const FEFJsonObject& ExtendedJson, const FString& FieldName, int32& OutNumber);

	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Backend|Json|Deprecated", meta = (ExpandBoolAsExecs = "Success", DeprecatedFunction, DeprecationMessage = "Use Try Get Number Field."))
	static void GetExtendedFloatField(bool& Success, const FEFJsonObject& ExtendedJson, const FString& FieldName, float& OutFloat);

	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Backend|Json|Deprecated", meta = (ExpandBoolAsExecs = "Success", DeprecatedFunction, DeprecationMessage = "Use Try Get Object Field."))
	static void GetExtendedObjectField(bool& Success, const FEFJsonObject& ExtendedJson, const FString& FieldName, FEFJsonObject& OutObjectField);

	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Backend|Json|Deprecated", meta = (ExpandBoolAsExecs = "Success", DeprecatedFunction, DeprecationMessage = "Use Try Get Object Array Field."))
	static void GetExtendedArrayField(bool& Success, const FEFJsonObject& ExtendedJson, const FString& FieldName, TArray<FEFJsonObject>& OutObjectField);

	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Backend|Json|Deprecated", meta = (ExpandBoolAsExecs = "Success", DeprecatedFunction, DeprecationMessage = "Use Get Field Names."))
	static void GetExtendedJsonValueKeys(bool& Success, const FEFJsonObject& ExtendedJson, TArray<FString>& GeneratedKeyArray);
};
