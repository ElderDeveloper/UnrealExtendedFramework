// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianStyle.h"

#include "Fonts/CompositeFont.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"
#include "Brushes/SlateImageBrush.h"
#include "Brushes/SlateColorBrush.h"

TSharedPtr<FSlateStyleSet> FExtendedAtlassianStyle::StyleInstance;

namespace ExtendedAtlassianStylePrivate
{
	TMap<FString, TSharedPtr<FStandaloneCompositeFont>> FontCache;

	FSlateFontInfo Font(
		const TSharedRef<FSlateStyleSet>& Style,
		const TCHAR* Family,
		const TCHAR* Weight,
		float Size)
	{
		// CSS specifies pixels; Slate font sizes are points rendered at 96 DPI.
		// 1 CSS px = 0.75 pt, so keep every source literal in CSS-pixel units
		// at call sites and perform the single exact conversion here.
		const float SlatePointSize = Size * 0.75f;
		const FString ResourceName = FString::Printf(TEXT("%s-%s.otf"), Family, Weight);
		const FString FontPath = Style->RootToContentDir(TEXT("Fonts/") + ResourceName);
		if (FPaths::FileExists(FontPath))
		{
			TSharedPtr<FStandaloneCompositeFont>& CompositeFont = FontCache.FindOrAdd(FontPath);
			if (!CompositeFont.IsValid())
			{
				CompositeFont = MakeShared<FStandaloneCompositeFont>(
					FName(*ResourceName),
					FontPath,
					EFontHinting::Default,
					EFontLoadingPolicy::LazyLoad);
				// Emoji and pictographic symbols first: IBM Plex carries none of them, and Confluence
				// pages use them inline, so without this every one renders as a missing-glyph box.
				// Ahead of the CJK fallback so a symbol both fonts happen to cover resolves from the
				// font that is actually about symbols.
				CompositeFont->FallbackTypeface.Typeface.AppendFont(
					TEXT("NotoEmoji"),
					Style->RootToContentDir(TEXT("Fonts/NotoEmoji-Regular.ttf")),
					EFontHinting::Default,
					EFontLoadingPolicy::LazyLoad);
				CompositeFont->FallbackTypeface.Typeface.AppendFont(
					TEXT("DroidSansFallback"),
					FPaths::Combine(
						FPaths::EngineContentDir(),
						TEXT("Slate/Fonts/DroidSansFallback.ttf")),
					EFontHinting::Default,
					EFontLoadingPolicy::LazyLoad);
			}
			return FSlateFontInfo(CompositeFont, SlatePointSize);
		}

		const bool bMono = FCString::Strcmp(Family, TEXT("IBMPlexMono")) == 0;
		const bool bBold =
			FCString::Strcmp(Weight, TEXT("Medium")) == 0
			|| FCString::Strcmp(Weight, TEXT("SemiBold")) == 0;
		return FCoreStyle::GetDefaultFontStyle(
			bMono ? TEXT("Mono") : (bBold ? TEXT("Bold") : TEXT("Regular")),
			SlatePointSize);
	}

	FTextBlockStyle Text(
		const TSharedRef<FSlateStyleSet>& Style,
		const TCHAR* Family,
		const TCHAR* Weight,
		float Size,
		const FLinearColor& Color,
		int32 LetterSpacing = 0)
	{
		FSlateFontInfo FontInfo = Font(Style, Family, Weight, Size);
		FontInfo.LetterSpacing = LetterSpacing;
		return FTextBlockStyle()
			.SetFont(FontInfo)
			.SetColorAndOpacity(FSlateColor(Color));
	}

	FButtonStyle Button(
		const FLinearColor& Normal,
		const FLinearColor& Hover,
		const FLinearColor& Pressed,
		float Radius = 6.0f)
	{
		return FButtonStyle()
			.SetNormal(FSlateRoundedBoxBrush(Normal, Radius))
			.SetHovered(FSlateRoundedBoxBrush(Hover, Radius))
			.SetPressed(FSlateRoundedBoxBrush(Pressed, Radius))
			.SetDisabled(FSlateRoundedBoxBrush(Normal.CopyWithNewOpacity(0.45f), Radius))
			.SetNormalPadding(FMargin(0.0f))
			.SetPressedPadding(FMargin(0.0f));
	}

	FButtonStyle BorderedButton(
		const FLinearColor& Normal,
		const FLinearColor& NormalBorder,
		const FLinearColor& HoverBorder,
		float Radius = 7.0f)
	{
		return FButtonStyle()
			.SetNormal(FSlateRoundedBoxBrush(Normal, Radius, NormalBorder, 1.0f))
			.SetHovered(FSlateRoundedBoxBrush(Normal, Radius, HoverBorder, 1.0f))
			.SetPressed(FSlateRoundedBoxBrush(Normal, Radius, HoverBorder, 1.0f))
			.SetDisabled(FSlateRoundedBoxBrush(
				Normal.CopyWithNewOpacity(0.45f),
				Radius,
				NormalBorder.CopyWithNewOpacity(0.45f),
				1.0f))
			.SetNormalPadding(FMargin(0.0f))
			.SetPressedPadding(FMargin(0.0f));
	}
}

FName FExtendedAtlassianStyle::GetStyleSetName()
{
	static const FName Name(TEXT("ExtendedAtlassianBacklotStyle"));
	return Name;
}

const ISlateStyle& FExtendedAtlassianStyle::Get()
{
	check(StyleInstance.IsValid());
	return *StyleInstance;
}

FLinearColor FExtendedAtlassianStyle::FromHex(const TCHAR* Hex)
{
	return FLinearColor::FromSRGBColor(FColor::FromHex(Hex));
}

FLinearColor FExtendedAtlassianStyle::Color(const FName& Name)
{
	return Get().GetColor(Name);
}

float FExtendedAtlassianStyle::Metric(const FName& Name)
{
	return Get().GetFloat(Name);
}

void FExtendedAtlassianStyle::Register()
{
	using namespace ExtendedAtlassianStylePrivate;

	if (StyleInstance.IsValid())
	{
		return;
	}

	const TSharedPtr<IPlugin> Plugin =
		IPluginManager::Get().FindPlugin(TEXT("UnrealExtendedAtlassian"));
	check(Plugin.IsValid());

	StyleInstance = MakeShared<FSlateStyleSet>(GetStyleSetName());
	StyleInstance->SetContentRoot(FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources")));
	const TSharedRef<FSlateStyleSet> Style = StyleInstance.ToSharedRef();

	struct FBacklotVectorIcon
	{
		const TCHAR* Name;
		const TCHAR* File;
		FVector2f Size;
	};
	const FBacklotVectorIcon Icons[] = {
		{ TEXT("Backlot.Icon.Docs"), TEXT("Icons/BacklotDocs.svg"), FVector2f(15.0f, 15.0f) },
		{ TEXT("Backlot.Icon.Issues"), TEXT("Icons/BacklotIssues.svg"), FVector2f(15.0f, 15.0f) },
		{ TEXT("Backlot.Icon.Board"), TEXT("Icons/BacklotBoard.svg"), FVector2f(15.0f, 15.0f) },
		{ TEXT("Backlot.Icon.Pins"), TEXT("Icons/BacklotPins.svg"), FVector2f(15.0f, 15.0f) },
		{ TEXT("Backlot.Icon.Inbox"), TEXT("Icons/BacklotInbox.svg"), FVector2f(15.0f, 15.0f) },
		{ TEXT("Backlot.Icon.Search"), TEXT("Icons/BacklotSearch.svg"), FVector2f(12.0f, 12.0f) },
		{ TEXT("Backlot.Icon.Edit"), TEXT("Icons/BacklotEdit.svg"), FVector2f(12.0f, 12.0f) },
		{ TEXT("Backlot.Icon.Capture"), TEXT("Icons/BacklotCapture.svg"), FVector2f(12.0f, 12.0f) },
		{ TEXT("Backlot.Icon.Dock"), TEXT("Icons/BacklotDock.svg"), FVector2f(12.0f, 12.0f) },
		{ TEXT("Backlot.Icon.Rail"), TEXT("Icons/BacklotRail.svg"), FVector2f(13.0f, 13.0f) },
		{ TEXT("Backlot.Icon.Comment"), TEXT("Icons/BacklotComment.svg"), FVector2f(10.0f, 10.0f) },
		// Square, like every other icon here. As a 13x3 brush the three round dots were
		// stretched to fill a 28x28 button and read as three vertical bars.
		{ TEXT("Backlot.Icon.More"), TEXT("Icons/BacklotMore.svg"), FVector2f(13.0f, 13.0f) },
		{ TEXT("Backlot.Icon.CaretDown"), TEXT("Icons/BacklotCaretDown.svg"), FVector2f(8.0f, 5.0f) },
		{ TEXT("Backlot.Icon.CaretRight"), TEXT("Icons/BacklotCaretRight.svg"), FVector2f(5.0f, 8.0f) },
		{ TEXT("Backlot.Icon.Command"), TEXT("Icons/BacklotCommand.svg"), FVector2f(10.0f, 10.0f) },
	};
	for (const FBacklotVectorIcon& Icon : Icons)
	{
		Style->Set(
			Icon.Name,
			new FSlateVectorImageBrush(
				Style->RootToContentDir(Icon.File),
				Icon.Size));
	}

	struct FNamedColor
	{
		const TCHAR* Name;
		const TCHAR* Hex;
	};
	const FNamedColor Colors[] = {
		{ TEXT("Backlot.Color.Canvas"), TEXT("#0b0c0e") },
		{ TEXT("Backlot.Color.TopStrip"), TEXT("#0f1114") },
		{ TEXT("Backlot.Color.Root"), TEXT("#16181c") },
		{ TEXT("Backlot.Color.Field"), TEXT("#171a1e") },
		{ TEXT("Backlot.Color.FieldAlt"), TEXT("#1a1d22") },
		{ TEXT("Backlot.Color.Panel"), TEXT("#191b1f") },
		{ TEXT("Backlot.Color.Card"), TEXT("#202329") },
		{ TEXT("Backlot.Color.CardHover"), TEXT("#23272e") },
		{ TEXT("Backlot.Color.CardSelected"), TEXT("#262a31") },
		{ TEXT("Backlot.Color.BorderSubtle"), TEXT("#24282e") },
		{ TEXT("Backlot.Color.Border"), TEXT("#2b3037") },
		{ TEXT("Backlot.Color.BorderStrong"), TEXT("#3d434c") },
		{ TEXT("Backlot.Color.Scrollbar"), TEXT("#33383f") },
		{ TEXT("Backlot.Color.ScrollbarHover"), TEXT("#454c55") },
		{ TEXT("Backlot.Color.TextDim"), TEXT("#4d545e") },
		{ TEXT("Backlot.Color.TextMuted"), TEXT("#6f7783") },
		{ TEXT("Backlot.Color.TextSecondary"), TEXT("#a2a9b4") },
		{ TEXT("Backlot.Color.Text"), TEXT("#d7dce3") },
		{ TEXT("Backlot.Color.TextStrong"), TEXT("#e6e8ec") },
		{ TEXT("Backlot.Color.TextTitle"), TEXT("#f2f4f7") },
		{ TEXT("Backlot.Color.Blue"), TEXT("#58a6ff") },
		{ TEXT("Backlot.Color.BlueButton"), TEXT("#1c3a5c") },
		{ TEXT("Backlot.Color.BlueHover"), TEXT("#23497a") },
		{ TEXT("Backlot.Color.BlueBorder"), TEXT("#2f5b8f") },
		{ TEXT("Backlot.Color.Green"), TEXT("#57cc8a") },
		{ TEXT("Backlot.Color.GreenPanel"), TEXT("#1b2a21") },
		{ TEXT("Backlot.Color.Amber"), TEXT("#e3a54a") },
		{ TEXT("Backlot.Color.Red"), TEXT("#f0665f") },
		{ TEXT("Backlot.Color.RedPanel"), TEXT("#2b1e1e") },
		{ TEXT("Backlot.Color.Purple"), TEXT("#b6a9ff") },
		{ TEXT("Backlot.Color.Review"), TEXT("#c58fff") },
#include "Generated/ExtendedAtlassianPalette.inl"
	};
	for (const FNamedColor& Entry : Colors)
	{
		Style->Set(Entry.Name, FromHex(Entry.Hex));
	}

	const FLinearColor TextDim = FromHex(TEXT("#4d545e"));
	const FLinearColor TextMuted = FromHex(TEXT("#6f7783"));
	const FLinearColor TextSecondary = FromHex(TEXT("#a2a9b4"));
	const FLinearColor TextColor = FromHex(TEXT("#d7dce3"));
	const FLinearColor TextStrong = FromHex(TEXT("#e6e8ec"));
	const FLinearColor TextTitle = FromHex(TEXT("#f2f4f7"));

#include "Generated/ExtendedAtlassianMetrics.inl"
#include "Generated/ExtendedAtlassianTypography.inl"

	// Slate expresses letter spacing in 1/1000 em, the same unit the generated
	// tracking tokens carry, so every authored `letter-spacing` reads through here.
	const auto Tracking = [&Style](const TCHAR* Token)
	{
		return static_cast<int32>(Style->GetFloat(Token));
	};

	Style->Set(TEXT("Backlot.Sans.10"), Text(Style, TEXT("IBMPlexSans"), TEXT("Regular"), 10, TextMuted));
	Style->Set(TEXT("Backlot.Sans.10.Medium"), Text(Style, TEXT("IBMPlexSans"), TEXT("Medium"), 10, TextSecondary));
	Style->Set(TEXT("Backlot.Sans.11"), Text(Style, TEXT("IBMPlexSans"), TEXT("Regular"), 11, TextSecondary));
	Style->Set(TEXT("Backlot.Sans.11.Medium"), Text(Style, TEXT("IBMPlexSans"), TEXT("Medium"), 11, TextStrong));
	Style->Set(TEXT("Backlot.Sans.11.5.Medium"), Text(Style, TEXT("IBMPlexSans"), TEXT("Medium"), 11.5f, TextStrong));
	Style->Set(TEXT("Backlot.Sans.12"), Text(Style, TEXT("IBMPlexSans"), TEXT("Regular"), 12, TextColor));
	Style->Set(TEXT("Backlot.Sans.12.Medium"), Text(Style, TEXT("IBMPlexSans"), TEXT("Medium"), 12, TextStrong));
	Style->Set(TEXT("Backlot.Sans.13"), Text(Style, TEXT("IBMPlexSans"), TEXT("Regular"), 13, TextColor));
	Style->Set(TEXT("Backlot.Sans.13.Medium"), Text(Style, TEXT("IBMPlexSans"), TEXT("Medium"), 13, TextStrong));
	Style->Set(TEXT("Backlot.Sans.14"), Text(Style, TEXT("IBMPlexSans"), TEXT("Regular"), 14, TextColor));
	Style->Set(TEXT("Backlot.Sans.14.5"), Text(Style, TEXT("IBMPlexSans"), TEXT("Regular"), 14.5f, FromHex(TEXT("#c9cfd8"))));
	Style->Set(TEXT("Backlot.Sans.15"), Text(Style, TEXT("IBMPlexSans"), TEXT("Regular"), 15, TextColor));
	Style->Set(TEXT("Backlot.Sans.15.Medium"), Text(Style, TEXT("IBMPlexSans"), TEXT("Medium"), 15, TextStrong));
	Style->Set(TEXT("Backlot.Sans.20.Semibold"), Text(Style, TEXT("IBMPlexSans"), TEXT("SemiBold"), 20, TextStrong));
	Style->Set(TEXT("Backlot.Sans.27.Semibold"), Text(Style, TEXT("IBMPlexSans"), TEXT("SemiBold"), 27, TextTitle));
	Style->Set(TEXT("Backlot.Sans.34.Semibold"), Text(Style, TEXT("IBMPlexSans"), TEXT("SemiBold"), 34, TextTitle, Tracking(TEXT("Backlot.HTML.Tracking.Neg015em"))));
	Style->Set(TEXT("Backlot.Mono.8"), Text(Style, TEXT("IBMPlexMono"), TEXT("Regular"), 8, TextDim, Tracking(TEXT("Backlot.HTML.Tracking.08em"))));
	Style->Set(TEXT("Backlot.Mono.9"), Text(Style, TEXT("IBMPlexMono"), TEXT("Regular"), 9, TextMuted, Tracking(TEXT("Backlot.HTML.Tracking.07em"))));
	Style->Set(TEXT("Backlot.Mono.9.Medium"), Text(Style, TEXT("IBMPlexMono"), TEXT("Medium"), 9, TextSecondary, Tracking(TEXT("Backlot.HTML.Tracking.07em"))));
	Style->Set(TEXT("Backlot.Mono.10"), Text(Style, TEXT("IBMPlexMono"), TEXT("Regular"), 10, TextMuted));
	Style->Set(TEXT("Backlot.Mono.10.Medium"), Text(Style, TEXT("IBMPlexMono"), TEXT("Medium"), 10, TextSecondary, Tracking(TEXT("Backlot.HTML.Tracking.06em"))));
	Style->Set(TEXT("Backlot.Mono.10.5"), Text(Style, TEXT("IBMPlexMono"), TEXT("Regular"), 10.5f, TextMuted));
	Style->Set(TEXT("Backlot.Mono.10.5.Medium"), Text(Style, TEXT("IBMPlexMono"), TEXT("Medium"), 10.5f, TextDim, Tracking(TEXT("Backlot.HTML.Tracking.09em"))));
	Style->Set(TEXT("Backlot.Mono.11"), Text(Style, TEXT("IBMPlexMono"), TEXT("Regular"), 11, TextSecondary));
	Style->Set(TEXT("Backlot.Mono.12"), Text(Style, TEXT("IBMPlexMono"), TEXT("Regular"), 12, TextColor));
	Style->Set(TEXT("Backlot.Mono.13"), Text(Style, TEXT("IBMPlexMono"), TEXT("Regular"), 13, TextColor));

	// Document compatibility names used by the existing rich-text renderer.
	Style->Set(TEXT("Doc.Body"), Text(Style, TEXT("IBMPlexSans"), TEXT("Regular"), 15.5f, FromHex(TEXT("#c9cfd8"))));
	Style->Set(TEXT("Doc.H1"), Text(Style, TEXT("IBMPlexSans"), TEXT("SemiBold"), 27, TextTitle));
	Style->Set(TEXT("Doc.H2"), Text(Style, TEXT("IBMPlexSans"), TEXT("SemiBold"), 20, FromHex(TEXT("#eef0f4"))));
	Style->Set(TEXT("Doc.H3"), Text(Style, TEXT("IBMPlexSans"), TEXT("SemiBold"), 15, TextStrong));
	Style->Set(TEXT("Doc.H4"), Text(Style, TEXT("IBMPlexSans"), TEXT("Medium"), 13, TextStrong));
	Style->Set(TEXT("Doc.H5"), Text(Style, TEXT("IBMPlexSans"), TEXT("Medium"), 12, TextSecondary));
	Style->Set(TEXT("Doc.H6"), Text(Style, TEXT("IBMPlexSans"), TEXT("Medium"), 11, TextSecondary));
	Style->Set(TEXT("Doc.Quote"), Text(Style, TEXT("IBMPlexSans"), TEXT("Regular"), 14, FromHex(TEXT("#bcd3e8"))));
	Style->Set(TEXT("Doc.CodeBlock"), Text(Style, TEXT("IBMPlexMono"), TEXT("Regular"), 13, FromHex(TEXT("#c3c9d2"))));
	Style->Set(TEXT("SyntaxComment"), Text(Style, TEXT("IBMPlexMono"), TEXT("Regular"), 13, FromHex(TEXT("#6f7783"))));
	Style->Set(TEXT("SyntaxKeyword"), Text(Style, TEXT("IBMPlexMono"), TEXT("Regular"), 13, FromHex(TEXT("#c58fff"))));
	Style->Set(TEXT("SyntaxNumber"), Text(Style, TEXT("IBMPlexMono"), TEXT("Regular"), 13, FromHex(TEXT("#e3a54a"))));
	Style->Set(TEXT("Doc.Marker"), Text(Style, TEXT("IBMPlexSans"), TEXT("Regular"), 12, TextMuted));
	Style->Set(TEXT("Doc.TableHeader"), Text(Style, TEXT("IBMPlexMono"), TEXT("Medium"), 10, TextSecondary, Tracking(TEXT("Backlot.HTML.Tracking.09em"))));
	Style->Set(TEXT("Doc.TableKey"), Text(Style, TEXT("IBMPlexMono"), TEXT("Regular"), 13, FromHex(TEXT("#9fd0ff"))));
	Style->Set(TEXT("Doc.TableRange"), Text(Style, TEXT("IBMPlexMono"), TEXT("Regular"), 13, FromHex(TEXT("#a2a9b4"))));
	Style->Set(TEXT("Doc.TableValue"), Text(Style, TEXT("IBMPlexSans"), TEXT("Regular"), 13.5f, FromHex(TEXT("#c9cfd8"))));
	Style->Set(TEXT("Doc.TableOwner"), Text(Style, TEXT("IBMPlexSans"), TEXT("Regular"), 13.5f, FromHex(TEXT("#8a919c"))));
	Style->Set(TEXT("Bold"), Text(Style, TEXT("IBMPlexSans"), TEXT("SemiBold"), 15.5f, FromHex(TEXT("#c9cfd8"))));
	Style->Set(TEXT("Italic"), Text(Style, TEXT("IBMPlexSans"), TEXT("Regular"), 15.5f, FromHex(TEXT("#c9cfd8"))));
	Style->Set(TEXT("Code"), Text(Style, TEXT("IBMPlexMono"), TEXT("Regular"), 13, FromHex(TEXT("#9fd0ff"))));
	Style->Set(TEXT("Strike"), Text(Style, TEXT("IBMPlexSans"), TEXT("Regular"), 15, TextMuted));

	FButtonStyle LinkButton = FButtonStyle()
		.SetNormal(FSlateNoResource())
		.SetHovered(FSlateNoResource())
		.SetPressed(FSlateNoResource());
	Style->Set(
		TEXT("Hyperlink"),
		FHyperlinkStyle()
			.SetUnderlineStyle(LinkButton)
			.SetTextStyle(Text(Style, TEXT("IBMPlexSans"), TEXT("Regular"), 15, FromHex(TEXT("#7cbcff"))))
			.SetPadding(FMargin(0.0f)));
	auto RegisterIssueHyperlink =
		[&Style](
			const TCHAR* Name,
			const TCHAR* Background,
			const TCHAR* Border,
			const TCHAR* Foreground)
		{
			const FButtonStyle ChipButton = FButtonStyle()
				.SetNormal(FSlateRoundedBoxBrush(
					FExtendedAtlassianStyle::FromHex(Background),
					4.0f,
					FExtendedAtlassianStyle::FromHex(Border),
					1.0f))
				.SetHovered(FSlateRoundedBoxBrush(
					FExtendedAtlassianStyle::FromHex(Background),
					4.0f,
					FExtendedAtlassianStyle::FromHex(Foreground),
					1.0f))
				.SetPressed(FSlateRoundedBoxBrush(
					FExtendedAtlassianStyle::FromHex(Background),
					4.0f,
					FExtendedAtlassianStyle::FromHex(Foreground),
					1.0f))
				.SetNormalPadding(FMargin(0.0f))
				.SetPressedPadding(FMargin(0.0f));
			Style->Set(
				Name,
				FHyperlinkStyle()
					.SetUnderlineStyle(ChipButton)
					.SetTextStyle(Text(
						Style,
						TEXT("IBMPlexMono"),
						TEXT("Medium"),
						11.5f,
						FExtendedAtlassianStyle::FromHex(Foreground)))
					.SetPadding(FMargin(6.0f, 1.0f, 6.0f, 2.0f)));
		};
	RegisterIssueHyperlink(
		TEXT("IssueBlocked"),
		TEXT("#241c1c"),
		TEXT("#6b3b38"),
		TEXT("#f0665f"));
	RegisterIssueHyperlink(
		TEXT("IssueReview"),
		TEXT("#2f2a44"),
		TEXT("#423a5e"),
		TEXT("#c58fff"));
	RegisterIssueHyperlink(
		TEXT("IssueProgress"),
		TEXT("#182533"),
		TEXT("#27425c"),
		TEXT("#58a6ff"));
	RegisterIssueHyperlink(
		TEXT("IssueDone"),
		TEXT("#1e2a1f"),
		TEXT("#2c4430"),
		TEXT("#57cc8a"));

	Style->Set(
		TEXT("Backlot.Button.Invisible"),
		FButtonStyle()
			.SetNormal(FSlateNoResource())
			.SetHovered(FSlateNoResource())
			.SetPressed(FSlateNoResource())
			.SetDisabled(FSlateNoResource())
			.SetNormalPadding(FMargin(0.0f))
			.SetPressedPadding(FMargin(0.0f)));
	Style->Set(
		TEXT("Backlot.Button.Clear"),
		Button(FLinearColor::Transparent, FromHex(TEXT("#23272e")), FromHex(TEXT("#262a31"))));
	Style->Set(
		TEXT("Backlot.Button.Ghost"),
		Button(FLinearColor::Transparent, FromHex(TEXT("#23272e")), FromHex(TEXT("#262a31"))));
	Style->Set(
		TEXT("Backlot.Button.Nav"),
		Button(FLinearColor::Transparent, FromHex(TEXT("#202329")), FromHex(TEXT("#262a31")), 7.0f));
	Style->Set(
		TEXT("Backlot.Button.Primary"),
		Button(FromHex(TEXT("#1c3a5c")), FromHex(TEXT("#23497a")), FromHex(TEXT("#182f4a"))));
	Style->Set(
		TEXT("Backlot.Button.PrimaryFocused"),
		BorderedButton(
			FromHex(TEXT("#1c3a5c")),
			FromHex(TEXT("#58a6ff")),
			FromHex(TEXT("#7cbcff")),
			6.0f));
	Style->Set(
		TEXT("Backlot.Button.PrimarySelected"),
		Button(FromHex(TEXT("#2f5b8f")), FromHex(TEXT("#3d7fc4")), FromHex(TEXT("#1c3a5c"))));
	Style->Set(
		TEXT("Backlot.Button.Secondary"),
		Button(FromHex(TEXT("#23272e")), FromHex(TEXT("#2d323b")), FromHex(TEXT("#1d2025"))));
	Style->Set(
		TEXT("Backlot.Button.SecondaryFocused"),
		BorderedButton(
			FromHex(TEXT("#23272e")),
			FromHex(TEXT("#58a6ff")),
			FromHex(TEXT("#7cbcff")),
			6.0f));
	Style->Set(
		TEXT("Backlot.Button.SecondarySelected"),
		Button(FromHex(TEXT("#262a31")), FromHex(TEXT("#2d323b")), FromHex(TEXT("#1d2025"))));
	Style->Set(
		TEXT("Backlot.Button.Danger"),
		Button(FromHex(TEXT("#3a2020")), FromHex(TEXT("#4a2827")), FromHex(TEXT("#2b1e1e"))));
	Style->Set(
		TEXT("Backlot.Button.Error"),
		BorderedButton(
			FromHex(TEXT("#2b1e1e")),
			FromHex(TEXT("#f0665f")),
			FromHex(TEXT("#f0a9a4")),
			6.0f));
	Style->Set(
		TEXT("Backlot.Button.Disabled"),
		Button(
			FromHex(TEXT("#202329")).CopyWithNewOpacity(0.45f),
			FromHex(TEXT("#202329")).CopyWithNewOpacity(0.45f),
			FromHex(TEXT("#202329")).CopyWithNewOpacity(0.45f)));
	const FLinearColor CaptureToolBackground =
		FLinearColor::FromSRGBColor(FColor(15, 17, 20, 209));
	Style->Set(
		TEXT("Backlot.Button.CaptureTool"),
		BorderedButton(
			CaptureToolBackground,
			FromHex(TEXT("#2b3037")),
			FromHex(TEXT("#4a525c")),
			5.0f));
	Style->Set(
		TEXT("Backlot.Button.CaptureToolSelected"),
		BorderedButton(
			CaptureToolBackground,
			FromHex(TEXT("#3d434c")),
			FromHex(TEXT("#4a525c")),
			5.0f));
	Style->Set(
		TEXT("Backlot.Button.CaptureChoice"),
		BorderedButton(
			FromHex(TEXT("#171a1e")),
			FromHex(TEXT("#2f343c")),
			FromHex(TEXT("#454c55"))));
	Style->Set(
		TEXT("Backlot.Button.CaptureTypeSelected"),
		BorderedButton(
			FromHex(TEXT("#1e2c3d")),
			FromHex(TEXT("#2f5b8f")),
			FromHex(TEXT("#3d7fc4"))));
	Style->Set(
		TEXT("Backlot.Button.CapturePrioritySelected"),
		BorderedButton(
			FromHex(TEXT("#2b1e1e")),
			FromHex(TEXT("#4a2b29")),
			FromHex(TEXT("#6a3936"))));

	Style->Set(TEXT("Backlot.Brush.Canvas"), new FSlateColorBrush(FromHex(TEXT("#0b0c0e"))));
	Style->Set(
		TEXT("Backlot.Brush.Transparent"),
		new FSlateColorBrush(FLinearColor::Transparent));
	Style->Set(
		TEXT("Backlot.Brush.BorderSubtle"),
		new FSlateColorBrush(FromHex(TEXT("#24282e"))));
	Style->Set(TEXT("Backlot.Brush.TopStrip"), new FSlateColorBrush(FromHex(TEXT("#0f1114"))));
	Style->Set(TEXT("Backlot.Brush.Root"), new FSlateColorBrush(FromHex(TEXT("#16181c"))));
	Style->Set(TEXT("Backlot.Brush.Workspace"), new FSlateColorBrush(FromHex(TEXT("#202329"))));
	Style->Set(TEXT("Backlot.Brush.RowSelected"), new FSlateColorBrush(FromHex(TEXT("#262a31"))));
	Style->Set(TEXT("Backlot.Brush.Navigation"), new FSlateColorBrush(FromHex(TEXT("#191b1f"))));
	Style->Set(TEXT("Backlot.Brush.Sidebar"), new FSlateColorBrush(FromHex(TEXT("#1d2025"))));
	Style->Set(TEXT("Backlot.Brush.DiagonalBase"), new FSlateColorBrush(FromHex(TEXT("#1a1c20"))));
	Style->Set(TEXT("Backlot.Brush.Panel"), new FSlateColorBrush(FromHex(TEXT("#191b1f"))));
	Style->Set(TEXT("Backlot.Brush.Card"), new FSlateRoundedBoxBrush(FromHex(TEXT("#202329")), 6.0f));
	Style->Set(TEXT("Backlot.Brush.CardSelected"), new FSlateRoundedBoxBrush(FromHex(TEXT("#262a31")), 6.0f));
	Style->Set(TEXT("Backlot.Brush.NavSelected"), new FSlateRoundedBoxBrush(FromHex(TEXT("#23272e")), 8.0f));
	Style->Set(
		TEXT("Backlot.Brush.PinCard"),
		new FSlateRoundedBoxBrush(
			FromHex(TEXT("#1d2025")),
			10.0f,
			FromHex(TEXT("#282c33")),
			1.0f));
	Style->Set(
		TEXT("Backlot.Brush.PinThread"),
		new FSlateRoundedBoxBrush(FLinearColor::Transparent, 7.0f));
	Style->Set(
		TEXT("Backlot.Brush.PinThreadSelected"),
		new FSlateRoundedBoxBrush(FromHex(TEXT("#23272e")), 7.0f));
	Style->Set(
		TEXT("Backlot.Brush.IssueBlocked"),
		new FSlateRoundedBoxBrush(
			FromHex(TEXT("#241c1c")),
			6.0f,
			FromHex(TEXT("#4a2b29")),
			1.0f));
	Style->Set(TEXT("Backlot.Brush.BoardDrop"), new FSlateColorBrush(FromHex(TEXT("#1f2a35"))));
	Style->Set(
		TEXT("Backlot.Brush.BoardColumn"),
		new FSlateRoundedBoxBrush(
			FromHex(TEXT("#202329")),
			10.0f,
			FromHex(TEXT("#282c33")),
			1.0f));
	Style->Set(
		TEXT("Backlot.Brush.BoardCard"),
		new FSlateRoundedBoxBrush(
			FromHex(TEXT("#23272e")),
			8.0f,
			FromHex(TEXT("#2f343c")),
			1.0f));
	Style->Set(
		TEXT("Backlot.Brush.IssueThreadAmber"),
		new FSlateRoundedBoxBrush(
			FromHex(TEXT("#26241c")),
			8.0f,
			FromHex(TEXT("#4a4022")),
			1.0f));
	Style->Set(
		TEXT("Backlot.Brush.IssueThreadBlue"),
		new FSlateRoundedBoxBrush(
			FromHex(TEXT("#1c2733")),
			8.0f,
			FromHex(TEXT("#27425c")),
			1.0f));
	Style->Set(
		TEXT("Backlot.Brush.IssueThreadGreen"),
		new FSlateRoundedBoxBrush(
			FromHex(TEXT("#1c2a21")),
			8.0f,
			FromHex(TEXT("#28442f")),
			1.0f));
	Style->Set(
		TEXT("Backlot.Brush.IssueThread"),
		new FSlateRoundedBoxBrush(
			FLinearColor::Transparent,
			8.0f,
			FromHex(TEXT("#2b3037")),
			1.0f));
	Style->Set(
		TEXT("Backlot.Brush.CommentOpen"),
		new FSlateRoundedBoxBrush(
			FromHex(TEXT("#202329")),
			9.0f,
			FromHex(TEXT("#2b3037")),
			1.0f));
	Style->Set(
		TEXT("Backlot.Brush.CommentAmber"),
		new FSlateRoundedBoxBrush(
			FromHex(TEXT("#202329")),
			9.0f,
			FromHex(TEXT("#454022")),
			1.0f));
	Style->Set(
		TEXT("Backlot.Brush.CommentResolved"),
		new FSlateRoundedBoxBrush(
			FromHex(TEXT("#202329")),
			9.0f,
			FromHex(TEXT("#2c4430")),
			1.0f));
	Style->Set(TEXT("Backlot.Brush.Field"), new FSlateRoundedBoxBrush(FromHex(TEXT("#171a1e")), 6.0f));
	Style->Set(TEXT("Backlot.Brush.FieldAlt"), new FSlateRoundedBoxBrush(FromHex(TEXT("#1a1d22")), 7.0f));
	Style->Set(
		TEXT("Backlot.Brush.DocumentCallout"),
		new FSlateRoundedBoxBrush(
			FromHex(TEXT("#1b2530")),
			7.0f,
			FromHex(TEXT("#274055")),
			1.0f));
	Style->Set(
		TEXT("Backlot.Brush.DocumentTable"),
		new FSlateRoundedBoxBrush(
			FromHex(TEXT("#202329")),
			8.0f,
			FromHex(TEXT("#2b3037")),
			1.0f));
	Style->Set(
		TEXT("Backlot.Brush.DocumentCode"),
		new FSlateRoundedBoxBrush(
			FromHex(TEXT("#171a1e")),
			8.0f,
			FromHex(TEXT("#2b3037")),
			1.0f));
	Style->Set(
		TEXT("Backlot.Brush.DocumentLiveTag"),
		new FSlateRoundedBoxBrush(
			FromHex(TEXT("#1e2a1f")),
			4.0f,
			FromHex(TEXT("#2c4430")),
			1.0f));
	Style->Set(
		TEXT("Backlot.Brush.DocumentDraftTag"),
		new FSlateRoundedBoxBrush(
			FromHex(TEXT("#2a2419")),
			4.0f,
			FromHex(TEXT("#4a4022")),
			1.0f));
	Style->Set(
		TEXT("Backlot.Brush.Avatar"),
		new FSlateRoundedBoxBrush(
			FromHex(TEXT("#3b4a63")),
			13.0f));
	Style->Set(
		TEXT("Backlot.Brush.Avatar.Blue"),
		new FSlateRoundedBoxBrush(FromHex(TEXT("#3b4a63")), 13.0f));
	Style->Set(
		TEXT("Backlot.Brush.Avatar.Purple"),
		new FSlateRoundedBoxBrush(FromHex(TEXT("#3a3154")), 13.0f));
	Style->Set(
		TEXT("Backlot.Brush.Avatar.Green"),
		new FSlateRoundedBoxBrush(FromHex(TEXT("#28442f")), 13.0f));
	Style->Set(
		TEXT("Backlot.Brush.Avatar.Amber"),
		new FSlateRoundedBoxBrush(FromHex(TEXT("#4a4022")), 13.0f));
	Style->Set(
		TEXT("Backlot.Brush.Avatar.Red"),
		new FSlateRoundedBoxBrush(FromHex(TEXT("#4a2b29")), 13.0f));
	Style->Set(TEXT("Backlot.Brush.Blue"), new FSlateRoundedBoxBrush(FromHex(TEXT("#1c3a5c")), 6.0f));
	Style->Set(TEXT("Backlot.Brush.Red"), new FSlateRoundedBoxBrush(FromHex(TEXT("#3a2020")), 3.0f));
	Style->Set(TEXT("Backlot.Brush.Purple"), new FSlateRoundedBoxBrush(FromHex(TEXT("#2a2440")), 3.0f));
	Style->Set(TEXT("Backlot.Brush.BlueSolid"), new FSlateColorBrush(FromHex(TEXT("#58a6ff"))));
	Style->Set(TEXT("Backlot.Brush.GreenSolid"), new FSlateColorBrush(FromHex(TEXT("#57cc8a"))));
	Style->Set(TEXT("Backlot.Brush.RedSolid"), new FSlateColorBrush(FromHex(TEXT("#f0665f"))));
	Style->Set(TEXT("Backlot.Brush.AmberSolid"), new FSlateColorBrush(FromHex(TEXT("#e3a54a"))));
	Style->Set(
		TEXT("Backlot.Brush.Scrim"),
		new FSlateColorBrush(
			FromHex(TEXT("#08090b")).CopyWithNewOpacity(0.66f)));
	Style->Set(
		TEXT("Backlot.Brush.Modal"),
		new FSlateRoundedBoxBrush(
			FromHex(TEXT("#202329")),
			12.0f,
			FromHex(TEXT("#363c45")),
			1.0f));
	Style->Set(
		TEXT("Backlot.Brush.CaptureFrame"),
		new FSlateRoundedBoxBrush(
			FromHex(TEXT("#111318")),
			6.0f,
			FromHex(TEXT("#343a42")),
			1.0f));
	Style->Set(
		TEXT("Backlot.Brush.Toast"),
		new FSlateRoundedBoxBrush(
			FromHex(TEXT("#202329")),
			6.0f,
			FromHex(TEXT("#454c55")),
			1.0f));
	Style->Set(
		TEXT("Backlot.Brush.AnnotationBlue"),
		new FSlateRoundedBoxBrush(FromHex(TEXT("#58a6ff")), 12.0f));
	Style->Set(
		TEXT("Backlot.Brush.AnnotationRed"),
		new FSlateRoundedBoxBrush(FromHex(TEXT("#f0665f")), 12.0f));

	// A 5px round dot, drawn rather than typed. The reference draws its status and sidebar-view
	// dots as shapes; as the glyph U+25CF they resolved from the engine's CJK fallback font, which
	// carries a different weight, optical size and baseline than IBM Plex and so read as the wrong
	// icon. White so the call site can tint it, which is also what lets one brush serve every dot.
	Style->Set(
		TEXT("Backlot.Brush.Dot"),
		new FSlateRoundedBoxBrush(
			FLinearColor::White,
			2.5f,
			FVector2f(5.0f, 5.0f)));

	FEditableTextBoxStyle FieldStyle = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(
		TEXT("NormalEditableTextBox"));
	FieldStyle.SetFont(Font(Style, TEXT("IBMPlexSans"), TEXT("Regular"), 12));
	FieldStyle.SetForegroundColor(TextColor);
	FieldStyle.SetBackgroundImageNormal(FSlateRoundedBoxBrush(FromHex(TEXT("#171a1e")), 6.0f, FromHex(TEXT("#2f343c")), 1.0f));
	FieldStyle.SetBackgroundImageHovered(FSlateRoundedBoxBrush(FromHex(TEXT("#171a1e")), 6.0f, FromHex(TEXT("#3d434c")), 1.0f));
	FieldStyle.SetBackgroundImageFocused(FSlateRoundedBoxBrush(FromHex(TEXT("#171a1e")), 6.0f, FromHex(TEXT("#3d7fc4")), 1.0f));
	FieldStyle.SetPadding(FMargin(9.0f, 0.0f));
	Style->Set(TEXT("Backlot.Field"), FieldStyle);
	FEditableTextBoxStyle SearchFieldStyle = FieldStyle;
	SearchFieldStyle.SetPadding(FMargin(29.0f, 0.0f, 9.0f, 0.0f));
	Style->Set(TEXT("Backlot.Field.Search"), SearchFieldStyle);
	FEditableTextBoxStyle ErrorFieldStyle = FieldStyle;
	ErrorFieldStyle.SetBackgroundImageNormal(FSlateRoundedBoxBrush(
		FromHex(TEXT("#2b1e1e")),
		6.0f,
		FromHex(TEXT("#f0665f")),
		1.0f));
	ErrorFieldStyle.SetBackgroundImageHovered(FSlateRoundedBoxBrush(
		FromHex(TEXT("#2b1e1e")),
		6.0f,
		FromHex(TEXT("#f0a9a4")),
		1.0f));
	ErrorFieldStyle.SetBackgroundImageFocused(FSlateRoundedBoxBrush(
		FromHex(TEXT("#2b1e1e")),
		6.0f,
		FromHex(TEXT("#58a6ff")),
		2.0f));
	Style->Set(TEXT("Backlot.Field.Error"), ErrorFieldStyle);
	FEditableTextBoxStyle DisabledFieldStyle = FieldStyle;
	DisabledFieldStyle.SetForegroundColor(
		FromHex(TEXT("#6f7783")).CopyWithNewOpacity(0.65f));
	DisabledFieldStyle.SetBackgroundImageNormal(FSlateRoundedBoxBrush(
		FromHex(TEXT("#171a1e")).CopyWithNewOpacity(0.45f),
		6.0f,
		FromHex(TEXT("#2f343c")).CopyWithNewOpacity(0.45f),
		1.0f));
	DisabledFieldStyle.SetBackgroundImageHovered(
		DisabledFieldStyle.BackgroundImageNormal);
	DisabledFieldStyle.SetBackgroundImageFocused(
		DisabledFieldStyle.BackgroundImageNormal);
	Style->Set(TEXT("Backlot.Field.Disabled"), DisabledFieldStyle);

	// `::-webkit-scrollbar { width: 9px }` with a transparent track and a thumb inset
	// by its 2px transparent border. SScrollBar's default 2px uniform padding supplies
	// that inset, so a 9px bar paints a 5px thumb exactly as `background-clip: content-box`.
	FScrollBarStyle ScrollBar = FCoreStyle::Get().GetWidgetStyle<FScrollBarStyle>(TEXT("ScrollBar"));
	ScrollBar
		.SetNormalThumbImage(FSlateRoundedBoxBrush(FromHex(TEXT("#33383f")), 5.0f))
		.SetHoveredThumbImage(FSlateRoundedBoxBrush(FromHex(TEXT("#454c55")), 5.0f))
		.SetDraggedThumbImage(FSlateRoundedBoxBrush(FromHex(TEXT("#454c55")), 5.0f))
		.SetHorizontalBackgroundImage(FSlateNoResource())
		.SetVerticalBackgroundImage(FSlateNoResource())
		.SetHorizontalTopSlotImage(FSlateNoResource())
		.SetHorizontalBottomSlotImage(FSlateNoResource())
		.SetVerticalTopSlotImage(FSlateNoResource())
		.SetVerticalBottomSlotImage(FSlateNoResource())
		.SetThickness(9.0f);
	Style->Set(TEXT("Backlot.ScrollBar"), ScrollBar);

	// `:focus-visible { outline: 2px solid #58a6ff; outline-offset: 1px; border-radius: 4px; }`
	Style->Set(
		TEXT("Backlot.Brush.FocusRing"),
		new FSlateRoundedBoxBrush(
			FLinearColor::Transparent,
			Style->GetFloat(TEXT("Backlot.Metric.Focus.OutlineRadius")),
			FromHex(TEXT("#58a6ff")),
			Style->GetFloat(TEXT("Backlot.Metric.Focus.OutlineWidth"))));
	Style->Set(
		TEXT("Backlot.Brush.HighContrastFrame"),
		new FSlateRoundedBoxBrush(
			FLinearColor::Black,
			0.0f,
			FLinearColor::White,
			2.0f));

	FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
}

void FExtendedAtlassianStyle::Unregister()
{
	if (!StyleInstance.IsValid())
	{
		return;
	}

	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	StyleInstance.Reset();
	ExtendedAtlassianStylePrivate::FontCache.Reset();
}
