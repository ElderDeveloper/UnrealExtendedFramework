// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "DebugLog.h"
#include "Input.h"
#include "Main.h"
#include "StringUtils.h"
#include "UIEvent.h"
#include "Console.h"

FConsoleLine::FConsoleLine(const std::wstring& InMessage, const FColor& InColor)
	: Message(InMessage)
	, Color(InColor)
{
}

FConsoleLine::FConsoleLine(std::wstring&& InMessage, const FColor& InColor)
	: Message(std::move(InMessage))
	, Color(InColor)
{
}

FConsole::FConsole() noexcept(false):
	Lines(MaxLines),
	bDirty(false)
{

}

FConsole::~FConsole()
{

}

void FConsole::Reset()
{
	Commands.clear();

	Clear();
}

bool FConsole::AddCommand(const std::wstring& CmdStr, std::function<void(const std::vector<std::wstring>&)>&& CmdFunc)
{
	std::wstring Command = FStringUtils::ToUpper(CmdStr);

	// Make sure we're not adding a duplicate
	auto it = Commands.find({ Command });
	if (it == Commands.end())
	{
		Commands.insert({ Command, std::move(CmdFunc) });
		return true;
	}
	else
	{
		FDebugLog::LogWarning(L"Console Command Already Added: %ls", CmdStr.c_str());
	}

	return false;
}

bool FConsole::RunCommand(const std::wstring& CmdStr)
{
	std::vector<std::wstring> Args = FStringUtils::Split(CmdStr, L' ');

	if (Args.size() > 0)
	{
		std::wstring Command = Args[0];
		Args.erase(Args.begin());

		return RunCommand(Command, Args);
	}

	return false;
}

bool FConsole::RunCommand(const std::wstring& CmdStr, const std::vector<std::wstring>& Args)
{
	std::wstring Command = FStringUtils::ToUpper(CmdStr);

	// Make sure we've added the console command with matching name
	auto it = Commands.find(Command);
	if (it != Commands.end())
	{
		// Call function
		(*it).second(Args);
		return true;
	}
	else if (!CmdStr.empty())
	{
		FDebugLog::LogWarning(L"Console Command Not Found: %ls", CmdStr.c_str());
	}

	return false;
}


size_t FConsole::GetNumLines() const
{
	std::unique_lock<std::mutex> Lock(Mutex);
	return Lines.Num();
}

std::vector<FConsoleLine> FConsole::GetLinesCopy(size_t FirstLineIndex, size_t LastLineIndex) const
{
	std::vector<FConsoleLine> LinesCopy;

	if (FirstLineIndex > LastLineIndex || Lines.IsEmpty())
	{
		return LinesCopy;
	}

	{
		std::unique_lock<std::mutex> Lock(Mutex);

		for (size_t Index = FirstLineIndex; Index <= LastLineIndex; ++Index)
		{
			LinesCopy.push_back(Lines[Index]);
		}
	}

	return LinesCopy;
}

void FConsole::AddLine(const std::wstring& Line, const FColor& Color)
{
	std::unique_lock<std::mutex> Lock(Mutex);

	if (Lines.PushBack(FConsoleLine(Line, Color)))
	{
		++NumLinesDropped;
	}

	bDirty = true;
}

void FConsole::Clear()
{
	std::unique_lock<std::mutex> Lock(Mutex);
	Lines.Clear();

	NumLinesDropped = 0;
	bDirty = true;
}
