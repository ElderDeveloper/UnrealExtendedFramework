// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

struct CommandLineConstants
{
	/** Platform Product ID (Format: String - e.g. "-productid 2318273821731232") */
	static constexpr wchar_t ProductId[] = L"productid";

	/** Platform Sandbox ID (Format: String - e.g. "-sandboxid 343438494872348974") */
	static constexpr wchar_t SandboxId[] = L"sandboxid";

	/** Platform Deployment ID (Format: String - e.g. "-deploymentid 89234923874923784") */
	static constexpr wchar_t DeploymentId[] = L"deploymentid";

	/** Platform Client ID (Format: String - e.g. "-clientid 3873284732847834") */
	static constexpr wchar_t ClientId[] = L"clientid";

	/** Platform Client Secret (Format: String - e.g. "-clientsecret 38732847328478343403430494034") */
	static constexpr wchar_t ClientSecret[] = L"clientsecret";

	/** Dev Auth Tool Host (Format: String - "ip:port", e.g. "-devhost localhost:1234") */
	static constexpr wchar_t DevAuthHost[] = L"devhost";

	/** Dev Auth Tool Credentials (Format: String - "name", e.g. "-devcred MyAccount") */
	static constexpr wchar_t DevAuthCred[] = L"devcred";

	/** Auto Login (Format: Flag - e.g. "-autologin") */
	static constexpr wchar_t AutoLogin[] = L"autologin";

	/** Fullscreen (Format: Flag - "e.g. "-fullscreen") */
	static constexpr wchar_t Fullscreen[] = L"fullscreen";

	/** Exchange Code (Format: String - e.g. "-exchangecode 123456789") */
	static constexpr wchar_t ExchangeCode[] = L"exchangecode";

	/** Debug Log File (Format: String - e.g. "-logfile MyDebugOutput.log") */
	static constexpr wchar_t DebugLogFile[] = L"logfile";

	/** Settings File (Format: String - e.g. "-settingsfile MySettings.cfg") */
	static constexpr wchar_t SettingsFile[] = L"settingsfile";

	/** EpicPortal will be set if game is being launched from Launcher (Format: Flag - e.g. "-EpicPortal") */
	static constexpr wchar_t EpicPortal[] = L"EpicPortal";

	/** Auth type set from launcher params (Format: String - e.g. "-AUTH_TYPE=exchangecode") */
	static constexpr wchar_t LauncherAuthType[] = L"AUTH_TYPE";

	/** Auth login set from launcher params (Format: String - e.g. "-AUTH_LOGIN=name") */
	static constexpr wchar_t LauncherAuthLogin[] = L"AUTH_LOGIN";

	/** Auth password set from launcher params (Format: String - e.g. "-AUTH_PASSWORD=123456789") */
	static constexpr wchar_t LauncherAuthPassword[] = L"AUTH_PASSWORD";

	/** Scopes EOS_AS_NoFlags permission will be added when user logs in */
	static constexpr wchar_t ScopesNoFlags[] = L"scopesnoflags";

	/** Scopes EOS_AS_BasicProfile permission will be added when user logs in */
	static constexpr wchar_t ScopesBasicProfile[] = L"scopesbasicprofile";

	/** Scopes EOS_AS_FriendsList permission will be added when user logs in */
	static constexpr wchar_t ScopesFriendsList[] = L"scopesfriendslist";

	/** Scopes EOS_AS_Presence permission will be added when user logs in */
	static constexpr wchar_t ScopesPresence[] = L"scopespresence";

	/** Scopes EOS_AS_FriendsManagement permission will be added when user logs in */
	static constexpr wchar_t ScopesFriendsManagement[] = L"scopesfriendsmanagement";

	/** Scopes EOS_AS_Email permission will be added when user logs in */
	static constexpr wchar_t ScopesEmail[] = L"scopesemail";

	/** Scopes EOS_AS_Country permission will be added when user logs in */
	static constexpr wchar_t ScopesCountry[] = L"scopescountry";

	/** Assets will be loaded from this directory */
	static constexpr wchar_t AssetDir[] = L"assetdir";

	/** Running as a Dedicated Server */
	static constexpr wchar_t Server[] = L"server";

	/** URL to be used for server */
	static constexpr wchar_t ServerURL[] = L"serverurl";

	/** Port to be used for server */
	static constexpr wchar_t ServerPort[] = L"serverport";

	/** Override locale to be used for EOS_Platform_Create */
	static constexpr wchar_t Locale[] = L"locale";

	/** Inventory name to be used for the server */
	static constexpr wchar_t ServerInventoryName[] = L"serverinventoryname";

#ifdef DEV_BUILD
	/** Username (Format: String - e.g. "userid myusername") */
	static constexpr wchar_t DevUsername[] = L"userid";

	/** Password (Format: String - e.g. "password mypassword") */
	static constexpr wchar_t DevPassword[] = L"password";
#endif
};

/**
* Command Line class
*/
class FCommandLine
{
public:
	/**
	* No copying or copy assignment allowed for this class.
	*/
	FCommandLine(FCommandLine const&) = delete;
	FCommandLine& operator=(FCommandLine const&) = delete;

	/**
	* Initialization using a string containing all params
	*/
	void Init(LPWSTR lpCmdLine);

	/**
	* Initialization using a collection of params
	*/
	void Init(const std::vector<std::wstring>& CmdLineParams);

	/**
	* Checks command line arguments for a matching param
	*
	* @param Param - Parameter to look for in command line arguments
	*
	* @return True if param is found in command line arguments
	*/
	bool HasParam(const std::wstring& Param);

	/**
	* Retrieves value associated with given command line parameter
	*
	* @param Param - Parameter to look for in command line arguments
	*
	* @return Value associated with command line parameter
	*/
	std::wstring GetParamValue(const std::wstring& Param);

	/**
	* Checks command line arguments for a matching flag param
	*
	* @param Param - Parameter to look for in command line arguments
	*
	* @return True if param is found in command line arguments and it is a flag param value
	*/
	bool HasFlagParam(const std::wstring& Param);

	/**
	 * Gets the raw command line string.
	 *
	 * @return The raw command line string.
	 */
	const std::wstring& GetString() const;

	/**
	* Singleton
	*/
	static FCommandLine& Get();

private:
	/** Gets a collection of sub-arguments from a string of one or two arguments */
	std::vector<std::wstring> GetSubArguments(const std::wstring& SubArgList);

	/** Add commandline params from a collection of arguments */
	void AddArguments(const std::vector<std::wstring>& Arglist);

	/**
	 * Adds parameter name to collection of command line parameters
	 * 
	 * @param Param - Parameter name to add
	 */
	void AddParam(const std::wstring& ParamName);

	FCommandLine() = default;
	/** Collections of command line arguments */
	std::map<std::wstring, std::wstring> CommandLineArgs;

	/** The raw command line string. */
	std::wstring CommandLine;
};
