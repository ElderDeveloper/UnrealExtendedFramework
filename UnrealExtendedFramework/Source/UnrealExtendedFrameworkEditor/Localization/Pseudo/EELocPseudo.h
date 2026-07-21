// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

/** LW-6 pseudo-localization transforms. Preview-only — never written to culture archives. */
enum class EEELocPseudoType : uint8
{
	None,
	/** Accented Latin + ~40% expansion inside [brackets]. */
	ExpandedLatin,
	/** ~100% expansion for worst-case layout stress. */
	ExtremeExpansion,
	/** Accented characters only (no expansion) — font/glyph stress. */
	Accented,
	/** CJK-density replacement — wide-glyph and fallback-font stress. */
	CJKDensity,
	/** RTL control characters around the text — mirroring/alignment stress. */
	RTLStress,
	/** Visible [! !] markers — finds unlocalized/hardcoded text at a glance. */
	Markers
};

namespace EELocPseudo
{
	/**
	 * Applies the transform, leaving {named} format arguments, printf-style %tokens, and
	 * rich-text <tags> untouched so transformed text still formats and parses.
	 */
	UNREALEXTENDEDFRAMEWORKEDITOR_API FString Transform(const FString& Source, EEELocPseudoType Type);

	UNREALEXTENDEDFRAMEWORKEDITOR_API FText TypeToLabel(EEELocPseudoType Type);

	inline constexpr EEELocPseudoType AllTypes[] = {
		EEELocPseudoType::None,
		EEELocPseudoType::ExpandedLatin,
		EEELocPseudoType::ExtremeExpansion,
		EEELocPseudoType::Accented,
		EEELocPseudoType::CJKDensity,
		EEELocPseudoType::RTLStress,
		EEELocPseudoType::Markers
	};
}

#endif // WITH_EDITOR
