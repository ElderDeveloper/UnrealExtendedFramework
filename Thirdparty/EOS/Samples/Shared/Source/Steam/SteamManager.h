// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#ifdef EOS_STEAM_ENABLED

/**
* Manages Steam API
*/
class FSteamManager
{
	/**
	 * Constructor
	 */
	FSteamManager();

	/**
	 * No copying or copy assignment allowed for this class.
	 */
	FSteamManager(FSteamManager const&) = delete;
	FSteamManager& operator=(FSteamManager const&) = delete;

public:
	/**
	 * Destructor
	 */
	virtual ~FSteamManager();

	static FSteamManager& GetInstance();
	static void ClearInstance();

	/** Initializes Steam and starts auth-login */
	void Init();

	/** Updates Steam Callbacks */
	void Update();

	/**
	 * Request an auth session ticket
	 */
	void RetrieveAuthSessionTicket();

	/**
	 * Receives game event
	 *
	 * @param Event - Game event to act on
	 */
	void OnGameEvent(const FGameEvent& Event);

	/** Starts login process with pre-requested Steam auth session ticket */
	void StartLogin();

private:
	static std::unique_ptr<FSteamManager> Instance;

	// Private implementation:
	class FImpl;
	std::unique_ptr<FImpl> Impl;
};
#endif // EOS_STEAM_ENABLED