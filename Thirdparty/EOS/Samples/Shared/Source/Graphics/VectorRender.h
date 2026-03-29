// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GfxComponent.h"

#ifdef DXTK
#include "PrimitiveBatch.h"
#include "Effects.h"
#include "VertexTypes.h"
#endif // DXTK

struct FColoredVertex
{
	Vector3 Position;
	FColor Color;
};


/**
* Vector Graphics Rendering
*/
class FVectorRender : IGfxComponent
{
public:
	/**
	* Constructor
	*/
	FVectorRender() noexcept(false);

	/**
	* No copying or copy assignment allowed for this class.
	*/
	FVectorRender(FVectorRender const&) = delete;
	FVectorRender& operator=(FVectorRender const&) = delete;

	/**
	* Destructor
	*/
	virtual ~FVectorRender() {};

	/**
	* IGfxComponent Overrides
	*/
	virtual void Create() override;
	virtual void Release() override {};
	virtual void Update() override {};
	virtual void Render(FSpriteBatchPtr&) override {};
#ifdef _DEBUG
	virtual void DebugRender() override {};
#endif

	/** Begin batch render */
	void Begin();

	/** End batch render */
	void End();

	void AddLine(const FColoredVertex& Vertex1, const FColoredVertex& Vertex2, size_t LineWidth = 1);
	void AddPolygon(const std::vector<FColoredVertex> Vertices, size_t LineWidth = 1);

	void DrawQueue();

	/** Set Enabled */
	void SetDebugRenderEnabled(bool bNewEnabled) { bEnabled = bNewEnabled; }
	
	/** Toggle Enabled */
	void ToggleDebugRender() { bEnabled = !bEnabled; }

	/** Is Enabled */
	bool IsDebugRenderEnabled() { return bEnabled; }

#ifdef DXTK
	/** Accessor for vector render batch */
	std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>>& GetRenderBatch()
	{
		return VectorRenderBatch;
	}
#endif // DXTK

#ifdef EOS_DEMO_SDL
	/** Accessor for vector render batch */
	std::unique_ptr<FSDLSpriteBatch>& GetRenderBatch()
	{
		return VectorRenderBatch;
	}
#endif // EOS_DEMO_SDL

private:
#ifdef DXTK
	/** Render effect */
	std::unique_ptr<DirectX::BasicEffect> VectorRenderEffect;

	/** Input layout */
	Microsoft::WRL::ComPtr<ID3D11InputLayout> VectorRenderInputLayout;

	/** Debug draw batch for rendering vector graphics. */
	std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> VectorRenderBatch;
#endif

#ifdef EOS_DEMO_SDL
	/** Debug draw batch for rendering of vector graphics. */
	std::unique_ptr<FSDLSpriteBatch> VectorRenderBatch;
#endif // EOS_DEMO_SDL

	std::vector<FColoredVertex> QueueVertices;
	std::vector<unsigned int> QueueIndices;

	bool bEnabled;
};