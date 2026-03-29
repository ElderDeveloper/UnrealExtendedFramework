// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Asset utility class
 */
class FAssetUtils
{
public:
	/**
	 * No copying or copy assignment allowed for this class.
	 */
	FAssetUtils(FAssetUtils const&) = delete;
	FAssetUtils& operator=(FAssetUtils const&) = delete;

	/** Get directory containing asset directory */
	static std::wstring GetAssetDir();

	/**
	* Singleton
	*/
	static FAssetUtils& Get();

private:
	FAssetUtils() = default;

	/** Directory path containing asset directory ("Asset") with asset files */
	static std::wstring AssetDir;
};
