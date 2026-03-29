// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "CommandLine.h"
#include "DebugLog.h"
#include "StringUtils.h"
#include "Utils.h"
#include "Settings.h"

namespace
{
	static std::wofstream OutputFileStream;
	static std::wifstream InputFileStream;
	static std::wstring SettingsFileName = L"Settings.cfg";
}

constexpr wchar_t SettingsConstants::DevAuthHost[];
constexpr wchar_t SettingsConstants::DevAuthCred[];
constexpr wchar_t SettingsConstants::AutoLogin[];
constexpr wchar_t SettingsConstants::PostLoginCommand[];


/**
 * SettingsEntry
 */
class FSettingsEntry
{
public:
	/**
	 * Constructor
	 */
	FSettingsEntry() noexcept(false);
	FSettingsEntry(int InIntVal) { Set(InIntVal); }
	FSettingsEntry(float InFloatVal) { Set(InFloatVal); }
	FSettingsEntry(std::wstring InStringVal) { Set(InStringVal); }
	FSettingsEntry(bool InBoolVal) { Set(InBoolVal); }

	/**
	 * No copying or copy assignment allowed for this class.
	 */
	FSettingsEntry(FSettingsEntry const&) = delete;
	FSettingsEntry& operator=(FSettingsEntry const&) = delete;

	/**
	 * Destructor
	 */
	virtual ~FSettingsEntry() {};

	/** Get entry type */
	FSettings::SettingsEntryType GetType() { return Type; };

	/** Get entry as an integer */
	int GetAsInt()
	{
		return Type == FSettings::SettingsEntryType::Integer ? IntVal : 0;
	};

	/** Get entry as a float */
	float GetAsFloat()
	{
		return Type == FSettings::SettingsEntryType::Float ? FloatVal : 0.f;
	};

	/** Get entry as a string */
	std::wstring GetAsString()
	{
		return Type == FSettings::SettingsEntryType::String ? StringVal : L"";
	};

	/** Get entry as a bool */
	bool GetAsBool()
	{
		return Type == FSettings::SettingsEntryType::Boolean ? BoolVal : false;
	};

	/** Set entry value as integer */
	void Set(int InIntVal)
	{
		if (Type == FSettings::SettingsEntryType::String)
		{
			StringVal.clear();
		}
		Type = FSettings::SettingsEntryType::Integer;
		IntVal = InIntVal;
	}

	/** Set entry value as float */
	void Set(float InFloatVal)
	{
		if (Type == FSettings::SettingsEntryType::String)
		{
			StringVal.clear();
		}
		Type = FSettings::SettingsEntryType::Float;
		FloatVal = InFloatVal;
	}

	/** Set entry value as string */
	void Set(std::wstring InStringVal)
	{
		if (Type == FSettings::SettingsEntryType::String)
		{
			StringVal.clear();
		}
		Type = FSettings::SettingsEntryType::String;
		StringVal = InStringVal;
	}

	/** Set entry value as bool */
	void Set(bool InBoolVal)
	{
		if (Type == FSettings::SettingsEntryType::String)
		{
			StringVal.clear();
		}
		Type = FSettings::SettingsEntryType::Boolean;
		BoolVal = InBoolVal;
	}

private:
	/** Entry type */
	FSettings::SettingsEntryType Type = FSettings::SettingsEntryType::None;

	/** Entry values */
	int IntVal;					// SettingsEntryType::Integer
	float FloatVal;				// SettingsEntryType::Float
	std::wstring StringVal;		// SettingsEntryType::String
	bool BoolVal;				// SettingsEntryType::Boolean
};


FSettings& FSettings::Get()
{
	static FSettings Settings;
	return Settings;
}

void FSettings::Init()
{
	std::wstring CmdFileName = FCommandLine::Get().GetParamValue(CommandLineConstants::SettingsFile);
	if (!CmdFileName.empty())
	{
		SettingsFileName = CmdFileName;
	}

	ParseFile();
}

bool FSettings::OpenForWrite()
{
	OutputFileStream.open(FStringUtils::Narrow(SettingsFileName), std::wfstream::out | std::wfstream::trunc);

	if (OutputFileStream.fail())
	{
		FDebugLog::LogError(L"FSettings::OpenForWrite - Failed to open file for writing, Filename: %ls", SettingsFileName.c_str());
		return false;
	}

	return true;
}

void FSettings::CloseForWrite()
{
	OutputFileStream.close();
}

void FSettings::Set(std::wstring Tag, int Value, bool bWrite/* = true*/)
{
	std::map<std::wstring, FSettingsEntryPtr>::iterator Itr = SettingsEntries.find(Tag);
	if (Itr != SettingsEntries.end())
	{
		if (Itr->second == nullptr || Itr->second->GetType() != SettingsEntryType::Integer)
		{
			FDebugLog::LogError(L"FSettings::Set - Settings integer data is invalid, Tag: %ls", Tag.c_str());
			return;
		}

		if (Itr->second->GetAsInt() == Value)
		{
			// Value same as current
			return;
		}
		else
		{
			// Replace current value
			Itr->second->Set(Value);
		}
	}
	else
	{
		// Add new value
		SettingsEntries.insert({ Tag, std::make_shared<FSettingsEntry>(Value) });
	}
	if (bWrite)
	{
		WriteFile();
	}
}

void FSettings::Set(std::wstring Tag, float Value, bool bWrite/* = true*/)
{
	std::map<std::wstring, FSettingsEntryPtr>::iterator Itr = SettingsEntries.find(Tag);
	if (Itr != SettingsEntries.end())
	{
		if (Itr->second == nullptr || Itr->second->GetType() != SettingsEntryType::Float)
		{
			FDebugLog::LogError(L"FSettings::Set - Settings float data is invalid, Tag: %ls", Tag.c_str());
			return;
		}

		if (Itr->second->GetAsFloat() == Value)
		{
			// Value same as current
			return;
		}
		else
		{
			// Replace current value
			Itr->second->Set(Value);
		}
	}
	else
	{
		// Add new value
		SettingsEntries.insert({ Tag, std::make_shared<FSettingsEntry>(Value) });
	}
	if (bWrite)
	{
		WriteFile();
	}
}

void FSettings::Set(std::wstring Tag, std::wstring Value, bool bWrite/* = true*/)
{
	std::map<std::wstring, FSettingsEntryPtr>::iterator Itr = SettingsEntries.find(Tag);
	if (Itr != SettingsEntries.end())
	{
		if (Itr->second == nullptr || Itr->second->GetType() != SettingsEntryType::String)
		{
			FDebugLog::LogError(L"FSettings::Set - Settings string data is invalid, Tag: %ls", Tag.c_str());
			return;
		}

		if (Itr->second->GetAsString() == Value)
		{
			// Value same as current
			return;
		}
		else
		{
			// Replace current value
			Itr->second->Set(std::wstring(Value));
		}
	}
	else
	{
		// Add new value
		SettingsEntries.insert({ Tag, std::make_shared<FSettingsEntry>(std::wstring(Value)) });
	}
	if (bWrite)
	{
		WriteFile();
	}
}

void FSettings::Set(std::wstring Tag, bool Value, bool bWrite/* = true*/)
{
	std::map<std::wstring, FSettingsEntryPtr>::iterator Itr = SettingsEntries.find(Tag);
	if (Itr != SettingsEntries.end())
	{
		if (Itr->second == nullptr || Itr->second->GetType() != SettingsEntryType::Boolean)
		{
			FDebugLog::LogError(L"FSettings::Set - Settings bool data is invalid, Tag: %ls", Tag.c_str());
			return;
		}

		if (Itr->second->GetAsBool() == Value)
		{
			// Value same as current
			return;
		}
		else
		{
			// Replace current value
			Itr->second->Set(Value);
		}
	}
	else
	{
		// Add new value
		SettingsEntries.insert({ Tag, std::make_shared<FSettingsEntry>(Value) });
	}
	if (bWrite)
	{
		WriteFile();
	}
}

void FSettings::ParseFile()
{
	InputFileStream.open(FStringUtils::Narrow(SettingsFileName), std::wfstream::in);

	if (InputFileStream.fail())
	{
		return;
	}

	std::wstring Line;
	while (std::getline(InputFileStream, Line))
	{
		std::wstring::size_type EqualSymbolPos = Line.find(L"=");

		std::wstring TagStr = Line.substr(0, EqualSymbolPos);
		std::wstring ValStr = Line.substr(EqualSymbolPos+1);

		SettingsEntryType EntryType = GetSettingsEntryTypeFromString(ValStr);
		switch (EntryType)
		{
			case SettingsEntryType::Integer:
			{
				SettingsEntries.insert({ TagStr, std::make_shared<FSettingsEntry>(std::stoi(ValStr)) });
				break;
			}
			case SettingsEntryType::Float:
			{
				SettingsEntries.insert({ TagStr, std::make_shared<FSettingsEntry>(std::stof(ValStr)) });
				break;
			}
			case SettingsEntryType::String:
			{
				if (ValStr.length() > 2)
				{
					std::wstring ValString = ValStr.substr(1, ValStr.length() - 2);
					SettingsEntries.insert({ TagStr, std::make_shared<FSettingsEntry>(std::wstring(ValString)) });
				}
				else
				{
					FDebugLog::LogError(L"Settings - Invalid String!");
				}
				break;
			}
			case SettingsEntryType::Boolean:
			{
				SettingsEntries.insert({ TagStr, std::make_shared<FSettingsEntry>(ValStr == L"true") });
				break;
			}
			default:
				break;
		}
	}

	InputFileStream.close();
}

FSettings::SettingsEntryType FSettings::GetSettingsEntryTypeFromString(std::wstring ValStr)
{
	if (ValStr.empty())
	{
		return SettingsEntryType::None;
	}

	// Boolean
	if (ValStr == L"true" || ValStr == L"false")
	{
		return SettingsEntryType::Boolean;
	}

	// String
	if (ValStr.size() >= 2 && ValStr.front() == '\"' && ValStr.back() == '\"')
	{
		return SettingsEntryType::String;
	}

	// Float
	if (ValStr.find(L".") != std::wstring::npos)
	{
		try
		{
			size_t Size(0);
			float FloatVal = std::stof(ValStr, &Size);
			return SettingsEntryType::Float;
		}
		catch (...)
		{
			return SettingsEntryType::None;
		}
	}

	// Int
	try
	{
		size_t Size(0);
		int IntVal = std::stoi(ValStr, &Size);
		return SettingsEntryType::Integer;
	}
	catch (...)
	{
		return SettingsEntryType::None;
	}

	return SettingsEntryType::None;
}

FSettingsEntryPtr FSettings::GetEntry(std::wstring Tag)
{
	std::map<std::wstring, FSettingsEntryPtr>::iterator Itr = SettingsEntries.find(Tag);
	if (Itr != SettingsEntries.end())
	{
		return Itr->second;
	}
	return nullptr;
}

FSettings::SettingsEntryType FSettings::GetEntryType(std::wstring Tag)
{
	if (FSettingsEntryPtr SettingsEntry = GetEntry(Tag))
	{
		return SettingsEntry->GetType();
	}
	return SettingsEntryType::None;
}

bool FSettings::TryGetAsInt(std::wstring Tag, int& OutValue)
{
	if (FSettingsEntryPtr SettingsEntry = GetEntry(Tag))
	{
		OutValue = SettingsEntry->GetAsInt();
		return SettingsEntry->GetType() == SettingsEntryType::Integer;
	}
	return false;
}

bool FSettings::TryGetAsFloat(std::wstring Tag, float& OutValue)
{
	if (FSettingsEntryPtr SettingsEntry = GetEntry(Tag))
	{
		OutValue = SettingsEntry->GetAsFloat();
		return SettingsEntry->GetType() == SettingsEntryType::Float;
	}
	return false;
}

bool FSettings::TryGetAsString(std::wstring Tag, std::wstring& OutValue)
{
	if (FSettingsEntryPtr SettingsEntry = GetEntry(Tag))
	{
		OutValue = SettingsEntry->GetAsString();
		return SettingsEntry->GetType() == SettingsEntryType::String;
	}
	return false;
}

bool FSettings::TryGetAsBool(std::wstring Tag, bool& OutValue)
{
	if (FSettingsEntryPtr SettingsEntry = GetEntry(Tag))
	{
		OutValue = SettingsEntry->GetAsBool();
		return SettingsEntry->GetType() == SettingsEntryType::Boolean;
	}
	return false;
}

bool FSettings::WriteFile()
{
	if (!OpenForWrite())
	{
		return false;
	}

	for (std::map<std::wstring, FSettingsEntryPtr>::iterator Itr = SettingsEntries.begin(); Itr != SettingsEntries.end(); ++Itr)
	{
		std::wstring TagStr = Itr->first;
		FSettingsEntryPtr Entry = Itr->second;
		if (!TagStr.empty() && Entry)
		{
			const size_t BufSize = 64;
			WCHAR Buf[BufSize];
			switch (Entry->GetType())
			{
				case SettingsEntryType::Integer:
				{
					swprintf(Buf, BufSize, L"%ls=%d\n", TagStr.c_str(), Entry->GetAsInt());
					break;
				}
				case SettingsEntryType::Float:
				{
					swprintf(Buf, BufSize, L"%ls=%f\n", TagStr.c_str(), Entry->GetAsFloat());
					break;
				}
				case SettingsEntryType::String:
				{
					swprintf(Buf, BufSize, L"%ls=\"%ls\"\n", TagStr.c_str(), Entry->GetAsString().c_str());
					break;
				}
				case SettingsEntryType::Boolean:
				{
					swprintf(Buf, BufSize, L"%ls=%ls\n", TagStr.c_str(), Entry->GetAsBool() ? L"true" : L"false");
					break;
				}
				default:
					break;
			}
			std::wstring WBuf(Buf);
			OutputFileStream.write(WBuf.c_str(), WBuf.length());
		}
	}

	CloseForWrite();

	return true;
}
