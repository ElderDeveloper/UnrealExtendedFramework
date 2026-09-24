// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFLogFormatting.h"

namespace EPFLogFormattingPrivate
{
	/** Fields that carry a credential somewhere in the PlayFab API, in a request or a response. */
	static const TCHAR* const SecretFieldNames[] =
	{
		TEXT("SessionTicket"),
		TEXT("EntityToken"),
		TEXT("SteamTicket"),
		TEXT("Ticket"),
		TEXT("AuthTicket"),
		TEXT("Password"),
		TEXT("AccessToken"),
		TEXT("IdToken"),
		TEXT("IdentityToken"),
		TEXT("AuthCode"),
		TEXT("ServerAuthCode"),
		TEXT("XboxToken"),
		TEXT("SecretKey"),
		TEXT("DeveloperSecretKey"),
		// Login ids double as the credential for the matching LoginWith* call.
		TEXT("CustomId"),
		TEXT("ServerCustomId"),
		TEXT("AndroidDeviceId"),
		TEXT("iOSDeviceId"),
	};

	static bool IsSecretFieldName(FStringView Name)
	{
		for (const TCHAR* Secret : SecretFieldNames)
		{
			if (Name.Equals(Secret, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	/** Index just past the closing quote of the string whose opening quote is at Start; the text's length when it never closes. */
	static int32 FindStringEnd(const FString& Text, int32 Start)
	{
		for (int32 Index = Start + 1; Index < Text.Len(); ++Index)
		{
			if (Text[Index] == TEXT('\\'))
			{
				// Skips the escaped character, which may be a quote.
				++Index;
				continue;
			}
			if (Text[Index] == TEXT('"'))
			{
				return Index + 1;
			}
		}
		return Text.Len();
	}

	static int32 SkipWhitespace(const FString& Text, int32 Index)
	{
		while (Index < Text.Len() && FChar::IsWhitespace(Text[Index]))
		{
			++Index;
		}
		return Index;
	}
}

FString EPFLogFormatting::RedactSecrets(const FString& Json)
{
	using namespace EPFLogFormattingPrivate;

	FString Result;
	Result.Reserve(Json.Len());

	// Json[Copied..] has not been copied into Result yet.
	int32 Copied = 0;
	int32 Index = 0;
	while (Index < Json.Len())
	{
		if (Json[Index] != TEXT('"'))
		{
			++Index;
			continue;
		}

		// A string is a field name when a ':' follows it. Anything else is a value, skipped whole so
		// that a quote inside it is never taken for the start of a name.
		const int32 NameEnd = FindStringEnd(Json, Index);
		const int32 AfterName = SkipWhitespace(Json, NameEnd);
		const bool bFieldName = NameEnd - Index >= 2 && AfterName < Json.Len() && Json[AfterName] == TEXT(':');
		if (!bFieldName || !IsSecretFieldName(FStringView(*Json + Index + 1, NameEnd - Index - 2)))
		{
			Index = NameEnd;
			continue;
		}

		// Only a string value is replaced. An object (the login response's EntityToken wrapper) is
		// walked into instead, and the "EntityToken" string inside it is caught on the way.
		const int32 ValueStart = SkipWhitespace(Json, AfterName + 1);
		if (ValueStart >= Json.Len() || Json[ValueStart] != TEXT('"'))
		{
			Index = AfterName + 1;
			continue;
		}

		// A value cut off by truncation never closes; it is still replaced whole.
		const int32 ValueEnd = FindStringEnd(Json, ValueStart);
		Result.AppendChars(*Json + Copied, ValueStart + 1 - Copied);
		Result += TEXT("<redacted>\"");
		Copied = ValueEnd;
		Index = ValueEnd;
	}

	Result.AppendChars(*Json + Copied, Json.Len() - Copied);
	return Result;
}

FString EPFLogFormatting::FormatBodyForLog(const FString& Body, int32 MaxChars)
{
	FString Text = RedactSecrets(Body);
	if (MaxChars > 0 && Text.Len() > MaxChars)
	{
		const int32 Omitted = Text.Len() - MaxChars;
		Text.LeftInline(MaxChars);
		Text += FString::Printf(TEXT("\n... %d more characters not logged (Project Settings > Extended PlayFab > Logged Body Max Chars)"), Omitted);
	}
	return Text;
}
