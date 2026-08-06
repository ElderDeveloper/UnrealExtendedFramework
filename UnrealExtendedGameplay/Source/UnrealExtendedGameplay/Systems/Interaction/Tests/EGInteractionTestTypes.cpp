// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGInteractionTestTypes.h"

#include "Components/SphereComponent.h"

AEGTestInteractable::AEGTestInteractable(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(50.0f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionResponseToAllChannels(ECR_Block);
	SetRootComponent(Collision);

	// The tests drive replication paths directly; a net driver is never stood up.
	bReplicates = false;
}
