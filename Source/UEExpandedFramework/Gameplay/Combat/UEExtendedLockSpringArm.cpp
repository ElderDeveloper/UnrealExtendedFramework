// Fill out your copyright notice in the Description page of Project Settings.


#include "UEExtendedLockSpringArm.h"


// Sets default values for this component's properties
UUEExtendedLockSpringArm::UUEExtendedLockSpringArm()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UUEExtendedLockSpringArm::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UUEExtendedLockSpringArm::TickComponent(float DeltaTime, ELevelTick TickType,
                                             FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

