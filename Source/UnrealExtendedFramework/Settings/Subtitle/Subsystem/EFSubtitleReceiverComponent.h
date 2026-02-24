// EFSubtitleReceiverComponent.h — RPC bridge on PlayerController for multiplayer subtitle delivery
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UnrealExtendedFramework/Settings/Subtitle/Data/EFSubtitleData.h"
#include "EFSubtitleReceiverComponent.generated.h"

/**
 * Lightweight component added to each PlayerController.
 * Bridges Client RPCs to the UEFSubtitleLocalSubsystem (which cannot receive RPCs directly).
 */
UCLASS(ClassGroup=(Subtitle), meta=(BlueprintSpawnableComponent))
class UNREALEXTENDEDFRAMEWORK_API UEFSubtitleReceiverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEFSubtitleReceiverComponent();

	// Server → Client: deliver a resolved subtitle
	UFUNCTION(Client, Reliable)
	void ClientReceiveSubtitle(FEFSubtitleEntry Entry, FEFSubtitleRequest Request);

	// Server → Client: cancel a subtitle
	UFUNCTION(Client, Reliable)
	void ClientCancelSubtitle(int32 RequestId);

	// Server → Client: clear all subtitles
	UFUNCTION(Client, Reliable)
	void ClientClearAllSubtitles();
};
