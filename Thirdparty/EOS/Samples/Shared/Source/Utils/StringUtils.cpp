// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "StringUtils.h"
#include <utf8.h>

std::wstring FStringUtils::ToUpper(const std::wstring & Src)
{
	std::wstring Dest(Src);
	std::transform(Src.begin(), Src.end(), Dest.begin(), ::toupper);
	return std::move(Dest);
}

std::wstring FStringUtils::Widen(const std::string& Utf8)
{
	static_assert(sizeof(wchar_t) == 2 || sizeof(wchar_t) == 4, "wchar_t size is unsupported.");

	if (Utf8.empty())
	{
		return std::wstring();
	}

	std::wstring Result;
	Result.reserve(Utf8.size());

	if (sizeof(wchar_t) == 2)
	{
		utf8::utf8to16(Utf8.begin(), Utf8.end(), back_inserter(Result));
	}
	else if (sizeof(wchar_t) == 4)
	{
		utf8::utf8to32(Utf8.begin(), Utf8.end(), back_inserter(Result));
	}
	else
	{
		assert(false);
	}

	return Result;
}

std::string FStringUtils::Narrow(const std::wstring& Str)
{
	if (Str.empty())
	{
		return std::string();
	}

	std::string Result;
	Result.reserve(Str.size());
	if (sizeof(wchar_t) == 2)
	{
		utf8::utf16to8(Str.begin(), Str.end(), back_inserter(Result));
	}
	else if (sizeof(wchar_t) == 4)
	{
		utf8::utf32to8(Str.begin(), Str.end(), back_inserter(Result));
	}
	else
	{
		assert(false);
	}

	return Result;
}

std::vector<std::wstring> FStringUtils::Split(const std::wstring& Str, const wchar_t Delimiter)
{
	std::vector<std::wstring> Words;

	std::size_t Current, Previous = 0;
	Current = Str.find(Delimiter);
	while (Current != std::string::npos) {
		Words.push_back(Str.substr(Previous, Current - Previous));
		Previous = Current + 1;
		Current = Str.find(Delimiter, Previous);
	}
	Words.push_back(Str.substr(Previous, Current - Previous));

	return Words;
}
