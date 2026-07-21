// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

/**
* Debug Logging
*/
class FDebugLog
{
public:
	/**
	* Constructor
	*/
	FDebugLog() noexcept(false);

	/**
	* No copying or copy assignment allowed for this class.
	*/
	FDebugLog(FDebugLog const&) = delete;
	FDebugLog& operator=(FDebugLog const&) = delete;

	/**
	* Destructor
	*/
	virtual ~FDebugLog();

	/**
	* Initialization
	*/
	static void Init();

	/**
	* Close
	*/
	static void Close();

	/**
	* Log Target
	*/
	enum ELogTarget
	{
		/** No Log Target */
		None = 0,

		/** Standard Debug Output */
		DebugOutput,

		/** In-Game Console */
		Console,

		/** File */
		File,

		/** All Above */
		All,

		/** Total Number of Targets */
		Total
	};

	/**
	* Adds a Log Target
	*
	* @param LogTarget - Log Target to Add
	*/
	static void AddTarget(ELogTarget LogTarget);

	/**
	* Removes a Log Target
	*
	* @param LogTarget - Log Target to Remove
	*/
	static void RemoveTarget(ELogTarget LogTarget);

	/**
	* Queries for a Log Target
	*
	* @param LogTarget - Log Target to Query
	*
	* @return True if a matching target is found
	*/
	static bool HasTarget(ELogTarget LogTarget);

	/**
	* Logs a message based on the targets set
	*
	* @param Msg - Variable length message to log out to set targets
	*/
	static void Log(const WCHAR *Msg, ...);

	/**
	* Logs an warning message based on the targets set
	*
	* @param Msg - Variable length message to log out to set targets
	*/
	static void LogWarning(const WCHAR *Msg, ...);

	/**
	* Logs an error message based on the targets set
	*
	* @param Msg - Variable length message to log out to set targets
	*/
	static void LogError(const WCHAR *Msg, ...);
};