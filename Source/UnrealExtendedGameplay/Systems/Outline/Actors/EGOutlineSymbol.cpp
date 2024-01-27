// Fill out your copyright notice in the Description page of Project Settings.


#include "EGOutlineSymbol.h"

#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"


// Sets default values
AEGOutlineSymbol::AEGOutlineSymbol()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	RootComponent = RootScene;
	
	SymbolMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SymbolMeshComponent"));
	SymbolMeshComponent->SetupAttachment(RootScene);
}

void AEGOutlineSymbol::OnRep_Color()
{
}

void AEGOutlineSymbol::InitializeSymbol(FEGOutlineSymbolSettings NewSymbolSettings, bool bShouldReplicate)
{
	SymbolSettings = NewSymbolSettings;
	ShouldReplicate = bShouldReplicate;
	SetReplicates( ShouldReplicate );
	SymbolMeshComponent->SetStaticMesh(SymbolSettings.SymbolMesh);
	
	if (SymbolSettings.AttachToActor && SymbolSettings.ParentActor)
	{
		AttachToActor( SymbolSettings.ParentActor , FAttachmentTransformRules::KeepRelativeTransform);
	}
}

void AEGOutlineSymbol::SetSymbolColor(FLinearColor NewColor)
{
	SymbolColor = NewColor;
}


// Called every frame
void AEGOutlineSymbol::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	if (SymbolSettings.bShouldRotate)
	{
		FRotator rot = GetActorRotation();
		rot.Yaw += DeltaTime * SymbolSettings.RotationSpeed;
		if (rot.Yaw > 360)
		{
			rot.Yaw = rot.Yaw - 360;
		}
		SetActorRotation(rot);
	}
	
	if (SymbolSettings.bUseUpAndDownMovement)
	{
		CurrentUpDownTime = CurrentUpDownTime + DeltaTime * SymbolSettings.UpAndDownSpeed;
		if (CurrentUpDownTime > 360)
		{
			CurrentUpDownTime = CurrentUpDownTime - 360;
		}
		const auto SineValue = UKismetMathLibrary::Sin(CurrentUpDownTime);
		const auto UpValue = UKismetMathLibrary::MapRangeClamped(SineValue , -1.0f , 1.0f , SymbolSettings.UpDownRange.X , SymbolSettings.UpDownRange.Y);
		SymbolMeshComponent->SetRelativeLocation( FVector(0,0,UpValue));
	}
}


void AEGOutlineSymbol::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEGOutlineSymbol, SymbolSettings);
	DOREPLIFETIME(AEGOutlineSymbol, SymbolColor);
}