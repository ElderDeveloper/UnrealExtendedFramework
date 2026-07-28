// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "Input/Reply.h"

/**
 * Slate panel for the Connection section of Project Settings > Extended Framework > Extended Atlassian.
 *
 * This exists because `UFUNCTION(CallInEditor)` buttons do not render or fire in the Project
 * Settings viewer in this engine build — confirmed dead in both this plugin and
 * UESteamPublishSettings. A details customization is the mechanism that actually works, and it also
 * allows a live status line, which a plain button row cannot express.
 *
 * Everything here binds through Slate attributes, so status updates repaint every frame instead of
 * waiting for the details panel to be invalidated.
 */
class FExtendedAtlassianSettingsCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** Class name used for registration, valid without touching the UObject system during shutdown. */
	static FName GetCustomizedClassName();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	FReply OnSaveAndTestClicked();
	FReply OnRefreshListsClicked();
	FReply OnClearClicked();
	FReply OnGetTokenClicked();
	FReply OnShowFileClicked();

	/** Live connection summary, re-evaluated every paint. */
	FText GetStatusText() const;
	FSlateColor GetStatusColor() const;
	FText GetLastMessageText() const;
	EVisibility GetBusyVisibility() const;
	bool IsReadyToTest() const;
};
