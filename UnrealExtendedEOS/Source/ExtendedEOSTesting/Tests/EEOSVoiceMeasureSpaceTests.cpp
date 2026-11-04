// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Voice/EEOSVoiceChatComponent.h"

namespace EEOSVoiceMeasureSpaceTests
{
	/** A cube scaled into a slab: its centre and its size in centimetres. */
	void AddSlab(UWorld* World, UStaticMesh* Cube, const FVector& Centre, const FVector& Size)
	{
		AStaticMeshActor* Slab = World->SpawnActor<AStaticMeshActor>(Centre, FRotator::ZeroRotator);
		UStaticMeshComponent* Mesh = Slab->GetStaticMeshComponent();
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetStaticMesh(Cube);
		Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Slab->SetActorScale3D(Size / 100.0f);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSVoiceMeasureSpaceTest,
	"UnrealExtendedEOS.Voice.MeasureSpace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSVoiceMeasureSpaceTest::RunTest(const FString& Parameters)
{
	using namespace EEOSVoiceMeasureSpaceTests;
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("The engine cube loads"), Cube))
	{
		return false;
	}

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("EEOSVoiceMeasureSpaceTest"));
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());

	// A closed room, 4 m square and 3 m high inside, walls 20 cm thick.
	AddSlab(World, Cube, FVector(0, 0, -10), FVector(440, 440, 20));
	AddSlab(World, Cube, FVector(0, 0, 310), FVector(440, 440, 20));
	AddSlab(World, Cube, FVector(210, 0, 150), FVector(20, 440, 300));
	AddSlab(World, Cube, FVector(-210, 0, 150), FVector(20, 440, 300));
	AddSlab(World, Cube, FVector(0, 210, 150), FVector(440, 20, 300));
	AddSlab(World, Cube, FVector(0, -210, 150), FVector(440, 20, 300));
	World->UpdateWorldComponents(true, false);
	// Let the physics scene take the new bodies in before anything traces against them.
	World->Tick(LEVELTICK_All, 0.016f);
	World->Tick(LEVELTICK_All, 0.016f);

	float Enclosure = 0.0f;
	float RoomSize = 0.0f;
	UEEOSVoiceChatComponent::MeasureSpace(World, FVector(0, 0, 150), ECC_Visibility, 2500.0f, nullptr, Enclosure, RoomSize);
	AddInfo(FString::Printf(TEXT("Inside the room: enclosure %.2f, room %.1f m."), Enclosure, RoomSize));
	TestTrue(TEXT("A small closed room reads as closed in"), Enclosure > 0.8f);
	TestTrue(TEXT("A small closed room reads as small"), RoomSize > 1.5f && RoomSize < 3.0f);

	UEEOSVoiceChatComponent::MeasureSpace(World, FVector(5000, 0, 150), ECC_Visibility, 2500.0f, nullptr, Enclosure, RoomSize);
	AddInfo(FString::Printf(TEXT("In the open: enclosure %.2f, room %.1f m."), Enclosure, RoomSize));
	TestTrue(TEXT("Open air reads as open"), Enclosure < 0.01f);
	TestTrue(TEXT("Open air reads as the full range"), FMath::IsNearlyEqual(RoomSize, 25.0f));

	// Inside the east wall: rays that start in it are ignored, not read as a wall at zero distance.
	UEEOSVoiceChatComponent::MeasureSpace(World, FVector(210, 0, 150), ECC_Visibility, 2500.0f, nullptr, Enclosure, RoomSize);
	AddInfo(FString::Printf(TEXT("Inside a wall: enclosure %.2f, room %.1f m."), Enclosure, RoomSize));
	TestTrue(TEXT("A point inside a wall does not read as a wall at zero distance"), Enclosure < 0.8f);

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif
