// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGChessBoardFeedbackComponent.h"

#include "Components/DecalComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "EGChessFeedbackRenderer.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Data/EGChessTableStyle.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Presentation/EGChessEngineAssets.h"

// ---------------------------------------------------------------------------------------------
// Component
// ---------------------------------------------------------------------------------------------

UEGChessBoardFeedbackComponent::UEGChessBoardFeedbackComponent()
{
	// Ticks only while a mark animates (ApplyMarks switches it).
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
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
	SetComponentTickEnabled(Renderer && Renderer->WantsTick());
}

void UEGChessBoardFeedbackComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (Renderer && Renderer->WantsTick())
	{
		Renderer->Tick(DeltaTime);
	}
	else
	{
		SetComponentTickEnabled(false);
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
	if (!Visual)
	{
		// A table with no style (or a style that dropped this type) uses the style class defaults.
		Visual = GetDefault<UEGChessTableStyle>()->FindMarker(Type);
	}
	if (Visual)
	{
		Out.Mesh = EGChessEngineAssets::Resolve(Visual->Mesh);
		Out.Material = EGChessEngineAssets::Resolve(Visual->Material);
		Out.Color = Visual->Color;
		Out.Scale = Visual->Scale;
		Out.HeightOffset = Visual->HeightOffset;
		Out.Shape = Visual->Shape;
		Out.FrameWidth = Visual->FrameWidth;
		Out.FillColor = Visual->FillColor;
		Out.OutlineColor = Visual->OutlineColor;
		Out.OutlineWidth = Visual->OutlineWidth;
		Out.PulseAmount = Visual->PulseAmount;
		Out.PulsePeriod = Visual->PulsePeriod;
		Out.bAuthoredMesh = Out.Mesh != nullptr;
	}
	else
	{
		Out.Color = FLinearColor(0.3f, 0.6f, 1.0f, 0.6f);
		Out.HeightOffset = 0.05f + 0.01f * static_cast<float>(Type);
	}
	if (!Out.Mesh)
	{
		Out.Mesh = Out.Shape == EEGChessMarkerShape::Disc ? EGChessEngineAssets::Cylinder() : EGChessEngineAssets::Plane();
	}
	if (!Out.Material)
	{
		// Unlit, so a mark is as bright in a dark room as in a lit one, and translucent, so its alpha counts.
		Out.Material = EGChessEngineAssets::UnlitTranslucentMaterial();
		Out.bUsesFallbackMaterial = true;
	}
	return Out;
}

// ---------------------------------------------------------------------------------------------
// Instanced
// ---------------------------------------------------------------------------------------------

namespace EGChessFeedbackInstancedPrivate
{
	/** Receives one flat piece of a mark: a box SizeX by SizeY, or a disc SizeX across, centred on Center. */
	using FEmitPiece = TFunctionRef<void(const FVector& Center, float SizeX, float SizeY, bool bDisc)>;

	/** Breaks one shape on one square into pieces. Size and Width in uu. */
	void EmitShape(EEGChessMarkerShape Shape, const FVector& Center, float Size, float Width, FEmitPiece Emit)
	{
		if (Size <= 0.0f)
		{
			return;
		}
		if (Shape != EEGChessMarkerShape::Frame || Width * 2.0f >= Size)
		{
			Emit(Center, Size, Size, Shape == EEGChessMarkerShape::Disc);
			return;
		}
		const float Offset = (Size - Width) * 0.5f;
		const float Inner = Size - 2.0f * Width;
		Emit(Center + FVector(Offset, 0.0f, 0.0f), Width, Size, false);
		Emit(Center - FVector(Offset, 0.0f, 0.0f), Width, Size, false);
		Emit(Center + FVector(0.0f, Offset, 0.0f), Inner, Width, false);
		Emit(Center - FVector(0.0f, Offset, 0.0f), Inner, Width, false);
	}

	/** Flat triangles facing up, for a procedural layer. */
	struct FFlatMesh
	{
		TArray<FVector> Vertices;
		TArray<int32> Triangles;

		void AddPiece(const FVector& Center, float SizeX, float SizeY, bool bDisc)
		{
			const int32 Base = Vertices.Num();
			if (!bDisc)
			{
				// Same corner order and winding as the engine's procedural box top.
				const float HX = SizeX * 0.5f;
				const float HY = SizeY * 0.5f;
				Vertices.Add(Center + FVector(-HX, HY, 0.0f));
				Vertices.Add(Center + FVector(HX, HY, 0.0f));
				Vertices.Add(Center + FVector(HX, -HY, 0.0f));
				Vertices.Add(Center + FVector(-HX, -HY, 0.0f));
				Triangles.Append({ Base, Base + 1, Base + 3, Base + 1, Base + 2, Base + 3 });
				return;
			}
			constexpr int32 Segments = 24;
			const float Radius = SizeX * 0.5f;
			Vertices.Add(Center);
			for (int32 Index = 0; Index < Segments; ++Index)
			{
				// Clockwise seen from above, as the quad.
				const float Angle = -UE_TWO_PI * static_cast<float>(Index) / Segments;
				Vertices.Add(Center + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0f));
			}
			for (int32 Index = 0; Index < Segments; ++Index)
			{
				Triangles.Append({ Base, Base + 1 + Index, Base + 1 + (Index + 1) % Segments });
			}
		}
	};

	/** Brightness of a pulse at Time: full at 0, dimmed by Amount at the half period. */
	float PulseFactor(float Amount, float Period, float Time)
	{
		if (Amount <= 0.0f)
		{
			return 1.0f;
		}
		const float Phase = FMath::Cos(UE_TWO_PI * Time / FMath::Max(0.1f, Period));
		return 1.0f - Amount * 0.5f * (1.0f - Phase);
	}
}

void UEGChessFeedbackRenderer_Instanced::Initialize(UEGChessBoardFeedbackComponent* InOwner, const UEGChessTableStyle* InStyle)
{
	Super::Initialize(InOwner, InStyle);
	Shutdown();

	if (!InOwner || !InOwner->GetOwner())
	{
		return;
	}

	// Translucent marks sort by priority, not by their few hundredths of a uu of height, so draw
	// them in HeightOffset order: the higher a mark sits, the later it draws.
	TArray<FResolvedMarker> Markers;
	TArray<int32> Order;
	for (int32 Index = 0; Index < EGChessMarks::NumTypes; ++Index)
	{
		Markers.Add(ResolveMarker(static_cast<EEGChessMarkType>(Index)));
		Order.Add(Index);
	}
	Order.StableSort([&Markers](int32 A, int32 B) { return Markers[A].HeightOffset < Markers[B].HeightOffset; });

	Layers.SetNum(EGChessMarks::NumTypes * NumParts);
	LayerMaterials.SetNum(Layers.Num());
	LayerColors.Init(FLinearColor::White, Layers.Num());
	PulseClocks.Init(0.0f, EGChessMarks::NumTypes);
	TypeShowing.Init(false, EGChessMarks::NumTypes);
	PulseSettings.Reset();
	for (const FResolvedMarker& Marker : Markers)
	{
		PulseSettings.Add(FVector2f(Marker.PulseAmount, Marker.PulsePeriod));
	}

	for (int32 Rank = 0; Rank < Order.Num(); ++Rank)
	{
		const int32 TypeIndex = Order[Rank];
		const FResolvedMarker& Marker = Markers[TypeIndex];
		for (int32 PartIndex = 0; PartIndex < NumParts; ++PartIndex)
		{
			const ELayerPart Part = static_cast<ELayerPart>(PartIndex);
			FLinearColor Color = Marker.Color;
			if (Part == Outline)
			{
				Color = Marker.OutlineColor;
				if (Color.A <= 0.0f || Marker.OutlineWidth <= 0.0f)
				{
					continue;
				}
			}
			else if (Part == Fill)
			{
				Color = Marker.FillColor;
				if (Color.A <= 0.0f || Marker.Shape != EEGChessMarkerShape::Frame)
				{
					continue;
				}
			}

			// An authored mesh is instanced (its material is the style's, built for that). The engine
			// stand-ins are built as procedural meshes instead: the fallback material is an engine
			// asset that lacks the instanced-mesh usage flag and would draw as the default grid.
			UMeshComponent* Layer = nullptr;
			if (Marker.bAuthoredMesh)
			{
				UInstancedStaticMeshComponent* Instanced = NewObject<UInstancedStaticMeshComponent>(InOwner->GetOwner(), NAME_None, RF_Transient);
				Instanced->SetStaticMesh(Marker.Mesh);
				Instanced->SetCanEverAffectNavigation(false);
				Layer = Instanced;
			}
			else
			{
				UProceduralMeshComponent* Procedural = NewObject<UProceduralMeshComponent>(InOwner->GetOwner(), NAME_None, RF_Transient);
				Procedural->SetCanEverAffectNavigation(false);
				Procedural->bUseComplexAsSimpleCollision = false;
				Layer = Procedural;
			}
			Layer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Layer->SetCastShadow(false);
			Layer->bReceivesDecals = false;
			Layer->SetTranslucentSortPriority(Rank * NumParts + PartIndex);
			Layer->SetupAttachment(InOwner);
			Layer->RegisterComponent();

			UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Marker.Material, Layer);
			if (Material)
			{
				Material->SetVectorParameterValue(TEXT("Color"), Color);
				Layer->SetMaterial(0, Material);
			}
			const int32 Index = LayerIndex(TypeIndex, Part);
			Layers[Index] = Layer;
			LayerMaterials[Index] = Material;
			LayerColors[Index] = Color;
		}
	}
	bHasApplied = false;
}

void UEGChessFeedbackRenderer_Instanced::Apply(const FEGChessSquareMarks& Marks, const FEGChessBoardGeometry& Geometry)
{
	using namespace EGChessFeedbackInstancedPrivate;

	const bool bGeometryChanged = !bHasApplied || Geometry.WhiteSeat != LastWhiteSeat || !FMath::IsNearlyEqual(Geometry.SquareSize, LastSquareSize);

	for (int32 TypeIndex = 0; TypeIndex < EGChessMarks::NumTypes; ++TypeIndex)
	{
		if (!Layers.IsValidIndex(LayerIndex(TypeIndex, Main)) || !Layers[LayerIndex(TypeIndex, Main)])
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
		PulseClocks[TypeIndex] = 0.0f;
		TypeShowing[TypeIndex] = false;
		for (int32 Square = 0; Square < 64 && !TypeShowing[TypeIndex]; ++Square)
		{
			TypeShowing[TypeIndex] = EGChessMarks::Has(Marks[Square], Type);
		}

		const FResolvedMarker Marker = ResolveMarker(Type);
		const FBoxSphereBounds Bounds = Marker.Mesh ? Marker.Mesh->GetBounds() : FBoxSphereBounds(FVector::ZeroVector, FVector(50.0f), 50.0f);
		const float MeshSize = FMath::Max(1.0f, static_cast<float>(FMath::Max(Bounds.BoxExtent.X, Bounds.BoxExtent.Y) * 2.0f));
		// An authored mesh with depth is flattened to a sliver.
		const float ZScale = Bounds.BoxExtent.Z > 0.01 ? 0.02f / static_cast<float>(Bounds.BoxExtent.Z * 2.0) : 1.0f;
		const float Size = Geometry.SquareSize * Marker.Scale;
		const float Width = Geometry.SquareSize * Marker.FrameWidth;
		const float Stroke = Geometry.SquareSize * Marker.OutlineWidth;

		for (int32 PartIndex = 0; PartIndex < NumParts; ++PartIndex)
		{
			UMeshComponent* Layer = Layers[LayerIndex(TypeIndex, static_cast<ELayerPart>(PartIndex))];
			if (!Layer)
			{
				continue;
			}

			// The stroke runs past both edges of a frame, so it frames the frame inside and out. The
			// lower parts sit a hair lower too, for any viewer whose sort order disagrees.
			EEGChessMarkerShape PartShape = Marker.Shape;
			float PartSize = Size;
			float PartWidth = Width;
			float PartDrop = 0.0f;
			if (PartIndex == Outline)
			{
				PartSize = Size + 2.0f * Stroke;
				PartWidth = Width + 2.0f * Stroke;
				PartDrop = 0.005f;
			}
			else if (PartIndex == Fill)
			{
				PartShape = EEGChessMarkerShape::Plate;
				PartSize = Size - 2.0f * Width;
				PartDrop = 0.0025f;
			}

			FFlatMesh Flat;
			UInstancedStaticMeshComponent* Instanced = Cast<UInstancedStaticMeshComponent>(Layer);
			if (Instanced)
			{
				Instanced->ClearInstances();
			}
			for (int32 Square = 0; Square < 64; ++Square)
			{
				if (!EGChessMarks::Has(Marks[Square], Type))
				{
					continue;
				}
				const FVector Center = Geometry.SquareToLocal(Square) + FVector(0.0f, 0.0f, Marker.HeightOffset - PartDrop);
				EmitShape(PartShape, Center, PartSize, PartWidth, [&](const FVector& PieceCenter, float SizeX, float SizeY, bool bDisc)
				{
					if (Instanced)
					{
						Instanced->AddInstance(FTransform(FQuat::Identity, PieceCenter, FVector(SizeX / MeshSize, SizeY / MeshSize, ZScale)), false);
					}
					else
					{
						Flat.AddPiece(PieceCenter, SizeX, SizeY, bDisc);
					}
				});
			}

			if (UProceduralMeshComponent* Procedural = Cast<UProceduralMeshComponent>(Layer))
			{
				if (Flat.Vertices.IsEmpty())
				{
					Procedural->ClearMeshSection(0);
				}
				else
				{
					TArray<FVector> Normals;
					Normals.Init(FVector::UpVector, Flat.Vertices.Num());
					Procedural->CreateMeshSection_LinearColor(0, Flat.Vertices, Flat.Triangles, Normals, {}, {}, {}, /*bCreateCollision=*/ false);
				}
			}
		}
	}

	LastMarks = Marks;
	LastWhiteSeat = Geometry.WhiteSeat;
	LastSquareSize = Geometry.SquareSize;
	bHasApplied = true;
}

bool UEGChessFeedbackRenderer_Instanced::WantsTick() const
{
	for (int32 TypeIndex = 0; TypeIndex < EGChessMarks::NumTypes; ++TypeIndex)
	{
		if (TypeShowing.IsValidIndex(TypeIndex) && TypeShowing[TypeIndex] && PulseSettings[TypeIndex].X > 0.0f)
		{
			return true;
		}
	}
	return false;
}

void UEGChessFeedbackRenderer_Instanced::Tick(float DeltaTime)
{
	using namespace EGChessFeedbackInstancedPrivate;

	for (int32 TypeIndex = 0; TypeIndex < EGChessMarks::NumTypes; ++TypeIndex)
	{
		const FVector2f Pulse = PulseSettings.IsValidIndex(TypeIndex) ? PulseSettings[TypeIndex] : FVector2f::ZeroVector;
		if (!TypeShowing.IsValidIndex(TypeIndex) || !TypeShowing[TypeIndex] || Pulse.X <= 0.0f)
		{
			continue;
		}
		PulseClocks[TypeIndex] += DeltaTime;
		const float Factor = PulseFactor(Pulse.X, Pulse.Y, PulseClocks[TypeIndex]);

		// The mark and its fill breathe; the stroke under them holds, so the edge never washes out.
		for (const ELayerPart Part : { Fill, Main })
		{
			const int32 Index = LayerIndex(TypeIndex, Part);
			if (UMaterialInstanceDynamic* Material = LayerMaterials[Index])
			{
				const FLinearColor& Base = LayerColors[Index];
				Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(Base.R * Factor, Base.G * Factor, Base.B * Factor, Base.A * FMath::Lerp(1.0f, Factor, 0.5f)));
			}
		}
	}
}

void UEGChessFeedbackRenderer_Instanced::Shutdown()
{
	for (UMeshComponent* Layer : Layers)
	{
		if (Layer)
		{
			Layer->DestroyComponent();
		}
	}
	Layers.Reset();
	LayerMaterials.Reset();
	LayerColors.Reset();
	PulseClocks.Reset();
	TypeShowing.Reset();
	PulseSettings.Reset();
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
