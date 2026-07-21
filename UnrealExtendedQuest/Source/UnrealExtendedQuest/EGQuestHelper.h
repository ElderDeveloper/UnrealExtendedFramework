// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Math/UnrealMathUtility.h"
#include "UObject/Object.h"
#include "UObject/UnrealType.h"
#include "UObject/ObjectMacros.h"
#include <functional>

#include "EGQuestEventCustom.h"
#include "NYEngineVersionHelpers.h"

class SDockTab;
class FTabManager;
struct FTabId;
class UEGQuestPluginSettings;

// Const version of FScriptArrayHelper
class FEGQuestConstScriptArrayHelper : public FScriptArrayHelper
{
	typedef FScriptArrayHelper Super;
	typedef FEGQuestConstScriptArrayHelper Self;
public:
	FORCEINLINE FEGQuestConstScriptArrayHelper(const FArrayProperty* InProperty, const void *InArray)
		: Super(InProperty, InArray) {}

	FORCEINLINE const uint8* GetConstRawPtr(int32 Index = 0) const
	{
		return const_cast<Self*>(this)->GetRawPtr(Index);
	}
};


// Const version of FScriptMapHelper
class FEGQuestConstScriptMapHelper : public FScriptMapHelper
{
	typedef FScriptMapHelper Super;
	typedef FEGQuestConstScriptMapHelper Self;
public:

	FORCEINLINE FEGQuestConstScriptMapHelper(const FMapProperty* InProperty, const void* InMap)
		: Super(InProperty, InMap) {}


	FORCEINLINE const uint8* GetConstKeyPtr(int32 Index) const
	{
		return const_cast<Self*>(this)->GetKeyPtr(Index);
	}

	FORCEINLINE const uint8* GetConstValuePtr(int32 Index) const
	{
		return const_cast<Self*>(this)->GetValuePtr(Index);
	}
};


/**
 * Classes created because Function templates cannot be partially specialised. so we use a delegate class trick
 * https://stackoverflow.com/questions/16154480/getting-illegal-use-of-explicit-template-arguments-when-doing-a-pointer-partia
 */
template <typename KeyType, typename ValueType>
class FEGQuestHelper_MapEqualImpl
{
public:
	static bool IsEqual(
		const TMap<KeyType, ValueType>& FirstMap,
		const TMap<KeyType, ValueType>& SecondMap,
		std::function<bool(const ValueType& FirstMapValue,
		const ValueType& SecondMapValue)> AreValuesEqual
	)
	{
		if (FirstMap.Num() == SecondMap.Num())
		{
			for (const auto& ElemFirstMap : FirstMap)
			{
				const auto* FoundValueSecondMap = SecondMap.Find(ElemFirstMap.Key);
				if (FoundValueSecondMap != nullptr)
				{
					// Key exists in second map
					if (!AreValuesEqual(ElemFirstMap.Value, *FoundValueSecondMap))
					{
						// Value differs
						return false;
					}
				}
				else
				{
					// Key does not even exist
					return false;
				}
			}

			return true;
		}

		// Length differs
		return false;
	}
};

// Variant with default comparison
template <typename KeyType, typename ValueType>
class FEGQuestHelper_MapEqualVariantImpl
{
public:
	static bool IsEqual(const TMap<KeyType, ValueType>& FirstMap, const TMap<KeyType, ValueType>& SecondMap)
	{
		return FEGQuestHelper_MapEqualImpl<KeyType, ValueType>::IsEqual(FirstMap, SecondMap,
			[](const ValueType& FirstMapValue, const ValueType& SecondMapValue) -> bool
		{
			return FirstMapValue == SecondMapValue;
		});
	}
};

// Variant with Specialization for float ValueType
template <typename KeyType>
class FEGQuestHelper_MapEqualVariantImpl<KeyType, float>
{
public:
	static bool IsEqual(const TMap<KeyType, float>& FirstMap, const TMap<KeyType, float>& SecondMap)
	{
		return FEGQuestHelper_MapEqualImpl<KeyType, float>::IsEqual(FirstMap, SecondMap,
			[](const float& FirstMapValue, const float& SecondMapValue) -> bool
		{
			return FMath::IsNearlyEqual(FirstMapValue, SecondMapValue, KINDA_SMALL_NUMBER);
		});
	}
};


template <typename ArrayType>
class FEGQuestHelper_ArrayEqualImpl
{
public:
	static bool IsEqual(
		const TArray<ArrayType>& FirstArray,
		const TArray<ArrayType>& SecondArray,
		std::function<bool(const ArrayType& FirstValue,
		const ArrayType& SecondValue)> AreValuesEqual
	)
	{
		if (FirstArray.Num() == SecondArray.Num())
		{
			// Some value is not equal
			for (int32 Index = 0; Index < FirstArray.Num(); Index++)
			{
				if (!AreValuesEqual(FirstArray[Index], SecondArray[Index]))
				{
					return false;
				}
			}

			return true;
		}

		// length differs
		return false;
	}
};

// Variant with default comparison
template <typename ArrayType>
class FEGQuestHelper_ArrayEqualVariantImpl
{
public:
	static bool IsEqual(const TArray<ArrayType>& FirstArray, const TArray<ArrayType>& SecondArray)
	{
		return FEGQuestHelper_ArrayEqualImpl<ArrayType>::IsEqual(FirstArray	, SecondArray,
			[](const ArrayType& FirstValue, const ArrayType& SecondValue) -> bool
		{
			return FirstValue == SecondValue;
		});
	}
};

// Variant with Specialization for float ArrayType
template <>
class FEGQuestHelper_ArrayEqualVariantImpl<float>
{
public:
	static bool IsEqual(const TArray<float>& FirstArray, const TArray<float>& SecondArray)
	{
		return FEGQuestHelper_ArrayEqualImpl<float>::IsEqual(FirstArray, SecondArray,
			[](const float& FirstValue, const float& SecondValue) -> bool
		{
			return FMath::IsNearlyEqual(FirstValue, SecondValue, KINDA_SMALL_NUMBER);
		});
	}
};


/**
 * General helper methods
 */
class UNREALEXTENDEDQUEST_API FEGQuestHelper
{
	typedef FEGQuestHelper Self;
public:
	FORCEINLINE static int64 RandomInt64() { return static_cast<int64>(FMath::Rand()) << 32 | FMath::Rand(); }
	FORCEINLINE static bool IsFloatEqual(const float A, const float B) { return FMath::IsNearlyEqual(A, B, KINDA_SMALL_NUMBER); }
	FORCEINLINE static bool IsPathInProjectDirectory(const FString& Path) { return Path.StartsWith("/Game");  }

	static FString GetFullNameFromObject(const UObject* Object)
	{
		if (!IsValid(Object))
		{
			return TEXT("nullptr");
		}
		return Object->GetFullName();
	}

	static FString GetClassNameFromObject(const UObject* Object)
	{
		if (!IsValid(Object))
		{
			return TEXT("INVALID CLASS");
		}
		return Object->GetClass()->GetName();
	}

	/**
	 * Try to open tab if it is closed at the last known location.  If it already exists, it will draw attention to the tab.
	 *
	 * @param TabManager The TabManager.
	 * @param TabId The tab identifier.
	 * @return The existing or newly spawned tab instance if successful.
	 */
	static TSharedPtr<SDockTab> InvokeTab(TSharedPtr<FTabManager> TabManager, const FTabId& TabID);

	// Removes _C from the end of the Name
	// And removes the .extension from the path names
	static FString CleanObjectName(FString Name);

	// Blueprint Helpers
	static bool IsABlueprintClass(const UClass* Class);

	// This also works with Blueprints
	static bool IsObjectAChildOf(const UObject* Object, const UClass* ParentClass);

	// Gets all child classes of ParentClass
	// NOTE: this is super slow, use with care
	static bool GetAllChildClassesOf(const UClass* ParentClass, TArray<UClass*>& OutNativeClasses, TArray<UClass*>& OutBlueprintClasses);

	// FileSystem
	static bool DeleteFile(const FString& PathName, bool bVerbose = true);
	static bool RenameFile(const FString& OldPathName, const FString& NewPathName, bool bOverWrite = false, bool bVerbose = true);

	// Gets the first element from a set. From https://answers.unrealengine.com/questions/332443/how-to-get-the-firstonly-element-in-tset.html
	template <typename SetType>
	static typename TCopyQualifiersFromTo<SetType, typename SetType::ElementType>::Type* GetFirstSetElement(SetType& Set)
	{
		for (auto& Element : Set)
		{
			return &Element;
		}

		return nullptr;
	}

	// Is FirstSet == SecondSet
	// NOTE for SetType = float this won't work, what are you even doing?
	template <typename SetType>
	static bool IsSetEqual(const TSet<SetType>& FirstSet, const TSet<SetType>& SecondSet)
	{
		if (FirstSet.Num() == SecondSet.Num())
		{
			// No duplicates should be found
			TSet<SetType> Set = FirstSet;
			Set.Append(SecondSet);

			return Set.Num() == FirstSet.Num();
		}

		return false;
	}

	// Is FirstArray == SecondArray ?
	template <typename ArrayType>
	static bool IsArrayEqual(const TArray<ArrayType>& FirstArray, const TArray<ArrayType>& SecondArray,
		std::function<bool(const ArrayType& FirstValue, const ArrayType& SecondValue)> AreValuesEqual)
	{
		return FEGQuestHelper_ArrayEqualImpl<ArrayType>::IsEqual(FirstArray, SecondArray, AreValuesEqual);
	}

	// Variant with default comparison
	template <typename ArrayType>
	static bool IsArrayEqual(const TArray<ArrayType>& FirstArray, const TArray<ArrayType>& SecondArray)
	{
		return FEGQuestHelper_ArrayEqualVariantImpl<ArrayType>::IsEqual(FirstArray, SecondArray);
	}

	// Is FirstMap == SecondMap ?
	template <typename KeyType, typename ValueType>
	static bool IsMapEqual(const TMap<KeyType, ValueType>& FirstMap, const TMap<KeyType, ValueType>& SecondMap,
		std::function<bool(const ValueType& FirstMapValue, const ValueType& SecondMapValue)> AreValuesEqual)
	{
		return FEGQuestHelper_MapEqualImpl<KeyType, ValueType>::IsEqual(FirstMap, SecondMap, AreValuesEqual);
	}

	// Variant with default comparison
	template <typename KeyType, typename ValueType>
	static bool IsMapEqual(const TMap<KeyType, ValueType>& FirstMap, const TMap<KeyType, ValueType>& SecondMap)
	{
		return FEGQuestHelper_MapEqualVariantImpl<KeyType, ValueType>::IsEqual(FirstMap, SecondMap);
	}
};
