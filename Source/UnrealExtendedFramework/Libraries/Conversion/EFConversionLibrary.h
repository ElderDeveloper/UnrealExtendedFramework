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
	 static std::string FStringToStdString(const FString in)     {  return  std::string(TCHAR_TO_UTF8(*in));   }

    
    static FString StdStringToFString(const std::string in)    {  return FString(in.c_str());  }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion String To Text", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|String")
    static FText FStringToText(const FString in)    {    return FText::FromString(in);     }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion String To Name", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|String")
    static FName FStringToName(const FString in)    {    return FName(*in);    }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion String To Float", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|String")
    static float FStringToFloat(const FString in)    {   return FCString::Atof(*in);     }


    static double FStringToDouble(const FString in)    {    return FCString::Atod(*in);    }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion String To int32", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|String")
    static int32 FStringToInt32(const FString in)    {    return FCString::Atoi(*in);    }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion String To int64", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|String")
    static int64 FStringToInt64(const FString in)    {    return FCString::Atoi64(*in);    }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion String To Vector", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|String")
    static FVector FStringToVector(const FString in);

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion String To Rotator", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|String")
    static FRotator FStringToRotator(const FString in);

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion String To Quat", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|String")
    static FQuat FStringToQuat(const FString in);

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion String To Bool", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|String")
    static bool FStringToBool(const FString in)    {    return FCString::ToBool(*in);    }

    
#pragma endregion


    

#pragma region Float

    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion Float To String", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Float")
    static  FString FloatToFString(const float in)     {    return FString::SanitizeFloat(in);    }


    static  std::string FloatToStdString(const float in)     {    return  std::string(TCHAR_TO_UTF8(*FString::SanitizeFloat(in)));    }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion Float To Text", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Float")
    static FText FloatToText(const float in)     {      return FText::FromString(FString::SanitizeFloat(in));     }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion Float To Name", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Float")
    static FName FloatToName(const float in)    {    return FName(*FString::SanitizeFloat(in));    }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion Float To int32", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Float")
    static int32 FloatToInt32(const float in , EIntContype Type);

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion Float To int64", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Float")
    static int64 FloatToInt64(const float in , EIntContype Type);

    
    static double FloatToDouble(const float in)    {    return in;    }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion Float To Bool", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Float")
    static bool FloatToBool(const float in)    {    return static_cast<bool>(in);     }


    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion Float To Vector", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Float")
    static FVector FloatToVector(const float in ,const int32 XYZ=0 );

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion Float To Rotator", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Float")
    static FRotator FloatToRotator(const float in,const int32 XYZ=0);


    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion Float To Quat", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Float")
    static FQuat FloatToQuat(const float in,const float inW=0,const int32 XYZ=0);
    
#pragma endregion 





    

#pragma region Int32


    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion int32 To FString", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Int32")
    static  FString Int32ToFString(const int32 in)     {      return FString::FromInt(in);    }

    
    static  std::string Int32ToStdString(const int32 in)     {    return  std::string(TCHAR_TO_UTF8(*FString::FromInt(in)));    }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion int32 To Text", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Int32")
    static FText Int32ToText(const int32 in)     {      return FText::FromString(FString::FromInt(in));     }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion int32 To Name", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Int32")
    static FName Int32ToName(const int32 in)    {    return FName(*FString::FromInt(in));    }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion int32 To Float", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Int32")
    static float Int32ToFloat(const int32 in)   {    return static_cast<float>(in);     }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion int32 To int64", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Int32")
    static int64 Int32ToInt64(const int32 in)    {    return static_cast<int64>(in);     }

    
    static double Int32ToDouble(const int32 in)    {    return static_cast<double>(in);    }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion int32 To Bool", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Int32")
    static bool Int32ToBool(const int32 in)    {    return static_cast<bool>(in);    }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion int32 To Vector", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Int32")
    static FVector Int32ToVector(const int32 in ,const int32 XYZ=0 );

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion int32 To Rotator", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Int32")
    static FRotator Int32ToRotator(const int32 in,const int32 XYZ=0);

    
    static FQuat Int32ToQuat(const int32 in,const float inW=0,const int32 XYZ=0);


#pragma endregion






    
    
#pragma region Int64

    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion int64 To FString", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Int64")
    static  FString Int64ToFString(const int64 in)     {      return FString::FromInt(in);    }

    
    static std::string Int64ToStdString(const int64 in)     {    return  std::string(TCHAR_TO_UTF8(*FString::FromInt(in)));    }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion int64 To Text", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Int64")
    static FText Int64ToText(const int64 in)     {      return FText::FromString(FString::FromInt(in));     }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion int64 To Name", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Int64")
    static FName Int64ToName(const int64 in)    {    return FName(*FString::FromInt(in));    }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion int64 To Float", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Int64")
    static float Int64ToFloat(const int64 in)   {    return static_cast<float>(in);    }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion int64 To int32", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Int64")
    static int32 Int64ToInt32(const int64 in)    {    return static_cast<int32>(in);     }

    
    static double Int64ToDouble(const int64 in)    {    return static_cast<double>(in);    }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion int64 To Bool", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Int64")
    static bool Int64ToBool(const int64 in)    {    return static_cast<bool>(in);    }


    /** 
    * Returns Quad From Rotator...
    *@param XYZ xyz value will determine how the value will fill the vector   .
    0: All Vector values will be filled with value   .
    1: Only Vector X  will be assigned to int value   .
    2: Only Vector Y  will be assigned to int value   .
    3: Only Vector Z  will be assigned to int value   .
    4: Vector X and Y will be assigned to int value   .
    5: Vector X and Z will be assigned to int value   .
    6: Vector Y and Z will be assigned to int value   .
    Any Another value will fill all vector values to the int value   .
    */
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion int64 To Vector", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Int64")
    static FVector Int64ToVector(const int64 in ,const int32 XYZ=0 );

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion int64 To Rotator", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Int64")
    static FRotator Int64ToRotator(const int64 in,const int32 XYZ=0);

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion int64 To Quat", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Int64")
    static FQuat Int64ToQuat(const int64 in,const float inW=0,const int32 XYZ=0);

#pragma endregion





    
#pragma region Bool

    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion Bool To FString", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Bool")
    static  FString BoolToFString(const bool in)     {     return in ? "True" : "False";    }

    
    static  std::string BoolToStdString(const bool in)
    {
        const FString A = in ? "True" : "False";
        return  std::string(TCHAR_TO_UTF8(*A));
    }

    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion Bool To Text", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Bool")
    static FText BoolToText(const bool in)     {      return FText::FromString(in ? "True" : "False");     }

    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion Bool To Name", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Bool")
    static FName BoolToName(const bool in)
    {
        const FString A = in ? "True" : "False";
        return FName(*A);
    }

    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion Bool To Float", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Bool")
    static float BoolToFloat(const bool in)   {    return static_cast<float>(in);     }

    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion Bool To int32", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Bool")
    static int32 BoolToInt32(const bool in)    {    return static_cast<int32>(in);     }

    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion Bool To int32", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Bool")
    static int64 BoolToInt64(const bool in)    {    return static_cast<int64>(in);     }

    
    static double BoolToDouble(const bool in)    {   return static_cast<int64>(in);    }

#pragma endregion





    
#pragma region Vector

    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion Vector To FString", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Vector")
    static  FString VectorToFString(const FVector in)     {      return in.ToString();    }

    
    static  std::string VectorToStdString(const FVector in)     {    return  std::string(TCHAR_TO_UTF8(*in.ToString()));    }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion Vector To Text", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Vector")
    static FText VectorToText(const FVector in)     {      return FText::FromString(in.ToString());     }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion Vector To Name", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Vector")
    static FName VectorToName(const FVector in)    {    return FName(*in.ToString());    }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion Vector To Rotator", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Vector")
    static FRotator VectorToRotator(const FVector in) {    return FRotator(in.Y,in.Z,in.X);}

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion Vector To Quat", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Vector")
    static FQuat VectorToQuat(const FVector in,const float inW=0)    { return FQuat(in.Y,in.Z,in.X,inW);   }
 

#pragma endregion




    

#pragma region Rotator

    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion Rotator To FString", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Rotator")
    static  FString RotatorToFString(const FRotator in)     {      return in.ToString();    }

    
    static  std::string RotatorToStdString(const FRotator  in)     {    return  std::string(TCHAR_TO_UTF8(*in.ToString()));    }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion Rotator To Text", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Rotator")
    static FText RotatorToText(const FRotator  in)     {      return FText::FromString(in.ToString());     }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion Rotator To Name", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Rotator")
    static FName RotatorToName(const FRotator  in)    {    return FName(*in.ToString());    }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion Rotator To Vector", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Rotator")
    static FVector RotatorToVector(const FRotator  in) {    return FVector(in.Pitch,in.Yaw,in.Roll) ;    }

    /** 
    * Returns Quad From Rotator...
    * @param inW float value for quad W value
    */
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion Rotator To Quat", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Rotator")
    static FQuat RotatorToQuat(const FRotator  in,const float inW=0)    { return FQuat(in.Pitch,in.Yaw,in.Roll,inW);   }
 

#pragma endregion




    
    
#pragma region Transform

    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion Transform To FString", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Transform")
    static  FString TransformToFString(const FTransform in)     {      return in.ToString();    }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion Transform To Text", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Transform")
    static FText TransformToText(const FTransform  in)     {      return FText::FromString(in.ToString());     }

    
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Conversion Transform To Name", CompactNodeTitle = "->", BlueprintAutocast), Category="ConversionLibrary|Transform")
    static FName TransformToName(const FTransform  in)    {    return FName(*in.ToString());    }

#pragma endregion

    
};
