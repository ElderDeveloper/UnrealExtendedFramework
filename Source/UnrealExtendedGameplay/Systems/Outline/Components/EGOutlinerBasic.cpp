// Fill out your copyright notice in the Description page of Project Settings.


#include "EGOutlinerBasic.h"

#include "Engine/RendererSettings.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealExtendedGameplay/Systems/Outline/Actors/EGOutlinerWorldActor.h"


// Sets default values for this component's properties
UEGOutlinerBasic::UEGOutlinerBasic()
{
	PrimaryComponentTick.bCanEverTick = true;
}


// Called when the game starts
void UEGOutlinerBasic::BeginPlay()
{
	Super::BeginPlay();
	
	if (URendererSettings* Settings = GetMutableDefault<URendererSettings>())
	{
		if (Settings->CustomDepthStencil != ECustomDepthStencil::EnabledWithStencil)
		{
			Settings->CustomDepthStencil = ECustomDepthStencil::EnabledWithStencil;
			Settings->SaveConfig();
			UE_LOG(LogTemp , Warning , TEXT("CustomDepthStencil was not enabled in the project settings. It has been enabled now."))
		}
	}


	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AEGOutlinerWorldActor::StaticClass(), FoundActors);

	if (FoundActors.IsEmpty())
	{
		OutlinerWorldActor = GetWorld()->SpawnActor<AEGOutlinerWorldActor>();
	}
	else
	{
		OutlinerWorldActor = Cast<AEGOutlinerWorldActor>(FoundActors[0]);
	}

	if (bOutlineOnBeginPlay)
	{
		ServerSwitchOutlineOnActor(GetOwner(), OwnerOutlineBehavior, true, OwnerOutlineType);
	}
}

void UEGOutlinerBasic::SwitchOutlineOnActor(AActor* ActorToOutline, EGOutlineBehavior OutlineBehavior, bool bEnable,EGOutlineType OutlineType)
{
	Super::SwitchOutlineOnActor(ActorToOutline, OutlineBehavior, bEnable, OutlineType);

	if (!ActorToOutline)
	{
		return;
	}

	const int32 CustomStencilValue = OutlineType == EGOutlineType::Primary ? 1 :  OutlineType == EGOutlineType::Secondary ? 3 : 5;

	if (OutlineBehavior == EGOutlineBehavior::OnAllFoundPrimitiveComponents)
	{
		TArray<UPrimitiveComponent*> PrimitiveComponents;
		ActorToOutline->GetComponents<UPrimitiveComponent>( PrimitiveComponents , true);
		for (const auto Primitive : PrimitiveComponents)
		{
			Primitive->SetRenderCustomDepth(bEnable);
			Primitive->SetCustomDepthStencilValue(CustomStencilValue);
		}
		return;
	}

	if (OutlineBehavior == EGOutlineBehavior::OnFirstFoundPrimitiveComponent)
	{
		if (const auto Component = ActorToOutline->GetComponentByClass(UPrimitiveComponent::StaticClass()))
		{
			if (const auto Primitive = Cast<UPrimitiveComponent>(Component))
			{
				Primitive->SetRenderCustomDepth(bEnable);
				Primitive->SetCustomDepthStencilValue(CustomStencilValue);
				return;
			}
		}
	}

	if (OutlineBehavior == EGOutlineBehavior::OnAllFoundPrimitiveComponentsByTag)
	{
		const auto ComponentArray = ActorToOutline->GetComponentsByTag(UPrimitiveComponent::StaticClass(), FName("Outline"));
		for (const auto Component : ComponentArray)
		{
			if (const auto Primitive = Cast<UPrimitiveComponent>(Component))
			{
				Primitive->SetRenderCustomDepth(bEnable);
				Primitive->SetCustomDepthStencilValue(CustomStencilValue);
			}
		}
		return;
	}


	if (OutlineBehavior == EGOutlineBehavior::OnFirstFoundPrimitiveComponentByTag)
	{
		const auto ComponentArray = ActorToOutline->GetComponentsByTag(UPrimitiveComponent::StaticClass(), FName("Outline"));
		if (ComponentArray.Num() > 0)
		{
			if (const auto Primitive = Cast<UPrimitiveComponent>(ComponentArray[0]))
			{
				Primitive->SetRenderCustomDepth(bEnable);
				Primitive->SetCustomDepthStencilValue(CustomStencilValue);
			}
		}
	}
}

void UEGOutlinerBasic::ServerSwitchOutlineOnActor(AActor* ActorToOutline, EGOutlineBehavior EgOutlineBehavior,bool bEnable, EGOutlineType OutlineType)
{
	Super::ServerSwitchOutlineOnActor(ActorToOutline, EgOutlineBehavior, bEnable, OutlineType);

	ActorToOutline->SetReplicates(true);
	SwitchOutlineOnActor(ActorToOutline, EgOutlineBehavior, bEnable, OutlineType);
}

