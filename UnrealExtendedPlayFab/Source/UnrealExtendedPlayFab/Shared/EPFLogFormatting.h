// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Shapes PlayFab request and response bodies for the ExtendedPlayFab log file, which is kept across
 * sessions (Saved/Logs/Extended and its archive) and therefore must never hold a working credential.
 */
namespace EPFLogFormatting
{
	/**
	 * Replaces the value of every credential field in a JSON body with "<redacted>": login and
	 * session tickets, entity tokens, passwords, platform tokens, secret keys and login ids.
	 * Field names match exactly, ignoring case, so "TicketId" or "ContinuationToken" stay readable.
	 * Works on the text rather than a parsed tree, so pretty-printed, compact, truncated and
	 * malformed bodies are all handled and keep their layout.
	 */
	UNREALEXTENDEDPLAYFAB_API FString RedactSecrets(const FString& Json);

	/**
	 * RedactSecrets, then cut to MaxChars (0 keeps everything) with a note saying how much was
	 * left out. Redaction comes first, so a cut can never expose part of a secret.
	 */
	UNREALEXTENDEDPLAYFAB_API FString FormatBodyForLog(const FString& Body, int32 MaxChars);
}
