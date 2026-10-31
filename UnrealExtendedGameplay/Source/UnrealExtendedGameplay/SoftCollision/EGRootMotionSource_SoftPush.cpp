// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGRootMotionSource_SoftPush.h"

#include "EGSoftCollisionMath.h"
#include "GameFramework/CharacterMovementComponent.h"

FEGRootMotionSource_SoftPush::FEGRootMotionSource_SoftPush()
{
	AccumulateMode = ERootMotionAccumulateMode::Additive;
	// Low priority: gameplay root motion (dashes, knockbacks) is applied first.
	Priority = 2;
	Settings.SetFlag(ERootMotionSourceSettingsFlags::IgnoreZAccumulate);
}

FVector FEGRootMotionSource_SoftPush::GetPushVelocity() const
{
	const FVector2D Velocity = EGSoftCollision::Dequantize(Push);
	return FVector(Velocity.X, Velocity.Y, 0.0);
}

FRootMotionSource* FEGRootMotionSource_SoftPush::Clone() const
{
	return new FEGRootMotionSource_SoftPush(*this);
}

bool FEGRootMotionSource_SoftPush::Matches(const FRootMotionSource* Other) const
{
	if (!FRootMotionSource::Matches(Other) || !Other || Other->GetScriptStruct() != GetScriptStruct())
	{
		return false;
	}

	const FEGRootMotionSource_SoftPush* OtherPush = static_cast<const FEGRootMotionSource_SoftPush*>(Other);
	return Sequence == OtherPush->Sequence && Push == OtherPush->Push && Decay == OtherPush->Decay;
}

bool FEGRootMotionSource_SoftPush::MatchesAndHasSameState(const FRootMotionSource* Other) const
{
	return FRootMotionSource::MatchesAndHasSameState(Other) && Matches(Other);
}

bool FEGRootMotionSource_SoftPush::UpdateStateFrom(const FRootMotionSource* SourceToTakeStateFrom, bool bMarkForSimulatedCatchup)
{
	// Nothing changes over a push's life except time, which the base takes.
	return FRootMotionSource::UpdateStateFrom(SourceToTakeStateFrom, bMarkForSimulatedCatchup);
}

void FEGRootMotionSource_SoftPush::PrepareRootMotion(float SimulationTime, float MovementTickTime, const ACharacter& Character, const UCharacterMovementComponent& MoveComponent)
{
	RootMotionParams.Clear();

	const float CurrentTime = GetTime();
	FVector Velocity = GetPushVelocity();
	if (Duration > UE_SMALL_NUMBER)
	{
		const float AlphaStart = CurrentTime / Duration;
		const float AlphaEnd = (CurrentTime + SimulationTime) / Duration;
		Velocity *= EGSoftCollision::EvaluateDecayAverage(Decay, AlphaStart, AlphaEnd);
	}

	// Velocity-based source: the translation is a velocity. When more simulation time than movement
	// time has to be covered (catch-up), scale it the same way the engine's constant force does.
	FTransform NewTransform(Velocity);
	if (MovementTickTime > UE_SMALL_NUMBER)
	{
		NewTransform.ScaleTranslation(SimulationTime / MovementTickTime);
	}
	RootMotionParams.Set(NewTransform);

	SetTime(CurrentTime + SimulationTime);
}

bool FEGRootMotionSource_SoftPush::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	if (!FRootMotionSource::NetSerialize(Ar, Map, bOutSuccess))
	{
		return false;
	}

	uint8 DecayValue = static_cast<uint8>(Decay);
	Ar << Sequence;
	Ar << Push.Yaw;
	Ar << Push.Strength;
	Ar << DecayValue;
	if (Ar.IsLoading())
	{
		Decay = static_cast<EEGSoftPushDecay>(FMath::Min<uint8>(DecayValue, static_cast<uint8>(EEGSoftPushDecay::EaseOut)));
	}

	bOutSuccess = !Ar.IsError();
	return true;
}

UScriptStruct* FEGRootMotionSource_SoftPush::GetScriptStruct() const
{
	return FEGRootMotionSource_SoftPush::StaticStruct();
}

FString FEGRootMotionSource_SoftPush::ToSimpleString() const
{
	return FString::Printf(TEXT("[ID:%u]FEGRootMotionSource_SoftPush %s Seq %u"), LocalID, *InstanceName.GetPlainNameString(), Sequence);
}

void FEGRootMotionSource_SoftPush::AddReferencedObjects(FReferenceCollector& Collector)
{
	FRootMotionSource::AddReferencedObjects(Collector);
}
