// Fill out your copyright notice in the Description page of Project Settings.


#include "EFConversionLibrary.h"
#include "Kismet/KismetStringLibrary.h"


//<<<<<<<<<<<<<<<<<<<<<<<  STRING  >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
FVector UEFConversionLibrary::FStringToVector(FString in)
{
    //UKismetStringLibrary::BuildString_Vector()
    FVector Out;
    bool Succes;
    UKismetStringLibrary::Conv_StringToVector(in,Out,Succes);
    return Out;
}

FRotator UEFConversionLibrary::FStringToRotator(FString in)
{
    FRotator Out;
    bool Succes;
    UKismetStringLibrary::Conv_StringToRotator(in,Out,Succes);
    return Out;
}

FQuat UEFConversionLibrary::FStringToQuat(FString in)
{
    FRotator Out;
    bool Succes;
    UKismetStringLibrary::Conv_StringToRotator(in,Out,Succes);
    return  Out.Quaternion();
}






//<<<<<<<<<<<<<<<<<<<<<<<  FLOAT  >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
int32 UEFConversionLibrary::FloatToInt32(float in , EIntContype Type)
{
    switch (Type)
    {
        case Direct:
            return in;
        case Floor:
            return FMath::FloorToInt(in);
        case Ceil:
            return FMath::CeilToInt(in);
        case Round:
            return FMath::RoundToInt(in);
        case Truncate:
            return FMath::TruncToInt(in);
        default:
            return in;
    }

}

int64 UEFConversionLibrary::FloatToInt64(float in , EIntContype Type)
{
    switch (Type)
    {
    case Direct:
        return in;
    case Floor:
        return FMath::FloorToInt(in);
    case Ceil:
        return FMath::CeilToInt(in);
    case Round:
        return FMath::RoundToInt(in);
    case Truncate:
        return FMath::TruncToInt(in);
    default:
        return in;
    }
}

FVector UEFConversionLibrary::FloatToVector(float in, int32 XYZ)
{
    FVector Out;
    switch (XYZ)
    {
    case 0:    Out=FVector(in,in,in);   break;
    case 1:    Out=FVector(in,0,0);  break;
    case 2:    Out=FVector(0,in,0);   break;
    case 3:    Out=FVector(0,0,in);  break;
    case 4:    Out=FVector(in,in,0);   break;
    case 5:    Out=FVector(in,0,in);   break;
    case 6:    Out=FVector(0,in,in);    break;
    default:   Out=FVector(in,in,in);   break;
    }
    return Out;
}

FRotator UEFConversionLibrary::FloatToRotator(float in, int32 XYZ)
{
    FRotator Out;
    switch (XYZ)
    {
    case 0:    Out=FRotator(in,in,in);   break;
    case 1:    Out=FRotator(in,0,0);  break;
    case 2:    Out=FRotator(0,in,0);   break;
    case 3:    Out=FRotator(0,0,in);  break;
    case 4:    Out=FRotator(in,in,0);   break;
    case 5:    Out=FRotator(in,0,in);   break;
    case 6:    Out=FRotator(0,in,in);    break;
    default:   Out=FRotator(in,in,in);   break;
    }
    return Out;
}

FQuat UEFConversionLibrary::FloatToQuat(float in,float inW,int32 XYZ)
{
    FQuat Out;
    switch (XYZ)
    {
    case 0:    Out=FQuat(in,in,in,inW);   break;
    case 1:    Out=FQuat(in,0,0,inW);  break;
    case 2:    Out=FQuat(0,in,0,inW);   break;
    case 3:    Out=FQuat(0,0,in,inW);  break;
    case 4:    Out=FQuat(in,in,0,inW);   break;
    case 5:    Out=FQuat(in,0,in,inW);   break;
    case 6:    Out=FQuat(0,in,in,inW);    break;
    default:   Out=FQuat(in,in,in,inW);   break;
    }
    return Out;
}






//<<<<<<<<<<<<<<<<<<<<<<<  Int32  >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
FVector UEFConversionLibrary::Int32ToVector(const int32 in, const int32 XYZ)
{
    FVector Out;
    switch (XYZ)
    {
    case 0:    Out=FVector(in,in,in);   break;
    case 1:    Out=FVector(in,0,0);  break;
    case 2:    Out=FVector(0,in,0);   break;
    case 3:    Out=FVector(0,0,in);  break;
    case 4:    Out=FVector(in,in,0);   break;
    case 5:    Out=FVector(in,0,in);   break;
    case 6:    Out=FVector(0,in,in);    break;
    default:   Out=FVector(in,in,in);   break;
    }
    return Out;
}

FRotator UEFConversionLibrary::Int32ToRotator(const int32 in, const int32 XYZ)
{
    FRotator Out;
    switch (XYZ)
    {
    case 0:    Out=FRotator(in,in,in);   break;
    case 1:    Out=FRotator(in,0,0);  break;
    case 2:    Out=FRotator(0,in,0);   break;
    case 3:    Out=FRotator(0,0,in);  break;
    case 4:    Out=FRotator(in,in,0);   break;
    case 5:    Out=FRotator(in,0,in);   break;
    case 6:    Out=FRotator(0,in,in);    break;
    default:   Out=FRotator(in,in,in);   break;
    }
    return Out;
}

FQuat UEFConversionLibrary::Int32ToQuat(const int32 in, const float inW, const int32 XYZ)
{
    FQuat Out;
    switch (XYZ)
    {
    case 0:    Out=FQuat(in,in,in,inW);   break;
    case 1:    Out=FQuat(in,0,0,inW);  break;
    case 2:    Out=FQuat(0,in,0,inW);   break;
    case 3:    Out=FQuat(0,0,in,inW);  break;
    case 4:    Out=FQuat(in,in,0,inW);   break;
    case 5:    Out=FQuat(in,0,in,inW);   break;
    case 6:    Out=FQuat(0,in,in,inW);    break;
    default:   Out=FQuat(in,in,in,inW);   break;
    }
    return Out;
}






//<<<<<<<<<<<<<<<<<<<<<<<  Int64  >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
FVector UEFConversionLibrary::Int64ToVector(const int64 in, const int32 XYZ)
{
    FVector Out;
    switch (XYZ)
    {
    case 0:    Out=FVector(in,in,in);   break;
    case 1:    Out=FVector(in,0,0);  break;
    case 2:    Out=FVector(0,in,0);   break;
    case 3:    Out=FVector(0,0,in);  break;
    case 4:    Out=FVector(in,in,0);   break;
    case 5:    Out=FVector(in,0,in);   break;
    case 6:    Out=FVector(0,in,in);    break;
    default:   Out=FVector(in,in,in);   break;
    }
    return Out;
}

FRotator UEFConversionLibrary::Int64ToRotator(const int64 in, const int32 XYZ)
{
    FRotator Out;
    switch (XYZ)
    {
    case 0:    Out=FRotator(in,in,in);   break;
    case 1:    Out=FRotator(in,0,0);  break;
    case 2:    Out=FRotator(0,in,0);   break;
    case 3:    Out=FRotator(0,0,in);  break;
    case 4:    Out=FRotator(in,in,0);   break;
    case 5:    Out=FRotator(in,0,in);   break;
    case 6:    Out=FRotator(0,in,in);    break;
    default:   Out=FRotator(in,in,in);   break;
    }
    return Out;
}

FQuat UEFConversionLibrary::Int64ToQuat(const int64 in, const float inW, const int32 XYZ)
{
    FQuat Out;
    switch (XYZ)
    {
    case 0:    Out=FQuat(in,in,in,inW);   break;
    case 1:    Out=FQuat(in,0,0,inW);  break;
    case 2:    Out=FQuat(0,in,0,inW);   break;
    case 3:    Out=FQuat(0,0,in,inW);  break;
    case 4:    Out=FQuat(in,in,0,inW);   break;
    case 5:    Out=FQuat(in,0,in,inW);   break;
    case 6:    Out=FQuat(0,in,in,inW);    break;
    default:   Out=FQuat(in,in,in,inW);   break;
    }
    return Out;
}