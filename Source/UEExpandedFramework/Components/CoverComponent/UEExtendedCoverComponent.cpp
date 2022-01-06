// Fill out your copyright notice in the Description page of Project Settings.


#include "UEExtendedCoverComponent.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "UEExpandedFramework/Gameplay/Trace/UEExtendedTraceLibrary.h"

DEFINE_LOG_CATEGORY(LogCover);

// Sets default values for this component's properties
UUEExtendedCoverComponent::UUEExtendedCoverComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}





// Called when the game starts
void UUEExtendedCoverComponent::BeginPlay()
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
void UUEExtendedCoverComponent::TickComponent(float DeltaTime, ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bComponentActive) return;

	StorePlayerValues();

	if (GetIsInCover())
	{
		SideTracers();
		MoveCoverRight();
		MoveCoverLeft();
		MoveInCover();
		
		InCoverHeightCheck();
		
		if (! bInCoverCanMoveRight && ! bJumpingCoverToCover)
			CoverJumpRightTracer();

		if (! bInCoverCanMoveLeft && ! bJumpingCoverToCover)
			CoverJumpLeftTracer();
			
	}
	else
	{
		ForwardTracer();
	}
	
}





void UUEExtendedCoverComponent::TakeCover()
{
	if (bCanCover && !GetIsInCover())
	{
		const int32 multiply = UseInvertedCoverNormal ? -1 : 1;
		const FVector Rotation = FVector(CoverWallNormal.X,CoverWallNormal.Y,0.f) * multiply;
		
		Player->SetActorRotation(Rotation.ToOrientationRotator());
		Player->SetActorLocation(FindCoverLocation());
		Player->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		
		bInCover = true;
		OnCoverStateChanged.Broadcast(true);

		if (GetInCoverMontage)
			PlayerMesh->GetAnimInstance()->Montage_Play(GetInCoverMontage);
		
	}
}





void UUEExtendedCoverComponent::ProcessRightMovement(const float rightInput)
{
	RightInputValue = rightInput;
		
	if (RightInputValue != 0)
	{
		if (RightInputValue > 0)
		{
			if (CoverDirection != RightSide)
				PlayMontageBasedOnPose(CoverTurnRight,CrouchedCoverTurnRight);
			
			CoverDirection = RightSide;
			if (bInCoverCanMoveRight)
				IsMoving = true;
			else
				IsMoving = false;
		}
		else
		{
			if (CoverDirection != LeftSide)
				PlayMontageBasedOnPose(CoverTurnLeft,CrouchedCoverTurnLeft);
			
			CoverDirection = LeftSide;
			
			if (bInCoverCanMoveLeft)
				IsMoving = true;
			else
				IsMoving = false;
		}
	}
	else
	{
		IsMoving = false;
	}
}





void UUEExtendedCoverComponent::ProcessForwardMovement(const float forwardInput)
{
	ForwardInputValue = forwardInput;
	
	if (GetIsInCover())
	{
		if (ForwardInputValue >= -1 && ForwardInputValue <= -0.6)
			ExitCover();
	}
}





void UUEExtendedCoverComponent::CoverJumpRightTracer()
{
}





void UUEExtendedCoverComponent::CoverJumpLeftTracer()
{
}





void UUEExtendedCoverComponent::SideTracers()
{
	
	FVector Location , Forward;
	SimulateArrows(70 , Location , Forward);
	
	FCapsuleTraceStruct CoverTrace = {Location , Forward  , 20 , 60 };
	
	CoverTrace.TraceType = ETraceTypes::TraceType;
	CoverTrace.TraceChannel = SphereTraceSettings.TraceChannel;
	CoverTrace.DrawDebugType = SphereTraceSettings.DrawDebugType;

	bInCoverCanMoveRight = UUEExtendedTraceLibrary::ExtendedCapsuleTraceSingle(GetWorld(),CoverTrace);

	SimulateArrows(-70 , Location , Forward);
	CoverTrace.Start = Location;
	CoverTrace.End = Forward;
	
	bInCoverCanMoveLeft = UUEExtendedTraceLibrary::ExtendedCapsuleTraceSingle(GetWorld(),CoverTrace);
	
}





void UUEExtendedCoverComponent::InCoverHeightCheck()
{
	
	const float Side =  CoverDirection == RightSide ? 30 : -30;
	
	CoverHeightCheckSettings.Start = PlayerLocation + PlayerUpFromRot*50 + PlayerRight * Side;
	CoverHeightCheckSettings.End = CoverHeightCheckSettings.Start + PlayerForwardFromRot * -100;

	const bool TraceHit = UUEExtendedTraceLibrary::ExtendedLineTraceSingle(GetWorld(),CoverHeightCheckSettings);
	bInCoverCrouched = !TraceHit;
}





void UUEExtendedCoverComponent::ForwardTracer()
{
	SphereTraceSettings.Start = PlayerLocation;
	SphereTraceSettings.End = PlayerLocation + GetActorForwardMultiply(50,50);

	if(UUEExtendedTraceLibrary::ExtendedSphereTraceSingle(GetWorld(),SphereTraceSettings))
	{
		SphereTraceSettings.Start = PlayerLocation + PlayerRightFromRot * 30;
		SphereTraceSettings.End = SphereTraceSettings.Start + GetActorForwardMultiply(30,30);

		FHitResult FirstHitResult = SphereTraceSettings.HitResult;

		if (UUEExtendedTraceLibrary::ExtendedSphereTraceSingle(GetWorld(),SphereTraceSettings))
		{
			bRightTracerHit = SphereTraceSettings.GetHitBlockingHit();
		}

		SphereTraceSettings.Start = PlayerLocation + PlayerRightFromRot * -30;
		SphereTraceSettings.End = SphereTraceSettings.Start + GetActorForwardMultiply(30,30);
		
		if (UUEExtendedTraceLibrary::ExtendedSphereTraceSingle(GetWorld(),SphereTraceSettings))
		{
			bLeftTracerHit = SphereTraceSettings.GetHitBlockingHit();
		}

		bCanCover = bRightTracerHit && bLeftTracerHit;

		if (bCanCover)
		{
			CoverWallLocation = FVector(FirstHitResult.Location.X , FirstHitResult.Location.Y , PlayerLocation.Z);
			CoverWallNormal	  =  FirstHitResult.Normal;
		}
		
	}
	else
	{
		bRightTracerHit = false;
		bLeftTracerHit = false;
		bCanCover = false;
	}

}





void UUEExtendedCoverComponent::CoverHeightTrace(FVector& Start, FVector& End)
{

}





void UUEExtendedCoverComponent::CoverJumpRightTraceLocation(FVector& Start, FVector& End)
{
	Start = PlayerLocation + PlayerRightFromRot * (InCoverTraceDistance + CoverToCoverMaxDistance);
	End = Start + PlayerForwardFromRot * -60;
}





void UUEExtendedCoverComponent::CoverJumpLeftTraceLocation(FVector& Start, FVector& End)
{
	Start = PlayerLocation + PlayerRightFromRot * (InCoverTraceDistance + CoverToCoverMaxDistance) * -1;
	End = Start + PlayerForwardFromRot * -60;
}





void UUEExtendedCoverComponent::CoverToCoverJump()
{
}





void UUEExtendedCoverComponent::ExitCover()
{
	bInCoverCanMoveRight = false;
	bInCoverCanMoveLeft = false;
	bCameraEdgeMove = false;
	bCameraMoving = true;
	Player->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::Type::QueryAndPhysics);
	bInCover = false;
	PlayMontageIfValid(GetOutCoverMontage);
	OnCoverStateChanged.Broadcast(bInCover);
}





void UUEExtendedCoverComponent::InCoverExitToGrab()
{
}





FVector UUEExtendedCoverComponent::FindCoverLocation() const
{
	const FVector CoverWallNorm = CoverWallNormal* FVector(5,5,0);
	return FVector(CoverWallNorm.X+CoverWallLocation.X , CoverWallNorm.Y + CoverWallLocation.Y , PlayerLocation.Z );
}





FVector UUEExtendedCoverComponent::FindCoverJumpLocation()
{
	const FVector CoverWallNorm = NewCoverNormal* FVector(5,5,0);
	return FVector(CoverWallNorm.X+NewCoverLocation.X , CoverWallNorm.Y + NewCoverLocation.Y , PlayerLocation.Z );
}





void UUEExtendedCoverComponent::CameraEdgeZoom(const ECoverSide coverSide)
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

	//GetWorld()->GetTimerManager().SetTimer(CameraHandle , this , &UUEExtendedCoverComponent::CameraMove,GetWorld()->GetDeltaSeconds() , true);
}





void UUEExtendedCoverComponent::CameraMove()
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





void UUEExtendedCoverComponent::SimulateArrows(float RightVectorMultiply, FVector& Location , FVector& Forward) const
{
	Location = PlayerLocation + ( PlayerForward * 42 ) +  ( PlayerRight * RightVectorMultiply )  + ( PlayerUp * 25 );
	Forward = Location + PlayerForward * -100;
}





void UUEExtendedCoverComponent::MoveCoverRight()
{
	if (RightInputValue < 0)
	{
		FSphereTraceStruct SphereTrace;
		SphereTrace.TraceChannel = SphereTraceSettings.TraceChannel;
		SphereTrace.DrawDebugType = SphereTraceSettings.DrawDebugType;
		SphereTrace.bTraceComplex = true;
		SphereTrace.Radius = 20;

		SphereTrace.Start = PlayerLocation + PlayerRightFromRot * InCoverTraceDistance;
		SphereTrace.End = SphereTrace.Start + PlayerForwardFromRot * -60 + PlayerRightFromRot * 20;

		if (UUEExtendedTraceLibrary::ExtendedSphereTraceSingle(GetWorld() , SphereTrace))
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
			}
		}
	}
}





void UUEExtendedCoverComponent::MoveCoverLeft()
{
	bInCoverCanMoveLeft;
	if ( RightInputValue > 0)
	{
		FSphereTraceStruct SphereTrace;
		SphereTrace.TraceChannel = SphereTraceSettings.TraceChannel;
		SphereTrace.DrawDebugType = SphereTraceSettings.DrawDebugType;
		SphereTrace.bTraceComplex = true;
		SphereTrace.Radius = 20;

		SphereTrace.Start = PlayerLocation + PlayerRightFromRot * (InCoverTraceDistance * -1) ;
		SphereTrace.End = SphereTrace.Start + PlayerForwardFromRot * -60 + PlayerRightFromRot * -20;

		if (UUEExtendedTraceLibrary::ExtendedSphereTraceSingle(GetWorld() , SphereTrace))
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
			}
		}
	}
}





void UUEExtendedCoverComponent::MoveInCover()
{
	if (RightInputValue == 0)
	{
		PlayerCameraArm->bDoCollisionTest = true;
	}
	else
	{
		PlayerCameraArm->bDoCollisionTest = false;
	}
}





void UUEExtendedCoverComponent::StorePlayerValues()
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




void UUEExtendedCoverComponent::PlayMontageIfValid(UAnimMontage* MontageToPlay)
{
	if (MontageToPlay && PlayerMesh)
		PlayerMesh->GetAnimInstance()->Montage_Play(MontageToPlay);
}

void UUEExtendedCoverComponent::PlayMontageBasedOnDirection(UAnimMontage* RightSideMontage,UAnimMontage* LeftSideMontage)
{
}

void UUEExtendedCoverComponent::PlayMontageBasedOnDirectionAndPose(UAnimMontage* IdleRightSideMontage,UAnimMontage* IdleLeftSideMontage, UAnimMontage* CrouchRightSideMontage, UAnimMontage* CrouchLeftSideMontage)
{
	
}

void UUEExtendedCoverComponent::PlayMontageBasedOnPose(UAnimMontage* IdleMontage , UAnimMontage* CrouchMontage)
{
	if (bInCoverCrouched)
		PlayMontageIfValid(CrouchMontage);
	else
		PlayMontageIfValid(IdleMontage);
}
