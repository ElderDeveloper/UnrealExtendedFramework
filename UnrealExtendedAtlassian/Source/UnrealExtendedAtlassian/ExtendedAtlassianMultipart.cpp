// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianMultipart.h"

#include "Misc/Guid.h"
#include "Misc/Paths.h"

namespace ExtendedAtlassianMultipartPrivate
{
	/** Appends a string as UTF-8 bytes, without a null terminator. */
	void AppendUtf8(TArray<uint8>& Out, const FString& Text)
	{
		const FTCHARToUTF8 Converted(*Text);
		Out.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
	}

	/**
	 * Strips path separators and quotes from a filename.
	 *
	 * The value is interpolated into a Content-Disposition header, so a quote or CRLF in it would
	 * let the caller inject header lines.
	 */
	FString SanitizeFileName(const FString& InName)
	{
		FString Result = FPaths::GetCleanFilename(InName);
		Result.ReplaceInline(TEXT("\""), TEXT("_"));
		Result.ReplaceInline(TEXT("\r"), TEXT("_"));
		Result.ReplaceInline(TEXT("\n"), TEXT("_"));

		return Result.IsEmpty() ? TEXT("attachment.bin") : Result;
	}
}

FString FExtendedAtlassianMultipart::MakeBoundary()
{
	// A GUID cannot realistically appear inside binary payload data at the exact byte offset that
	// would matter, and it keeps the token free of characters that need escaping.
	return FString::Printf(TEXT("----ExtendedAtlassianBoundary%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

FString FExtendedAtlassianMultipart::MakeContentTypeHeader(const FString& Boundary)
{
	return FString::Printf(TEXT("multipart/form-data; boundary=%s"), *Boundary);
}

TArray<uint8> FExtendedAtlassianMultipart::BuildBody(const TArray<FExtendedAtlassianMultipartFile>& Files, const FString& Boundary)
{
	using namespace ExtendedAtlassianMultipartPrivate;

	TArray<uint8> Body;

	// Reserve roughly the payload size plus header overhead to avoid repeated reallocation of
	// what can be several megabytes of screenshot.
	int64 EstimatedSize = 0;
	for (const FExtendedAtlassianMultipartFile& File : Files)
	{
		EstimatedSize += File.Data.Num() + 256;
	}
	Body.Reserve(static_cast<int32>(EstimatedSize + 128));

	for (const FExtendedAtlassianMultipartFile& File : Files)
	{
		AppendUtf8(Body, FString::Printf(TEXT("--%s\r\n"), *Boundary));

		AppendUtf8(Body, FString::Printf(
			TEXT("Content-Disposition: form-data; name=\"%s\"; filename=\"%s\"\r\n"),
			*File.FieldName,
			*SanitizeFileName(File.FileName)));

		AppendUtf8(Body, FString::Printf(TEXT("Content-Type: %s\r\n"), *File.ContentType));
		AppendUtf8(Body, TEXT("\r\n"));

		Body.Append(File.Data);

		AppendUtf8(Body, TEXT("\r\n"));
	}

	AppendUtf8(Body, FString::Printf(TEXT("--%s--\r\n"), *Boundary));

	return Body;
}
