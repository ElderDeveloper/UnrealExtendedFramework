// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "DebugLog.h"
#include "CommandLine.h"

constexpr wchar_t CommandLineConstants::ProductId[];
constexpr wchar_t CommandLineConstants::SandboxId[];
constexpr wchar_t CommandLineConstants::DeploymentId[];
constexpr wchar_t CommandLineConstants::ClientId[];
constexpr wchar_t CommandLineConstants::ClientSecret[];
constexpr wchar_t CommandLineConstants::DevAuthHost[];
constexpr wchar_t CommandLineConstants::DevAuthCred[];
constexpr wchar_t CommandLineConstants::AutoLogin[];
constexpr wchar_t CommandLineConstants::Fullscreen[];
constexpr wchar_t CommandLineConstants::ExchangeCode[];
constexpr wchar_t CommandLineConstants::DebugLogFile[];
constexpr wchar_t CommandLineConstants::SettingsFile[];
constexpr wchar_t CommandLineConstants::EpicPortal[];
constexpr wchar_t CommandLineConstants::LauncherAuthType[];
constexpr wchar_t CommandLineConstants::LauncherAuthLogin[];
constexpr wchar_t CommandLineConstants::LauncherAuthPassword[];
constexpr wchar_t CommandLineConstants::ScopesNoFlags[];
constexpr wchar_t CommandLineConstants::ScopesBasicProfile[];
constexpr wchar_t CommandLineConstants::ScopesFriendsList[];
constexpr wchar_t CommandLineConstants::ScopesPresence[];
constexpr wchar_t CommandLineConstants::ScopesFriendsManagement[];
constexpr wchar_t CommandLineConstants::ScopesEmail[];
constexpr wchar_t CommandLineConstants::ScopesCountry[];
constexpr wchar_t CommandLineConstants::AssetDir[];
constexpr wchar_t CommandLineConstants::Server[];
constexpr wchar_t CommandLineConstants::ServerURL[];
constexpr wchar_t CommandLineConstants::ServerPort[];
constexpr wchar_t CommandLineConstants::Locale[];
constexpr wchar_t CommandLineConstants::ServerInventoryName[];

#ifdef DEV_BUILD
constexpr wchar_t CommandLineConstants::DevUsername[];
constexpr wchar_t CommandLineConstants::DevPassword[];
#endif // DEV_BUILD

namespace
{
	const std::wstring FlagParamValue = L"__FLAG__";
	const std::wstring UnusedParamValue = L"unused";
}

FCommandLine& FCommandLine::Get()
{
	static FCommandLine CommandLine;
	return CommandLine;
}

void FCommandLine::Init(LPWSTR CmdLine)
{
	std::vector<std::wstring> Arglist;
	CommandLine = CmdLine;
	std::wstring::size_type TokenizeStartPos = CommandLine.find_first_of(L' ') + 1;
	std::wstring::size_type TokenizeEndPos = CommandLine.find_first_of(L' ', TokenizeStartPos);

	if (TokenizeStartPos != std::wstring::npos)
	{
		// Add first arg
		Arglist.push_back(CommandLine.substr(0, TokenizeStartPos - 1));
	}

	std::wstring PrevSubArg;

	while (TokenizeStartPos != std::wstring::npos && TokenizeEndPos != std::wstring::npos)
	{
		std::wstring SubArg = CommandLine.substr(TokenizeStartPos, TokenizeEndPos - TokenizeStartPos);

		if (!SubArg.empty() && SubArg.front() == L'\"')
		{
			// Get string between the 2 sets of quotes since we may have spaces in-between
			TokenizeEndPos = CommandLine.find_first_of(L'\"', TokenizeStartPos + 1);
			SubArg = CommandLine.substr(TokenizeStartPos + 1, TokenizeEndPos - TokenizeStartPos - 1);
		}

		for (const std::wstring& Arg : GetSubArguments(SubArg))
		{
			Arglist.push_back(Arg);
		}

		if (TokenizeEndPos < CommandLine.size() - 1)
		{
			TokenizeStartPos = CommandLine.find_first_of(L' ', TokenizeEndPos) + 1;
			TokenizeEndPos = CommandLine.find_first_of(L' ', TokenizeStartPos);
		}
		else
		{
			// We've reached the end of the commandline params
			TokenizeStartPos = std::wstring::npos;
			TokenizeEndPos = std::wstring::npos;
		}

		PrevSubArg = SubArg;
	}

	if (TokenizeStartPos != std::wstring::npos)
	{
		// Add last arg
		Arglist.push_back(CommandLine.substr(TokenizeStartPos, CommandLine.length()));
	}

	AddArguments(Arglist);
}

void FCommandLine::Init(const std::vector<std::wstring>& CmdLineParams)
{
	std::vector<std::wstring> Arglist;
	for (std::wstring CmdLineParam : CmdLineParams)
	{
		CommandLine += CmdLineParam;
		CommandLine += L" ";

		for (const std::wstring& Arg : GetSubArguments(CmdLineParam))
		{
			Arglist.push_back(Arg);
		}
	}
	AddArguments(Arglist);
}

std::vector<std::wstring> FCommandLine::GetSubArguments(const std::wstring& SubArgList)
{
	std::vector<std::wstring> Arglist;
	std::wstring::size_type SubArgEqualsPos = SubArgList.find_first_of(L'=');
	if (SubArgEqualsPos != std::wstring::npos)
	{
		// Add both args
		Arglist.push_back(SubArgList.substr(0, SubArgEqualsPos));
		Arglist.push_back(SubArgList.substr(SubArgEqualsPos + 1, SubArgList.length() - 1));
	}
	else
	{
		// Add single arg
		Arglist.push_back(SubArgList);
	}
	return Arglist;
}

void FCommandLine::AddArguments(const std::vector<std::wstring>& Arglist)
{
	if (!Arglist.empty())
	{
		const std::wstring Delims(L"-,;\\");
		std::wstring ParamName;
		bool bPrevWasParamName = false;
		
		int NumArgs = int(Arglist.size());

		for (int i = 0; i < NumArgs; ++i)
		{
			std::wstring CommandLineArg = Arglist[i];

			// Do we have a param name
			std::wstring::size_type Pos = CommandLineArg.find_first_not_of(Delims);
			if (Pos > 0 && Pos != std::wstring::npos)
			{
				if (bPrevWasParamName)
				{
					// We have another param name so previous was a flag (no param value)
					// Add a value to signify we have a flag param
					AddParam(ParamName);
				}

				ParamName = CommandLineArg;

				// strip out first part so we're left with just the param name
				ParamName.erase(0, Pos);

				if (bPrevWasParamName == false && i == NumArgs - 1)
				{
					// Add final flag param
					AddParam(ParamName);
				}

				bPrevWasParamName = true;
				continue;
			}
			else
			{
				bPrevWasParamName = false;
			}

			// Make sure we're not adding a duplicate
			std::map<std::wstring, std::wstring>::iterator it = CommandLineArgs.find({ ParamName });
			if (it == CommandLineArgs.end())
			{
				if (CommandLineArg.empty())
				{
					// Treat empty param value as a flag
					CommandLineArgs.insert({ ParamName, FlagParamValue });
				}
				else
				{
					CommandLineArgs.insert({ ParamName, CommandLineArg });
				}
			}
			else
			{
				FDebugLog::LogWarning(L"Command Line Arg Already Added - Param Name: %ls", ParamName.c_str());
			}

			// Clear for next param
			ParamName.clear();
		}
	}
}

void FCommandLine::AddParam(const std::wstring& ParamName)
{
	// Make sure we're not adding a duplicate
	std::map<std::wstring, std::wstring>::iterator it = CommandLineArgs.find({ ParamName });
	if (it == CommandLineArgs.end())
	{
		CommandLineArgs.insert({ ParamName, FlagParamValue });
	}
	else
	{
		FDebugLog::LogWarning(L"Command Line Arg Already Added - Param Name: %ls", ParamName.c_str());
	}
}

bool FCommandLine::HasParam(const std::wstring& Param)
{
	std::map<std::wstring, std::wstring>::iterator Itr = CommandLineArgs.find({ Param });
	if (Itr != CommandLineArgs.end())
	{
		if (Itr->second != UnusedParamValue)
		{
			return true;
		}
	}

	return false;
}

std::wstring FCommandLine::GetParamValue(const std::wstring& Param)
{
	std::map<std::wstring, std::wstring>::iterator Itr = CommandLineArgs.find({ Param });
	if (Itr != CommandLineArgs.end())
	{
		return Itr->second;
	}

	return std::wstring();
}

bool FCommandLine::HasFlagParam(const std::wstring& Param)
{
	std::map<std::wstring, std::wstring>::iterator Itr = CommandLineArgs.find({ Param });
	if (Itr != CommandLineArgs.end())
	{
		return Itr->second == FlagParamValue;
	}

	return false;
}

const std::wstring& FCommandLine::GetString() const
{
	return CommandLine;
}