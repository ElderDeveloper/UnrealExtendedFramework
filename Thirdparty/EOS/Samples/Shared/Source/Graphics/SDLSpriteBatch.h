// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#ifdef EOS_DEMO_SDL

#include "Texture.h"
#include "AnimatedTexture.h"
#include "Widget.h"
#include "VectorRender.h"

struct FSimpleVertex
{
	Vector3 Pos;
	Vector2 Tex;
};

struct FSimpleMesh
{
	std::vector<FSimpleVertex> Vertices;
	std::vector<unsigned int> TriangleList;
	FTexturePtr Texture;
	FAnimatedTexturePtr AnimatedTexture;
	FColor MeshColor = Color::White;

	void Add2DQuad(Vector2 Corner, Vector2 Size, UILayer Layer);
	void Add2DQuad(Vector2 Corner, Vector2 Size, Vector2 TextureCoordCorner, Vector2 TextureCoordSize, UILayer Layer);
	void Add2DLine(Vector2 Pos1, Vector2 Pos2, UILayer Layer);
	void Add2DHollowRect(Vector2 Corner, Vector2 Size, UILayer Layer);
	bool AreMaterialsEqual(const FSimpleMesh& Other) const;
};

class FSDLSpriteBatch
{
public:
	enum EMode
	{
		Render2D,
		Render3D,
		Render2DWires
	};

	FSDLSpriteBatch();
	~FSDLSpriteBatch();

	FSDLSpriteBatch(const FSDLSpriteBatch&) = delete;
	FSDLSpriteBatch& operator=(const FSDLSpriteBatch&) = delete;

	static void Init();

	void Begin(EMode Type = Render2D, FMatrix ModelView = BuildIdentity());
	void Draw(FSimpleMesh Mesh);
	//2D wires mode only
	void DrawLines(const std::vector<FColoredVertex>& QueueVertices, const std::vector<unsigned int>& QueueIndices);
	void End();

private:
	EMode CurrentMode = Render2D;
	struct FImpl;
	std::unique_ptr<FImpl> Impl;
};

using SpriteBatch = FSDLSpriteBatch;

#endif