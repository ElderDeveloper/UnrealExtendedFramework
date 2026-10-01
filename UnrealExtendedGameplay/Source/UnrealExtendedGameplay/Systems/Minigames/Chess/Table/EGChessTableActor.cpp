// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGChessTableActor.h"

#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "DrawDebugHelpers.h"
#include "EGChessInteraction_Sit.h"
#include "Engine/AssetManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/DataValidation.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundConcurrency.h"
#include "StructUtils/InstancedStruct.h"
#include "TimerManager.h"
#include "UnrealExtendedGameplay/InteractionSystem/EGInteractionProviderComponent.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/AI/EGChessAI.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/AI/EGChessAI_RuleBased.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Core/EGChessMoveGen.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Core/EGChessNotation.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Core/EGChessRules.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Data/EGChessAIProfile.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Data/EGChessAnimationSet.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Data/EGChessInputConfig.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Data/EGChessPieceSet.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Data/EGChessSoundSet.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Data/EGChessTableConfig.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Data/EGChessTableStyle.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Data/EGChessTimeControl.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Data/EGChessUIConfig.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Feedback/EGChessBoardFeedback.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Feedback/EGChessBoardFeedbackComponent.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Player/EGChessCharacterInterface.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Player/EGChessPlayerComponent.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Presentation/EGChessEngineAssets.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Presentation/EGChessIconCapture.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Presentation/EGChessPieceVisuals.h"

#define LOCTEXT_NAMESPACE "EGChess"

DEFINE_LOG_CATEGORY_STATIC(LogEGChess, Log, All);

namespace EGChessTablePrivate
{
	// A component's transform relative to the actor root, composed from relative transforms only, so
	// it is right on templates (class default objects) whose components are never registered.
	FTransform ComponentToActor(const USceneComponent* Component, const USceneComponent* ActorRoot)
	{
		FTransform Result = FTransform::Identity;
		for (const USceneComponent* It = Component; It && It != ActorRoot; It = It->GetAttachParent())
		{
			Result = Result * It->GetRelativeTransform();
		}
		return Result;
	}

	FStreamableManager& Streamable()
	{
		if (UAssetManager* Manager = UAssetManager::GetIfInitialized())
		{
			return Manager->GetStreamableManager();
		}
		static FStreamableManager Fallback;
		return Fallback;
	}

	constexpr float DefaultSquareSize = 5.0f;
}

// =============================================================================================
// Construction
// =============================================================================================

AEGChessTableActor::AEGChessTableActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	TableMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TableMesh"));
	TableMesh->SetupAttachment(Root);

	BoardMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoardMesh"));
	BoardMesh->SetupAttachment(Root);
	BoardMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 75.0f));

	BoardSurface = CreateDefaultSubobject<USceneComponent>(TEXT("BoardSurface"));
	BoardSurface->SetupAttachment(BoardMesh);
	BoardSurface->SetRelativeLocation(FVector(-3.5f * EGChessTablePrivate::DefaultSquareSize, -3.5f * EGChessTablePrivate::DefaultSquareSize, 1.0f));

	TrayA = CreateDefaultSubobject<USceneComponent>(TEXT("TrayA"));
	TrayA->SetupAttachment(BoardSurface);
	TrayA->SetRelativeLocation(FVector(0.0f, 9.0f * EGChessTablePrivate::DefaultSquareSize, 0.0f));

	TrayB = CreateDefaultSubobject<USceneComponent>(TEXT("TrayB"));
	TrayB->SetupAttachment(BoardSurface);
	TrayB->SetRelativeLocationAndRotation(FVector(7.0f * EGChessTablePrivate::DefaultSquareSize, -2.0f * EGChessTablePrivate::DefaultSquareSize, 0.0f), FRotator(0.0f, 180.0f, 0.0f));

	Feedback = CreateDefaultSubobject<UEGChessBoardFeedbackComponent>(TEXT("Feedback"));
	Feedback->SetupAttachment(BoardSurface);

	ClockMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ClockMesh"));
	ClockMesh->SetupAttachment(Root);
	ClockMesh->SetRelativeLocation(FVector(0.0f, 30.0f, 75.0f));

	auto MakeClockPart = [this](const TCHAR* Name)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(ClockMesh);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return Part;
	};
	ClockButtonA = MakeClockPart(TEXT("ClockButtonA"));
	ClockButtonB = MakeClockPart(TEXT("ClockButtonB"));
	ClockHandA = MakeClockPart(TEXT("ClockHandA"));
	ClockHandB = MakeClockPart(TEXT("ClockHandB"));

	auto MakeSeat = [this](const TCHAR* SeatName, const TCHAR* ChairName, const TCHAR* CameraName, EEGChessSeat Id, const FVector& Location, float Yaw,
		UStaticMeshComponent*& OutChair, UCameraComponent*& OutCamera)
	{
		UEGChessSeatComponent* Seat = CreateDefaultSubobject<UEGChessSeatComponent>(SeatName);
		Seat->SetupAttachment(Root);
		Seat->SetRelativeLocationAndRotation(Location, FRotator(0.0f, Yaw, 0.0f));
		Seat->SeatId = Id;

		OutChair = CreateDefaultSubobject<UStaticMeshComponent>(ChairName);
		OutChair->SetupAttachment(Seat);

		OutCamera = CreateDefaultSubobject<UCameraComponent>(CameraName);
		OutCamera->SetupAttachment(Seat);
		OutCamera->SetRelativeLocationAndRotation(FVector(-15.0f, 0.0f, 150.0f), FRotator(-50.0f, 0.0f, 0.0f));
		OutCamera->SetAutoActivate(false);
		OutCamera->FieldOfView = 60.0f;

		Seat->Chair = OutChair;
		Seat->BoardCamera = OutCamera;
		return Seat;
	};
	UStaticMeshComponent* ChairAPtr = nullptr;
	UStaticMeshComponent* ChairBPtr = nullptr;
	UCameraComponent* CameraAPtr = nullptr;
	UCameraComponent* CameraBPtr = nullptr;
	SeatA = MakeSeat(TEXT("SeatA"), TEXT("ChairA"), TEXT("CameraA"), EEGChessSeat::A, FVector(-80.0f, 0.0f, 0.0f), 0.0f, ChairAPtr, CameraAPtr);
	SeatB = MakeSeat(TEXT("SeatB"), TEXT("ChairB"), TEXT("CameraB"), EEGChessSeat::B, FVector(80.0f, 0.0f, 0.0f), 180.0f, ChairBPtr, CameraBPtr);
	ChairA = ChairAPtr;
	ChairB = ChairBPtr;
	CameraA = CameraAPtr;
	CameraB = CameraBPtr;

	PieceVisuals = CreateDefaultSubobject<UEGChessPieceVisuals>(TEXT("PieceVisuals"));

	SitInteraction = CreateDefaultSubobject<UEGInteractionProviderComponent>(TEXT("SitInteraction"));
	SitInteraction->bHighlightOnFocus = false;
	{
		FEGInteractionDefinition SitDefinition;
		SitDefinition.Id = TAG_EGChess_Interaction_Sit;
		SitDefinition.InteractionClass = UEGChessInteraction_Sit::StaticClass();
		SitDefinition.Text = LOCTEXT("SitPrompt", "Sit");
		SitInteraction->Definitions.Add(FInstancedStruct::Make(SitDefinition));
	}
}

void AEGChessTableActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AEGChessTableActor, Phase);
	DOREPLIFETIME(AEGChessTableActor, PhaseEndServerTime);
	DOREPLIFETIME(AEGChessTableActor, WhiteSeat);
	DOREPLIFETIME(AEGChessTableActor, GameIndex);
	DOREPLIFETIME(AEGChessTableActor, MoveHistory);
	DOREPLIFETIME(AEGChessTableActor, StartFEN);
	DOREPLIFETIME(AEGChessTableActor, Clock);
	DOREPLIFETIME(AEGChessTableActor, Result);
	DOREPLIFETIME(AEGChessTableActor, DrawOffer);
	DOREPLIFETIME(AEGChessTableActor, RematchRequests);
	DOREPLIFETIME(AEGChessTableActor, ActiveRules);
	DOREPLIFETIME(AEGChessTableActor, TakebacksUsed);
}

void AEGChessTableActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	const UWorld* World = GetWorld();
	if (World && !World->IsGameWorld())
	{
		ApplyStyle(true);
	}
}

void AEGChessTableActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	SeatA->SetOwningTable(this);
	SeatB->SetOwningTable(this);

	if (SitInputTag.IsValid())
	{
		for (FInstancedStruct& Definition : SitInteraction->Definitions)
		{
			FEGInteractionDefinition* Base = Definition.GetMutablePtr<FEGInteractionDefinition>();
			if (Base && Base->Id == TAG_EGChess_Interaction_Sit)
			{
				Base->InputTag = SitInputTag;
			}
		}
		SitInteraction->NotifyDefinitionsChanged();
	}
	SitInteraction->OnFocusChanged.AddDynamic(this, &AEGChessTableActor::HandleSitFocusChanged);
}

void AEGChessTableActor::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		ActiveRules = ResolveRules();
		if (StartFEN.IsEmpty())
		{
			StartFEN = FEGChessPosition::StartFEN();
		}
		LogoutHandle = FGameModeEvents::OnGameModeLogoutEvent().AddUObject(this, &AEGChessTableActor::HandleLogout);
		PostLoginHandle = FGameModeEvents::OnGameModePostLoginEvent().AddUObject(this, &AEGChessTableActor::HandlePostLogin);
	}

	RebuildPosition();
	ResolveConfigLoads();

	if (HasAuthority() && ActiveRules.OpponentPolicy == EEGChessOpponentPolicy::AIvsAI)
	{
		DebugStartAIvsAI();
	}
}

void AEGChessTableActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FGameModeEvents::OnGameModeLogoutEvent().Remove(LogoutHandle);
	FGameModeEvents::OnGameModePostLoginEvent().Remove(PostLoginHandle);
	GetWorldTimerManager().ClearAllTimersForObject(this);

	if (VisualContentHandle.IsValid())
	{
		VisualContentHandle->CancelHandle();
	}
	if (InteractionContentHandle.IsValid())
	{
		InteractionContentHandle->CancelHandle();
	}
	if (IconCapture)
	{
		IconCapture->Shutdown();
	}
	Super::EndPlay(EndPlayReason);
}

FEGChessMatchRules AEGChessTableActor::ResolveRules() const
{
	if (bOverrideMatchRules)
	{
		return MatchRulesOverride;
	}
	return Config ? Config->MatchRules : FEGChessMatchRules();
}

// =============================================================================================
// Content
// =============================================================================================

UEGChessTableStyle* AEGChessTableActor::GetTableStyle() const { return Config ? Config->TableStyle : nullptr; }
UEGChessPieceSet* AEGChessTableActor::GetPieceSet() const { return Config ? Config->PieceSet : nullptr; }
UEGChessSoundSet* AEGChessTableActor::GetSoundSet() const { return Config ? Config->SoundSet : nullptr; }
UEGChessEffectsSet* AEGChessTableActor::GetEffectsSet() const { return Config ? Config->EffectsSet : nullptr; }
UEGChessUIConfig* AEGChessTableActor::GetUIConfig() const { return Config ? Config->UI : nullptr; }
UEGChessInputConfig* AEGChessTableActor::GetInputConfig() const { return Config ? Config->Input : nullptr; }

UEGChessAnimationSet* AEGChessTableActor::GetAnimationSetFor(const APawn* Pawn) const
{
	if (Pawn && Pawn->Implements<UEGChessCharacterInterface>())
	{
		if (UEGChessAnimationSet* Own = IEGChessCharacterInterface::Execute_GetChessAnimationSet(const_cast<APawn*>(Pawn)))
		{
			return Own;
		}
	}
	return Config ? Config->AnimationSet : nullptr;
}

void AEGChessTableActor::ResolveConfigLoads()
{
	TArray<FSoftObjectPath> Paths;
	if (const UEGChessTableStyle* Style = GetTableStyle())
	{
		Style->GatherSoftPaths(Paths);
	}
	if (const UEGChessPieceSet* Set = GetPieceSet())
	{
		Set->GatherSoftPaths(Paths);
	}

	if (Paths.Num() == 0)
	{
		OnVisualContentLoaded();
		return;
	}
	VisualContentHandle = EGChessTablePrivate::Streamable().RequestAsyncLoad(Paths, FStreamableDelegate::CreateUObject(this, &AEGChessTableActor::OnVisualContentLoaded));
	if (!VisualContentHandle.IsValid())
	{
		OnVisualContentLoaded();
	}
}

void AEGChessTableActor::OnVisualContentLoaded()
{
	if (bVisualsReady || !IsValid(this))
	{
		return;
	}
	ApplyStyle(false);

	Feedback->SetBoardMesh(BoardMesh);
	Feedback->InitializeRenderer(GetTableStyle());
	PieceVisuals->Initialize(this);
	PieceVisuals->Snap(GetEffectiveStartFEN(), MoveHistory, Result);
	PresentedGameIndex = GameIndex;
	bVisualsReady = true;

	const UEGChessTableStyle* Style = GetTableStyle();
	bCoordinatesVisible = Style && Style->bShowCoordinatesByDefault;
	RefreshCoordinates();
	RefreshMarks();
}

void AEGChessTableActor::EnsureInteractionContentLoaded()
{
	if (InteractionContentHandle.IsValid())
	{
		return;
	}
	TArray<FSoftObjectPath> Paths;
	if (const UEGChessAnimationSet* Set = Config ? Config->AnimationSet : nullptr)
	{
		Set->GatherSoftPaths(Paths);
	}
	for (const UEGChessSeatComponent* Seat : { SeatA.Get(), SeatB.Get() })
	{
		if (const UEGChessAnimationSet* Set = GetAnimationSetFor(Seat->GetSeatedPawn()))
		{
			Set->GatherSoftPaths(Paths);
		}
	}
	if (const UEGChessSoundSet* Sounds = GetSoundSet())
	{
		Sounds->GatherSoftPaths(Paths);
	}
	if (const UEGChessEffectsSet* Effects = GetEffectsSet())
	{
		Effects->GatherSoftPaths(Paths);
	}
	if (const UEGChessUIConfig* UI = GetUIConfig())
	{
		for (const FSoftObjectPath& Path : { UI->HUDClass.ToSoftObjectPath(), UI->MenuClass.ToSoftObjectPath(), UI->PromptClass.ToSoftObjectPath(), UI->PromotionClass.ToSoftObjectPath(), UI->GameOverClass.ToSoftObjectPath() })
		{
			if (Path.IsValid())
			{
				Paths.AddUnique(Path);
			}
		}
	}
	if (Paths.Num() > 0)
	{
		InteractionContentHandle = EGChessTablePrivate::Streamable().RequestAsyncLoad(Paths, FStreamableDelegate());
	}
}

void AEGChessTableActor::ApplyStyle(bool bEditorPreview)
{
	const UEGChessTableStyle* Style = GetTableStyle();

	auto SetMesh = [](UStaticMeshComponent* Component, const TSoftObjectPtr<UStaticMesh>& Soft)
	{
		UStaticMesh* Mesh = EGChessEngineAssets::Resolve(Soft);
		Component->SetStaticMesh(Mesh);
		Component->SetVisibility(Mesh != nullptr);
		return Mesh != nullptr;
	};

	if (!Style)
	{
		for (UStaticMeshComponent* Component : { TableMesh.Get(), ChairA.Get(), ChairB.Get(), BoardMesh.Get(), ClockMesh.Get(), ClockButtonA.Get(), ClockButtonB.Get(), ClockHandA.Get(), ClockHandB.Get() })
		{
			Component->SetStaticMesh(nullptr);
			Component->SetVisibility(false);
		}
		BoardSurface->SetRelativeTransform(FTransform(FVector(-3.5f * EGChessTablePrivate::DefaultSquareSize, -3.5f * EGChessTablePrivate::DefaultSquareSize, 1.0f)));
	}
	else
	{
		SetMesh(TableMesh, Style->TableMesh);
		SetMesh(ChairA, Style->ChairMesh);
		SetMesh(ChairB, Style->ChairMesh);
		SetMesh(BoardMesh, Style->BoardMesh);
		BoardSurface->SetRelativeTransform(Style->BoardSurfaceTransform);

		if (SetMesh(ClockMesh, Style->ClockMesh))
		{
			ClockButtonA->AttachToComponent(ClockMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Style->ButtonSocketA);
			ClockButtonB->AttachToComponent(ClockMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Style->ButtonSocketB);
			SetMesh(ClockButtonA, Style->ClockButtonMesh);
			SetMesh(ClockButtonB, Style->ClockButtonMesh);

			ClockHandA->AttachToComponent(ClockMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Style->HandSocketA);
			ClockHandB->AttachToComponent(ClockMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Style->HandSocketB);
			SetMesh(ClockHandA, Style->ClockHandMesh);
			SetMesh(ClockHandB, Style->ClockHandMesh);
		}
		else
		{
			for (UStaticMeshComponent* Component : { ClockButtonA.Get(), ClockButtonB.Get(), ClockHandA.Get(), ClockHandB.Get() })
			{
				Component->SetStaticMesh(nullptr);
				Component->SetVisibility(false);
			}
		}
	}

	if (!bEditorPreview && !BoardMesh->GetStaticMesh())
	{
		BuildFallbackBoard();
	}
}

void AEGChessTableActor::BuildFallbackBoard()
{
	for (UInstancedStaticMeshComponent* Layer : FallbackBoard)
	{
		if (Layer)
		{
			Layer->DestroyComponent();
		}
	}
	FallbackBoard.Reset();

	const UEGChessTableStyle* Style = GetTableStyle();
	const float Square = GetBoardGeometry().SquareSize;
	const FLinearColor Colors[2] = {
		Style ? Style->FallbackDarkColor : FLinearColor(0.25f, 0.15f, 0.08f),
		Style ? Style->FallbackLightColor : FLinearColor(0.8f, 0.7f, 0.55f)
	};

	for (int32 Layer = 0; Layer < 2; ++Layer)
	{
		UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
		Component->SetStaticMesh(EGChessEngineAssets::Plane());
		Component->SetMaterial(0, EGChessEngineAssets::TintedMaterial(Component, Colors[Layer]));
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetupAttachment(BoardSurface);
		Component->RegisterComponent();
		const float Scale = Square / 100.0f;
		for (int32 X = 0; X < 8; ++X)
		{
			for (int32 Y = 0; Y < 8; ++Y)
			{
				// a1 is dark from either side: dark squares are those with an even grid sum.
				if (((X + Y) % 2 == 0) == (Layer == 0))
				{
					Component->AddInstance(FTransform(FQuat::Identity, FVector(X * Square, Y * Square, -0.05f), FVector(Scale, Scale, 1.0f)), false);
				}
			}
		}
		FallbackBoard.Add(Component);
	}
}

void AEGChessTableActor::SetCoordinatesVisible(bool bVisible)
{
	bCoordinatesVisible = bVisible;
	RefreshCoordinates();
}

void AEGChessTableActor::RefreshCoordinates()
{
	for (UTextRenderComponent* Label : CoordinateLabels)
	{
		if (Label)
		{
			Label->DestroyComponent();
		}
	}
	CoordinateLabels.Reset();

	if (!bCoordinatesVisible || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const UEGChessTableStyle* Style = GetTableStyle();
	const FEGChessBoardGeometry Geometry = GetBoardGeometry();
	const float Square = Geometry.SquareSize;
	const float Offset = (0.5f + (Style ? Style->CoordinateOffset : 0.75f)) * Square;
	const FColor Color = (Style ? Style->CoordinateColor : FLinearColor(0.9f, 0.85f, 0.75f)).ToFColor(true);

	// "Behind" and "left" as seen from the white seat, in the surface frame.
	const FVector WhiteBack = Geometry.WhiteSeat == EEGChessSeat::A ? FVector(-1.0f, 0.0f, 0.0f) : FVector(1.0f, 0.0f, 0.0f);
	const FVector WhiteLeft = Geometry.WhiteSeat == EEGChessSeat::A ? FVector(0.0f, -1.0f, 0.0f) : FVector(0.0f, 1.0f, 0.0f);
	const FRotator Flat(90.0f, Geometry.WhiteSeat == EEGChessSeat::A ? 180.0f : 0.0f, 0.0f);

	auto AddLabel = [&](const FString& Text, const FVector& Location)
	{
		UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this, NAME_None, RF_Transient);
		Label->SetupAttachment(BoardSurface);
		Label->SetRelativeLocationAndRotation(Location + FVector(0.0f, 0.0f, 0.1f), Flat);
		Label->SetText(FText::FromString(Text));
		Label->SetWorldSize((Style ? Style->CoordinateSize : 0.3f) * Square);
		Label->SetTextRenderColor(Color);
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetVerticalAlignment(EVRTA_TextCenter);
		Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Label->RegisterComponent();
		CoordinateLabels.Add(Label);
	};

	for (int32 File = 0; File < 8; ++File)
	{
		AddLabel(FString::Printf(TEXT("%c"), static_cast<TCHAR>(TEXT('a') + File)),Geometry.SquareToLocal(EGChess::MakeSquare(File, 0)) + WhiteBack * Offset);
	}
	for (int32 Rank = 0; Rank < 8; ++Rank)
	{
		AddLabel(FString::FromInt(Rank + 1), Geometry.SquareToLocal(EGChess::MakeSquare(0, Rank)) + WhiteLeft * Offset);
	}
}

void AEGChessTableActor::UpdateClockHands()
{
	const UEGChessTableStyle* Style = GetTableStyle();
	if (!Style || !ClockHandA->GetStaticMesh() || !Clock.bTimed)
	{
		return;
	}
	for (const EEGChessSeat Seat : { EEGChessSeat::A, EEGChessSeat::B })
	{
		UStaticMeshComponent* Hand = Seat == EEGChessSeat::A ? ClockHandA : ClockHandB;
		const float Minutes = GetRemainingTime(GetSeatColor(Seat)) / 60.0f;
		const float Angle = FMath::DegreesToRadians(Minutes * Style->HandDegreesPerMinute);
		Hand->SetRelativeRotation(FQuat(Style->HandAxis.GetSafeNormal(), -Angle));
	}
}

UTexture* AEGChessTableActor::GetPieceIcon(EEGChessPieceType Type, EEGChessColor Color)
{
	const UEGChessPieceSet* Set = GetPieceSet();
	if (Set)
	{
		const FEGChessPieceVisual& Visual = Set->GetVisual(Type);
		if (UTexture2D* Authored = EGChessEngineAssets::Resolve(Color == EEGChessColor::White ? Visual.IconWhite : Visual.IconBlack))
		{
			return Authored;
		}
		if (!Set->bCaptureIconsAtRuntime)
		{
			return nullptr;
		}
	}
	if (!IconCapture)
	{
		IconCapture = NewObject<UEGChessIconCapture>(this);
		IconCapture->Initialize(this);
	}
	return IconCapture->GetIcon(Type, Color);
}

// =============================================================================================
// Queries
// =============================================================================================

double AEGChessTableActor::GetServerTime() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0;
	}
	const AGameStateBase* GameState = World->GetGameState();
	return GameState ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
}

float AEGChessTableActor::GetRemainingTime(EEGChessColor Color) const
{
	return Clock.GetRemaining(Color, GetServerTime());
}

TArray<FString> AEGChessTableActor::GetMoveListSAN() const
{
	TArray<FString> Out;
	for (const FEGChessMoveRecord& Record : MoveHistory)
	{
		Out.Add(Record.SAN);
	}
	return Out;
}

TArray<int32> AEGChessTableActor::GetLegalTargets(int32 FromSquare) const
{
	TArray<FEGChessMove> Moves;
	EGChessMoveGen::GenerateLegalFrom(Position, FromSquare, Moves);
	TArray<int32> Out;
	for (const FEGChessMove& Move : Moves)
	{
		Out.AddUnique(Move.To);
	}
	return Out;
}

bool AEGChessTableActor::FindSeatOfPlayer(const APlayerState* PlayerState, EEGChessSeat& OutSeat) const
{
	if (!PlayerState)
	{
		return false;
	}
	for (const UEGChessSeatComponent* Seat : { SeatA.Get(), SeatB.Get() })
	{
		if (Seat->GetOccupantRef().PlayerState == PlayerState)
		{
			OutSeat = Seat->SeatId;
			return true;
		}
	}
	return false;
}

bool AEGChessTableActor::FindSeatOfPawn(const APawn* Pawn, EEGChessSeat& OutSeat) const
{
	if (!Pawn)
	{
		return false;
	}
	for (const UEGChessSeatComponent* Seat : { SeatA.Get(), SeatB.Get() })
	{
		if (Seat->GetOccupantRef().Pawn == Pawn)
		{
			OutSeat = Seat->SeatId;
			return true;
		}
	}
	return false;
}

TArray<EEGChessPieceType> AEGChessTableActor::GetCapturedPieces(EEGChessColor Color) const
{
	TArray<EEGChessPieceType> Out;
	for (const FEGChessMoveRecord& Record : MoveHistory)
	{
		if (Record.CapturedType != EEGChessPieceType::None && Record.MoverColor != Color)
		{
			Out.Add(Record.CapturedType);
		}
	}
	return Out;
}

FText AEGChessTableActor::GetSeatDisplayName(EEGChessSeat Seat) const
{
	const FEGChessSeatOccupant& Occupant = GetSeat(Seat)->GetOccupantRef();
	if (Occupant.bAI)
	{
		const UEGChessAIProfile* Profile = ActiveRules.AIProfile;
		return Profile ? Profile->DisplayName : LOCTEXT("AIName", "Computer");
	}
	if (Occupant.PlayerState)
	{
		return FText::FromString(Occupant.PlayerState->GetPlayerName());
	}
	return FText::GetEmpty();
}

FEGChessBoardGeometry AEGChessTableActor::GetBoardGeometry() const
{
	FEGChessBoardGeometry Geometry;
	Geometry.SurfaceToWorld = FTransform(BoardSurface->GetComponentQuat(), BoardSurface->GetComponentLocation());
	const UEGChessTableStyle* Style = GetTableStyle();
	Geometry.SquareSize = Style ? Style->SquareSize : EGChessTablePrivate::DefaultSquareSize;
	Geometry.WhiteSeat = WhiteSeat;
	return Geometry;
}

bool AEGChessTableActor::IsClockOnSeatRight(EEGChessSeat Seat) const
{
	if (!ClockMesh->GetStaticMesh())
	{
		return true;
	}
	const FVector Local = GetSeat(Seat)->GetComponentTransform().InverseTransformPosition(ClockMesh->GetComponentLocation());
	return Local.Y >= 0.0f;
}

bool AEGChessTableActor::IsSeatLocallyControlled(EEGChessSeat Seat) const
{
	const FEGChessSeatOccupant& Occupant = GetSeat(Seat)->GetOccupantRef();
	return !Occupant.bAI && Occupant.Pawn && Occupant.Pawn->IsLocallyControlled();
}

bool AEGChessTableActor::IsSeatAI(EEGChessSeat Seat) const
{
	return GetSeat(Seat)->GetOccupantRef().bAI;
}

bool AEGChessTableActor::IsAIGame() const
{
	return IsSeatAI(EEGChessSeat::A) || IsSeatAI(EEGChessSeat::B);
}

// =============================================================================================
// Position
// =============================================================================================

FString AEGChessTableActor::GetEffectiveStartFEN() const
{
	return StartFEN.IsEmpty() ? FString(FEGChessPosition::StartFEN()) : StartFEN;
}

void AEGChessTableActor::RebuildPosition()
{
	if (!Position.FromFEN(GetEffectiveStartFEN()))
	{
		Position = FEGChessPosition::Start();
	}
	PositionHashes.Reset();
	PositionHashes.Add(Position.ComputeHash());
	IrreversibleIndex = 0;
	for (const FEGChessMoveRecord& Record : MoveHistory)
	{
		Position.ApplyMove(Record.Move);
		PositionHashes.Add(Position.ComputeHash());
		if (Position.HalfmoveClock == 0)
		{
			IrreversibleIndex = PositionHashes.Num() - 1;
		}
	}
}

TConstArrayView<uint64> AEGChessTableActor::GetRepetitionView() const
{
	const int32 Count = FMath::Max(0, PositionHashes.Num() - 1 - IrreversibleIndex);
	return TConstArrayView<uint64>(PositionHashes.GetData() + IrreversibleIndex, Count);
}

void AEGChessTableActor::ScheduleVisualReconcile()
{
	// The multicast that animates a move usually arrives with (or before) the history; give it a
	// moment before deciding the visuals are out of date and snapping them.
	GetWorldTimerManager().SetTimer(ReconcileTimer, this, &AEGChessTableActor::ReconcileVisuals, 0.4f, false);
}

void AEGChessTableActor::ReconcileVisuals()
{
	if (bVisualsReady && !PieceVisuals->MatchesHistory(GetEffectiveStartFEN(), MoveHistory))
	{
		PieceVisuals->Snap(GetEffectiveStartFEN(), MoveHistory, Result);
		RefreshMarks();
	}
}

// =============================================================================================
// Replication callbacks
// =============================================================================================

void AEGChessTableActor::NotifyStateChanged()
{
	OnStateChanged.Broadcast();
	if (UEGChessPlayerComponent* Driver = LocalDriver.Get())
	{
		Driver->HandleTableStateChanged();
	}
}

void AEGChessTableActor::OnRep_Phase()
{
	if (Phase == EEGChessPhase::Playing)
	{
		TriggerLocalEvent(EEGChessEvent::GameStart);
		bLowTimeBroadcast[0] = bLowTimeBroadcast[1] = false;
	}
	LastCountdownSecond = -1;
	if (Phase != EEGChessPhase::Playing && bLowTimeLoopPlaying)
	{
		bLowTimeLoopPlaying = false;
		FEGChessEventContext Context;
		Context.Event = EEGChessEvent::ClockLowTimeLoop;
		Context.bStart = false;
		Context.Location = ClockMesh->GetComponentLocation();
		TriggerEvent(Context);
	}
	OnPhaseChanged.Broadcast(Phase);
	NotifyStateChanged();
	RefreshMarks();
}

void AEGChessTableActor::OnRep_WhiteSeat()
{
	OnColoursAssigned.Broadcast(WhiteSeat);
	RefreshCoordinates();
}

void AEGChessTableActor::OnRep_GameIndex()
{
	if (bVisualsReady && PresentedGameIndex != GameIndex)
	{
		PieceVisuals->PresentReset(GetEffectiveStartFEN());
	}
	PresentedGameIndex = GameIndex;
	bLowTimeBroadcast[0] = bLowTimeBroadcast[1] = false;
	RefreshMarks();
}

void AEGChessTableActor::OnRep_MoveHistory()
{
	RebuildPosition();
	ScheduleVisualReconcile();
	NotifyStateChanged();
	RefreshMarks();
}

void AEGChessTableActor::OnRep_Clock()
{
	NotifyStateChanged();
}

void AEGChessTableActor::OnRep_Result()
{
	if (Result.IsOver())
	{
		OnGameEnded.Broadcast(Result);
		if (UEGChessPlayerComponent* Driver = LocalDriver.Get())
		{
			Driver->HandleGameEnded(Result);
		}
	}
	NotifyStateChanged();
	RefreshMarks();
}

void AEGChessTableActor::OnRep_DrawOffer()
{
	if (DrawOffer.bPending)
	{
		OnDrawOffered.Broadcast(DrawOffer.OfferedBy);
	}
	NotifyStateChanged();
}

void AEGChessTableActor::OnRep_Rematch()
{
	for (const EEGChessSeat Seat : { EEGChessSeat::A, EEGChessSeat::B })
	{
		const uint8 Bit = static_cast<uint8>(1 << EGChess::SeatIndex(Seat));
		if ((RematchRequests & Bit) && !(LastRematchBits & Bit))
		{
			OnRematchRequested.Broadcast(Seat);
		}
	}
	LastRematchBits = RematchRequests;
	NotifyStateChanged();
}

void AEGChessTableActor::OnRep_Rules()
{
	NotifyStateChanged();
}

// =============================================================================================
// Multicasts
// =============================================================================================

void AEGChessTableActor::MulticastMoveCommitted_Implementation(const FEGChessMoveRecord& Record, int32 PlyIndex, EEGChessSeat MoverSeat)
{
	FEGChessMoveTiming Timing;
	const UEGChessPieceSet* Set = GetPieceSet();
	const float MoveTime = Set ? Set->MoveTime : 0.45f;
	Timing.PickTime = 0.05f;
	Timing.PlaceTime = Timing.PickTime + MoveTime;
	Timing.ClockPressTime = Timing.PlaceTime + 0.2f;

	if (ActiveRules.MoveAnimation != EEGChessMoveAnimation::Off)
	{
		const APawn* Pawn = GetSeat(MoverSeat)->GetSeatedPawn();
		const UEGChessAnimationSet* AnimSet = GetAnimationSetFor(Pawn);
		const float Rate = (ActiveRules.MoveAnimation == EEGChessMoveAnimation::Fast && AnimSet) ? AnimSet->FastPlayRate : 1.0f;
		FEGChessMoveTiming CharacterTiming;
		if (GetSeat(MoverSeat)->PlayMoveClip(Rate, IsClockOnSeatRight(MoverSeat), CharacterTiming))
		{
			if (AnimSet && !AnimSet->bPieceWaitsForHand)
			{
				// The piece leaves at once on its own timing; the clip still presses the clock.
				CharacterTiming.bPieceFollowsHand = false;
				CharacterTiming.PickTime = 0.0f;
				CharacterTiming.PlaceTime = MoveTime;
				CharacterTiming.ClockPressTime = FMath::Max(CharacterTiming.ClockPressTime, CharacterTiming.PlaceTime);
			}
			Timing = CharacterTiming;
		}
	}

	if (bVisualsReady)
	{
		PieceVisuals->PresentMove(Record, PlyIndex, MoverSeat, Timing);
	}

	OnMoveCommitted.Broadcast(Record);
	if (UEGChessPlayerComponent* Driver = LocalDriver.Get())
	{
		Driver->HandleMoveCommitted(Record);
	}
	RefreshMarks();
}

void AEGChessTableActor::MulticastTakeback_Implementation(int32 NewLength)
{
	if (bVisualsReady)
	{
		PieceVisuals->PresentTakeback(MoveHistory, NewLength);
	}
	OnTakeback.Broadcast();
	RefreshMarks();
}

void AEGChessTableActor::MulticastReaction_Implementation(EEGChessSeat Seat, EEGChessReaction Reaction, int32 Variant)
{
	GetSeat(Seat)->PlayReaction(Reaction, Variant);
}

void AEGChessTableActor::MulticastGameEnded_Implementation(const FEGChessResult& EndResult)
{
	FEGChessEventContext Context;
	Context.Event = EEGChessEvent::GameEnded;
	Context.Location = GetBoardGeometry().GetCenterWorld();
	TriggerEvent(Context);

	if (bVisualsReady)
	{
		PieceVisuals->PresentGameEnd(EndResult);
	}
	if (UEGChessPlayerComponent* Driver = LocalDriver.Get())
	{
		if (EndResult.IsDraw())
		{
			TriggerLocalEvent(EEGChessEvent::ResultDraw);
		}
		else
		{
			TriggerLocalEvent(EndResult.GetWinner() == Driver->GetLocalColor() ? EEGChessEvent::ResultWin : EEGChessEvent::ResultLose);
		}
	}
}

// =============================================================================================
// Presentation routing
// =============================================================================================

void AEGChessTableActor::TriggerEvent(const FEGChessEventContext& Context)
{
	PlayChessSound(Context.Event, Context);
	if (Context.bSpatial)
	{
		SpawnChessEffect(Context.Event, Context);
	}
	OnChessEvent.Broadcast(Context);
}

void AEGChessTableActor::TriggerLocalEvent(EEGChessEvent Event)
{
	const UEGChessPlayerComponent* Driver = LocalDriver.Get();
	if (!Driver)
	{
		return;
	}
	FEGChessEventContext Context;
	Context.Event = Event;
	Context.bSpatial = false;
	Context.Seat = Driver->GetSeat();
	Context.PieceColor = Driver->GetLocalColor();
	Context.bLocalPlayerActed = true;
	Context.TimeLeft = GetRemainingTime(Driver->GetLocalColor());
	TriggerEvent(Context);
}

void AEGChessTableActor::PlayChessSound_Implementation(EEGChessEvent Event, const FEGChessEventContext& Context)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (Event == EEGChessEvent::ClockLowTimeLoop && !Context.bStart)
	{
		if (LowTimeLoop)
		{
			LowTimeLoop->FadeOut(0.2f, 0.0f);
			LowTimeLoop = nullptr;
		}
		return;
	}

	const UEGChessSoundSet* Set = GetSoundSet();
	const FEGChessSoundEntry* Entry = Set ? Set->Sounds.Find(Event) : nullptr;
	if (!Entry)
	{
		return;
	}
	USoundBase* Sound = nullptr;
	if (const TSoftObjectPtr<USoundBase>* Override = Entry->PieceOverrides.Find(Context.PieceType))
	{
		Sound = EGChessEngineAssets::Resolve(*Override);
	}
	if (!Sound)
	{
		Sound = EGChessEngineAssets::Resolve(Entry->Sound);
	}
	if (!Sound)
	{
		return;
	}

	const float Pitch = FMath::FRandRange(Entry->PitchMin, FMath::Max(Entry->PitchMin, Entry->PitchMax));
	USoundAttenuation* Attenuation = EGChessEngineAssets::Resolve(Entry->Attenuation);
	USoundConcurrency* Concurrency = EGChessEngineAssets::Resolve(Entry->Concurrency);

	if (Event == EEGChessEvent::ClockLowTimeLoop)
	{
		if (!LowTimeLoop)
		{
			LowTimeLoop = UGameplayStatics::SpawnSoundAttached(Sound, ClockMesh, NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, true, Entry->Volume, Pitch, 0.0f, Attenuation, Concurrency, true);
		}
		return;
	}

	if (Context.bSpatial)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Context.Location, FRotator::ZeroRotator, Entry->Volume, Pitch, 0.0f, Attenuation, Concurrency, this);
	}
	else
	{
		UGameplayStatics::PlaySound2D(this, Sound, Entry->Volume, Pitch, 0.0f, Concurrency, this);
	}
}

void AEGChessTableActor::SpawnChessEffect_Implementation(EEGChessEvent Event, const FEGChessEventContext& Context)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const UEGChessEffectsSet* Set = GetEffectsSet();
	const FEGChessEffectEntry* Entry = Set ? Set->Effects.Find(Event) : nullptr;
	UNiagaraSystem* System = Entry ? EGChessEngineAssets::Resolve(Entry->System) : nullptr;
	if (System)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, System, Context.Location, FRotator::ZeroRotator, FVector(Entry->Scale));
	}
}

// =============================================================================================
// Seats (every machine)
// =============================================================================================

void AEGChessTableActor::HandleSeatOccupantChanged(EEGChessSeat Seat)
{
	if (GetSeat(Seat)->IsOccupied())
	{
		EnsureInteractionContentLoaded();
	}
	OnSeatChanged.Broadcast(Seat);
	NotifyStateChanged();
}

void AEGChessTableActor::HandleSeatActionStarted(EEGChessSeat Seat, EEGChessSeatAction Action)
{
	const UEGChessSeatComponent* SeatComponent = GetSeat(Seat);
	// A late joiner replays the state; only a fresh action makes a sound.
	if (GetServerTime() - SeatComponent->GetAnimState().StartServerTime > 0.75)
	{
		return;
	}
	FEGChessEventContext Context;
	Context.Event = Action == EEGChessSeatAction::Exiting ? EEGChessEvent::StandUp : EEGChessEvent::SitDown;
	Context.Seat = Seat;
	Context.Location = SeatComponent->Chair ? SeatComponent->Chair->GetComponentLocation() : SeatComponent->GetComponentLocation();
	Context.bLocalPlayerActed = IsSeatLocallyControlled(Seat);
	Context.bMovedByCharacter = true;
	TriggerEvent(Context);
}

void AEGChessTableActor::HandleSeatExitFinished(EEGChessSeat Seat)
{
	if (!HasAuthority())
	{
		return;
	}
	UnbindOccupantDelegates(Seat);
	GetSeat(Seat)->ClearOccupant();
	UpdatePhaseAfterSeatChange();
}

void AEGChessTableActor::HandleCharacterNotify(EEGChessSeat Seat, FName NotifyName)
{
	const UEGChessAnimationSet* Set = GetAnimationSetFor(GetSeat(Seat)->GetSeatedPawn());
	const FName Pick = Set ? Set->NotifyPick : FName(TEXT("Chess.Pick"));
	const FName Place = Set ? Set->NotifyPlace : FName(TEXT("Chess.Place"));
	const FName ClockPress = Set ? Set->NotifyClockPress : FName(TEXT("Chess.ClockPress"));
	PieceVisuals->HandleCharacterNotify(Seat, NotifyName, Pick, Place, ClockPress);
}

void AEGChessTableActor::HandleSitFocusChanged(bool bFocused, UPrimitiveComponent* HitComponent)
{
	const UEGChessTableStyle* Style = GetTableStyle();
	const int32 Stencil = Style ? Style->ChairOutlineStencil : 0;

	if (UPrimitiveComponent* Previous = FocusedChair.Get())
	{
		Previous->SetRenderCustomDepth(false);
	}
	FocusedChair.Reset();

	if (!bFocused || Stencil <= 0 || !HitComponent)
	{
		return;
	}
	for (UEGChessSeatComponent* Seat : { SeatA.Get(), SeatB.Get() })
	{
		if (Seat->Chair == HitComponent && !Seat->IsOccupied())
		{
			HitComponent->SetRenderCustomDepth(true);
			HitComponent->SetCustomDepthStencilValue(Stencil);
			FocusedChair = HitComponent;
		}
	}
}

void AEGChessTableActor::SetLocalDriver(UEGChessPlayerComponent* Driver)
{
	LocalDriver = Driver;
	RefreshMarks();
}

void AEGChessTableActor::RefreshMarks()
{
	if (!bVisualsReady)
	{
		return;
	}
	FEGChessFeedbackInput Input;
	Input.Position = &Position;
	Input.Phase = Phase;
	Input.Result = Result;
	Input.bHasLastMove = MoveHistory.Num() > 0;
	if (Input.bHasLastMove)
	{
		Input.LastMove = MoveHistory.Last().Move;
	}
	if (const UEGChessPlayerComponent* Driver = LocalDriver.Get())
	{
		Driver->FillFeedbackInput(Input);
	}

	FEGChessSquareMarks Marks;
	EGChessBoardFeedback::Build(Input, Marks);
	Feedback->ApplyMarks(Marks, GetBoardGeometry());
}

// =============================================================================================
// Server: seats
// =============================================================================================

bool AEGChessTableActor::CanPawnSit(const APawn* Pawn) const
{
	if (!Pawn || !Pawn->GetController() || !Pawn->GetController()->IsPlayerController())
	{
		return false;
	}
	EEGChessSeat Existing;
	if (FindSeatOfPawn(Pawn, Existing))
	{
		return false;
	}
	if (ActiveRules.OpponentPolicy == EEGChessOpponentPolicy::AIvsAI)
	{
		return false;
	}
	return !SeatA->IsOccupied() || !SeatB->IsOccupied();
}

bool AEGChessTableActor::RequestSeat(APawn* Pawn, UPrimitiveComponent* HitComponent)
{
	if (!HasAuthority() || !Pawn)
	{
		return false;
	}
	APlayerController* Controller = Cast<APlayerController>(Pawn->GetController());
	if (!Controller)
	{
		return false;
	}
	if (!CanPawnSit(Pawn))
	{
		if (UEGChessPlayerComponent* Component = UEGChessPlayerComponent::FindOrAdd(Controller, PlayerComponentClass))
		{
			Component->ClientNotice(EEGChessNotice::SeatTaken);
		}
		return false;
	}

	// The chair looked at, else the nearest free one.
	UEGChessSeatComponent* Chosen = nullptr;
	for (UEGChessSeatComponent* Seat : { SeatA.Get(), SeatB.Get() })
	{
		if (!Seat->IsOccupied() && HitComponent && (Seat->Chair == HitComponent || HitComponent->IsAttachedTo(Seat)))
		{
			Chosen = Seat;
		}
	}
	if (!Chosen)
	{
		float Best = MAX_flt;
		for (UEGChessSeatComponent* Seat : { SeatA.Get(), SeatB.Get() })
		{
			const float Distance = FVector::DistSquared(Seat->GetComponentLocation(), Pawn->GetActorLocation());
			if (!Seat->IsOccupied() && Distance < Best)
			{
				Best = Distance;
				Chosen = Seat;
			}
		}
	}
	if (!Chosen)
	{
		return false;
	}

	FEGChessSeatOccupant Occupant;
	Occupant.PlayerState = Controller->PlayerState;
	Occupant.Pawn = Pawn;
	if (Controller->PlayerState)
	{
		Occupant.UniqueId = Controller->PlayerState->GetUniqueId();
	}
	Chosen->SetOccupant(Occupant);
	BindOccupantDelegates(Chosen->SeatId);

	if (UEGChessPlayerComponent* Component = UEGChessPlayerComponent::FindOrAdd(Controller, PlayerComponentClass))
	{
		Component->SetActiveTable(this, Chosen->SeatId);
	}

	EnsureInteractionContentLoaded();
	Chosen->BeginEnter(Chosen->ChooseEntrySide(Pawn), Pawn->GetActorTransform());

	const EEGChessSeat Other = EGChess::OtherSeat(Chosen->SeatId);
	if (GetSeat(Other)->GetOccupantRef().IsPlayer())
	{
		NotifyPlayer(Other, EEGChessNotice::OpponentJoined);
	}

	UpdatePhaseAfterSeatChange();
	return true;
}

void AEGChessTableActor::OccupyWithAI(EEGChessSeat Seat)
{
	FEGChessSeatOccupant Occupant;
	Occupant.bAI = true;
	GetSeat(Seat)->SetOccupant(Occupant);
}

void AEGChessTableActor::SetSeatPawn(EEGChessSeat Seat, APawn* Pawn)
{
	if (!HasAuthority())
	{
		return;
	}
	UEGChessSeatComponent* SeatComponent = GetSeat(Seat);
	FEGChessSeatOccupant Occupant = SeatComponent->GetOccupantRef();
	if (!Occupant.bAI)
	{
		UE_LOG(LogEGChess, Warning, TEXT("%s: SetSeatPawn only gives the AI's seat a body."), *GetName());
		return;
	}
	Occupant.Pawn = Pawn;
	SeatComponent->SetOccupant(Occupant);
	if (Pawn)
	{
		EnsureInteractionContentLoaded();
		SeatComponent->SnapSeated();
	}
}

void AEGChessTableActor::FreeSeat(EEGChessSeat Seat, bool bAnimatedExit, bool bImmediate)
{
	UEGChessSeatComponent* SeatComponent = GetSeat(Seat);
	const FEGChessSeatOccupant Occupant = SeatComponent->GetOccupantRef();

	if (UEGChessPlayerComponent* Component = GetPlayerComponent(Seat))
	{
		Component->SetActiveTable(nullptr, Seat);
	}

	const EEGChessSeat Other = EGChess::OtherSeat(Seat);
	if (Occupant.IsPlayer() && GetSeat(Other)->GetOccupantRef().IsPlayer())
	{
		NotifyPlayer(Other, EEGChessNotice::OpponentLeft);
	}

	if (bAnimatedExit && Occupant.Pawn && !Occupant.bAI)
	{
		// The seat stays taken until the exit ends; HandleSeatExitFinished frees it.
		SeatComponent->BeginExit(SeatComponent->ChooseExitSide(Occupant.Pawn), bImmediate);
		return;
	}

	UnbindOccupantDelegates(Seat);
	SeatComponent->ClearOccupant();
	UpdatePhaseAfterSeatChange();
}

void AEGChessTableActor::UpdatePhaseAfterSeatChange()
{
	if (!HasAuthority() || Phase == EEGChessPhase::Playing || Phase == EEGChessPhase::Starting)
	{
		return;
	}
	if (ActiveRules.OpponentPolicy == EEGChessOpponentPolicy::AIvsAI)
	{
		return;
	}

	const bool bAOccupied = SeatA->IsOccupied();
	const bool bBOccupied = SeatB->IsOccupied();
	const int32 Players = (SeatA->GetOccupantRef().IsPlayer() ? 1 : 0) + (SeatB->GetOccupantRef().IsPlayer() ? 1 : 0);

	if (Players == 0)
	{
		for (UEGChessSeatComponent* Seat : { SeatA.Get(), SeatB.Get() })
		{
			if (Seat->IsAI())
			{
				Seat->ClearOccupant();
			}
		}
		SetPhase(EEGChessPhase::Idle);
		return;
	}

	if (bAOccupied && bBOccupied)
	{
		if (Phase == EEGChessPhase::Idle || Phase == EEGChessPhase::WaitingForOpponent)
		{
			BeginStarting(false);
		}
		return;
	}

	// One player, one empty seat.
	if (ActiveRules.OpponentPolicy == EEGChessOpponentPolicy::AIOnly)
	{
		OccupyWithAI(bAOccupied ? EEGChessSeat::B : EEGChessSeat::A);
		BeginStarting(false);
		return;
	}
	if (Phase != EEGChessPhase::WaitingForOpponent)
	{
		SetPhase(EEGChessPhase::WaitingForOpponent);
	}
}

void AEGChessTableActor::ForceStandUp(APawn* Pawn, bool bForfeit, bool bImmediate)
{
	EEGChessSeat Seat;
	if (!HasAuthority() || !FindSeatOfPawn(Pawn, Seat))
	{
		return;
	}
	if (bForfeit && Phase == EEGChessPhase::Playing)
	{
		ForfeitSeat(Seat);
	}
	FreeSeat(Seat, true, bImmediate);
}

void AEGChessTableActor::BindOccupantDelegates(EEGChessSeat Seat)
{
	UnbindOccupantDelegates(Seat);
	const int32 Index = EGChess::SeatIndex(Seat);
	const FEGChessSeatOccupant& Occupant = GetSeat(Seat)->GetOccupantRef();
	if (APawn* Pawn = Occupant.Pawn)
	{
		Pawn->OnDestroyed.AddUniqueDynamic(this, &AEGChessTableActor::HandleOccupantPawnDestroyed);
		BoundPawns[Index] = Pawn;
		if (AController* Controller = Pawn->GetController())
		{
			Controller->OnPossessedPawnChanged.AddUniqueDynamic(this, &AEGChessTableActor::HandleOccupantPossessionChanged);
			OccupantControllers[Index] = Controller;
		}
	}
}

void AEGChessTableActor::UnbindOccupantDelegates(EEGChessSeat Seat)
{
	const int32 Index = EGChess::SeatIndex(Seat);
	if (APawn* Pawn = BoundPawns[Index].Get())
	{
		const int32 Other = 1 - Index;
		if (BoundPawns[Other].Get() != Pawn)
		{
			Pawn->OnDestroyed.RemoveDynamic(this, &AEGChessTableActor::HandleOccupantPawnDestroyed);
		}
	}
	BoundPawns[Index].Reset();
	OccupantControllers[Index].Reset();
	// Possession delegates stay bound: the same handler also reclaims seats after reconnects.
}

void AEGChessTableActor::HandleOccupantPawnDestroyed(AActor* DestroyedActor)
{
	if (!HasAuthority())
	{
		return;
	}
	for (const EEGChessSeat Seat : { EEGChessSeat::A, EEGChessSeat::B })
	{
		if (BoundPawns[EGChess::SeatIndex(Seat)].Get() != DestroyedActor)
		{
			continue;
		}
		const FEGChessSeatOccupant& Occupant = GetSeat(Seat)->GetOccupantRef();
		BoundPawns[EGChess::SeatIndex(Seat)].Reset();
		if (Occupant.bDisconnected)
		{
			continue; // the seat is being held for a reconnect
		}
		// Still connected, but can no longer play.
		if (Phase == EEGChessPhase::Playing && !Occupant.bAI)
		{
			ForfeitSeat(Seat);
		}
		FreeSeat(Seat, false, true);
	}
}

void AEGChessTableActor::HandleOccupantPossessionChanged(APawn* OldPawn, APawn* NewPawn)
{
	if (!HasAuthority())
	{
		return;
	}

	// A reconnected player's new pawn reclaims a held seat.
	if (NewPawn)
	{
		if (APlayerController* Controller = Cast<APlayerController>(NewPawn->GetController()))
		{
			TryReclaimSeat(Controller);
		}
	}

	// A seated pawn that lost its controller's possession has left the table.
	for (const EEGChessSeat Seat : { EEGChessSeat::A, EEGChessSeat::B })
	{
		const FEGChessSeatOccupant& Occupant = GetSeat(Seat)->GetOccupantRef();
		if (OldPawn && Occupant.Pawn == OldPawn && NewPawn != OldPawn && !Occupant.bDisconnected && !Occupant.bAI)
		{
			if (Phase == EEGChessPhase::Playing)
			{
				ForfeitSeat(Seat);
			}
			FreeSeat(Seat, false, true);
		}
	}
}

void AEGChessTableActor::HandleLogout(AGameModeBase* GameMode, AController* Exiting)
{
	if (!Exiting || GameMode == nullptr || GameMode->GetWorld() != GetWorld())
	{
		return;
	}
	EEGChessSeat Seat;
	if (!FindSeatOfPlayer(Exiting->PlayerState, Seat))
	{
		return;
	}

	if (Phase == EEGChessPhase::Playing)
	{
		const double ForfeitAt = GetServerTime() + ActiveRules.DisconnectForfeitSeconds;
		UEGChessSeatComponent* SeatComponent = GetSeat(Seat);
		SeatComponent->MarkDisconnected(ForfeitAt);
		NotifyPlayer(EGChess::OtherSeat(Seat), EEGChessNotice::OpponentDisconnected);

		FTimerDelegate Forfeit = FTimerDelegate::CreateWeakLambda(this, [this, Seat]()
		{
			if (GetSeat(Seat)->GetOccupantRef().bDisconnected)
			{
				ForfeitSeat(Seat);
				UnbindOccupantDelegates(Seat);
				GetSeat(Seat)->ClearOccupant();
				UpdatePhaseAfterSeatChange();
			}
		});
		GetWorldTimerManager().SetTimer(ForfeitTimers[EGChess::SeatIndex(Seat)], Forfeit, FMath::Max(0.1f, ActiveRules.DisconnectForfeitSeconds), false);
		return;
	}

	FreeSeat(Seat, false, true);
}

void AEGChessTableActor::HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer)
{
	if (!NewPlayer || GameMode == nullptr || GameMode->GetWorld() != GetWorld())
	{
		return;
	}
	if (!SeatA->GetOccupantRef().bDisconnected && !SeatB->GetOccupantRef().bDisconnected)
	{
		return;
	}
	NewPlayer->OnPossessedPawnChanged.AddUniqueDynamic(this, &AEGChessTableActor::HandleOccupantPossessionChanged);
	TryReclaimSeat(NewPlayer);
}

void AEGChessTableActor::TryReclaimSeat(APlayerController* Controller)
{
	APlayerState* PlayerState = Controller ? Controller->PlayerState.Get() : nullptr;
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (!PlayerState || !Pawn)
	{
		return;
	}
	const FUniqueNetIdRepl Id = PlayerState->GetUniqueId();
	for (const EEGChessSeat Seat : { EEGChessSeat::A, EEGChessSeat::B })
	{
		UEGChessSeatComponent* SeatComponent = GetSeat(Seat);
		FEGChessSeatOccupant Occupant = SeatComponent->GetOccupantRef();
		if (!Occupant.bDisconnected || !Id.IsValid() || !(Occupant.UniqueId == Id))
		{
			continue;
		}
		Occupant.bDisconnected = false;
		Occupant.PlayerState = PlayerState;
		Occupant.Pawn = Pawn;
		SeatComponent->SetOccupant(Occupant);
		GetWorldTimerManager().ClearTimer(ForfeitTimers[EGChess::SeatIndex(Seat)]);
		BindOccupantDelegates(Seat);
		if (UEGChessPlayerComponent* Component = UEGChessPlayerComponent::FindOrAdd(Controller, PlayerComponentClass))
		{
			Component->SetActiveTable(this, Seat);
		}
		SeatComponent->SnapSeated();
		NotifyPlayer(EGChess::OtherSeat(Seat), EEGChessNotice::OpponentReconnected);
		return;
	}
}

// =============================================================================================
// Server: requests
// =============================================================================================

bool AEGChessTableActor::ResolveRequester(APlayerController* Controller, EEGChessSeat& OutSeat) const
{
	return HasAuthority() && Controller && FindSeatOfPlayer(Controller->PlayerState, OutSeat);
}

UEGChessPlayerComponent* AEGChessTableActor::GetPlayerComponent(EEGChessSeat Seat) const
{
	const APlayerState* PlayerState = GetSeat(Seat)->GetOccupantRef().PlayerState;
	return PlayerState ? UEGChessPlayerComponent::Find(PlayerState->GetPlayerController()) : nullptr;
}

void AEGChessTableActor::NotifyPlayer(EEGChessSeat Seat, EEGChessNotice Notice)
{
	if (UEGChessPlayerComponent* Component = GetPlayerComponent(Seat))
	{
		Component->ClientNotice(Notice);
	}
}

void AEGChessTableActor::HandleLeaveSeat(APlayerController* Controller)
{
	EEGChessSeat Seat;
	if (!ResolveRequester(Controller, Seat))
	{
		return;
	}
	if (Phase == EEGChessPhase::Playing)
	{
		MulticastReaction(Seat, EEGChessReaction::Resign, 0);
		FEGChessResult Resigned;
		Resigned.Result = GetSeatColor(Seat) == EEGChessColor::White ? EEGChessGameResult::BlackWins : EEGChessGameResult::WhiteWins;
		Resigned.Reason = EEGChessEndReason::Resignation;
		EndGame(Resigned);
	}
	FreeSeat(Seat, true, false);
}

void AEGChessTableActor::HandleSubmitMove(APlayerController* Controller, const FEGChessMove& Move)
{
	EEGChessSeat Seat;
	UEGChessPlayerComponent* Component = UEGChessPlayerComponent::Find(Controller);
	FEGChessMove Legal;
	const bool bValid = ResolveRequester(Controller, Seat)
		&& Phase == EEGChessPhase::Playing
		&& GetSeatColor(Seat) == Position.SideToMove
		&& EGChessRules::FindLegalMove(Position, Move.From, Move.To, Move.Promotion, Legal);
	if (!bValid)
	{
		if (Component)
		{
			Component->ClientMoveRejected();
		}
		return;
	}
	CommitMove(Legal);
}

void AEGChessTableActor::HandleResign(APlayerController* Controller)
{
	EEGChessSeat Seat;
	if (!ResolveRequester(Controller, Seat) || Phase != EEGChessPhase::Playing)
	{
		return;
	}
	MulticastReaction(Seat, EEGChessReaction::Resign, 0);
	FEGChessResult Resigned;
	Resigned.Result = GetSeatColor(Seat) == EEGChessColor::White ? EEGChessGameResult::BlackWins : EEGChessGameResult::WhiteWins;
	Resigned.Reason = EEGChessEndReason::Resignation;
	EndGame(Resigned);
}

void AEGChessTableActor::HandleOfferDraw(APlayerController* Controller)
{
	EEGChessSeat Seat;
	if (!ResolveRequester(Controller, Seat) || Phase != EEGChessPhase::Playing || DrawOffer.bPending)
	{
		return;
	}
	const EEGChessColor Color = GetSeatColor(Seat);
	if (DrawOffer.GetCooldown(Color) > 0)
	{
		return;
	}

	DrawOffer.bPending = true;
	DrawOffer.OfferedBy = Color;
	DrawOffer.SetCooldown(Color, ActiveRules.DrawOfferCooldownMoves);
	OnRep_DrawOffer();
	MulticastReaction(Seat, EEGChessReaction::OfferDraw, 0);

	const EEGChessSeat Other = EGChess::OtherSeat(Seat);
	if (IsSeatAI(Other))
	{
		UEGChessAI* AI = GetOrCreateAI();
		const UEGChessAIProfile* Profile = GetEffectiveAIProfile();
		const float Delay = (AI && Profile) ? AI->GetThinkDelay(Position, *Profile, AIRandom) : 1.0f;
		FTimerDelegate Respond = FTimerDelegate::CreateWeakLambda(this, [this, Other, Seat]()
		{
			if (!DrawOffer.bPending || Phase != EEGChessPhase::Playing)
			{
				return;
			}
			UEGChessAI* AI = GetOrCreateAI();
			const UEGChessAIProfile* Profile = GetEffectiveAIProfile();
			FEGChessPosition Perspective = Position;
			Perspective.SideToMove = GetSeatColor(Other);
			Perspective.EnPassantSquare = -1;
			if (AI && Profile && AI->RespondToDrawOffer(Perspective, *Profile, AIRandom))
			{
				FEGChessResult Agreed;
				Agreed.Result = EEGChessGameResult::Draw;
				Agreed.Reason = EEGChessEndReason::Agreement;
				EndGame(Agreed);
			}
			else
			{
				DrawOffer.bPending = false;
				OnRep_DrawOffer();
				NotifyPlayer(Seat, EEGChessNotice::DrawDeclined);
			}
		});
		GetWorldTimerManager().SetTimer(AIDrawTimer, Respond, FMath::Max(0.1f, Delay), false);
	}
}

void AEGChessTableActor::HandleRespondDraw(APlayerController* Controller, bool bAccept)
{
	EEGChessSeat Seat;
	if (!ResolveRequester(Controller, Seat) || Phase != EEGChessPhase::Playing || !DrawOffer.bPending || DrawOffer.OfferedBy == GetSeatColor(Seat))
	{
		return;
	}
	if (bAccept)
	{
		FEGChessResult Agreed;
		Agreed.Result = EEGChessGameResult::Draw;
		Agreed.Reason = EEGChessEndReason::Agreement;
		EndGame(Agreed);
		return;
	}
	DrawOffer.bPending = false;
	OnRep_DrawOffer();
	NotifyPlayer(EGChess::OtherSeat(Seat), EEGChessNotice::DrawDeclined);
}

void AEGChessTableActor::HandleRequestTakeback(APlayerController* Controller)
{
	EEGChessSeat Seat;
	if (!ResolveRequester(Controller, Seat) || Phase != EEGChessPhase::Playing || !IsSeatAI(EGChess::OtherSeat(Seat)) || MoveHistory.Num() == 0)
	{
		return;
	}
	const UEGChessAIProfile* Profile = GetEffectiveAIProfile();
	if (Profile && Profile->MaxTakebacks >= 0 && TakebacksUsed >= Profile->MaxTakebacks)
	{
		return;
	}

	// Undo the player's last move, and the AI's reply to it if there was one.
	const EEGChessColor Mine = GetSeatColor(Seat);
	int32 Plies = Position.SideToMove == Mine ? 2 : 1;
	if (MoveHistory.Num() < Plies)
	{
		return;
	}
	const int32 NewLength = MoveHistory.Num() - Plies;

	GetWorldTimerManager().ClearTimer(AITimer);
	GetWorldTimerManager().ClearTimer(ThinkingTimer);

	if (NewLength > 0)
	{
		Clock.WhiteRemaining = MoveHistory[NewLength - 1].WhiteTimeAfter;
		Clock.BlackRemaining = MoveHistory[NewLength - 1].BlackTimeAfter;
	}
	else if (ActiveRules.TimeControl)
	{
		Clock.WhiteRemaining = Clock.BlackRemaining = ActiveRules.TimeControl->BaseSeconds;
	}

	MoveHistory.SetNum(NewLength);
	RebuildPosition();
	Clock.RunningColor = Position.SideToMove;
	Clock.TurnStartServerTime = GetServerTime();
	OnRep_Clock();
	ArmFlagTimer();

	if (DrawOffer.bPending)
	{
		DrawOffer.bPending = false;
		OnRep_DrawOffer();
	}
	++TakebacksUsed;

	MulticastTakeback(NewLength);
	NotifyPlayer(Seat, EEGChessNotice::TakebackDone);
	NotifyStateChanged();
	ScheduleAITurn();
	ScheduleThinkingIdle();
}

void AEGChessTableActor::HandleRequestRematch(APlayerController* Controller)
{
	EEGChessSeat Seat;
	if (!ResolveRequester(Controller, Seat) || Phase != EEGChessPhase::GameOver)
	{
		return;
	}
	RematchRequests |= static_cast<uint8>(1 << EGChess::SeatIndex(Seat));
	OnRep_Rematch();

	const EEGChessSeat Other = EGChess::OtherSeat(Seat);
	if (IsSeatAI(Other) || HasRematchRequest(Other))
	{
		BeginStarting(true);
	}
	else
	{
		NotifyPlayer(Other, EEGChessNotice::RematchRequested);
	}
}

void AEGChessTableActor::HandleStartVersusAI(APlayerController* Controller)
{
	EEGChessSeat Seat;
	if (!ResolveRequester(Controller, Seat) || Phase != EEGChessPhase::WaitingForOpponent || ActiveRules.OpponentPolicy != EEGChessOpponentPolicy::PlayersOrAI)
	{
		return;
	}
	const EEGChessSeat Other = EGChess::OtherSeat(Seat);
	if (GetSeat(Other)->IsOccupied())
	{
		return;
	}
	OccupyWithAI(Other);
	BeginStarting(false);
}

void AEGChessTableActor::SetMatchRules(const FEGChessMatchRules& NewRules)
{
	if (!HasAuthority())
	{
		return;
	}
	if (Phase == EEGChessPhase::Playing || Phase == EEGChessPhase::Starting)
	{
		UE_LOG(LogEGChess, Warning, TEXT("%s: match rules changed during a game; they apply from the next game."), *GetName());
	}
	ActiveRules = NewRules;
	OnRep_Rules();
}

bool AEGChessTableActor::DebugSetPosition(const FString& Fen)
{
	if (!HasAuthority())
	{
		return false;
	}
	FEGChessPosition Test;
	FString Error;
	if (!Test.FromFEN(Fen, &Error))
	{
		UE_LOG(LogEGChess, Warning, TEXT("%s: bad FEN (%s)"), *GetName(), *Error);
		return false;
	}
	GetWorldTimerManager().ClearTimer(AITimer);
	StartFEN = Fen;
	MoveHistory.Reset();
	Result = FEGChessResult();
	RebuildPosition();
	OnRep_Result();
	if (Phase == EEGChessPhase::Playing)
	{
		Clock.RunningColor = Position.SideToMove;
		Clock.TurnStartServerTime = GetServerTime();
		OnRep_Clock();
		ArmFlagTimer();
		ScheduleAITurn();
	}
	ReconcileVisuals();
	NotifyStateChanged();
	RefreshMarks();
	return true;
}

void AEGChessTableActor::DebugStartAIvsAI()
{
	if (!HasAuthority())
	{
		return;
	}
	ActiveRules.OpponentPolicy = EEGChessOpponentPolicy::AIvsAI;
	OnRep_Rules();
	for (const EEGChessSeat Seat : { EEGChessSeat::A, EEGChessSeat::B })
	{
		if (GetSeat(Seat)->GetOccupantRef().IsPlayer())
		{
			FreeSeat(Seat, false, true);
		}
		OccupyWithAI(Seat);
	}
	BeginStarting(false);
}

// =============================================================================================
// Server: game flow
// =============================================================================================

void AEGChessTableActor::SetPhase(EEGChessPhase NewPhase, double EndServerTime)
{
	Phase = NewPhase;
	PhaseEndServerTime = EndServerTime;
	OnRep_Phase();
}

void AEGChessTableActor::BeginStarting(bool bRematch)
{
	if (!HasAuthority())
	{
		return;
	}
	FTimerManager& Timers = GetWorldTimerManager();
	Timers.ClearTimer(RestartTimer);
	Timers.ClearTimer(FlagTimer);
	Timers.ClearTimer(AITimer);
	Timers.ClearTimer(AIDrawTimer);
	Timers.ClearTimer(ThinkingTimer);

	WhiteSeat = (bRematch && ActiveRules.bSwapColoursOnRematch) ? EGChess::OtherSeat(WhiteSeat) : (bRematch ? WhiteSeat : ActiveRules.FirstWhiteSeat);
	OnRep_WhiteSeat();

	StartFEN = FEGChessPosition::StartFEN();
	MoveHistory.Reset();
	Result = FEGChessResult();
	DrawOffer = FEGChessDrawOfferState();
	RematchRequests = 0;
	LastRematchBits = 0;
	TakebacksUsed = 0;
	LastCheckReactionPly = -2;

	Clock = FEGChessClockState();
	if (const UEGChessTimeControl* TimeControl = ActiveRules.TimeControl)
	{
		Clock.bTimed = true;
		Clock.WhiteRemaining = Clock.BlackRemaining = TimeControl->BaseSeconds;
		Clock.Increment = TimeControl->IncrementSeconds;
		Clock.LowTimeThreshold = TimeControl->LowTimeThresholdSeconds;
	}

	RebuildPosition();
	++GameIndex;
	OnRep_GameIndex();
	OnRep_Clock();
	OnRep_DrawOffer();
	OnRep_Rematch();

	if (IsAIGame())
	{
		AIRandom.Initialize(FMath::Rand());
		GetOrCreateAI();
	}

	const UEGChessPieceSet* Set = GetPieceSet();
	const float Delay = (Set ? Set->ResetTime : 1.0f) + ActiveRules.StartCountdownSeconds;
	SetPhase(EEGChessPhase::Starting, GetServerTime() + Delay);
	Timers.SetTimer(StartTimer, this, &AEGChessTableActor::BeginPlaying, FMath::Max(0.05f, Delay), false);
	NotifyStateChanged();
}

void AEGChessTableActor::BeginPlaying()
{
	if (!HasAuthority() || Phase != EEGChessPhase::Starting)
	{
		return;
	}
	Clock.bRunning = Clock.bTimed;
	Clock.RunningColor = Position.SideToMove;
	Clock.TurnStartServerTime = GetServerTime();
	OnRep_Clock();

	bHasPlayedGame = true;
	SetPhase(EEGChessPhase::Playing);
	ArmFlagTimer();
	ScheduleAITurn();
	ScheduleThinkingIdle();
}

void AEGChessTableActor::CommitMove(const FEGChessMove& Move)
{
	const EEGChessColor Mover = Position.SideToMove;
	const double Now = GetServerTime();

	if (Clock.bTimed && Clock.bRunning)
	{
		Clock.SetStored(Mover, Clock.GetRemaining(Mover, Now) + Clock.Increment);
	}

	FEGChessMoveRecord Record;
	Record.Move = Move;
	Record.SAN = EGChessNotation::ToSAN(Position, Move);
	Record.MovedType = Position.TypeAt(Move.From);
	Record.MoverColor = Mover;
	if (Move.HasFlag(EGChessMoveFlags::EnPassant))
	{
		Record.CapturedType = EEGChessPieceType::Pawn;
	}
	else if (Move.IsCapture())
	{
		Record.CapturedType = Position.TypeAt(Move.To);
	}
	FEGChessPosition After = Position;
	After.ApplyMove(Move);
	Record.bGivesCheck = After.IsInCheck(After.SideToMove);
	Record.WhiteTimeAfter = Clock.WhiteRemaining;
	Record.BlackTimeAfter = Clock.BlackRemaining;

	MoveHistory.Add(Record);
	RebuildPosition();

	// Making a move declines an offer the opponent had on the table.
	if (DrawOffer.bPending && DrawOffer.OfferedBy != Mover)
	{
		DrawOffer.bPending = false;
		NotifyPlayer(GetSeatForColor(DrawOffer.OfferedBy), EEGChessNotice::DrawDeclined);
	}
	DrawOffer.SetCooldown(Mover, FMath::Max(0, DrawOffer.GetCooldown(Mover) - 1));
	OnRep_DrawOffer();

	Clock.RunningColor = Position.SideToMove;
	Clock.TurnStartServerTime = Now;
	OnRep_Clock();

	GetWorldTimerManager().ClearTimer(ThinkingTimer);
	MulticastMoveCommitted(Record, MoveHistory.Num() - 1, GetSeatForColor(Mover));
	NotifyStateChanged();

	const EEGChessStatus Status = EGChessRules::Evaluate(Position, GetRepetitionView(), ActiveRules.bAutomaticDrawClaims);
	if (Status != EEGChessStatus::Ongoing)
	{
		EndGame(EGChessRules::ResultFromStatus(Status, Position.SideToMove));
		return;
	}

	ArmFlagTimer();

	// A check reaction, but not on every one of a run of checks by the same side.
	const int32 Ply = MoveHistory.Num() - 1;
	if (Record.bGivesCheck && LastCheckReactionPly != Ply - 2)
	{
		MulticastReaction(GetSeatForColor(Position.SideToMove), EEGChessReaction::Check, 0);
	}
	if (Record.bGivesCheck)
	{
		LastCheckReactionPly = Ply;
	}

	ScheduleAITurn();
	ScheduleThinkingIdle();
}

void AEGChessTableActor::EndGame(const FEGChessResult& NewResult)
{
	if (!HasAuthority() || (Phase != EEGChessPhase::Playing && Phase != EEGChessPhase::Starting))
	{
		return;
	}

	FTimerManager& Timers = GetWorldTimerManager();
	Timers.ClearTimer(StartTimer);
	Timers.ClearTimer(FlagTimer);
	Timers.ClearTimer(AITimer);
	Timers.ClearTimer(AIDrawTimer);
	Timers.ClearTimer(ThinkingTimer);

	if (Clock.bRunning)
	{
		Clock.SetStored(Clock.RunningColor, Clock.GetRemaining(Clock.RunningColor, GetServerTime()));
		Clock.bRunning = false;
		OnRep_Clock();
	}
	if (DrawOffer.bPending)
	{
		DrawOffer.bPending = false;
		OnRep_DrawOffer();
	}

	Result = NewResult;
	OnRep_Result();
	SetPhase(EEGChessPhase::GameOver);
	MulticastGameEnded(Result);

	for (const EEGChessSeat Seat : { EEGChessSeat::A, EEGChessSeat::B })
	{
		if (Result.IsDraw())
		{
			MulticastReaction(Seat, EEGChessReaction::Draw, 0);
		}
		else if (GetSeatColor(Seat) == Result.GetWinner())
		{
			MulticastReaction(Seat, EEGChessReaction::Win, 0);
		}
		else if (Result.Reason != EEGChessEndReason::Resignation)
		{
			MulticastReaction(Seat, EEGChessReaction::Lose, 0); // a resigner is already playing Resign
		}
	}

	if (ActiveRules.OpponentPolicy == EEGChessOpponentPolicy::AIvsAI)
	{
		Timers.SetTimer(RestartTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { BeginStarting(true); }), FMath::Max(0.1f, ActiveRules.AIvsAIRestartDelay), false);
	}
}

void AEGChessTableActor::ForfeitSeat(EEGChessSeat Seat)
{
	if (Phase != EEGChessPhase::Playing)
	{
		return;
	}
	FEGChessResult Forfeit;
	Forfeit.Result = GetSeatColor(Seat) == EEGChessColor::White ? EEGChessGameResult::BlackWins : EEGChessGameResult::WhiteWins;
	Forfeit.Reason = EEGChessEndReason::Forfeit;
	EndGame(Forfeit);
}

void AEGChessTableActor::ArmFlagTimer()
{
	GetWorldTimerManager().ClearTimer(FlagTimer);
	if (!Clock.bTimed || !Clock.bRunning || Phase != EEGChessPhase::Playing)
	{
		return;
	}
	const float Remaining = Clock.GetRemaining(Clock.RunningColor, GetServerTime());
	GetWorldTimerManager().SetTimer(FlagTimer, this, &AEGChessTableActor::HandleFlagFall, FMath::Max(0.01f, Remaining + 0.01f), false);
}

void AEGChessTableActor::HandleFlagFall()
{
	if (Phase != EEGChessPhase::Playing)
	{
		return;
	}
	const EEGChessColor Loser = Clock.RunningColor;
	const EEGChessColor Winner = EGChess::Opposite(Loser);
	Clock.SetStored(Loser, 0.0f);

	FEGChessResult Timeout;
	Timeout.Reason = EEGChessEndReason::Timeout;
	if (!EGChessRules::HasMatingMaterial(Position, Winner))
	{
		Timeout.Result = EEGChessGameResult::Draw; // FIDE: the side with time left could never mate
	}
	else
	{
		Timeout.Result = Winner == EEGChessColor::White ? EEGChessGameResult::WhiteWins : EEGChessGameResult::BlackWins;
	}
	EndGame(Timeout);
}

// =============================================================================================
// Server: AI
// =============================================================================================

UEGChessAIProfile* AEGChessTableActor::GetEffectiveAIProfile() const
{
	if (ActiveRules.AIProfile)
	{
		return ActiveRules.AIProfile;
	}
	if (!FallbackAIProfile)
	{
		AEGChessTableActor* MutableThis = const_cast<AEGChessTableActor*>(this);
		MutableThis->FallbackAIProfile = NewObject<UEGChessAIProfile>(MutableThis, NAME_None, RF_Transient);
	}
	return FallbackAIProfile;
}

UEGChessAI* AEGChessTableActor::GetOrCreateAI()
{
	const UEGChessAIProfile* Profile = GetEffectiveAIProfile();
	UClass* Class = (Profile && Profile->AIClass) ? Profile->AIClass.Get() : UEGChessAI_RuleBased::StaticClass();
	if (Class->HasAnyClassFlags(CLASS_Abstract))
	{
		Class = UEGChessAI_RuleBased::StaticClass();
	}
	if (!AIInstance || AIInstance->GetClass() != Class)
	{
		AIInstance = NewObject<UEGChessAI>(this, Class, NAME_None, RF_Transient);
	}
	return AIInstance;
}

float AEGChessTableActor::EstimateLastMovePresentation() const
{
	if (MoveHistory.Num() == 0)
	{
		return 0.0f;
	}
	const FEGChessMoveRecord& Last = MoveHistory.Last();
	const EEGChessSeat Seat = GetSeatForColor(Last.MoverColor);
	if (ActiveRules.MoveAnimation != EEGChessMoveAnimation::Off)
	{
		const UEGChessAnimationSet* Set = GetAnimationSetFor(GetSeat(Seat)->GetSeatedPawn());
		const float Rate = (ActiveRules.MoveAnimation == EEGChessMoveAnimation::Fast && Set) ? Set->FastPlayRate : 1.0f;
		const float Clip = GetSeat(Seat)->GetMoveClipDuration(Rate, IsClockOnSeatRight(Seat));
		if (Clip > 0.0f)
		{
			return Clip;
		}
	}
	const UEGChessPieceSet* PieceSet = GetPieceSet();
	return (PieceSet ? PieceSet->MoveTime : 0.45f) + 0.4f;
}

void AEGChessTableActor::ScheduleAITurn()
{
	GetWorldTimerManager().ClearTimer(AITimer);
	if (!HasAuthority() || Phase != EEGChessPhase::Playing || !IsSeatAI(GetSeatForColor(Position.SideToMove)))
	{
		return;
	}
	UEGChessAI* AI = GetOrCreateAI();
	const UEGChessAIProfile* Profile = GetEffectiveAIProfile();
	if (!AI || !Profile)
	{
		return;
	}
	// Wait for the last move's presentation first, so the AI never answers mid-animation.
	const float Delay = EstimateLastMovePresentation() + AI->GetThinkDelay(Position, *Profile, AIRandom);
	GetWorldTimerManager().SetTimer(AITimer, this, &AEGChessTableActor::PerformAIMove, FMath::Max(0.05f, Delay), false);
}

void AEGChessTableActor::PerformAIMove()
{
	if (Phase != EEGChessPhase::Playing || !IsSeatAI(GetSeatForColor(Position.SideToMove)))
	{
		return;
	}
	UEGChessAI* AI = GetOrCreateAI();
	const UEGChessAIProfile* Profile = GetEffectiveAIProfile();
	if (!AI || !Profile)
	{
		return;
	}
	const FEGChessMove Chosen = AI->ChooseMove(Position, *Profile, AIRandom);
	FEGChessMove Legal;
	if (Chosen.IsValid() && EGChessRules::FindLegalMove(Position, Chosen.From, Chosen.To, Chosen.Promotion, Legal))
	{
		CommitMove(Legal);
	}
	else
	{
		UE_LOG(LogEGChess, Warning, TEXT("%s: the AI returned no legal move in %s."), *GetName(), *Position.ToFEN());
	}
}

void AEGChessTableActor::ScheduleThinkingIdle()
{
	GetWorldTimerManager().ClearTimer(ThinkingTimer);
	if (Phase != EEGChessPhase::Playing)
	{
		return;
	}
	const EEGChessSeat Seat = GetSeatForColor(Position.SideToMove);
	const UEGChessAnimationSet* Set = GetAnimationSetFor(GetSeat(Seat)->GetSeatedPawn());
	if (!GetSeat(Seat)->GetSeatedPawn() || !Set || Set->ThinkingIdles.Num() == 0)
	{
		return;
	}
	ThinkingPly = MoveHistory.Num();
	GetWorldTimerManager().SetTimer(ThinkingTimer, this, &AEGChessTableActor::PlayThinkingIdle, FMath::Max(1.0f, Set->ThinkingIdleDelay), false);
}

void AEGChessTableActor::PlayThinkingIdle()
{
	if (Phase != EEGChessPhase::Playing || ThinkingPly != MoveHistory.Num())
	{
		return;
	}
	const EEGChessSeat Seat = GetSeatForColor(Position.SideToMove);
	const UEGChessAnimationSet* Set = GetAnimationSetFor(GetSeat(Seat)->GetSeatedPawn());
	if (!Set || Set->ThinkingIdles.Num() == 0)
	{
		return;
	}
	MulticastReaction(Seat, EEGChessReaction::Thinking, FMath::RandRange(0, Set->ThinkingIdles.Num() - 1));
	GetWorldTimerManager().SetTimer(ThinkingTimer, this, &AEGChessTableActor::PlayThinkingIdle, FMath::Max(1.0f, Set->ThinkingIdleMinGap), false);
}

// =============================================================================================
// Tick
// =============================================================================================

void AEGChessTableActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bVisualsReady)
	{
		UpdateClockHands();
	}

	const double Now = GetServerTime();

	if (Phase == EEGChessPhase::Starting && LocalDriver.IsValid())
	{
		const int32 Second = FMath::CeilToInt(static_cast<float>(PhaseEndServerTime - Now));
		if (Second >= 1 && Second <= FMath::CeilToInt(ActiveRules.StartCountdownSeconds) && Second != LastCountdownSecond)
		{
			LastCountdownSecond = Second;
			TriggerLocalEvent(EEGChessEvent::Countdown);
		}
	}

	if (Phase == EEGChessPhase::Playing && Clock.bTimed && Clock.bRunning)
	{
		const float Remaining = Clock.GetRemaining(Clock.RunningColor, Now);
		const bool bLow = Remaining > 0.0f && Remaining <= Clock.LowTimeThreshold;
		if (bLow != bLowTimeLoopPlaying)
		{
			bLowTimeLoopPlaying = bLow;
			FEGChessEventContext Context;
			Context.Event = EEGChessEvent::ClockLowTimeLoop;
			Context.bStart = bLow;
			Context.Location = ClockMesh->GetComponentLocation();
			Context.PieceColor = Clock.RunningColor;
			Context.TimeLeft = Remaining;
			TriggerEvent(Context);
		}
		const int32 ColorIndex = EGChess::ColorIndex(Clock.RunningColor);
		if (bLow && !bLowTimeBroadcast[ColorIndex])
		{
			bLowTimeBroadcast[ColorIndex] = true;
			OnClockLow.Broadcast(Clock.RunningColor);
		}
	}
	else if (bLowTimeLoopPlaying)
	{
		bLowTimeLoopPlaying = false;
		FEGChessEventContext Context;
		Context.Event = EEGChessEvent::ClockLowTimeLoop;
		Context.bStart = false;
		Context.Location = ClockMesh->GetComponentLocation();
		TriggerEvent(Context);
	}

	static const IConsoleVariable* DebugVariable = IConsoleManager::Get().FindConsoleVariable(TEXT("EG.Chess.Debug"));
	if (bDebugDraw || (DebugVariable && DebugVariable->GetInt() > 0))
	{
		DebugDraw();
	}
}

void AEGChessTableActor::DebugDraw() const
{
#if ENABLE_DRAW_DEBUG
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FEGChessBoardGeometry Geometry = GetBoardGeometry();
	const FVector HalfSquare(Geometry.SquareSize * 0.48f, Geometry.SquareSize * 0.48f, 0.1f);
	for (int32 Square = 0; Square < 64; ++Square)
	{
		const bool bDark = (EGChess::FileOf(Square) + EGChess::RankOf(Square)) % 2 == 0;
		DrawDebugBox(World, Geometry.SquareToWorld(Square), HalfSquare, Geometry.SurfaceToWorld.GetRotation(), bDark ? FColor(80, 50, 20) : FColor(220, 200, 160), false, -1.0f, 0, 0.1f);
	}
	DrawDebugString(World, Geometry.SquareToWorld(0) + FVector(0.0f, 0.0f, 2.0f), TEXT("a1"), nullptr, FColor::White, 0.0f, true);
	DrawDebugCoordinateSystem(World, Geometry.SurfaceToWorld.GetLocation(), Geometry.SurfaceToWorld.Rotator(), Geometry.SquareSize * 2.0f, false, -1.0f, 0, 0.2f);

	for (const UEGChessSeatComponent* Seat : { SeatA.Get(), SeatB.Get() })
	{
		const FVector Anchor = Seat->GetComponentLocation();
		DrawDebugSphere(World, Anchor, 8.0f, 8, FColor::Cyan, false, -1.0f, 0, 0.5f);
		DrawDebugDirectionalArrow(World, Anchor, Anchor + Seat->GetForwardVector() * 40.0f, 10.0f, FColor::Cyan, false, -1.0f, 0, 0.5f);
		for (const EEGChessEntrySide Side : { EEGChessEntrySide::Left, EEGChessEntrySide::Right, EEGChessEntrySide::Back })
		{
			const FVector Entry = Anchor + Seat->GetSideDirection(Side) * Seat->FallbackEntryDistance;
			DrawDebugCapsule(World, Entry + FVector(0.0f, 0.0f, 88.0f), 88.0f, 34.0f, FQuat::Identity, FColor::Yellow, false, -1.0f, 0, 0.3f);
		}
		DrawDebugString(World, Anchor + FVector(0.0f, 0.0f, 30.0f), Seat->SeatId == EEGChessSeat::A ? TEXT("Seat A") : TEXT("Seat B"), nullptr, FColor::Cyan, 0.0f, true);
	}
	DrawDebugString(World, GetActorLocation() + FVector(0.0f, 0.0f, 150.0f), Position.ToFEN(), nullptr, FColor::Green, 0.0f, true);
#endif
}

// =============================================================================================
// Editor validation
// =============================================================================================

#if WITH_EDITOR
EDataValidationResult AEGChessTableActor::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Validation = Super::IsDataValid(Context);

	if (!Config)
	{
		Context.AddWarning(LOCTEXT("ValidateNoConfig", "Chess table has no config; it will play with engine basic shapes."));
		return Validation;
	}

	// Everything below is measured in actor space from relative transforms. Validation also runs on
	// the Blueprint's class default object, whose components are never registered (so their world
	// transforms and bounds are stale) and which never ran OnConstruction (so the style has not been
	// applied to BoardSurface or the meshes yet).
	const UEGChessTableStyle* Style = GetTableStyle();
	const FTransform SurfaceToActor = Style
		? Style->BoardSurfaceTransform * EGChessTablePrivate::ComponentToActor(BoardMesh, Root)
		: EGChessTablePrivate::ComponentToActor(BoardSurface, Root);
	FEGChessBoardGeometry Geometry = GetBoardGeometry();
	Geometry.SurfaceToWorld = FTransform(SurfaceToActor.GetRotation(), SurfaceToActor.GetLocation());
	const FVector BoardCenter = Geometry.GetCenterWorld();

	UStaticMesh* TableAsset = Style ? Style->TableMesh.LoadSynchronous() : nullptr;
	UStaticMesh* ChairAsset = Style ? Style->ChairMesh.LoadSynchronous() : nullptr;
	const FBox TableBox = TableAsset ? TableAsset->GetBoundingBox().TransformBy(EGChessTablePrivate::ComponentToActor(TableMesh, Root)) : FBox(ForceInit);

	for (const UEGChessSeatComponent* Seat : { SeatA.Get(), SeatB.Get() })
	{
		const FString SeatName = Seat->SeatId == EEGChessSeat::A ? TEXT("Seat A") : TEXT("Seat B");

		if (TableAsset && ChairAsset && Seat->Chair)
		{
			// Shrunk a little: a chair tucked under the table edge touches it by design.
			const FBox ChairBox = ChairAsset->GetBoundingBox().TransformBy(EGChessTablePrivate::ComponentToActor(Seat->Chair, Root)).ExpandBy(-2.0f);
			if (TableBox.Intersect(ChairBox))
			{
				Context.AddWarning(FText::Format(LOCTEXT("ValidateChairOverlap", "{0}: the chair overlaps the table."), FText::FromString(SeatName)));
			}
		}

		if (const UEGChessAnimationSet* Set = Config->AnimationSet)
		{
			const float Distance = FVector::Dist2D(EGChessTablePrivate::ComponentToActor(Seat, Root).GetLocation(), BoardCenter);
			if (FMath::Abs(Distance - Set->AuthoredBoardDistance) > Set->LayoutTolerance)
			{
				Context.AddWarning(FText::Format(LOCTEXT("ValidateLayout", "{0} is {1} cm from the board centre; its animations were made for {2} cm."),
					FText::FromString(SeatName), FText::AsNumber(FMath::RoundToInt(Distance)), FText::AsNumber(FMath::RoundToInt(Set->AuthoredBoardDistance))));
			}
		}

		if (Seat->BoardCamera)
		{
			const FTransform CameraToActor = EGChessTablePrivate::ComponentToActor(Seat->BoardCamera, Root);
			const FVector ToBoard = (BoardCenter - CameraToActor.GetLocation()).GetSafeNormal();
			if (FVector::DotProduct(ToBoard, CameraToActor.GetUnitAxis(EAxis::X)) < 0.7f)
			{
				Context.AddWarning(FText::Format(LOCTEXT("ValidateCamera", "{0}: the board camera is not looking at the board."), FText::FromString(SeatName)));
			}
		}
	}

	if (const UEGChessPieceSet* Set = Config->PieceSet)
	{
		if (Set->bScaleToBoard)
		{
			const float Scale = Geometry.SquareSize / FMath::Max(Set->AuthoredSquareSize, KINDA_SMALL_NUMBER);
			if (Scale < 0.5f || Scale > 2.0f)
			{
				Context.AddWarning(FText::Format(LOCTEXT("ValidateScale", "The piece set is scaled by {0} to fit this board; check AuthoredSquareSize."), FText::AsNumber(Scale)));
			}
		}
	}
	return Validation;
}
#endif

#undef LOCTEXT_NAMESPACE
