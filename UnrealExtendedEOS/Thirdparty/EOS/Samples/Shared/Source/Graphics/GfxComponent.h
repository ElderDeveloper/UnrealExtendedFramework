// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#ifdef DXTK
namespace DirectX
{
	class SpriteBatch;
}
#define FSpriteBatchPtr std::unique_ptr<DirectX::SpriteBatch>
#endif // DXTK

#ifdef EOS_DEMO_SDL
class FSDLSpriteBatch;
#define FSpriteBatchPtr std::unique_ptr<FSDLSpriteBatch>
#endif

/**
* Graphics Component Base Class
*/
class IGfxComponent: public std::enable_shared_from_this<IGfxComponent>
{
public:
	/**
	* Constructor
	*/
	IGfxComponent() noexcept(false) {};

	/**
	* No copying or copy assignment allowed for this class.
	*/
	IGfxComponent(IGfxComponent const&) = delete;
	IGfxComponent& operator=(IGfxComponent const&) = delete;

	/**
	* Destructor
	*/
	virtual ~IGfxComponent() {};

	/**
	* Create
	*/
	virtual void Create() = 0;

	/**
	* Release
	*/
	virtual void Release() = 0;

	/**
	* Update
	*/
	virtual void Update() = 0;

	/**
	* Render
	*/
	virtual void Render(FSpriteBatchPtr& Batch) = 0;

#ifdef _DEBUG
	/**
	* Render in debugging mode
	*/
	virtual void DebugRender() = 0;
#endif
};