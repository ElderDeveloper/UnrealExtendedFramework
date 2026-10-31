// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EDbgTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EDbgSubsystem.generated.h"

class AGameModeBase;
class APlayerController;
class FEDbgInputProcessor;
class SEDbgMenu;
class SWidget;
class UCanvas;

/**
 * Development-build debug tools for one game instance: cheats, the debug menu and watches.
 *
 * Cheats:
 *
 *     // Every game instance, from a module's StartupModule:
 *     UEDbgSubsystem::RegisterGlobalCheat({ TEXT("GiveGold"), TEXT("Adds gold"), TEXT("Economy"), TEXT("<amount>"), true },
 *         [](const FEDbgCheatContext& Context) { ... Context.GetInt(0, 100) ... });
 *
 *     // Blueprint: Register Cheat with a custom event, e.g. from the game mode's BeginPlay.
 *
 * Run them from the console (Extended.Cheat GiveGold 500, Extended.Cheat.List) or the debug menu.
 * Cheats marked Run On Server travel to the server through UEDbgCheatComponent and run there for
 * the player who asked.
 *
 * Debug menu: the MenuKey (F10 by default), both thumbsticks clicked, or Extended.Debug.Menu. It
 * lists the cheats by category and shows Extended.Status, recent message bus traffic, recent
 * analytics events and the watches.
 *
 * Watches: Set Debug Watch (key, value) puts a line on screen until it is cleared or goes stale.
 *
 * In Shipping (EXTENDED_DEBUG_ENABLED 0) this subsystem is not created and every library function
 * does nothing, so Blueprints that use them still load.
 */
UCLASS()
class EXTENDEDDEBUG_API UEDbgSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	using FCheatHandler = TFunction<void(const FEDbgCheatContext& Context)>;

	static UEDbgSubsystem* Get(const UObject* WorldContextObject);

	/** Registers a cheat in every game instance, existing and future. A cheat with the same name is replaced. */
	static void RegisterGlobalCheat(const FEDbgCheatInfo& Info, FCheatHandler Handler);
	static void UnregisterGlobalCheat(FName CheatName);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Cheats

	/** Registers a cheat in this game instance. With Owner set, it is removed when Owner is destroyed. */
	void RegisterCheat(const FEDbgCheatInfo& Info, FCheatHandler Handler, const UObject* Owner = nullptr);

	/** Registers a Blueprint cheat; it is removed when the object that bound the event is destroyed. */
	void RegisterBlueprintCheat(const FEDbgCheatInfo& Info, const FEDbgCheatDelegate& Handler);

	void UnregisterCheat(FName CheatName);

	/** Every cheat this game instance knows, sorted by category and name. */
	TArray<FEDbgCheatInfo> GetCheats() const;

	bool HasCheat(FName CheatName) const;

	/**
	 * Runs a cheat for Player: on the server when it is marked Run On Server and Player is a
	 * client, here otherwise. Returns false when there is no such cheat.
	 */
	bool RunCheat(APlayerController* Player, FName CheatName, const TArray<FString>& Arguments);

	/** Runs a cheat on this machine, whatever its Run On Server flag. Used by UEDbgCheatComponent on the server. */
	bool ExecuteCheatHere(APlayerController* Player, FName CheatName, const TArray<FString>& Arguments);

	// Watches

	void SetWatch(const FString& Key, const FString& Value);
	void ClearWatch(const FString& Key);
	void ClearAllWatches();
	const TMap<FString, FEDbgWatch>& GetWatches() const { return Watches; }

	/** Shows or hides the watch overlay for this game instance. */
	void SetWatchesVisible(bool bVisible) { bWatchesVisible = bVisible; }
	bool AreWatchesVisible() const { return bWatchesVisible; }

	// Menu

	void OpenMenu();
	void CloseMenu();
	void ToggleMenu();
	bool IsMenuOpen() const { return MenuRoot.IsValid(); }

private:
	struct FCheatEntry
	{
		FEDbgCheatInfo Info;
		FCheatHandler Handler;
		FEDbgCheatDelegate BlueprintHandler;
		TWeakObjectPtr<const UObject> Owner;
		bool bHasOwner = false;
	};

	static TArray<FCheatEntry>& GlobalCheats();
	const FCheatEntry* FindCheat(FName CheatName) const;
	void PruneDeadCheats();
	void HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer);
	void DrawWatches(UCanvas* Canvas, APlayerController* PlayerController);

	TArray<FCheatEntry> Cheats;
	TMap<FString, FEDbgWatch> Watches;
	TSharedPtr<SWidget> MenuRoot;
	TSharedPtr<SEDbgMenu> Menu;
	TSharedPtr<FEDbgInputProcessor> InputProcessor;
	FDelegateHandle PostLoginHandle;
	FDelegateHandle DrawHandle;
	bool bWatchesVisible = true;
	bool bMenuShowedCursor = false;
};

/**
 * Shows a value on screen in development builds; compiles to nothing in Shipping.
 *
 *     EDBG_WATCH(this, TEXT("Speed"), TEXT("%.0f cm/s"), GetVelocity().Size());
 */
#if EXTENDED_DEBUG_ENABLED
#define EDBG_WATCH(WorldContextObject, Key, Format, ...) \
	do \
	{ \
		if (UEDbgSubsystem* EDbgWatchSubsystem = UEDbgSubsystem::Get(WorldContextObject)) \
		{ \
			EDbgWatchSubsystem->SetWatch(Key, FString::Printf(Format, ##__VA_ARGS__)); \
		} \
	} while (0)
#else
#define EDBG_WATCH(WorldContextObject, Key, Format, ...) do {} while (0)
#endif
