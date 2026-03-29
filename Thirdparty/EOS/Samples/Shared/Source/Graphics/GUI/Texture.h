// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class FTexture;
using FTexturePtr = std::shared_ptr<FTexture>;

/**
 * Texture class
 */
class FTexture
{
public:
	/** Constructor */
	FTexture(const std::wstring& TexAssetFile);
	FTexture(const std::wstring& TexAssetFile, const std::vector<char>& TextureData);

	/** Destructor */
	virtual ~FTexture();

	/** Create */
	void Create();

	/** Release */
	void Release();

	/** Get texture asset name */
	const std::wstring& GetAssetName() const { return AssetFile; }

	/** Is texture initialized correctly? */
	bool IsInitialized() const { return bInitialized; }

#ifdef DXTK
	/** Get Texture */
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetTexture();
#endif //DXTK

#ifdef EOS_DEMO_SDL
	GLuint GetTexture();

	static FTexturePtr LoadFromSurface(SDL_Surface* Surface);
#endif // EOS_DEMO_SDL

private:
#ifdef EOS_DEMO_SDL
	explicit FTexture(GLuint TextureId);
	bool LoadDDSTextureFromFile(const std::wstring& TextureFile);
	bool LoadDDSTexture(const std::wstring& TextureFile, const std::vector<char>& TextureData);
#endif // EOS_DEMO_SDL

	/** Asset File */
	std::wstring AssetFile;

	/** Is texture loaded */
	bool bInitialized = false;

	/** Is this texture loaded from file or some other source? */
	const bool bFileTexture;

#ifdef DXTK
	/** Texture */
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Texture;
#endif //DXTK

#ifdef EOS_DEMO_SDL
	/** OpoenGL Texture ID */
	GLuint Texture;
#endif // EOS_DEMO_SDL
};
