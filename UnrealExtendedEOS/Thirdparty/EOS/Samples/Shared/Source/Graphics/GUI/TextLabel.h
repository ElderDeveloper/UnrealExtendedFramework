// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widget.h"
#include "Font.h"

enum class EAlignmentType
{
	Center = 0,
	Left,
	Right
};

/**
 * Forward declarations
 */
class FSpriteWidget;

/**
 * Text Label class
 */
class FTextLabelWidget : public IWidget
{
public:
	/** Constructor */
	FTextLabelWidget(Vector2 LabelPos,
			   Vector2 LabelSize,
			   UILayer LabelLayer,
			   const std::wstring& LabelText,
			   const std::wstring& LabelAssetFile,
			   FColor LabelBackCol = FColor(1.f, 1.f, 1.f, 1.f),
			   FColor LabelTextCol = FColor(1.f, 1.f, 1.f, 1.f),
			   EAlignmentType LabelAllignmentType = EAlignmentType::Center);

	/** Constructor for animated texture */
	FTextLabelWidget(Vector2 LabelPos,
		Vector2 LabelSize,
		UILayer LabelLayer,
		const std::wstring& LabelText,
		const std::vector<std::wstring>& LabelAssetFiles,
		FColor LabelBackCol = FColor(1.f, 1.f, 1.f, 1.f),
		FColor LabelTextCol = FColor(1.f, 1.f, 1.f, 1.f),
		EAlignmentType LabelAllignmentType = EAlignmentType::Center);

	/** Destructor */
	virtual ~FTextLabelWidget() {};

	/** IGfxComponent */
	virtual void Create() override;
	virtual void Release() override;
	virtual void Update() override;
	virtual void Render(FSpriteBatchPtr& Batch) override;
#ifdef _DEBUG
	virtual void DebugRender() override;
#endif

	/** IWidget */
	virtual void OnUIEvent(const FUIEvent& event) override;
	virtual void SetPosition(Vector2 Pos) override;
	virtual void SetSize(Vector2 NewSize) override;

	/**
	 * Sets font to use to display text string
	 *
	 * @param NewFont - Font to use for text
	 */
	void SetFont(FontPtr NewFont);

	/**
	 * Gets font used to display text string
	 *
	 * @return Font used for text
	 */
	FontPtr GetFont();

	/**
	 * Sets text string that will be displayed on label
	 *
	 * @param LabelText - Text string to set
	 */
	void SetText(const std::wstring& LabelText);

	/** 
	* Implementation of 'SetData' for List view. Same as SetText.
	*/
	void SetData(const std::wstring& Data)
	{
		SetText(Data);
	}

	/**
	 * Gets text currently set for this label
	 */
	std::wstring GetText() { return Text; };

	/**
	 * Clears text string that will be displayed on label
	 */
	void ClearText();

	/**
	 * Gets position the text is relative to label position
	 */
	Vector2 GetTextPosition() { return TextPosition; };

	/**
	 * Sets color for text string that will be displayed on label
	 *
	 * @param Col - Color to set for text
	 */
	void SetTextColor(FColor Col);

	/**
	 * Sets color for background image
	 *
	 * @param Col - Color to set for background image
	 */
	void SetBackgroundColor(FColor Col);

	/**
	 * Gets FColor used for background image
	 *
	 * @return Color used for background image
	 */
	FColor GetBackgroundColor();

	/**
	 * Sets visibility for background image
	 *
	 * @param Visible - Visibility
	 */
	void SetBackgroundVisible(bool bVisible) { bBackgroundVisible = bVisible; };

	/**
	 * Gets visibility for background image
	 *
	 * @return True if background image is visible
	 */
	bool IsBackgroundVisible() { return bBackgroundVisible; };

	/** 
	 * Marks widget as dirty which can lead to rendering objects regeneration
	 */
	void MarkDirty();

	/** This way underlying background texture can be manipulated directly. */
	std::shared_ptr<FSpriteWidget> GetBackgroundImage() { return BackgroundImage; }

	/** Set horizontal offset */
	void SetHorizontalOffset(float Value) { HorizontalOffset = Value; }

protected:
	/**
	 * Sets position text will be relative to position of the label
	 *
	 * @param Pos - Position for text
	 */
	void UpdateTextPosition();

	/** Background Image */
	std::shared_ptr<FSpriteWidget> BackgroundImage;

	/** Text */
	std::wstring Text;

	/** Font */
	FontPtr Font;

	/** Position of text element */
	Vector2 TextPosition;

	/** Horizontal offset for text */
	float HorizontalOffset = 10.0f;

	/** Color of text */
	FColor TextCol;

	/** Alignment type for text */
	EAlignmentType AlignmentType;

	/** True if background is visible */
	bool bBackgroundVisible;

	/** Were the contents changed after last render? */
	bool bDirtyFlag = true;

#ifdef DXTK
	std::unique_ptr<DirectX::SpriteBatch> Sprites;
#endif // DXTK

#ifdef EOS_DEMO_SDL
	std::unique_ptr<FSDLSpriteBatch> Sprites;
	FTexturePtr FontTexture;
#endif
};