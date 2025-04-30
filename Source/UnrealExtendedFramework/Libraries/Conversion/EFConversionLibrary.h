// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include <iostream>
#include "EFConversionLibrary.generated.h"


UENUM(Blueprintable)
enum EIntContype
{
	Direct, Floor, Ceil, Round, Truncate
};

UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFConversionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()


public:

#pragma region FString
	 static std::string FStringToStdString(const FString& in)     {  return  std::string(TCHAR_TO_UTF8(*in));   }

    
    static FString StdStringToFString(const std::string in)    {  return FString(in.c_str());  }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "StringToText", BlueprintAutocast), Category="ConversionLibrary|String")
    static FText FStringToText(const FString& in)    {    return FText::FromString(in);     }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "StringToName", BlueprintAutocast), Category="ConversionLibrary|String")
    static FName FStringToName(const FString& in)    {    return FName(*in);    }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "StringToFloat", BlueprintAutocast), Category="ConversionLibrary|String")
    static float FStringToFloat(const FString& in)    {   return FCString::Atof(*in);     }


    static double FStringToDouble(const FString& in)    {    return FCString::Atod(*in);    }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "StringToint32", BlueprintAutocast), Category="ConversionLibrary|String")
    static int32 FStringToInt32(const FString& in)    {    return FCString::Atoi(*in);    }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "StringToint64", BlueprintAutocast), Category="ConversionLibrary|String")
    static int64 FStringToInt64(const FString& in)    {    return FCString::Atoi64(*in);    }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "StringToVector", BlueprintAutocast), Category="ConversionLibrary|String")
    static FVector FStringToVector(const FString& in);

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "StringToRotator", BlueprintAutocast), Category="ConversionLibrary|String")
    static FRotator FStringToRotator(const FString& in);

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "StringToQuat", BlueprintAutocast), Category="ConversionLibrary|String")
    static FQuat FStringToQuat(const FString& in);

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "StringToBool", BlueprintAutocast), Category="ConversionLibrary|String")
    static bool FStringToBool(const FString& in)    {    return FCString::ToBool(*in);    }

    
#pragma endregion


    

#pragma region Float

    UFUNCTION(BlueprintPure, meta=(DisplayName = "FloatToString", BlueprintAutocast), Category="ConversionLibrary|Float")
    static FString FloatToFString(const float in)     {    return FString::SanitizeFloat(in);    }


    static std::string FloatToStdString(const float in)     {    return  std::string(TCHAR_TO_UTF8(*FString::SanitizeFloat(in)));    }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "FloatToText", BlueprintAutocast), Category="ConversionLibrary|Float")
    static FText FloatToText(const float in)     {      return FText::FromString(FString::SanitizeFloat(in));     }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "FloatToName", BlueprintAutocast), Category="ConversionLibrary|Float")
    static FName FloatToName(const float in)    {    return FName(*FString::SanitizeFloat(in));    }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "FloatToint32", BlueprintAutocast), Category="ConversionLibrary|Float")
    static int32 FloatToInt32(const float in , EIntContype Type);

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "FloatToint64", BlueprintAutocast), Category="ConversionLibrary|Float")
    static int64 FloatToInt64(const float in , EIntContype Type);

    
    static double FloatToDouble(const float in)    {    return in;    }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "FloatToBool", BlueprintAutocast), Category="ConversionLibrary|Float")
    static bool FloatToBool(const float in)    {    return static_cast<bool>(in);     }


    UFUNCTION(BlueprintPure, meta=(DisplayName = "FloatToVector", BlueprintAutocast), Category="ConversionLibrary|Float")
    static FVector FloatToVector(const float in ,const int32 XYZ=0 );

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "FloatToRotator", BlueprintAutocast), Category="ConversionLibrary|Float")
    static FRotator FloatToRotator(const float in,const int32 XYZ=0);


    UFUNCTION(BlueprintPure, meta=(DisplayName = "FloatToQuat", BlueprintAutocast), Category="ConversionLibrary|Float")
    static FQuat FloatToQuat(const float in,const float inW=0,const int32 XYZ=0);
    
#pragma endregion 





    

#pragma region Int32


    UFUNCTION(BlueprintPure, meta=(DisplayName = "int32ToFString", BlueprintAutocast), Category="ConversionLibrary|Int32")
    static FString Int32ToFString(const int32 in)     {      return FString::FromInt(in);    }

    
    static std::string Int32ToStdString(const int32 in)     {    return  std::string(TCHAR_TO_UTF8(*FString::FromInt(in)));    }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "int32ToText", BlueprintAutocast), Category="ConversionLibrary|Int32")
    static FText Int32ToText(const int32 in)     {      return FText::FromString(FString::FromInt(in));     }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "int32ToName", BlueprintAutocast), Category="ConversionLibrary|Int32")
    static FName Int32ToName(const int32 in)    {    return FName(*FString::FromInt(in));    }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "int32ToFloat", BlueprintAutocast), Category="ConversionLibrary|Int32")
    static float Int32ToFloat(const int32 in)   {    return static_cast<float>(in);     }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "int32Toint64", BlueprintAutocast), Category="ConversionLibrary|Int32")
    static int64 Int32ToInt64(const int32 in)    {    return static_cast<int64>(in);     }

    
    static double Int32ToDouble(const int32 in)    {    return static_cast<double>(in);    }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "int32ToBool", BlueprintAutocast), Category="ConversionLibrary|Int32")
    static bool Int32ToBool(const int32 in)    {    return static_cast<bool>(in);    }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "int32ToVector", BlueprintAutocast), Category="ConversionLibrary|Int32")
    static FVector Int32ToVector(const int32 in ,const int32 XYZ=0 );

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "int32ToRotator", BlueprintAutocast), Category="ConversionLibrary|Int32")
    static FRotator Int32ToRotator(const int32 in,const int32 XYZ=0);

    
    static FQuat Int32ToQuat(const int32 in,const float inW=0,const int32 XYZ=0);


#pragma endregion


    
#pragma region Bool

    UFUNCTION(BlueprintPure, meta=(DisplayName = "BoolToFString", BlueprintAutocast), Category="ConversionLibrary|Bool")
    static FString BoolToFString(const bool in)     {     return in ? "True" : "False";    }

    
    static std::string BoolToStdString(const bool in)
    {
        const FString& A = in ? "True" : "False";
        return  std::string(TCHAR_TO_UTF8(*A));
    }

    UFUNCTION(BlueprintPure, meta=(DisplayName = "BoolToText", BlueprintAutocast), Category="ConversionLibrary|Bool")
    static FText BoolToText(const bool in)     {      return FText::FromString(in ? "True" : "False");     }

    UFUNCTION(BlueprintPure, meta=(DisplayName = "BoolToName", BlueprintAutocast), Category="ConversionLibrary|Bool")
    static FName BoolToName(const bool in)
    {
        const FString& A = in ? "True" : "False";
        return FName(*A);
    }

    UFUNCTION(BlueprintPure, meta=(DisplayName = "BoolToFloat", BlueprintAutocast), Category="ConversionLibrary|Bool")
    static float BoolToFloat(const bool in)   {    return static_cast<float>(in);     }

    UFUNCTION(BlueprintPure, meta=(DisplayName = "BoolToint32", BlueprintAutocast), Category="ConversionLibrary|Bool")
    static int32 BoolToInt32(const bool in)    {    return static_cast<int32>(in);     }

    UFUNCTION(BlueprintPure, meta=(DisplayName = "BoolToint32", BlueprintAutocast), Category="ConversionLibrary|Bool")
    static int64 BoolToInt64(const bool in)    {    return static_cast<int64>(in);     }

    
    static double BoolToDouble(const bool in)    {   return static_cast<int64>(in);    }

#pragma endregion





    
#pragma region Vector

    UFUNCTION(BlueprintPure, meta=(DisplayName = "VectorToFString", BlueprintAutocast), Category="ConversionLibrary|Vector")
    static FString VectorToFString(const FVector& in)     {      return in.ToString();    }

    
    static std::string VectorToStdString(const FVector& in)     {    return  std::string(TCHAR_TO_UTF8(*in.ToString()));    }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "VectorToText", BlueprintAutocast), Category="ConversionLibrary|Vector")
    static FText VectorToText(const FVector& in)     {      return FText::FromString(in.ToString());     }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "VectorToName", BlueprintAutocast), Category="ConversionLibrary|Vector")
    static FName VectorToName(const FVector& in)    {    return FName(*in.ToString());    }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "VectorToRotator", BlueprintAutocast), Category="ConversionLibrary|Vector")
    static FRotator VectorToRotator(const FVector& in) {    return FRotator(in.Y,in.Z,in.X);}

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "VectorToQuat", BlueprintAutocast), Category="ConversionLibrary|Vector")
    static FQuat VectorToQuat(const FVector& in,const float inW=0)    { return FQuat(in.Y,in.Z,in.X,inW);   }
 

#pragma endregion




    

#pragma region Rotator

    UFUNCTION(BlueprintPure, meta=(DisplayName = "RotatorToFString", BlueprintAutocast), Category="ConversionLibrary|Rotator")
    static FString RotatorToFString(const FRotator& in)     {      return in.ToString();    }

    
    static std::string RotatorToStdString(const FRotator& in)     {    return  std::string(TCHAR_TO_UTF8(*in.ToString()));    }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "RotatorToText", BlueprintAutocast), Category="ConversionLibrary|Rotator")
    static FText RotatorToText(const FRotator&  in)     {      return FText::FromString(in.ToString());     }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "RotatorToName", BlueprintAutocast), Category="ConversionLibrary|Rotator")
    static FName RotatorToName(const FRotator&  in)    {    return FName(*in.ToString());    }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "RotatorToVector", BlueprintAutocast), Category="ConversionLibrary|Rotator")
    static FVector RotatorToVector(const FRotator&  in) {    return FVector(in.Pitch,in.Yaw,in.Roll) ;    }

    /** 
    * Returns Quad From Rotator...
    * @param inW float value for quad W value
    */
    UFUNCTION(BlueprintPure, meta=(DisplayName = "RotatorToQuat", BlueprintAutocast), Category="ConversionLibrary|Rotator")
    static FQuat RotatorToQuat(const FRotator&  in,const float inW=0)    { return FQuat(in.Pitch,in.Yaw,in.Roll,inW);   }
 

#pragma endregion


    
#pragma region Transform

    UFUNCTION(BlueprintPure, meta=(DisplayName = "TransformToFString", BlueprintAutocast), Category="ConversionLibrary|Transform")
    static FString TransformToFString(const FTransform& in)     {      return in.ToString();    }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "TransformToText", BlueprintAutocast), Category="ConversionLibrary|Transform")
    static FText TransformToText(const FTransform&  in)     {      return FText::FromString(in.ToString());     }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "TransformToName", BlueprintAutocast), Category="ConversionLibrary|Transform")
    static FName TransformToName(const FTransform&  in)    {    return FName(*in.ToString());    }

#pragma endregion
    
    template<typename T>
    static FString EnumToString(const FString& enumName, const T value)
    {
        // Use nullptr as the outer if the enum is in the global scope, or provide a valid outer object
        UEnum* pEnum = FindObject<UEnum>(nullptr, *enumName);
        return (pEnum ? pEnum->GetNameStringByIndex(static_cast<uint8>(value)) : TEXT("null"));
    }
    
    
};
