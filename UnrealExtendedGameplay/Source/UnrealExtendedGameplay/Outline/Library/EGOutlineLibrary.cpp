// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGOutlineLibrary.h"
#include "UnrealExtendedGameplay/Outline/Components/EGOutlinerAbstract.h"


UEGOutlinerAbstract* UEGOutlineLibrary::GetOutlinerManagerFromActor(const AActor* Actor)
{
	if (Actor)
	{
		return Cast<UEGOutlinerAbstract>(Actor->GetComponentByClass(UEGOutlinerAbstract::StaticClass()));
	}
	return nullptr;
}


UEGOutlinerAbstract* UEGOutlineLibrary::GetOutlinerManagerFromController(const AController* Controller)
{
	if (Controller && Controller->GetPawn())
	{
		return Cast<UEGOutlinerAbstract>(Controller->GetPawn()->GetComponentByClass(UEGOutlinerAbstract::StaticClass()));
	}
	return nullptr;
}


UEGOutlinerAbstract* UEGOutlineLibrary::GetOutlinerManagerFromComponent(const UActorComponent* Component)
{
	if (Component && Component->GetOwner())
	{
		return Cast<UEGOutlinerAbstract>(Component->GetOwner()->GetComponentByClass(UEGOutlinerAbstract::StaticClass()));
	}
	return nullptr;
}
