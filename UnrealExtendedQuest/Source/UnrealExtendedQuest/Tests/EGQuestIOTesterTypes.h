// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Misc/DateTime.h"
#include "Engine/Texture2D.h"
#include "GameplayTagContainer.h"

#include "UnrealExtendedQuest/EGQuestHelper.h"
#include "UnrealExtendedQuest/EGQuestTextArgumentCustom.h"
#include "UnrealExtendedQuest/EGQuestTypes.h"

#include "EGQuestIOTesterTypes.generated.h"

class UEGQuestContext;
class UEGQuestComponent;

// Custom text argument that resolves from a per-context table the test seeds. Stands in for a game
// that formats objective text from its own per-player state.
UCLASS()
class UEGQuestTestContextTextArgument : public UEGQuestTextArgumentCustom
{
	GENERATED_BODY()
public:
	// Raw keys: the test owns every context it registers and outlives them.
	static TMap<const UEGQuestContext*, FText> ContextValues;

	FText GetText_Implementation(const UEGQuestContext* Context, const FString& DisplayStringParam) override
	{
		const FText* Found = ContextValues.Find(Context);
		return Found ? *Found : FText::GetEmpty();
	}
};

// Dynamic-delegate sink for UEGQuestComponent::OnQuestGameplayNotify automation coverage.
UCLASS()
class UEGQuestTestGameplayNotifySink : public UObject
{
	GENERATED_BODY()
public:
	UFUNCTION()
	void HandleNotify(FGuid QuestInstanceGuid, FGameplayTag NotifyTag, float Magnitude)
	{
		ReceivedInstances.Add(QuestInstanceGuid);
		ReceivedTags.Add(NotifyTag);
		ReceivedMagnitudes.Add(Magnitude);
	}

	TArray<FGuid> ReceivedInstances;
	TArray<FGameplayTag> ReceivedTags;
	TArray<float> ReceivedMagnitudes;
};

/** Calls back into the component from OnGameplayEventAccepted to prove mutation re-entry is queued. */
UCLASS()
class UEGQuestTestReentrySink : public UObject
{
	GENERATED_BODY()
public:
	void Configure(UEGQuestComponent* InComponent, FGuid InRunId, FGuid InObjectiveGuid);

	UFUNCTION()
	void HandleAcceptedEvent(const FEGQuestGameplayEvent& Event);

	TObjectPtr<UEGQuestComponent> Component;
	FGuid RunId;
	FGuid ObjectiveGuid;
	EEGQuestOperationStatus CallbackStatus = EEGQuestOperationStatus::Rejected;
	int32 ProjectionRevisionSeenByCallback = INDEX_NONE;
};

UCLASS()
class UEGQuestTestPlatformSink : public UObject
{
	GENERATED_BODY()
public:
	UFUNCTION() void HandleDirective(FGuid RunId, const FEGQuestDirective& Directive, EEGQuestDirectivePhase Phase)
	{
		DirectiveRuns.Add(RunId); Directives.Add(Directive); DirectivePhases.Add(Phase);
	}
	UFUNCTION() void HandleTelemetry(EEGQuestTelemetryEventType Type, FName DefinitionId, FGuid RunId,
		FGuid ElementGuid, double ServerTime)
	{
		TelemetryTypes.Add(Type); TelemetryRuns.Add(RunId);
	}
	UFUNCTION() void HandleRoleLost(FGuid RunId, FName RoleName, FEGQuestEntityHandle Handle)
	{
		LostRoles.Add(RoleName); LostHandles.Add(Handle);
	}
	TArray<FGuid> DirectiveRuns;
	TArray<FEGQuestDirective> Directives;
	TArray<EEGQuestDirectivePhase> DirectivePhases;
	TArray<EEGQuestTelemetryEventType> TelemetryTypes;
	TArray<FGuid> TelemetryRuns;
	TArray<FName> LostRoles;
	TArray<FEGQuestEntityHandle> LostHandles;
};


USTRUCT()
struct FEGQuestIOTesterOptions
{
	GENERATED_USTRUCT_BODY()

public:
	FEGQuestIOTesterOptions() {}

	// can Have TArray<Enum>, TSet<Enum>
	UPROPERTY()
	bool bSupportsPureEnumContainer = true;

	// Can have TSet<FStructType>
	UPROPERTY()
	bool bSupportsNonPrimitiveInSet = true;

	// Can we write FLinearColor and FColor
	UPROPERTY()
	bool bSupportsColorPrimitives = true;

	// Can we write FDateTime
	UPROPERTY()
	bool bSupportsDatePrimitive = true;

	// Can we Have TMap<Key, UObject*> ?
	UPROPERTY()
	bool bSupportsUObjectValueInMap = true;

public:
	bool operator==(const FEGQuestIOTesterOptions& Other) const
	{
		return bSupportsPureEnumContainer == Other.bSupportsPureEnumContainer &&
			bSupportsNonPrimitiveInSet == Other.bSupportsNonPrimitiveInSet &&
			bSupportsColorPrimitives == Other.bSupportsColorPrimitives;
	}
	bool operator!=(const FEGQuestIOTesterOptions& Other) const { return !(*this == Other); }

	FString ToString() const
	{
		return FString::Printf(TEXT("bSupportsPureEnumContainer=%d, bSupportsNonPrimitiveInSet=%d, bSupportsColorPrimitives=%d"),
			bSupportsPureEnumContainer, bSupportsNonPrimitiveInSet, bSupportsColorPrimitives);
	}

};


UENUM()
enum class EEGQuestTestEnum : uint8
{
	First = 0,
	Second,
	Third,
	NumOf
};

UCLASS()
class UEGQuestTestObjectPrimitivesBase : public UObject
{
	GENERATED_BODY()
	typedef UEGQuestTestObjectPrimitivesBase Self;
public:
	UEGQuestTestObjectPrimitivesBase() { SetToDefaults(); }
	virtual void GenerateRandomData(const FEGQuestIOTesterOptions& InOptions);
	virtual void SetToDefaults();
	virtual bool IsEqual(const Self* Other, FString& OutError) const;
	bool operator==(const Self& Other) const
	{
		FString DiscardError;
		return IsEqual(&Other, DiscardError);
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("Integer=%d, String=%s"), Integer, *String);
	}

public:
	// Tester Options
	FEGQuestIOTesterOptions Options;

	UPROPERTY()
	int32 Integer;

	UPROPERTY()
	FString String;
};

UCLASS(DefaultToInstanced)
class UEGQuestTestObjectPrimitives_DefaultToInstanced : public UEGQuestTestObjectPrimitivesBase
{
	GENERATED_BODY()
	typedef UEGQuestTestObjectPrimitives_DefaultToInstanced Self;
public:
	UEGQuestTestObjectPrimitives_DefaultToInstanced() { SetToDefaults(); }
	void GenerateRandomData(const FEGQuestIOTesterOptions& InOptions) override;
	void SetToDefaults() override;
	bool IsEqual(const Super* Other, FString& OutError) const override;
	bool operator==(const Self& Other) const
	{
		FString DiscardError;
		return IsEqual(&Other, DiscardError);
	}

public:
	// Tester Options
	FEGQuestIOTesterOptions Options;

	UPROPERTY()
	int32 InstancedChild;
};

UCLASS()
class UEGQuestTestObjectPrimitives_ChildA : public UEGQuestTestObjectPrimitivesBase
{
	GENERATED_BODY()
	typedef UEGQuestTestObjectPrimitives_ChildA Self;
public:
	UEGQuestTestObjectPrimitives_ChildA() { SetToDefaults(); }
	void GenerateRandomData(const FEGQuestIOTesterOptions& InOptions) override;
	void SetToDefaults() override;
	bool IsEqual(const Super* Other, FString& OutError) const override;
	bool operator==(const Self& Other) const
	{
		FString DiscardError;
		return IsEqual(&Other, DiscardError);
	}

public:
	// Tester Options
	FEGQuestIOTesterOptions Options;

	UPROPERTY()
	int32 IntegerChildA;
};

UCLASS()
class UEGQuestTestObjectPrimitives_ChildB : public UEGQuestTestObjectPrimitivesBase
{
	GENERATED_BODY()
	typedef UEGQuestTestObjectPrimitives_ChildB Self;
public:
	UEGQuestTestObjectPrimitives_ChildB() { SetToDefaults(); }
	void GenerateRandomData(const FEGQuestIOTesterOptions& InOptions) override;
	void SetToDefaults() override;
	bool IsEqual(const Super* Other, FString& OutError) const override;
	bool operator==(const Self& Other) const
	{
		FString DiscardError;
		return IsEqual(&Other, DiscardError);
	}

public:
	// Tester Options
	FEGQuestIOTesterOptions Options;

	UPROPERTY()
	FString StringChildB;
};

UCLASS()
class UEGQuestTestObjectPrimitives_GrandChildA_Of_ChildA : public UEGQuestTestObjectPrimitives_ChildA
{
	GENERATED_BODY()
	typedef UEGQuestTestObjectPrimitives_GrandChildA_Of_ChildA Self;
	typedef UEGQuestTestObjectPrimitivesBase SuperBase;
public:
	UEGQuestTestObjectPrimitives_GrandChildA_Of_ChildA() { SetToDefaults(); }
	void GenerateRandomData(const FEGQuestIOTesterOptions& InOptions) override;
	void SetToDefaults() override;
	bool IsEqual(const SuperBase* Other, FString& OutError) const override;
	bool operator==(const Self& Other) const
	{
		FString DiscardError;
		return IsEqual(&Other, DiscardError);
	}

public:
	// Tester Options
	FEGQuestIOTesterOptions Options;

	UPROPERTY()
	int32 IntegerGrandChildA_Of_ChildA;
};

// Struct of primitives
USTRUCT()
struct UNREALEXTENDEDQUEST_API FEGQuestTestStructPrimitives
{
	GENERATED_USTRUCT_BODY()
	typedef FEGQuestTestStructPrimitives Self;
public:
	FEGQuestTestStructPrimitives() { SetToDefaults(); }
	bool IsEqual(const Self& Other, FString& OutError) const;
	bool operator==(const Self& Other) const
	{
		FString DiscardError;
		return IsEqual(Other, DiscardError);
	}
	bool operator!=(const Self& Other) const { return !(*this == Other); }
	friend uint32 GetTypeHash(const Self& This)
	{
		// NOTE not floats in the hash, these should be enough
		uint32 KeyHash = GetTypeHash(This.Integer32);
		KeyHash = HashCombine(KeyHash, GetTypeHash(This.Integer64));
		KeyHash = HashCombine(KeyHash, GetTypeHash(This.String));
		KeyHash = HashCombine(KeyHash, GetTypeHash(This.Name));
		KeyHash = HashCombine(KeyHash, GetTypeHash(This.bBoolean));
		KeyHash = HashCombine(KeyHash, GetTypeHash(This.Enum));
		KeyHash = HashCombine(KeyHash, GetTypeHash(This.Color));
		KeyHash = HashCombine(KeyHash, GetTypeHash(This.DateTime));
		KeyHash = HashCombine(KeyHash, GetTypeHash(This.IntPoint));
		KeyHash = HashCombine(KeyHash, GetTypeHash(This.GUID));
		KeyHash = HashCombine(KeyHash, GetTypeHash(This.Texture2DReference));
		return KeyHash;
	}
	void GenerateRandomData(const FEGQuestIOTesterOptions& InOptions);
	void SetToDefaults();

	FString ToString() const
	{
		return FString::Printf(TEXT("bBoolean=%d, Integer32=%d, Integer64=%lld, Float=%f, Enum=%d, Name=%s, String=%s, Text=%s, Color=%s, LinearColor=%s, DateTime=%s"),
			bBoolean, Integer32, Integer64, Float, static_cast<int32>(Enum), *Name.ToString(), *String, *Text.ToString(), *Color.ToString(), *LinearColor.ToString(), *DateTime.ToString());
	}

public:
	// Tester Options
	FEGQuestIOTesterOptions Options;

	UPROPERTY()
	int32 Integer32;

	UPROPERTY()
	int64 Integer64;

	UPROPERTY()
	bool bBoolean;

	UPROPERTY()
	EEGQuestTestEnum Enum;

	UPROPERTY()
	float Float;

	UPROPERTY()
	FName Name;

	UPROPERTY()
	FString String;

	UPROPERTY()
	FString EmptyString;

	UPROPERTY()
	FText Text;

	UPROPERTY()
	FColor Color;

	UPROPERTY()
	FLinearColor LinearColor;

	UPROPERTY()
	FDateTime DateTime;

	UPROPERTY()
	FIntPoint IntPoint;

	UPROPERTY()
	FVector Vector3;

	UPROPERTY()
	FVector2D Vector2;

	UPROPERTY()
	FVector4 Vector4;

	UPROPERTY()
	FRotator Rotator;

	UPROPERTY()
	FMatrix Matrix;

	UPROPERTY()
	FTransform Transform;

	UPROPERTY()
	FGuid GUID;

	UPROPERTY()
	UClass* Class;

	UPROPERTY()
	UObject* EmptyObjectInitialized = nullptr;

	UPROPERTY(meta = (QuestSaveOnlyReference))
	UObject* EmptyObjectInitializedReference = nullptr;

	// Not initialized, check if any writer crashes. It does sadly. Can't know in C++ if a variable is initialized
	//UPROPERTY()
	//UObject* EmptyObject;

	// Check if anything crashes
	UPROPERTY()
	UTexture2D* ConstTexture2D;

	UPROPERTY(meta=(QuestSaveOnlyReference))
	UTexture2D* Texture2DReference;

	UPROPERTY()
	UEGQuestTestObjectPrimitivesBase* ObjectPrimitivesBase;

	UPROPERTY()
	UEGQuestTestObjectPrimitives_DefaultToInstanced* ObjectDefaultToInstanced;

	UPROPERTY()
	UEGQuestTestObjectPrimitives_ChildA* ObjectPrimitivesChildA;

	// Can be nullptr or not
	UPROPERTY()
	UEGQuestTestObjectPrimitivesBase* ObjectSwitch;

	// Object is defined as base but actually assigned to Child A
	UPROPERTY()
	UEGQuestTestObjectPrimitivesBase* ObjectPrimitivesPolymorphismChildA;

	UPROPERTY()
	UEGQuestTestObjectPrimitivesBase* ObjectPrimitivesPolymorphismChildB;

	UPROPERTY()
	UEGQuestTestObjectPrimitives_GrandChildA_Of_ChildA* ObjectPrimitivesGrandChildA;

	UPROPERTY()
	UEGQuestTestObjectPrimitivesBase* ObjectPrimitivesPolymorphismBaseGrandChildA;

	UPROPERTY()
	UEGQuestTestObjectPrimitives_ChildA* ObjectPrimitivesPolymorphismChildGrandChildA;
};


// Struct of Complex types
USTRUCT()
struct UNREALEXTENDEDQUEST_API FEGQuestTestStructComplex
{
	GENERATED_USTRUCT_BODY()
	typedef FEGQuestTestStructComplex Self;
public:
	FEGQuestTestStructComplex() { SetToDefaults(); }
	bool IsEqual(const Self& Other, FString& OutError) const;
	bool operator==(const Self& Other) const
	{
		FString DiscardError;
		return IsEqual(Other, DiscardError);
	}
	bool operator!=(const Self& Other) const { return !(*this == Other); }
	void GenerateRandomData(const FEGQuestIOTesterOptions& InOptions);
	void SetToDefaults();

	FString ToString() const
	{
		return FString();
	}

public:
	// Tester Options
	FEGQuestIOTesterOptions Options;

	UPROPERTY()
	TArray<FEGQuestTestStructPrimitives> StructArrayPrimitives;

	UPROPERTY()
	TArray<UEGQuestTestObjectPrimitivesBase*> ArrayOfObjects;

	UPROPERTY(meta = (QuestSaveOnlyReference))
	TArray<UEGQuestTestObjectPrimitivesBase*> ArrayOfObjectsAsReference;
};


// Arrays simple
USTRUCT()
struct UNREALEXTENDEDQUEST_API FEGQuestTestArrayPrimitive
{
	GENERATED_USTRUCT_BODY()
	typedef FEGQuestTestArrayPrimitive Self;
public:
	FEGQuestTestArrayPrimitive() {}
	bool IsEqual(const Self& Other, FString& OutError) const;
	bool operator==(const Self& Other) const
	{
		FString DiscardError;
		return IsEqual(Other, DiscardError);
	}
	void GenerateRandomData(const FEGQuestIOTesterOptions& InOptions);

public:
	// Tester Options
	FEGQuestIOTesterOptions Options;

	UPROPERTY()
	TArray<int32> EmptyArray;

	UPROPERTY()
	TArray<int32> Num1_Array;

	UPROPERTY()
	TArray<int32> Int32Array;

	UPROPERTY()
	TArray<int64> Int64Array;

	UPROPERTY()
	TArray<bool> BoolArray;

	UPROPERTY()
	TArray<float> FloatArray;

	UPROPERTY()
	TArray<EEGQuestTestEnum> EnumArray;

	UPROPERTY()
	TArray<FName> NameArray;

	UPROPERTY()
	TArray<FString> StringArray;

	// Filled with only nulls, check if the writers support it
	UPROPERTY()
	TArray<UEGQuestTestObjectPrimitivesBase*> ObjectArrayConstantNulls;
};

// Array complex
USTRUCT()
struct UNREALEXTENDEDQUEST_API FEGQuestTestArrayComplex
{
	GENERATED_USTRUCT_BODY()
	typedef FEGQuestTestArrayComplex Self;
public:
	FEGQuestTestArrayComplex() {}
	bool IsEqual(const Self& Other, FString& OutError) const;
	void GenerateRandomData(const FEGQuestIOTesterOptions& InOptions);
	bool operator==(const Self& Other) const
	{
		FString DiscardError;
		return IsEqual(Other, DiscardError);
	}

public:
	// Tester Options
	FEGQuestIOTesterOptions Options;

	UPROPERTY()
	TArray<FEGQuestTestStructPrimitives> StructArrayPrimitives;

	UPROPERTY()
	TArray<FEGQuestTestArrayPrimitive> StructArrayOfArrayPrimitives;

	UPROPERTY()
	TArray<UEGQuestTestObjectPrimitivesBase*> ObjectArrayFrequentsNulls;

	UPROPERTY()
	TArray<UEGQuestTestObjectPrimitivesBase*> ObjectArrayPrimitivesBase;

	UPROPERTY()
	TArray<UEGQuestTestObjectPrimitivesBase*> ObjectArrayPrimitivesAll;
};


// Set primitive
USTRUCT()
struct UNREALEXTENDEDQUEST_API FEGQuestTestSetPrimitive
{
	GENERATED_USTRUCT_BODY()
	typedef FEGQuestTestSetPrimitive Self;
public:
	FEGQuestTestSetPrimitive() {}
	bool IsEqual(const Self& Other, FString& OutError) const;
	void GenerateRandomData(const FEGQuestIOTesterOptions& InOptions);
	bool operator==(const Self& Other) const
	{
		FString DiscardError;
		return IsEqual(Other, DiscardError);
	}

public:
	// Tester Options
	FEGQuestIOTesterOptions Options;

	UPROPERTY()
	TSet<int32> EmptySet;

	UPROPERTY()
	TSet<int32> Num1_Set;

	UPROPERTY()
	TSet<int32> Int32Set;

	UPROPERTY()
	TSet<int64> Int64Set;

	UPROPERTY()
	TSet<EEGQuestTestEnum> EnumSet;

	UPROPERTY()
	TSet<FName> NameSet;

	UPROPERTY()
	TSet<FString> StringSet;
};

// Set complex
USTRUCT()
struct UNREALEXTENDEDQUEST_API FEGQuestTestSetComplex
{
	GENERATED_USTRUCT_BODY()
	typedef FEGQuestTestSetComplex Self;
public:
	FEGQuestTestSetComplex() {}
	bool IsEqual(const Self& Other, FString& OutError) const;
	void GenerateRandomData(const FEGQuestIOTesterOptions& InOptions);
	bool operator==(const Self& Other) const
	{
		FString DiscardError;
		return IsEqual(Other, DiscardError);
	}

public:
	// Tester Options
	FEGQuestIOTesterOptions Options;

	UPROPERTY()
	TSet<FEGQuestTestStructPrimitives> StructSetPrimitives;
};


// Map primitive
USTRUCT()
struct UNREALEXTENDEDQUEST_API FEGQuestTestMapPrimitive
{
	GENERATED_USTRUCT_BODY()
	typedef FEGQuestTestMapPrimitive Self;
public:
	FEGQuestTestMapPrimitive() {}
	bool IsEqual(const Self& Other, FString& OutError) const;
	void GenerateRandomData(const FEGQuestIOTesterOptions& InOptions);
	void CheckInvariants() const;
	bool operator==(const Self& Other) const
	{
		FString DiscardError;
		return IsEqual(Other, DiscardError);
	}
	FString ToString() const
	{
		return FString::Printf(TEXT("IntToIntMap.Num()=%d, IntToStringMap.Num()=%d"),  Int32ToInt32Map.Num(), Int32ToStringMap.Num());
	}

public:
	// Tester Options
	FEGQuestIOTesterOptions Options;

	UPROPERTY()
	TMap<int32, int32> EmptyMap;

	UPROPERTY()
	TMap<int32, int32> Int32ToInt32Map;

	UPROPERTY()
	TMap<int64, int64> Int64ToInt64Map;

	UPROPERTY()
	TMap<int32, FString> Int32ToStringMap;

	UPROPERTY()
	TMap<int32, FName> Int32ToNameMap;

	UPROPERTY()
	TMap<FString, int32> StringToInt32Map;

	UPROPERTY()
	TMap<FString, FString> StringToStringMap;

	UPROPERTY()
	TMap<FName, int32> NameToInt32Map;

	UPROPERTY()
	TMap<FName, FName> NameToNameMap;

	UPROPERTY()
	TMap<FString, float> StringToFloatMap;

	UPROPERTY()
	TMap<int32, float> Int32ToFloatMap;

	UPROPERTY()
	TMap<FName, FColor> NameToColorMap;

	UPROPERTY()
	TMap<FString, UEGQuestTestObjectPrimitivesBase*> ObjectFrequentsNullsMap;

	// Filled with only nulls, check if the writers support it
	UPROPERTY()
	TMap<FString, UEGQuestTestObjectPrimitivesBase*> ObjectConstantNullMap;

	UPROPERTY()
	TMap<FString, UEGQuestTestObjectPrimitivesBase*> ObjectPrimitivesAllMap;
};

// Map complex
USTRUCT()
struct UNREALEXTENDEDQUEST_API FEGQuestTestMapComplex
{
	GENERATED_USTRUCT_BODY()
	typedef FEGQuestTestMapComplex Self;
public:
	FEGQuestTestMapComplex() {}
	bool IsEqual(const Self& Other, FString& OutError) const;
	void GenerateRandomData(const FEGQuestIOTesterOptions& InOptions);
	bool operator==(const Self& Other) const
	{
		FString DiscardError;
		return IsEqual(Other, DiscardError);
	}

public:
	// Tester Options
	FEGQuestIOTesterOptions Options;

	UPROPERTY()
	TMap<int32, FEGQuestTestStructPrimitives> Int32ToStructPrimitiveMap;

	UPROPERTY()
	TMap<FName, FEGQuestTestStructPrimitives> NameToStructPrimitiveMap;

	UPROPERTY()
	TMap<FEGQuestTestStructPrimitives, int32> StructPrimitiveToIntMap;

	UPROPERTY()
	TMap<FName, FEGQuestTestMapPrimitive> NameToStructOfMapPrimitives;

	UPROPERTY()
	TMap<FName, FEGQuestTestArrayPrimitive> NameToStructOfArrayPrimitives;

	UPROPERTY()
	TMap<FName, FEGQuestTestSetPrimitive> NameToStructOfSetPrimitives;

	UPROPERTY()
	TMap<FName, FEGQuestTestArrayComplex> NameToStructOfArrayComplex;

	UPROPERTY()
	TMap<FName, FEGQuestTestSetComplex> NameToStructOfSetComplex;
};
