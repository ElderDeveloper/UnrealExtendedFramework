// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AccountHelpers.h"

/**
* Forward declarations
*/
class FGameEvent;

/**
* Player class
*/
class FPlayer
{
public:
	/**
	* Constructor
	*/
	FPlayer(const FEpicAccountId PlayerId) noexcept(false);

	/**
	* No copying or copy assignment allowed for this class.
	*/
	FPlayer(FPlayer const&) = delete;
	FPlayer& operator=(FPlayer const&) = delete;

	/**
	* Destructor
	*/
	virtual ~FPlayer();

	/**
	* Accessor for user id
	*
	* @return user account id
	*/
	FEpicAccountId const GetUserID() { return UserId; };

	/**
	* Sets Account Id for user
	*
	* @param AccountId - Account Id for user
	*/
	void SetUserID(const FEpicAccountId AccountId) { UserId = AccountId; };

	/**
	* Accessor for product user id
	*
	* @return product user id
	*/
	FProductUserId const GetProductUserID() { return ProductUserId; };

	/**
	* Sets Account Id for user
	*
	* @param productUserId - Account Id for user
	*/
	void SetProductUserID(const FProductUserId productUserId) { ProductUserId = productUserId; };

	/**
	* Accessor for user display name
	*
	* @return user display name
	*/
	std::wstring const GetDisplayName() { return UserDisplayName; };

	/**
	* Sets display name for this player
	*
	* @param DisplayName - Display name for this player
	*/
	void SetDisplayName(const std::wstring& DisplayName) { UserDisplayName = DisplayName; };

	/**
	* Accessor for user locale
	*
	* @return user locale
	*/
	std::string const GetLocale() { return UserLocale; };

	/**
	* Sets locale for this player
	*
	* @param Locale - Locale for this player
	*/
	void SetLocale(const std::string& Locale) { UserLocale = Locale; };


private:
	/** Account Id for user */
	FEpicAccountId UserId;

	/** Product User Id for user */
	FProductUserId ProductUserId;

	/** Display name for user */
	std::wstring UserDisplayName;

	/** Locale for user */
	std::string UserLocale;
};

using PlayerPtr = std::shared_ptr<FPlayer>;

/**
* Player Manager
*/
class FPlayerManager
{
public:
	/**
	* Constructor
	*/
	FPlayerManager() noexcept(false);

	/**
	* No copying or copy assignment allowed for this class.
	*/
	FPlayerManager(FPlayerManager const&) = delete;
	FPlayerManager& operator=(FPlayerManager const&) = delete;

	/**
	* Destructor
	*/
	virtual ~FPlayerManager();

	/**
	* Adds a new player
	*
	* @param UserId - User account id for the player to be added
	*/
	void Add(FEpicAccountId UserId);

	/**
	* Removes a player
	*
	* @param UserId - User account id for the player to be removed
	*/
	void Remove(FEpicAccountId UserId);

	/**
	 * Gets the number of players in the game
	 */
	size_t GetNumPlayers() { return PlayerIds.size(); };

	/**
	 * Gets a pointer to a player with matching user id
	 *
	 * @param UserId - User id for player
	 *
	 * @return Pointer to player if found, NULL otherwise
	 */
	PlayerPtr GetPlayer(FEpicAccountId UserId);

	/**
	 * Gets a pointer to a player with matching user id
	 *
	 * @param UserId - User id for player
	 *
	 * @return Pointer to player if found, NULL otherwise
	 */
	PlayerPtr GetPlayer(FProductUserId UserId);

	/**
	 * Gets a pointer to a player with matching user display name
	 *
	 * @param DisplayName - Display name for player
	 *
	 * @return Pointer to player if found, NULL otherwise
	 */
	PlayerPtr GetPlayer(const std::wstring DisplayName);

	/**
	 * Gets index for player
	 *
	 * @param UserId - User id for player
	 *
	 * @return Index for player, starts at 1, zero means not found
	 */
	int GetPlayerIndex(FEpicAccountId UserId);

	/**
	 * Gets a account id for previous player
	 *
	 * @param UserId - User id for player
	 *
	 * @return Pointer to player if found, NULL otherwise
	 */
	FEpicAccountId GetPrevPlayerId(FEpicAccountId UserId);

	/**
	 * Gets a account id for next player
	 *
	 * @param UserId - User id for player
	 *
	 * @return Pointer to player if found, NULL otherwise
	 */
	FEpicAccountId GetNextPlayerId(FEpicAccountId UserId);

	/**
	* Sets a player's display name for a player with matching user id
	*
	* @param UserId - User id for player
	* @param DisplayName - Display name to set
	*/
	void SetDisplayName(FEpicAccountId UserId, const std::wstring DisplayName);

	/**
	* Gets a player's display name for a player with matching user id
	*
	* @param UserId - User id for player
	*
	* @return A string representing player's display name if found, empty if not found
	*/
	std::wstring GetDisplayName(FEpicAccountId UserId);

	/**
	 * Sets the current user id
	 *
	 * @param UserId - Id of current 'primary' user
	 */
	void SetCurrentUser(FEpicAccountId UserId) { CurrentUserId = UserId; };

	/**
	 * Accessor for current user id
	 *
	 * @return User id for current user
	 */
	FEpicAccountId GetCurrentUser() { return CurrentUserId; };

	/**
	 * Sets the active locale then updates all players.
	 *
	 * @param NewLocale - New locale code.
	 */
	void SetActiveLocale(const std::wstring& NewLocale);

	/**
	 * Sets the product user id for a user
	 */
	void SetProductUserID(FEpicAccountId UserId, FProductUserId ProductUserId);

	/**
	 * Singleton
	 */
	static FPlayerManager& __cdecl Get();

	/**
	 * Receives game event
	 *
	 * @param Event - Game event to act on
	 */
	void OnGameEvent(const FGameEvent& Event);

private:
	/**
	 * Sets the locale for a user
	 */
	void SetUserLocale(FEpicAccountId UserId);

	/**
	 * Sets the rich text for presence
	 */
	void SetPresenceRichText(FEpicAccountId UserId, const std::string RichText);

	/** Sets presence rich text for the local user */
	void SetPresenceRichText(const std::wstring& RichText);

	/** Sets initial presence for the local user */
	void SetInitialPresence();

	static void EOS_CALL SetPresenceCallbackFn(const EOS_Presence_SetPresenceCallbackInfo* Data);

	/** Collection of player account ids in game */
	std::vector<EOS_EpicAccountId> PlayerIds;

	/** Collection of players in game */
	std::map<EOS_EpicAccountId, PlayerPtr> Players;

	/** Collection of players in game */
	std::map<EOS_ProductUserId, PlayerPtr> ConnectPlayers;

	/** Current 'primary' user. */
	EOS_EpicAccountId CurrentUserId;

	/** Private implementation */
	class Impl;

	/** Implementation */
	std::unique_ptr<Impl> TheImpl;
};
