// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianDocumentStyle.h"

#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"

TSharedPtr<FSlateStyleSet> FExtendedAtlassianDocumentStyle::StyleInstance = nullptr;

namespace ExtendedAtlassianDocumentStylePrivate
{
	const FLinearColor BodyColor(0.86f, 0.86f, 0.86f);
	const FLinearColor HeadingColor(1.0f, 1.0f, 1.0f);
	const FLinearColor MutedColor(0.62f, 0.62f, 0.62f);
	const FLinearColor LinkColor(0.36f, 0.66f, 0.96f);
	const FLinearColor CodeColor(0.95f, 0.78f, 0.55f);

	FTextBlockStyle MakeStyle(const TCHAR* Typeface, int32 Size, const FLinearColor& Color)
	{
		return FTextBlockStyle()
			.SetFont(FCoreStyle::GetDefaultFontStyle(Typeface, Size))
			.SetColorAndOpacity(FSlateColor(Color));
	}
}

FName FExtendedAtlassianDocumentStyle::GetStyleSetName()
{
	static const FName Name(TEXT("ExtendedAtlassianDocumentStyle"));
	return Name;
}

const ISlateStyle& FExtendedAtlassianDocumentStyle::Get()
{
	return *StyleInstance;
}

void FExtendedAtlassianDocumentStyle::Register()
{
	using namespace ExtendedAtlassianDocumentStylePrivate;

	if (StyleInstance.IsValid())
	{
		return;
	}

	StyleInstance = MakeShared<FSlateStyleSet>(GetStyleSetName());

	// Body copy. Everything else is sized relative to this.
	StyleInstance->Set("Doc.Body", MakeStyle(TEXT("Regular"), 10, BodyColor));

	// Heading sizes step down but stay distinguishable at a glance; h4-h6 differ by weight and
	// colour rather than size, because four more size steps would be indistinguishable.
	StyleInstance->Set("Doc.H1", MakeStyle(TEXT("Bold"), 20, HeadingColor));
	StyleInstance->Set("Doc.H2", MakeStyle(TEXT("Bold"), 16, HeadingColor));
	StyleInstance->Set("Doc.H3", MakeStyle(TEXT("Bold"), 13, HeadingColor));
	StyleInstance->Set("Doc.H4", MakeStyle(TEXT("Bold"), 11, HeadingColor));
	StyleInstance->Set("Doc.H5", MakeStyle(TEXT("Bold"), 10, MutedColor));
	StyleInstance->Set("Doc.H6", MakeStyle(TEXT("Bold"), 10, MutedColor));

	StyleInstance->Set("Doc.Quote", MakeStyle(TEXT("Italic"), 10, MutedColor));
	StyleInstance->Set("Doc.CodeBlock", MakeStyle(TEXT("Mono"), 9, CodeColor));
	StyleInstance->Set("Doc.Marker", MakeStyle(TEXT("Regular"), 10, MutedColor));
	StyleInstance->Set("Doc.TableHeader", MakeStyle(TEXT("Bold"), 10, HeadingColor));

	// Inline run styles. These names are the markup tags emitted by FExtendedAtlassianMarkup, so
	// renaming one here silently stops that formatting from resolving.
	StyleInstance->Set("Bold", MakeStyle(TEXT("Bold"), 10, BodyColor));
	StyleInstance->Set("Italic", MakeStyle(TEXT("Italic"), 10, BodyColor));
	StyleInstance->Set("Code", MakeStyle(TEXT("Mono"), 9, CodeColor));
	StyleInstance->Set("Strike", MakeStyle(TEXT("Regular"), 10, MutedColor));

	FTextBlockStyle LinkTextStyle = MakeStyle(TEXT("Regular"), 10, LinkColor);

	FButtonStyle LinkButtonStyle = FButtonStyle()
		.SetNormal(FSlateNoResource())
		.SetHovered(FSlateNoResource())
		.SetPressed(FSlateNoResource());

	FHyperlinkStyle LinkStyle = FHyperlinkStyle()
		.SetUnderlineStyle(LinkButtonStyle)
		.SetTextStyle(LinkTextStyle)
		.SetPadding(FMargin(0.0f));

	// Must be exactly "Hyperlink": that is the name SRichTextBlock's hyperlink decorator looks up in
	// the decorator style set, and a mismatch silently renders links as plain text.
	StyleInstance->Set("Hyperlink", LinkStyle);

	FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
}

void FExtendedAtlassianDocumentStyle::Unregister()
{
	if (!StyleInstance.IsValid())
	{
		return;
	}

	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	StyleInstance.Reset();
}
