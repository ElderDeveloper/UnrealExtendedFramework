// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformMisc.h"
#include "IO/IoHash.h"

namespace ESQLUsqliteHash
{
	inline const TCHAR* GetDefaultAlgorithm()
	{
		return TEXT("sha256");
	}

	inline FString NormalizeAlgorithm(const FString& InAlgorithm)
	{
		const FString TrimmedAlgorithm = InAlgorithm.TrimStartAndEnd().ToLower();
		return TrimmedAlgorithm.IsEmpty() ? FString(GetDefaultAlgorithm()) : TrimmedAlgorithm;
	}

	inline bool ComputeTextHash(const FString& Text, const FString& InAlgorithm, FString& OutHash)
	{
		const FString Algorithm = NormalizeAlgorithm(InAlgorithm);
		FTCHARToUTF8 Utf8(*Text);
		const void* Data = Utf8.Length() > 0 ? static_cast<const void*>(Utf8.Get()) : static_cast<const void*>("");

		if (Algorithm == TEXT("sha256"))
		{
			check(Utf8.Length() >= 0);
			FSHA256Signature Signature;
			if (!FPlatformMisc::GetSHA256Signature(Data, static_cast<uint32>(Utf8.Length()), Signature))
			{
				return false;
			}

			OutHash = Signature.ToString();
			return true;
		}

		if (Algorithm == TEXT("blake3"))
		{
			OutHash = LexToString(FIoHash::HashBuffer(Data, Utf8.Length()));
			return true;
		}

		return false;
	}
}