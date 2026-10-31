// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Components/SkeletalMeshComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

/**
 * Shared checks for the plugin's anim notifies.
 *
 * A notify object belongs to the animation asset, not to the character playing it: one instance
 * serves every mesh that plays the clip, at the same time. Per-play state therefore never lives in
 * plain members; it goes in a map keyed by mesh, or in a registry keyed by what the notify changes.
 */
namespace EGAnimNotify
{
	/** The server, standalone, or an actor that does not replicate: where gameplay outcomes are decided. */
	inline bool HasAuthority(const USkeletalMeshComponent* MeshComp)
	{
		const AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
		return Owner && Owner->HasAuthority();
	}

	/**
	 * Whether this machine should move or rotate the owner. Simulated proxies with replicated
	 * movement are skipped: they already receive the result from the server, and moving them
	 * locally as well makes them jump and then snap back.
	 */
	inline bool CanDriveOwnerTransform(const USkeletalMeshComponent* MeshComp)
	{
		const AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
		if (!Owner)
		{
			return false;
		}
		return !(Owner->GetLocalRole() == ROLE_SimulatedProxy && Owner->IsReplicatingMovement());
	}
}
