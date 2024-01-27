// Fill out your copyright notice in the Description page of Project Settings.

#include "EGOutlinerWorldActor.h"
#include "Components/PostProcessComponent.h"


AEGOutlinerWorldActor::AEGOutlinerWorldActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	OutlinerPostProcessComponent = CreateDefaultSubobject<UPostProcessComponent>(TEXT("OutlinerPostProcessComponent"));
	OutlinerPostProcessComponent->bUnbound = true;
	OutlinerPostProcessComponent->bEnabled = true;
	RootComponent = OutlinerPostProcessComponent;

	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FObjectFinder<UMaterialInstance> SceneDepthMaterialClass(TEXT("/UnrealExtendedFramework/Systems/Outline/Materials/MI_EFOutliner.MI_EFOutliner"));
	if (SceneDepthMaterialClass.Object != NULL)
	{
		SceneDepthMaterial = SceneDepthMaterialClass.Object;
	}

	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FObjectFinder<UMaterialInstance> NonSceneDepthMaterialClass(TEXT("/UnrealExtendedFramework/Systems/Outline/Materials/MI_EFOutliner_SceneDepth.MI_EFOutliner_SceneDepth"));
	if (NonSceneDepthMaterialClass.Object != NULL)
	{
		NonSceneDepthMaterial = NonSceneDepthMaterialClass.Object;
	}

	OutlinerPostProcessComponent->AddOrUpdateBlendable(SceneDepthMaterial);
}



void AEGOutlinerWorldActor::SetUsingSceneDepth(bool bUseSceneDepth)
{
	const FWeightedBlendable SceneBlendable = FWeightedBlendable(1, SceneDepthMaterial);
	const FWeightedBlendable NonSceneBlendable = FWeightedBlendable(1, NonSceneDepthMaterial);

	if (OutlinerPostProcessComponent->Settings.WeightedBlendables.Array.IsValidIndex(0))
	{
		OutlinerPostProcessComponent->Settings.WeightedBlendables.Array[0] = bUseSceneDepth ? SceneBlendable : NonSceneBlendable;
	}
}

