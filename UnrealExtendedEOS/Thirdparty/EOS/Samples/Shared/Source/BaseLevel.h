// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GfxComponent.h"

/** Forward declarations */
class FGameEvent;
class FModel;

/**
* In-Game Level (base class)
*/
class FBaseLevel: public IGfxComponent
{
public:
	/**
	* Constructor
	*/
	FBaseLevel() noexcept(false);

	/**
	* No copying or copy assignment allowed for this class.
	*/
	FBaseLevel(FBaseLevel const&) = delete;
	FBaseLevel& operator=(FBaseLevel const&) = delete;

	/**
	* Destructor
	*/
	virtual ~FBaseLevel() {};

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

	/**
	* Receives game event
	*
	* @param Event - Game event to act on
	*/
	void OnGameEvent(const FGameEvent& Event);

private:
	/**
	 * Sets the color of the spinning cube.
	 * The color will stay on the cube until a new color is set.
	 *
	 * @param Col - Color to set the cube
	 */
	void SetCubeColor(FColor Col);

	/**
	 * Sets the color of the spinning cube for a certain time
	 * 
	 * @param Col - Color to set the cube
	 * @param TimeToKeepCol - Time the color will be set on the cube before it resets to default
	 */
	void SetCubeColor(FColor Col, float TimeToKeepCol);

	/** Updates timer for cube color */
	void UpdateCubeColTimer();

	/** Cube Rotation Timer */
	float CubeRotationTimer;

	/** True if cube is rotating */
	bool bRotateCube;

	/** Cube Position */
	Vector3 CubePosition;

	/** Cube Rotation */
	Vector3 CubeRotation;

	/** Cube Scale */
	Vector3 CubeScale;

	/** World Matrix */
	FMatrix World;

	/** Cube Model */
	std::shared_ptr<FModel> CubeModel;

	/** Timer to keep color on cube before reverting to default */
	float CubeColTimer = 0.f;

	/** Base color for rendering spinning cube */
	FColor CubeCol;

	/** Good color for rendering spinning cube */
	static constexpr FColor DefaultCubeCol = FColor(1.f, 1.f, 1.f, 1.f);

	/** Good color for rendering spinning cube */
	static constexpr FColor GoodCubeCol = FColor(0.f, 0.5f, 0.f, 1.f);

	/** Bad color for rendering spinning cube */
	static constexpr FColor BadCubeCol = FColor(0.5f, 0.0f, 0.f, 1.f);

	/** Default time to set cube color */
	static constexpr float DefaultCubeColTime = 2.f;
};