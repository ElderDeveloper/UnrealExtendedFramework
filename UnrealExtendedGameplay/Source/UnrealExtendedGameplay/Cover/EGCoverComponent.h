// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Components/ActorComponent.h"
#include "UnrealExtendedFramework/Data/EFTraceData.h"
#include "UnrealExtendedFramework/Data/EFEnums.h"
#include "GameplayTagContainer.h"
#include "UnrealExtendedGameplay/Animation/AnimInstance/EGAnimStateProvider.h"
#include "UnrealExtendedGameplay/Animation/Montage/EGAnimationSet.h"
#include "EGCoverComponent.generated.h"


UENUM(BlueprintType)
enum EGCoverSide
{
	RightSide,
	LeftSide
};

UENUM(BlueprintType)
enum EGCoverCameraSide
{
	CameraCenter,
	CameraRightSide,
	CameraLeftSide,
};


DECLARE_LOG_CATEGORY_EXTERN(LogCover, Error, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCoverStateChanged , bool , CoverState);

class UAnimInstance;
class UAnimMontage;
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALEXTENDEDGAMEPLAY_API UEGCoverComponent : public UActorComponent, public IEGAnimStateProvider
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UEGCoverComponent();


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
	bool ShouldCrouchAutomatically = true;

	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite , Category="Cover|Settings")
	bool CameraZoomOnEdge = true;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite , Category="Cover|Settings")
	float CameraEdgeTargetOffset = 300;

	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite , Category="Cover|Settings")
	bool UseInvertedCoverNormal = false;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite , Category="Cover|Settings")
	float SideTracerForward = 60;

	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite , Category="Cover|Settings")
	float SideTracerRight = 20;



	
	//<<<<<<<<<<<<<<<<<<<<< MONTAGES >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/**
	 * Cover montages by ExtendedGameplay.Anim.Cover.* tag, used when the character's own set
	 * (IEGMontagePlayerInterface::GetAnimationSet) has none. The slots below are the last fallback.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Cover|Montages")
	TObjectPtr<UEGAnimationSet> AnimationSet = nullptr;

	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite , Category="Cover|Montages|GetInOut")
	UAnimMontage* GetInCoverMontage = nullptr;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite , Category="Cover|Montages|GetInOut")
	UAnimMontage* GetOutCoverMontage = nullptr;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite , Category="Cover|Montages|GetInOut")
	UAnimMontage* CrouchedGetInCoverMontage = nullptr;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite , Category="Cover|Montages|GetInOut")
	UAnimMontage* CrouchedGetOutCoverMontage = nullptr;

	

	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite , Category="Cover|Montages|Turn")
	UAnimMontage* CoverTurnRight = nullptr;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite , Category="Cover|Montages|Turn")
	UAnimMontage* CoverTurnLeft = nullptr;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite , Category="Cover|Montages|Turn")
	UAnimMontage* CrouchedCoverTurnRight = nullptr;

	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite , Category="Cover|Montages|Turn")
	UAnimMontage* CrouchedCoverTurnLeft = nullptr;

	


	//<<<<<<<<<<<<<<<<<<<<<<< Event Dispatcher >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UPROPERTY(BlueprintAssignable)
	FOnCoverStateChanged OnCoverStateChanged;
	
	
	//<<<<<<<<<<<<<<<<<<<<<< Public Pure Inline Functions >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintPure , Category="Cover")
	FORCEINLINE bool GetIsInCover() const { return bInCover; }

	UFUNCTION(BlueprintPure , Category="Cover")
	FORCEINLINE bool GetCanCover() const { return bCanCover; }
	
	UFUNCTION(BlueprintPure , Category="Cover")
	FORCEINLINE bool GetIsInCoverCrouched() const { return bInCoverCrouched; }

	UFUNCTION(BlueprintPure , Category="Cover")
	FORCEINLINE bool GetCoverMoving() const { return IsMoving; }
	
	UFUNCTION(BlueprintPure , Category="Cover")
	FORCEINLINE TEnumAsByte<EGCoverSide> GetCoverMovementDirection() const { return CoverDirection; }

	UFUNCTION(BlueprintPure , Category="Cover")
	FORCEINLINE TEnumAsByte<EGCoverCameraSide> GetCoverCameraSide() const { return CoverCameraDirection; }
	
	UFUNCTION(BlueprintPure , Category="Cover")
	FORCEINLINE float GetCoverCameraOffset() const { switch (CoverCameraDirection) { case CameraCenter: return 0; case CameraRightSide: return  CameraEdgeTargetOffset; case CameraLeftSide: return CameraEdgeTargetOffset * -1; default: return 0;} }
	
	UFUNCTION(BlueprintPure , Category="Cover")
	FORCEINLINE bool GetIsInMontage() const { return IsInMontage; }

	/** ExtendedGameplay.Anim.State.Cover (+ .Crouched, .Moving, .Left / .Right, .Montage) for UEGAnimInstance. */
	virtual FGameplayTagContainer GetAnimStateTags_Implementation() const override;
	
	
	
	UFUNCTION(BlueprintCallable,meta = (DisplayName = "IsInCoverExec",CompactNodeTitle = "IsInCover", ExpandEnumAsExecs = "OutPins") , Category="Cover")
	FORCEINLINE bool GetIsInCoverExec(TEnumAsByte<EFConditionOutput>& OutPins) {	bInCover?  OutPins = UEF_True : OutPins = UEF_False; return bInCover; }


	
	
	//<<<<<<<<<<<<<<<<<<<<<< Public Functions >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintCallable , Category="Cover")
	void TakeCover();

	UFUNCTION(BlueprintCallable , Category="Cover")
	void ExitCover();
	
	UFUNCTION(BlueprintCallable , Category="Cover")
	void ProcessRightMovement(const float rightInput);
	
	UFUNCTION(BlueprintCallable , Category="Cover")
	void ProcessForwardMovement(const float forwardInput);
	
	UFUNCTION(BlueprintCallable , Category="Cover")
	void SetCoverComponentActive(const bool IsActive) { bComponentActive = IsActive; }

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

	TEnumAsByte<EGCoverSide> CoverDirection;
	TEnumAsByte<EGCoverCameraSide> CoverCameraDirection;

	bool bComponentActive = true;
	bool bInCover;

	bool bInCoverCrouched;
	bool bInCoverCanMoveRight;
	bool bInCoverCanMoveLeft;
	bool bCanCover;

	bool IsInMontage = false;

	bool IsMoving = false;

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
	void SideTracers();
	void InCoverHeightCheck();
	void ForwardTracer();



	//<<<<<<<<<<<<<<<<<<<<< COVER >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>


	FVector FindCoverLocation() const;


	//<<<<<<<<<<<<<<<<<<<<< CAMERA >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	void CameraEdgeZoom(const EGCoverSide coverSide);
	void CameraMove();


	//<<<<<<<<<<<<<<<<<<<<< Arrow Functions >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	void SimulateArrows(float RightVectorMultiply, FVector& Location , FVector& Forward) const;


	//<<<<<<<<<<<<<<<<<<<<<< Tick Cover Movement >>>>>>>>>>>>>>>>>>>>>>>>>
	void MoveCoverRight();
	void MoveCoverLeft();
	void StorePlayerValues();

	//<<<<<<<<<<<<<<<<<<<<<< Animation >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	FEGResolvedAnimation ResolveCoverAnimation(FGameplayTag Tag, UAnimMontage* Fallback, bool bExactTag) const;
	void PlayCoverMontage(FGameplayTag StandingTag, FGameplayTag CrouchedTag, UAnimMontage* StandingMontage, UAnimMontage* CrouchedMontage);
	void PlayResolvedAnimation(const FEGResolvedAnimation& Animation);

	void OnCoverMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted, uint32 PlayId);

	/**
	 * For montages the owner played itself (IEGMontagePlayerInterface): its single-slot montage
	 * delegates belong to the owner, so the shared event is used instead.
	 */
	UFUNCTION()
	void OnOwnerMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);

	void StopTrackingOwnerMontage();

	TWeakObjectPtr<UAnimMontage> OwnerPlayedMontage;
	TWeakObjectPtr<UAnimInstance> OwnerPlayedAnimInstance;

	/** Bumped for every cover montage, so a montage interrupted by the next one does not clear IsInMontage. */
	uint32 CoverMontagePlayId = 0;
};

#pragma once
