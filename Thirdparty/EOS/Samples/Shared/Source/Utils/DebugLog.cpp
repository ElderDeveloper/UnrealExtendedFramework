// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "Main.h"
#include "CommandLine.h"
#include "DebugLog.h"
#include "StringUtils.h"
#include "Utils.h"

namespace
{
	static std::vector<FDebugLog::ELogTarget> LogTargets;
	static std::wofstream LogFileStream;
}

FDebugLog::FDebugLog()
{
		
}

FDebugLog::~FDebugLog()
{
		
}

void FDebugLog::Init()
{
	std::wstring FileName = L"DebugOutput.log";

	if (FCommandLine::Get().HasParam(CommandLineConstants::DebugLogFile))
	{
		std::wstring CmdFileName = FCommandLine::Get().GetParamValue(CommandLineConstants::DebugLogFile);
		if (!CmdFileName.empty())
		{
			FileName = CmdFileName;
		}
	}

	LogFileStream.open(FStringUtils::Narrow(FileName), std::wfstream::in | std::wfstream::out | std::wfstream::trunc);
}

void FDebugLog::Close()
{
	LogFileStream.close();
}

void FDebugLog::AddTarget(ELogTarget LogTarget)
{
	auto itr = std::find(LogTargets.begin(), LogTargets.end(), LogTarget);
	if (itr == LogTargets.end())
	{
		LogTargets.push_back(LogTarget);
	}
}

void FDebugLog::RemoveTarget(ELogTarget LogTarget)
{
	auto itr = std::find(LogTargets.begin(), LogTargets.end(), LogTarget);
	if (itr != LogTargets.end())
	{
		LogTargets.erase(itr);
	}
}

bool FDebugLog::HasTarget(ELogTarget LogTarget)
{
	if (std::find(LogTargets.begin(), LogTargets.end(), LogTarget) != LogTargets.end())
	{
		return true;
	}
	return false;
}

void FDebugLog::Log(const WCHAR *Msg, ...)
{
	if (Msg)
	{
		std::wstring TimeStamp = FStringUtils::Widen(FUtils::UTCTimestamp());
	
		// Allow room for new line
		WCHAR Buf[4096];
		size_t CharsTaken = wsprintf(Buf, L"%ls ", TimeStamp.c_str());

		va_list Args1;
		va_start(Args1, Msg);
		va_list Args2;
		va_copy(Args2, Args1);
		va_end(Args1);
		vswprintf(&Buf[CharsTaken], sizeof(Buf) / sizeof(Buf[0]) - CharsTaken, Msg, Args2);
		va_end(Args2);
		// Add new line
		std::wstring WBuf(Buf);

		if ((HasTarget(ELogTarget::Console) || HasTarget(ELogTarget::All)) && Main)
		{
			// Log to Console
			Main->PrintToConsole(WBuf);
		}

		// Add new line for writing to debug and file
		WBuf += L"\n";

		if (HasTarget(ELogTarget::DebugOutput) || HasTarget(ELogTarget::All))
		{
			// Log to Debug Output
			OutputDebugStringW(WBuf.c_str());
		}

		if (HasTarget(ELogTarget::File) || HasTarget(ELogTarget::All))
		{
			// Log to File
			LogFileStream.write(WBuf.c_str(), WBuf.length());
		}
	}
}

void FDebugLog::LogWarning(const WCHAR *Msg, ...)
{
	if (Msg)
	{
		std::wstring TimeStamp = FStringUtils::Widen(FUtils::UTCTimestamp());

		// Allow room for new line
		WCHAR Buf[4096];
		size_t CharsTaken = wsprintf(Buf, L"%ls ", TimeStamp.c_str());

		va_list Args1;
		va_start(Args1, Msg);
		va_list Args2;
		va_copy(Args2, Args1);
		va_end(Args1);
		vswprintf(&Buf[CharsTaken], sizeof(Buf) / sizeof(Buf[0]) - CharsTaken, Msg, Args2);
		va_end(Args2);
		// Add new line
		std::wstring WBuf(Buf);

		if ((HasTarget(ELogTarget::Console) || HasTarget(ELogTarget::All)) && Main)
		{
			// Log warning to Console
			Main->PrintWarningToConsole(WBuf);
		}

		// Add new line for writing to debug and file
		WBuf += L"\n";

		if (HasTarget(ELogTarget::DebugOutput) || HasTarget(ELogTarget::All))
		{
			// Log to Debug Output
			OutputDebugStringW(WBuf.c_str());
		}

		if (HasTarget(ELogTarget::File) || HasTarget(ELogTarget::All))
		{
			// Log to File
			LogFileStream.write(WBuf.c_str(), WBuf.length());
		}
	}
}

void FDebugLog::LogError(const WCHAR *Msg, ...)
{
	if (Msg)
	{
		std::wstring TimeStamp = FStringUtils::Widen(FUtils::UTCTimestamp());

		// Allow room for new line
		WCHAR Buf[4096];
		size_t CharsTaken = wsprintf(Buf, L"%ls ", TimeStamp.c_str());

		va_list Args1;
		va_start(Args1, Msg);
		va_list Args2;
		va_copy(Args2, Args1);
		va_end(Args1);
		vswprintf(&Buf[CharsTaken], sizeof(Buf) / sizeof(Buf[0]) - CharsTaken, Msg, Args2);
		va_end(Args2);
		// Add new line
		std::wstring WBuf(Buf);

		if ((HasTarget(ELogTarget::Console) || HasTarget(ELogTarget::All)) && Main)
		{
			// Log error to Console
			Main->PrintErrorToConsole(WBuf);
		}

		// Add new line for writing to debug and file
		WBuf += L"\n";

		if (HasTarget(ELogTarget::DebugOutput) || HasTarget(ELogTarget::All))
		{
			// Log to Debug Output
			OutputDebugStringW(WBuf.c_str());
		}

		if (HasTarget(ELogTarget::File) || HasTarget(ELogTarget::All))
		{
			// Log to File
			LogFileStream.write(WBuf.c_str(), WBuf.length());
		}
	}
}
