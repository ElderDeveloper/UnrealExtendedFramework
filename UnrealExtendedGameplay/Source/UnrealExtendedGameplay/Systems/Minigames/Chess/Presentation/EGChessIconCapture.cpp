// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGChessIconCapture.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EGChessPieceVisuals.h"
#include "GameFramework/Actor.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "TimerManager.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Table/EGChessTableActor.h"

namespace EGChessIconCapturePrivate
{
	int32 Key(EEGChessPieceType Type, EEGChessColor Color)
	{
		return static_cast<int32>(Type) * 2 + (Color == EEGChessColor::Black ? 1 : 0);
	}

	/** Far below anything a level is likely to contain. */
	const FVector StageOrigin(0.0f, 0.0f, -500000.0f);
}

void UEGChessIconCapture::Initialize(AEGChessTableActor* InTable)
{
	Table = InTable;
}

UTexture* UEGChessIconCapture::GetIcon(EEGChessPieceType Type, EEGChessColor Color)
{
	if (Type == EEGChessPieceType::None)
	{
		return nullptr;
	}
	if (TObjectPtr<UTextureRenderTarget2D>* Found = Targets.Find(EGChessIconCapturePrivate::Key(Type, Color)))
	{
		return *Found;
	}

	AEGChessTableActor* T = Table.Get();
	UWorld* World = T ? T->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return nullptr;
	}

	// First request: build every icon at once and render them next frame.
	const EEGChessPieceType Types[6] = { EEGChessPieceType::Pawn, EEGChessPieceType::Knight, EEGChessPieceType::Bishop, EEGChessPieceType::Rook, EEGChessPieceType::Queen, EEGChessPieceType::King };
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Stage = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(EGChessIconCapturePrivate::StageOrigin), Params);
	if (!Stage)
	{
		return nullptr;
	}
	USceneComponent* StageRoot = NewObject<USceneComponent>(Stage, TEXT("Root"));
	Stage->SetRootComponent(StageRoot);
	StageRoot->RegisterComponent();
	StageRoot->SetWorldLocation(EGChessIconCapturePrivate::StageOrigin);

	int32 Slot = 0;
	for (const EEGChessPieceType PieceType : Types)
	{
		for (const EEGChessColor PieceColor : { EEGChessColor::White, EEGChessColor::Black })
		{
			const FVector Offset(Slot * 1000.0f, 0.0f, 0.0f);
			++Slot;

			UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Stage);
			Mesh->SetupAttachment(StageRoot);
			Mesh->SetRelativeLocation(Offset);
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Mesh->SetVisibleInSceneCaptureOnly(true);
			Mesh->SetCastShadow(false);
			Mesh->RegisterComponent();
			UEGChessPieceVisuals::ApplyPieceAppearance(Mesh, T->GetPieceSet(), PieceType, PieceColor);

			const FBoxSphereBounds Bounds = Mesh->Bounds;
			const float Size = FMath::Max(1.0f, static_cast<float>(FMath::Max3(Bounds.BoxExtent.X, Bounds.BoxExtent.Y, Bounds.BoxExtent.Z)) * 2.4f);

			UTextureRenderTarget2D* Target = UKismetRenderingLibrary::CreateRenderTarget2D(this, Resolution, Resolution, RTF_RGBA8, FLinearColor::Transparent);
			Targets.Add(EGChessIconCapturePrivate::Key(PieceType, PieceColor), Target);

			USceneCaptureComponent2D* Capture = NewObject<USceneCaptureComponent2D>(Stage);
			Capture->SetupAttachment(StageRoot);
			Capture->ProjectionType = ECameraProjectionMode::Orthographic;
			Capture->OrthoWidth = Size;
			Capture->CaptureSource = SCS_BaseColor;
			Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
			Capture->ShowOnlyComponents.Add(Mesh);
			Capture->bCaptureEveryFrame = false;
			Capture->bCaptureOnMovement = false;
			Capture->TextureTarget = Target;
			Capture->SetWorldLocationAndRotation(Bounds.Origin + FVector(Size * 2.0f, 0.0f, 0.0f), FRotator(0.0f, 180.0f, 0.0f));
			Capture->RegisterComponent();
		}
	}

	if (!bCaptureScheduled)
	{
		bCaptureScheduled = true;
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UEGChessIconCapture::CaptureAll));
	}

	TObjectPtr<UTextureRenderTarget2D>* Created = Targets.Find(EGChessIconCapturePrivate::Key(Type, Color));
	return Created ? Created->Get() : nullptr;
}

void UEGChessIconCapture::CaptureAll()
{
	bCaptureScheduled = false;
	if (!Stage)
	{
		return;
	}
	TArray<USceneCaptureComponent2D*> Captures;
	Stage->GetComponents(Captures);
	for (USceneCaptureComponent2D* Capture : Captures)
	{
		Capture->CaptureScene();
	}
	// The captures are recorded; the stage is no longer needed.
	Stage->Destroy();
	Stage = nullptr;
}

void UEGChessIconCapture::Shutdown()
{
	if (Stage)
	{
		Stage->Destroy();
		Stage = nullptr;
	}
	Targets.Reset();
}
