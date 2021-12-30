// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UEExpandedFramework/Gameplay/Trace/UEExtendedTraceData.h"
#include "UEExtendedCoverComponent.generated.h"


enum ECoverSide
{
	RightSide,
	LeftSide
};


DECLARE_LOG_CATEGORY_EXTERN(LogCover, Error, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCoverStateChanged , bool , CoverState);

class UAnimMontage;
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UEEXPANDEDFRAMEWORK_API UUEExtendedCoverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UUEExtendedCoverComponent();


	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite , Category="Cover|Trace")
	FSphereTraceStruct SphereTraceSettings;

	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite , Category="Cover|Trace")
	FLineTraceStruct CoverHeightCheckSettings;

	//<<<<<<<<<<<<<<<<<<<<< SETTINGS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite , Category="Cover|Settings")
	float InCoverSpeedScalar = 1;

	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite , Category="Cover|Settings")
	float InCoverTraceDistance = 60;

	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite , Category="Cover|Settings")
	float MaxWalkInCoverAngle = 90;

	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite , Category="Cover|Settings")
	float CoverToCoverMaxDistance = 180;

	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite , Category="Cover|Settings")
	bool ShouldCrouchAutomatically = true;

	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite , Category="Cover|Settings")
	bool CameraZoomOnEdge = true;

	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite , Category="Cover|Settings")
	bool UseKeyToExitCover = false;

	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite , Category="Cover|Settings")
	bool UseInvertedCoverNormal = false;

	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite , Category="Cover|Settings")
	bool AutomaticallyCoverJump = false;


	
	//<<<<<<<<<<<<<<<<<<<<< MONTAGES >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite , Category="Cover|Montages")
	UAnimMontage* GetInCoverMontage = nullptr;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite , Category="Cover|Montages")
	UAnimMontage* GetOutCoverMontage = nullptr;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite , Category="Cover|Montages")
	UAnimMontage* CrouchedGetOutCoverMontage = nullptr;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite , Category="Cover|Montages")
	UAnimMontage* CrouchedGetInCoverMontage = nullptr;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite , Category="Cover|Montages")
	UAnimMontage* CoverToCoverLeftMontage = nullptr;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite , Category="Cover|Montages")
	UAnimMontage* CoverToCoverRightMontage = nullptr;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite , Category="Cover|Montages")
	UAnimMontage* CrouchedCoverToCoverRightMontage = nullptr;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite , Category="Cover|Montages")
	UAnimMontage* CrouchedCoverToCoverLeftMontage = nullptr;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite , Category="Cover|Montages")
	UAnimMontage* GetInCoverCrouchedMontage = nullptr;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite , Category="Cover|Montages")
	UAnimMontage* GetOutCoverCrouchedMontage = nullptr;



	UPROPERTY(BlueprintAssignable)
	FOnCoverStateChanged OnCoverStateChanged;

	UFUNCTION(BlueprintPure , Category="Cover")
	FORCEINLINE bool GetShouldCrouch() const { return bInCoverCrouched; }
	UFUNCTION(BlueprintPure , Category="Cover")
	FORCEINLINE bool GetIsInCover() const { return bInCover; }

	//<<<<<<<<<<<<<<<<<<<<<< Public Functions >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintCallable , Category="Cover")
	void TakeCover();
	
	UFUNCTION(BlueprintCallable , Category="Cover")
	void ProcessRightMovement(const float rightInput);
	
	UFUNCTION(BlueprintCallable , Category="Cover")
	void ProcessForwardMovement(const float forwardInput);
	
	UFUNCTION(BlueprintCallable , Category="Cover")
	void InputBlocked(const bool blocked) { bIsInputBlocked = blocked; }

	UFUNCTION(BlueprintCallable , Category="Cover")
	void SetComponentPaused(const bool IsPaused) { bComponentPaused = IsPaused; }

protected:
	
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;

	FORCEINLINE FVector GetActorForwardMultiply(const float X = 1 , const float Y = 1,const float Z = 1) const { return FVector(PlayerRotation.Vector().X*X , PlayerRotation.Vector().Y*Y , PlayerRotation.Vector().Z*Z); }
private:

	UPROPERTY()
	class USkeletalMeshComponent* PlayerMesh;
	UPROPERTY()
	class ACharacter* Player;
	UPROPERTY()
	class USpringArmComponent* PlayerCameraArm;

	bool bCameraMoving;
	bool bCameraEdgeMove;
	bool bCameraZoomRight;

	bool bRightTracerHit;
	bool bLeftTracerHit;


	bool bComponentPaused;
	bool bInCover;
	bool bInCoverMoveRight;
	bool bInCoverMoveLeft;
	bool bInCoverCrouched;
	bool bInCoverCanMoveRight;
	bool bInCoverCanMoveLeft;
	bool bCanCover;
	bool bInCoverIdleRight;
	bool bJumpingCoverToCover;
	bool bCanJumpToCoverLeft;
	bool bCanJumpToCoverRight;
	bool bRightInput;
	bool bCanStealthKill;
	bool bIsInputBlocked;
	

	float RightInputValue;
	float ForwardInputValue;

	FVector CoverWallNormal;
	FVector CoverWallLocation;
	FVector NewCoverLocation;
	FVector NewCoverNormal;

	FVector PlayerLocation;
	FVector PlayerForward;
	FVector PlayerRight;
	FVector PlayerUp;
	FRotator PlayerRotation;

	FVector PlayerForwardFromRot;
	FVector PlayerRightFromRot;
	FVector PlayerUpFromRot;
	
	//<<<<<<<<<<<<<<<<<<< TRACERS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	void CoverJumpRightTracer();
	void CoverJumpLeftTracer();
	void SideTracers();
	void InCoverHeightCheck();
	void ForwardTracer();

	void CoverHeightTrace(FVector& Start , FVector& End);
	void CoverJumpRightTraceLocation(FVector& Start , FVector& End);
	void CoverJumpLeftTraceLocation(FVector& Start , FVector& End);

	//<<<<<<<<<<<<<<<<<<<<< COVER >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	void CoverToCoverJump();
	void ExitCover();
	void InCoverExitToGrab();

	FVector FindCoverLocation() const;
	FVector FindCoverJumpLocation();

	//<<<<<<<<<<<<<<<<<<<<< CAMERA >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	void CameraEdgeZoom(const ECoverSide coverSide);
	void CameraMove();


	//<<<<<<<<<<<<<<<<<<<<< Arrow Functions >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	void SimulateArrows(const ECoverSide coverSide, FVector& Location , FVector& Forward , FVector& Right ,FVector& Up ) const;


	//<<<<<<<<<<<<<<<<<<<<<< Tick Cover Movement >>>>>>>>>>>>>>>>>>>>>>>>>
	void MoveCoverRight();
	void MoveCoverLeft();
	void MoveInCover();
	void StorePlayerValues();
	
};
