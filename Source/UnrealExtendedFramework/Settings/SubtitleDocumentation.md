# EF Modular Subtitle System — Technical Specification

## 1. Design Goals

| Priority | Goal |
|----------|------|
| 🎯 | **Modular** — each concern (data, scheduling, display, audio, settings) is a replaceable layer |
| 🎯 | **Multiplayer-first** — per-player UI; server-authoritative triggering; proximity-aware |
| 🎯 | **Easy to use** — one-liner `ExecuteSubtitle(Key)` from BP/C++; DataTable authoring |
| ⭐ | **Accessibility** — text size, color, background, speaker labels, closed-caption icons |
| ⭐ | **Extensible** — queue strategies, custom display widgets, dialogue integration hooks |

---

## 2. Architecture Overview

```
┌──────────────────────────────────────────────────────────────┐
│                     Server / Authority                        │
│                                                              │
│  Gameplay Code ──► UEFSubtitleSubsystem::RequestSubtitle()   │
│                         │                                    │
│                         ▼                                    │
│              FEFSubtitleRequest (NetSerialize)                │
│                         │                                    │
│         ┌───────────────┼────────────────┐                   │
│         ▼ Multicast RPC ▼ Relevancy Check▼                   │
│      Client A        Client B        Client C                │
└──────┬───────────────┬───────────────┬───────────────────────┘
       │               │               │
       ▼               ▼               ▼
┌──────────────────────────────────────────────────────────────┐
│                  Per-Client / Local Player                    │
│                                                              │
│  UEFSubtitleLocalSubsystem (ULocalPlayerSubsystem)           │
│       │                                                      │
│       ├── FEFSubtitleQueue (scheduling + priority)           │
│       │       │                                              │
│       │       ▼                                              │
│       ├── UEFSubtitleDisplayWidget (visual rendering)        │
│       │       ├── Speaker Label                              │
│       │       ├── Subtitle Text (rich text + typewriter)     │
│       │       └── Background / Border                        │
│       │                                                      │
│       └── UEFSubtitleAudioPlayer (2D / 3D / attenuation)    │
│                                                              │
│  UEFSubtitleSettings (UDeveloperSettings — project-wide)     │
│  UEFSubtitleUserSettings (per-player prefs — ModularSettings)│
└──────────────────────────────────────────────────────────────┘
```

### Layer Breakdown

| Layer | Class | Subsystem Type | Role |
|-------|-------|----------------|------|
| **Authority** | `UEFSubtitleSubsystem` | `UGameInstanceSubsystem` | Validate & dispatch requests. Multicast to relevant clients. |
| **Local Player** | `UEFSubtitleLocalSubsystem` | `ULocalPlayerSubsystem` | Queue management, widget lifecycle, audio playback per-player. |
| **Data** | `FEFSubtitleEntry` / `UEFSubtitleDataAsset` | Struct / DataAsset | Subtitle content, speaker info, audio, culture variants. |
| **Display** | `UEFSubtitleDisplayWidget` | `UUserWidget` | Renders active subtitle(s). Swappable via settings. |
| **Audio** | `UEFSubtitleAudioPlayer` | Component on widget or subsystem | Plays voiceover with 2D/3D/attenuation. |
| **Settings (Project)** | `UEFSubtitleProjectSettings` | `UDeveloperSettings` | Default widget class, default DataTable, global appearance. |
| **Settings (User)** | `UEFSubtitleUserSettings` | `UEFModularSettingsBase` derived | Per-player: enabled, text size, background opacity, etc. |

---

## 3. Data Layer

### 3.1 `FEFSubtitleEntry` (DataTable Row / DataAsset member)

```cpp
USTRUCT(BlueprintType)
struct FEFSubtitleEntry : public FTableRowBase
{
    GENERATED_BODY()

    // ── Content ──
    
    // The subtitle text. Supports UE RichTextBlock markup: <Bold>, <Italic>, <Color>
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Content")
    FText Text;

    // Optional speaker / character name (displayed as label above subtitle)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Content")
    FText SpeakerName;

    // Optional speaker color for the label
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Content")
    FLinearColor SpeakerColor = FLinearColor::White;

    // GameplayTag for the speaker — used for icon lookup, color theming, filtering
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Content")
    FGameplayTag SpeakerTag;

    // ── Timing ──
    
    // Duration in seconds. 0 = auto-calculate from text length.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing")
    float Duration = 0.0f;

    // Delay before showing (useful for syncing with animation / audio)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing")
    float Delay = 0.0f;

    // ── Audio ──
    
    // Default voiceover sound
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
    TSoftObjectPtr<USoundBase> VoiceSound;

    // Per-culture overrides: Key = culture code ("en", "fr", "tr"), Value = sound
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
    TMap<FString, TSoftObjectPtr<USoundBase>> CultureSoundMap;

    // ── Priority & Behavior ──
    
    // Higher priority subtitles can interrupt lower ones
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior")
    int32 Priority = 0;

    // If true, this subtitle cannot be interrupted by equal-priority subs
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior")
    bool bUninterruptible = false;
};
```

### 3.2 `UEFSubtitleDataAsset` (Alternative to DataTable)

For larger projects, DataAssets provide better modularity than a single monolithic DataTable:

```cpp
UCLASS(BlueprintType)
class UEFSubtitleDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TMap<FName, FEFSubtitleEntry> Entries;

    // GameplayTag category for this asset (e.g. Dialogue.Shop, Dialogue.Quest)
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGameplayTag Category;
};
```

### 3.3 Data Source Resolution

The subsystem resolves subtitle keys in this order:
1. **Registered DataAssets** (via `RegisterSubtitleSource`)
2. **Active DataTable** (set in project settings or at runtime)
3. **Fallback** — log warning + return empty subtitle

This allows Game Feature Plugins to register their own subtitle data without modifying the core DataTable.

---

## 4. Request & Dispatch (Authority Layer)

### 4.1 `FEFSubtitleRequest`

```cpp
USTRUCT(BlueprintType)
struct FEFSubtitleRequest
{
    GENERATED_BODY()

    // The key to look up in the data source
    UPROPERTY(BlueprintReadWrite)
    FName SubtitleKey;

    // Which data source to use (empty = search all registered)
    UPROPERTY(BlueprintReadWrite)
    TSoftObjectPtr<UEFSubtitleDataAsset> DataAssetOverride;

    // Execution type
    UPROPERTY(BlueprintReadWrite)
    EEFSubtitleExecutionType ExecutionType = EEFSubtitleExecutionType::Boundless;

    // World location (for Location/AttachedToActor types)
    UPROPERTY(BlueprintReadWrite)
    FVector WorldLocation = FVector::ZeroVector;

    // Actor to attach audio to (for AttachedToActor type)
    UPROPERTY(BlueprintReadWrite)
    TWeakObjectPtr<AActor> AttachActor;

    // Max audible distance — clients beyond this won't receive the subtitle
    // 0 = infinite (all clients receive it)
    UPROPERTY(BlueprintReadWrite)
    float MaxDistance = 0.0f;

    // The player controller that triggered this (filled by subsystem)
    UPROPERTY()
    APlayerController* Instigator = nullptr;

    // Unique ID for tracking / cancellation
    UPROPERTY()
    int32 RequestId = 0;
};
```

### 4.2 Execution Types

```cpp
UENUM(BlueprintType)
enum class EEFSubtitleExecutionType : uint8
{
    // No spatial audio — played as 2D for all (or filtered) clients
    Boundless,

    // Audio is spatialized at a world location, subtitle shown to nearby players
    Location,

    // Audio attached to an actor, subtitle follows the actor's position
    AttachedToActor,

    // Only shown to a specific player (e.g., inner monologue, tutorial prompts)
    PlayerOnly
};
```

### 4.3 `UEFSubtitleSubsystem` (GameInstanceSubsystem)

Authority-side responsibilities:

```cpp
UCLASS()
class UEFSubtitleSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:

    // ── One-liner API (simplest usage) ──

    // Play subtitle by key — searches all registered data sources
    UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject"))
    void ExecuteSubtitle(const UObject* WorldContextObject, FName SubtitleKey);

    // Play subtitle at a world location (3D audio + distance filtering)
    UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject"))
    void ExecuteSubtitleAtLocation(const UObject* WorldContextObject,
        FName SubtitleKey, FVector Location, float MaxDistance = 3000.0f);

    // Play subtitle attached to an actor
    UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject"))
    void ExecuteSubtitleAttached(const UObject* WorldContextObject,
        FName SubtitleKey, AActor* AttachActor, float MaxDistance = 3000.0f);

    // Play subtitle only for a specific player
    UFUNCTION(BlueprintCallable)
    void ExecuteSubtitleForPlayer(APlayerController* Player, FName SubtitleKey);

    // ── Advanced API ──

    // Full control over the request
    UFUNCTION(BlueprintCallable)
    int32 RequestSubtitle(const FEFSubtitleRequest& Request);

    // Cancel an active / queued subtitle by ID
    UFUNCTION(BlueprintCallable)
    void CancelSubtitle(int32 RequestId);

    // Cancel all subtitles matching a speaker tag
    UFUNCTION(BlueprintCallable)
    void CancelSubtitlesBySpeaker(FGameplayTag SpeakerTag);

    // ── Data Source Registration ──

    UFUNCTION(BlueprintCallable)
    void RegisterSubtitleSource(UEFSubtitleDataAsset* DataAsset);

    UFUNCTION(BlueprintCallable)
    void UnregisterSubtitleSource(UEFSubtitleDataAsset* DataAsset);

    UFUNCTION(BlueprintCallable)
    void SetActiveDataTable(UDataTable* DataTable);

    // ── Delegates ──

    UPROPERTY(BlueprintAssignable)
    FOnSubtitleExecuted OnSubtitleExecuted;    // (FEFSubtitleEntry, int32 RequestId)

    UPROPERTY(BlueprintAssignable)
    FOnSubtitleFinished OnSubtitleFinished;    // (int32 RequestId)
};
```

### 4.4 Multiplayer Flow

```
 Server                          Client (Local Player)
   │                                    │
   │  ExecuteSubtitleAtLocation(Key, Loc, 3000)
   │         │                          │
   │    [Resolve subtitle data]         │
   │    [For each PlayerController:]    │
   │      [Distance check]             │
   │         │                          │
   │── Client RPC ──────────────────────►│
   │   (FEFSubtitleRequest + resolved   │
   │    FEFSubtitleEntry)               │
   │                             [UEFSubtitleLocalSubsystem]
   │                                ├── Enqueue
   │                                ├── Check user prefs (enabled? text size?)
   │                                ├── Play audio
   │                                └── Show widget
```

**Key design choices:**
- The **server resolves** the subtitle data and sends the full `FEFSubtitleEntry` to each client. This avoids requiring clients to have loaded the DataTable/DataAsset.
- **Distance filtering** happens server-side. Only relevant clients receive the RPC.
- **Listen Server** clients are treated identically to remote clients (subtitle goes through the local subsystem).
- **`PlayerOnly`** type uses a targeted Client RPC.

---

## 5. Local Player Layer (Client-Side)

### 5.1 `UEFSubtitleLocalSubsystem` (ULocalPlayerSubsystem)

Each local player has their own subtitle subsystem managing their queue and widget:

```cpp
UCLASS()
class UEFSubtitleLocalSubsystem : public ULocalPlayerSubsystem
{
    GENERATED_BODY()
public:

    // Called by the authority subsystem (via RPC on PC or directly in standalone)
    void ReceiveSubtitle(const FEFSubtitleEntry& Entry, const FEFSubtitleRequest& Request);

    // Cancel by request ID
    void CancelSubtitle(int32 RequestId);

    // Clear all active and queued subtitles
    UFUNCTION(BlueprintCallable)
    void ClearAllSubtitles();

    // ── User Settings Access ──
    UFUNCTION(BlueprintPure)
    bool AreSubtitlesEnabled() const;

    UFUNCTION(BlueprintPure)
    float GetSubtitleTextScale() const;

private:

    // Manages ordering, interruption, expiration
    FEFSubtitleQueue SubtitleQueue;

    // The active display widget (pooled — created once, reused)
    UPROPERTY()
    UEFSubtitleDisplayWidget* DisplayWidget;

    // Ensures the display widget exists and is in the viewport
    void EnsureWidgetReady();

    // Called by the queue when the active subtitle changes
    void OnActiveSubtitleChanged(const FEFActiveSubtitle& Active);

    // Called when the active subtitle expires
    void OnSubtitleExpired(int32 RequestId);
};
```

### 5.2 `FEFSubtitleQueue`

The queue controls which subtitle is visible and how new ones interact with existing ones:

```cpp
struct FEFActiveSubtitle
{
    FEFSubtitleEntry Entry;
    FEFSubtitleRequest Request;
    float RemainingTime;
    float ElapsedTime;
    int32 RequestId;
};

UENUM(BlueprintType)
enum class EEFSubtitleQueueMode : uint8
{
    // New subtitle replaces current one immediately
    Replace,

    // New subtitle is queued; plays after current finishes
    Queue,

    // New subtitle is queued; higher priority interrupts lower
    PriorityQueue,

    // Multiple subtitles can display simultaneously (stacked)
    Stack
};
```

**Queue rules:**
1. `bUninterruptible` entries can only be interrupted by strictly higher priority.
2. In `Stack` mode, max visible count is configurable (default 3).
3. Expired subtitles are auto-removed via Timer.
4. `CancelSubtitle(Id)` removes from active or pending.

---

## 6. Display Widget

### 6.1 `UEFSubtitleDisplayWidget`

The widget is **swappable** — set the class in `UEFSubtitleProjectSettings::SubtitleWidgetClass`. The base provides:

```cpp
UCLASS(Blueprintable)
class UEFSubtitleDisplayWidget : public UUserWidget
{
    GENERATED_BODY()
public:

    // ── Required BindWidgets (C++ base) ──
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    URichTextBlock* SubtitleText;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UTextBlock* SpeakerLabel;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UImage* SpeakerIcon;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UBorder* BackgroundBorder;

    // ── API ──

    // Show a subtitle (handles animation, typewriter, etc.)
    UFUNCTION(BlueprintNativeEvent)
    void ShowSubtitle(const FEFSubtitleEntry& Entry, const FEFSubtitleRequest& Request);

    // Hide the subtitle (with optional fade-out)
    UFUNCTION(BlueprintNativeEvent)
    void HideSubtitle();

    // Update the subtitle progress (called each tick while active)
    UFUNCTION(BlueprintNativeEvent)
    void UpdateSubtitle(float DeltaTime, float Progress);

    // Apply visual settings from project + user prefs
    UFUNCTION(BlueprintCallable)
    void ApplyVisualSettings();

    // ── Stacking Support ──

    // For Stack queue mode: add an additional subtitle line
    UFUNCTION(BlueprintNativeEvent)
    void AddStackedSubtitle(const FEFSubtitleEntry& Entry, int32 RequestId);

    // Remove a specific stacked subtitle
    UFUNCTION(BlueprintNativeEvent)
    void RemoveStackedSubtitle(int32 RequestId);
};
```

### 6.2 Visual Features

| Feature | Description | Driven By |
|---------|-------------|-----------|
| **Typewriter** | Per-letter reveal animation | `DurationSettings.AnimateSubtitleLetters` |
| **Speaker Label** | Colored name above text | `FEFSubtitleEntry::SpeakerName/SpeakerColor` |
| **Speaker Icon** | Small icon next to speaker name | Looked up via `SpeakerTag` in icon DataTable |
| **Rich Text** | `<Bold>`, `<Italic>`, `<Color=#FF0000>` | Native URichTextBlock support |
| **Background** | Semi-transparent background | `BackgroundSettings` |
| **Border** | Stylized border | `BorderSettings` |
| **Fade In/Out** | Smooth opacity transitions | Widget animation (BlueprintImplementable) |
| **Text Size Scale** | User accessibility preference | `UEFSubtitleUserSettings` |

---

## 7. Audio Playback

### 7.1 Strategy

Audio playback is handled by `UEFSubtitleLocalSubsystem` directly — no separate component needed:

```cpp
// Pseudocode inside UEFSubtitleLocalSubsystem::ReceiveSubtitle

USoundBase* Sound = ResolveCultureSound(Entry);
if (!Sound) return;  // No audio for this subtitle

switch (Request.ExecutionType)
{
    case Boundless:
    case PlayerOnly:
        // 2D audio on the local player
        UGameplayStatics::PlaySound2D(this, Sound);
        break;

    case Location:
        // 3D audio at world position
        UGameplayStatics::PlaySoundAtLocation(this, Sound, Request.WorldLocation);
        break;

    case AttachedToActor:
        // 3D audio following the actor
        if (Request.AttachActor.IsValid())
            UGameplayStatics::SpawnSoundAttached(Sound, 
                Request.AttachActor->GetRootComponent());
        break;
}
```

### 7.2 Culture Resolution

```
1. Get current culture: FInternationalization::Get().GetCurrentCulture()->GetName()
2. Check CultureSoundMap for exact match (e.g., "en-US")
3. Check CultureSoundMap for language-only match (e.g., "en")
4. Fallback to Entry.VoiceSound
5. Return nullptr if all empty (silent subtitle)
```

---

## 8. Settings Integration

### 8.1 Project Settings (`UEFSubtitleProjectSettings` — DeveloperSettings)

```cpp
UCLASS(Config=Game, defaultconfig, meta=(DisplayName="EF Subtitle Settings"))
class UEFSubtitleProjectSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    // Widget class to instantiate for subtitle display
    UPROPERTY(EditAnywhere, Category = "Widget")
    TSubclassOf<UEFSubtitleDisplayWidget> SubtitleWidgetClass;

    // Default subtitle data sources
    UPROPERTY(EditAnywhere, Category = "Data")
    TSoftObjectPtr<UDataTable> DefaultDataTable;

    UPROPERTY(EditAnywhere, Category = "Data")
    TArray<TSoftObjectPtr<UEFSubtitleDataAsset>> DefaultDataAssets;

    // Default queue mode
    UPROPERTY(EditAnywhere, Category = "Behavior")
    EEFSubtitleQueueMode DefaultQueueMode = EEFSubtitleQueueMode::Replace;

    // Max simultaneous subtitles in Stack mode
    UPROPERTY(EditAnywhere, Category = "Behavior",
        meta=(EditCondition="DefaultQueueMode==EEFSubtitleQueueMode::Stack"))
    int32 MaxStackedSubtitles = 3;

    // Appearance defaults
    UPROPERTY(EditAnywhere, Category = "Appearance")
    FSlateFontInfo DefaultFont;

    UPROPERTY(EditAnywhere, Category = "Appearance")
    FLinearColor DefaultFontColor = FLinearColor::White;

    UPROPERTY(EditAnywhere, Category = "Appearance")
    FExtendedSubtitleDurationSettings DurationSettings;

    UPROPERTY(EditAnywhere, Category = "Appearance")
    FExtendedSubtitleBorderSettings BorderSettings;

    UPROPERTY(EditAnywhere, Category = "Appearance")
    FExtendedSubtitleBackgroundSettings BackgroundSettings;
};
```

### 8.2 User Settings (via ModularSettings)

These inherit from the existing ModularSettings base classes to integrate with the settings UI:

| Setting | Base Class | Tag | Default |
|---------|------------|-----|---------|
| Subtitles Enabled | `UEFModularSettingsBool` | `Settings.Accessibility.SubtitlesEnabled` | `true` |
| Subtitle Text Size | `UEFModularSettingsFloat` | `Settings.Accessibility.SubtitleTextSize` | `1.0` |
| Background Opacity | `UEFModularSettingsFloat` | `Settings.Accessibility.SubtitleBgOpacity` | `0.7` |
| Speaker Labels | `UEFModularSettingsBool` | `Settings.Accessibility.SubtitleSpeakerLabels` | `true` |

---

## 9. Multiplayer Deep Dive

### 9.1 Network Architecture

```
┌───────── Dedicated Server ──────────┐
│  UEFSubtitleSubsystem (Authority)   │
│   - Owns subtitle data sources      │
│   - Resolves FEFSubtitleEntry       │
│   - Filters by distance per-player  │
│   - Sends Client RPC to relevant PC │
└─────────────────────────────────────┘
          │               │
    ClientRPC        ClientRPC
          │               │
┌─────────▼───┐   ┌──────▼──────┐
│   Client A  │   │  Client B   │
│ UEFSubtitle │   │ UEFSubtitle │
│ LocalSub.   │   │ LocalSub.   │
│  - Queue    │   │  - Queue    │
│  - Widget   │   │  - Widget   │
│  - Audio    │   │  - Audio    │
└─────────────┘   └─────────────┘
```

### 9.2 RPC Design

Subtitles are dispatched through a lightweight **Component on PlayerController** to enable Client RPCs:

```cpp
UCLASS()
class UEFSubtitleReceiverComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    // Server -> Client: deliver a resolved subtitle
    UFUNCTION(Client, Reliable)
    void ClientReceiveSubtitle(FEFSubtitleEntry Entry, FEFSubtitleRequest Request);

    // Server -> Client: cancel a subtitle
    UFUNCTION(Client, Reliable)
    void ClientCancelSubtitle(int32 RequestId);
};
```

**Why a Component on PC?**
- `ULocalPlayerSubsystem` cannot receive RPCs directly.
- The component is auto-added to each `APlayerController` in `UEFSubtitleSubsystem::Initialize`.
- The component simply forwards calls to the local `UEFSubtitleLocalSubsystem`.

### 9.3 Listen Server Handling

On a listen server, the host's `APlayerController` has the same component. When dispatching:
- **Remote clients** → `ClientReceiveSubtitle` RPC
- **Listen server host** → Direct call to `UEFSubtitleLocalSubsystem::ReceiveSubtitle` (skip RPC)

### 9.4 Standalone / Single-Player

In standalone mode, the `UEFSubtitleSubsystem` calls the local subsystem directly — no RPCs, no components. The API is identical.

---

## 10. Sequence Subtitles (Dialogue Chains)

For dialogue sequences where multiple subtitles need to play in order:

```cpp
USTRUCT(BlueprintType)
struct FEFSubtitleSequence
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FName> SubtitleKeys;

    // If true, wait for audio to finish before showing the next subtitle
    // If false, use each entry's Duration field
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bWaitForAudio = true;
};
```

```cpp
// API on UEFSubtitleSubsystem:
UFUNCTION(BlueprintCallable)
int32 ExecuteSubtitleSequence(const FEFSubtitleSequence& Sequence);

UFUNCTION(BlueprintCallable)
void CancelSubtitleSequence(int32 SequenceId);

UPROPERTY(BlueprintAssignable)
FOnSubtitleSequenceFinished OnSubtitleSequenceFinished;  // (int32 SequenceId)
```

---

## 11. Integration Hooks

### 11.1 Dialogue System Integration

If using a dialogue system (e.g., `NotYetDlgSystem`), the subtitle subsystem provides hooks:

```cpp
// Called by dialogue system to push subtitles
UFUNCTION(BlueprintCallable, Category = "Subtitle|Dialogue")
void ExecuteDialogueSubtitle(FName SubtitleKey, AActor* Speaker, 
    const TArray<APlayerController*>& Listeners);
```

### 11.2 Closed Captions / Sound Effects

```cpp
USTRUCT()
struct FEFClosedCaptionEntry : public FEFSubtitleEntry
{
    GENERATED_BODY()

    // Icon displayed instead of or alongside the text
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSoftObjectPtr<UTexture2D> CaptionIcon;

    // Category tag for filtering (e.g., "CC.Footsteps", "CC.Explosion")
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGameplayTag CaptionCategory;
};
```

### 11.3 Event-Driven Hooks

```cpp
// Delegates fired at key lifecycle points:
FOnSubtitleQueued         // Subtitle entered the queue
FOnSubtitleStarted        // Subtitle became active (visible)
FOnSubtitleFinished       // Subtitle duration expired
FOnSubtitleCancelled      // Subtitle was cancelled
FOnSubtitleSequenceStep   // Next subtitle in a sequence started (Index, Total)
```

---

## 12. File & Folder Structure

```
Settings/Subtitle/
├── Data/
│   ├── EFSubtitleData.h              // FEFSubtitleEntry, enums, visual settings structs
│   ├── EFSubtitleDataAsset.h/.cpp    // UEFSubtitleDataAsset (DataAsset alternative)
│   └── EFSubtitleProjectSettings.h/.cpp  // UEFSubtitleProjectSettings (DeveloperSettings)
├── Subsystem/
│   ├── EFSubtitleSubsystem.h/.cpp          // Authority subsystem (GameInstance)
│   ├── EFSubtitleLocalSubsystem.h/.cpp     // Per-player subsystem (LocalPlayer)
│   ├── EFSubtitleQueue.h/.cpp              // Queue logic (priority, stack, replace)
│   └── EFSubtitleReceiverComponent.h/.cpp  // RPC bridge on PlayerController
├── Widget/
│   └── EFSubtitleDisplayWidget.h/.cpp      // Base display widget (swappable)
└── UserSettings/
    └── EFSubtitleUserSettings.h            // ModularSettings-derived per-player prefs
```

---

## 13. Migration from Current System

| Current | New | Notes |
|---------|-----|-------|
| `FExtendedSubtitle` | `FEFSubtitleEntry` | Add speaker, priority, delay fields; keep DataTable compat |
| `UEFSubtitleSubsystem` (does everything) | Split into Authority + Local | Authority dispatches, Local renders |
| `UEFSubtitleWidget` | `UEFSubtitleDisplayWidget` | RichTextBlock, speaker label, stacking |
| `UEFSubtitleSettings` | `UEFSubtitleProjectSettings` + `UEFSubtitleUserSettings` | Separate project-wide vs per-player |
| Direct widget creation in subsystem | Widget lifecycle in LocalSubsystem | Proper per-player widget management |
| `ExecuteExtendedSubtitle(Key)` | `ExecuteSubtitle(Key)` | API simplified; old name deprecated |

### Backward Compatibility

- Existing DataTables with `FExtendedSubtitle` rows will continue to work — `FEFSubtitleEntry` extends the same base with additional optional fields.
- A thin wrapper `ExecuteExtendedSubtitle()` will remain as a `DEPRECATED` redirect to `ExecuteSubtitle()`.

---

## 14. Usage Examples

### Simplest (Blueprint One-Liner)

```
Get EFSubtitleSubsystem → Execute Subtitle (Key: "Shopkeeper_Greeting")
```

### Spatial Subtitle (NPC speaks in world)

```
Get EFSubtitleSubsystem → Execute Subtitle At Location
    Key: "Guard_Warning"
    Location: NPC Actor Location
    Max Distance: 2000
```

### Player-Only (Tutorial / Inner Monologue)

```
Get EFSubtitleSubsystem → Execute Subtitle For Player
    Player: Get Player Controller (0)
    Key: "Tutorial_Welcome"
```

### Dialogue Sequence (C++)

```cpp
FEFSubtitleSequence Seq;
Seq.SubtitleKeys = { "Merchant_Hello", "Merchant_Offer", "Merchant_Goodbye" };
Seq.bWaitForAudio = true;
SubtitleSubsystem->ExecuteSubtitleSequence(Seq);
```
