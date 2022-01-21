// Fill out your copyright notice in the Description page of Project Settings.


#include "EGLadder.h"
#include "EGLadderComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"


// Sets default values
AEGLadder::AEGLadder()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	
	LadderMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LadderMeshComponent"));
	LadderMeshComponent->SetStaticMesh(LadderMesh);
	RootComponent = LadderMeshComponent;

	LadderEntryArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("LadderEntryArrow"));
	LadderEntryArrow ->SetupAttachment(RootComponent);

	LadderExitArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("LadderExitArrow"));
	LadderExitArrow ->SetupAttachment(RootComponent);

	LadderEnterBox = CreateDefaultSubobject<UBoxComponent>(TEXT("LadderEnterBox"));
	LadderEnterBox ->SetupAttachment(RootComponent);
	LadderEnterBox ->SetBoxExtent(FVector(42,42,42));


}



// Called when the game starts or when spawned
void AEGLadder::BeginPlay()
{
	Super::BeginPlay();

	LadderEnterBox->OnComponentBeginOverlap.AddDynamic(this , &AEGLadder::OnLadderEnterBoxPlayerEnter);
	LadderEnterBox->OnComponentEndOverlap.AddDynamic(this , &AEGLadder::OnLadderEnterBoxPlayerLeave);
}



void AEGLadder::OnLadderEnterBoxPlayerEnter(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (const auto Character = Cast<ACharacter>(OtherActor))
	{
		if (const auto LadderComponent = Cast<UEGLadderComponent>(Character->FindComponentByClass<UEGLadderComponent>()))
		{
			PlayerRef = Character;
			FoundLadderComponent = LadderComponent;

			FoundLadderComponent->SetIsReadyForEnterLadder(true, this);
			LadderEnterCollisionStateChanged.Broadcast(true , Character);
		}
	}
}



void AEGLadder::OnLadderEnterBoxPlayerLeave(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (const auto Character = Cast<ACharacter>(OtherActor))
	{
		if (const auto LadderComponent = Cast<UEGLadderComponent>(Character->FindComponentByClass<UEGLadderComponent>()))
		{
			if (FoundLadderComponent)
			{
				FoundLadderComponent->SetIsReadyForEnterLadder(false , this);
				FoundLadderComponent = nullptr;
			}
			
			PlayerRef = nullptr;
			LadderEnterCollisionStateChanged.Broadcast(false , Character);
		}
	}
}



