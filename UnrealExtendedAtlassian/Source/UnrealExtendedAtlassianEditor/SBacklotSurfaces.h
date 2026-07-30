// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Route-level widget boundaries for the Backlot workspace.
 *
 * Surface builders still share the root controller and primitive library, but each route is
 * mounted beneath its own Slate widget. This keeps accessibility, clipping, invalidation and
 * future surface-specific refactors from accumulating on SExtendedAtlassianWorkspace itself.
 */
class SBacklotSurfaceBase : public SCompoundWidget
{
public:
	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

protected:
	void ConstructSurface(const TSharedRef<SWidget>& Content);
};

#define DECLARE_BACKLOT_SURFACE(SurfaceClass) \
	class SurfaceClass final : public SBacklotSurfaceBase \
	{ \
	public: \
		SLATE_BEGIN_ARGS(SurfaceClass) {} \
			SLATE_DEFAULT_SLOT(FArguments, Content) \
		SLATE_END_ARGS() \
		void Construct(const FArguments& InArgs); \
	};

DECLARE_BACKLOT_SURFACE(SBacklotDocsSurface)
DECLARE_BACKLOT_SURFACE(SBacklotIssuesSurface)
DECLARE_BACKLOT_SURFACE(SBacklotIssueDetailSurface)
DECLARE_BACKLOT_SURFACE(SBacklotBoardSurface)
DECLARE_BACKLOT_SURFACE(SBacklotPinsSurface)
DECLARE_BACKLOT_SURFACE(SBacklotInboxSurface)

#undef DECLARE_BACKLOT_SURFACE
