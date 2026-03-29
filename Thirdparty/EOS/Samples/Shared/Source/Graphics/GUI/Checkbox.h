// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widget.h"
#include "Font.h"

/**
 * Forward declarations
 */
class FTextLabelWidget;
class FSpriteWidget;

/**
 * Checkbox Widget
 */
class FCheckboxWidget : public IWidget
{
public:
	/**
	 * Constructor
	 */
	FCheckboxWidget(Vector2 CheckboxPos,
		Vector2 Size,
		UILayer Layer,
		const std::wstring& Text,
		const std::wstring& LabelBackTexture,
		FontPtr Font,
		const std::wstring& TickedTextureFile = L"Assets/checkbox_ticked.dds",
		const std::wstring& UntickedTextureFile = L"Assets/checkbox_unticked.dds",
		FColor LabelBackCol = FColor(1.f, 1.f, 1.f, 1.f),
		FColor LabelTextCol = FColor(1.f, 1.f, 1.f, 1.f)
	);

	/**
	 * Destructor
	 */
	virtual ~FCheckboxWidget() {};

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
	 * Sets callback function to be called when this button is pressed
	 */
	void SetOnTickedCallback(std::function<void(bool)> Callback);

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
	 * Sets the color of the ticked image
	 */
	void SetTickedColor(FColor Col) { TickedSprite->SetBackgroundColor(Col); };

	/**
	 * Sets the color of the unticked image
	 */
	void SetUntickedColor(FColor Col) { UntickedSprite->SetBackgroundColor(Col); };

	/** Get current state */
	bool IsTicked() const { return bTicked; }

	/** Set current state */
	void SetTicked(bool bValue, bool bNotifyCallback = false);

	/**
	 * Sets label's font
	 */
	void SetFont(FontPtr InFont) { Label->SetFont(InFont); }

protected:
	/** Button label */
	std::shared_ptr<FTextLabelWidget> Label;

	/** Tick sprite */
	std::shared_ptr<FSpriteWidget> TickedSprite;

	/** Tick sprite (unticked) */
	std::shared_ptr<FSpriteWidget> UntickedSprite;

	/** Callback function, called when button is pressed */
	std::function<void(bool)> OnTickedCallback;

	/** True if button is pressed */
	bool bTicked = false;
};