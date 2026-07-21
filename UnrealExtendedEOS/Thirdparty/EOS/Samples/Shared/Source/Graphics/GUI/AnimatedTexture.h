// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Texture.h"

class FAnimatedTexture;
using FAnimatedTexturePtr = std::shared_ptr<FAnimatedTexture>;

/**
 * Animated texture class
 */
class FAnimatedTexture
{
public:
	/** Constructor */
	FAnimatedTexture(const std::vector<std::wstring>& AssetFiles);
	FAnimatedTexture(std::vector<FTexturePtr>&& Textures);


	/** Destructor */
	virtual ~FAnimatedTexture();

	/** Create */
	void Create();

	/** Release */
	void Release();

	void Play(float FPS, bool bLoop = false);
	void Tick();
	void Stop();
	void Pause();
	void SetFrame(size_t Frame) { CurrentFrame = Frame; }
	size_t GetCurrentFrame() const { return CurrentFrame; }
	size_t GetNumFrames() const { return AssetFiles.size();	}
	float GetAnimDuration() const { return Textures.size() / FPS; }

	const std::vector<std::wstring>& GetAssetNames() const { return AssetFiles; }

	const FTexturePtr GetCurrentTexturePtr() const;

#ifdef DXTK
	/** Get Texture */
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetCurrentTexture();
#endif //DXTK

#ifdef EOS_DEMO_SDL
	GLuint GetCurrentTexture();
#endif // EOS_DEMO_SDL

private:
	/** Asset Files */
	std::vector<std::wstring> AssetFiles;

	/** Texture frames */
	std::vector<FTexturePtr> Textures;

	/** Current frame being displayed */
	size_t CurrentFrame = 0;

	/** Currently playing */
	bool bPlaying = false;

	/** Is playing looping animation */
	bool bLooping = false;

	/** Playback frames per second */
	float FPS = 30.0f;

	/** Progress (seconds) from anim start */
	float Progress = 0.0f;
};
