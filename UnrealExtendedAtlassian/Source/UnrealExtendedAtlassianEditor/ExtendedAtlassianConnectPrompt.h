// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Shown by both tabs in place of their content when the plugin has no site URL or credentials.
 *
 * A first run is not an error, so this reads as setup instructions rather than a failure message,
 * and puts the two things the user actually needs — the settings page and Atlassian's token page —
 * one click away.
 */
class SExtendedAtlassianConnectPrompt : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SExtendedAtlassianConnectPrompt) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** True when a site URL and credentials are both present, so the tabs can show real content. */
	static bool IsConnected();
};
