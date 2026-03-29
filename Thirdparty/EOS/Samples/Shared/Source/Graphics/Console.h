// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <string>
#include <mutex>
#include "Utils/CircularBuffer.h"

class FConsoleLine
{
public:
	FConsoleLine() = default;
	FConsoleLine(const std::wstring& Message, const FColor& Color);
	FConsoleLine(std::wstring&& Message, const FColor& Color);

	std::wstring& GetMessage() { return Message; }
	const std::wstring& GetMessage() const { return Message; }

	FColor& GetColor() { return Color; }
	const FColor& GetColor() const { return Color; }

private:
	std::wstring Message;
	FColor Color;
};

/**
* In-Game Console
*/
class FConsole
{
public:
	/**
	* Constructor
	*/
	FConsole() noexcept(false);

	/**
	* No copying or copy assignment allowed for this class.
	*/
	FConsole(FConsole const&) = delete;
	FConsole& operator=(FConsole const&) = delete;

	/**
	* Destructor
	*/
	virtual ~FConsole();

	/**
	* Reset
	*/
	void Reset();

	/**
	* Adds a new console command with callback function
	*
	* @param CmdStr - Command string used to identify the console command
	* @param CmdFunc - Callback function which is called when a console command is executed
	*
	* @return True if the command was added successfully
	*/
	bool AddCommand(const std::wstring& CmdStr, std::function<void(const std::vector<std::wstring>&)>&& CmdFunc);

	/**
	* Parse and executes a console command which if successful will call the callback function
	*
	* @param CmdStr - Command string with parameters
	*
	* @return True if the command was executed successfully
	*/
	bool RunCommand(const std::wstring& CmdStr);

	/**
	* Executes a console command which if successful will call the callback function
	*
	* @param CmdStr - Command string used to identify the console command
	* @param Args - Callback function which is called when a console command is executed
	*
	* @return True if the command was executed successfully
	*/
	bool RunCommand(const std::wstring& CmdStr, const std::vector<std::wstring>& Args);

	/** 
	 * Returns how many lines of text are currently stored in the console.
	 */
	size_t GetNumLines() const;

	/**
	* Gets the lines of text that are currently added to the console from first line to the last line (both included).
	*
	* @param FirstLineIndex Index of the first line to copy
	* @param LastLineIndex Index of the last line to copy
	*
	* @return A collection of lines of text
	*/
	std::vector<FConsoleLine> GetLinesCopy(size_t FirstLineIndex, size_t LastLineIndex) const;

	/**
	* Adds a line to the console
	*
	* @param Line - String of text to add
	* @param Col - Col
	*/
	void AddLine(const std::wstring& Line, const FColor& Col = FColor(1.f, 1.f, 1.f, 1.f));

	/**
	 * How many lines were dropped so far.
	 */
	size_t GetNumLinesDropped() const { return NumLinesDropped; }

	/**
	* Clears all lines
	*/
	void Clear();

	/**
	* Returns true if console lines have been modified
	*
	* @return true if console lines have been modified
	*/
	bool IsDirty() { return bDirty; };

	/**
	 * Sets dirty flag
	 *
	 * @param NewIsDirty - Flag to set Dirty to
	 */
	void SetDirty(bool bNewIsDirty) { bDirty = bNewIsDirty; }

	/** Maximum number of lines console will have before oldest lines are removed when new ones are added */
	static constexpr int MaxLines = 1024;

private:
	/** List of console commands */
	std::unordered_map<std::wstring, std::function<void(std::vector<std::wstring>)>> Commands;

	/** Collection of lines of text (circular buffer) */
	TCircularBuffer<FConsoleLine> Lines;

	/** How many lines were dropped from the start so far. */
	size_t NumLinesDropped = 0;

	/** True if console has been updated (dirty) */
	bool bDirty = false;

	/** Mutex for mutating the console lines */
	mutable std::mutex Mutex;
};