// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

/**
* String Utils
*/
class FStringUtils
{
public:
	/**
	* No constructor
	*/
	FStringUtils() = delete;

	/**
	* No copying or copy assignment allowed for this class.
	*/
	FStringUtils(FStringUtils const&) = delete;
	FStringUtils& operator=(FStringUtils const&) = delete;

	/**
	* Converts string to upper case
	*
	* @param Src - Source string to convert
	* 
	* @return String converted to upper case
	*/
	static std::wstring ToUpper(const std::wstring & Src);

	/**
	* Converts narrow string to wide string
	*
	* @param Utf8 - Source string to convert
	*
	* @return String converted to wide string
	*/
	static std::wstring Widen(const std::string& Utf8);

	/**
	* Converts wide string to narrow string
	*
	* @param Str - Source string to convert
	*
	* @return String converted to narrow string
	*/
	static std::string Narrow(const std::wstring& Str);

	/*
	* Return a list of the words in the string, using sep as the delimiter string
	* 
	* @param Str - Source string
	* @param Delimiter - Delimiter char
	* 
	* @return vertor of words
	*/
	static std::vector<std::wstring> Split(const std::wstring& Str, const wchar_t Delimiter);
};
