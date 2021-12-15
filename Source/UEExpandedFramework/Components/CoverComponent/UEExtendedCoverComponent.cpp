// Fill out your copyright notice in the Description page of Project Settings.


#include "UEExtendedCoverComponent.h"

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
	
}




// Called every frame
void UUEExtendedCoverComponent::TickComponent(float DeltaTime, ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	if (!Player  && !PlayerMesh && ! PlayerCameraArm)
	{
		UE_LOG(LogCover,Warning,TEXT("There was error in referances , Component Destroyed"));
		DestroyComponent();
	}

	StorePlayerValues();
	
}




void UUEExtendedCoverComponent::TakeCover()
{
}




void UUEExtendedCoverComponent::ProcessRightMovement(const float rightInput)
{
}




void UUEExtendedCoverComponent::ProcessForwardMovement(const float leftInput)
{
}




void UUEExtendedCoverComponent::InputBlocked(const bool blocked)
{
}




void UUEExtendedCoverComponent::CoverJumpRightTracer()
{
}




void UUEExtendedCoverComponent::CoverJumpLeftTracer()
{
}




void UUEExtendedCoverComponent::SideTracers()
{
}




void UUEExtendedCoverComponent::InCoverHeightCheck()
{
}




void UUEExtendedCoverComponent::ForwardTracer()
{
	FSphereTraceStruct SphereTraceStruct;
	SphereTraceStruct.TraceChannel = CapsuleTraceSettings.TraceChannel;
	SphereTraceStruct.DrawDebugType = CapsuleTraceSettings.DrawDebugType;
	SphereTraceStruct.Radius = 20;

	SphereTraceStruct.Start = PlayerLocation;
	SphereTraceStruct.End = PlayerLocation + GetActorForwardMultiply(50,50);

	if(UUEExtendedTraceLibrary::ExtendedSphereTraceSingle(GetWorld(),SphereTraceStruct))
	{
		SphereTraceStruct.Start = PlayerLocation + PlayerRightFromRot * 30;
		SphereTraceStruct.End = SphereTraceStruct.Start + GetActorForwardMultiply(30,30);

		FHitResult FirstHitResult = SphereTraceStruct.HitResult;

		if (UUEExtendedTraceLibrary::ExtendedSphereTraceSingle(GetWorld(),SphereTraceStruct))
		{
			RightTracerHit = SphereTraceStruct.GetHitBlockingHit();
		}

		SphereTraceStruct.Start = PlayerLocation + PlayerRightFromRot * -30;
		SphereTraceStruct.End = SphereTraceStruct.Start + GetActorForwardMultiply(30,30);
		
		if (UUEExtendedTraceLibrary::ExtendedSphereTraceSingle(GetWorld(),SphereTraceStruct))
		{
			LeftTracerHit = SphereTraceStruct.GetHitBlockingHit();
		}

		bCanCover = RightTracerHit && LeftTracerHit;

		if (bCanCover)
		{
			CoverWallLocation = FVector(FirstHitResult.Location.X , FirstHitResult.Location.Y , PlayerLocation.Z);
			CoverWallNormal	  =  FirstHitResult.Normal;
		}
		
	}
	else
	{
		RightTracerHit = false;
		LeftTracerHit = false;
		bCanCover = false;
	}

}





void UUEExtendedCoverComponent::CoverHeightTrace(FVector& Start, FVector& End)
{
	const float Side =  bInCoverMoveRight ? 30 : -30;
	Start = PlayerLocation + PlayerUpFromRot*50 + PlayerRight * Side;
	End = Start + PlayerForwardFromRot * -100;
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
}




void UUEExtendedCoverComponent::InCoverExitToGrab()
{
}




FVector UUEExtendedCoverComponent::FindCoverLocation()
{
	FVector coverWallNorm = CoverWallNormal* FVector(5,5,0);
	return FVector(coverWallNorm.X+CoverWallLocation.X , coverWallNorm.Y + CoverWallLocation.Y , PlayerLocation.Z );
}




FVector UUEExtendedCoverComponent::FindCoverJumpLocation()
{
	FVector coverWallNorm = NewCoverNormal* FVector(5,5,0);
	return FVector(coverWallNorm.X+NewCoverLocation.X , coverWallNorm.Y + NewCoverLocation.Y , PlayerLocation.Z );
}




void UUEExtendedCoverComponent::CameraEdgeZoom(const ECoverSide coverSide)
{
}




void UUEExtendedCoverComponent::CameraMove()
{
}




void UUEExtendedCoverComponent::SimulateArrows(const ECoverSide coverSide, FVector& Location, FVector& Forward,FVector& Right, FVector& Up)
{
	const float forw = 42;
	const float Side =  coverSide == ECoverSide::RightSide ? 1 : -1;
	const FRotator Rotator = UKismetMathLibrary::MakeRotationFromAxes(PlayerForward * forw , PlayerRight * forw, PlayerUp * forw);
	
	Location = PlayerLocation + PlayerForward * forw + PlayerRight * forw + PlayerUp * forw;
	Forward = Rotator.Vector();
	Right	= UKismetMathLibrary::GetRightVector(Rotator) * Side ;
	Up		= UKismetMathLibrary::GetUpVector(Rotator) * Side;
}




void UUEExtendedCoverComponent::MoveCoverRight()
{
	if (bInCoverCanMoveRight && RightInputValue < 0)
	{
		FSphereTraceStruct SphereTrace;
		SphereTrace.TraceChannel = CapsuleTraceSettings.TraceChannel;
		SphereTrace.DrawDebugType = CapsuleTraceSettings.DrawDebugType;
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
				
				bInCoverMoveRight	= false;
				bInCoverMoveLeft	= true;
				bInCoverIdleRight	= true;
			}
			else bInCoverMoveLeft = false;
		}
	}
	else bInCoverMoveLeft = false;
}




void UUEExtendedCoverComponent::MoveCoverLeft()
{
	if (bInCoverCanMoveLeft && RightInputValue > 0)
	{
		FSphereTraceStruct SphereTrace;
		SphereTrace.TraceChannel = CapsuleTraceSettings.TraceChannel;
		SphereTrace.DrawDebugType = CapsuleTraceSettings.DrawDebugType;
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
				
				bInCoverMoveRight	= true;
				bInCoverMoveLeft	= false;
				bInCoverIdleRight	= false;
			}
			else bInCoverMoveRight = false;
		}
	}
	else bInCoverMoveRight = false;
}




void UUEExtendedCoverComponent::MoveInCover()
{
}




void UUEExtendedCoverComponent::StorePlayerValues()
{
	if (bInCover && Player)
	{
		PlayerLocation = Player->GetActorLocation();
		PlayerForward = Player->GetActorForwardVector();
		PlayerRight = Player->GetActorRightVector();
		PlayerUp = Player->GetActorUpVector();
		PlayerRotation = Player->GetActorRotation();

		
		PlayerForwardFromRot = PlayerRotation.Vector();
		PlayerRightFromRot = FRotationMatrix(PlayerRotation).GetScaledAxis(EAxis::Y);
		PlayerUpFromRot = FRotationMatrix(PlayerRotation).GetScaledAxis(EAxis::Z);
	}
}
