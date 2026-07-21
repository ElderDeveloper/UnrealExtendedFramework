// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once
#if WITH_GAMEPLAY_DEBUGGER

#include "CoreMinimal.h"
#include "GameplayDebuggerCategory.h"

class AActor;
class APlayerController;
class FGameplayDebuggerCanvasContext;

// The data we're going to print inside the viewport
struct UNREALEXTENDEDQUEST_API FEGQuestDataToPrint
{
	int32 NumLoadedQuests = 0;

	// One preformatted line per quest snapshot found on quest components in the world.
	// Collected on the machine running the debugger, so on a client this shows the
	// replicated view (useful for spotting host/client divergence on a listen server).
	TArray<FString> SnapshotLines;
};

class UNREALEXTENDEDQUEST_API FEGQuestGameplayDebuggerCategory : public FGameplayDebuggerCategory
{
private:
	typedef FEGQuestGameplayDebuggerCategory Self;

public:
	FEGQuestGameplayDebuggerCategory();

	/** Creates an instance of this category - will be used on module startup to include our category in the Editor */
	static TSharedRef<FGameplayDebuggerCategory> MakeInstance() { return MakeShared<Self>(); }

	// Begin FGameplayDebuggerCategory Interface

	/** Collects the data we would like to print */
	void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;

	/** Displays the data we collected in the CollectData function */
	void DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext) override;

protected:
	// The data that we're going to print
	FEGQuestDataToPrint Data;
};

#endif // WITH_GAMEPLAY_DEBUGGER
