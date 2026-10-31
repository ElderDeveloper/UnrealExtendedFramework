// Copyright Kemal Erdem YILMAZ. All Rights Reserved.


#include "EGCoverComponent.h"



#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "UnrealExtendedGameplay/Animation/EGAnimationTags.h"
#include "UnrealExtendedGameplay/Animation/Montage/EGMontageLibrary.h"
#include "UnrealExtendedGameplay/Animation/TurnInPlace/EGTurnInPlaceComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "UnrealExtendedFramework/Libraries/Trace/EFTraceLibrary.h"


DEFINE_LOG_CATEGORY(LogCover);

// Sets default values for this component's properties
UEGCoverComponent::UEGCoverComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}





// Called when the game starts
void UEGCoverComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const auto ownChar = Cast<ACharacter>(GetOwner()))
	{
		Player = ownChar;
		PlayerMesh = Player->GetMesh();

		if(const auto Arm = Player->FindComponentByClass<USpringArmComponent>())
		{
			PlayerCameraArm = Arm;
		}
		else
		{
			UE_LOG(LogCover,Error,TEXT("Player Spring Arm Not Valid"));
			DestroyComponent();
		}
	}
	else
	{
		UE_LOG(LogCover,Error,TEXT("Component Mush Be Attached To A Character"));
		DestroyComponent();
	}

	if (!Player  && !PlayerMesh && ! PlayerCameraArm)
	{
		UE_LOG(LogCover,Warning,TEXT("There was error in referances , Component Destroyed"));
		DestroyComponent();
	}
	
}





// Called every frame
void UEGCoverComponent::TickComponent(float DeltaTime, ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bComponentActive) return;

	StorePlayerValues();

	if (GetIsInCover())
	{
		if (!IsInMontage)
		{
			SideTracers();
			MoveCoverRight();
			MoveCoverLeft();
		}
		/*
		if (RightInputValue == 0)
		{
			PlayerCameraArm->bDoCollisionTest = true;
		}
		else
		{
			PlayerCameraArm->bDoCollisionTest = false;
		}
		*/
		
		InCoverHeightCheck();
	}
	else
	{
		ForwardTracer();
	}
	
}





void UEGCoverComponent::TakeCover()
{
	if (bCanCover && !GetIsInCover())
	{
		const int32 multiply = UseInvertedCoverNormal ? -1 : 1;
		const FVector Rotation = FVector(CoverWallNormal.X,CoverWallNormal.Y,0.f) * multiply;
		
		Player->SetActorRotation(Rotation.ToOrientationRotator());
		Player->SetActorLocation(FindCoverLocation());
		Player->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		
		bInCover = true;
		if (UEGTurnInPlaceComponent* const TurnInPlace = UEGTurnInPlaceComponent::FindFor(GetOwner()))
		{
			ReleaseTurnInPlaceLock();
			TurnInPlaceLock = TurnInPlace->PushLock(TEXT("Cover"));
		}
		OnCoverStateChanged.Broadcast(true);

		PlayCoverMontage(TAG_EGAnim_Cover_GetIn, TAG_EGAnim_Cover_GetIn_Crouched, GetInCoverMontage, CrouchedGetInCoverMontage);
		
	}
}





void UEGCoverComponent::ProcessRightMovement(const float rightInput)
{
	RightInputValue = rightInput;
		
	if (RightInputValue != 0)
	{
		if (RightInputValue > 0)
		{
			if (CoverDirection != RightSide)
				PlayCoverMontage(TAG_EGAnim_Cover_TurnRight, TAG_EGAnim_Cover_TurnRight_Crouched, CoverTurnRight, CrouchedCoverTurnRight);
			CoverDirection = RightSide;
		}
		else
		{
			if (CoverDirection != LeftSide)
				PlayCoverMontage(TAG_EGAnim_Cover_TurnLeft, TAG_EGAnim_Cover_TurnLeft_Crouched, CoverTurnLeft, CrouchedCoverTurnLeft);
			CoverDirection = LeftSide;
		}
	}
	else
	{
		IsMoving = false;
	}
}





void UEGCoverComponent::ProcessForwardMovement(const float forwardInput)
{
	ForwardInputValue = forwardInput;
	
	if (GetIsInCover())
	{
		if (ForwardInputValue >= -1 && ForwardInputValue <= -0.6)
			ExitCover();
	}
}





void UEGCoverComponent::SideTracers()
{
	
	FVector Location , Forward;
	SimulateArrows(70 , Location , Forward);
	
	FCapsuleTraceStruct CoverTrace = {Location , Forward  , 20 , 60 };
	
	CoverTrace.TraceType = ETraceTypes::TraceType;
	CoverTrace.TraceChannel = SphereTraceSettings.TraceChannel;
	CoverTrace.DrawDebugType = SphereTraceSettings.DrawDebugType;

	bInCoverCanMoveRight = UEFTraceLibrary::ExtendedCapsuleTraceSingle(GetWorld(),CoverTrace);

	SimulateArrows(-70 , Location , Forward);
	CoverTrace.Start = Location;
	CoverTrace.End = Forward;
	
	bInCoverCanMoveLeft = UEFTraceLibrary::ExtendedCapsuleTraceSingle(GetWorld(),CoverTrace);
	
}





void UEGCoverComponent::InCoverHeightCheck()
{
	const float Side =  CoverDirection == RightSide ? 30 : -30;
	
	CoverHeightCheckSettings.Start = PlayerLocation + PlayerUpFromRot*50 + PlayerRight * Side;
	CoverHeightCheckSettings.End = CoverHeightCheckSettings.Start + PlayerForwardFromRot * -100;
	
	CoverHeightCheckSettings.TraceChannel = SphereTraceSettings.TraceChannel;
	CoverHeightCheckSettings.DrawDebugType = SphereTraceSettings.DrawDebugType;
	
	const bool TraceHit = UEFTraceLibrary::ExtendedLineTraceSingle(GetWorld(),CoverHeightCheckSettings);
	bInCoverCrouched = !TraceHit;
}





void UEGCoverComponent::ForwardTracer()
{
	SphereTraceSettings.Start = PlayerLocation;
	SphereTraceSettings.End = PlayerLocation + GetActorForwardMultiply(50,50);

	if(UEFTraceLibrary::ExtendedSphereTraceSingle(GetWorld(),SphereTraceSettings))
	{
		const auto StaticMesh = Cast<UStaticMeshComponent>(SphereTraceSettings.GetHitComponent());
		
		if ( !SphereTraceSettings.HitResult.GetComponent()->IsSimulatingPhysics() && IsValid(StaticMesh) )
		{
			FHitResult FirstHitResult = SphereTraceSettings.HitResult;
			
			SphereTraceSettings.Start = PlayerLocation + PlayerRightFromRot * 30;
			SphereTraceSettings.End = SphereTraceSettings.Start + GetActorForwardMultiply(30,30);
			
			if (UEFTraceLibrary::ExtendedSphereTraceSingle(GetWorld(),SphereTraceSettings))
				bRightTracerHit = SphereTraceSettings.GetHitBlockingHit();

			SphereTraceSettings.Start = PlayerLocation + PlayerRightFromRot * -30;
			SphereTraceSettings.End = SphereTraceSettings.Start + GetActorForwardMultiply(30,30);
			
			if (UEFTraceLibrary::ExtendedSphereTraceSingle(GetWorld(),SphereTraceSettings))
				bLeftTracerHit = SphereTraceSettings.GetHitBlockingHit();

			
			bCanCover = bRightTracerHit || bLeftTracerHit;
			
			if (bCanCover)
			{
				CoverWallLocation = FVector(FirstHitResult.Location.X , FirstHitResult.Location.Y , PlayerLocation.Z);
				CoverWallNormal	  =  FirstHitResult.Normal;
			}
		}
	}
	else
	{
		bRightTracerHit = false;
		bLeftTracerHit = false;
		bCanCover = false;
	}

}





void UEGCoverComponent::ExitCover()
{
	bInCoverCanMoveRight = false;
	bInCoverCanMoveLeft = false;
	bCameraEdgeMove = false;
	bCameraMoving = true;
	Player->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::Type::QueryAndPhysics);
	bInCover = false;
	ReleaseTurnInPlaceLock();
	CoverCameraDirection = CameraCenter;
	PlayCoverMontage(TAG_EGAnim_Cover_GetOut, TAG_EGAnim_Cover_GetOut_Crouched, GetOutCoverMontage, CrouchedGetOutCoverMontage);
	OnCoverStateChanged.Broadcast(bInCover);
}





FVector UEGCoverComponent::FindCoverLocation() const
{
	const FVector CoverWallNorm = CoverWallNormal* FVector(5,5,0);
	return FVector(CoverWallNorm.X+CoverWallLocation.X , CoverWallNorm.Y + CoverWallLocation.Y , PlayerLocation.Z );
}





void UEGCoverComponent::CameraEdgeZoom(const EGCoverSide coverSide)
{
	if(bCameraMoving) return;
	
	switch (coverSide)
	{
		case RightSide:
			if (bInCoverCanMoveLeft)
			{
				if (bInCoverCanMoveLeft && bInCoverCanMoveRight && bCameraEdgeMove)
				{
					bCameraEdgeMove = false;
					bCameraMoving = true;
				}
			}
			else if (!bCameraEdgeMove)
			{
				bCameraEdgeMove = true;
				bCameraMoving = true;
				bCameraZoomRight = true;
			}
			
			break;
		
		case LeftSide:
			if (bInCoverCanMoveRight)
			{
				if (bInCoverCanMoveLeft && bInCoverCanMoveRight && bCameraEdgeMove)
				{
					bCameraEdgeMove = false;
					bCameraMoving = true;
				}
			}
			else if (!bCameraEdgeMove)
			{
				bCameraEdgeMove = true;
				bCameraMoving = true;
				bCameraZoomRight = false;
			}
			break;
	}

	//GetWorld()->GetTimerManager().SetTimer(CameraHandle , this , &UEGCoverComponent::CameraMove,GetWorld()->GetDeltaSeconds() , true);
}





void UEGCoverComponent::CameraMove()
{
	if (!bInCover) bCameraEdgeMove = false;

	if(bCameraMoving)
	{
		const FVector ZoomSide = bCameraEdgeMove ?bCameraZoomRight ? FVector(200 , 100 , 50)  :  FVector(200 , -100 , 50) : FVector::ZeroVector;
		
		const FVector InterpMovement = UKismetMathLibrary::VInterpTo(PlayerCameraArm->SocketOffset , ZoomSide , GetWorld()->GetDeltaSeconds() , 0.3/GetWorld()->GetDeltaSeconds());

		PlayerCameraArm->SocketOffset = InterpMovement;
		
		bCameraMoving = UKismetMathLibrary::NotEqual_VectorVector(PlayerCameraArm->SocketOffset , InterpMovement );
	}
}





void UEGCoverComponent::SimulateArrows(float RightVectorMultiply, FVector& Location , FVector& Forward) const
{
	Location = PlayerLocation + ( PlayerForward * 42 ) +  ( PlayerRight * RightVectorMultiply )  + ( PlayerUp * 25 );
	Forward = Location + PlayerForward * -100;
}





void UEGCoverComponent::MoveCoverRight()
{
	if (RightInputValue < 0)
	{
		FSphereTraceStruct SphereTrace;
		SphereTrace.TraceChannel = SphereTraceSettings.TraceChannel;
		SphereTrace.DrawDebugType = SphereTraceSettings.DrawDebugType;
		SphereTrace.bTraceComplex = true;
		SphereTrace.Radius = 20;

		SphereTrace.Start = PlayerLocation + PlayerRightFromRot * InCoverTraceDistance;
		SphereTrace.End = SphereTrace.Start + PlayerForwardFromRot * (SideTracerForward* -1) + PlayerRightFromRot * SideTracerRight;

		if (UEFTraceLibrary::ExtendedSphereTraceSingle(GetWorld() , SphereTrace))
		{
			const float AngleCheck =	UKismetMathLibrary::FTrunc(UKismetMathLibrary::DegAcos(FVector::DotProduct(SphereTrace.GetHitImpactNormal(), CoverWallNormal)));
			if (AngleCheck < MaxWalkInCoverAngle + 1)
			{
				const FVector MoveLocation = UKismetMathLibrary::VInterpTo(PlayerLocation , SphereTrace.GetHitImpactPoint() + SphereTrace.GetHitImpactNormal() * 30 , GetWorld()->GetDeltaSeconds() , InCoverTraceDistance/20 * InCoverSpeedScalar);
				Player->SetActorLocation(MoveLocation);
				
				const float InvertCoverNormal = UseInvertedCoverNormal ? -1 : 1;
				const FRotator MoveRotation = UKismetMathLibrary::MakeRotFromX(SphereTrace.GetHitImpactNormal()*InvertCoverNormal);
				
				if(Player->SetActorRotation(UKismetMathLibrary::RInterpTo(PlayerRotation ,MoveRotation,GetWorld()->GetDeltaSeconds() , 5 )))
					CoverWallNormal  = SphereTrace.GetHitImpactNormal();

				CoverCameraDirection = CameraCenter;
				IsMoving = true;
			}
		}
		else
		{
			IsMoving = false;
			CoverCameraDirection = CameraLeftSide;
		}
	}
	const FVector Socket = PlayerCameraArm->SocketOffset;
	PlayerCameraArm->SocketOffset = UKismetMathLibrary::VInterpTo( Socket , FVector(Socket.X ,GetCoverCameraOffset() , Socket.Z) , GetWorld()->GetDeltaSeconds() , 4);
}





void UEGCoverComponent::MoveCoverLeft()
{
	if ( RightInputValue > 0)
	{
		FSphereTraceStruct SphereTrace;
		SphereTrace.TraceChannel = SphereTraceSettings.TraceChannel;
		SphereTrace.DrawDebugType = SphereTraceSettings.DrawDebugType;
		SphereTrace.bTraceComplex = true;
		SphereTrace.Radius = 20;

		SphereTrace.Start = PlayerLocation + PlayerRightFromRot * (InCoverTraceDistance * -1) ;
		SphereTrace.End = SphereTrace.Start + PlayerForwardFromRot * (SideTracerForward * -1 ) + PlayerRightFromRot * SideTracerRight * -1 ;

		if (UEFTraceLibrary::ExtendedSphereTraceSingle(GetWorld() , SphereTrace))
		{
			const float AngleCheck =	UKismetMathLibrary::FTrunc(UKismetMathLibrary::DegAcos(FVector::DotProduct(SphereTrace.GetHitImpactNormal(), CoverWallNormal)));
			if (AngleCheck < MaxWalkInCoverAngle + 1)
			{
				const FVector MoveLocation = UKismetMathLibrary::VInterpTo(PlayerLocation , SphereTrace.GetHitImpactPoint() + SphereTrace.GetHitImpactNormal() * 30 , GetWorld()->GetDeltaSeconds() , InCoverTraceDistance/20 * InCoverSpeedScalar);
				Player->SetActorLocation(MoveLocation);
				
				const float InvertCoverNormal = UseInvertedCoverNormal ? -1 : 1;
				const FRotator MoveRotation = UKismetMathLibrary::MakeRotFromX(SphereTrace.GetHitImpactNormal()*InvertCoverNormal);
				
				if(Player->SetActorRotation(UKismetMathLibrary::RInterpTo(PlayerRotation ,MoveRotation,GetWorld()->GetDeltaSeconds() , 5 )))
					CoverWallNormal  = SphereTrace.GetHitImpactNormal();

				CoverCameraDirection = CameraCenter;
				IsMoving = true;
			}
		}
		else
		{
			CoverCameraDirection = CameraRightSide;
			IsMoving = false;
		}
	}
}






void UEGCoverComponent::StorePlayerValues()
{
	if(Player)
	{
		PlayerLocation = Player->GetActorLocation();
		PlayerForward = Player->GetActorForwardVector();
		PlayerRotation = Player->GetActorRotation();
	}
	if (bInCover)
	{
		PlayerRight = Player->GetActorRightVector();
		PlayerUp = Player->GetActorUpVector();
		
		PlayerForwardFromRot = PlayerRotation.Vector();
		PlayerRightFromRot = FRotationMatrix(PlayerRotation).GetScaledAxis(EAxis::Y);
		PlayerUpFromRot = FRotationMatrix(PlayerRotation).GetScaledAxis(EAxis::Z);
	}
}




FEGResolvedAnimation UEGCoverComponent::ResolveCoverAnimation(FGameplayTag Tag, UAnimMontage* Fallback, bool bExactTag) const
{
	// The character's animation set, then this component's, then the montage slot.
	FEGResolvedAnimation Resolved = UEGMontageLibrary::ResolveAnimation(GetOwner(), Tag, -1, AnimationSet, bExactTag);
	if (!Resolved.IsValid())
	{
		Resolved.Montage = Fallback;
	}
	return Resolved;
}

void UEGCoverComponent::PlayCoverMontage(FGameplayTag StandingTag, FGameplayTag CrouchedTag, UAnimMontage* StandingMontage, UAnimMontage* CrouchedMontage)
{
	// Crouched: a crouched clip from any set, then the crouched slot, before any standing clip.
	if (bInCoverCrouched)
	{
		const FEGResolvedAnimation Crouched = ResolveCoverAnimation(CrouchedTag, CrouchedMontage, true);
		if (Crouched.IsValid())
		{
			PlayResolvedAnimation(Crouched);
			return;
		}
	}
	PlayResolvedAnimation(ResolveCoverAnimation(StandingTag, StandingMontage, false));
}

void UEGCoverComponent::PlayResolvedAnimation(const FEGResolvedAnimation& Animation)
{
	if (!Animation.IsValid())
	{
		return;
	}

	UAnimInstance* AnimInstance = nullptr;
	const EEGMontagePlayResult Result = UEGMontageLibrary::PlayMontage(GetOwner(), Animation.Montage, Animation.PlayRate, Animation.StartSection, Animation.StartTime, TAG_EGAnim_Mesh_Body, AnimInstance);

	// Played by the owner somewhere we cannot see: nothing to track.
	if (Result == EEGMontagePlayResult::Failed || !AnimInstance || !AnimInstance->Montage_IsPlaying(Animation.Montage))
	{
		return;
	}

	// Track this montage only. The anim instance's shared events fire for every montage on the
	// character, so they are filtered by montage, and bound at most once.
	++CoverMontagePlayId;
	IsInMontage = true;
	StopTrackingOwnerMontage();

	if (Result == EEGMontagePlayResult::Played)
	{
		FOnMontageBlendingOutStarted BlendingOutDelegate = FOnMontageBlendingOutStarted::CreateUObject(this, &UEGCoverComponent::OnCoverMontageBlendingOut, CoverMontagePlayId);
		AnimInstance->Montage_SetBlendingOutDelegate(BlendingOutDelegate, Animation.Montage);
	}
	else
	{
		OwnerPlayedMontage = Animation.Montage;
		OwnerPlayedAnimInstance = AnimInstance;
		AnimInstance->OnMontageBlendingOut.AddUniqueDynamic(this, &UEGCoverComponent::OnOwnerMontageBlendingOut);
	}
}

void UEGCoverComponent::OnOwnerMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	UAnimInstance* const AnimInstance = OwnerPlayedAnimInstance.Get();
	if (!Montage || Montage != OwnerPlayedMontage.Get() || (AnimInstance && AnimInstance->Montage_IsPlaying(Montage)))
	{
		// Another montage, or an older play of ours while the newest still runs.
		return;
	}
	IsInMontage = false;
	StopTrackingOwnerMontage();
}

void UEGCoverComponent::StopTrackingOwnerMontage()
{
	if (UAnimInstance* const AnimInstance = OwnerPlayedAnimInstance.Get())
	{
		AnimInstance->OnMontageBlendingOut.RemoveDynamic(this, &UEGCoverComponent::OnOwnerMontageBlendingOut);
	}
	OwnerPlayedMontage.Reset();
	OwnerPlayedAnimInstance.Reset();
}

void UEGCoverComponent::OnCoverMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted, uint32 PlayId)
{
	// A newer cover montage interrupted this one; that montage owns the flag now.
	if (PlayId == CoverMontagePlayId)
	{
		IsInMontage = false;
	}
}

FGameplayTagContainer UEGCoverComponent::GetAnimStateTags_Implementation() const
{
	FGameplayTagContainer Tags;
	if (!bInCover)
	{
		return Tags;
	}

	Tags.AddTag(TAG_EGAnim_State_Cover);
	if (bInCoverCrouched)
	{
		Tags.AddTag(TAG_EGAnim_State_Cover_Crouched);
	}
	if (IsMoving)
	{
		Tags.AddTag(TAG_EGAnim_State_Cover_Moving);
	}
	Tags.AddTag(CoverDirection == RightSide ? TAG_EGAnim_State_Cover_Right : TAG_EGAnim_State_Cover_Left);
	if (IsInMontage)
	{
		Tags.AddTag(TAG_EGAnim_State_Cover_Montage);
	}
	return Tags;
}

void UEGCoverComponent::ReleaseTurnInPlaceLock()
{
	if (UEGTurnInPlaceComponent* const TurnInPlace = UEGTurnInPlaceComponent::FindFor(GetOwner()))
	{
		TurnInPlace->RemoveRequest(TurnInPlaceLock);
	}
	TurnInPlaceLock.Invalidate();
}

void UEGCoverComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseTurnInPlaceLock();
	StopTrackingOwnerMontage();
	Super::EndPlay(EndPlayReason);
}
