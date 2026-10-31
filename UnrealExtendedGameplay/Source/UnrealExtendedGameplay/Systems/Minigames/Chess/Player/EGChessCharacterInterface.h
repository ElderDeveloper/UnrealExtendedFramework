// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Player/EGChessHandTypes.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Table/EGChessMatchTypes.h"

#include "EGChessCharacterInterface.generated.h"

class UAnimMontage;
class UEGChessAnimationSet;
class USkeletalMeshComponent;

UINTERFACE(BlueprintType, MinimalAPI)
class UEGChessCharacterInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * IEGChessCharacterInterface
 *
 * Optional, on a pawn that sits at a chess table. Nothing is required: without it the table
 * uses its config's animation set and plays montages on ACharacter::GetMesh().
 */
class UNREALEXTENDEDGAMEPLAY_API IEGChessCharacterInterface
{
	GENERATED_BODY()

public:
	/** Bring a character-specific animation set. Null uses the table's. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Chess")
	UEGChessAnimationSet* GetChessAnimationSet() const;

	/** The mesh chess montages play on. Null uses ACharacter::GetMesh(). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Chess")
	USkeletalMeshComponent* GetChessMesh() const;

	/**
	 * Take over montage playback (GAS, a custom animation system). Return true when handled; the
	 * table then does nothing further for this montage, including piece sync notifies unless the
	 * game forwards them itself through AEGChessTableActor::HandleCharacterNotify.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Chess")
	bool PlayChessMontage(UAnimMontage* Montage, float StartTime, float PlayRate);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Chess")
	bool StopChessMontage(UAnimMontage* Montage);

	/** The pawn sat down or stood up. Hide weapons, switch first-person visibility, and so on. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Chess")
	void OnChessSeatChanged(EEGChessSeat Seat, bool bSeated);

	/** The local player switched board view. A first-person game shows the full body in the eye view. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Chess")
	void OnChessViewChanged(bool bEyeView);

	/**
	 * True when this character moves pieces with its own hand: its animation reads SetChessHandState.
	 * False leaves the pieces to move by themselves while the character only presses the clock.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Chess")
	bool CanMoveChessPiecesByHand() const;

	/**
	 * Every frame while the hand moves pieces, on every machine, then once with bActive false when it
	 * lets go of the board (or the move is cut short). Only called when CanMoveChessPiecesByHand is true.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Chess")
	void SetChessHandState(const FEGChessHandState& State);

	// Defaults, so a native class only overrides what it needs.
	virtual UEGChessAnimationSet* GetChessAnimationSet_Implementation() const { return nullptr; }
	virtual USkeletalMeshComponent* GetChessMesh_Implementation() const { return nullptr; }
	virtual bool PlayChessMontage_Implementation(UAnimMontage* Montage, float StartTime, float PlayRate) { return false; }
	virtual bool StopChessMontage_Implementation(UAnimMontage* Montage) { return false; }
	virtual void OnChessSeatChanged_Implementation(EEGChessSeat Seat, bool bSeated) {}
	virtual void OnChessViewChanged_Implementation(bool bEyeView) {}
	virtual bool CanMoveChessPiecesByHand_Implementation() const { return false; }
	virtual void SetChessHandState_Implementation(const FEGChessHandState& State) {}
};
