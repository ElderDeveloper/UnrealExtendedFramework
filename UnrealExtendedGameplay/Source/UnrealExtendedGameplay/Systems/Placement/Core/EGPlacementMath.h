// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementTypes.h"

/**
 * Stateless placement math: surface frames, spin, snapping, oriented boxes and plane polygons.
 * No world access. Everything here is deterministic for the same inputs, which is what lets an
 * authority solve a client's pose again and land on the same answer.
 *
 * Rotation model: a placed rotation is Frame * Swing(Z -> Up in frame space) * Twist(Spin about Z).
 * Frame is the stable grid frame (world or support), Up the object's up, Spin the angle the
 * spin step snaps. ExtractSpin inverts it exactly.
 */
namespace EGPlacementMath
{
	/** Position tolerance used by the solvers and the idempotence contract, in cm. */
	inline constexpr double PoseTolerance = 0.05;
	/** Angle tolerance of the idempotence contract, in degrees. */
	inline constexpr double AngleToleranceDegrees = 0.05;

	using FPolygon2D = TArray<FVector2D>;

	UNREALEXTENDEDGAMEPLAY_API EEGPlacementOrientation ClassifyNormal(const FVector& Normal, const FEGPlacementSurfaceBands& Bands);

	/** Degrees between Normal and the ideal for its orientation: straight up for floors, down for ceilings, level for walls. */
	UNREALEXTENDEDGAMEPLAY_API float SlopeDegrees(const FVector& Normal, EEGPlacementOrientation Orientation);

	/** The object's up for a surface: the normal, or world up/down, or the normal made level for walls. */
	UNREALEXTENDEDGAMEPLAY_API FVector AlignedUp(const FVector& Normal, EEGPlacementOrientation Orientation, EEGPlacementAlignment Alignment);

	/** Frame * Swing(Z -> Up) * Twist(SpinDegrees). */
	UNREALEXTENDEDGAMEPLAY_API FQuat MakeRotation(const FQuat& Frame, const FVector& Up, float SpinDegrees);

	/** The spin of Rotation about Up, measured in Frame, after the smallest turn that aligns Rotation's up with Up. Inverse of MakeRotation. */
	UNREALEXTENDEDGAMEPLAY_API float ExtractSpin(const FQuat& Rotation, const FQuat& Frame, const FVector& Up);

	/** Rounds to the nearest multiple of Step; Step <= 0 returns Degrees unchanged. Result in (-180, 180]. */
	UNREALEXTENDEDGAMEPLAY_API float SnapAngle(float Degrees, float Step);

	/** The index (0 X, 1 Y, 2 Z) of Frame's axis most parallel to Direction. Ties go to the lower index. */
	UNREALEXTENDEDGAMEPLAY_API int32 DominantAxis(const FQuat& Frame, const FVector& Direction);

	/** Snaps World to the grid in Frame on every axis except SkipAxis (-1 snaps all three). */
	UNREALEXTENDEDGAMEPLAY_API FVector SnapToGrid(const FVector& World, const FTransform& Frame, const FVector& Step, const FVector& Origin, int32 SkipAxis);

	/** Box in pivot space to world space. Pose must have positive uniform scale; its X scale is used. */
	UNREALEXTENDEDGAMEPLAY_API FEGPlacementOrientedBox TransformBox(const FEGPlacementBox& Box, const FTransform& Pose);

	/** Exact separating-axis test (15 axes). Touching counts as intersecting; Margin > 0 also counts near misses. */
	UNREALEXTENDEDGAMEPLAY_API bool BoxesIntersect(const FEGPlacementOrientedBox& A, const FEGPlacementOrientedBox& B, double Margin = 0.0);

	UNREALEXTENDEDGAMEPLAY_API bool BoxContainsPoint(const FEGPlacementOrientedBox& Box, const FVector& Point, double Tolerance = PoseTolerance);

	/** Exact: a convex box contains another iff it contains its eight corners. */
	UNREALEXTENDEDGAMEPLAY_API bool BoxContainsBox(const FEGPlacementOrientedBox& Outer, const FEGPlacementOrientedBox& Inner, double Tolerance = PoseTolerance);

	/** The box shrunk by Amount on every side (never below zero). */
	UNREALEXTENDEDGAMEPLAY_API FEGPlacementOrientedBox Inflate(const FEGPlacementOrientedBox& Box, double Amount);

	/** Two unit vectors spanning the plane with the given normal. Deterministic for a given normal. */
	UNREALEXTENDEDGAMEPLAY_API void MakePlaneBasis(const FVector& Normal, FVector& OutU, FVector& OutV);

	/** Counter-clockwise convex hull (Andrew's monotone chain). Collinear points are dropped. */
	UNREALEXTENDEDGAMEPLAY_API void ConvexHull2D(TArray<FVector2D> Points, FPolygon2D& OutHull);

	/** The box's shadow on the plane through Origin spanned by U and V, as a CCW hull. */
	UNREALEXTENDEDGAMEPLAY_API void ProjectBoxToPlane(const FEGPlacementOrientedBox& Box, const FVector& Origin, const FVector& U, const FVector& V, FPolygon2D& OutHull);

	UNREALEXTENDEDGAMEPLAY_API double PolygonArea(const FPolygon2D& Polygon);

	/** Keeps the part of Polygon left of the directed line A->B (inclusive). */
	UNREALEXTENDEDGAMEPLAY_API void ClipToLeft(const FPolygon2D& Polygon, const FVector2D& A, const FVector2D& B, FPolygon2D& Out);

	/** Subject minus a convex CCW polygon, as convex pieces. */
	UNREALEXTENDEDGAMEPLAY_API void SubtractConvex(const FPolygon2D& Subject, const FPolygon2D& Clip, TArray<FPolygon2D>& OutPieces);

	/** Exact: whether the union of convex CCW Covers contains convex Subject, up to AreaTolerance of uncovered area. */
	UNREALEXTENDEDGAMEPLAY_API bool IsCoveredByUnion(const FPolygon2D& Subject, const TArray<FPolygon2D>& Covers, double AreaTolerance = 0.01);

	/** Least-squares plane z = ax + by + c through Points. OutNormal points up. False for fewer than three non-collinear points. */
	UNREALEXTENDEDGAMEPLAY_API bool FitPlane(const TArray<FVector>& Points, FVector& OutCentroid, FVector& OutNormal);

	/** Rounds each component to Step. */
	UNREALEXTENDEDGAMEPLAY_API FVector QuantizeVector(const FVector& Value, double Step);

	UNREALEXTENDEDGAMEPLAY_API bool IsUniformPositiveScale(const FVector& Scale, double Tolerance = 1.e-3);

	UNREALEXTENDEDGAMEPLAY_API bool IsFiniteTransform(const FTransform& Transform);
}
