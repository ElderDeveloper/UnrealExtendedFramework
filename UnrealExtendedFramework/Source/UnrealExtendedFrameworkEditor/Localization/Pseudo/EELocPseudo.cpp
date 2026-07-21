// Copyright Moon Punch Games. All Rights Reserved.

#include "EELocPseudo.h"

#if WITH_EDITOR

#define LOCTEXT_NAMESPACE "EELocPseudo"

namespace
{
	TCHAR AccentChar(const TCHAR Char)
	{
		switch (Char)
		{
		case TEXT('a'): return TEXT('á');
		case TEXT('e'): return TEXT('é');
		case TEXT('i'): return TEXT('í');
		case TEXT('o'): return TEXT('ö');
		case TEXT('u'): return TEXT('ü');
		case TEXT('c'): return TEXT('ç');
		case TEXT('n'): return TEXT('ñ');
		case TEXT('s'): return TEXT('š');
		case TEXT('y'): return TEXT('ý');
		case TEXT('A'): return TEXT('Á');
		case TEXT('E'): return TEXT('É');
		case TEXT('I'): return TEXT('Í');
		case TEXT('O'): return TEXT('Ö');
		case TEXT('U'): return TEXT('Ü');
		case TEXT('C'): return TEXT('Ç');
		case TEXT('N'): return TEXT('Ñ');
		case TEXT('S'): return TEXT('Š');
		default: return Char;
		}
	}

	/** Splits text into protected tokens ({..}, %x, <..>) and plain segments. */
	void TransformPlainSegments(const FString& Source, FString& OutResult, const TFunctionRef<FString(const FString&)>& TransformSegment)
	{
		FString Segment;
		auto FlushSegment = [&]()
		{
			if (!Segment.IsEmpty())
			{
				OutResult += TransformSegment(Segment);
				Segment.Reset();
			}
		};

		for (int32 i = 0; i < Source.Len(); ++i)
		{
			const TCHAR Char = Source[i];

			if (Char == TEXT('{') || Char == TEXT('<'))
			{
				const TCHAR CloseChar = Char == TEXT('{') ? TEXT('}') : TEXT('>');
				const int32 Close = Source.Find(FString::Chr(CloseChar), ESearchCase::CaseSensitive, ESearchDir::FromStart, i + 1);
				if (Close != INDEX_NONE)
				{
					FlushSegment();
					OutResult += Source.Mid(i, Close - i + 1);
					i = Close;
					continue;
				}
			}

			if (Char == TEXT('%') && i + 1 < Source.Len() && FChar::IsAlpha(Source[i + 1]))
			{
				FlushSegment();
				OutResult += Source.Mid(i, 2);
				++i;
				continue;
			}

			Segment.AppendChar(Char);
		}

		FlushSegment();
	}

	FString Expand(const FString& Text, const float Ratio)
	{
		const int32 ExtraChars = FMath::CeilToInt(Text.Len() * Ratio);
		FString Result = Text;
		for (int32 i = 0; i < ExtraChars; ++i)
		{
			Result.AppendChar(TEXT('~'));
		}
		return Result;
	}
}

FString EELocPseudo::Transform(const FString& Source, const EEELocPseudoType Type)
{
	if (Type == EEELocPseudoType::None || Source.IsEmpty())
	{
		return Source;
	}

	FString Result;

	switch (Type)
	{
	case EEELocPseudoType::ExpandedLatin:
	{
		FString Body;
		TransformPlainSegments(Source, Body, [](const FString& Segment)
		{
			FString Out;
			for (const TCHAR Char : Segment)
			{
				Out.AppendChar(AccentChar(Char));
			}
			return Expand(Out, 0.4f);
		});
		Result = FString::Printf(TEXT("[%s]"), *Body);
		break;
	}

	case EEELocPseudoType::ExtremeExpansion:
	{
		FString Body;
		TransformPlainSegments(Source, Body, [](const FString& Segment)
		{
			return Expand(Segment, 1.0f);
		});
		Result = FString::Printf(TEXT("[%s]"), *Body);
		break;
	}

	case EEELocPseudoType::Accented:
	{
		TransformPlainSegments(Source, Result, [](const FString& Segment)
		{
			FString Out;
			for (const TCHAR Char : Segment)
			{
				Out.AppendChar(AccentChar(Char));
			}
			return Out;
		});
		break;
	}

	case EEELocPseudoType::CJKDensity:
	{
		TransformPlainSegments(Source, Result, [](const FString& Segment)
		{
			FString Out;
			for (const TCHAR Char : Segment)
			{
				Out.AppendChar(FChar::IsAlpha(Char) ? TEXT('言') : Char);
			}
			return Out;
		});
		break;
	}

	case EEELocPseudoType::RTLStress:
	{
		// RLO ... PDF: forces right-to-left presentation of the whole run.
		Result = FString::Printf(TEXT("‮%s‬"), *Source);
		break;
	}

	case EEELocPseudoType::Markers:
	{
		Result = FString::Printf(TEXT("[!%s!]"), *Source);
		break;
	}

	default:
		Result = Source;
		break;
	}

	return Result;
}

FText EELocPseudo::TypeToLabel(const EEELocPseudoType Type)
{
	switch (Type)
	{
	case EEELocPseudoType::ExpandedLatin: return LOCTEXT("ExpandedLatin", "Expanded Latin");
	case EEELocPseudoType::ExtremeExpansion: return LOCTEXT("ExtremeExpansion", "Extreme Expansion");
	case EEELocPseudoType::Accented: return LOCTEXT("Accented", "Accented");
	case EEELocPseudoType::CJKDensity: return LOCTEXT("CJKDensity", "CJK Density");
	case EEELocPseudoType::RTLStress: return LOCTEXT("RTLStress", "RTL Stress");
	case EEELocPseudoType::Markers: return LOCTEXT("Markers", "Markers");
	default: return LOCTEXT("NoPseudo", "Pseudo: Off");
	}
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
