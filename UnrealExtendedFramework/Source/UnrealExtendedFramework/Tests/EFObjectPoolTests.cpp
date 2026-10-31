// Copyright Moon Punch Games. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "Systems/Pool/EFObjectPoolSubsystem.h"
#include "Testing/EFTestHelpers.h"
#include "Tests/EFPoolTestTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFObjectPoolTest, "UnrealExtendedFramework.Pool.AcquireAndRelease",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFObjectPoolTest::RunTest(const FString& Parameters)
{
	EFTest::FScopedGameInstance Game;
	UWorld* World = Game.GetWorld();
	UEFObjectPoolSubsystem* Pool = World ? World->GetSubsystem<UEFObjectPoolSubsystem>() : nullptr;
	if (!TestNotNull(TEXT("game worlds have an object pool"), Pool))
	{
		return false;
	}

	const FTransform Here(FVector(100, 0, 0));
	AEFPoolTestActor* First = Pool->Acquire<AEFPoolTestActor>(AEFPoolTestActor::StaticClass(), Here);
	if (!TestNotNull(TEXT("the first acquire spawns"), First))
	{
		return false;
	}
	TestEqual(TEXT("an acquire is announced"), First->Acquires, 1);
	TestTrue(TEXT("the pool knows its actor"), Pool->IsPooledActor(First) && Pool->IsActorAcquired(First));
	TestTrue(TEXT("it is placed"), First->GetActorLocation().Equals(FVector(100, 0, 0)));

	Pool->ReleaseActor(First);
	TestEqual(TEXT("a release is announced"), First->Releases, 1);
	TestTrue(TEXT("a released actor is hidden"), First->IsHidden());
	TestFalse(TEXT("without collision"), First->GetActorEnableCollision());
	TestFalse(TEXT("and without tick"), First->IsActorTickEnabled());
	TestFalse(TEXT("it is no longer acquired"), Pool->IsActorAcquired(First));
	Pool->ReleaseActor(First);
	TestEqual(TEXT("releasing twice does nothing more"), First->Releases, 1);

	AEFPoolTestActor* Second = Pool->Acquire<AEFPoolTestActor>(AEFPoolTestActor::StaticClass(), FTransform(FVector(0, 50, 0)));
	TestTrue(TEXT("the next acquire reuses the released actor"), Second == First);
	TestFalse(TEXT("shown again"), Second->IsHidden());
	TestTrue(TEXT("with collision"), Second->GetActorEnableCollision());
	TestTrue(TEXT("and tick restored"), Second->IsActorTickEnabled());
	TestTrue(TEXT("at the new place"), Second->GetActorLocation().Equals(FVector(0, 50, 0)));
	TestEqual(TEXT("each acquire is announced"), Second->Acquires, 2);

	// A pool limited to one inactive actor destroys the extra one on release.
	Pool->SetMaxPooled(AEFPoolTestActor::StaticClass(), 1);
	AEFPoolTestActor* Third = Pool->Acquire<AEFPoolTestActor>(AEFPoolTestActor::StaticClass(), Here);
	TestTrue(TEXT("an acquire while every actor is in use spawns another"), Third && Third != Second);
	Pool->ReleaseActor(Second);
	Pool->ReleaseActor(Third);
	TestTrue(TEXT("the actor beyond the limit is destroyed"), !IsValid(Third) || Third->IsActorBeingDestroyed());

	TArray<FEFPoolStats> Stats = Pool->GetPoolStats();
	if (TestEqual(TEXT("one class is pooled"), Stats.Num(), 1))
	{
		TestEqual(TEXT("two actors were spawned"), Stats[0].Spawned, 2);
		TestEqual(TEXT("one acquire reused an actor"), Stats[0].Reused, 1);
		TestEqual(TEXT("one actor overflowed"), Stats[0].Overflowed, 1);
		TestEqual(TEXT("none is active"), Stats[0].Active, 0);
		TestEqual(TEXT("one is available"), Stats[0].Available, 1);
	}

	// Prewarm fills the pool up to its limit.
	Pool->SetMaxPooled(AEFPoolTestActor::StaticClass(), 3);
	Pool->Prewarm(AEFPoolTestActor::StaticClass(), 3);
	Stats = Pool->GetPoolStats();
	TestTrue(TEXT("prewarm fills the pool"), Stats.Num() == 1 && Stats[0].Available == 3);

	// An actor the pool did not spawn is destroyed on release.
	AEFPoolTestActor* Outsider = World->SpawnActor<AEFPoolTestActor>();
	Pool->ReleaseActor(Outsider);
	TestTrue(TEXT("releasing an actor the pool did not spawn destroys it"), !IsValid(Outsider) || Outsider->IsActorBeingDestroyed());

	// An acquired actor that is destroyed leaves the pool.
	AEFPoolTestActor* Doomed = Pool->Acquire<AEFPoolTestActor>(AEFPoolTestActor::StaticClass(), Here);
	Doomed->Destroy();
	TestFalse(TEXT("a destroyed actor leaves the pool"), Pool->IsPooledActor(Doomed));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
