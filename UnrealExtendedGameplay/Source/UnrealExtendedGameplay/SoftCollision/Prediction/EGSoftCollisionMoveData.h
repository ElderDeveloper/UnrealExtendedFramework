// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/CharacterMovementReplication.h"
#include "UnrealExtendedGameplay/SoftCollision/EGSoftCollisionComponent.h"
#include "UnrealExtendedGameplay/SoftCollision/EGSoftCollisionTypes.h"

/**
 * Soft Collision move-data transport. Opt-in, for projects that already have their own
 * CharacterMovementComponent (turn in place asks for one too).
 *
 * By default a predicted push reaches the server as an RPC sent just before the move it starts on.
 * That needs no movement subclass, but the server can only guess when a lost request would have
 * applied. With these templates the push rides inside the move itself: the server applies it
 * immediately before simulating that move, on exactly the same move as the owning client, and the
 * movement component's own redundancy (important moves are resent) carries it. Nothing else changes:
 * validation, rejects, the safety lease and every driver work as before.
 *
 * ---------------------------------------------------------------------------
 * Project integration (four pieces, all in the project's movement component)
 * ---------------------------------------------------------------------------
 *
 *   // Saved move and prediction data (stack turn in place on top if the project uses it):
 *   using FMySavedMove = TEGSoftCollisionSavedMove<FSavedMove_Character>;
 *   // or: using FMySavedMove = TEGTurnInPlaceSavedMove<TEGSoftCollisionSavedMove<FSavedMove_Character>>;
 *   using FMyPredictionData = TEGSoftCollisionPredictionData<FMySavedMove>;
 *   using FMyMoveData = TEGSoftCollisionNetworkMoveData<FMySavedMove>;
 *
 *   class UMyMovement : public UCharacterMovementComponent
 *   {
 *       TEGSoftCollisionMoveDataContainer<FMyMoveData> SoftCollisionMoveData;
 *
 *       UMyMovement(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
 *       {
 *           SetNetworkMoveDataContainer(SoftCollisionMoveData);                        // 1
 *       }
 *
 *       virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override // 2
 *       {
 *           if (!ClientPredictionData)
 *           {
 *               const_cast<UMyMovement*>(this)->ClientPredictionData = new FMyPredictionData(*this);
 *           }
 *           return ClientPredictionData;
 *       }
 *
 *       virtual void MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 CompressedFlags, const FVector& NewAccel) override
 *       {
 *           EGSoftCollision::HandleMoveAutonomous<FMyMoveData>(*this, GetCurrentNetworkMoveData(), ClientTimeStamp, DeltaTime); // 3
 *           Super::MoveAutonomous(ClientTimeStamp, DeltaTime, CompressedFlags, NewAccel);
 *       }
 *   };
 *
 * A turn-in-place project already has piece 2 with TEGTurnInPlacePredictionData; give it the
 * stacked move type instead. Piece 4 is nothing: the component notices the saved move on the first
 * move and switches its own transport (only while packed move RPCs are on, the engine default).
 *
 * Delivery: a move carrying a push is important, so the movement component resends it once as the
 * old move. If that is lost too, the request also goes out once by RPC after a round trip, and the
 * server, which de-duplicates by sequence, applies whichever copy arrives first.
 *
 * Cost: one bit per move, plus about eleven bytes in the move that starts a push.
 */

/** The push a saved move or a network move carries. Shared by the templates so they can find it. */
struct FEGSoftCollisionMoveFields
{
	bool bHasSoftPush = false;
	FEGSoftPushRequest SoftPush;
};

namespace EGSoftCollision
{
	/** One bit when the move carries no push; the request otherwise. */
	UNREALEXTENDEDGAMEPLAY_API void SerializeMovePush(FArchive& Ar, FEGSoftCollisionMoveFields& Fields);

	/** Server: applies the push the move about to be simulated carries. MoveStartTime is the move's start on the client's clock. */
	UNREALEXTENDEDGAMEPLAY_API void ReceiveMovePush(UCharacterMovementComponent& Movement, const FEGSoftPushRequest& Request, float MoveStartTime);

	/**
	 * Server, first thing in the project's MoveAutonomous, with MoveAutonomous's own ClientTimeStamp
	 * and DeltaTime. TMoveData is the network move data type the project's container holds. Returns
	 * true when the move carried a push.
	 */
	template <typename TMoveData>
	bool HandleMoveAutonomous(UCharacterMovementComponent& Movement, const FCharacterNetworkMoveData* MoveData, float ClientTimeStamp, float DeltaTime)
	{
		const TMoveData* const Data = static_cast<const TMoveData*>(MoveData);
		if (!Data || !Data->bHasSoftPush)
		{
			return false;
		}
		ReceiveMovePush(Movement, Data->SoftPush, ClientTimeStamp - DeltaTime);
		return true;
	}
}

/**
 * Adds the push that starts with a move to the saved move. Such a move is important (the movement
 * component resends it until acknowledged) and is never combined with another.
 */
template <typename TBaseMove = FSavedMove_Character>
class TEGSoftCollisionSavedMove : public TBaseMove, public FEGSoftCollisionMoveFields
{
	using Super = TBaseMove;

public:
	virtual void Clear() override
	{
		Super::Clear();
		bHasSoftPush = false;
		SoftPush = FEGSoftPushRequest();
	}

	virtual void SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override
	{
		Super::SetMoveFor(Character, InDeltaTime, NewAccel, ClientData);
		if (UEGSoftCollisionComponent* const SoftCollision = UEGSoftCollisionComponent::FindSoftCollisionComponent(Character))
		{
			SoftCollision->NotifyMoveDataTransport();
			bHasSoftPush = SoftCollision->ConsumePendingMovePush(SoftPush);
		}
	}

	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override
	{
		const TEGSoftCollisionSavedMove* const Other = static_cast<const TEGSoftCollisionSavedMove*>(NewMove.Get());
		if (bHasSoftPush || (Other && Other->bHasSoftPush))
		{
			return false;
		}
		return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
	}

	virtual bool IsImportantMove(const FSavedMovePtr& LastAckedMove) const override
	{
		return bHasSoftPush || Super::IsImportantMove(LastAckedMove);
	}
};

/** Prediction data that allocates TMove. */
template <typename TMove, typename TBaseData = FNetworkPredictionData_Client_Character>
class TEGSoftCollisionPredictionData : public TBaseData
{
public:
	explicit TEGSoftCollisionPredictionData(const UCharacterMovementComponent& Movement)
		: TBaseData(Movement)
	{
	}

	virtual FSavedMovePtr AllocateNewMove() override
	{
		return MakeShared<TMove>();
	}
};

/** Network move data that carries TMove's push. TMove derives from TEGSoftCollisionSavedMove. */
template <typename TMove, typename TBaseData = FCharacterNetworkMoveData>
struct TEGSoftCollisionNetworkMoveData : public TBaseData, public FEGSoftCollisionMoveFields
{
	using Super = TBaseData;

	virtual void ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, FCharacterNetworkMoveData::ENetworkMoveType MoveType) override
	{
		Super::ClientFillNetworkMoveData(ClientMove, MoveType);
		const FEGSoftCollisionMoveFields& Fields = static_cast<const TMove&>(ClientMove);
		bHasSoftPush = Fields.bHasSoftPush;
		SoftPush = Fields.SoftPush;
	}

	virtual bool Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap, FCharacterNetworkMoveData::ENetworkMoveType MoveType) override
	{
		const bool bSuperSucceeded = Super::Serialize(CharacterMovement, Ar, PackageMap, MoveType);
		EGSoftCollision::SerializeMovePush(Ar, *this);
		return bSuperSucceeded && !Ar.IsError();
	}
};

/** The container to give SetNetworkMoveDataContainer. Holds the new, pending and old move. */
template <typename TMoveData>
struct TEGSoftCollisionMoveDataContainer : public FCharacterNetworkMoveDataContainer
{
	TEGSoftCollisionMoveDataContainer()
	{
		NewMoveData = &Moves[0];
		PendingMoveData = &Moves[1];
		OldMoveData = &Moves[2];
	}

	TMoveData Moves[3];
};
