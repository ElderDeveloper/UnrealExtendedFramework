// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Dialog.h"
#include "Font.h"
#include "Authentication.h"

/**
 * Forward declarations
 */
class FUIEvent;
class FGameEvent;
enum class ELoginMode;

struct FLoginMethodButtonData
{
	FLoginMethodButtonData(std::wstring&& InLabel, ELoginMode InLoginMode) :
		Label(InLabel),
		LoginMode(InLoginMode)
	{
	}

	std::wstring Label;
	ELoginMode LoginMode;
};

/**
 * Exit dialog
 */
class FAuthDialogs : public IGfxComponent
{
public:
	/**
	 * Constructor
	 */
	FAuthDialogs(DialogPtr Parent,
		std::wstring DialogLoginText,
		FontPtr DialogBoldSmallFont,
		FontPtr DialogSmallFont,
		FontPtr DialogTinyFont);

	/**
	 * Destructor
	 */
	virtual ~FAuthDialogs() {};

	/**
	* IGfxComponent Overrides
	*/
	virtual void Update() override;
	virtual void Render(FSpriteBatchPtr& Batch) override;
#ifdef _DEBUG
	virtual void DebugRender() override;
#endif
	virtual void Create() override;
	virtual void Release() override;

	void Init();

	void SetPosition(Vector2 Pos);

	void SetSize(Vector2 NewSize);

	/**
	* UI Event Callback
	*
	* @param Event - UI event
	*/
	void OnUIEvent(const FUIEvent& Event);

	void UpdateLayout();

	/**
	* Receives game event
	*
	* @param Event - Game event to act on
	*/
	void OnGameEvent(const FGameEvent& Event);

	/**
	 * Sets user info to testing state
	 */
	void SetTestUserInfo();

	/** 
	 * Sets and offset for user logged in label
	*/
	void SetUserLabelOffset(Vector2 Offset) { UserLabelOffset = Offset;	}

	/** 
	 * Switches multi-user/single user mode.
	 */
	void SetSingleUserOnly(bool bValue);

private:
	/**
	 * Creates the widgets for the login with id and password dialog
	 */
	void CreateLoginIDPasswordDialog();

	/**
	 * Creates the widgets for the login with exchange code dialog
	 */
	void CreateLoginExchangeCodeDialog();

	/**
	 * Creates the widgets for the login with dev auth tool credentials dialog
	 */
	void CreateLoginDevAuthDialog();

	/**
	 * Creates the widgets for the login with account portal dialog
	 */
	void CreateLoginAccountPortalDialog();

	/**
	 * Creates the button widgets to allow change of login mode
	 */
	void CreateLoginMethodWidgets(DialogPtr LoginDialog, ELoginMode LoginMode, float PosX);

	/**
	 * Creates the widgets for the user logged in dialog
	 */
	void CreateUserLoggedIn();

	/**
	 * Creates the widgets for the MFA login dialog
	 */
	void CreateMFALoginDialog();

	/**
	 * Schedules new login mode. Actual operation will be performed in DoChangeLoginMode on next update.
	 *
	 * @param LoginMode - Login mode to change to
	 */
	void ChangeLoginMode(ELoginMode LoginMode);

	/**
	 * Sets the login mode when demo starts
	 */
	void SetStartLoginMode();

	/**
	 * Sets new login mode.
	 *
	 * @param LoginMode - Login mode to change to
	 */
	void DoChangeLoginMode(ELoginMode LoginMode);

	/**
	 * Updates state of the login buttons based on whether you are able to log in
	 */
	void UpdateLoginButtonsState();

	/**
	 * Updates state of a dialog's login button based on whether you are able to log in
	 *
	 * @param AuthButton - Button to update
	 */
	void UpdateLoginButtonState(WidgetPtr AuthButton);

	/**
	* Enables / disables a dialog's login buttons
	*
	* @param Enable - true to enable, false to disable
	*/
	void EnableLoginButtons(bool bEnable);

	/**
	* Enables / disables a dialog's login method buttons
	*
	* @param Enable - true to enable, false to disable
	*/
	void EnableLoginMethodButtons(bool bEnable);

	/**
	* Enables / disables a button
	*
	* @param AuthButton - Button to update
	* @param Enable - True to enable, False to disable
	*/
	void EnableButton(WidgetPtr AuthButton, bool bEnable);

	/**
	 * Update user button state
	 */
	void UpdateUserButtons();

	/**
	 * Updates the login password dialog position and size based on friend dialog
	 */
	void UpdateLoginIDPasswordDialog();

	/**
	 * Updates the login exchange code dialog position and size based on friend dialog
	 */
	void UpdateLoginExchangeCodeDialog();

	/**
	 * Updates the login dev auth dialog position and size based on friend dialog
	 */
	void UpdateLoginDevAuthDialog();

	/**
	 * Updates the login account portal dialog position and size based on friend dialog
	 */
	void UpdateLoginAccountPortalDialog();

	/**
	 * Updated login method buttons
	 */
	void UpdateLoginMethodButtons(DialogPtr LoginDialog);

	/**
	 * Updates the MFA login dialog position and size based on friend dialog
	 */
	void UpdateMFALoginDialog();

	/**
	 * Updates the user logged in dialog position and size based on friend dialog
	 */
	void UpdateUserLoggedIn();

	/**
	* Hides login dialogs
	*/
	void HideLoginDialogs();

	/**
	* Updates current player info
	*/
	void UpdateCurrentPlayer();

	/**
	* Start a new login
	*/
	void NewLogin();

	/**
	 * Updates user info label text based on logged in user
	 */
	void UpdateUserInfo();

	/**
	 * Resets user info label text
	 */
	void ResetUserInfo();

	/**
	 * Sets user info label text to pending state
	 */
	void SetUserInfoPending();

	/** Hide Dialog */
	void HideDialog(DialogPtr Dialog);

	/** Show Dialog */
	void ShowDialog(DialogPtr Dialog);

	/**
	* Adds the auth dialog to the collection of dialogs
	*
	* @param - Dialog to add
	*/
	void AddDialog(DialogPtr Dialog);

	/**
	 * Check for any dialog ready for input
	 *
	 * @return True if any Dialog is ready for input
	 */
	bool IsDialogReadyForInput();

	/** Auth dialogs */
	std::vector<DialogPtr> Dialogs;

	/** Parent Dialog */
	DialogPtr ParentDialog;

	/** Login Dialog for ID and Password login */
	DialogPtr LoginIDPasswordDialog;

	/** Login Dialog for Exchange Code login */
	DialogPtr LoginExchangeCodeDialog;

	/** Login Dialog for Dev Auth login */
	DialogPtr LoginDevAuthDialog;

	/** Login Dialog for Account Portal login */
	DialogPtr LoginAccountPortalDialog;

	/** User Logged In Dialog */
	DialogPtr UserLoggedInDialog;

	/** MFA Login Dialog */
	DialogPtr MFALoginDialog;

	/** Label with text showing info for user logged in */
	std::shared_ptr<FTextLabelWidget> UserLoggedInLabel;

	/** Additional offset that's applied to user label. */
	Vector2 UserLabelOffset;

	/** Bold Small Font */
	FontPtr BoldSmallFont;

	/** Small Font */
	FontPtr SmallFont;

	/** Tiny Font */
	FontPtr TinyFont;

	/** Login mode used last time a user tried to login */
	ELoginMode SavedLoginMode;

	/** Login mode user want to switch to */
	ELoginMode NewLoginMode;

	/** Time in seconds when a dialog was last shown */
	float ShowDialogTime = 0;

	/** Log In Text */
	std::wstring LoginText;

	/** Shows when user logout was triggered */
	double LogoutTriggeredTimestamp = 0;

	/** Are we in the process of logging user out? */
	bool bLoggingOut = false;

	/** Have we logged a user in? */
	bool bHasLoggedIn = false;

	/** When true multi-user support is disabled */
	bool bIsSingleUserOnly = false;

	/** List of currently available login methods */
	std::vector<FLoginMethodButtonData> LoginMethods;

	/** Color to use for Auth button background */
	static constexpr FColor AuthButtonBackCol = FColor(0.f, 0.47f, 0.95f, 1.f);

	/** Color to use for Auth button background when disabled */
	static constexpr FColor AuthButtonBackDisabledCol = FColor(0.5f, 0.5f, 0.5f, 0.6f);
};
