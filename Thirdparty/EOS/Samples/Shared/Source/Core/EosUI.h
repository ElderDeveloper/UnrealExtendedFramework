// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "eos_sdk.h"
#include "AccountHelpers.h"

/**
* Forward declarations
*/
class FGameEvent;

/**
* Manages UI methods for local user
*/
class FEosUI
{
public:
	/**
	* Constructor
	*/
	FEosUI() noexcept(false);

	/**
	* No copying or copy assignment allowed for this class.
	*/
	FEosUI(FEosUI const&) = delete;
	FEosUI& operator=(FEosUI const&) = delete;

	/**
	* Destructor
	*/
	virtual ~FEosUI();

	static std::wstring LocationToString(EOS_UI_ENotificationLocation Location);


	/**
	 * Registers for the notification of display updates.
	 */
	void Init();

	/**
	 * Unregister for the notification of display updates.
	 */
	void OnShutdown();

	/**
	* Show the friends overlay.
	*/
	void ShowFriendsOverlay();

	/**
	 * Change the toggle friends key.
	 */
	void SetToggleFriendsKey(const std::vector<std::wstring>& Keys);

	/**
	 * Change the notification display preference.
	 */
	void SetDisplayPreference(EOS_UI_ENotificationLocation Location);

#if defined(DEV_BUILD) && (defined(_DEBUG) || defined(_TEST))
	/**
	* Run the pact tests.
	*/
	void RunPactTests();

	/**
	 * Load a custom overlay URL.
	 */
	void LoadURLCustom(const char* URL);
#endif

	void OnGameEvent(const FGameEvent& Event);

	/**
	 * Open the Overlay to block a player
	 */
	void ShowBlockPlayer(const FEpicAccountId TargetPlayer);

	/**
	 * Open the Overlay to report a player
	 */
	void ShowReportPlayer(const FEpicAccountId TargetPlayer);
private:
	/**
	* Callback that is fired when the show friends overlay async operation completes, either successfully or in error
	*
	* @param Data - Output parameters for the EOS_UI_ShowFriends Function
	*/
	static void EOS_CALL ShowFriendsCallbackFn(const EOS_UI_ShowFriendsCallbackInfo* UiData);
	
	/**
	* Callback that is fired when the show "block player" overlay UI async operation completes, either successfully or in error
	*
	* @param Data - Output parameters for the EOS_UI_ShowBlockPlayer Function
	*/
	static void EOS_CALL ShowBlockPlayerCallbackFn(const EOS_UI_OnShowBlockPlayerCallbackInfo* Data);

	/**
	* Callback that is fired when the show "report player" overlay UI async operation completes, either successfully or in error
	*
	* @param Data - Output parameters for the EOS_UI_ShowReportPlayer Function
	*/
	static void EOS_CALL ShowReportPlayerCallbackFn(const EOS_UI_OnShowReportPlayerCallbackInfo* Data);

	/**
	* callback that is fired when the notify for display settings update occurs.
	*
	* @param UpdatedData - Current state provided by the notify.
	*/
	static void EOS_CALL OnDisplaySettingsUpdated(const EOS_UI_OnDisplaySettingsUpdatedCallbackInfo* UpdatedData);

	/**
	* Handle for the notification for display settings updates.
	*/
	EOS_NotificationId DisplayUpdateNotificationId = EOS_INVALID_NOTIFICATIONID;
};
