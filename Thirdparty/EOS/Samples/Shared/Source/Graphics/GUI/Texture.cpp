// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "Main.h"
#include "Texture.h"
#include "DebugLog.h"
#include "CommandLine.h"
#include "StringUtils.h"
#include "AssetUtils.h"

#ifdef DXTK
#include "DDSTextureLoader.h"
#endif // DXTK

#ifdef EOS_DEMO_SDL

namespace
{
	const unsigned long FOURCC_DXT1 = 0x31545844;
	const unsigned long FOURCC_DXT3 = 0x33545844;
	const unsigned long FOURCC_DXT5 = 0x35545844;
}
#endif // EOS_DEMO_SDL

FTexture::FTexture(const std::wstring& TexAssetFile):
	AssetFile(TexAssetFile),
	bFileTexture(true)
{
	
}

FTexture::FTexture(const std::wstring& TexAssetFile, const std::vector<char>& TextureData) :
	AssetFile(TexAssetFile),
	bFileTexture(false)
{
#ifdef DXTK
	std::unique_ptr<DeviceResources> const& DeviceResources = Main->GetDeviceResources();
	ID3D11Device* Device = DeviceResources->GetD3DDevice();

	if (DirectX::CreateDDSTextureFromMemory(Device, (const uint8_t*)(TextureData.data()), TextureData.size(), nullptr, Texture.ReleaseAndGetAddressOf()) == S_OK)
	{
		bInitialized = true;
	}
#endif // DXTK

#ifdef EOS_DEMO_SDL
	bInitialized = LoadDDSTexture(TexAssetFile, TextureData);
#endif // EOS_DEMO_SDL

	if (!bInitialized)
	{
		FDebugLog::LogError(L"Could not initialize texture: %ls.", TexAssetFile.c_str());
	}
}

#ifdef EOS_DEMO_SDL
FTexture::FTexture(GLuint TextureId):
	bFileTexture(false)
{
	Texture = TextureId;
	bInitialized = true;
}
#endif // EOS_DEMO_SDL

FTexture::~FTexture()
{
#ifdef EOS_DEMO_SDL
	if (bInitialized)
	{
		glDeleteTextures(1, &Texture);
	}
#endif // EOS_DEMO_SDL
}

void FTexture::Create()
{
	//Delayed loading from file
	if (!AssetFile.empty() && bFileTexture)
	{
		if (bInitialized)
		{
			return;
		}

		std::wstring AssetPath = FAssetUtils::Get().GetAssetDir() + AssetFile;

#ifdef DXTK
		std::unique_ptr<DeviceResources> const& DeviceResources = Main->GetDeviceResources();
		ID3D11Device* Device = DeviceResources->GetD3DDevice();

		if (DirectX::CreateDDSTextureFromFile(Device, AssetPath.c_str(), nullptr, Texture.ReleaseAndGetAddressOf()) == S_OK)
		{
			bInitialized = true;
		}
#endif // DXTK

#ifdef EOS_DEMO_SDL
		if (LoadDDSTextureFromFile(AssetPath))
		{
			bInitialized = true;
		}
#endif // EOS_DEMO_SDL		

		if (!bInitialized)
		{
			FDebugLog::LogError(L"Could not initialize texture: %ls.", AssetFile.c_str());
		}
	}
}

void FTexture::Release()
{
	if (!bInitialized)
	{
		return;
	}

#ifdef DXTK
	if (Texture)
	{
		Texture.Reset();
	}
#endif

#ifdef EOS_DEMO_SDL
	glDeleteTextures(1, &Texture);
	Texture = 0;
#endif //EOS_DEMO_SDL

	bInitialized = false;
}

#ifdef DXTK
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> FTexture::GetTexture()
{
	if (Texture)
	{
		return Texture;
	}

	return NULL;
}
#endif //DXTK


#ifdef EOS_DEMO_SDL
GLuint FTexture::GetTexture()
{
	return Texture;
}

FTexturePtr FTexture::LoadFromSurface(SDL_Surface* Surface)
{
	if (Surface)
	{
		GLuint NewTexture = 0;
		glActiveTexture(GL_TEXTURE0);
		glGenTextures(1, &NewTexture);

		// Bind the texture object
		glBindTexture(GL_TEXTURE_2D, NewTexture);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		GLenum TextureFormat = GL_NONE;

		Uint32 SDLSurfaceFormat = Surface->format->format;
		int Width = Surface->w, Height = Surface->h;

		if (SDLSurfaceFormat == SDL_PIXELFORMAT_RGBA32)
			TextureFormat = GL_RGBA;
		else if (SDLSurfaceFormat == SDL_PIXELFORMAT_BGRA32)
			TextureFormat = GL_BGRA;
		else
		{
			glDeleteTextures(1, &NewTexture);

			FDebugLog::LogError(L"Could not load texture from unsupported SDL surface format.");
			return nullptr;
		}

		void* Pixels = Surface->pixels;

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Width, Height, 0, TextureFormat, GL_UNSIGNED_BYTE, Pixels);

		if (Main->GLError(L"FTexture::LoadFromSurface"))
		{
			glDeleteTextures(1, &NewTexture);
			return nullptr;
		}

		return FTexturePtr(new FTexture(NewTexture));
	}

	return nullptr;
}


bool FTexture::LoadDDSTextureFromFile(const std::wstring& TextureFile)
{
	if (bInitialized)
	{
		Release();
	}

	if (glCompressedTexImage2D == NULL)
	{
		FDebugLog::LogError(L"Failed to load texture: %ls. glCompressedTexImage2D proc not found!", TextureFile.c_str());
		return false;
	}

#ifdef WIN32
	FILE *File;
	_wfopen_s(&File, TextureFile.c_str(), L"rb");
#else
	FILE *File = fopen(FStringUtils::Narrow(TextureFile.c_str()).c_str(), "rb");
#endif
	if (File == NULL)
	{
		FDebugLog::LogError(L"Failed to load texture: %ls. File not found!", TextureFile.c_str());
		return false;
	}

	std::vector<char> FileData;

	fseek(File, 0, SEEK_END);
	long FileSize = ftell(File);
	fseek(File, 0, SEEK_SET);

	FileData.resize(FileSize);
	size_t NumBytesRead = fread(FileData.data(), 1, FileSize, File);
	fclose(File);

	if (NumBytesRead == 0)
	{
		FDebugLog::LogError(L"Failed to load texture. Zero bytes read!");
		return false;
	}

	return LoadDDSTexture(TextureFile, FileData);
}

bool FTexture::LoadDDSTexture(const std::wstring& TextureFile, const std::vector<char>& TextureData)
{
	if (bInitialized)
	{
		Release();
	}

	if (glCompressedTexImage2D == NULL)
	{
		FDebugLog::LogError(L"Failed to load texture. glCompressedTexImage2D proc not found!");
		return false;
	}

	if (TextureData.size() < 128)
	{
		FDebugLog::LogError(L"Failed to load texture: %ls. DDS file too small.", TextureFile.c_str());
		return false;
	}

	if (strncmp(TextureData.data(), "DDS ", 4) != 0)
	{
		FDebugLog::LogError(L"Failed to load texture: %ls. Not a DDS file!", TextureFile.c_str());
		return false;
	}

	unsigned int Height = *(unsigned int*)&(TextureData[8 + 4]);
	unsigned int Width = *(unsigned int*)&(TextureData[12 + 4]);
	unsigned int LinearSize = *(unsigned int*)&(TextureData[16 + 4]);
	unsigned int MipMapCount = *(unsigned int*)&(TextureData[24 + 4]);
	unsigned int FourCC = *(unsigned int*)&(TextureData[80 + 4]);

	unsigned int Bufsize = MipMapCount > 1 ? LinearSize * 2 : LinearSize;

	unsigned int Components = (FourCC == FOURCC_DXT1) ? 3 : 4;
	unsigned int Format;
	switch (FourCC)
	{
	case FOURCC_DXT1:
		Format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
		break;
	case FOURCC_DXT3:
		Format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
		break;
	case FOURCC_DXT5:
		Format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
		break;
	default:
		FDebugLog::LogError(L"Failed to load texture: %ls. Unsupported compression format of DDS file!", TextureFile.c_str());
		return false;
	}

	glGenTextures(1, &Texture);
	glBindTexture(GL_TEXTURE_2D, Texture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	unsigned int BlockSize = (Format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT) ? 8 : 16;
	unsigned int Offset = 0;
	for (unsigned int Level = 0; Level < MipMapCount && (Width || Height); ++Level)
	{
		unsigned int Size = ((Width + 3) / 4)*((Height + 3) / 4) * BlockSize;
		glCompressedTexImage2D(GL_TEXTURE_2D, Level, Format, Width, Height, 0, Size, TextureData.data() + 128 + Offset);

		Offset += Size;
		Width /= 2;
		Height /= 2;
	}

	return true;
}
#endif // EOS_DEMO_SDL
