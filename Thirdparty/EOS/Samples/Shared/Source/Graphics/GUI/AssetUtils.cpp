// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "AssetUtils.h"
#include "DebugLog.h"
#include "CommandLine.h"
#include "StringUtils.h"

std::wstring FAssetUtils::AssetDir = L"";

FAssetUtils& FAssetUtils::Get()
{
	static FAssetUtils AssetUtils;
	return AssetUtils;
}

std::wstring FAssetUtils::GetAssetDir()
{
	if (AssetDir.empty())
	{
#ifdef DXTK
		// Create AssetDir based on executable working directory
		wchar_t CurDir[MAX_PATH + 1] = {};
		::GetCurrentDirectoryW(MAX_PATH + 1u, CurDir);
		std::wstring BaseAssetPath = std::wstring(CurDir);
#endif
#ifdef EOS_DEMO_SDL
		// Create AssetDir based on executable base path
		std::wstring BaseAssetPath = FStringUtils::Widen(SDL_GetBasePath());
#endif // EOS_DEMO_SDL
		AssetDir = FCommandLine::Get().HasParam(CommandLineConstants::AssetDir) ?
			FCommandLine::Get().GetParamValue(CommandLineConstants::AssetDir) : std::wstring(EOS_ASSETS_PATH_PREFIX);
		if (BaseAssetPath.back() != L'/' && BaseAssetPath.back() != L'\\' && AssetDir.front() != L'/' && AssetDir.front() != L'\\')
		{
			BaseAssetPath += L'/';
		}
		AssetDir = BaseAssetPath + AssetDir;
		if (AssetDir.back() != L'/' && AssetDir.back() != L'\\')
		{
			AssetDir += L'/';
		}
		FDebugLog::Log(L"AssetDir: %ls", AssetDir.c_str());
	}

	return AssetDir;
}
