// Fill out your copyright notice in the Description page of Project Settings.


#include "EFPerceptionLibrary.h"

#include "UnrealExtendedFramework/UnrealExtendedFramework.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Sight.h"


UAISenseConfig* UEFPerceptionLibrary::GetPerceptionSenseConfig(UAIPerceptionComponent* Perception, TSubclassOf<UAISense> SenseClass)
{
    if (!Perception)
    {
        UE_LOG(LogExtendedFramework, Error, TEXT("GetPerceptionSenseConfig: Perception == nullptr"));
        return nullptr;
    }
    
    if (const auto Id = UAISense::GetSenseID(SenseClass))
        return Perception->GetSenseConfig(Id);
    
    UE_LOG(LogExtendedFramework, Error, TEXT("GetPerceptionSenseConfig: Wrong Sense ID"));
    return nullptr;
}




bool UEFPerceptionLibrary::ForgetActor(UAIPerceptionComponent* Perception, AActor* Actor)
{
    if (Perception && Actor)
    {
        Perception->ForgetActor(Actor);
        return true;
    }

    UE_LOG(LogExtendedFramework, Error, TEXT("ForgetActor: Perception or Actor == nullptr"));
    return false;
}




bool UEFPerceptionLibrary::ForgetAll(UAIPerceptionComponent* Perception)
{
    if (Perception)
    {
        Perception->ForgetAll();
        return true;
    }
    
    UE_LOG(LogExtendedFramework, Error, TEXT("ForgetAll: Perception == nullptr"));
    return false;
}




TSubclassOf<UAISense> UEFPerceptionLibrary::GetDominantSense(UAIPerceptionComponent* Perception)
{
    if (Perception)
        return Perception->GetDominantSense();
    
    UE_LOG(LogExtendedFramework, Error, TEXT("GetDominantSense: Perception == nullptr"));
    return nullptr;
}




bool UEFPerceptionLibrary::SetDominantSense(UAIPerceptionComponent* Perception, TSubclassOf<UAISense> SenseClass)
{
    if (Perception && SenseClass)
    {
        Perception->SetDominantSense(SenseClass);
        return true;
    }
    UE_LOG(LogExtendedFramework, Error, TEXT("SetDominantSense: Perception or SenseClass == nullptr"));
    return false;
}




FAISenseAffiliationFilter UEFPerceptionLibrary::GetDetectionByAffiliation(UAIPerceptionComponent* Perception, TSubclassOf<UAISense> SenseClass)
{
    if (Perception && SenseClass)
    {
        if (const auto config = GetPerceptionSenseConfig(Perception, SenseClass))
        {
            if (const auto sightConfig = Cast<UAISenseConfig_Sight>(config))
                return sightConfig->DetectionByAffiliation;
            
            if (const auto hearingSenseConfig = Cast<UAISenseConfig_Hearing>(config))
                return hearingSenseConfig->DetectionByAffiliation;
        }
    }
    UE_LOG(LogExtendedFramework, Error, TEXT("GetDetectionByAffiliation: Perception or Sense Config == nullptr"));
    return FAISenseAffiliationFilter();
}




bool UEFPerceptionLibrary::SetDetectionByAffiliation(UAIPerceptionComponent* Perception, TSubclassOf<UAISense> SenseClass, bool DetectEnemies, bool DetectNeutrals, bool DetectFriendlies)
{
    if (Perception && SenseClass)
    {
        if (const auto config = GetPerceptionSenseConfig(Perception, SenseClass))
        {
            if (const auto sightConfig = Cast<UAISenseConfig_Sight>(config))
            {
                sightConfig->DetectionByAffiliation.bDetectEnemies = DetectEnemies;
                sightConfig->DetectionByAffiliation.bDetectNeutrals = DetectNeutrals;
                sightConfig->DetectionByAffiliation.bDetectFriendlies = DetectFriendlies;
            }
            
            if (const auto hearingSenseConfig = Cast<UAISenseConfig_Hearing>(config))
            {
                hearingSenseConfig->DetectionByAffiliation.bDetectEnemies = DetectEnemies;
                hearingSenseConfig->DetectionByAffiliation.bDetectNeutrals = DetectNeutrals;
                hearingSenseConfig->DetectionByAffiliation.bDetectFriendlies = DetectFriendlies;
            }
            return true;
        }
    }
    UE_LOG(LogExtendedFramework, Error, TEXT("SetDetectionByAffiliation: Perception or SenseClass == nullptr"));
    return false;
}




float UEFPerceptionLibrary::GetMaxAge(UAIPerceptionComponent* Perception, TSubclassOf<UAISense> SenseClass)
{
    if (Perception && SenseClass)
    {
        if (const auto config = GetPerceptionSenseConfig(Perception, SenseClass))
        {
            if (const auto sightConfig = Cast<UAISenseConfig_Sight>(config))
                return sightConfig->GetMaxAge();
            
            if (const auto hearingSenseConfig = Cast<UAISenseConfig_Hearing>(config))
                return hearingSenseConfig->GetMaxAge();
        }
    }

    UE_LOG(LogExtendedFramework, Error, TEXT("GetMaxAge: Perception or SenseClass == nullptr"));
    return -1;
}




bool UEFPerceptionLibrary::SetMaxAge(UAIPerceptionComponent* Perception, TSubclassOf<UAISense> SenseClass, float MaxAge)
{
    if (Perception && SenseClass)
    {
        if (const auto config = GetPerceptionSenseConfig(Perception, SenseClass))
        {
            if (const auto sightConfig = Cast<UAISenseConfig_Sight>(config))
                sightConfig->SetMaxAge(MaxAge);
            
            if (const auto hearingSenseConfig = Cast<UAISenseConfig_Hearing>(config))
                hearingSenseConfig->SetMaxAge(MaxAge);

            return true;
        }
    }
    UE_LOG(LogExtendedFramework, Error, TEXT("SetMaxAge: Perception or SenseClass == nullptr"));
    return false;
}




float UEFPerceptionLibrary::GetSightRange(UAIPerceptionComponent* Perception)
{
    if (Perception)
    {
        if (const auto config = GetPerceptionSenseConfig(Perception, UAISense_Sight::StaticClass()))
        {
            if (const auto sightConfig = Cast<UAISenseConfig_Sight>(config))
                return sightConfig->SightRadius;
        }
    }
    UE_LOG(LogExtendedFramework, Error, TEXT("GetSightRange: Config == nullptr"));
    return 0;
}




bool UEFPerceptionLibrary::SetSightRange(UAIPerceptionComponent* Perception, float SightRange)
{
    if (Perception)
    {
        if (const auto config = GetPerceptionSenseConfig(Perception, UAISense_Sight::StaticClass()))
        {
            if (const auto sightConfig = Cast<UAISenseConfig_Sight>(config))
            {
                const float LoseOffset = sightConfig->LoseSightRadius - sightConfig->SightRadius;
                sightConfig->SightRadius = SightRange;
                sightConfig->LoseSightRadius = SightRange + LoseOffset;
                
                Perception->RequestStimuliListenerUpdate();
                return true;
            }
        }
    }
    UE_LOG(LogExtendedFramework, Error, TEXT("SetSightRange: Perception or Config == nullptr"));
    return false;
}




float UEFPerceptionLibrary::GetLoseSightRange(UAIPerceptionComponent* Perception)
{
    if (Perception)
    {
        if (const auto config = GetPerceptionSenseConfig(Perception, UAISense_Sight::StaticClass()))
        {
            if (const auto sightConfig = Cast<UAISenseConfig_Sight>(config))
                return sightConfig->LoseSightRadius;
        }
    }
    UE_LOG(LogExtendedFramework, Error, TEXT("GetLoseSightRange: Config == nullptr"));
    return 0;
}




bool UEFPerceptionLibrary::SetLoseSightRange(UAIPerceptionComponent* Perception, float LoseSightRange)
{
    if (Perception)
    {
        if (const auto config = GetPerceptionSenseConfig(Perception, UAISense_Sight::StaticClass()))
        {
            if (const auto sightConfig = Cast<UAISenseConfig_Sight>(config))
            {
                if (LoseSightRange < sightConfig->SightRadius)
                    LoseSightRange = sightConfig->SightRadius;
                
                sightConfig->LoseSightRadius = LoseSightRange;
                Perception->RequestStimuliListenerUpdate();
                return true;
            }
        }
    }
    UE_LOG(LogExtendedFramework, Error, TEXT("SetLoseSightRange: Perception or Config == nullptr"));
    return false;
}




float UEFPerceptionLibrary::GetVisionAngle(UAIPerceptionComponent* Perception)
{
    if (Perception)
    {
        if (const auto config = GetPerceptionSenseConfig(Perception, UAISense_Sight::StaticClass()))
        {
            if (const auto sightConfig = Cast<UAISenseConfig_Sight>(config))
                return sightConfig->PeripheralVisionAngleDegrees * 2;
        }
    }
    UE_LOG(LogExtendedFramework, Error, TEXT("GetVisionAngle: Config == nullptr"));
    return 0;
}




bool UEFPerceptionLibrary::SetVisionAngle(UAIPerceptionComponent* Perception, float VisionAngle)
{
    if (Perception)
    {
        if (const auto config = GetPerceptionSenseConfig(Perception, UAISense_Sight::StaticClass()))
        {
            if (const auto sightConfig = Cast<UAISenseConfig_Sight>(config))
            {
                VisionAngle = VisionAngle / 2.0f;
                sightConfig->PeripheralVisionAngleDegrees = VisionAngle;
                Perception->RequestStimuliListenerUpdate();
                return true;
            }
        }
    }
    UE_LOG(LogExtendedFramework, Error, TEXT("SetVisionAngle: Perception or Config == nullptr"));
    return false;
}




float UEFPerceptionLibrary::GetHearingRange(UAIPerceptionComponent* Perception)
{
    if (Perception)
    {
        if (const auto config = GetPerceptionSenseConfig(Perception, UAISense_Hearing::StaticClass()))
        {
            if (const auto hearConfig = Cast<UAISenseConfig_Hearing>(config))
                return hearConfig->HearingRange;
        }
    }
    UE_LOG(LogExtendedFramework, Error, TEXT("GetHearingRange: Perception or Config == nullptr"));
    return 0;
}




bool UEFPerceptionLibrary::SetHearingRange(UAIPerceptionComponent* Perception, float HearingRange)
{
    if (Perception)
    {
        if (const auto config = GetPerceptionSenseConfig(Perception, UAISense_Hearing::StaticClass()))
        {
            if (const auto hearConfig = Cast<UAISenseConfig_Hearing>(config))
            {
                hearConfig->HearingRange = HearingRange;
                Perception->RequestStimuliListenerUpdate();
                return true;
            }
        }
    }
    UE_LOG(LogExtendedFramework, Error, TEXT("SetHearingRange: Perception or Config == nullptr"));
    return false;
}




float UEFPerceptionLibrary::GetLoSHearingRange(UAIPerceptionComponent* Perception)
{
    if (Perception)
    {
        if (const auto config = GetPerceptionSenseConfig(Perception, UAISense_Hearing::StaticClass()))
        {
            if (const auto hearConfig = Cast<UAISenseConfig_Hearing>(config))
            {
PRAGMA_DISABLE_DEPRECATION_WARNINGS
                return hearConfig->LoSHearingRange;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
            }
        }
    }
    UE_LOG(LogExtendedFramework, Error, TEXT("GetLoSHearingRange: Perception or Config == nullptr"));
    return 0.f;
}




bool UEFPerceptionLibrary::SetLoSHearingRange(UAIPerceptionComponent* Perception, float LoSHearingRange)
{
    if (Perception)
    {
        if (const auto config = GetPerceptionSenseConfig(Perception, UAISense_Hearing::StaticClass()))
        {
            if (const auto hearConfig = Cast<UAISenseConfig_Hearing>(config))
            {
PRAGMA_DISABLE_DEPRECATION_WARNINGS
                hearConfig->LoSHearingRange = LoSHearingRange;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
                Perception->RequestStimuliListenerUpdate();
                return true;
            }
        }
    }
    UE_LOG(LogExtendedFramework, Error, TEXT("SetLoSHearingRange: Perception or Config == nullptr"));
    return false;
}