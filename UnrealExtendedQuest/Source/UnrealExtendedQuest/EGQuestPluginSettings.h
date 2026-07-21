// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Misc/Build.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Templates/SubclassOf.h"
#include "GameplayTagContainer.h"

#include "Engine/DeveloperSettings.h"
#include "Layout/Margin.h"
#include "NYEngineVersionHelpers.h"

#include "Logging/IEGQuestLoggerBase.h"

#if WITH_EDITOR
#include "ClassViewerModule.h"
#endif


#include "EGQuestPluginSettings.generated.h"


// Defines the format of the Quest text
UENUM()
enum class EEGQuestGraphTextFormat : uint8
{
	// No Text Format used.
	None	UMETA(DisplayName = "No Text Format"),

	// Output all text formats, mostly used for debugging
	All	 UMETA(Hidden),

	// The JSON format.
	JSON				UMETA(DisplayName = "JSON"),

	// Hidden represents the start of the text formats index
	StartTextFormats = JSON 	UMETA(Hidden),

	// Hidden, represents the number of text formats */
	NumTextFormats 		UMETA(Hidden),
};

// Defines what key combination to press to add a new line to an FText
UENUM()
enum class EEGQuestTextInputKeyForNewLine : uint8
{
	// Press 'Enter' to add a new line.
	Enter				UMETA(DisplayName = "Enter"),

	// Preset 'Shift + Enter' to add a new line. (like in blueprints)
	ShiftPlusEnter		UMETA(DisplayName = "Shift + Enter"),
};

// Defines how the overriden namespace will be set
UENUM()
enum class EEGQuestTextNamespaceLocalization : uint8
{
	// The system does not modify the Namespace and Key values of the Text fields.
	Ignore			UMETA(DisplayName = "Ignore"),

	// The system sets the Namespace for Text fields for each quest separately. Unique keys are also generated.
	PerQuest		UMETA(DisplayName = "Namespace Per Quest (QuestName)"),

	// Same as PerQuest only we will have a prefix set
	WithPrefixPerQuest UMETA(DisplayName = "Prefix + Namespace Per Quest (Prefix.QuestName)"),

	// The system sets the Namespace for Text fields for each quest into the same value. Unique keys are also generated.
	Global				UMETA(DisplayName = "Global Namespace")
};


UENUM()
enum class EEGQuestClassPickerDisplayMode : uint8
{
	// Default will choose what view mode based on if in Viewer or Picker mode.
	DefaultView,

	// Displays all classes as a tree.
	TreeView,

	// Displays all classes as a list.
	ListView
};

// UDeveloperSettings classes are auto discovered https://wiki.unrealengine.com/CustomSettings
UCLASS(Config = Engine, DefaultConfig, meta = (DisplayName = "Quest System Settings"))
class UNREALEXTENDEDQUEST_API UEGQuestPluginSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UEGQuestPluginSettings();

	// UDeveloperSettings interface
	// Gets the settings container name for the settings, either Project or Editor
	FName GetContainerName() const override { return TEXT("Project"); }
	// Gets the category for the settings, some high level grouping like, Editor, Engine, Game...etc.
	FName GetCategoryName() const override { return TEXT("Editor"); };
	// The unique name for your section of settings, uses the class's FName.
	FName GetSectionName() const override { return Super::GetSectionName(); };

#if WITH_EDITOR
	// Gets the section text, uses the classes DisplayName by default.
	FText GetSectionText() const override;
	// Gets the description for the section, uses the classes ToolTip by default.
	FText GetSectionDescription() const override;

	// Whether or not this class supports auto registration or if the settings have a custom setup
	bool SupportsAutoRegistration() const override { return true; }

	// UObject interface
	bool CanEditChange(const FProperty* InProperty) const override;

	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

	// Own functions
#define CREATE_SETTER(_NameMethod, _VariableType, _VariableName)  \
	void _NameMethod(_VariableType InVariableValue)               \
	{                                                             \
		if (_VariableName != InVariableValue)                     \
		{                                                         \
			_VariableName = InVariableValue;                      \
			SaveSettings();                                       \
		}                                                         \
	}

	CREATE_SETTER(SetHideEmptyQuestBrowserCategories, bool, bHideEmptyQuestBrowserCategories)

#undef CREATE_SETTER

	// Depends on:
	// - LocalizationIgnoredTexts
	// - LocalizationIgnoredStrings
	bool IsIgnoredTextForLocalization(const FText& Text) const;

	// Is this text remapped
	FORCEINLINE bool IsTextRemapped(const FText& Text) const { return IsSourceStringRemapped(*FTextInspector::GetSourceString(Text));  }
	FORCEINLINE bool IsSourceStringRemapped(const FString& SourceString) const { return LocalizationRemapSourceStringsToTexts.Contains(SourceString); }
	FORCEINLINE const FText& GetTextRemappedText(const FText& Text) const { return GetSourceStringRemappedText(*FTextInspector::GetSourceString(Text)); }
	FORCEINLINE const FText& GetSourceStringRemappedText(const FString& SourceString) const { return LocalizationRemapSourceStringsToTexts.FindChecked(SourceString); }

	// Saves the settings to the config file depending on the settings of this class.
	void SaveSettings()
	{
		const UClass* ThisClass = GetClass();
		if (ThisClass->HasAnyClassFlags(CLASS_DefaultConfig))
		{
#if NY_ENGINE_VERSION >= 500
			TryUpdateDefaultConfigFile();
#else
			UpdateDefaultConfigFile();
#endif
		}
		else if (ThisClass->HasAnyClassFlags(CLASS_GlobalUserConfig))
		{
			UpdateGlobalUserConfigFile();
		}
		else
		{
			SaveConfig();
		}
	}

	// @return the extension of the text file depending on the InTextFormat.
	static FString GetTextFileExtension(EEGQuestGraphTextFormat TextFormat);
	static bool HasTextFileExtension(EEGQuestGraphTextFormat TextFormat) { return !GetTextFileExtension(TextFormat).IsEmpty(); }

	// Only the current ones from the enum
	static const TSet<FString>& GetAllCurrentTextFileExtensions();

	// GetAllCurrentTextFileExtensions() + AdditionalTextFormatFileExtensionsToLookFor
	TSet<FString> GetAllTextFileExtensions() const;

#if WITH_EDITOR
	EClassViewerDisplayMode::Type GetUnrealClassPickerDisplayMode() const
	{
		if (ClassPickerDisplayMode == EEGQuestClassPickerDisplayMode::ListView)
		{
			return EClassViewerDisplayMode::ListView;
		}
		if (ClassPickerDisplayMode == EEGQuestClassPickerDisplayMode::TreeView)
		{
			return EClassViewerDisplayMode::TreeView;
		}

		return EClassViewerDisplayMode::DefaultView;
	}
#endif // WITH_EDITOR

public:
	// If enabled this auto registers and unregisters the quest console commands on Begin Play
	// Calls RegisterQuestConsoleCommands and UnregisterQuestConsoleCommands
	UPROPERTY(Category = "Runtime", Config, EditAnywhere)
	bool bRegisterQuestConsoleCommandsAutomatically = true;

	/** Facts are server-only unless their tag matches one of these roots. Empty is the safe default. */
	UPROPERTY(Category = "Runtime|Facts", Config, EditAnywhere)
	FGameplayTagContainer ReplicatedFactRootTags;

	/** Optional compatibility bridge: each accepted quest event adds rounded magnitude to its tag fact. */
	UPROPERTY(Category = "Runtime|Facts", Config, EditAnywhere)
	bool bGameplayEventsWriteFacts = false;

	/** Enables quest completion/failure/stage history write-back when QuestWriteBackRoot is valid. */
	UPROPERTY(Category = "Runtime|Facts", Config, EditAnywhere)
	bool bQuestLifecycleWritesFacts = false;

	/** Declared root for write-back tags. Projects must register the resulting child tags. */
	UPROPERTY(Category = "Runtime|Facts", Config, EditAnywhere)
	FGameplayTag QuestWriteBackRoot;

	// The quest text format used for saving and reloading from text files.
	UPROPERTY(Category = "Quest", Config, EditAnywhere, DisplayName = "Text Format")
	EEGQuestGraphTextFormat QuestTextFormat = EEGQuestGraphTextFormat::None;

	// What key combination to press to add a new line for FText fields in the Quest Editor.
	UPROPERTY(Category = "Quest", Config, EditAnywhere, DisplayName = "Text Input Key for NewLine")
	EEGQuestTextInputKeyForNewLine QuestTextInputKeyForNewLine = EEGQuestTextInputKeyForNewLine::Enter;


	// Any properties that belong to these classes won't be shown in the suggestion list when you use the reflection system (class variables).
	UPROPERTY(Category = "Quest", Config, EditAnywhere)
	TArray<UClass*> BlacklistedReflectionClasses;


	// How the Blueprint class pricker looks like
	UPROPERTY(Category = "Blueprint", Config, EditAnywhere)
	EEGQuestClassPickerDisplayMode ClassPickerDisplayMode = EEGQuestClassPickerDisplayMode::DefaultView;

	// Should we only process batch quests that are only in the /Game folder.
	// This is used for saving all quests or deleting all text files.
	UPROPERTY(Category = "Batch", Config, EditAnywhere)
	bool bBatchOnlyInGameQuests = true;

	// Additional file extension to look for when doing operations with quest text formats, like: deleting/renaming.
	// NOTE: file extensions must start with a full stop
	UPROPERTY(Category = "Batch", Config, EditAnywhere)
	TSet<FString> AdditionalTextFormatFileExtensionsToLookFor;


	// Defines what the system should do with Text Namespaces for localization
	UPROPERTY(Category = "Localization", Config, EditAnywhere, DisplayName = "Text Namespace")
	EEGQuestTextNamespaceLocalization QuestTextNamespaceLocalization = EEGQuestTextNamespaceLocalization::Ignore;

	// Depending on TextLocalizationMode it can be used as the namespace for all quest
	// Only used for GlobalNamespace
	UPROPERTY(Category = "Localization", Config, EditAnywhere, DisplayName = "Text Global Namespace Name")
	FString QuestTextGlobalNamespaceName = "Quest";

	// Depending on TextLocalizationMode it can be used as the prefix for all quests namespace name
	// Only used for WithPrefixPerQuest
	UPROPERTY(Category = "Localization", Config, EditAnywhere, DisplayName = "Text Namespace Name Prefix")
	FString QuestTextPrefixNamespaceName = "Quest_";

	// Additional Array of texts that this system won't overwrite the namespace or key for
	//UPROPERTY(Category = "Localization", Config, EditAnywhere, DisplayName = "Ignored Texts")
	//TArray<FText> LocalizationIgnoredTexts;

	// Additional Array of source strings that this system won't overwrite the namespace or key for
	UPROPERTY(Category = "Localization", Config, EditAnywhere, AdvancedDisplay, DisplayName = "Ignored Strings")
	TSet<FString> LocalizationIgnoredStrings;

	// Map used to remap some SourceStrings texts found in the quests with a new Text value/namespace/key
	// Key: SourceString we are searching for
	// Value: Text replacement. NOTE: if the text value is usually not empty
	UPROPERTY(Category = "Localization", Config, EditAnywhere, AdvancedDisplay, DisplayName = "Remap Source Strings to Texts")
	TMap<FString, FText> LocalizationRemapSourceStringsToTexts;


	// Enables the message log to output info/errors/warnings to it
	UPROPERTY(Category = "Logger", Config, EditAnywhere)
	bool bEnableMessageLog = true;

	// Should the message log mirror the message with the output log, used even if the output log is disabled.
	UPROPERTY(Category = "Logger", Config, EditAnywhere)
	bool bMessageLogMirrorToOutputLog = true;

	// Opens the message log in front of the user if messages are displayed
	// See OpenMessageLogLevelsHigherThan for the filter
	UPROPERTY(Category = "Logger", Config, EditAnywhere)
	bool bMessageLogOpen = true;

	// NOTE: Not editable is intended so that not to allow the user to disable logging completely
	UPROPERTY(Config)
	bool bEnableOutputLog = false;

	// By default the message log does not support debug output, latest is info.
	// For the sake of sanity we redirect all levels higher than RedirectMessageLogLevelsHigherThan to the output log
	// even if the output log is disabled.
	// So that not to output for example debug output to the message log only to the output log.
	// NOTE: A value of EEGQuestLogLevel::NoLogging means no log level will get redirected
	UPROPERTY(Category = "Logger", Config, EditAnywhere, AdvancedDisplay)
	EEGQuestLogLevel RedirectMessageLogLevelsHigherThan = EEGQuestLogLevel::Warning;

	// All the log levels messages that will open the message log window if bMessageLogOpen is true
	// NOTE: A value of  EEGQuestLogLevel::NoLogging means all log levels will be opened if bMessageLogOpen is true
	UPROPERTY(Category = "Logger", Config, EditAnywhere, AdvancedDisplay)
	EEGQuestLogLevel OpenMessageLogLevelsHigherThan = EEGQuestLogLevel::NoLogging;


	// Should we hide the categories in the Quest browser that do not have any children?
	UPROPERTY(Category = "Browser", Config, EditAnywhere)
	bool bHideEmptyQuestBrowserCategories = true;


	//
	// Graph Node
	//

	// Whether the description text wraps onto a new line when it's length exceeds this width;
	// Tf this value is zero or negative, no wrapping occurs.
	UPROPERTY(Category = "Graph Node", Config, EditAnywhere)
	float DescriptionWrapTextAt = 256.f;

	// The amount of blank space left around the edges of the description text area.
	UPROPERTY(Category = "Graph Node", Config, EditAnywhere)
	FMargin DescriptionTextMargin = FMargin(5.f);

	// The horizontal alignment of the graph node title and icon
	UPROPERTY(Category = "Graph Node", Config, EditAnywhere)
	TEnumAsByte<EHorizontalAlignment> TitleHorizontalAlignment = HAlign_Fill;


	//
	// Colors based on https://material.io/guidelines/style/color.html#color-color-palette
	//

	// The background color of the root node.
	UPROPERTY(Category = "Graph Node Color", Config, EditAnywhere)
	FLinearColor RootNodeColor = FLinearColor{0.105882f, 0.368627f, 0.125490f, 1.f}; // greenish

	// The background color of the end node.
	UPROPERTY(Category = "Graph Node Color", Config, EditAnywhere)
	FLinearColor EndNodeColor = FLinearColor{0.835294f, 0.f, 0.f, 1.f}; // redish

	// The background color of the stage card: the blue the retired objective nodes used to wear.
	UPROPERTY(Category = "Graph Node Color", Config, EditAnywhere)
	FLinearColor StageNodeColor = FLinearColor{0.04f, 0.22f, 0.48f, 1.f}; // blueish

	// The background color of the node borders.
	UPROPERTY(Category = "Graph Node Color", Config, EditAnywhere)
	FLinearColor BorderBackgroundColor = FLinearColor::Black;

	// The background color of the node borders when hovered over
	UPROPERTY(Category = "Graph Node Color", Config, EditAnywhere)
	FLinearColor BorderHoveredBackgroundColor = FLinearColor(0.380392f, 0.380392f, 0.380392f, 1.0f); // gray

	// The background color of the node borders for nodes which can't have children.
	UPROPERTY(Category = "Graph Node Color", Config, EditAnywhere)
	FLinearColor BorderBackgroundColorNoChildren = FLinearColor(0.0f, 0.05f, 0.1f, 1.0f);

	// The background color of the node borders for nodes which can't have children when hovered over
	UPROPERTY(Category = "Graph Node Color", Config, EditAnywhere)
	FLinearColor BorderHoveredBackgroundColorNoChildren = FLinearColor(0.0f, 0.1f, 0.2f, 1.0f);

	// The wire thickness of the connections between nodes.
	UPROPERTY(Category = "Graph Edge", Config, EditAnywhere)
	float WireThickness = 2.0f;

	// Flag indicating to draw the bubbles of the wire or not.
	UPROPERTY(Category = "Graph Edge", Config, EditAnywhere)
	bool bWireDrawBubbles = false;

	// The color of a wire that carries no outcome: a start node's arrow, or a stage's edges to the
	// objectives it owns.
	UPROPERTY(Category = "Graph Edge Color", Config, EditAnywhere)
	FLinearColor WireBaseColor = FLinearColor{1.0f, 1.0f, 1.0f, 1.0f}; // white

	// The color of an arrow leaving an objective on its Success outcome.
	UPROPERTY(Category = "Graph Edge Color", Config, EditAnywhere)
	FLinearColor WireSuccessArrowColor = FLinearColor{0.145098f, 0.729412f, 0.231373f, 1.0f}; // green

	// The color of an arrow leaving an objective on its Fail outcome.
	UPROPERTY(Category = "Graph Edge Color", Config, EditAnywhere)
	FLinearColor WireFailArrowColor = FLinearColor{0.831373f, 0.152941f, 0.152941f, 1.0f}; // red

	// The color of the wire when hovered over
	UPROPERTY(Category = "Graph Edge Color", Config, EditAnywhere)
	FLinearColor WireHoveredColor = FLinearColor{1.0f, 0.596078f, 0.0f, 1.0f}; // orange

	//
	// Advanced Section
	//

	// The offset on the X axis (left/right) to use when automatically positioning nodes.
	UPROPERTY(Category = "Position", Config, EditAnywhere, AdvancedDisplay)
	int32 OffsetBetweenColumnsX = 500;

	// The offset on the Y axis (up/down) to use when automatically positioning nodes.
	UPROPERTY(Category = "Position", Config, EditAnywhere, AdvancedDisplay)
	int32 OffsetBetweenRowsY = 200;
};
