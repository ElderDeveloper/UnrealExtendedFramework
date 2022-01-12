// Fill out your copyright notice in the Description page of Project Settings.


#include "UEExtendedLadder.h"

#include "UEExtendedLadderComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"


// Sets default values
AUEExtendedLadder::AUEExtendedLadder()
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

	LadderClimbBox = CreateDefaultSubobject<UBoxComponent>(TEXT("LadderClimbBox"));
	LadderClimbBox ->SetupAttachment(RootComponent);
	LadderClimbBox ->SetBoxExtent(FVector(42,42,12));
}

// Called when the game starts or when spawned
void AUEExtendedLadder::BeginPlay()
{
	Super::BeginPlay();

	LadderEnterBox->OnComponentBeginOverlap.AddDynamic(this , &AUEExtendedLadder::OnLadderEnterBoxPlayerEnter);
	LadderEnterBox->OnComponentEndOverlap.AddDynamic(this , &AUEExtendedLadder::OnLadderEnterBoxPlayerLeave);

	LadderClimbBox->OnComponentBeginOverlap.AddDynamic(this , &AUEExtendedLadder::AUEExtendedLadder::OnLadderClimbBoxPlayerEnter);
	LadderClimbBox->OnComponentEndOverlap.AddDynamic(this , &AUEExtendedLadder::OnLadderClimbBoxPlayerLeave);
	
}



void AUEExtendedLadder::OnLadderEnterBoxPlayerEnter(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (const auto Character = Cast<ACharacter>(OtherActor))
	{
		if (const auto LadderComponent = Cast<UUEExtendedLadderComponent>(Character->FindComponentByClass<UUEExtendedLadderComponent>()))
		{
			PlayerRef = Character;
			FoundLadderComponent = LadderComponent;

			FoundLadderComponent->SetIsReadyForEnterLadder(true, this);
		}
	}
}

void AUEExtendedLadder::OnLadderEnterBoxPlayerLeave(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (const auto Character = Cast<ACharacter>(OtherActor))
	{
		if (const auto LadderComponent = Cast<UUEExtendedLadderComponent>(Character->FindComponentByClass<UUEExtendedLadderComponent>()))
		{
			PlayerRef = nullptr;
			FoundLadderComponent = nullptr;
			FoundLadderComponent->SetIsReadyForEnterLadder(false , nullptr);
		}
	}
}




void AUEExtendedLadder::OnLadderClimbBoxPlayerEnter(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor == PlayerRef && FoundLadderComponent)
	{
		FoundLadderComponent->SetIsReadyForClimbLeaveLadder(true);
	}
}



void AUEExtendedLadder::OnLadderClimbBoxPlayerLeave(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (OtherActor == PlayerRef && FoundLadderComponent)
	{
		FoundLadderComponent->SetIsReadyForClimbLeaveLadder(false);
	}
}

