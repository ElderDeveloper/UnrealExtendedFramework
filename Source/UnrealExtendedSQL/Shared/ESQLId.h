// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ESQLId.generated.h"

class UESQLTableAsset;

/**
 * A type-safe SQL row identifier with built-in editor picker.
 *
 * In the Unreal Editor, an FESQLId property shows a searchable dropdown
 * that reads rows directly from a SQLite database. The picker configuration
 * comes from the referenced UESQLTableAsset.
 *
 * ── Blueprint Usage (recommended) ────────────────────────────────────
 *
 *   1. Create a UESQLTableAsset (e.g. DA_Weapons) for your table
 *   2. In the table asset, set DefaultLabelColumn to the human-readable column
 *   3. Add an FESQLId variable to your Blueprint struct/class
 *   4. Set SourceTable to DA_Weapons
 *   5. Optional: override LabelColumn per FESQLId if this property should
 *      display a different column than the table asset default
 *   6. The picker appears automatically, showing rows from the database
 *
 * ── C++ Usage (meta tags, zero overhead) ─────────────────────────────
 *
 *   UPROPERTY(EditAnywhere, meta=(ESQLIdTable="weapons", ESQLIdColumn="weapon_id", ESQLLabelColumn="name"))
 *   FESQLId EquippedWeaponId;
 *
 *   Optional: meta=(ESQLDatabase="GameDB") — default "EditorData"
 *
 * ── Customization Priority ───────────────────────────────────────────
 *
 *   meta tags (C++) → SourceTable asset (BP) → plain text box (fallback)
 */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDSQL_API FESQLId
{
	GENERATED_BODY()

	FESQLId() = default;
	explicit FESQLId(const FString& InValue)
		: Value(InValue)
	{
	}


	// ── Value ────────────────────────────────────────────────────────

	/** The raw SQL id value. This is what gets stored and queried at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL")
	FString Value;


	// ── Picker Source ────────────────────────────────────────────────

	/** The UESQLTableAsset that defines the database, table, and columns
	    for the editor picker. Set this in your Blueprint struct defaults
	    by dragging a table asset from the Content Browser.
	    C++ users can use meta tags instead (see class comment). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL")
	TSoftObjectPtr<UESQLTableAsset> SourceTable;

	/** Optional column override for the editor picker label.
	    Empty uses SourceTable.DefaultLabelColumn.
	    The stored Value remains the row id / primary key. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL")
	FString LabelColumn;


	// ── Utility ──────────────────────────────────────────────────────

	/** Returns true if no id value is set. */
	bool IsEmpty() const
	{
		return Value.IsEmpty();
	}

	/** Clear the id value. */
	void Reset()
	{
		Value.Reset();
	}

	/** Get the id as a string reference. */
	const FString& ToString() const
	{
		return Value;
	}

	bool operator==(const FESQLId& Other) const
	{
		return Value == Other.Value;
	}

	bool operator!=(const FESQLId& Other) const
	{
		return !(*this == Other);
	}

	friend uint32 GetTypeHash(const FESQLId& SqlId)
	{
		return GetTypeHash(SqlId.Value);
	}
};
