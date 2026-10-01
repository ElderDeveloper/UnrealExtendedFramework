// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Core/EGChessTypes.h"

#include "EGChessIconCapture.generated.h"

class AActor;
class AEGChessTableActor;
class UTexture;
class UTextureRenderTarget2D;

/**
 * Renders piece icons from the piece meshes when the piece set has none, so the HUD's captured
 * pieces and the promotion picker have pictures without a single authored texture.
 *
 * One small capture per piece and colour, each seeing only its own mesh, placed far below the
 * world and invisible to normal views. Rendered once, on the frame after the request (the mesh's
 * render proxy has to exist first); until then the icon is blank.
 */
UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGChessIconCapture : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(AEGChessTableActor* InTable);

	/** The rendered icon, created on first request. */
	UTexture* GetIcon(EEGChessPieceType Type, EEGChessColor Color);

	void Shutdown();

	UPROPERTY(EditAnywhere, Category = "Chess")
	int32 Resolution = 128;

private:
	void CaptureAll();

	TWeakObjectPtr<AEGChessTableActor> Table;

	UPROPERTY(Transient)
	TObjectPtr<AActor> Stage;

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UTextureRenderTarget2D>> Targets;

	bool bCaptureScheduled = false;
};
