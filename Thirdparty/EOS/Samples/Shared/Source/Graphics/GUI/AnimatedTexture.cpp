// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "DebugLog.h"
#include "Main.h"
#include "AnimatedTexture.h"
#include "Texture.h"
#include "Game.h"
#include "TextureManager.h"

FAnimatedTexture::FAnimatedTexture(const std::vector<std::wstring>& InAssetFiles): AssetFiles(InAssetFiles)
{
	Textures.resize(AssetFiles.size());

	for (size_t i = 0; i < AssetFiles.size(); ++i)
	{
		Textures[i] = FGame::Get().GetTextureManager()->GetTexture(AssetFiles[i]);
	}
}

FAnimatedTexture::FAnimatedTexture(std::vector<FTexturePtr>&& InTextures) : Textures(InTextures)
{

}

FAnimatedTexture::~FAnimatedTexture()
{

}

void FAnimatedTexture::Create()
{
	for (size_t i = 0; i < Textures.size(); ++i)
	{
		if (Textures[i])
		{
			Textures[i]->Create();
		}
	}
}

void FAnimatedTexture::Release()
{
	Textures.clear();
}

void FAnimatedTexture::Play(float InFPS, bool bLoop /*= false*/)
{
	FPS = InFPS;
	bLooping = bLoop;

	Progress = 0.0f;
	bPlaying = true;
}

void FAnimatedTexture::Tick()
{
	if (bPlaying)
	{
		float DT = static_cast<float>(Main->GetTimer().GetElapsedSeconds());
		Progress += DT;

		float AnimDuration = GetAnimDuration();
		if (AnimDuration < Progress)
		{
			if (bLooping)
			{
				Progress = Progress - AnimDuration;
			}
			else
			{
				Progress = 0.0f;
				bPlaying = false;
			}
		}

		if (Progress > GetAnimDuration())
		{
			Progress = GetAnimDuration();
		}

		CurrentFrame = static_cast<size_t>((Textures.size() * Progress) / GetAnimDuration());
	}
}

void FAnimatedTexture::Stop()
{
	bPlaying = false;
	Progress = 0.0f;
	CurrentFrame = 0;
}

void FAnimatedTexture::Pause()
{
	bPlaying = false;
}

const FTexturePtr FAnimatedTexture::GetCurrentTexturePtr() const
{
	if (CurrentFrame < Textures.size())
	{
		return Textures[CurrentFrame];
	}

	return nullptr;
}

#ifdef DXTK
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> FAnimatedTexture::GetCurrentTexture()
{
	if (CurrentFrame < Textures.size())
	{
		return Textures[CurrentFrame]->GetTexture();
	}

	return nullptr;
}
#endif //DXTK

#ifdef EOS_DEMO_SDL
GLuint FAnimatedTexture::GetCurrentTexture()
{
	if (CurrentFrame < Textures.size())
	{
		return Textures[CurrentFrame]->GetTexture();
	}

	return 0;
}
#endif // EOS_DEMO_SDL