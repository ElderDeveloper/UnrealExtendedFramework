// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EFJsonTypes.generated.h"

/** The kind of value a JSON handle or field holds. */
UENUM(BlueprintType)
enum class EEFJsonType : uint8
{
	/** No value: the handle is empty or the field does not exist. */
	None,
	Null,
	String,
	Number,
	Boolean,
	Array,
	Object
};

/**
 * Handle to a JSON object.
 *
 * Copies of the handle share one underlying object, the same way a TSharedPtr does: a field set
 * through one copy is visible through every copy. UEFJsonLibrary::CopyJsonObject makes an
 * independent deep copy.
 *
 * The handle is runtime data. It is not saved with assets or SaveGames and is not replicated;
 * convert it to a string or a struct for that.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "EF Json Object"))
struct EXTENDEDFOUNDATION_API FEFJsonObject
{
	GENERATED_BODY()

	FEFJsonObject() = default;
	explicit FEFJsonObject(TSharedPtr<FJsonObject> InObject)
		: Object(MoveTemp(InObject))
	{
	}

	/** A handle to a new, empty object. */
	static FEFJsonObject MakeEmpty() { return FEFJsonObject(MakeShared<FJsonObject>()); }

	bool IsValid() const { return Object.IsValid(); }

	/** The underlying object. Null when the handle is empty. */
	TSharedPtr<FJsonObject> Object;
};

/**
 * Handle to any JSON value: null, string, number, boolean, array or object.
 *
 * Array and object values share their contents between copies, like FEFJsonObject.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "EF Json Value"))
struct EXTENDEDFOUNDATION_API FEFJsonValue
{
	GENERATED_BODY()

	FEFJsonValue() = default;
	explicit FEFJsonValue(TSharedPtr<FJsonValue> InValue)
		: Value(MoveTemp(InValue))
	{
	}

	bool IsValid() const { return Value.IsValid(); }

	EEFJsonType GetType() const;

	/** The underlying value. Null when the handle is empty. */
	TSharedPtr<FJsonValue> Value;
};

namespace EFJson
{
	/** Maps the engine's EJson to EEFJsonType. A null pointer maps to None. */
	EXTENDEDFOUNDATION_API EEFJsonType ToType(const TSharedPtr<FJsonValue>& Value);

	/** Recursively copies arrays and objects. Primitive values are immutable and are shared. */
	EXTENDEDFOUNDATION_API TSharedPtr<FJsonValue> DeepCopyValue(const TSharedPtr<FJsonValue>& Value);
	EXTENDEDFOUNDATION_API TSharedPtr<FJsonObject> DeepCopyObject(const TSharedPtr<FJsonObject>& Object);

	/**
	 * Resolves a path such as "data.items[2].name" from Root.
	 *
	 * Dots separate object keys and [n] indexes arrays. An empty path resolves to Root itself.
	 * Keys that contain '.' or '[' cannot be addressed this way.
	 */
	EXTENDEDFOUNDATION_API bool ResolvePath(const TSharedPtr<FJsonObject>& Root, const FString& Path, TSharedPtr<FJsonValue>& OutValue);

	/** Parses a JSON object. Fails when the text is not valid JSON or its root is not an object. */
	EXTENDEDFOUNDATION_API bool ParseObject(const FString& JsonText, TSharedPtr<FJsonObject>& OutObject);

	/** Parses any JSON value with an object or array at its root. */
	EXTENDEDFOUNDATION_API bool ParseValue(const FString& JsonText, TSharedPtr<FJsonValue>& OutValue);

	/** Serializes an object. A null object serializes to an empty string. */
	EXTENDEDFOUNDATION_API FString ObjectToString(const TSharedPtr<FJsonObject>& Object, bool bPrettyPrint = false);

	/** Serializes any value. Primitives produce their JSON literal, e.g. "text" with quotes, 42, true, null. */
	EXTENDEDFOUNDATION_API FString ValueToString(const TSharedPtr<FJsonValue>& Value, bool bPrettyPrint = false);
}
