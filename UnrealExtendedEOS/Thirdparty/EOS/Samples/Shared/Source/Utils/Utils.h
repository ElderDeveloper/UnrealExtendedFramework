// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

/**
* Generic Utils
*/
class FUtils
{
public:
	/**
	 * No Constructor
	 */
	FUtils() = delete;

	/**
	 * No copying or copy assignment allowed for this class.
	 */
	FUtils(FUtils const&) = delete;
	FUtils& operator=(FUtils const&) = delete;

	/** 
	 * Opens URL in platform-specific way: e.g. via opening it in default system browser.
	 */
	static void OpenURL(const std::string& URL);

	/** 
	 * Generates UTC timestamp that contains current date and time.
	 */
	static std::string UTCTimestamp();

	/**
	 * Converts a unix timestamp into a date string.
	 */
	static std::wstring ConvertUnixTimestampToUTCString(int64_t UnixTimeStamp);

	/** 
	 * Returns full path to system temporary directory
	 */
	static const char* GetTempDirectory();

	/**
	 * Generate a random string of alphanumeric characters
	 */
	static std::string GenerateRandomId(size_t Length);
};
