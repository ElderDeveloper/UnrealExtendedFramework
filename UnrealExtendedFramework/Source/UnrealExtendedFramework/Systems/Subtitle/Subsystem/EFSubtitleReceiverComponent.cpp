// EFSubtitleReceiverComponent.cpp

#include "EFSubtitleReceiverComponent.h"
#include "EFSubtitleLocalSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"

UEFSubtitleReceiverComponent::UEFSubtitleReceiverComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}


void UEFSubtitleReceiverComponent::ClientReceiveSubtitle_Implementation(FEFSubtitleEntry Entry, FEFSubtitleRequest Request)
{
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC) return;

	ULocalPlayer* LP = PC->GetLocalPlayer();
	if (!LP) return;

	if (UEFSubtitleLocalSubsystem* LocalSub = LP->GetSubsystem<UEFSubtitleLocalSubsystem>())
	{
		LocalSub->ReceiveSubtitle(Entry, Request);
	}
}


void UEFSubtitleReceiverComponent::ClientCancelSubtitle_Implementation(int32 RequestId)
{
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC) return;

	ULocalPlayer* LP = PC->GetLocalPlayer();
	if (!LP) return;

	if (UEFSubtitleLocalSubsystem* LocalSub = LP->GetSubsystem<UEFSubtitleLocalSubsystem>())
	{
		LocalSub->CancelSubtitle(RequestId);
	}
}


void UEFSubtitleReceiverComponent::ClientClearAllSubtitles_Implementation()
{
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC) return;

	ULocalPlayer* LP = PC->GetLocalPlayer();
	if (!LP) return;

	if (UEFSubtitleLocalSubsystem* LocalSub = LP->GetSubsystem<UEFSubtitleLocalSubsystem>())
	{
		LocalSub->ClearAllSubtitles();
	}
}
