// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Forward class declarations
 */
class FGameEvent;
class FInput;
class FConsole;
class FBaseMenu;
class FBaseLevel;
class FAuthentication;
class FFriends;
class FEosUI;
class FMetrics;
class FUsers;
class FPlayerManager;
class FVectorRender;
class FTextureManager;

/**
 * Main game class
 */
class FBaseGame
{
public:
	/**
	 * Constructor
	 */
	FBaseGame() noexcept(false);

	/**
	 * No copying or copy assignment allowed for this class.
	 */
	FBaseGame(FBaseGame const&) = delete;
	FBaseGame& operator=(FBaseGame const&) = delete;

	/**
	 * Destructor
	 */
	virtual ~FBaseGame();

	/**
	 * Initialization
	 */
	virtual void Init();

	/**
	 * Exit Game
	 */
	void Exit();

	/**
	 * Main update game loop
	 */
	virtual void Update();

	/**
	 * Render pass-through
	 */
	virtual void Render();

	/**
	 * Create Resources
	 */
	virtual void Create();

	/**
	 * Game event dispatcher
	 *
	 * @param Event - Game event to be dispatched
	 */
	virtual void OnGameEvent(const FGameEvent& Event);

	/**
	 * Called just before shutting down the game. Allows to finish current operations.
	 */
	virtual void OnShutdown();

	/**
	 * Called in the process of shutdown. Shutdown is delayed until this function returns false.
	 * This allows to finish and cleanup something before shutting down.
	 */
	virtual bool IsShutdownDelayed();

	/**
	 * Updates layout of UI elements
	 */
	virtual void UpdateLayout(int Width, int Height);

	/**
	 * Singleton's getter
	 */
	static FBaseGame& GetBase();

	/**
	 * Accessor for Authentication
	 */
	const std::shared_ptr<FAuthentication>& GetAuthentication();

	/**
	 * Accessor for Friends
	 */
	const std::unique_ptr<FFriends>& GetFriends();

	/**
	 * Accessor for EosUI
	 */
	const std::unique_ptr<FEosUI>& GetEosUI();

	/**
	 * Accessor for User
	 */
	const std::unique_ptr<FUsers>& GetUsers();

	/**
	 * Accessor for Input
	 */
	const std::unique_ptr<FInput>& GetInput();

	/**
	 * Accessor for Console
	 */
	const std::shared_ptr<FConsole>& GetConsole();

	/**
	 * Accessor for Menu
	 */
	const std::shared_ptr<FBaseMenu>& GetMenu();

	/**
	 * Accessor for Texture Manager
	 */
	const std::unique_ptr<FTextureManager>& GetTextureManager();

	/**
	 * Accessor for Vector Render
	 */
	const std::unique_ptr<FVectorRender>& GetVectorRender();

	/**
	 * Utility for printing a message to the in-game console
	 *
	 * @param Msg - A string representing the message to print out
	 */
	void PrintToConsole(const std::wstring& Msg);

	/**
	 * Utility for printing a warning message to the in-game console
	 *
	 * @param Msg - A string representing the message to print out
	 */
	void PrintWarningToConsole(const std::wstring& Msg);

	/**
	 * Utility for printing an error message to the in-game console
	 *
	 * @param Msg - A string representing the message to print out
	 */
	void PrintErrorToConsole(const std::wstring& Msg);

protected:
	/**
	 * Creates all console commands
	 */
	virtual void CreateConsoleCommands();

	/**
	  * Appends extra help message lines (to be used in 'help' console command)
	  */
	void AppendHelpMessageLines(const std::vector<const wchar_t*>& MoreLines);

	/**
	 * Release components
	 */
	virtual void Release();

	/** Authentication component */
	std::shared_ptr<FAuthentication> Authentication;

	/** Friends component */
	std::unique_ptr<FFriends> Friends;

	/** EosUI component */
	std::unique_ptr<FEosUI> EosUI;

	/** Metrics component */
	std::unique_ptr<FMetrics> Metrics;

	/** Users component */
	std::unique_ptr<FUsers> Users;

	/** Input component */
	std::unique_ptr<FInput> Input;

	/** Console graphical component */
	std::shared_ptr<FConsole> Console;

	/** Texture manager with caching */
	std::unique_ptr<FTextureManager> TextureManager;

	/** Menu graphical component */
	std::shared_ptr<FBaseMenu> Menu;

	/** Level graphical component */
	std::unique_ptr<FBaseLevel> Level;

	/** Player Manager component */
	std::unique_ptr<FPlayerManager> PlayerManager;

	/** Vector Render component for rendering vector graphics */
	std::unique_ptr<FVectorRender> VectorRender;

	/** Private implementation */
	class Impl;

	/** Implementation */
	std::unique_ptr<Impl> TheImpl;
};

/** Accessor for Console */
inline const std::shared_ptr<FConsole>& GetConsole() { return FBaseGame::GetBase().GetConsole(); }