// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "Utils.h"
#include "StringUtils.h"
#include "DebugLog.h"

#ifndef _WIN32
#include <sys/time.h>
#include <time.h>
#else
#include <windows.h>
#endif

void FUtils::OpenURL(const std::string& URL)
{
	if (URL.empty())
	{
		return;
	}

	std::string URLFixed = URL;

	if (URL.find("http://") != 0 && URL.find("https://") != 0)
	{
		URLFixed = std::string("http://") + URL;
	}

#ifdef _WIN32
	ShellExecuteA(0, 0, URLFixed.c_str(), 0, 0, SW_SHOW);
#elif __APPLE__
	std::string ShellCommand = std::string("open ") + URLFixed;
	system(ShellCommand.c_str());
#else
    std::string ShellCommand = std::string("xdg-open ") + URLFixed;
    int RetVal = system(ShellCommand.c_str());
	if (RetVal < 0)
	{
		FDebugLog::LogError(L"Failed to open URL: %ls", FStringUtils::Widen(URLFixed).c_str());
	}
#endif
}

std::string FUtils::UTCTimestamp()
{
	std::string Timestamp;

	int Year = 0, Month = 0, Day = 0, Hour = 0, Min = 0, Sec = 0, MSec = 0;
	bool bSuccess = false;
#ifdef _WIN32
	FILETIME FileTime;
	GetSystemTimeAsFileTime(&FileTime);
	SYSTEMTIME SystemTime;
	if (FileTimeToSystemTime(&FileTime, &SystemTime))
	{
		Year = SystemTime.wYear;
		Month = SystemTime.wMonth;
		Day = SystemTime.wDay;

		Hour = SystemTime.wHour;
		Min = SystemTime.wMinute;
		Sec = SystemTime.wSecond;
		MSec = SystemTime.wMilliseconds;

		bSuccess = true;
	}
#else
	// query for calendar time
	struct timeval Time;
	gettimeofday(&Time, NULL);

	// convert it to UTC
	struct tm LocalTime;
	gmtime_r(&Time.tv_sec, &LocalTime);


	Year = LocalTime.tm_year + 1900;
	Month = LocalTime.tm_mon + 1;
	Day = LocalTime.tm_mday;
	Hour = LocalTime.tm_hour;
	Min = LocalTime.tm_min;
	Sec = LocalTime.tm_sec;
	MSec = Time.tv_usec / 1000;

	bSuccess = true;
#endif

	if (bSuccess)
	{
		char Buffer[256] = { 0 };
		sprintf_s(Buffer, sizeof(Buffer), "[%d.%02d.%02d-%02d.%02d.%02d:%03d]", Year, Month, Day, Hour, Min, Sec, MSec);
		Timestamp = Buffer;
	}

	return Timestamp;
}

std::wstring FUtils::ConvertUnixTimestampToUTCString(int64_t UnixTimeStamp)
{
	struct tm UnlockTimeBuf;
	const time_t UnlockTime = UnixTimeStamp;
	char DateTimeBuf[26];

#ifdef _WIN32
	if (gmtime_s(&UnlockTimeBuf, &UnlockTime))
	{
		return std::wstring();
	}
	if (asctime_s(DateTimeBuf, sizeof DateTimeBuf, &UnlockTimeBuf))
	{
		return std::wstring();
	}
#else
	gmtime_r(&UnlockTime, &UnlockTimeBuf);
	asctime_r(&UnlockTimeBuf, DateTimeBuf);
#endif

	std::string DateTimeStr = DateTimeBuf;

	// Widen
	std::wstring DateTimeStrW(FStringUtils::Widen(DateTimeStr));

	// Remove newline
	size_t pos = DateTimeStrW.find(L"\n");
	if (pos != std::wstring::npos)
	{
		DateTimeStrW.replace(pos, 5, L"");
	}

	return DateTimeStrW;
}

const char* FUtils::GetTempDirectory()
{
#ifdef _WIN32
	static char Buffer[1024] = { 0 };
	if (Buffer[0] == 0)
	{
		GetTempPathA(sizeof(Buffer), Buffer);
	}

	return Buffer;

#elif defined(__APPLE__)
	return "/private/var/tmp";
#else
	return "/var/tmp";
#endif
}

std::string FUtils::GenerateRandomId(size_t Length)
{	
	const char CharSet[] =
		"0123456789"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz";		
	
	thread_local static std::mt19937 Rng{ std::random_device{}() };
	thread_local static std::uniform_int_distribution<std::string::size_type> Distribution(0, sizeof(CharSet) - 2);

	std::string IDStr(Length, 0);	
	while (Length--)
		IDStr[Length] = CharSet[Distribution(Rng)];

	return IDStr;
}
