// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class FTexture;
using FTexturePtr = std::shared_ptr<FTexture>;

/**
 * Texture manager class
 */
class FTextureManager
{
public:
	/** Constructor */
	FTextureManager();

	/** Destructor */
	virtual ~FTextureManager();

	FTextureManager(const FTextureManager&) = delete;
	FTextureManager& operator=(const FTextureManager&) = delete;

	/** Release */
	void Release();

	/** Get texture */
	FTexturePtr GetTexture(const std::wstring& AssetName);

	/** Is texture in cache. */
	bool IsTexturePresent(const std::wstring& AssetName) const;

	/** Manually set texture (should only be used for textures coming not from file). */
	void SetTexture(const std::wstring& AssetName, FTexturePtr Texture);

private:
	/** Asset File */
	std::unordered_map<std::wstring, FTexturePtr> TextureCache;
};
