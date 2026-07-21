// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

/**
* Forward class declarations
*/
class FGame;
class FCommandLine;
class FStepTimer;

#ifdef EOS_DEMO_SDL
struct SDL_Window;
#endif //EOS_DEMO_SDL

/**
* Main class for Windows project
*/
class FMain
#ifdef DXTK
	: public IDeviceNotify
#endif // DXTK
{
public:
	/**
	* Constructor
	*/
	FMain() noexcept(false);

	/**
	* No copying or copy assignment allowed for this class.
	*/
	FMain(FMain const&) = delete;
	FMain& operator=(FMain const&) = delete;
		
	/**
	* Destructor
	*/
	virtual ~FMain();

	/**
	 * Load EOS and Initialize the platform
	 */
	void InitPlatform();

#ifdef DXTK
	/**
	* Initialization
	*/
	void Initialize(HWND Window, int Width, int Height);
#endif //DXTK

#ifdef EOS_DEMO_SDL
	/**
	* Initialization
	*/
	void Initialize(SDL_Window* Window, SDL_GLContext GLContext, int Width, int Height);

	/** Checks OpenGL error and returns true if an error has been found */
	bool GLError(const wchar_t *ContextStr);
#endif //EOS_DEMO_SDL

	/**
	* Main update loop
	*/
	void Tick();

#ifdef DXTK
	/**
	* IDeviceNotify methods
	*/
	virtual void OnDeviceLost() override;
	virtual void OnDeviceRestored() override;
#endif // DXTK

	/**
	* Called when main window has been activated
	*/
	void OnActivated();

	/**
	* Called when main window has been deactivated
	*/
	void OnDeactivated();

	/**
	* Called when main window is suspending
	*/
	void OnSuspending();

	/**
	* Called when main window is resuming
	*/
	void OnResuming();

	/**
	* Called when main window has moved
	*/
	void OnWindowMoved();

	/**
	* Called when main window size has changed
	* 
	* @param Width - Window width
	* @param Height - Window height
	*/
	void OnWindowSizeChanged(int Width, int Height);

	/** 
	* Called when application is about to shutdown
	*/
	void OnShutdown();

	/** 
	* Called in the process of shutdown. Shutdown is delayed until this function returns false.
	* This allows to finish and cleanup something before shutting down.
	*/
	bool IsShutdownDelayed() const;

	/**
	* Gets min window size
	*
	* @param Width - Window width
	* @param Height - Window height
	*/
	void GetMinimumSize(int& Width, int& Height) const;

	/**
	* Gets default window size
	*
	* @param Width - Window width
	* @param Height - Window height
	*/
	void GetDefaultSize(int& Width, int& Height) const;

	/**
	* Initializes command line
	*/
	void InitCommandLine();

	/**
	* Checks for fullscreen in command line arguments
	*
	* @return True if fullscreen is found in command line arguments
	*/
	bool HasFullScreenCommandLine();

	/** Accessor for timer */
	FStepTimer const& GetTimer();

#ifdef DXTK
	/** Accessor for device resources */
	std::unique_ptr<DeviceResources>const& GetDeviceResources();

	/** Accessor for world matrix */
	FMatrix const& GetWorld();

	/** Accessor for view matrix */
	FMatrix const& GetView();

	/** Accessor for projection matrix */
	FMatrix const& GetProjection();
#endif //DXTK

#ifdef EOS_DEMO_SDL
	SDL_Window* GetWindow() { return SDLWindow; }
	SDL_GLContext GetGLContext() { return GLContext; }
#endif // EOS_DEMO_SDL

	/** 
	* Utility function for printing a message to in-game console
	* 
	* @param Message - Message to print to console
	*/
	void PrintToConsole(const std::wstring& Message);

	/**
	* Utility function for printing a warning message to in-game console
	*
	* @param Message - Message to print to console
	*/
	void PrintWarningToConsole(const std::wstring& Message);

	/**
	* Utility function for printing an error message to in-game console
	*
	* @param Message - Message to print to console
	*/
	void PrintErrorToConsole(const std::wstring& Message);

	/** True if main window is currently in fullscreen mode */
	bool bIsFullScreen;

private:
	/** Main update loop */
	void Update();

	/** Main render for drawing all elements in window */
	void Render();

	/** Clears all resources */
	void Clear();

	/** Creates device-dependent resources */
	void CreateDeviceDependentResources();

	/** Creates resources that depend on window size */
	void CreateWindowSizeDependentResources();

	/** Game */
	std::unique_ptr<FGame> Game;

	/** Rendering loop timer */
	FStepTimer Timer;

#ifdef DXTK
	/** Device resources */
	std::unique_ptr<DeviceResources> DXDeviceResources;

	/** World matrix */
	FMatrix World;

	/** View matrix */
	FMatrix View;

	/** Projection matrix */
	FMatrix Projection;
#endif //DXTK

#ifdef EOS_DEMO_SDL
	SDL_Window* SDLWindow = nullptr;
	SDL_GLContext GLContext = nullptr;
#endif
};

/** Global accessor for main */
extern std::unique_ptr<FMain> Main;
