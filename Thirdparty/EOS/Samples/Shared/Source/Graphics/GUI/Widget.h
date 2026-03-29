// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GfxComponent.h"
#include "UIEvent.h"

class FDialog;

using UILayer = size_t;
const UILayer DefaultLayer = 100;

float UILayerToDepth(UILayer Layer);

/**
 * Base widget class (UI element)
 */
class IWidget : public IGfxComponent
{
public:
	/**
	 * Constructor
	 */
	IWidget(Vector2 position, Vector2 size, UILayer layer): Position(position), Size(size), Layer(layer) {}


	IWidget(const IWidget&) = delete;
	IWidget& operator=(const IWidget&) = delete;

	/**
	 * Destructor
	 */
	virtual ~IWidget() {}

	/**
	* UI Event Callback
	*/
	virtual void OnUIEvent(const FUIEvent& event) = 0;

	/**
	* Checks point is within this UI widget's bounds 
	*/
	virtual bool CheckCollision(Vector2 Pos) const
	{
		return (Pos.x >= Position.x && Pos.x <= (Position.x + Size.x) &&
				Pos.y >= Position.y && Pos.y <= (Position.y + Size.y));
	}

	/** Is mouse positioned over the widget? */
	bool IsMouseHovered() const;

	/** Get Position */
	virtual Vector2 GetPosition() const { return Position; }

	/** Get Size */
	virtual Vector2 GetSize() const { return Size; }

	/** Get Layer */
	virtual UILayer GetLayer() const { return Layer; }

	/** Set Position */
	virtual void SetPosition(Vector2 Pos) { Position = Pos; }

	/** Set Size */
	virtual void SetSize(Vector2 NewSize) { Size = NewSize; }

	/** Visibility */
	virtual void Hide() { bShown = false; }
	virtual void Show() { bShown = true; }
	virtual void Toggle() { bShown = !bShown; }
	virtual bool IsShown() const { return bShown; }
	
	/** Enabled */
	virtual void Enable() { bEnabled = true; }
	virtual void Disable() { bEnabled = false; }
	virtual bool IsEnabled() const { return bEnabled; }

	/** Focus */
	virtual bool IsFocused() const { return bFocused; }
	virtual void SetFocused(bool pNewFocused) { bFocused = pNewFocused; }

	virtual void SetParent(std::weak_ptr<FDialog> InParent);

	virtual void RenderBorders();

	void SetBorderColor(FColor InColor)
	{
		bBordersEnabled = true;
		BorderColor = InColor;
	}

	void ClearBorderColor()
	{
		bBordersEnabled = false;
	}

	virtual void Render(FSpriteBatchPtr& Batch) override;

#ifdef _DEBUG
	/** Debug Render */
	virtual void DebugRender() override;
#endif

protected:
	/** Position */
	Vector2 Position;

	/** Size */
	Vector2 Size;

	/** Depth */
	UILayer Layer;

	/** Optional - parent dialog */
	std::weak_ptr<FDialog> Parent;

	/** True if this widget is visible */
	bool bShown = true;

	/** True if this widget is enabled */
	bool bEnabled = true;

	/** True if this widget is focused */
	bool bFocused = false;

	/** Do we render border with vector graphics? */
	bool bBordersEnabled = false;

	/** Color of border */
	FColor BorderColor;
	//TODO: thickness as well?
};

using WidgetPtr = std::shared_ptr<IWidget>;