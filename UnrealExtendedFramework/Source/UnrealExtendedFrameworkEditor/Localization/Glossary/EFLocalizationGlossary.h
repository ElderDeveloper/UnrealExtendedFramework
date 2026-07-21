// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "EFLocalizationGlossary.generated.h"

USTRUCT()
struct FEFGlossaryTerm
{
	GENERATED_BODY()

	/** Canonical term as it appears in native source text. */
	UPROPERTY(EditAnywhere, Category = "Term")
	FString Term;

	UPROPERTY(EditAnywhere, Category = "Term", meta = (MultiLine = true))
	FString Definition;

	/** Culture code -> approved translation. */
	UPROPERTY(EditAnywhere, Category = "Term")
	TMap<FString, FString> ApprovedTranslations;

	/** The term must remain untranslated in every culture (product names, ...). */
	UPROPERTY(EditAnywhere, Category = "Term")
	bool bDoNotTranslate = false;

	/** Translations that must never be used for this term (any culture). */
	UPROPERTY(EditAnywhere, Category = "Term")
	TArray<FString> ForbiddenAlternatives;

	/** Capitalization/plural/formality notes for translators. */
	UPROPERTY(EditAnywhere, Category = "Term", meta = (MultiLine = true))
	FString Notes;

	UPROPERTY(EditAnywhere, Category = "Term")
	TArray<FName> FeatureTags;
};

/**
 * LW-5 glossary: canonical terms with per-culture approved translations, do-not-translate
 * flags, and forbidden alternatives. Editor-only asset (UncookedOnly module) consumed by the
 * glossary validation rule and exported into translator context packages.
 */
UCLASS(BlueprintType)
class UNREALEXTENDEDFRAMEWORKEDITOR_API UEFLocalizationGlossary : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Glossary")
	TArray<FEFGlossaryTerm> Terms;
};
