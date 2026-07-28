// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** One file part of a multipart/form-data body. */
struct FExtendedAtlassianMultipartFile
{
	/** Jira's attachment endpoint expects the field to be named "file". */
	FString FieldName = TEXT("file");

	FString FileName;
	FString ContentType = TEXT("application/octet-stream");
	TArray<uint8> Data;
};

/**
 * Builds multipart/form-data bodies by hand.
 *
 * UE's HTTP module has no multipart support, and Jira's attachment endpoint accepts nothing else.
 * The framing is byte-exact and CRLF-sensitive: a stray LF or a missing trailing boundary produces
 * a 400 that reads like an authentication problem, so this is kept isolated and testable.
 */
class UNREALEXTENDEDATLASSIAN_API FExtendedAtlassianMultipart
{
public:
	/** A boundary token that will not collide with the payload. */
	static FString MakeBoundary();

	/** Assembles the complete body, including the terminating boundary. */
	static TArray<uint8> BuildBody(const TArray<FExtendedAtlassianMultipartFile>& Files, const FString& Boundary);

	/** The matching Content-Type header value. */
	static FString MakeContentTypeHeader(const FString& Boundary);
};
