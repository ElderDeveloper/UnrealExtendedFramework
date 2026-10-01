// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Components/SceneComponent.h"
#include "CoreMinimal.h"
#include "EGChessBoardFeedback.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Table/EGChessBoardGeometry.h"

#include "EGChessBoardFeedbackComponent.generated.h"

class UEGChessFeedbackRenderer;
class UEGChessTableStyle;
class UStaticMeshComponent;

/**
 * UEGChessBoardFeedbackComponent
 *
 * Holds the renderer and hands it the marks. Attached to the table's BoardSurface, so a renderer
 * that places components works in surface space. Local only: nothing here replicates, and the
 * marks are rebuilt by whoever owns the viewer's state (the seated player's component, or the
 * table for spectators).
 */
UCLASS(ClassGroup = (Chess))
class UNREALEXTENDEDGAMEPLAY_API UEGChessBoardFeedbackComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UEGChessBoardFeedbackComponent();

	/** Create the renderer from the style (a duplicate of its template) or the default. */
	void InitializeRenderer(const UEGChessTableStyle* Style);

	void ApplyMarks(const FEGChessSquareMarks& Marks, const FEGChessBoardGeometry& Geometry);

	/** The board mesh, for renderers that draw through its material. */
	UStaticMeshComponent* GetBoardMesh() const { return BoardMesh.Get(); }
	void SetBoardMesh(UStaticMeshComponent* InBoardMesh) { BoardMesh = InBoardMesh; }

	const FEGChessSquareMarks& GetCurrentMarks() const { return CurrentMarks; }

	virtual void OnUnregister() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UEGChessFeedbackRenderer> Renderer;

	TWeakObjectPtr<UStaticMeshComponent> BoardMesh;
	FEGChessSquareMarks CurrentMarks;
};
