// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "Texture.h"
#include "TextureManager.h"


FTextureManager::FTextureManager()
{

}

FTextureManager::~FTextureManager()
{

}

void FTextureManager::Release()
{
	TextureCache.clear();
}

FTexturePtr FTextureManager::GetTexture(const std::wstring& AssetName)
{
	auto Iter = TextureCache.find(AssetName);
	if (Iter != TextureCache.end())
	{
		return Iter->second;
	}

	//load texture
	FTexturePtr Texture = std::make_shared<FTexture>(AssetName);
	if (Texture)
	{
		TextureCache[AssetName] = Texture;
	}

	return Texture;
}


bool FTextureManager::IsTexturePresent(const std::wstring& AssetName) const
{
	const auto Iter = TextureCache.find(AssetName);
	return Iter != TextureCache.end() && Iter->second->IsInitialized();
}

void FTextureManager::SetTexture(const std::wstring& AssetName, FTexturePtr Texture)
{
	if (Texture && Texture->IsInitialized())
	{
		TextureCache[AssetName] = Texture;
	}
}
