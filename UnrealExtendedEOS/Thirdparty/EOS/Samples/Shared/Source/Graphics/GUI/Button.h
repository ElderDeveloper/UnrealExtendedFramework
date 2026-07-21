// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widget.h"
#include "Font.h"

/**
 * Forward declarations
 */
class FTextLabelWidget;

/**
 * Button Widget
 */
class FButtonWidget : public IWidget
{
public:
	enum class EButtonVisualState : size_t
	{
		Idle = 0,
		Pressed = 1,
		Hovered = 2,
		Disabled = 3,
		Last
	};

	/**
	 * Constructor for animated presses
	 */
	FButtonWidget(Vector2 position,
		Vector2 size,
		UILayer layer,
		const std::wstring& label,
		const std::vector<std::wstring>& Textures,
		FontPtr font,
		FColor backCol = FColor(1.f, 1.f, 1.f, 1.f),
		FColor textCol = FColor(1.f, 1.f, 1.f, 1.f),
		EAlignmentType LabelAllignmentType = EAlignmentType::Center);

	/**
	 * Destructor
	 */
	virtual ~FButtonWidget() {};

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
	virtual void Enable() override;
	virtual void Disable() override;
	virtual void SetFocused(bool bFocused) override;

	/**
	 * Sets callback function to be called when this button is pressed
	 */
	void SetOnPressedCallback(std::function<void()> Callback);

	/**
	 * Sets text to display on button label
	 */
	void SetText(const std::wstring& Text) { Label->SetText(Text); }

	/**
	 * Sets color of text for button label
	 */
	void SetTextColor(FColor Col) { Label->SetTextColor(Col); };

	/**
	 * Sets color for button background image (Idle state)
	 */
	void SetBackgroundColor(FColor Col) { Label->SetBackgroundColor(Col); };

	/** 
	 * Sets background colors for multi-color mode.
	 */
	void SetBackgroundColors(std::vector<FColor> InColors) { BackgroundColors.swap(InColors); }

	/**
	 * Sets label's font
	 */
	void SetFont(FontPtr InFont) { Label->SetFont(InFont); }

protected:
	/** Button label */
	std::shared_ptr<FTextLabelWidget> Label;

	/** Callback function, called when button is pressed */
	std::function<void()> OnPressedCallback;

	/** True if button is pressed */
	bool bPressed = false;

	/** Has several textures to animate button press */
	bool bAnimated = false;

	/** Was mouse hovering the button last frame? */
	bool bMouseWasHovered = false;

	std::vector<FColor> BackgroundColors;

	/** Color to use for button background when disabled */
	static constexpr FColor ButtonBackDisabledCol = FColor(0.15f, 0.15f, 0.15f, 1.0f);
};

namespace assets
{
	const std::vector<std::wstring> DefaultButtonAssets = { L"Assets/button.dds", L"Assets/button.dds" };
	const std::vector<std::wstring> LargeButtonAssets = { L"Assets/black_grey_button.dds", L"Assets/black_grey_button_pressed.dds" };

	const std::vector<FColor> DefaultButtonColors = { 
		FColor(0.018f, 0.48f, 0.933f, 1.0f), //Idle
		FColor(0.018f, 0.48f, 0.933f, 1.0f), //Pressed
		FColor(0.16f, 0.557f, 0.94f, 1.0f), //Hovered
		FColor(0.15f, 0.15f, 0.15f, 1.0f) }; //Disabled
}