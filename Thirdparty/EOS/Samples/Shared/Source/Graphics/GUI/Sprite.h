// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widget.h"
#include "AnimatedTexture.h"

class FTexture;
class FAnimatedTexture;

// Sprite widget class (UI element)
class FSpriteWidget : public IWidget
{
public:
	FSpriteWidget(Vector2 SpritePos,
		Vector2 SpriteSize,
		UILayer SpriteLayer,
		const std::wstring& SpriteTextureFile,
		FColor SpriteBackCol = FColor(1.f, 1.f, 1.f, 1.f));

	FSpriteWidget(Vector2 SpritePos,
		Vector2 SpriteSize,
		UILayer SpriteLayer,
		const std::vector<std::wstring>& AnimatedSpriteFiles,
		FColor SpriteBackCol = FColor(1.f, 1.f, 1.f, 1.f));

	FSpriteWidget(Vector2 SpritePos,
		Vector2 SpriteSize,
		UILayer SpriteLayer,
		const std::wstring& SpriteTextureName,
		const std::vector<char>& SpriteTextureData,
		FColor SpriteBackCol = FColor(1.f, 1.f, 1.f, 1.f));
	

	virtual ~FSpriteWidget() {};

	virtual void Create() override;
	virtual void Release() override;
	virtual void Update() override;
	virtual void Render(FSpriteBatchPtr& Batch) override;
#ifdef _DEBUG
	virtual void DebugRender() override;
#endif

	virtual void OnUIEvent(const FUIEvent& event) override {};

	/**
	 * Sets color for background image
	 *
	 * @param Col - Color to set for background image
	 */
	void SetBackgroundColor(FColor Col);

	/**
	 * Gets color used for background image
	 *
	 * @return Color used for background image
	 */
	FColor GetBackgroundColor() { return BackgroundCol; }

	/** Get Texture */
	FTexturePtr GetTexture() { return Texture; }

	/** Getter for animated texture. */
	FAnimatedTexturePtr GetAnimatedTexture() { return AnimatedTexture; }

	/** Is texture loaded correctly and can be rendered? */
	const bool IsTextureValid() const;

protected:
	std::wstring TextureFile;
	std::vector<std::wstring> AnimatedTextureFiles;

	std::shared_ptr<FTexture> Texture;
	std::shared_ptr<FAnimatedTexture> AnimatedTexture;

	FColor BackgroundCol;

	bool bEnabled = true;
};