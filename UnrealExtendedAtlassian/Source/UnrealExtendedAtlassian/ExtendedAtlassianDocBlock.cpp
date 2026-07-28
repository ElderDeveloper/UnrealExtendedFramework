// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianDocBlock.h"

FString FExtendedAtlassianMarkup::Escape(const FString& PlainText)
{
	FString Result = PlainText;

	// Ampersand first: escaping it after the others would corrupt the entities they introduce.
	Result.ReplaceInline(TEXT("&"), TEXT("&amp;"));
	Result.ReplaceInline(TEXT("<"), TEXT("&lt;"));
	Result.ReplaceInline(TEXT(">"), TEXT("&gt;"));
	Result.ReplaceInline(TEXT("\""), TEXT("&quot;"));

	return Result;
}

FString FExtendedAtlassianMarkup::Styled(const FString& StyleName, const FString& EscapedInner)
{
	if (EscapedInner.IsEmpty())
	{
		return FString();
	}

	return FString::Printf(TEXT("<%s>%s</>"), *StyleName, *EscapedInner);
}

FString FExtendedAtlassianMarkup::Link(const FString& Url, const FString& EscapedInner)
{
	if (EscapedInner.IsEmpty())
	{
		return FString();
	}

	// The href is an attribute value, so quotes in it would break out of the tag.
	FString SafeUrl = Url;
	SafeUrl.ReplaceInline(TEXT("\""), TEXT("%22"));
	SafeUrl.ReplaceInline(TEXT("<"), TEXT("%3C"));
	SafeUrl.ReplaceInline(TEXT(">"), TEXT("%3E"));

	return FString::Printf(TEXT("<a href=\"%s\">%s</>"), *SafeUrl, *EscapedInner);
}
