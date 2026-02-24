// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include <string>
#include "UObject/Object.h"
#include "EFConversionLibrary.generated.h"


/** Specifies the rounding method when converting a float to an integer. */
UENUM(Blueprintable)
enum EIntContype
{
	Direct, Floor, Ceil, Round, Truncate
};

/**
 * Blueprint function library providing comprehensive type conversion utilities.
 * Supports conversions between FString, float, int32, int64, double, bool,
 * FVector, FRotator, FQuat, FTransform, FText, FName, FLinearColor, and std::string.
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFConversionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()


public:

	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< FString >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

#pragma region FString

	/** Converts an FString to a std::string (UTF-8 encoded). C++ only. */
	static std::string FStringToStdString(const FString& in) { return std::string(TCHAR_TO_UTF8(*in)); }

	/** Converts a std::string to an FString. C++ only. */
	static FString StdStringToFString(const std::string in) { return FString(in.c_str()); }

	/** Converts an FString to FText. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "StringToText", BlueprintAutocast), Category="ConversionLibrary|String")
	static FText FStringToText(const FString& in) { return FText::FromString(in); }

	/** Converts an FString to FName. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "StringToName", BlueprintAutocast), Category="ConversionLibrary|String")
	static FName FStringToName(const FString& in) { return FName(*in); }

	/** Converts an FString to float. Returns 0 if the string is not a valid number. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "StringToFloat", BlueprintAutocast), Category="ConversionLibrary|String")
	static float FStringToFloat(const FString& in) { return FCString::Atof(*in); }

	/** Converts an FString to double. C++ only. Returns 0 if the string is not a valid number. */
	static double FStringToDouble(const FString& in) { return FCString::Atod(*in); }

	/** Converts an FString to double (Blueprint-exposed version).
	 * @param in The string to convert
	 * @return The parsed double value, or 0 if invalid
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "StringToDouble", BlueprintAutocast), Category="ConversionLibrary|String")
	static double FStringToDoubleBP(const FString& in) { return FCString::Atod(*in); }

	/** Converts an FString to int32. Returns 0 if the string is not a valid number. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "StringToint32", BlueprintAutocast), Category="ConversionLibrary|String")
	static int32 FStringToInt32(const FString& in) { return FCString::Atoi(*in); }

	/** Converts an FString to int64. Returns 0 if the string is not a valid number. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "StringToint64", BlueprintAutocast), Category="ConversionLibrary|String")
	static int64 FStringToInt64(const FString& in) { return FCString::Atoi64(*in); }

	/**
	 * Parses an FString into an FVector (expects format "X=... Y=... Z=...").
	 * @param in The string to parse
	 * @return The parsed vector, or ZeroVector on failure
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "StringToVector", BlueprintAutocast), Category="ConversionLibrary|String")
	static FVector FStringToVector(const FString& in);

	/**
	 * Parses an FString into an FRotator (expects format "P=... Y=... R=...").
	 * @param in The string to parse
	 * @return The parsed rotator, or ZeroRotator on failure
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "StringToRotator", BlueprintAutocast), Category="ConversionLibrary|String")
	static FRotator FStringToRotator(const FString& in);

	/**
	 * Parses an FString into an FQuat by first parsing as a rotator, then converting to quaternion.
	 * @param in The string to parse (rotator format: "P=... Y=... R=...")
	 * @return The resulting quaternion (normalized), or identity on failure
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "StringToQuat", BlueprintAutocast), Category="ConversionLibrary|String")
	static FQuat FStringToQuat(const FString& in);

	/** Converts an FString to bool. Recognizes "True", "Yes", "1", "On" as true. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "StringToBool", BlueprintAutocast), Category="ConversionLibrary|String")
	static bool FStringToBool(const FString& in) { return FCString::ToBool(*in); }

#pragma endregion


	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Float >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

#pragma region Float

	/** Converts a float to FString with sanitized formatting (removes trailing zeros). */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "FloatToString", BlueprintAutocast), Category="ConversionLibrary|Float")
	static FString FloatToFString(const float in) { return FString::SanitizeFloat(in); }

	/** Converts a float to std::string. C++ only. */
	static std::string FloatToStdString(const float in) { return std::string(TCHAR_TO_UTF8(*FString::SanitizeFloat(in))); }

	/** Converts a float to FText. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "FloatToText", BlueprintAutocast), Category="ConversionLibrary|Float")
	static FText FloatToText(const float in) { return FText::FromString(FString::SanitizeFloat(in)); }

	/** Converts a float to FName. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "FloatToName", BlueprintAutocast), Category="ConversionLibrary|Float")
	static FName FloatToName(const float in) { return FName(*FString::SanitizeFloat(in)); }

	/**
	 * Converts a float to int32 using the specified rounding method.
	 * @param in The float value to convert
	 * @param Type The rounding method (Direct, Floor, Ceil, Round, Truncate)
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "FloatToint32", BlueprintAutocast), Category="ConversionLibrary|Float")
	static int32 FloatToInt32(const float in , EIntContype Type);

	/**
	 * Converts a float to int64 using the specified rounding method.
	 * @param in The float value to convert
	 * @param Type The rounding method (Direct, Floor, Ceil, Round, Truncate)
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "FloatToint64", BlueprintAutocast), Category="ConversionLibrary|Float")
	static int64 FloatToInt64(const float in , EIntContype Type);

	/** Converts a float to double (lossless widening). C++ only. */
	static double FloatToDouble(const float in) { return in; }

	/** Converts a float to bool. Returns true for any non-zero value. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "FloatToBool", BlueprintAutocast), Category="ConversionLibrary|Float")
	static bool FloatToBool(const float in) { return in != 0.f; }

	/**
	 * Creates an FVector from a float value, assigning it to axes based on the XYZ parameter.
	 * XYZ: 0=All, 1=X only, 2=Y only, 3=Z only, 4=XY, 5=XZ, 6=YZ
	 * @param in The float value
	 * @param XYZ Axis selection (0-6)
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "FloatToVector", BlueprintAutocast), Category="ConversionLibrary|Float")
	static FVector FloatToVector(const float in ,const int32 XYZ=0 );

	/**
	 * Creates an FRotator from a float value, assigning it to axes based on the XYZ parameter.
	 * XYZ: 0=All, 1=Pitch only, 2=Yaw only, 3=Roll only, 4=PitchYaw, 5=PitchRoll, 6=YawRoll
	 * @param in The float value
	 * @param XYZ Axis selection (0-6)
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "FloatToRotator", BlueprintAutocast), Category="ConversionLibrary|Float")
	static FRotator FloatToRotator(const float in,const int32 XYZ=0);

	/**
	 * Creates an FQuat from a float value, assigning it to components based on the XYZ parameter.
	 * WARNING: The resulting quaternion is normalized. Raw component assignment may not
	 * produce meaningful rotations unless the values form a valid unit quaternion.
	 * @param in The float value for selected components
	 * @param inW The W component value
	 * @param XYZ Component selection (0-6)
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "FloatToQuat", BlueprintAutocast), Category="ConversionLibrary|Float")
	static FQuat FloatToQuat(const float in,const float inW=0,const int32 XYZ=0);
    
#pragma endregion 


	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Int32 >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

#pragma region Int32

	/** Converts an int32 to FString. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "int32ToFString", BlueprintAutocast), Category="ConversionLibrary|Int32")
	static FString Int32ToFString(const int32 in) { return FString::FromInt(in); }

	/** Converts an int32 to std::string. C++ only. */
	static std::string Int32ToStdString(const int32 in) { return std::string(TCHAR_TO_UTF8(*FString::FromInt(in))); }

	/** Converts an int32 to FText. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "int32ToText", BlueprintAutocast), Category="ConversionLibrary|Int32")
	static FText Int32ToText(const int32 in) { return FText::FromString(FString::FromInt(in)); }

	/** Converts an int32 to FName. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "int32ToName", BlueprintAutocast), Category="ConversionLibrary|Int32")
	static FName Int32ToName(const int32 in) { return FName(*FString::FromInt(in)); }

	/** Converts an int32 to float. Note: may lose precision for large values. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "int32ToFloat", BlueprintAutocast), Category="ConversionLibrary|Int32")
	static float Int32ToFloat(const int32 in) { return static_cast<float>(in); }

	/** Converts an int32 to int64 (lossless widening). */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "int32Toint64", BlueprintAutocast), Category="ConversionLibrary|Int32")
	static int64 Int32ToInt64(const int32 in) { return static_cast<int64>(in); }

	/** Converts an int32 to double (lossless). C++ only. */
	static double Int32ToDouble(const int32 in) { return static_cast<double>(in); }

	/** Converts an int32 to bool. Returns true for any non-zero value. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "int32ToBool", BlueprintAutocast), Category="ConversionLibrary|Int32")
	static bool Int32ToBool(const int32 in) { return in != 0; }

	/**
	 * Creates an FVector from an int32 value, assigning it to axes based on the XYZ parameter.
	 * XYZ: 0=All, 1=X only, 2=Y only, 3=Z only, 4=XY, 5=XZ, 6=YZ
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "int32ToVector", BlueprintAutocast), Category="ConversionLibrary|Int32")
	static FVector Int32ToVector(const int32 in ,const int32 XYZ=0 );

	/**
	 * Creates an FRotator from an int32 value, assigning it to axes based on the XYZ parameter.
	 * XYZ: 0=All, 1=Pitch only, 2=Yaw only, 3=Roll only, 4=PitchYaw, 5=PitchRoll, 6=YawRoll
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "int32ToRotator", BlueprintAutocast), Category="ConversionLibrary|Int32")
	static FRotator Int32ToRotator(const int32 in,const int32 XYZ=0);

	/** Creates an FQuat from an int32 value. Result is normalized. C++ only. */
	static FQuat Int32ToQuat(const int32 in,const float inW=0,const int32 XYZ=0);


#pragma endregion


	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Double >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

#pragma region Double

	/** Converts a double to FString with full precision. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "DoubleToString", BlueprintAutocast), Category="ConversionLibrary|Double")
	static FString DoubleToFString(const double in) { return FString::Printf(TEXT("%.17g"), in); }

#pragma endregion


	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Bool >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

#pragma region Bool

	/** Converts a bool to FString ("True" or "False"). */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "BoolToFString", BlueprintAutocast), Category="ConversionLibrary|Bool")
	static FString BoolToFString(const bool in) { return in ? "True" : "False"; }

	/** Converts a bool to std::string. C++ only. */
	static std::string BoolToStdString(const bool in)
	{
		const FString& A = in ? "True" : "False";
		return std::string(TCHAR_TO_UTF8(*A));
	}

	/** Converts a bool to FText ("True" or "False"). */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "BoolToText", BlueprintAutocast), Category="ConversionLibrary|Bool")
	static FText BoolToText(const bool in) { return FText::FromString(in ? "True" : "False"); }

	/** Converts a bool to FName ("True" or "False"). */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "BoolToName", BlueprintAutocast), Category="ConversionLibrary|Bool")
	static FName BoolToName(const bool in)
	{
		const FString& A = in ? "True" : "False";
		return FName(*A);
	}

	/** Converts a bool to float (1.0 for true, 0.0 for false). */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "BoolToFloat", BlueprintAutocast), Category="ConversionLibrary|Bool")
	static float BoolToFloat(const bool in) { return static_cast<float>(in); }

	/** Converts a bool to int32 (1 for true, 0 for false). */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "BoolToint32", BlueprintAutocast), Category="ConversionLibrary|Bool")
	static int32 BoolToInt32(const bool in) { return static_cast<int32>(in); }

	/** Converts a bool to int64 (1 for true, 0 for false). */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "BoolToint64", BlueprintAutocast), Category="ConversionLibrary|Bool")
	static int64 BoolToInt64(const bool in) { return static_cast<int64>(in); }

	/** Converts a bool to double (1.0 for true, 0.0 for false). C++ only. */
	static double BoolToDouble(const bool in) { return static_cast<double>(in); }

#pragma endregion


	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Vector >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

#pragma region Vector

	/** Converts an FVector to its string representation "X=... Y=... Z=...". */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "VectorToFString", BlueprintAutocast), Category="ConversionLibrary|Vector")
	static FString VectorToFString(const FVector& in) { return in.ToString(); }

	/** Converts an FVector to std::string. C++ only. */
	static std::string VectorToStdString(const FVector& in) { return std::string(TCHAR_TO_UTF8(*in.ToString())); }

	/** Converts an FVector to FText. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "VectorToText", BlueprintAutocast), Category="ConversionLibrary|Vector")
	static FText VectorToText(const FVector& in) { return FText::FromString(in.ToString()); }

	/** Converts an FVector to FName. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "VectorToName", BlueprintAutocast), Category="ConversionLibrary|Vector")
	static FName VectorToName(const FVector& in) { return FName(*in.ToString()); }

	/**
	 * Converts an FVector to FRotator by mapping X->Roll, Y->Pitch, Z->Yaw.
	 * Note: The mapping is FRotator(Pitch=Y, Yaw=Z, Roll=X).
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "VectorToRotator", BlueprintAutocast), Category="ConversionLibrary|Vector")
	static FRotator VectorToRotator(const FVector& in) { return FRotator(in.Y, in.Z, in.X); }

	/**
	 * Creates an FQuat from vector components (X->Pitch, Y->Yaw, Z->Roll) plus a W value.
	 * The resulting quaternion is normalized.
	 * @param in The vector providing X/Y/Z components
	 * @param inW The W component
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "VectorToQuat", BlueprintAutocast), Category="ConversionLibrary|Vector")
	static FQuat VectorToQuat(const FVector& in, const float inW=0);

#pragma endregion


	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Rotator >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

#pragma region Rotator

	/** Converts an FRotator to its string representation "P=... Y=... R=...". */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "RotatorToFString", BlueprintAutocast), Category="ConversionLibrary|Rotator")
	static FString RotatorToFString(const FRotator& in) { return in.ToString(); }

	/** Converts an FRotator to std::string. C++ only. */
	static std::string RotatorToStdString(const FRotator& in) { return std::string(TCHAR_TO_UTF8(*in.ToString())); }

	/** Converts an FRotator to FText. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "RotatorToText", BlueprintAutocast), Category="ConversionLibrary|Rotator")
	static FText RotatorToText(const FRotator&  in) { return FText::FromString(in.ToString()); }

	/** Converts an FRotator to FName. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "RotatorToName", BlueprintAutocast), Category="ConversionLibrary|Rotator")
	static FName RotatorToName(const FRotator&  in) { return FName(*in.ToString()); }

	/**
	 * Converts an FRotator to FVector by mapping Pitch->X, Yaw->Y, Roll->Z.
	 * Note: This is a component mapping, not a direction vector conversion.
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "RotatorToVector", BlueprintAutocast), Category="ConversionLibrary|Rotator")
	static FVector RotatorToVector(const FRotator&  in) { return FVector(in.Pitch, in.Yaw, in.Roll); }

	/**
	 * Converts an FRotator to FQuat. The resulting quaternion is normalized.
	 * @param in The rotator to convert
	 * @param inW The W component (only used for raw component construction, not from Rotator->Quat conversion)
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "RotatorToQuat", BlueprintAutocast), Category="ConversionLibrary|Rotator")
	static FQuat RotatorToQuat(const FRotator&  in, const float inW=0);

#pragma endregion


	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Transform >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

#pragma region Transform

	/** Converts an FTransform to its string representation. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "TransformToFString", BlueprintAutocast), Category="ConversionLibrary|Transform")
	static FString TransformToFString(const FTransform& in) { return in.ToString(); }

	/** Converts an FTransform to FText. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "TransformToText", BlueprintAutocast), Category="ConversionLibrary|Transform")
	static FText TransformToText(const FTransform&  in) { return FText::FromString(in.ToString()); }

	/** Converts an FTransform to FName. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "TransformToName", BlueprintAutocast), Category="ConversionLibrary|Transform")
	static FName TransformToName(const FTransform&  in) { return FName(*in.ToString()); }

#pragma endregion


	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Color >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

#pragma region Color

	/**
	 * Converts an FString to FLinearColor. Expects hex format "#RRGGBBAA" or "RRGGBBAA",
	 * or named format "R=... G=... B=... A=...".
	 * @param in The string to parse
	 * @return The parsed color, or black on failure
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "StringToColor", BlueprintAutocast), Category="ConversionLibrary|Color")
	static FLinearColor StringToColor(const FString& in);

	/** Converts an FLinearColor to its hex string representation "#RRGGBBAA". */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "ColorToString", BlueprintAutocast), Category="ConversionLibrary|Color")
	static FString ColorToString(const FLinearColor& in);

#pragma endregion


	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Enum >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/**
	 * Converts an enum value to its display name string.
	 * NOTE: enumName must be the full path, e.g. "/Script/MyModule.EMyEnum".
	 * @param enumName The full path of the enum type
	 * @param value The enum value to convert
	 * @return The display name string, or "null" if the enum was not found
	 */
	template<typename T>
	static FString EnumToString(const FString& enumName, const T value)
	{
		UEnum* pEnum = FindObject<UEnum>(nullptr, *enumName);
		return (pEnum ? pEnum->GetNameStringByIndex(static_cast<uint8>(value)) : TEXT("null"));
	}
    
    
};
