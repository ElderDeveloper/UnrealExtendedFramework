// Fill out your copyright notice in the Description page of Project Settings.


#include "EFPerceptionLibrary.h"

#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Sight.h"


#define LOG_Error(Text)  UE_LOG(LogTemp,Error,TEXT(Text)); 


UAISenseConfig* UEFPerceptionLibrary::GetPerceptionSenseConfig(UAIPerceptionComponent* Perception, TSubclassOf<UAISense> SenseClass)
{
    if (!Perception)
    {
        LOG_Error("GetPerceptionSenseConfig: Wrong Sense ID")
        return nullptr;
    }
    
    if(const auto Id = UAISense::GetSenseID(SenseClass))
        return  Perception->GetSenseConfig(Id);
    
    LOG_Error("GetPerceptionSenseConfig: Wrong Sense ID"); return nullptr;

}




bool UEFPerceptionLibrary::ForgetActor(UAIPerceptionComponent* Perception, AActor* Actor)
{
    if (Perception && Actor)
    {
        Perception->ForgetActor(Actor);
        return true;
    }

    LOG_Error("ForgetActor: Perception or Actor == nullptr");
    return false;
}




bool UEFPerceptionLibrary::ForgetAll(UAIPerceptionComponent* Perception)
{
    if (Perception)
    {
        Perception->ForgetAll();
        return true;
    }
    
    LOG_Error("ForgetAll: Perception == nullptr");
    return false;
}




TSubclassOf<UAISense> UEFPerceptionLibrary::GetDominantSense(UAIPerceptionComponent* Perception)
{
    if (Perception)
        return Perception->GetDominantSense();
    
    LOG_Error("GetDominantSense: Perception == nullptr");
    return nullptr;
}




bool UEFPerceptionLibrary::SetDominantSense(UAIPerceptionComponent* Perception, TSubclassOf<UAISense> SenseClass)
{
    if (Perception && SenseClass)
    {
        Perception->SetDominantSense(SenseClass);
        return true;
    }
    LOG_Error("SetDominantSense: Perception or SenseClass == nullptr");
    return false;
}




FAISenseAffiliationFilter UEFPerceptionLibrary::GetDetectionByAffiliation(UAIPerceptionComponent* Perception, TSubclassOf<UAISense> SenseClass)
{
    if (Perception && SenseClass)
    {
        if (const auto config = GetPerceptionSenseConfig(Perception , SenseClass))
        {
            if (const auto sightConfig = Cast<UAISenseConfig_Sight>(config))
                return  sightConfig->DetectionByAffiliation;
            
            if (const auto hearingSenseConfig = Cast<UAISenseConfig_Hearing>(config))
                return hearingSenseConfig->DetectionByAffiliation;
        }
    }
    LOG_Error("GetDetectionByAffiliation: Perception or Sense Config == nullptr");
    return FAISenseAffiliationFilter();
}




bool UEFPerceptionLibrary::SetDetectionByAffiliation(UAIPerceptionComponent* Perception, TSubclassOf<UAISense> SenseClass, bool DetectEnemies, bool DetectNeutrals, bool DetectFriendlies)
{
    if (Perception && SenseClass)
    {
        if (const auto config = GetPerceptionSenseConfig(Perception , SenseClass))
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
    LOG_Error("SetDetectionByAffiliation: Perception or SenseClass == nullptr");
    return false;
}




float UEFPerceptionLibrary::GetMaxAge(UAIPerceptionComponent* Perception, TSubclassOf<UAISense> SenseClass)
{
    if (Perception && SenseClass)
    {
        if (const auto config = GetPerceptionSenseConfig(Perception , SenseClass))
        {
            if (const auto sightConfig = Cast<UAISenseConfig_Sight>(config))
                return   sightConfig->GetMaxAge();
            
            if (const auto hearingSenseConfig = Cast<UAISenseConfig_Hearing>(config))
                return hearingSenseConfig->GetMaxAge();
        }
    }

    LOG_Error("GetMaxAge: Perception or SenseClass == nullptr");
    return -1;
}




bool UEFPerceptionLibrary::SetMaxAge(UAIPerceptionComponent* Perception, TSubclassOf<UAISense> SenseClass, float MaxAge)
{
    if (Perception && SenseClass)
    {
        if (const auto config = GetPerceptionSenseConfig(Perception , SenseClass))
        {
            if (const auto sightConfig = Cast<UAISenseConfig_Sight>(config))
                sightConfig->SetMaxAge(MaxAge);
            
            if (const auto hearingSenseConfig = Cast<UAISenseConfig_Hearing>(config))
                hearingSenseConfig->SetMaxAge(MaxAge);

            return true;
        }
    }
    LOG_Error("SetMaxAge: Perception or SenseClass == nullptr");
    return false;
}




float UEFPerceptionLibrary::GetSightRange(UAIPerceptionComponent* Perception)
{
    if (Perception)
    {
        if (const auto config = GetPerceptionSenseConfig(Perception , UAISense_Sight::StaticClass()))
        {
            if (const auto sightConfig = Cast<UAISenseConfig_Sight>(config))
                return sightConfig->SightRadius;
        }
    }
    LOG_Error("GetSightRange: Config == nullptr");
    return 0;
}




bool UEFPerceptionLibrary::SetSightRange(UAIPerceptionComponent* Perception, float SightRange)
{
    if (Perception)
    {
        if (const auto config = GetPerceptionSenseConfig(Perception , UAISense_Sight::StaticClass()))
        {
            if (const auto sightConfig = Cast<UAISenseConfig_Sight>(config))
            {
                // Save original lose range
                const float LoseRange = sightConfig->LoseSightRadius - sightConfig->SightRadius;
        
                // Apply lose range to new radius of the sight
                sightConfig->LoseSightRadius = sightConfig->SightRadius + LoseRange;
                sightConfig->SightRadius = SightRange;
                
                Perception->RequestStimuliListenerUpdate();
                return true;
            }
        }
    }
    LOG_Error("SetSightRange: Perception or Config == nullptr");
    return false;
}




float UEFPerceptionLibrary::GetLoseSightRange(UAIPerceptionComponent* Perception)
{
    if (Perception)
    {
        if (const auto config = GetPerceptionSenseConfig(Perception , UAISense_Sight::StaticClass()))
        {
            if (const auto sightConfig = Cast<UAISenseConfig_Sight>(config))
                return sightConfig->LoseSightRadius;
        }
    }
    LOG_Error("GetLoseSightRange: Config == nullptr");
    return 0;
}




bool UEFPerceptionLibrary::SetLoseSightRange(UAIPerceptionComponent* Perception, float LoseSightRange)
{
    if (Perception)
    {
        if (const auto config = GetPerceptionSenseConfig(Perception , UAISense_Sight::StaticClass()))
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
    LOG_Error("SetLoseSightRange: Perception or Config == nullptr");
    return false;
}




float UEFPerceptionLibrary::GetVisionAngle(UAIPerceptionComponent* Perception)
{
    if (Perception)
    {
        if (const auto config = GetPerceptionSenseConfig(Perception , UAISense_Sight::StaticClass()))
        {
            if (const auto sightConfig = Cast<UAISenseConfig_Sight>(config))
                return sightConfig->PeripheralVisionAngleDegrees * 2;
        }
    }
    LOG_Error("GetVisionAngle: Config == nullptr");
    return 0;
}




bool UEFPerceptionLibrary::SetVisionAngle(UAIPerceptionComponent* Perception, float VisionAngle)
{
    if (Perception)
    {
        if (const auto config = GetPerceptionSenseConfig(Perception , UAISense_Sight::StaticClass()))
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
    LOG_Error("SetVisionAngle: Perception or Config == nullptr");
    return false;
}




float UEFPerceptionLibrary::GetHearingRange(UAIPerceptionComponent* Perception)
{
    if (Perception)
    {
        if (const auto config  = GetPerceptionSenseConfig(Perception , UAISense_Hearing::StaticClass()))
        {
            if (const auto hearConfig = Cast<UAISenseConfig_Hearing>(config))
                return hearConfig->HearingRange;
        }
    }
    LOG_Error("GetHearingRange: Perception or Config == nullptr");
    return 0;
}




bool UEFPerceptionLibrary::SetHearingRange(UAIPerceptionComponent* Perception, float HearingRange)
{
    if (Perception)
    {
        if (const auto config  = GetPerceptionSenseConfig(Perception , UAISense_Hearing::StaticClass()))
        {
            if (const auto hearConfig = Cast<UAISenseConfig_Hearing>(config))
            {
                hearConfig->HearingRange = HearingRange;
                Perception->RequestStimuliListenerUpdate();
                return true;
            }
        }
    }
    LOG_Error("SetHearingRange: Perception or Config == nullptr");
    return false;
}





float UEFPerceptionLibrary::GetLoSHearingRange(UAIPerceptionComponent* Perception)
{
    if (Perception)
    {
        if (const auto config  = GetPerceptionSenseConfig(Perception , UAISense_Hearing::StaticClass()))
        {
            if (const auto hearConfig = Cast<UAISenseConfig_Hearing>(config))
                return hearConfig->HearingRange;
        }
    }
    LOG_Error("GetLoSHearingRange: Perception or Config == nullptr");
    return false;
}




bool UEFPerceptionLibrary::SetLoSHearingRange(UAIPerceptionComponent* Perception, float LoSHearingRange)
{
    if (Perception)
    {
        if (const auto config  = GetPerceptionSenseConfig(Perception , UAISense_Hearing::StaticClass()))
        {
            if (const auto hearConfig = Cast<UAISenseConfig_Hearing>(config))
            {
                hearConfig->HearingRange = LoSHearingRange;
                Perception->RequestStimuliListenerUpdate();
                return true;
            }
        }
    }
    LOG_Error("SetLoSHearingRange: Perception or Config == nullptr");
    return false;
}