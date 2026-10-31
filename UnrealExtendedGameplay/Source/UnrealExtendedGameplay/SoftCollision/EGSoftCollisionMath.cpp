// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGSoftCollisionMath.h"

namespace EGSoftCollision
{
	namespace
	{
		constexpr double DegenerateDistance = 1.0e-3;

		/** Direction used when two centres coincide. Opposite for the two pawns of a pair. */
		FVector2D FallbackDirection(const FEGSoftBody& Self, const FEGSoftBody& Other)
		{
			return Self.Id <= Other.Id ? FVector2D(1.0, 0.0) : FVector2D(-1.0, 0.0);
		}

		FVector2D Flat(const FVector& Location)
		{
			return FVector2D(Location.X, Location.Y);
		}

		FVector2D RightOf(const FVector2D& Forward)
		{
			return FVector2D(-Forward.Y, Forward.X);
		}

		double BoxSupport(const FEGSoftBody& Box, const FVector2D& Normal)
		{
			const FVector2D U = Box.Axis;
			const FVector2D V = RightOf(U);
			return Box.BoxHalfExtent.X * FMath::Abs(FVector2D::DotProduct(Normal, U))
				+ Box.BoxHalfExtent.Y * FMath::Abs(FVector2D::DotProduct(Normal, V));
		}

		bool CylinderCylinder(const FEGSoftBody& Self, const FEGSoftBody& Other, FContact& Out)
		{
			const FVector2D Delta = Flat(Self.Location) - Flat(Other.Location);
			const double Sum = static_cast<double>(Self.Radius) + Other.Radius;
			const double DistanceSquared = Delta.SizeSquared();
			if (Sum <= 0.0 || DistanceSquared >= Sum * Sum)
			{
				return false;
			}

			const double Distance = FMath::Sqrt(DistanceSquared);
			Out.Direction = Distance > DegenerateDistance ? Delta / Distance : FallbackDirection(Self, Other);
			Out.Overlap = static_cast<float>(Sum - Distance);
			Out.Penetration = static_cast<float>(FMath::Clamp((Sum - Distance) / Sum, 0.0, 1.0));
			return true;
		}

		/**
		 * Circle against a rotated box. OutNormal points from the box towards the circle's centre.
		 * A centre inside the box leaves through the nearest face.
		 */
		bool CircleBox(const FVector2D& Centre, double Radius, const FEGSoftBody& Box,
			FVector2D& OutNormal, double& OutOverlap, double& OutSupportSum)
		{
			const FVector2D U = Box.Axis;
			const FVector2D V = RightOf(U);
			const FVector2D Delta = Centre - Flat(Box.Location);
			const double LocalX = FVector2D::DotProduct(Delta, U);
			const double LocalY = FVector2D::DotProduct(Delta, V);
			const double HalfX = Box.BoxHalfExtent.X;
			const double HalfY = Box.BoxHalfExtent.Y;

			FVector2D LocalNormal;
			const bool bInside = FMath::Abs(LocalX) <= HalfX && FMath::Abs(LocalY) <= HalfY;
			if (!bInside)
			{
				const FVector2D Diff(LocalX - FMath::Clamp(LocalX, -HalfX, HalfX), LocalY - FMath::Clamp(LocalY, -HalfY, HalfY));
				const double Distance = Diff.Size();
				if (Distance >= Radius)
				{
					return false;
				}
				LocalNormal = Distance > DegenerateDistance ? Diff / Distance : FVector2D(1.0, 0.0);
				OutOverlap = Radius - Distance;
			}
			else
			{
				const double ToFaceX = HalfX - FMath::Abs(LocalX);
				const double ToFaceY = HalfY - FMath::Abs(LocalY);
				if (ToFaceX < ToFaceY)
				{
					LocalNormal = FVector2D(LocalX >= 0.0 ? 1.0 : -1.0, 0.0);
					OutOverlap = Radius + ToFaceX;
				}
				else
				{
					LocalNormal = FVector2D(0.0, LocalY >= 0.0 ? 1.0 : -1.0);
					OutOverlap = Radius + ToFaceY;
				}
			}

			OutNormal = U * LocalNormal.X + V * LocalNormal.Y;
			OutSupportSum = Radius + BoxSupport(Box, OutNormal);
			return true;
		}

		bool BoxBox(const FEGSoftBody& Self, const FEGSoftBody& Other, FContact& Out)
		{
			const FVector2D Delta = Flat(Self.Location) - Flat(Other.Location);
			const FVector2D Axes[4] = { Self.Axis, RightOf(Self.Axis), Other.Axis, RightOf(Other.Axis) };

			double BestOverlap = TNumericLimits<double>::Max();
			double BestSupport = 0.0;
			FVector2D BestNormal = FVector2D::ZeroVector;
			for (const FVector2D& Axis : Axes)
			{
				const double SelfRadius = BoxSupport(Self, Axis);
				const double OtherRadius = BoxSupport(Other, Axis);
				const double Projection = FVector2D::DotProduct(Delta, Axis);
				const double Overlap = SelfRadius + OtherRadius - FMath::Abs(Projection);
				if (Overlap <= 0.0)
				{
					return false;
				}
				if (Overlap < BestOverlap)
				{
					BestOverlap = Overlap;
					BestSupport = SelfRadius + OtherRadius;
					// No offset along the axis (long boxes overlapping lengthwise, centres in line) would give
					// both pawns of the pair the same direction, and they would drift together: split by Id.
					const bool bAway = FMath::Abs(Projection) > DegenerateDistance ? Projection > 0.0 : Self.Id <= Other.Id;
					BestNormal = bAway ? Axis : -Axis;
				}
			}

			if (Delta.SizeSquared() <= DegenerateDistance * DegenerateDistance)
			{
				BestNormal = FallbackDirection(Self, Other);
			}

			Out.Direction = BestNormal;
			Out.Overlap = static_cast<float>(BestOverlap);
			Out.Penetration = BestSupport > 0.0 ? static_cast<float>(FMath::Clamp(BestOverlap / BestSupport, 0.0, 1.0)) : 0.0f;
			return true;
		}
	}

	bool ComputeContact(const FEGSoftBody& Self, const FEGSoftBody& Other, FContact& OutContact)
	{
		const double VerticalGap = FMath::Abs(Self.Location.Z - Other.Location.Z);
		if (VerticalGap >= static_cast<double>(Self.HalfHeight) + Other.HalfHeight)
		{
			return false;
		}

		const bool bSelfBox = Self.Shape == EEGSoftCollisionShapeType::Box;
		const bool bOtherBox = Other.Shape == EEGSoftCollisionShapeType::Box;

		if (!bSelfBox && !bOtherBox)
		{
			return CylinderCylinder(Self, Other, OutContact);
		}
		if (bSelfBox && bOtherBox)
		{
			return BoxBox(Self, Other, OutContact);
		}

		FVector2D Normal;
		double Overlap = 0.0;
		double SupportSum = 0.0;
		if (!bSelfBox)
		{
			// Self is the circle: the normal already points from the other pawn's box towards Self.
			if (!CircleBox(Flat(Self.Location), Self.Radius, Other, Normal, Overlap, SupportSum))
			{
				return false;
			}
			OutContact.Direction = Normal;
		}
		else
		{
			// Self is the box: the normal points from Self towards the other pawn's circle.
			if (!CircleBox(Flat(Other.Location), Other.Radius, Self, Normal, Overlap, SupportSum))
			{
				return false;
			}
			OutContact.Direction = -Normal;
		}

		OutContact.Overlap = static_cast<float>(Overlap);
		OutContact.Penetration = SupportSum > 0.0 ? static_cast<float>(FMath::Clamp(Overlap / SupportSum, 0.0, 1.0)) : 0.0f;
		return true;
	}

	float EvaluateResponse(EEGSoftPushResponse Response, float Penetration)
	{
		const float P = FMath::Clamp(Penetration, 0.0f, 1.0f);
		switch (Response)
		{
		case EEGSoftPushResponse::Linear:
			return P;
		case EEGSoftPushResponse::Quadratic:
			return P * P;
		case EEGSoftPushResponse::SmoothStep:
		default:
			return P * P * (3.0f - 2.0f * P);
		}
	}

	float EvaluatePenetrationFactor(const FEGSoftCollisionTuning& Tuning, float Penetration)
	{
		const float P = FMath::Clamp(Penetration, 0.0f, 1.0f);
		float Factor = EvaluateResponse(Tuning.Response, P);
		const float Deep = FMath::Clamp(Tuning.DeepPenetration, 0.0f, 0.99f);
		if (P > Deep)
		{
			const float Alpha = (P - Deep) / (1.0f - Deep);
			Factor *= FMath::Lerp(1.0f, FMath::Max(Tuning.DeepPenetrationMultiplier, 1.0f), Alpha);
		}
		return Factor;
	}

	float ComputeMomentumShare(float SelfWeight, float SelfApproachSpeed, float OtherWeight, float OtherPushPower,
		float OtherApproachSpeed, float IdleFloorSpeed)
	{
		const float Floor = FMath::Max(IdleFloorSpeed, 1.0f);
		const float SelfMomentum = FMath::Max(SelfWeight, 0.01f) * FMath::Max(SelfApproachSpeed, Floor);
		const float OtherMomentum = FMath::Max(OtherWeight, 0.01f) * FMath::Max(OtherPushPower, 0.0f) * FMath::Max(OtherApproachSpeed, Floor);
		const float Sum = SelfMomentum + OtherMomentum;
		return Sum > UE_SMALL_NUMBER ? OtherMomentum / Sum : 0.0f;
	}

	float EvaluateDecay(EEGSoftPushDecay Decay, float Alpha)
	{
		const float A = FMath::Clamp(Alpha, 0.0f, 1.0f);
		switch (Decay)
		{
		case EEGSoftPushDecay::Linear:
			return 1.0f - A;
		case EEGSoftPushDecay::EaseOut:
			return (1.0f - A) * (1.0f - A);
		case EEGSoftPushDecay::Constant:
		default:
			return 1.0f;
		}
	}

	float EvaluateDecayAverage(EEGSoftPushDecay Decay, float AlphaStart, float AlphaEnd)
	{
		const float A0 = FMath::Clamp(FMath::Min(AlphaStart, AlphaEnd), 0.0f, 1.0f);
		const float A1 = FMath::Clamp(FMath::Max(AlphaStart, AlphaEnd), 0.0f, 1.0f);
		const float Span = A1 - A0;
		if (Span <= UE_KINDA_SMALL_NUMBER)
		{
			return EvaluateDecay(Decay, 0.5f * (A0 + A1));
		}

		switch (Decay)
		{
		case EEGSoftPushDecay::Linear:
			return 1.0f - 0.5f * (A0 + A1);
		case EEGSoftPushDecay::EaseOut:
		{
			const float B0 = 1.0f - A0;
			const float B1 = 1.0f - A1;
			return (B0 * B0 * B0 - B1 * B1 * B1) / (3.0f * Span);
		}
		case EEGSoftPushDecay::Constant:
		default:
			return 1.0f;
		}
	}

	bool FPushAccumulator::AddNeighbour(const FEGSoftCollisionTuning& Tuning, const FEGSoftBody& Self, const FEGSoftBody& Other)
	{
		++NeighbourTests;
		if (!CanPush(Other, Self))
		{
			return false;
		}

		FContact Contact;
		if (!ComputeContact(Self, Other, Contact))
		{
			return false;
		}

		AddContact(Tuning, Self, Other, Contact);
		return true;
	}

	void FPushAccumulator::AddContact(const FEGSoftCollisionTuning& Tuning, const FEGSoftBody& Self, const FEGSoftBody& Other, const FContact& Contact)
	{
		const FVector2D Direction = Contact.Direction;
		const float OtherApproach = FMath::Max(static_cast<float>(FVector2D::DotProduct(Other.Velocity, Direction)), 0.0f);
		const float SelfApproach = FMath::Max(static_cast<float>(FVector2D::DotProduct(Self.Velocity, -Direction)), 0.0f);
		const float Share = ComputeMomentumShare(Self.Weight, SelfApproach, Other.Weight, Other.PushPower, OtherApproach, Tuning.IdleFloorSpeed);

		// Twice the share, so two idle equal pawns each take the full separation strength.
		const float Factor = EvaluatePenetrationFactor(Tuning, Contact.Penetration);
		SeparationSum += Direction * (Tuning.SeparationStrength * Factor * 2.0f * Share);

		++NeighbourCount;
		MaxPenetration = FMath::Max(MaxPenetration, Contact.Penetration);

		const float RelativeSpeed = FMath::Max(static_cast<float>(FVector2D::DotProduct(Other.Velocity - Self.Velocity, Direction)), 0.0f);
		if (Share >= Tuning.BumpMinShare && RelativeSpeed >= Tuning.BumpMinRelativeSpeed)
		{
			const float Score = RelativeSpeed * Share;
			if (Score > BumpScore)
			{
				BumpScore = Score;
				BumpDirection = Direction;
				BumpRelativeSpeed = RelativeSpeed;
				BumpShare = Share;
			}
		}
	}

	FEGSoftPushResult FPushAccumulator::Finish(const FEGSoftCollisionTuning& Tuning) const
	{
		FEGSoftPushResult Result;
		Result.NeighbourCount = NeighbourCount;
		Result.NeighbourTests = NeighbourTests;
		Result.MaxPenetration = MaxPenetration;

		const double MaxSpeed = FMath::Min(static_cast<double>(FMath::Max(Tuning.MaxTotalStrength, 0.0f)), static_cast<double>(MaxQuantizedStrength));
		const auto Cap = [MaxSpeed](FVector2D Vector)
		{
			const double Speed = Vector.Size();
			return Speed > MaxSpeed && Speed > 0.0 ? Vector * (MaxSpeed / Speed) : Vector;
		};
		const double MinSpeed = FMath::Max(Tuning.MinPushStrength, UE_KINDA_SMALL_NUMBER);

		const FVector2D Separation = Cap(SeparationSum);
		if (Separation.Size() >= MinSpeed)
		{
			Result.SeparationVelocity = Separation;
		}

		FVector2D Velocity = SeparationSum;
		const bool bBump = BumpScore > 0.0f;
		if (bBump)
		{
			const float Ratio = FMath::Clamp(BumpRelativeSpeed / FMath::Max(Tuning.BumpReferenceSpeed, 1.0f), 0.0f, 1.5f);
			Velocity += BumpDirection * (Tuning.BumpStrength * Ratio * FMath::Min(2.0f * BumpShare, 2.0f));
		}
		Velocity = Cap(Velocity);

		if (Velocity.Size() < MinSpeed)
		{
			return Result;
		}

		Result.Velocity = Velocity;
		Result.Band = bBump ? EEGSoftPushBand::Bump : EEGSoftPushBand::Separation;
		return Result;
	}

	FEGSoftPushQuantized Quantize(const FVector2D& Velocity)
	{
		FEGSoftPushQuantized Quantized;
		const double Speed = Velocity.Size();
		if (Speed <= 0.0)
		{
			return Quantized;
		}

		double Turns = FMath::Atan2(Velocity.Y, Velocity.X) / UE_DOUBLE_TWO_PI;
		if (Turns < 0.0)
		{
			Turns += 1.0;
		}
		Quantized.Yaw = static_cast<uint16>(FMath::RoundToInt64(Turns * 65536.0) & 0xFFFF);
		Quantized.Strength = static_cast<uint16>(FMath::Clamp<int64>(FMath::RoundToInt64(Speed / QuantizedStrengthStep), 0, 65535));
		return Quantized;
	}

	float DequantizeStrength(uint16 Strength)
	{
		return static_cast<float>(Strength) * QuantizedStrengthStep;
	}

	FVector2D Dequantize(const FEGSoftPushQuantized& Quantized)
	{
		const double Angle = static_cast<double>(Quantized.Yaw) * (UE_DOUBLE_TWO_PI / 65536.0);
		const double Speed = DequantizeStrength(Quantized.Strength);
		return FVector2D(FMath::Cos(Angle) * Speed, FMath::Sin(Angle) * Speed);
	}

	float AngleBetweenDegrees(const FVector2D& A, const FVector2D& B)
	{
		const double SizeProduct = A.Size() * B.Size();
		if (SizeProduct <= UE_SMALL_NUMBER)
		{
			return 180.0f;
		}
		const double Cosine = FMath::Clamp(FVector2D::DotProduct(A, B) / SizeProduct, -1.0, 1.0);
		return static_cast<float>(FMath::RadiansToDegrees(FMath::Acos(Cosine)));
	}

	bool FTokenBucket::TryConsume(double Now, float RatePerSecond, float Burst)
	{
		const float Capacity = FMath::Max(Burst, 1.0f);
		if (Tokens < 0.0f)
		{
			Tokens = Capacity;
			LastTime = Now;
		}

		const double Elapsed = FMath::Max(Now - LastTime, 0.0);
		Tokens = FMath::Min(Capacity, Tokens + static_cast<float>(Elapsed) * FMath::Max(RatePerSecond, 0.0f));
		LastTime = Now;

		if (Tokens >= 1.0f)
		{
			Tokens -= 1.0f;
			return true;
		}
		return false;
	}
}
