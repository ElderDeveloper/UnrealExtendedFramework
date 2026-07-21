// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widget.h"
#include "GfxComponent.h"
#include "Dialog.h"

/**
* Forward declarations
*/
class FUIEvent;
class FConsole;
class FGameEvent;
class FFont;
class FConsoleDialog;
class FFriendsDialog;
class FExitDialog;
class FAuthDialogs;
class FSpriteWidget;
class FTextLabelWidget;
class FPopupDialog;

/**
* In-Game Menu
*/
class FBaseMenu : public IGfxComponent
{
public:
	/**
	* Constructor
	*/
	FBaseMenu(std::weak_ptr<FConsole> console) noexcept(false);

	/**
	* No copying or copy assignment allowed for this class.
	*/
	FBaseMenu(FBaseMenu const&) = delete;
	FBaseMenu& operator=(FBaseMenu const&) = delete;

	/**
	* Destructor
	*/
	virtual ~FBaseMenu() {};

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

	/**
	 * Initializes fonts
	 */
	virtual void CreateFonts();

	/**
	* Initialization
	*/
	virtual void Init();

	/**
	* Updates layout of elements
	*/
	virtual void UpdateLayout(int Width, int Height);

	/**
	* Event Callback
	*
	* @param Event - UI event
	*/
	virtual void OnUIEvent(const FUIEvent& Event);

	/**
	* Add the dialog to the list of dialogs
	*
	* @param - Dialog to add
	*/
	void AddDialog(DialogPtr Dialog);

	/**
	 * Shows Dialog
	 *
	 * @Param Dialog - Dialog to show
	 */
	void ShowDialog(DialogPtr Dialog);

	/**
	 * Hide Dialog
	 *
	 * @Param Dialog - Dialog to hide
	 */
	void HideDialog(DialogPtr Dialog);

	/**
	* Receives game event
	*
	* @param Event - Game event to act on
	*/
	virtual void OnGameEvent(const FGameEvent& Event);

	/**
	 * Sets user info to testing state
	 */
	void SetTestUserInfo();

protected:
	/**
	 * Creates the in-game console dialog
	 */
	virtual void CreateConsoleDialog();

	/**
	 * Creates the friends dialog
	 */
	virtual void CreateFriendsDialog();

	/**
	 * Creates the auth dialogs
	 */
	virtual void CreateAuthDialogs();

	/**
	 * Creates the exit dialog
	 */
	void CreateExitDialog();

	/**
	 * Create popup dialog
	 */
	void CreatePopupDialog();

	/**
	 * Updates the fps label text based on current frame rate
	 */
	void UpdateFPS();

	/**
	* Toggle rendering of fps label
	*/
	void ToggleFPS();

	/**
	* Updates friends info
	*/
	virtual void UpdateFriends();

	/**
	 * Check for any dialog ready for input
	 *
	 * @return True if any Dialog is ready for input
	 */
	bool IsDialogReadyForInput();

	/** Dialogs */
	std::vector<DialogPtr> Dialogs;

	/** Console Dialog */
	std::shared_ptr<FConsoleDialog> ConsoleDialog;

	/** Friends Dialog */
	std::shared_ptr<FFriendsDialog> FriendsDialog;

	/** Auth Dialogs */
	std::shared_ptr<FAuthDialogs> AuthDialogs;

	/** Exit Dialog */
	std::shared_ptr<FExitDialog> ExitDialog;

	/** Generic popup dialog */
	std::shared_ptr<FPopupDialog> PopupDialog;

	/** Background Image */
	std::shared_ptr<FSpriteWidget> BackgroundImage;

	/** Title Label */
	std::shared_ptr<FTextLabelWidget> TitleLabel;

	/** FPS Label */
	std::shared_ptr<FTextLabelWidget> FPSLabel;

	/** Large Font */
	std::shared_ptr<FFont> LargeFont;

	/** Normal Font */
	std::shared_ptr<FFont> NormalFont;

	/** Small Font */
	std::shared_ptr<FFont> SmallFont;

	/** Tiny Font */
	std::shared_ptr<FFont> TinyFont;

	/** Bold Large Font */
	std::shared_ptr<FFont> BoldLargeFont;

	/** Bold Normal Font */
	std::shared_ptr<FFont> BoldNormalFont;

	/** Bold Small Font */
	std::shared_ptr<FFont> BoldSmallFont;

	/** Console */
	std::weak_ptr<FConsole> Console;

	/** Keys that are being held by user at the moment. Used to generate repeated key press events. */
	FInput::Keys KeyCurrentlyHeld = FInput::None;

	/** How many frames it's held */
	float KeyCurrentlyHeldSeconds = 0;

	/** How many seconds key needs to be held to generate another key press event. */
	static constexpr float SecondsTilKeyRepeat = 0.3f;

	/** Time in seconds when a dialog was last shown */
	float ShowDialogTime = 0;

	/** True to show FPS Label */
	bool bShowFPS;
};
