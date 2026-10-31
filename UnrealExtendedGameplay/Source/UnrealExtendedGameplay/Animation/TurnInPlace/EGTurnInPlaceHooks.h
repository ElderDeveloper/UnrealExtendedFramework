// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EGTurnInPlaceComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

/**
 * What a project calls from its own character and movement component for predicted turn in place.
 * Optional: without them the component rotates the owner itself (Rotation Driver: Auto or
 * Component), which is fine for single player, AI and Blueprint-only projects. Player characters
 * in multiplayer need them, with TEGTurnInPlaceSavedMove, so the owning client predicts the turn.
 * No base class is needed. See Docs/TurnInPlace.md, "Project integration".
 *
 *   void AMyCharacter::FaceRotation(FRotator NewControlRotation, float DeltaTime)
 *   {
 *       if (!EGTurnInPlace::HandleFaceRotation(this, NewControlRotation, DeltaTime))
 *       {
 *           Super::FaceRotation(NewControlRotation, DeltaTime);
 *       }
 *   }
 *
 *   void UMyMovement::PhysicsRotation(float DeltaTime)
 *   {
 *       if (!EGTurnInPlace::HandlePhysicsRotation(this, DeltaTime))
 *       {
 *           Super::PhysicsRotation(DeltaTime);
 *       }
 *   }
 */
namespace EGTurnInPlace
{
	/** True when turn in place handled the rotation; then do not call Super::FaceRotation. */
	UNREALEXTENDEDGAMEPLAY_API bool HandleFaceRotation(ACharacter* Character, const FRotator& NewControlRotation, float DeltaTime);

	/** True when turn in place handled the rotation; then do not call Super::PhysicsRotation. */
	UNREALEXTENDEDGAMEPLAY_API bool HandlePhysicsRotation(UCharacterMovementComponent* Movement, float DeltaTime);

	/**
	 * Optional, from ComputeOrientToMovementRotation: while there is no input, keep turning towards
	 * the last input direction (also while still sliding to a stop). False: call Super.
	 */
	UNREALEXTENDEDGAMEPLAY_API bool ComputeOrientToMovementRotation(const UCharacterMovementComponent* Movement, const FRotator& CurrentRotation, FRotator& OutRotation);
}

/**
 * Adds the turn state to a saved move. Combining two moves rewinds the rotation to the older
 * move's start and re-runs the rotation once; the turn state must be rewound with it, or the
 * owning client turns only part of the way the server does.
 *
 *   class FMySavedMove : public TEGTurnInPlaceSavedMove<FSavedMove_Character> { ... };
 *
 * A project with its own saved move base passes that as TBaseMove instead.
 */
template <typename TBaseMove = FSavedMove_Character>
class TEGTurnInPlaceSavedMove : public TBaseMove
{
	using Super = TBaseMove;

public:
	FEGTurnInPlaceMoveState StartTurnState;

	virtual void Clear() override
	{
		Super::Clear();
		StartTurnState = FEGTurnInPlaceMoveState();
	}

	virtual void SetInitialPosition(ACharacter* Character) override
	{
		Super::SetInitialPosition(Character);
		if (UEGTurnInPlaceComponent* const TurnInPlace = UEGTurnInPlaceComponent::FindFor(Character))
		{
			StartTurnState = TurnInPlace->GetMoveState();
			TurnInPlace->NotifySavedMoveHook();
		}
	}

	virtual void CombineWith(const FSavedMove_Character* OldMove, ACharacter* Character, APlayerController* PC, const FVector& OldStartLocation) override
	{
		Super::CombineWith(OldMove, Character, PC, OldStartLocation);
		if (UEGTurnInPlaceComponent* const TurnInPlace = UEGTurnInPlaceComponent::FindFor(Character))
		{
			TurnInPlace->RestoreMoveState(static_cast<const TEGTurnInPlaceSavedMove*>(OldMove)->StartTurnState);
		}
	}
};

/**
 * Prediction data that allocates TMove. Return it from GetPredictionData_Client:
 *
 *   FNetworkPredictionData_Client* UMyMovement::GetPredictionData_Client() const
 *   {
 *       if (!ClientPredictionData)
 *       {
 *           const_cast<UMyMovement*>(this)->ClientPredictionData = new FEGTurnInPlacePredictionData(*this);
 *       }
 *       return ClientPredictionData;
 *   }
 */
template <typename TMove, typename TBaseData = FNetworkPredictionData_Client_Character>
class TEGTurnInPlacePredictionData : public TBaseData
{
public:
	explicit TEGTurnInPlacePredictionData(const UCharacterMovementComponent& Movement)
		: TBaseData(Movement)
	{
	}

	virtual FSavedMovePtr AllocateNewMove() override
	{
		return MakeShared<TMove>();
	}
};

/** For projects without a saved move of their own. */
using FEGTurnInPlaceSavedMove = TEGTurnInPlaceSavedMove<FSavedMove_Character>;
using FEGTurnInPlacePredictionData = TEGTurnInPlacePredictionData<FEGTurnInPlaceSavedMove>;
