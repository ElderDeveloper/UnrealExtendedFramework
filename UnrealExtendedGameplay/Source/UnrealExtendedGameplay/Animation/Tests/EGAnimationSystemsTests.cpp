// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Animation/AnimMontage.h"
#include "Misc/AutomationTest.h"
#include "UnrealExtendedGameplay/Animation/EGAnimationTags.h"
#include "UnrealExtendedGameplay/Animation/Montage/EGAnimationSet.h"
#include "UnrealExtendedGameplay/Combat/EGMeleeTypes.h"
#include "UnrealExtendedGameplay/Movement/EGMovementModifierComponent.h"
#include "UnrealExtendedGameplay/Time/EGTimeDilationStack.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace EGAnimationSystemsTest
{
	static FEGTimeDilationStack::FRequest MakeRequest(int32 Id, float Value, int32 Priority = 0, float BlendIn = 0.0f, float BlendOut = 0.0f, double ExpireTime = -1.0)
	{
		FEGTimeDilationStack::FRequest Request;
		Request.Id = Id;
		Request.Value = Value;
		Request.Priority = Priority;
		Request.BlendIn = BlendIn;
		Request.BlendOut = BlendOut;
		Request.ExpireTime = ExpireTime;
		return Request;
	}
}

// -----------------------------------------------------------------------------
// Time dilation: priority, ties and restoring the base
// -----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGTimeDilationPriorityTest,
	"UnrealExtendedGameplay.Animation.TimeDilation.Priority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGTimeDilationPriorityTest::RunTest(const FString& Parameters)
{
	using namespace EGAnimationSystemsTest;

	FEGTimeDilationStack Stack;
	Stack.Reset(1.0f);

	Stack.Push(MakeRequest(1, 0.5f, 0), 0.0);
	TestEqual(TEXT("A lone request applies"), Stack.Advance(0.0), 0.5f);

	Stack.Push(MakeRequest(2, 0.2f, 10), 0.0);
	TestEqual(TEXT("Higher priority wins"), Stack.Advance(0.0), 0.2f);

	Stack.Push(MakeRequest(3, 0.8f, 0), 0.0);
	TestEqual(TEXT("A newer low-priority request does not take over"), Stack.Advance(0.0), 0.2f);

	Stack.Remove(2, 0.0);
	TestEqual(TEXT("Removing the winner hands over to the newest of the rest"), Stack.Advance(0.0), 0.8f);

	Stack.Remove(3, 0.0);
	Stack.Remove(1, 0.0);
	TestEqual(TEXT("With no requests the base comes back"), Stack.Advance(0.0), 1.0f);
	TestFalse(TEXT("Nothing left to do"), Stack.IsActive());

	// The overlapping-finisher case the old notify got wrong: the first window ending must not
	// leave slow motion while the second is still open.
	Stack.Push(MakeRequest(10, 0.2f), 0.0);
	Stack.Push(MakeRequest(11, 0.2f), 0.0);
	Stack.Remove(10, 0.0);
	TestEqual(TEXT("A second window keeps slow motion"), Stack.Advance(0.0), 0.2f);
	Stack.Remove(11, 0.0);
	TestEqual(TEXT("The last window restores the base"), Stack.Advance(0.0), 1.0f);
	return true;
}

// -----------------------------------------------------------------------------
// Time dilation: blends and expiry
// -----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGTimeDilationBlendTest,
	"UnrealExtendedGameplay.Animation.TimeDilation.BlendAndExpiry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGTimeDilationBlendTest::RunTest(const FString& Parameters)
{
	using namespace EGAnimationSystemsTest;

	FEGTimeDilationStack Stack;
	Stack.Reset(1.0f);

	Stack.Push(MakeRequest(1, 0.0f, 0, /*BlendIn*/ 1.0f, /*BlendOut*/ 2.0f), 0.0);
	TestEqual(TEXT("Blend in starts at the base"), Stack.Advance(0.0), 1.0f);
	TestEqual(TEXT("Half way through the blend in"), Stack.Advance(0.5), 0.5f);
	TestEqual(TEXT("Blend in complete"), Stack.Advance(1.0), 0.0f);

	Stack.Remove(1, 1.0);
	TestEqual(TEXT("Blend out uses the removed request's time"), Stack.Advance(2.0), 0.5f);
	TestTrue(TEXT("Still active while blending back"), Stack.IsActive());
	TestEqual(TEXT("Blend out complete"), Stack.Advance(3.0), 1.0f);
	TestFalse(TEXT("Done once back at the base"), Stack.IsActive());

	Stack.Push(MakeRequest(2, 0.1f, 0, 0.0f, 0.0f, /*ExpireTime*/ 5.0), 4.0);
	TestEqual(TEXT("Before expiry"), Stack.Advance(4.9), 0.1f);
	TestEqual(TEXT("A timed request removes itself"), Stack.Advance(5.0), 1.0f);
	TestFalse(TEXT("The expired request is gone"), Stack.Contains(2));

	Stack.Push(MakeRequest(3, 0.5f), 6.0);
	Stack.SetBase(0.75f, 6.0);
	Stack.Remove(3, 6.0);
	TestEqual(TEXT("A changed base is what gets restored"), Stack.Advance(6.0), 0.75f);
	return true;
}

// -----------------------------------------------------------------------------
// Movement modifiers
// -----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGMovementModifierStackTest,
	"UnrealExtendedGameplay.Animation.MovementModifiers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGMovementModifierStackTest::RunTest(const FString& Parameters)
{
	FEGMovementModifierStack Stack;

	FEGMovementModifier Lock;
	Lock.AccelerationScale = 0.0f;
	FEGMovementModifier Slow;
	Slow.MaxWalkSpeedScale = 0.5f;
	Slow.AccelerationScale = 0.5f;

	const int32 LockId = Stack.Push(Lock);
	const int32 SlowId = Stack.Push(Slow);
	TestEqual(TEXT("Scales multiply"), Stack.GetCombined().AccelerationScale, 0.0f);
	TestEqual(TEXT("Untouched channels combine too"), Stack.GetCombined().MaxWalkSpeedScale, 0.5f);

	TestTrue(TEXT("Removing a modifier"), Stack.Remove(LockId));
	TestEqual(TEXT("The other modifier still holds"), Stack.GetCombined().AccelerationScale, 0.5f);
	TestFalse(TEXT("A removed id is not removed twice"), Stack.Remove(LockId));
	Stack.Remove(SlowId);
	TestTrue(TEXT("Empty"), Stack.IsEmpty());
	TestEqual(TEXT("An empty stack is identity"), Stack.GetCombined().RotationRateScale, 1.0f);

	TestEqual(TEXT("No outside change keeps the base"), FEGMovementModifierStack::Rebase(600.0f, 300.0f, 300.0f, 0.5f), 600.0f);
	TestEqual(TEXT("An outside change is unscaled into the new base"), FEGMovementModifierStack::Rebase(600.0f, 300.0f, 400.0f, 0.5f), 800.0f);
	TestEqual(TEXT("A zero scale hides the base, so it is kept"), FEGMovementModifierStack::Rebase(600.0f, 0.0f, 50.0f, 0.0f), 600.0f);
	return true;
}

// -----------------------------------------------------------------------------
// Melee: points along a blade
// -----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGMeleeSampleSegmentTest,
	"UnrealExtendedGameplay.Animation.Melee.SampleSegment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGMeleeSampleSegmentTest::RunTest(const FString& Parameters)
{
	TArray<FVector> Points;

	EGMelee::SampleSegment(FVector::ZeroVector, FVector(100.0, 0.0, 0.0), 3, Points);
	TestEqual(TEXT("Three points"), Points.Num(), 3);
	TestEqual(TEXT("Starts at A"), Points[0], FVector::ZeroVector);
	TestEqual(TEXT("Middle"), Points[1], FVector(50.0, 0.0, 0.0));
	TestEqual(TEXT("Ends at B"), Points[2], FVector(100.0, 0.0, 0.0));

	EGMelee::SampleSegment(FVector(1.0, 2.0, 3.0), FVector(100.0, 0.0, 0.0), 1, Points);
	TestEqual(TEXT("One point is A"), Points.Num(), 1);
	TestEqual(TEXT("One point is A (value)"), Points[0], FVector(1.0, 2.0, 3.0));
	return true;
}

// -----------------------------------------------------------------------------
// Animation sets: tag and parent fallback, variant picks
// -----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGAnimationSetLookupTest,
	"UnrealExtendedGameplay.Animation.AnimationSet.Lookup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGAnimationSetLookupTest::RunTest(const FString& Parameters)
{
	UAnimMontage* const GetIn = NewObject<UAnimMontage>(GetTransientPackage(), NAME_None, RF_Transient);
	UAnimMontage* const GetOutA = NewObject<UAnimMontage>(GetTransientPackage(), NAME_None, RF_Transient);
	UAnimMontage* const GetOutB = NewObject<UAnimMontage>(GetTransientPackage(), NAME_None, RF_Transient);

	UEGAnimationSet* const Base = NewObject<UEGAnimationSet>(GetTransientPackage(), NAME_None, RF_Transient);
	FEGAnimationSetEntry& GetInEntry = Base->Entries.Add(TAG_EGAnim_Cover_GetIn);
	GetInEntry.Variants.Add(TSoftObjectPtr<UAnimMontage>(GetIn));
	GetInEntry.PlayRate = 1.5f;

	UEGAnimationSet* const Child = NewObject<UEGAnimationSet>(GetTransientPackage(), NAME_None, RF_Transient);
	Child->Parent = Base;
	FEGAnimationSetEntry& GetOutEntry = Child->Entries.Add(TAG_EGAnim_Cover_GetOut);
	GetOutEntry.Variants.Add(TSoftObjectPtr<UAnimMontage>(GetOutA));
	GetOutEntry.Variants.Add(TSoftObjectPtr<UAnimMontage>(GetOutB));
	GetOutEntry.Pick = EEGAnimationVariantPick::Sequential;

	const FEGResolvedAnimation Crouched = Child->ResolveAnimation(TAG_EGAnim_Cover_GetIn_Crouched);
	TestTrue(TEXT("A child tag falls back to its parent tag, in the parent set"), Crouched.Montage == GetIn);
	TestEqual(TEXT("The entry's play rate comes with it"), Crouched.PlayRate, 1.5f);

	TestTrue(TEXT("Sequential: first"), Child->ResolveAnimation(TAG_EGAnim_Cover_GetOut).Montage == GetOutA);
	TestTrue(TEXT("Sequential: second"), Child->ResolveAnimation(TAG_EGAnim_Cover_GetOut).Montage == GetOutB);
	TestTrue(TEXT("Sequential: wraps"), Child->ResolveAnimation(TAG_EGAnim_Cover_GetOut).Montage == GetOutA);
	TestTrue(TEXT("An explicit variant index wins"), Child->ResolveAnimation(TAG_EGAnim_Cover_GetOut, 1).Montage == GetOutB);

	TestFalse(TEXT("Unknown tags resolve to nothing"), Child->ResolveAnimation(TAG_EGAnim_Cover_TurnLeft).IsValid());
	TestFalse(TEXT("The parent does not see the child's entries"), Base->HasAnimation(TAG_EGAnim_Cover_GetOut));

	// The most specific tag wins across the chain: a crouched clip in the parent beats a standing
	// clip in the child, which a parent-tag-first walk would have picked.
	UAnimMontage* const GetOutCrouched = NewObject<UAnimMontage>(GetTransientPackage(), NAME_None, RF_Transient);
	Base->Entries.Add(TAG_EGAnim_Cover_GetOut_Crouched).Variants.Add(TSoftObjectPtr<UAnimMontage>(GetOutCrouched));
	TestTrue(TEXT("A crouched entry in the parent beats the child's standing one"), Child->ResolveAnimation(TAG_EGAnim_Cover_GetOut_Crouched).Montage == GetOutCrouched);

	// Across separate sets (a character's set, then a system fallback) the same rule holds.
	UEGAnimationSet* const Fallback = NewObject<UEGAnimationSet>(GetTransientPackage(), NAME_None, RF_Transient);
	UAnimMontage* const TurnLeftCrouched = NewObject<UAnimMontage>(GetTransientPackage(), NAME_None, RF_Transient);
	Fallback->Entries.Add(TAG_EGAnim_Cover_TurnLeft_Crouched).Variants.Add(TSoftObjectPtr<UAnimMontage>(TurnLeftCrouched));
	UAnimMontage* const TurnLeft = NewObject<UAnimMontage>(GetTransientPackage(), NAME_None, RF_Transient);
	Child->Entries.Add(TAG_EGAnim_Cover_TurnLeft).Variants.Add(TSoftObjectPtr<UAnimMontage>(TurnLeft));

	const UEGAnimationSet* const Sets[] = { Child, Fallback };
	TSoftObjectPtr<UAnimMontage> Picked;
	FEGAnimationSetEntry PickedEntry;
	TestTrue(TEXT("Lookup across sets finds something"), UEGAnimationSet::PickVariantInSets(Sets, TAG_EGAnim_Cover_TurnLeft_Crouched, false, -1, Picked, PickedEntry));
	TestTrue(TEXT("The fallback's crouched clip beats the character's standing one"), Picked.Get() == TurnLeftCrouched);
	TestTrue(TEXT("Exact lookup skips parent tags"), UEGAnimationSet::FindEntryInSets(Sets, TAG_EGAnim_Cover_GetIn_Crouched, true) == nullptr);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
