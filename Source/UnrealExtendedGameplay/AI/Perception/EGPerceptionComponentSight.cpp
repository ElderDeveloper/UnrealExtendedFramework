// Fill out your copyright notice in the Description page of Project Settings.


#include "EGPerceptionComponentSight.h"

#include "GameFramework/Character.h"
#include "Kismet/KismetMathLibrary.h"
#include "UnrealExtendedGameplay/AI/StimuliSource/EGPerceptionStimuliSource.h"
#include "Perception/AISenseConfig_Sight.h"
#include "UnrealExtendedFramework/Data/EFTraceData.h"
#include "UnrealExtendedFramework/Libraries/Trace/EFTraceLibrary.h"




UEGPerceptionComponentSight::UEGPerceptionComponentSight(const FObjectInitializer& ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

	SightCheckBoneNames.Add("pelvis");
	SightCheckBoneNames.Add("spine_03");
	SightCheckBoneNames.Add("lowerarm_l");
	SightCheckBoneNames.Add("lowerarm_r");
	SightCheckBoneNames.Add("hand_l");
	SightCheckBoneNames.Add("hand_r");
	SightCheckBoneNames.Add("head");
	SightCheckBoneNames.Add("calf_r");
	SightCheckBoneNames.Add("calf_l");
	SightCheckBoneNames.Add("foot_r");
	SightCheckBoneNames.Add("foot_l");
	
	ConfigureSense(*SightConfig);
	SetDominantSense(SightConfig->GetSenseImplementation());
	
}




void UEGPerceptionComponentSight::TickDetectPercent(float Strength)
{
	if (HostileStimuliSource && Strength != 0)
	{
		SightDetectionPercent += UKismetMathLibrary::MapRangeClamped( FMath::Clamp(
			HostileStimuliSource->GetActorDetectionStrength() * Strength , (float)0.0 , (float)1.0 ),
			0,
			1,
			DetectionFillRateMin ,
			DetectionFillRateMax);

		if (SightDetectionPercent >= 100 )
			SightDetectionPercent = 100;
	}
	else
	{
		
	}
}




void UEGPerceptionComponentSight::CheckCharacterVisibility()
{
	float Strength;
	int32 BoneHitCount = 0;
	
	FRotator Rotator;
	GetOwner()->GetActorEyesViewPoint(SightLineTrace.Start , Rotator);

	if (SightCheckBoneNames.Num() == 0)
	{
		SightLineTrace.End = DetectedHostileAsCharacter->GetActorLocation();
		if(UEFTraceLibrary::ExtendedLineTraceSingle(GetWorld(),SightLineTrace))
		{
			if (SightLineTrace.GetHitActor() == DetectedHostileAsCharacter)
			{
				Strength = 1; 
				TickDetectPercent(Strength);
				return;
			}
		}
	}
	
	for (const auto i : SightCheckBoneNames)
	{
		SightLineTrace.End = DetectedHostileAsCharacter->GetMesh()->GetSocketLocation(i);

		if(UEFTraceLibrary::ExtendedLineTraceSingle(GetWorld(),SightLineTrace))
		{
			if (SightLineTrace.GetHitActor() == DetectedHostileAsCharacter)
				BoneHitCount += 1; 
		}
	}

	const float BoneNameCountClamp = FMath::Clamp(SightCheckBoneNames.Num() * 40 / 100 , 1 , SightCheckBoneNames.Num() );

	if (BoneHitCount < BoneNameCountClamp)
		Strength = 0;
	else
		Strength = UKismetMathLibrary::MapRangeClamped(BoneHitCount ,BoneNameCountClamp ,SightCheckBoneNames.Num() ,0.1 , 1);
			
	TickDetectPercent(Strength);
	
}




void UEGPerceptionComponentSight::CheckActorVisibility()
{
	float Strength = 0;
	FRotator Rotator;
	GetOwner()->GetActorEyesViewPoint(SightLineTrace.Start , Rotator);
	SightLineTrace.End = DetectedHostile->GetActorLocation();
	
	if(UEFTraceLibrary::ExtendedLineTraceSingle(GetWorld(),SightLineTrace))
	{
		if (SightLineTrace.GetHitActor() == DetectedHostile)
			Strength = 1; 
	}
	TickDetectPercent(Strength);
}




void UEGPerceptionComponentSight::TickComponent(float DeltaTime, ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if(DetectedHostile)
	{
		if (DetectedHostileAsCharacter)
		{
			CheckCharacterVisibility();
			return;
		}
		CheckActorVisibility();
	}
}




void UEGPerceptionComponentSight::BeginPlay()
{
	Super::BeginPlay();
	SightLineTrace.bIgnoreSelf = true;
	SightLineTrace.ActorsToIgnore.Add(GetOwner());
}




void UEGPerceptionComponentSight::EGTargetPerceptionStateChanged(EFPerceptionState State, EFPerceptionFaction Faction,AActor* Source, FAIStimulus Stimulus, bool IsSensed)
{
	if (Faction == Hostile)
	{
		DetectedHostile = Source;
		DetectedHostileStimulus = Stimulus;

		if (const auto PerceptionStimuli = DetectedHostile->FindComponentByClass<UEGPerceptionStimuliSource>())
			HostileStimuliSource = PerceptionStimuli;
	}
}
