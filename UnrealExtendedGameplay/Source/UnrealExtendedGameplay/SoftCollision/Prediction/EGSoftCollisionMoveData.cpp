// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGSoftCollisionMoveData.h"

#include "GameFramework/Character.h"

namespace EGSoftCollision
{
	void SerializeMovePush(FArchive& Ar, FEGSoftCollisionMoveFields& Fields)
	{
		uint8 bHasPush = Fields.bHasSoftPush ? 1 : 0;
		Ar.SerializeBits(&bHasPush, 1);
		Fields.bHasSoftPush = bHasPush != 0;

		if (!Fields.bHasSoftPush)
		{
			if (Ar.IsLoading())
			{
				Fields.SoftPush = FEGSoftPushRequest();
			}
			return;
		}

		FEGSoftPushRequest& Request = Fields.SoftPush;
		uint8 Band = static_cast<uint8>(Request.Band);
		Ar << Request.Sequence;
		Ar << Request.ClientTimeStamp;
		Ar << Request.Push.Yaw;
		Ar << Request.Push.Strength;
		Ar << Band;
		if (Ar.IsLoading())
		{
			Request.Band = static_cast<EEGSoftPushBand>(FMath::Min<uint8>(Band, static_cast<uint8>(EEGSoftPushBand::Bump)));
		}
	}

	void ReceiveMovePush(UCharacterMovementComponent& Movement, const FEGSoftPushRequest& Request, float MoveStartTime)
	{
		if (UEGSoftCollisionComponent* const SoftCollision = UEGSoftCollisionComponent::FindSoftCollisionComponent(Movement.GetCharacterOwner()))
		{
			SoftCollision->ReceiveMovePush(Request, MoveStartTime);
		}
	}
}
