// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGChessBoardFeedbackComponent.h"

#include "Components/DecalComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "EGChessFeedbackRenderer.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Data/EGChessTableStyle.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Presentation/EGChessEngineAssets.h"

// ---------------------------------------------------------------------------------------------
// Component
// ---------------------------------------------------------------------------------------------

UEGChessBoardFeedbackComponent::UEGChessBoardFeedbackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	for (uint16& Flags : CurrentMarks)
	{
		Flags = 0;
	}
}

void UEGChessBoardFeedbackComponent::InitializeRenderer(const UEGChessTableStyle* Style)
{
	if (Renderer)
	{
		Renderer->Shutdown();
		Renderer = nullptr;
	}

	if (Style && Style->Renderer)
	{
		Renderer = DuplicateObject<UEGChessFeedbackRenderer>(Style->Renderer, this);
	}
	if (!Renderer)
	{
		Renderer = NewObject<UEGChessFeedbackRenderer_Instanced>(this);
	}
	Renderer->Initialize(this, Style);
}

void UEGChessBoardFeedbackComponent::ApplyMarks(const FEGChessSquareMarks& Marks, const FEGChessBoardGeometry& Geometry)
{
	CurrentMarks = Marks;
	if (Renderer)
	{
		Renderer->Apply(Marks, Geometry);
	}
}

void UEGChessBoardFeedbackComponent::OnUnregister()
{
	if (Renderer)
	{
		Renderer->Shutdown();
	}
	Super::OnUnregister();
}

// ---------------------------------------------------------------------------------------------
// Renderer base
// ---------------------------------------------------------------------------------------------

void UEGChessFeedbackRenderer::Initialize(UEGChessBoardFeedbackComponent* InOwner, const UEGChessTableStyle* InStyle)
{
	OwnerComponent = InOwner;
	Style = InStyle;
}

UEGChessFeedbackRenderer::FResolvedMarker UEGChessFeedbackRenderer::ResolveMarker(EEGChessMarkType Type) const
{
	FResolvedMarker Out;
	const FEGChessMarkerVisual* Visual = Style.IsValid() ? Style->FindMarker(Type) : nullptr;
	if (Visual)
	{
		Out.Mesh = EGChessEngineAssets::Resolve(Visual->Mesh);
		Out.Material = EGChessEngineAssets::Resolve(Visual->Material);
		Out.Color = Visual->Color;
		Out.Scale = Visual->Scale;
		Out.HeightOffset = Visual->HeightOffset;
	}
	else
	{
		// A table with no style still needs to be readable.
		switch (Type)
		{
		case EEGChessMarkType::Move: Out.Color = FLinearColor(0.1f, 0.1f, 0.1f); Out.Scale = 0.3f; break;
		case EEGChessMarkType::Capture: Out.Color = FLinearColor(0.9f, 0.3f, 0.2f); Out.Scale = 0.9f; break;
		case EEGChessMarkType::Check:
		case EEGChessMarkType::Illegal: Out.Color = FLinearColor(0.9f, 0.1f, 0.1f); break;
		case EEGChessMarkType::LastMoveFrom:
		case EEGChessMarkType::LastMoveTo: Out.Color = FLinearColor(0.9f, 0.8f, 0.2f); Out.Scale = 0.95f; break;
		case EEGChessMarkType::Cursor: Out.Color = FLinearColor::White; Out.Scale = 0.5f; break;
		default: Out.Color = FLinearColor(0.3f, 0.6f, 1.0f); Out.Scale = 0.95f; break;
		}
		Out.HeightOffset = 0.05f + 0.01f * static_cast<float>(Type);
	}
	if (!Out.Mesh)
	{
		Out.Mesh = EGChessEngineAssets::Plane();
	}
	if (!Out.Material)
	{
		Out.Material = EGChessEngineAssets::BasicMaterial();
		Out.bUsesFallbackMaterial = true;
	}
	return Out;
}

// ---------------------------------------------------------------------------------------------
// Instanced
// ---------------------------------------------------------------------------------------------

void UEGChessFeedbackRenderer_Instanced::Initialize(UEGChessBoardFeedbackComponent* InOwner, const UEGChessTableStyle* InStyle)
{
	Super::Initialize(InOwner, InStyle);
	Shutdown();

	if (!InOwner || !InOwner->GetOwner())
	{
		return;
	}

	Layers.SetNum(EGChessMarks::NumTypes);
	for (int32 Index = 0; Index < EGChessMarks::NumTypes; ++Index)
	{
		const FResolvedMarker Marker = ResolveMarker(static_cast<EEGChessMarkType>(Index));
		UInstancedStaticMeshComponent* Layer = NewObject<UInstancedStaticMeshComponent>(InOwner->GetOwner(), NAME_None, RF_Transient);
		Layer->SetStaticMesh(Marker.Mesh);
		Layer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Layer->SetCastShadow(false);
		Layer->SetCanEverAffectNavigation(false);
		Layer->bReceivesDecals = false;
		Layer->SetupAttachment(InOwner);
		Layer->RegisterComponent();

		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Marker.Material, Layer);
		if (Material)
		{
			Material->SetVectorParameterValue(TEXT("Color"), Marker.Color);
			Layer->SetMaterial(0, Material);
		}
		Layers[Index] = Layer;
	}
	bHasApplied = false;
}

void UEGChessFeedbackRenderer_Instanced::Apply(const FEGChessSquareMarks& Marks, const FEGChessBoardGeometry& Geometry)
{
	const bool bGeometryChanged = !bHasApplied || Geometry.WhiteSeat != LastWhiteSeat || !FMath::IsNearlyEqual(Geometry.SquareSize, LastSquareSize);

	for (int32 TypeIndex = 0; TypeIndex < Layers.Num(); ++TypeIndex)
	{
		UInstancedStaticMeshComponent* Layer = Layers[TypeIndex];
		if (!Layer)
		{
			continue;
		}
		const EEGChessMarkType Type = static_cast<EEGChessMarkType>(TypeIndex);

		bool bChanged = bGeometryChanged;
		for (int32 Square = 0; Square < 64 && !bChanged; ++Square)
		{
			bChanged = EGChessMarks::Has(Marks[Square], Type) != EGChessMarks::Has(LastMarks[Square], Type);
		}
		if (!bChanged)
		{
			continue;
		}

		const FResolvedMarker Marker = ResolveMarker(Type);
		const FBoxSphereBounds Bounds = Marker.Mesh ? Marker.Mesh->GetBounds() : FBoxSphereBounds(FVector::ZeroVector, FVector(50.0f), 50.0f);
		const float MeshSize = FMath::Max(1.0f, static_cast<float>(FMath::Max(Bounds.BoxExtent.X, Bounds.BoxExtent.Y) * 2.0f));
		const float Scale = Geometry.SquareSize * Marker.Scale / MeshSize;

		Layer->ClearInstances();
		for (int32 Square = 0; Square < 64; ++Square)
		{
			if (!EGChessMarks::Has(Marks[Square], Type))
			{
				continue;
			}
			const FVector Location = Geometry.SquareToLocal(Square) + FVector(0.0f, 0.0f, Marker.HeightOffset);
			Layer->AddInstance(FTransform(FQuat::Identity, Location, FVector(Scale, Scale, 1.0f)), false);
		}
	}

	LastMarks = Marks;
	LastWhiteSeat = Geometry.WhiteSeat;
	LastSquareSize = Geometry.SquareSize;
	bHasApplied = true;
}

void UEGChessFeedbackRenderer_Instanced::Shutdown()
{
	for (UInstancedStaticMeshComponent* Layer : Layers)
	{
		if (Layer)
		{
			Layer->DestroyComponent();
		}
	}
	Layers.Reset();
	bHasApplied = false;
}

// ---------------------------------------------------------------------------------------------
// Decal
// ---------------------------------------------------------------------------------------------

UDecalComponent* UEGChessFeedbackRenderer_Decal::AcquireDecal(int32 Index)
{
	UEGChessBoardFeedbackComponent* Owner = GetOwnerComponent();
	if (!Owner || !Owner->GetOwner())
	{
		return nullptr;
	}
	while (Pool.Num() <= Index)
	{
		UDecalComponent* Decal = NewObject<UDecalComponent>(Owner->GetOwner(), NAME_None, RF_Transient);
		Decal->SetupAttachment(Owner);
		Decal->RegisterComponent();
		Pool.Add(Decal);
	}
	return Pool[Index];
}

void UEGChessFeedbackRenderer_Decal::Apply(const FEGChessSquareMarks& Marks, const FEGChessBoardGeometry& Geometry)
{
	int32 Used = 0;
	for (int32 TypeIndex = 0; TypeIndex < EGChessMarks::NumTypes; ++TypeIndex)
	{
		const EEGChessMarkType Type = static_cast<EEGChessMarkType>(TypeIndex);
		FResolvedMarker Marker;
		bool bResolved = false;
		for (int32 Square = 0; Square < 64; ++Square)
		{
			if (!EGChessMarks::Has(Marks[Square], Type))
			{
				continue;
			}
			if (!bResolved)
			{
				Marker = ResolveMarker(Type);
				bResolved = true;
			}
			UDecalComponent* Decal = AcquireDecal(Used++);
			if (!Decal)
			{
				return;
			}
			const float Half = Geometry.SquareSize * Marker.Scale * 0.5f;
			Decal->DecalSize = FVector(DecalDepth, Half, Half);
			Decal->SetRelativeLocation(Geometry.SquareToLocal(Square));
			Decal->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
			Decal->SetSortOrder(TypeIndex);
			if (Decal->GetDecalMaterial() != Marker.Material)
			{
				UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Marker.Material, Decal);
				if (Material)
				{
					Material->SetVectorParameterValue(TEXT("Color"), Marker.Color);
				}
				Decal->SetDecalMaterial(Material ? static_cast<UMaterialInterface*>(Material) : Marker.Material);
			}
			Decal->SetVisibility(true);
		}
	}
	for (int32 Index = Used; Index < Pool.Num(); ++Index)
	{
		if (Pool[Index])
		{
			Pool[Index]->SetVisibility(false);
		}
	}
}

void UEGChessFeedbackRenderer_Decal::Shutdown()
{
	for (UDecalComponent* Decal : Pool)
	{
		if (Decal)
		{
			Decal->DestroyComponent();
		}
	}
	Pool.Reset();
}

// ---------------------------------------------------------------------------------------------
// Board material
// ---------------------------------------------------------------------------------------------

void UEGChessFeedbackRenderer_BoardMaterial::Initialize(UEGChessBoardFeedbackComponent* InOwner, const UEGChessTableStyle* InStyle)
{
	Super::Initialize(InOwner, InStyle);

	MarksTexture = UTexture2D::CreateTransient(8, 8, PF_B8G8R8A8);
	if (MarksTexture)
	{
		MarksTexture->Filter = TF_Nearest;
		MarksTexture->SRGB = false;
		MarksTexture->AddressX = TA_Clamp;
		MarksTexture->AddressY = TA_Clamp;
		MarksTexture->UpdateResource();
	}

	UStaticMeshComponent* Board = InOwner ? InOwner->GetBoardMesh() : nullptr;
	if (Board)
	{
		BoardMaterial = Board->CreateAndSetMaterialInstanceDynamic(MaterialSlot);
		if (BoardMaterial && MarksTexture)
		{
			BoardMaterial->SetTextureParameterValue(TextureParameter, MarksTexture);
		}
	}
}

void UEGChessFeedbackRenderer_BoardMaterial::Apply(const FEGChessSquareMarks& Marks, const FEGChessBoardGeometry& Geometry)
{
	if (!MarksTexture)
	{
		return;
	}

	uint8* Data = new uint8[8 * 8 * 4];
	FMemory::Memzero(Data, 8 * 8 * 4);
	for (int32 Square = 0; Square < 64; ++Square)
	{
		const FIntPoint Grid = Geometry.SquareToGrid(Square);
		uint8* Texel = Data + (Grid.Y * 8 + Grid.X) * 4;
		// BGRA
		Texel[0] = 0;
		Texel[1] = static_cast<uint8>((Marks[Square] >> 8) & 0xFF);
		Texel[2] = static_cast<uint8>(Marks[Square] & 0xFF);
		Texel[3] = 255;
	}

	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, 8, 8);
	MarksTexture->UpdateTextureRegions(0, 1, Region, 8 * 4, 4, Data, [](uint8* SrcData, const FUpdateTextureRegion2D* Regions)
	{
		delete[] SrcData;
		delete Regions;
	});
}

void UEGChessFeedbackRenderer_BoardMaterial::Shutdown()
{
	BoardMaterial = nullptr;
	MarksTexture = nullptr;
}
