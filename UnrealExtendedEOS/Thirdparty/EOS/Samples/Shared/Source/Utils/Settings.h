// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Settings File Examples
 *
 * MyStringId="TestStringValue"		// String (String value includes quotes "")
 * MyIntId=1234						// Integer (A simple integer value)
 * MyFloatId=56.78					// Float (A number with decimal point)
 * MyBoolId=true					// Boolean (true or false)
 */

struct SettingsConstants
{
	/** Dev Auth Tool Host (Format: String - "ip:port", e.g. "localhost:1234") */
	static constexpr wchar_t DevAuthHost[] = L"devhost";

	/** Dev Auth Tool Credentials (Format: String - "name", e.g. "MyAccount") */
	static constexpr wchar_t DevAuthCred[] = L"devcred";

	/** Auto Login (Format: Integer - 0: No Auto Login or 1: Auto Login) */
	static constexpr wchar_t AutoLogin[] = L"autologin";

	/** Command after login (Format: String, e.g. "findbybucketid 123" */
	static constexpr wchar_t PostLoginCommand[] = L"postlogincommand";
};

class FSettingsEntry;

using FSettingsEntryPtr = std::shared_ptr<FSettingsEntry>;

/**
* Settings
*/
class FSettings
{
public:
	/**
	 * No copying or copy assignment allowed for this class.
	 */
	FSettings(FSettings const&) = delete;
	FSettings& operator=(FSettings const&) = delete;

	enum class SettingsEntryType
	{
		None = 0,
		Integer,
		Float,
		String,
		Boolean
	};

	/** Initialization */
	void Init();

	/** Open file for writing */
	bool OpenForWrite();
	
	/** Close file for writing */
	void CloseForWrite();

	/** Sets an integer value to settings, file is written if bWrite is true */
	void Set(std::wstring Tag, int Value, bool bWrite = true);

	/** Sets a float value to settings, file is written if bWrite is true */
	void Set(std::wstring Tag, float Value, bool bWrite = true);

	/** Sets a string value to settings, file is written if bWrite is true */
	void Set(std::wstring Tag, std::wstring Value, bool bWrite = true);

	/** Sets a boolean value to settings, file is written if bWrite is true */
	void Set(std::wstring Tag, bool Value, bool bWrite = true);

	/** Get settings entry type */
	FSettings::SettingsEntryType GetEntryType(std::wstring Tag);

	/** Returns true if a matching entry is found in settings data and entry is an integer */
	bool TryGetAsInt(std::wstring Tag, int& OutValue);

	/** Returns true if a matching entry is found in settings data and entry is a float */
	bool TryGetAsFloat(std::wstring Tag, float& OutValue);

	/** Returns true if a matching entry is found in settings data and entry is a string*/
	bool TryGetAsString(std::wstring Tag, std::wstring& OutValue);

	/** Returns true if a matching entry is found in settings data and entry is a bool */
	bool TryGetAsBool(std::wstring Tag, bool& OutValue);

	/** Write entries to file */
	bool WriteFile();

	/**
	* Singleton
	*/
	static FSettings& Get();

private:
	FSettings() = default;

	/**
	 * Parse contents of settings file and save each set of data (tag and value)
	 */
	void ParseFile();

	/** Returns valid settings entry if a matching entry is found in settings data */
	FSettingsEntryPtr GetEntry(std::wstring Tag);

	/**
	 * Get the settings entry type for the value stored in the given string
	 */
	FSettings::SettingsEntryType GetSettingsEntryTypeFromString(std::wstring ValStr);

	/** Mapping of settings tags to settings entries */
	std::map<std::wstring, FSettingsEntryPtr> SettingsEntries;
};
