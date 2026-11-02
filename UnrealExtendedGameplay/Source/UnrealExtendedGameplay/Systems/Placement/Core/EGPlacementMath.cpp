// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGPlacementMath.h"

namespace EGPlacementMath
{
	EEGPlacementOrientation ClassifyNormal(const FVector& Normal, const FEGPlacementSurfaceBands& Bands)
	{
		const FVector Unit = Normal.GetSafeNormal();
		if (Unit.IsNearlyZero())
		{
			return EEGPlacementOrientation::None;
		}
		if (Unit.Z >= Bands.FloorMinNormalZ)
		{
			return EEGPlacementOrientation::Floor;
		}
		if (Unit.Z <= Bands.CeilingMaxNormalZ)
		{
			return EEGPlacementOrientation::Ceiling;
		}
		return EEGPlacementOrientation::Wall;
	}

	float SlopeDegrees(const FVector& Normal, EEGPlacementOrientation Orientation)
	{
		const FVector Unit = Normal.GetSafeNormal();
		switch (Orientation)
		{
		case EEGPlacementOrientation::Ceiling:
			return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(-Unit.Z, -1.0, 1.0)));
		case EEGPlacementOrientation::Wall:
			return FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(FMath::Abs(Unit.Z), 0.0, 1.0)));
		default:
			return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Unit.Z, -1.0, 1.0)));
		}
	}

	FVector AlignedUp(const FVector& Normal, EEGPlacementOrientation Orientation, EEGPlacementAlignment Alignment)
	{
		const FVector Unit = Normal.GetSafeNormal();
		if (Alignment == EEGPlacementAlignment::Surface)
		{
			return Unit;
		}
		switch (Orientation)
		{
		case EEGPlacementOrientation::Floor:
			return FVector::UpVector;
		case EEGPlacementOrientation::Ceiling:
			return -FVector::UpVector;
		case EEGPlacementOrientation::Wall:
		{
			const FVector Level = FVector(Unit.X, Unit.Y, 0.0).GetSafeNormal();
			return Level.IsNearlyZero() ? Unit : Level;
		}
		default:
			return Unit;
		}
	}

	FQuat MakeRotation(const FQuat& Frame, const FVector& Up, float SpinDegrees)
	{
		const FVector LocalUp = Frame.UnrotateVector(Up).GetSafeNormal();
		const FQuat Swing = FQuat::FindBetweenNormals(FVector::UpVector, LocalUp.IsNearlyZero() ? FVector::UpVector : LocalUp);
		const FQuat Twist(FVector::UpVector, FMath::DegreesToRadians(static_cast<double>(SpinDegrees)));
		return (Frame * Swing * Twist).GetNormalized();
	}

	float ExtractSpin(const FQuat& Rotation, const FQuat& Frame, const FVector& Up)
	{
		const FVector UnitUp = Up.GetSafeNormal();
		const FQuat Aligned = FQuat::FindBetweenNormals(Rotation.GetUpVector(), UnitUp) * Rotation;
		const FVector LocalUp = Frame.UnrotateVector(UnitUp).GetSafeNormal();
		const FQuat Swing = FQuat::FindBetweenNormals(FVector::UpVector, LocalUp.IsNearlyZero() ? FVector::UpVector : LocalUp);
		const FQuat Twist = (Swing.Inverse() * Frame.Inverse() * Aligned).GetNormalized();
		return FRotator::NormalizeAxis(static_cast<float>(FMath::RadiansToDegrees(Twist.GetTwistAngle(FVector::UpVector))));
	}

	float SnapAngle(float Degrees, float Step)
	{
		if (Step <= UE_KINDA_SMALL_NUMBER)
		{
			return FRotator::NormalizeAxis(Degrees);
		}
		return FRotator::NormalizeAxis(FMath::RoundToFloat(Degrees / Step) * Step);
	}

	int32 DominantAxis(const FQuat& Frame, const FVector& Direction)
	{
		int32 Best = 0;
		double BestDot = -1.0;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			const FVector AxisVector = Axis == 0 ? Frame.GetAxisX() : (Axis == 1 ? Frame.GetAxisY() : Frame.GetAxisZ());
			const double Dot = FMath::Abs(FVector::DotProduct(AxisVector, Direction));
			if (Dot > BestDot + 1.e-6)
			{
				BestDot = Dot;
				Best = Axis;
			}
		}
		return Best;
	}

	FVector SnapToGrid(const FVector& World, const FTransform& Frame, const FVector& Step, const FVector& Origin, int32 SkipAxis)
	{
		FVector Local = Frame.InverseTransformPositionNoScale(World) - Origin;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			if (Axis != SkipAxis && Step[Axis] > UE_KINDA_SMALL_NUMBER)
			{
				Local[Axis] = FMath::RoundToDouble(Local[Axis] / Step[Axis]) * Step[Axis];
			}
		}
		return Frame.TransformPositionNoScale(Local + Origin);
	}

	FEGPlacementOrientedBox TransformBox(const FEGPlacementBox& Box, const FTransform& Pose)
	{
		const double Scale = FMath::Abs(Pose.GetScale3D().X);
		return FEGPlacementOrientedBox(
			Pose.TransformPosition(Box.Center),
			(Pose.GetRotation() * Box.Rotation.Quaternion()).GetNormalized(),
			Box.Extent * Scale);
	}

	bool BoxesIntersect(const FEGPlacementOrientedBox& A, const FEGPlacementOrientedBox& B, double Margin)
	{
		const FVector AAxes[3] = { A.GetAxis(0), A.GetAxis(1), A.GetAxis(2) };
		const FVector BAxes[3] = { B.GetAxis(0), B.GetAxis(1), B.GetAxis(2) };
		const double a[3] = { A.Extent.X, A.Extent.Y, A.Extent.Z };
		const double b[3] = { B.Extent.X, B.Extent.Y, B.Extent.Z };

		// Epsilon on |R| keeps the cross-product axes of parallel edges from reporting a false separation.
		constexpr double Epsilon = 1.e-9;
		double R[3][3];
		double AbsR[3][3];
		for (int32 i = 0; i < 3; ++i)
		{
			for (int32 j = 0; j < 3; ++j)
			{
				R[i][j] = FVector::DotProduct(AAxes[i], BAxes[j]);
				AbsR[i][j] = FMath::Abs(R[i][j]) + Epsilon;
			}
		}
		const FVector Delta = B.Center - A.Center;
		const double t[3] = { FVector::DotProduct(Delta, AAxes[0]), FVector::DotProduct(Delta, AAxes[1]), FVector::DotProduct(Delta, AAxes[2]) };

		for (int32 i = 0; i < 3; ++i)
		{
			const double ra = a[i];
			const double rb = b[0] * AbsR[i][0] + b[1] * AbsR[i][1] + b[2] * AbsR[i][2];
			if (FMath::Abs(t[i]) > ra + rb + Margin)
			{
				return false;
			}
		}
		for (int32 j = 0; j < 3; ++j)
		{
			const double ra = a[0] * AbsR[0][j] + a[1] * AbsR[1][j] + a[2] * AbsR[2][j];
			const double rb = b[j];
			if (FMath::Abs(t[0] * R[0][j] + t[1] * R[1][j] + t[2] * R[2][j]) > ra + rb + Margin)
			{
				return false;
			}
		}
		for (int32 j = 0; j < 3; ++j)
		{
			const int32 j1 = (j + 1) % 3;
			const int32 j2 = (j + 2) % 3;
			// A0 x Bj
			{
				const double ra = a[1] * AbsR[2][j] + a[2] * AbsR[1][j];
				const double rb = b[j1] * AbsR[0][j2] + b[j2] * AbsR[0][j1];
				if (FMath::Abs(t[2] * R[1][j] - t[1] * R[2][j]) > ra + rb + Margin)
				{
					return false;
				}
			}
			// A1 x Bj
			{
				const double ra = a[0] * AbsR[2][j] + a[2] * AbsR[0][j];
				const double rb = b[j1] * AbsR[1][j2] + b[j2] * AbsR[1][j1];
				if (FMath::Abs(t[0] * R[2][j] - t[2] * R[0][j]) > ra + rb + Margin)
				{
					return false;
				}
			}
			// A2 x Bj
			{
				const double ra = a[0] * AbsR[1][j] + a[1] * AbsR[0][j];
				const double rb = b[j1] * AbsR[2][j2] + b[j2] * AbsR[2][j1];
				if (FMath::Abs(t[1] * R[0][j] - t[0] * R[1][j]) > ra + rb + Margin)
				{
					return false;
				}
			}
		}
		return true;
	}

	bool BoxContainsPoint(const FEGPlacementOrientedBox& Box, const FVector& Point, double Tolerance)
	{
		const FVector Local = Box.Rotation.UnrotateVector(Point - Box.Center);
		return FMath::Abs(Local.X) <= Box.Extent.X + Tolerance
			&& FMath::Abs(Local.Y) <= Box.Extent.Y + Tolerance
			&& FMath::Abs(Local.Z) <= Box.Extent.Z + Tolerance;
	}

	bool BoxContainsBox(const FEGPlacementOrientedBox& Outer, const FEGPlacementOrientedBox& Inner, double Tolerance)
	{
		FVector Corners[8];
		Inner.GetCorners(Corners);
		for (const FVector& Corner : Corners)
		{
			if (!BoxContainsPoint(Outer, Corner, Tolerance))
			{
				return false;
			}
		}
		return true;
	}

	FEGPlacementOrientedBox Inflate(const FEGPlacementOrientedBox& Box, double Amount)
	{
		FEGPlacementOrientedBox Result = Box;
		Result.Extent = (Box.Extent + FVector(Amount)).ComponentMax(FVector::ZeroVector);
		return Result;
	}

	void MakePlaneBasis(const FVector& Normal, FVector& OutU, FVector& OutV)
	{
		Normal.GetSafeNormal().FindBestAxisVectors(OutU, OutV);
	}

	namespace
	{
		double Cross2D(const FVector2D& O, const FVector2D& A, const FVector2D& B)
		{
			return (A.X - O.X) * (B.Y - O.Y) - (A.Y - O.Y) * (B.X - O.X);
		}
	}

	void ConvexHull2D(TArray<FVector2D> Points, FPolygon2D& OutHull)
	{
		OutHull.Reset();
		Points.Sort([](const FVector2D& L, const FVector2D& R) { return L.X < R.X || (L.X == R.X && L.Y < R.Y); });
		if (Points.Num() < 3)
		{
			OutHull = MoveTemp(Points);
			return;
		}
		constexpr double Epsilon = 1.e-9;
		TArray<FVector2D> Hull;
		Hull.SetNum(Points.Num() * 2);
		int32 Count = 0;
		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			while (Count >= 2 && Cross2D(Hull[Count - 2], Hull[Count - 1], Points[Index]) <= Epsilon)
			{
				--Count;
			}
			Hull[Count++] = Points[Index];
		}
		for (int32 Index = Points.Num() - 2, Lower = Count + 1; Index >= 0; --Index)
		{
			while (Count >= Lower && Cross2D(Hull[Count - 2], Hull[Count - 1], Points[Index]) <= Epsilon)
			{
				--Count;
			}
			Hull[Count++] = Points[Index];
		}
		Hull.SetNum(FMath::Max(Count - 1, 0));
		OutHull = MoveTemp(Hull);
	}

	void ProjectBoxToPlane(const FEGPlacementOrientedBox& Box, const FVector& Origin, const FVector& U, const FVector& V, FPolygon2D& OutHull)
	{
		FVector Corners[8];
		Box.GetCorners(Corners);
		TArray<FVector2D> Points;
		Points.Reserve(8);
		for (const FVector& Corner : Corners)
		{
			const FVector Offset = Corner - Origin;
			Points.Emplace(FVector::DotProduct(Offset, U), FVector::DotProduct(Offset, V));
		}
		ConvexHull2D(MoveTemp(Points), OutHull);
	}

	double PolygonArea(const FPolygon2D& Polygon)
	{
		double Twice = 0.0;
		for (int32 Index = 0; Index < Polygon.Num(); ++Index)
		{
			const FVector2D& A = Polygon[Index];
			const FVector2D& B = Polygon[(Index + 1) % Polygon.Num()];
			Twice += A.X * B.Y - B.X * A.Y;
		}
		return FMath::Abs(Twice) * 0.5;
	}

	void ClipToLeft(const FPolygon2D& Polygon, const FVector2D& A, const FVector2D& B, FPolygon2D& Out)
	{
		Out.Reset();
		const int32 Num = Polygon.Num();
		if (Num == 0)
		{
			return;
		}
		constexpr double Epsilon = 1.e-9;
		auto Side = [&A, &B](const FVector2D& P) { return Cross2D(A, B, P); };
		for (int32 Index = 0; Index < Num; ++Index)
		{
			const FVector2D& Current = Polygon[Index];
			const FVector2D& Next = Polygon[(Index + 1) % Num];
			const double SideCurrent = Side(Current);
			const double SideNext = Side(Next);
			const bool bCurrentIn = SideCurrent >= -Epsilon;
			const bool bNextIn = SideNext >= -Epsilon;
			if (bCurrentIn)
			{
				Out.Add(Current);
			}
			if (bCurrentIn != bNextIn)
			{
				const double T = SideCurrent / (SideCurrent - SideNext);
				Out.Add(Current + (Next - Current) * T);
			}
		}
	}

	void SubtractConvex(const FPolygon2D& Subject, const FPolygon2D& Clip, TArray<FPolygon2D>& OutPieces)
	{
		constexpr double MinPieceArea = 1.e-6;
		if (Clip.Num() < 3)
		{
			OutPieces.Add(Subject);
			return;
		}
		FPolygon2D Remaining = Subject;
		FPolygon2D Outside;
		FPolygon2D Inside;
		for (int32 Index = 0; Index < Clip.Num() && Remaining.Num() >= 3; ++Index)
		{
			const FVector2D& A = Clip[Index];
			const FVector2D& B = Clip[(Index + 1) % Clip.Num()];
			ClipToLeft(Remaining, B, A, Outside);
			if (Outside.Num() >= 3 && PolygonArea(Outside) > MinPieceArea)
			{
				OutPieces.Add(Outside);
			}
			ClipToLeft(Remaining, A, B, Inside);
			Remaining = Inside;
		}
	}

	bool IsCoveredByUnion(const FPolygon2D& Subject, const TArray<FPolygon2D>& Covers, double AreaTolerance)
	{
		TArray<FPolygon2D> Pieces;
		Pieces.Add(Subject);
		for (const FPolygon2D& Cover : Covers)
		{
			TArray<FPolygon2D> Next;
			for (const FPolygon2D& Piece : Pieces)
			{
				SubtractConvex(Piece, Cover, Next);
			}
			Pieces = MoveTemp(Next);
			if (Pieces.Num() == 0)
			{
				return true;
			}
		}
		double Uncovered = 0.0;
		for (const FPolygon2D& Piece : Pieces)
		{
			Uncovered += PolygonArea(Piece);
		}
		return Uncovered <= AreaTolerance;
	}

	bool FitPlane(const TArray<FVector>& Points, FVector& OutCentroid, FVector& OutNormal)
	{
		if (Points.Num() < 3)
		{
			return false;
		}
		FVector Centroid = FVector::ZeroVector;
		for (const FVector& Point : Points)
		{
			Centroid += Point;
		}
		Centroid /= static_cast<double>(Points.Num());

		double Sxx = 0.0, Sxy = 0.0, Syy = 0.0, Sxz = 0.0, Syz = 0.0;
		for (const FVector& Point : Points)
		{
			const FVector D = Point - Centroid;
			Sxx += D.X * D.X;
			Sxy += D.X * D.Y;
			Syy += D.Y * D.Y;
			Sxz += D.X * D.Z;
			Syz += D.Y * D.Z;
		}
		const double Det = Sxx * Syy - Sxy * Sxy;
		const double Scale = FMath::Square(Sxx + Syy);
		if (Scale <= UE_SMALL_NUMBER || FMath::Abs(Det) <= 1.e-6 * Scale)
		{
			return false;
		}
		const double SlopeX = (Sxz * Syy - Syz * Sxy) / Det;
		const double SlopeY = (Syz * Sxx - Sxz * Sxy) / Det;
		OutCentroid = Centroid;
		OutNormal = FVector(-SlopeX, -SlopeY, 1.0).GetSafeNormal();
		return true;
	}

	FVector QuantizeVector(const FVector& Value, double Step)
	{
		return FVector(FMath::GridSnap(Value.X, Step), FMath::GridSnap(Value.Y, Step), FMath::GridSnap(Value.Z, Step));
	}

	bool IsUniformPositiveScale(const FVector& Scale, double Tolerance)
	{
		const double Reference = FMath::Max(FMath::Abs(Scale.X), 1.0);
		return Scale.X > Tolerance
			&& FMath::Abs(Scale.X - Scale.Y) <= Tolerance * Reference
			&& FMath::Abs(Scale.X - Scale.Z) <= Tolerance * Reference;
	}

	bool IsFiniteTransform(const FTransform& Transform)
	{
		return !Transform.ContainsNaN();
	}
}
