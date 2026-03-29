// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "DebugLog.h"
#include "AccountHelpers.h"

FAccountHelpers::FAccountHelpers()
{

}

FAccountHelpers::~FAccountHelpers()
{

}

char const* FAccountHelpers::EpicAccountIDToString(EOS_EpicAccountId InAccountId)
{
	if (InAccountId == nullptr)
	{
		return "NULL";
	}

	static char TempBuffer[EOS_EPICACCOUNTID_MAX_LENGTH + 1];
	int32_t TempBufferSize = sizeof(TempBuffer);
	EOS_EResult Result = EOS_EpicAccountId_ToString(InAccountId, TempBuffer, &TempBufferSize);

	if (Result == EOS_EResult::EOS_Success)
	{
		return TempBuffer;
	}

	FDebugLog::LogError(L"[EOS SDK] Epic Account Id To String Error: %d", (int32_t)Result);

	return "ERROR";
}

char const* FAccountHelpers::ProductUserIDToString(EOS_ProductUserId InAccountId)
{
	if (InAccountId == nullptr)
	{
		return "NULL";
	}

	static char TempBuffer[EOS_PRODUCTUSERID_MAX_LENGTH + 1];
	int32_t TempBufferSize = sizeof(TempBuffer);
	EOS_EResult Result = EOS_ProductUserId_ToString(InAccountId, TempBuffer, &TempBufferSize);

	if (Result == EOS_EResult::EOS_Success)
	{
		return TempBuffer;
	}

	FDebugLog::LogError(L"[EOS SDK] Product User Id To String Error: %d", (int32_t)Result);

	return "ERROR";
}

EOS_EpicAccountId FAccountHelpers::EpicAccountIDFromString(const char* AccountString)
{
	if (AccountString == nullptr)
	{
		return nullptr;
	}

	return EOS_EpicAccountId_FromString(AccountString);
}

EOS_ProductUserId FAccountHelpers::ProductUserIDFromString(const char* ProductUserIdString)
{
	if (ProductUserIdString == nullptr)
	{
		return nullptr;
	}

	return EOS_ProductUserId_FromString(ProductUserIdString);
}
