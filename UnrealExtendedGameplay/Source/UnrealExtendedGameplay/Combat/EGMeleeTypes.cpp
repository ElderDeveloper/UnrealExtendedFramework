// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGMeleeTypes.h"

void EGMelee::SampleSegment(const FVector& A, const FVector& B, int32 Count, TArray<FVector>& OutPoints)
{
	OutPoints.Reset();
	if (Count <= 1)
	{
		OutPoints.Add(A);
		return;
	}

	OutPoints.Reserve(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		OutPoints.Add(FMath::Lerp(A, B, static_cast<double>(Index) / static_cast<double>(Count - 1)));
	}
}
