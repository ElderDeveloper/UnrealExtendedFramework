// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EDbgSubsystem.h"

#include "Analytics/EFAnalyticsSubsystem.h"
#include "CoreGlobals.h"
#include "Debug/DebugDrawService.h"
#include "EDbgCheatComponent.h"
#include "EDbgMenu.h"
#include "EDbgSettings.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "ExtendedDebugModule.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Subsystems/EFSubsystems.h"
#include "Widgets/Layout/SBox.h"

// FEDbgCheatContext

FString FEDbgCheatContext::GetString(int32 Index, const FString& Default) const
{
	return Arguments.IsValidIndex(Index) ? Arguments[Index] : Default;
}

float FEDbgCheatContext::GetFloat(int32 Index, float Default) const
{
	float Value = Default;
	if (Arguments.IsValidIndex(Index))
	{
		LexFromString(Value, *Arguments[Index]);
	}
	return Value;
}

int32 FEDbgCheatContext::GetInt(int32 Index, int32 Default) const
{
	int32 Value = Default;
	if (Arguments.IsValidIndex(Index))
	{
		LexFromString(Value, *Arguments[Index]);
	}
	return Value;
}

// Console commands

#if EXTENDED_DEBUG_ENABLED
namespace EDbgPrivate
{
	FAutoConsoleCommandWithWorldAndArgs CheatCommand(
		TEXT("Extended.Cheat"),
		TEXT("Runs an Extended cheat for the first local player: Extended.Cheat <Name> [arguments]. Extended.Cheat.List lists them."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UEDbgSubsystem* Debug = UEDbgSubsystem::Get(World);
			if (!Debug || Args.Num() == 0)
			{
				UE_LOG(LogExtendedDebug, Display, TEXT("Usage: Extended.Cheat <Name> [arguments]. Extended.Cheat.List lists the cheats."));
				return;
			}
			TArray<FString> Arguments = Args;
			Arguments.RemoveAt(0);
			if (!Debug->RunCheat(World ? World->GetFirstPlayerController() : nullptr, FName(*Args[0]), Arguments))
			{
				UE_LOG(LogExtendedDebug, Display, TEXT("No cheat named %s. Extended.Cheat.List lists the cheats."), *Args[0]);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs ListCommand(
		TEXT("Extended.Cheat.List"),
		TEXT("Lists the Extended cheats by category."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			const UEDbgSubsystem* Debug = UEDbgSubsystem::Get(World);
			if (!Debug)
			{
				UE_LOG(LogExtendedDebug, Display, TEXT("Extended debug tools are not running in this world."));
				return;
			}
			FName Category;
			for (const FEDbgCheatInfo& Info : Debug->GetCheats())
			{
				if (Info.Category != Category)
				{
					Category = Info.Category;
					UE_LOG(LogExtendedDebug, Display, TEXT("%s"), *Category.ToString());
				}
				UE_LOG(LogExtendedDebug, Display, TEXT("  %s %s%s - %s"), *Info.Name.ToString(), *Info.ArgumentHint,
					Info.bRunOnServer ? TEXT(" [server]") : TEXT(""), *Info.Help);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs MenuCommand(
		TEXT("Extended.Debug.Menu"),
		TEXT("Opens or closes the Extended debug menu."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (UEDbgSubsystem* Debug = UEDbgSubsystem::Get(World))
			{
				Debug->ToggleMenu();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs WatchesCommand(
		TEXT("Extended.Debug.Watches"),
		TEXT("Shows (1) or hides (0) the watch overlay; no argument toggles it."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UEDbgSubsystem* Debug = UEDbgSubsystem::Get(World))
			{
				Debug->SetWatchesVisible(Args.Num() > 0 ? Args[0].ToBool() : !Debug->AreWatchesVisible());
			}
		}));
}
#endif // EXTENDED_DEBUG_ENABLED

// Subsystem

UEDbgSubsystem* UEDbgSubsystem::Get(const UObject* WorldContextObject)
{
	return EF::GetGameInstanceSubsystem<UEDbgSubsystem>(WorldContextObject);
}

TArray<UEDbgSubsystem::FCheatEntry>& UEDbgSubsystem::GlobalCheats()
{
	static TArray<FCheatEntry> Entries;
	return Entries;
}

void UEDbgSubsystem::RegisterGlobalCheat(const FEDbgCheatInfo& Info, FCheatHandler Handler)
{
	if (Info.Name.IsNone() || !Handler)
	{
		return;
	}
	UnregisterGlobalCheat(Info.Name);
	FCheatEntry& Entry = GlobalCheats().AddDefaulted_GetRef();
	Entry.Info = Info;
	Entry.Handler = MoveTemp(Handler);
}

void UEDbgSubsystem::UnregisterGlobalCheat(FName CheatName)
{
	GlobalCheats().RemoveAll([CheatName](const FCheatEntry& Entry) { return Entry.Info.Name == CheatName; });
}

bool UEDbgSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if EXTENDED_DEBUG_ENABLED
	return UEDbgSettings::Get()->bEnabled && Super::ShouldCreateSubsystem(Outer);
#else
	return false;
#endif
}

void UEDbgSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

#if EXTENDED_DEBUG_ENABLED
	bWatchesVisible = UEDbgSettings::Get()->bShowWatches;
	PostLoginHandle = FGameModeEvents::GameModePostLoginEvent.AddUObject(this, &UEDbgSubsystem::HandlePostLogin);

	if (!IsRunningDedicatedServer())
	{
		DrawHandle = UDebugDrawService::Register(TEXT("Game"), FDebugDrawDelegate::CreateUObject(this, &UEDbgSubsystem::DrawWatches));
		if (FSlateApplication::IsInitialized())
		{
			InputProcessor = MakeShared<FEDbgInputProcessor>(this);
			FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor);
		}
	}
#endif
}

void UEDbgSubsystem::Deinitialize()
{
	CloseMenu();

	if (InputProcessor.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
	}
	InputProcessor.Reset();
	if (DrawHandle.IsValid())
	{
		UDebugDrawService::Unregister(DrawHandle);
		DrawHandle.Reset();
	}
	FGameModeEvents::GameModePostLoginEvent.Remove(PostLoginHandle);
	PostLoginHandle.Reset();

	Cheats.Reset();
	Watches.Reset();
	Super::Deinitialize();
}

// Cheats

void UEDbgSubsystem::RegisterCheat(const FEDbgCheatInfo& Info, FCheatHandler Handler, const UObject* Owner)
{
	if (Info.Name.IsNone() || !Handler)
	{
		UE_LOG(LogExtendedDebug, Warning, TEXT("RegisterCheat: a cheat needs a name and a handler."));
		return;
	}
	UnregisterCheat(Info.Name);
	FCheatEntry& Entry = Cheats.AddDefaulted_GetRef();
	Entry.Info = Info;
	Entry.Handler = MoveTemp(Handler);
	Entry.Owner = Owner;
	Entry.bHasOwner = Owner != nullptr;
	if (Menu.IsValid())
	{
		Menu->RebuildCheats();
	}
}

void UEDbgSubsystem::RegisterBlueprintCheat(const FEDbgCheatInfo& Info, const FEDbgCheatDelegate& Handler)
{
	if (Info.Name.IsNone() || !Handler.IsBound())
	{
		UE_LOG(LogExtendedDebug, Warning, TEXT("Register Cheat: a cheat needs a name and a bound event."));
		return;
	}
	UnregisterCheat(Info.Name);
	FCheatEntry& Entry = Cheats.AddDefaulted_GetRef();
	Entry.Info = Info;
	Entry.BlueprintHandler = Handler;
	Entry.Owner = Handler.GetUObject();
	Entry.bHasOwner = Entry.Owner.IsValid();
	if (Menu.IsValid())
	{
		Menu->RebuildCheats();
	}
}

void UEDbgSubsystem::UnregisterCheat(FName CheatName)
{
	Cheats.RemoveAll([CheatName](const FCheatEntry& Entry) { return Entry.Info.Name == CheatName; });
}

void UEDbgSubsystem::PruneDeadCheats()
{
	Cheats.RemoveAll([](const FCheatEntry& Entry) { return Entry.bHasOwner && !Entry.Owner.IsValid(); });
}

const UEDbgSubsystem::FCheatEntry* UEDbgSubsystem::FindCheat(FName CheatName) const
{
	const FCheatEntry* Local = Cheats.FindByPredicate([CheatName](const FCheatEntry& Entry)
	{
		return Entry.Info.Name == CheatName && (!Entry.bHasOwner || Entry.Owner.IsValid());
	});
	return Local ? Local : GlobalCheats().FindByPredicate([CheatName](const FCheatEntry& Entry) { return Entry.Info.Name == CheatName; });
}

bool UEDbgSubsystem::HasCheat(FName CheatName) const
{
	return FindCheat(CheatName) != nullptr;
}

TArray<FEDbgCheatInfo> UEDbgSubsystem::GetCheats() const
{
	TArray<FEDbgCheatInfo> Result;
	for (const FCheatEntry& Entry : Cheats)
	{
		if (!Entry.bHasOwner || Entry.Owner.IsValid())
		{
			Result.Add(Entry.Info);
		}
	}
	for (const FCheatEntry& Entry : GlobalCheats())
	{
		if (!Result.ContainsByPredicate([&Entry](const FEDbgCheatInfo& Info) { return Info.Name == Entry.Info.Name; }))
		{
			Result.Add(Entry.Info);
		}
	}
	Result.Sort([](const FEDbgCheatInfo& A, const FEDbgCheatInfo& B)
	{
		return A.Category != B.Category ? A.Category.LexicalLess(B.Category) : A.Name.LexicalLess(B.Name);
	});
	return Result;
}

bool UEDbgSubsystem::RunCheat(APlayerController* Player, FName CheatName, const TArray<FString>& Arguments)
{
#if EXTENDED_DEBUG_ENABLED
	const FCheatEntry* Entry = FindCheat(CheatName);
	if (!Entry)
	{
		return false;
	}

	if (Entry->Info.bRunOnServer && Player && Player->GetNetMode() == NM_Client)
	{
		UEDbgCheatComponent* Carrier = Player->FindComponentByClass<UEDbgCheatComponent>();
		if (!Carrier)
		{
			UE_LOG(LogExtendedDebug, Warning, TEXT("%s runs on the server, but this player has no cheat component yet. Is Extended Debug enabled on the server?"), *CheatName.ToString());
			return false;
		}
		UE_LOG(LogExtendedDebug, Log, TEXT("Cheat %s: sent to the server."), *CheatName.ToString());
		Carrier->ServerRunCheat(CheatName, Arguments);
		return true;
	}
	return ExecuteCheatHere(Player, CheatName, Arguments);
#else
	return false;
#endif
}

bool UEDbgSubsystem::ExecuteCheatHere(APlayerController* Player, FName CheatName, const TArray<FString>& Arguments)
{
#if EXTENDED_DEBUG_ENABLED
	PruneDeadCheats();
	const FCheatEntry* Found = FindCheat(CheatName);
	if (!Found)
	{
		UE_LOG(LogExtendedDebug, Warning, TEXT("There is no cheat named %s here."), *CheatName.ToString());
		return false;
	}

	// Copy: the handler may register or unregister cheats.
	const FCheatEntry Entry = *Found;
	UE_LOG(LogExtendedDebug, Log, TEXT("Cheat %s %s (player %s)"), *CheatName.ToString(), *FString::Join(Arguments, TEXT(" ")), *GetNameSafe(Player));

	if (Entry.Handler)
	{
		FEDbgCheatContext Context;
		Context.Player = Player;
		Context.World = Player ? Player->GetWorld() : (GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr);
		Context.Arguments = Arguments;
		Entry.Handler(Context);
	}
	else
	{
		Entry.BlueprintHandler.ExecuteIfBound(Player, Arguments);
	}

	// Playtest data where cheats were used can then be filtered out.
	if (UEFAnalyticsSubsystem* Analytics = GetGameInstance() ? GetGameInstance()->GetSubsystem<UEFAnalyticsSubsystem>() : nullptr)
	{
		TMap<FString, FString> Attributes;
		Attributes.Add(TEXT("cheat"), CheatName.ToString());
		Analytics->RecordEvent(TEXT("debug_cheat"), Attributes);
	}
	return true;
#else
	return false;
#endif
}

void UEDbgSubsystem::HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer)
{
	if (!NewPlayer || !NewPlayer->HasAuthority() || NewPlayer->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	if (!NewPlayer->FindComponentByClass<UEDbgCheatComponent>())
	{
		// Replicated, so the player's client gets it too and can send server cheats through it.
		UEDbgCheatComponent* Carrier = NewObject<UEDbgCheatComponent>(NewPlayer, TEXT("ExtendedDebugCheats"));
		Carrier->RegisterComponent();
	}
}

// Watches

void UEDbgSubsystem::SetWatch(const FString& Key, const FString& Value)
{
	if (Key.IsEmpty())
	{
		return;
	}
	FEDbgWatch& Watch = Watches.FindOrAdd(Key);
	Watch.Value = Value;
	Watch.UpdatedSeconds = FPlatformTime::Seconds();
}

void UEDbgSubsystem::ClearWatch(const FString& Key)
{
	Watches.Remove(Key);
}

void UEDbgSubsystem::ClearAllWatches()
{
	Watches.Reset();
}

void UEDbgSubsystem::DrawWatches(UCanvas* Canvas, APlayerController* PlayerController)
{
	if (!Canvas || !bWatchesVisible || Watches.Num() == 0)
	{
		return;
	}
	if (PlayerController && PlayerController->GetGameInstance() != GetGameInstance())
	{
		return;  // another game instance's viewport (PIE with several clients)
	}

	const float Timeout = UEDbgSettings::Get()->WatchTimeoutSeconds;
	if (Timeout > 0.f)
	{
		const double Oldest = FPlatformTime::Seconds() - Timeout;
		for (auto It = Watches.CreateIterator(); It; ++It)
		{
			if (It.Value().UpdatedSeconds < Oldest)
			{
				It.RemoveCurrent();
			}
		}
	}

	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
	if (!Font || Watches.Num() == 0)
	{
		return;
	}

	TArray<FString> Keys;
	Watches.GetKeys(Keys);
	Keys.Sort();

	const float X = FMath::Max(Canvas->ClipX - 420.f, 10.f);
	float Y = 60.f;
	Canvas->SetDrawColor(FColor(255, 210, 110));
	Canvas->DrawText(Font, TEXT("Watches"), X, Y);
	Y += 14.f;
	Canvas->SetDrawColor(FColor::White);
	for (const FString& Key : Keys)
	{
		Canvas->DrawText(Font, FString::Printf(TEXT("%s = %s"), *Key, *Watches.FindChecked(Key).Value), X, Y);
		Y += 12.f;
	}
}

// Menu

void UEDbgSubsystem::OpenMenu()
{
#if EXTENDED_DEBUG_ENABLED
	if (MenuRoot.IsValid())
	{
		return;
	}
	UGameInstance* GameInstance = GetGameInstance();
	UGameViewportClient* Viewport = GameInstance ? GameInstance->GetGameViewportClient() : nullptr;
	if (!Viewport)
	{
		UE_LOG(LogExtendedDebug, Log, TEXT("The debug menu needs a game viewport."));
		return;
	}

	const TSharedRef<SEDbgMenu> NewMenu = SNew(SEDbgMenu).Subsystem(this);
	Menu = NewMenu;
	MenuRoot = SNew(SBox)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.Padding(FMargin(24.f))
		[
			NewMenu
		];
	Viewport->AddViewportWidgetContent(MenuRoot.ToSharedRef(), 10000);

	const TSharedPtr<SWidget> Focus = NewMenu->GetInitialFocus().IsValid() ? NewMenu->GetInitialFocus() : TSharedPtr<SWidget>(NewMenu);
	bMenuShowedCursor = false;
	if (APlayerController* Player = GameInstance->GetFirstLocalPlayerController())
	{
		bMenuShowedCursor = !Player->ShouldShowMouseCursor();
		Player->SetShowMouseCursor(true);
		FInputModeGameAndUI Mode;
		Mode.SetWidgetToFocus(Focus);
		Mode.SetHideCursorDuringCapture(false);
		Player->SetInputMode(Mode);
	}
	else if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetKeyboardFocus(Focus, EFocusCause::SetDirectly);
	}
#endif
}

void UEDbgSubsystem::CloseMenu()
{
	if (!MenuRoot.IsValid())
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	if (UGameViewportClient* Viewport = GameInstance ? GameInstance->GetGameViewportClient() : nullptr)
	{
		Viewport->RemoveViewportWidgetContent(MenuRoot.ToSharedRef());
	}
	MenuRoot.Reset();
	Menu.Reset();

	if (bMenuShowedCursor)
	{
		// Only undo what opening the menu changed.
		if (APlayerController* Player = GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr)
		{
			Player->SetShowMouseCursor(false);
			Player->SetInputMode(FInputModeGameOnly());
		}
		bMenuShowedCursor = false;
	}
}

void UEDbgSubsystem::ToggleMenu()
{
	IsMenuOpen() ? CloseMenu() : OpenMenu();
}
