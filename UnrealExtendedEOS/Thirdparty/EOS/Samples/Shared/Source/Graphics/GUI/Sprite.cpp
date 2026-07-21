// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "Main.h"
#include "Texture.h"
#include "AnimatedTexture.h"
#include "Sprite.h"
#include "Game.h"
#include "TextureManager.h"

#ifdef DXTK
#include "SpriteBatch.h"
#endif

FSpriteWidget::FSpriteWidget(Vector2 SpritePos,
							 Vector2 SpriteSize,
							 UILayer SpriteLayer,
							 const std::wstring& SpriteTextureFile,
							 FColor SpriteBackCol) :
	IWidget(SpritePos, SpriteSize, SpriteLayer),
	TextureFile(SpriteTextureFile),
	BackgroundCol(SpriteBackCol)
{
	if (!TextureFile.empty())
	{
		Texture = FGame::Get().GetTextureManager()->GetTexture(TextureFile);
	}
}

FSpriteWidget::FSpriteWidget(Vector2 SpritePos,
	Vector2 SpriteSize,
	UILayer SpriteLayer,
	const std::wstring& SpriteTextureName,
	const std::vector<char>& SpriteTextureData,
	FColor SpriteBackCol) :
	IWidget(SpritePos, SpriteSize, SpriteLayer),
	TextureFile(SpriteTextureName),
	BackgroundCol(SpriteBackCol)
{
	if (!TextureFile.empty() && !SpriteTextureData.empty())
	{
		if (FGame::Get().GetTextureManager()->IsTexturePresent(TextureFile))
		{
			Texture = FGame::Get().GetTextureManager()->GetTexture(TextureFile);
		}
		else
		{
			Texture = std::make_shared<FTexture>(TextureFile, SpriteTextureData);
			FGame::Get().GetTextureManager()->SetTexture(TextureFile, Texture);
		}
	}
}

FSpriteWidget::FSpriteWidget(Vector2 SpritePos,
							Vector2 SpriteSize,
						    UILayer SpriteLayer,
							const std::vector<std::wstring>& AnimatedSpriteFiles,
							FColor SpriteBackCol) :
	IWidget(SpritePos, SpriteSize, SpriteLayer),
	AnimatedTextureFiles(AnimatedSpriteFiles),
	BackgroundCol(SpriteBackCol)
{
	if (!AnimatedTextureFiles.empty())
	{
		AnimatedTexture = std::make_shared<FAnimatedTexture>(AnimatedTextureFiles);
	}
}

void FSpriteWidget::Create()
{
	if (Texture == nullptr && AnimatedTexture == nullptr)
	{
		return;
	}

	if (Texture)
	{
		Texture->Create();
	}

	if (AnimatedTexture)
	{
		AnimatedTexture->Create();
	}
}

void FSpriteWidget::Release()
{
	Texture.reset();
	AnimatedTexture.reset();
}

void FSpriteWidget::Update()
{
	if (AnimatedTexture)
	{
		AnimatedTexture->Tick();
	}
}

void FSpriteWidget::Render(FSpriteBatchPtr& Batch)
{
	IWidget::Render(Batch);

	if (Batch &&
		((Texture && Texture->GetTexture()) ||
		(AnimatedTexture && AnimatedTexture->GetCurrentTexture())) )
	{
#ifdef DXTK
		// Rect for sprite
		RECT Rect = { LONG(Position.x), LONG(Position.y), LONG(Position.x + Size.x), LONG(Position.y + Size.y) };

		if (Texture)
		{
			Batch->Draw(Texture->GetTexture().Get(), Rect, BackgroundCol);
		}

		if (AnimatedTexture)
		{
			Batch->Draw(AnimatedTexture->GetCurrentTexture().Get(), Rect, BackgroundCol);
		}
#endif // DXTK

#ifdef EOS_DEMO_SDL
		FSimpleMesh Mesh;

		if (Texture)
		{
			Mesh.Texture = Texture;
		}

		if (AnimatedTexture)
		{
			Mesh.AnimatedTexture = AnimatedTexture;
		}

		Mesh.MeshColor = BackgroundCol;

		Mesh.Add2DQuad(Position, Size, Layer);

		Batch->Draw(Mesh);
#endif
	}
}

#ifdef _DEBUG
void FSpriteWidget::DebugRender()
{
	IWidget::DebugRender();
}
#endif

void FSpriteWidget::SetBackgroundColor(FColor Col)
{
	BackgroundCol = Col;
}

const bool FSpriteWidget::IsTextureValid() const
{
	bool bTextureValid = false;
	if (Texture)
	{
		bTextureValid = Texture->IsInitialized();
	}

	if (bTextureValid)
	{
		return true;
	}

	if (AnimatedTexture && AnimatedTexture->GetCurrentTexturePtr())
	{
		return AnimatedTexture->GetCurrentTexturePtr()->IsInitialized();
	}

	return false;
}
