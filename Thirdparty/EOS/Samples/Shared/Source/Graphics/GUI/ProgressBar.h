// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widget.h"
#include "Font.h"

/**
 * Forward declarations
 */
class FTextLabelWidget;
class FTexture;

/**
 * Checkbox Widget
 */
class FProgressBar : public IWidget
{
public:
	/**
	 * Constructor
	 */
	FProgressBar(Vector2 Pos,
		Vector2 Size,
		UILayer Layer,
		const std::wstring& Text,
		const std::wstring& FinishedTextureFilename,
		const std::wstring& UnFinishedTextureFilename,
		FontPtr Font,
		FColor LabelBackCol = FColor(1.f, 1.f, 1.f, 1.f),
		FColor LabelTextCol = FColor(1.f, 1.f, 1.f, 1.f)
	);

	/**
	 * Destructor
	 */
	virtual ~FProgressBar() {};

	/** IGfxComponent */
	virtual void Create() override;
	virtual void Release() override;
	virtual void Update() override;
	virtual void Render(FSpriteBatchPtr& Batch) override;
#ifdef _DEBUG
	virtual void DebugRender() override;
#endif

	/** IWidget */
	virtual void OnUIEvent(const FUIEvent& Event) override;
	virtual void SetPosition(Vector2 Pos) override;
	virtual void SetSize(Vector2 NewSize) override;

	/** 
	 * Sets current progress in the range [0.0, 1.0].
	*/
	void SetProgress(float Progress);

	/**
	 * Gets current progress.
	*/
	float GetProgress() const { return CurrentProgress; }

	/**
	 * Sets text to display on button label
	 */
	void SetText(const std::wstring& Text) { Label->SetText(Text); }

	/**
	 * Sets color of text for button label
	 */
	void SetTextColor(FColor Col) { Label->SetTextColor(Col); };

	/**
	 * Sets color of text for button background image
	 */
	void SetBackgroundColor(FColor Col) { Label->SetBackgroundColor(Col); };

	/**
	 * Sets color of text for button background image
     */
	void SetFont(FontPtr Font) { Label->SetFont(Font); }

protected:
	/** Progress bar label */
	std::shared_ptr<FTextLabelWidget> Label;
	std::wstring LabelText;

	std::wstring FinishedTextureFile;
	std::wstring UnfinishedTextureFile;

	std::shared_ptr<FTexture> FinishedTexture;
	std::shared_ptr<FTexture> UnfinishedTexture;

	float CurrentProgress = 0.0f;
};