// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GfxComponent.h"

#ifdef DXTK
#include "GeometricPrimitive.h"
#endif

/** Model Type */
enum class EModelType
{
	/** None */
	None,

	/** Cube */
	Cube
};

/**
 * Forward declarations
 */
class FTexture;

/**
* In-Game Model
*/
class FModel : public IGfxComponent
{
public:
	/**
	* Constructor
	*/
	FModel(EModelType Type, float Size, const std::wstring& TextureFile) noexcept(false);

	/**
	* No copying or copy assignment allowed for this class.
	*/
	FModel(FModel const&) = delete;
	FModel& operator=(FModel const&) = delete;

	/**
	* Destructor
	*/
	virtual ~FModel() {};

	/**
	* IGfxComponent Overrides
	*/
	virtual void Create() override;
	virtual void Release() override;
	virtual void Update() override;
	virtual void Render(FSpriteBatchPtr& Batch) override;
#ifdef _DEBUG
	virtual void DebugRender() override;
#endif

	void SetWorldMatrix(FMatrix Matrix) { WorldMatrix = Matrix; }

	void SetColor(FColor Col) { ModelCol = Col; };

#ifdef EOS_DEMO_SDL
	void SetPosition(Vector3 Pos);
	void SetRotation(Vector3 Rot);
	void SetScale(Vector3 Scale);
#endif // EOS_DEMO_SDL
	
private:
#ifdef EOS_DEMO_SDL
	/** Draw Model */
	void GenerateModelMesh();

	/** Draw Cube */
	void GenerateCube();

	/** Load OpenGL Texture */
	void LoadGLTexture(const std::wstring& TextureFile);
#endif // EOS_DEMO_SDL

	/** Model File */
	EModelType ModelType;

	/** Texture File */
	std::wstring TextureFile;

#ifdef DXTK
	/** Model Primitive */
	std::unique_ptr<DirectX::GeometricPrimitive> Model;
#endif // DXTK

#ifdef EOS_DEMO_SDL
	/** Mesh */
	FSimpleMesh Mesh;

	/** Batch for rendering */
	std::unique_ptr<FSDLSpriteBatch> SpriteBatch;

	FMatrix Transform = BuildIdentity();
#endif //EOS_DEMO_SDL

	/** Cube Texture */
	std::shared_ptr<FTexture> Texture;

	/** World Matrix */
	FMatrix WorldMatrix;

	/** Color for rendering model */
	FColor ModelCol;

	/** Model Size */
	float ModelSize;
};