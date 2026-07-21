// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "Interfaces/Interface_AssetUserData.h"
#include "Engine/AssetUserData.h"

#include "IEGQuestEditorAccess.h"
#include "EGQuestPluginSettings.h"
// EEGQuestResumePolicy is a UPROPERTY below, so the type must be complete here.
#include "EGQuestTypes.h"

#if NY_ENGINE_VERSION >= 500
#include "UObject/ObjectSaveContext.h"
#endif

#include "EGQuestGraph.generated.h"

class UEGQuestNode;
class UEGQuestScript;
class UBlueprint;

// Custom serialization version for changes made in Dev-Quests stream
struct UNREALEXTENDEDQUEST_API FEGQuestGraphObjectVersion
{
	enum Type
	{
		// Before any version changes were made
		BeforeCustomVersionWasAdded = 0,
		ConvertedNodesToUObject,
		UseOnlyOneOutputAndInputPin,
		MergeVirtualParentAndSelectorTypes,
		ConvertQuestDataArraysToSets,
		AddGUID,
		AddComparisonWithOtherContextValue,
		AddTextFormatArguments,
		AddLocalizationOverwrittenNamespacesAndKeys,
		AddVirtualParentFireDirectChildEnterEvents,
		AddGUIDToNodes,
		AddCustomObjectsToContextData,
		AddSupportForMultipleStartNodes,

		// Route arbitration became authored data (UEGQuestNode::RoutePriorities,
		// UEGQuestNode_Start::EntryPriority) instead of canvas position. An asset serialized before
		// this has neither, so the next compile seeds both from its layout - see
		// UEGQuestGraph::NeedsPriorityMigration.
		AddAuthoredRoutePriorities,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;

private:
	FEGQuestGraphObjectVersion() {}
};


/**
 *  Quest asset containing the static data of a quest
 *  Instances can be created in content browser
 *  Quests have a custom blueprint editor
 */
UCLASS(BlueprintType, Meta = (DisplayThumbnail = "true"))
class UNREALEXTENDEDQUEST_API UEGQuestGraph : public UObject, public IInterface_AssetUserData
{
	GENERATED_BODY()
public:

	//
	// Begin UObject Interface.
	//

	/** @return a one line description of an object for viewing in the thumbnail view of the generic browser */
	FString GetDesc() override { return TEXT(" DESCRIPTION = ") + GetName();  }

	/**
	 * Registers quest graphs with Asset Manager for cooked discovery and stable save identifiers.
	 *
	 * KEPT DELIBERATELY, AND DELIBERATELY NOT KEYED ON DefinitionId. The reasoning, since this looks
	 * like the obvious place to put the new namespaced id:
	 *
	 * - Overriding this is not redundant, but it is close. Config/DefaultGame.ini already registers
	 *   PrimaryAssetType "QuestGraph" over /Game/Quests, and UObject::GetPrimaryAssetId forwards to
	 *   UAssetManager::DeterminePrimaryAssetIdForObject, which - for a non-blueprint type under a
	 *   scanned directory - produces FPrimaryAssetId("QuestGraph", <short package name>). That is what
	 *   this override already returns for every asset the config covers. It is not a no-op though:
	 *   the override runs through GetAssetRegistryTags, so it is what the registry records, and
	 *   UAssetManager::ExtractPrimaryAssetIdFromData prefers the recorded tag over its own guess.
	 *
	 * - It is insufficient as an identity, and mildly harmful. The name is the bare asset FName, so
	 *   two quests called Q_Intro in different folders collide; and it hands a *valid* id to graphs
	 *   outside every scan directory (this plugin's own test content, /Engine/Transient duplicates),
	 *   which the Asset Manager then never registers - an id that cannot be resolved back to a path.
	 *
	 * - It is still not where the fix goes. Changing what this returns changes what the registry
	 *   records, which rewrites the cook's chunk assignment and invalidates every
	 *   FEGQuestRuntimeSnapshot::QuestAssetId already in a save, so it is a save-format change rather
	 *   than an identity one. So the plugin's catalog key is
	 *   DefinitionId (below) - the plugin's own concept, namespaced by construction, changeable
	 *   without touching cook or engine identity - and this stays the engine's discovery identity.
	 */
	FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("QuestGraph"), GetFName());
	}

	/**
	 * Presave function. Gets called once before an object gets serialized for saving. This function is necessary
	 * for save time computation as Serialize gets called three times per object from within SavePackage.
	 *
	 * @warning: Objects created from within PreSave will NOT have PreSave called on them!!!
	 */
#if NY_ENGINE_VERSION >= 500
	void PreSave(FObjectPreSaveContext SaveContext) override;
#else
	void PreSave(const class ITargetPlatform* TargetPlatform) override;
#endif
	/** UObject serializer. */
	void Serialize(FArchive& Ar) override;

	/**
	 * Do any object-specific cleanup required immediately after loading an object,
	 * and immediately after any undo/redo.
	 */
	void PostLoad() override;

	/**
	 * Called after the C++ constructor and after the properties have been initialized, including those loaded from config.
	 * mainly this is to emulate some behavior of when the constructor was called after the properties were initialized.
	 * This creates the QuestGraph for this Quest.
	 */
	void PostInitProperties() override;

	/** Executed after Rename is executed. */
	void PostRename(UObject* OldOuter, FName OldName) override;

	/**
	 * Called after duplication & serialization and before PostLoad. Used to e.g. make sure UStaticMesh's UModel gets copied as well.
	 * Note: NOT called on components on actor duplication (alt-drag or copy-paste).  Use PostEditImport as well to cover that case.
	 */
	void PostDuplicate(bool bDuplicateForPIE) override;

	/**
	* Called after importing property values for this object (paste, duplicate or .t3d import)
	* Allow the object to perform any cleanup for properties which shouldn't be duplicated or
	* are unsupported by the script serialization
	*/
	void PostEditImport() override;

#if WITH_EDITOR
	/**
	 * Note that the object will be modified.  If we are currently recording into the
	 * transaction buffer (undo/redo), save a copy of this object into the buffer and
	 * marks the package as needing to be saved.
	 *
	 * @param	bAlwaysMarkDirty	if true, marks the package dirty even if we aren't
	 *								currently recording an active undo/redo transaction
	 * @return true if the object was saved to the transaction buffer
	 */
	bool Modify(bool bAlwaysMarkDirty = true) override;

	/**
	 * Called when a property on this object has been modified externally
	 *
	 * @param PropertyChangedEvent the property that was modified
	 */
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	/**
	 * This alternate version of PostEditChange is called when properties inside structs are modified.  The property that was actually modified
	 * is located at the tail of the list.  The head of the list of the FStructProperty member variable that contains the property that was modified.
	 */
	void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent) override;

	/**
	 * Callback used to allow object register its direct object references that are not already covered by
	 * the token stream.
	 *
	 * @param InThis Object to collect references from.
	 * @param Collector	FReferenceCollector objects to be used to collect references.
	 */
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	/**
	 * Reports what a compile would report, without compiling.
	 *
	 * CONST, AND STRICTLY SO. UObject::IsDataValid is const and validate-on-save calls it on every
	 * asset it touches: it must not compile, mint a GUID, renumber a priority, seed a migration or
	 * call Modify(), or saving one quest would dirty every quest that validated alongside it and the
	 * user would be asked to re-save assets they never opened. Validation reads; only the compiler
	 * writes. A node missing a GUID is *reported* here (Quest.Node.MissingGUID), never fixed.
	 *
	 * Routed through IEGQuestEditorAccess::ValidateQuest, since the rules need the EdGraph pins and
	 * live in the editor module. Returns Valid when the graph has no editor access (no rule can be
	 * evaluated, so nothing can be asserted).
	 */
	EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
	// End UObject Interface.

	//
	// Begin IInterface_AssetUserData Interface
	//
	virtual void AddAssetUserData(UAssetUserData* InUserData) override;
	virtual void RemoveUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass) override;
	virtual UAssetUserData* GetAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass) override;
	virtual const TArray<UAssetUserData*>* GetAssetUserDataArray() const override;
	//
	// End IInterface_AssetUserData Interface
	//

	//
	// Begin own functions
	//
#if WITH_EDITOR
	// Broadcasts whenever a property of this quest changes.
	DECLARE_EVENT_OneParam(UEGQuestGraph, FEGQuestPropertyChanged, const FPropertyChangedEvent& /* PropertyChangedEvent */);
	FEGQuestPropertyChanged OnQuestPropertyChanged;

	// Helper functions to get the names of some properties. Used by the QuestPluginEditor module.
	static FName GetMemberNameName() { return GET_MEMBER_NAME_CHECKED(UEGQuestGraph, Name); }
	static FName GetMemberNameGUID() { return GET_MEMBER_NAME_CHECKED(UEGQuestGraph, GUID); }
	static FName GetMemberNameNodes() { return GET_MEMBER_NAME_CHECKED(UEGQuestGraph, Nodes); }
	static FName GetMemberNameStartNodes() { return GET_MEMBER_NAME_CHECKED(UEGQuestGraph, StartNodes); }
	static FName GetMemberNameNodesGUIDToIndexMap() { return GET_MEMBER_NAME_CHECKED(UEGQuestGraph, NodesGUIDToIndexMap); }
	static FName GetMemberNameAssetUserData() { return GET_MEMBER_NAME_CHECKED(UEGQuestGraph, AssetUserData); }
	static FName GetMemberNameDefinitionId() { return GET_MEMBER_NAME_CHECKED(UEGQuestGraph, DefinitionId); }
	static FName GetMemberNameContentVersion() { return GET_MEMBER_NAME_CHECKED(UEGQuestGraph, ContentVersion); }
	static FName GetMemberNameResumePolicy() { return GET_MEMBER_NAME_CHECKED(UEGQuestGraph, ResumePolicy); }

	//
	// Priority migration (compiler only)
	//
	// This asset predates FEGQuestGraphObjectVersion::AddAuthoredRoutePriorities, so its arbitration
	// order still only exists as canvas layout and the next compile must seed it. The compiler seeds
	// in two places because the two seeds straddle GUID minting: EntryPriority needs only the root's
	// NodePosX and seeds before OrderRootGraphNodes, while route priority is keyed by destination
	// GUID and cannot seed until AssignIndices has minted them. Hence peek, then consume.
	//

	/** Peek. Does not clear the flag: the entry-priority seed runs first and the route seed still needs it. */
	bool NeedsPriorityMigration() const { return bNeedsPriorityMigration; }

	/** Consume. Call once, from the route-priority seed - the second and last site. */
	bool ConsumePriorityMigration()
	{
		const bool bWasNeeded = bNeedsPriorityMigration;
		bNeedsPriorityMigration = false;
		return bWasNeeded;
	}

	// The authority-side script of this quest, or null when it has none.
	TSubclassOf<UEGQuestScript> GetQuestScriptClass() const { return QuestScriptClass; }
	void SetQuestScriptClass(TSubclassOf<UEGQuestScript> InClass) { QuestScriptClass = InClass; }

#if WITH_EDITORONLY_DATA
	// The script blueprint baked into this asset, or null when the quest has none.
	UBlueprint* GetQuestScriptBlueprint() const { return QuestScriptBlueprint; }
	void SetQuestScriptBlueprint(UBlueprint* InBlueprint) { QuestScriptBlueprint = InBlueprint; }
#endif // WITH_EDITORONLY_DATA

	// Create the basic quest graph.
	void CreateGraph();

	// Clears all nodes from the graph.
	void ClearGraph();

	// Gets the editor graph of this Quest.
	UEdGraph* GetGraph()
	{
		check(QuestGraph);
		return QuestGraph;
	}
	const UEdGraph* GetGraph() const
	{
		check(QuestGraph);
		return QuestGraph;
	}

	// Useful for initially compiling the Quest when we need the extra processing steps done by the compiler.
	void InitialCompileQuestNodesFromGraphNodes()
	{
		if (bWasCompiledAtLeastOnce)
			return;

		CompileQuestNodesFromGraphNodes();
		bWasCompiledAtLeastOnce = true;
	}

	// Compiles the quest nodes from the graph nodes. Meaning it transforms the graph data -> (into) quest data.
	void CompileQuestNodesFromGraphNodes();

	// Sets the quest editor implementation. This is called in the constructor of the QuestGraphGraph in the QuestSytemEditor module.
	static void SetQuestEditorAccess(const TSharedPtr<IEGQuestEditorAccess>& InQuestEditor)
	{
		check(!QuestEditorAccess.IsValid());
		check(InQuestEditor.IsValid());
		QuestEditorAccess = InQuestEditor;
	}

	// Gets the quest editor implementation.
	static TSharedPtr<IEGQuestEditorAccess> GetQuestEditorAccess() { return QuestEditorAccess; }

	// Enables/disables the compilation of the quests in the editor, use with care. Mainly used for optimization.
	void EnableCompileQuest() { bCompileQuest = true; }
	void DisableCompileQuest() { bCompileQuest = false; }
#endif

	// Construct and initialize a node within this Quest.
	template<class T>
	T* ConstructQuestNode(TSubclassOf<UEGQuestNode> QuestNodeClass = T::StaticClass())
	{
		// Set flag to be transactional so it registers with undo system
		T* QuestNode = NewObject<T>(this, QuestNodeClass, NAME_None, RF_Transactional);
		QuestNode->OnCreatedInEditor();
		return QuestNode;
	}

	UFUNCTION(BlueprintPure, Category = "Quest")
	int32 GetQuestVersion() const { return Version; }

	//
	// Identity and content version
	//

	/**
	 * The catalog key: a namespaced stable id, "Namespace.Name" (e.g. "Game.Q_Intro").
	 *
	 * This is how anything outside the plugin names a quest definition - offers, telemetry, and from
	 * the run record. It is stable across renames and moves in a way the asset path and
	 * GetPrimaryAssetId are not, and namespaced so a project and a plugin cannot collide.
	 *
	 * It does NOT replace GUID. GUID is integrity identity: it answers "is this the same asset",
	 * survives a DefinitionId edit, and is regenerated on duplicate. DefinitionId is a name a human
	 * chose and may change. Two ids because they answer two questions.
	 *
	 * Seeded in PostLoad when empty (mount point + asset name) so existing content validates without
	 * a resave, and persisted by the next compile.
	 */
	UFUNCTION(BlueprintPure, Category = "Quest")
	FName GetDefinitionId() const { return DefinitionId; }
	void SetDefinitionId(const FName InDefinitionId) { DefinitionId = InDefinitionId; }

	/** Is DefinitionId exactly "Namespace.Name", both halves non-empty? The Quest.Graph.MalformedDefinitionId rule. */
	UFUNCTION(BlueprintPure, Category = "Quest")
	bool IsDefinitionIdWellFormed() const;

	/**
	 * Authored. Bump it when an edit to this graph makes a run recorded against the previous value
	 * unsafe to resume.
	 *
	 * Coarse on purpose: this is a number a human types, not a hash of the graph, so a text-only fix
	 * that gets bumped costs players a restart. That is the accepted price of keeping the authored
	 * graph as the single artifact - a derived hash would have to be a second serialized artifact,
	 * and would fire on cosmetic edits anyway (UEGQuestNode::PostDuplicate re-GUIDs every node, so
	 * anything GUID-derived changes when a designer duplicates a quest to branch it).
	 *
	 * Stamped into every run record on start and compared against the save's on resume.
	 */
	UFUNCTION(BlueprintPure, Category = "Quest")
	int32 GetContentVersion() const { return ContentVersion; }
	void SetContentVersion(const int32 InContentVersion) { ContentVersion = FMath::Max(1, InContentVersion); }

	/** What to do with a save whose ContentVersion no longer matches this graph. Applied on resume. */
	UFUNCTION(BlueprintPure, Category = "Quest")
	EEGQuestResumePolicy GetResumePolicy() const { return ResumePolicy; }
	EEGQuestAutoTrackPolicy GetAutoTrackPolicy() const { return AutoTrackPolicy; }
	void SetAutoTrackPolicy(EEGQuestAutoTrackPolicy InPolicy) { AutoTrackPolicy = InPolicy; }
	void SetResumePolicy(const EEGQuestResumePolicy InResumePolicy) { ResumePolicy = InResumePolicy; }

	/** Quest-lifetime roles resolved before the first stage is published. */
	const TArray<FEGQuestRoleDefinition>& GetRoleDefinitions() const { return RoleDefinitions; }
	TArray<FEGQuestRoleDefinition>& GetMutableRoleDefinitions() { return RoleDefinitions; }
	void SetRoleDefinitions(const TArray<FEGQuestRoleDefinition>& InDefinitions) { RoleDefinitions = InDefinitions; }

	// Gets/extracts the name (without extension) of the dialog from the uasset filename
	UFUNCTION(BlueprintPure, Category = "Quest")
	FString GetQuestName() const
	{
		// Note: GetPathName() calls this at the end, so this just gets the direct name that we want.
		// Assumption only true for objects that have the Outer an UPackage.
		// Otherwise call FPaths::GetBaseFilename(GetPathName())
		return GetName();
	}

	// Same as the GetQuestName only it returns a FName.
	UFUNCTION(BlueprintPure, Category = "Quest")
	FName GetQuestFName() const { return GetFName(); }

	// Gets the unique identifier for this quest.
	UFUNCTION(BlueprintPure, Category = "Quest|GUID")
	FGuid GetGUID() const { check(GUID.IsValid()); return GUID; }

	// Regenerate the GUID of this Quest
	void RegenerateGUID() { GUID = FGuid::NewGuid(); }

	UFUNCTION(BlueprintPure, Category = "Quest|GUID")
	bool HasGUID() const { return GUID.IsValid(); }

	// Gets all the nodes
	UFUNCTION(BlueprintPure, Category = "Quest")
	const TArray<UEGQuestNode*>& GetNodes() const { return Nodes; }

	UFUNCTION(BlueprintPure, Category = "Quest", DisplayName = "Get Start Nodes")
	const TArray<UEGQuestNode*>& GetMutableStartNodes() { return StartNodes; }
	const TArray<UEGQuestNode*>& GetStartNodes() const { return StartNodes; }

	UFUNCTION(BlueprintPure, Category = "Quest")
	bool IsValidNodeIndex(int32 NodeIndex) const { return Nodes.IsValidIndex(NodeIndex); }

	UFUNCTION(BlueprintPure, Category = "Quest")
	bool IsValidNodeGUID(const FGuid& NodeGUID) const { return IsValidNodeIndex(GetNodeIndexForGUID(NodeGUID)); }

	// Gets the GUID for the Node at NodeIndex
	UFUNCTION(BlueprintPure, Category = "Quest", DisplayName = "Get Node GUID For Index")
	FGuid GetNodeGUIDForIndex(int32 NodeIndex) const;

	// Gets the corresponding Node Index for the supplied NodeGUID
	// Returns -1 (INDEX_NONE) if the Node GUID does not exist.
	UFUNCTION(BlueprintPure, Category = "Quest", DisplayName = "Get Node Index For GUID")
	int32 GetNodeIndexForGUID(const FGuid& NodeGUID) const;

	// Gets the Node as a mutable pointer.
	UFUNCTION(BlueprintPure, Category = "Quest", DisplayName = "Get Node From Index")
	UEGQuestNode* GetMutableNodeFromIndex(int32 NodeIndex) const { return Nodes.IsValidIndex(NodeIndex) ? Nodes[NodeIndex] : nullptr; }

	UFUNCTION(BlueprintPure, Category = "Quest|Data", DisplayName = "Get Node From GUID")
	UEGQuestNode* GetMutableNodeFromGUID(const FGuid& NodeGUID) const { return GetMutableNodeFromIndex(GetNodeIndexForGUID(NodeGUID));   }

	// Sets the new Start Node. Use with care.
	void SetStartNodes(TArray<UEGQuestNode*> InStartNodes);

	// NOTE: don't call this if you don't know what you are doing, you most likely need to call
	// SetStartNode
	// SetNodes
	// After this
	void EmptyNodesGUIDToIndexMap() { NodesGUIDToIndexMap.Empty(); }

	// Sets the Quest Nodes. Use with care.
	void SetNodes(const TArray<UEGQuestNode*>& InNodes);

	// Sets the Node at index NodeIndex. Use with care.
	void SetNode(int32 NodeIndex, UEGQuestNode* InNode);

	// Is the Node at NodeIndex (if it exists) an end node?
	bool IsEndNode(int32 NodeIndex) const;

	// Check if a text file in the same folder with the same name (Name) exists and loads the data from that file.
	void ImportFromFile();

	// Method to handle when this asset is going to be saved. Compiles the quest and saves to the text file.
	void OnPreAssetSaved();

	// Useful for initially reloading the data from the text file so that the quest is always in sync.
	void InitialSyncWithTextFile()
	{
		if (bIsSyncedWithTextFile)
		{
			return;
		}

		ImportFromFile();
		bIsSyncedWithTextFile = true;
	}

	// Exports this quest data into its corresponding ".quest" text file with the same name.
	void ExportToFile() const;

	// Updates the data of some nodes
	// Fills the QuestData with the updated data
	// NOTE: this can do a quest data -> graph node data update
	void UpdateAndRefreshData(bool bUpdateTextsNamespacesAndKeys = false);

	// Adds a new node to this quest, returns the index location of the added node in the Nodes array.
	int32 AddNode(UEGQuestNode* NodeToAdd) { return Nodes.Add(NodeToAdd); }

	// Adds a new start node to this quest, returns the index location of the added node in the Nodes array.
	int32 AddStartNode(UEGQuestNode* NodeToAdd) { return StartNodes.Add(NodeToAdd); }



	/**
	 * @param	bAddExtension	If this adds the .quest or .quest.json extension depending on the TextFormat.
	 * @return The path (as a relative path) and name of the text file, or empty string if something is wrong.
	 */
	FString GetTextFilePathName(bool bAddExtension = true) const;
	FString GetTextFilePathName(EEGQuestGraphTextFormat TextFormat, bool bAddExtension = true) const;

	// Perform deletion on the text files
	bool DeleteTextFileForTextFormat(EEGQuestGraphTextFormat TextFormat) const;
	bool DeleteTextFileForExtension(const FString& FileExtension) const;
	bool DeleteAllTextFiles() const;

	// Is this quest located inside the project directory
	bool IsInProjectDirectory() const;

	/**
	 * @return the text file path name (as a relative path) from the asset path name.
	 * NOTE: does not have extension, call GetTextFileExtension for that.
	 */
	static FString GetTextFilePathNameFromAssetPathName(const FString& AssetPathName);

private:
	// Rebuild & Update and node and its edges
	void RebuildAndUpdateNode(UEGQuestNode* Node, const UEGQuestPluginSettings& Settings, bool bUpdateTextsNamespacesAndKeys);

	void ImportFromFileFormat(EEGQuestGraphTextFormat TextFormat);
	void ExportToFileFormat(EEGQuestGraphTextFormat TextFormat) const;

	// Updates NodesGUIDToIndexMap with Node
	void UpdateGUIDToIndexMap(const UEGQuestNode* Node, int32 NodeIndex);

protected:
	// Used to keep track of the version in text  file too, besides being written in the .uasset file.
	UPROPERTY()
	int32 Version = FEGQuestGraphObjectVersion::LatestVersion;

	// Quest name used for text-file reference; it must match the .uasset and .quest file names.
	UPROPERTY(VisibleAnywhere, Category = "Quest")
	FName Name;

	// The Unique identifier for each quest. This is used to uniquely identify a Quest, instead of it's name or path. Much more safer.
	UPROPERTY(VisibleAnywhere, Category = "Quest")
	FGuid GUID;

	// The catalog key, "Namespace.Name". See GetDefinitionId.
	// Editable because it is a name a human chooses; validated (Quest.Graph.MalformedDefinitionId,
	// and Quest.Graph.DuplicateDefinitionId across assets in the validate-all commandlet) because an
	// editable catalog key is a typo away from breaking every save that references it.
	UPROPERTY(EditAnywhere, Category = "Quest")
	FName DefinitionId;

	// Authored content version. See GetContentVersion. Starts at 1: 0 would be indistinguishable from
	// "a save written before run records carried a version at all", which resume must be able to tell apart.
	UPROPERTY(EditAnywhere, Category = "Quest", Meta = (ClampMin = "1"))
	int32 ContentVersion = 1;

	// What to do with a save from a different ContentVersion. Defaults to Restart: losing progress is
	// recoverable, resuming a run into a node that no longer means what it did is not.
	UPROPERTY(EditAnywhere, Category = "Quest")
	EEGQuestResumePolicy ResumePolicy = EEGQuestResumePolicy::Restart;

	/** Player-facing journal selection policy; the component still enforces exactly one tracked run. */
	UPROPERTY(EditAnywhere, Category = "Quest|Presentation")
	EEGQuestAutoTrackPolicy AutoTrackPolicy = EEGQuestAutoTrackPolicy::IfNone;

	UPROPERTY(EditAnywhere, Category = "Quest|Roles")
	TArray<FEGQuestRoleDefinition> RoleDefinitions;

	/**
	 * The quest's event graph: one instance of this script runs per running quest instance, on the
	 * authority only. Optional.
	 *
	 * The quest editor's Open Script button bakes a script blueprint into this asset and keeps this
	 * class pointing at it - while an embedded script exists, this property belongs to it. Without
	 * one, a native UEGQuestScript subclass may be assigned here directly.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	TSubclassOf<UEGQuestScript> QuestScriptClass;

#if WITH_EDITORONLY_DATA
	/**
	 * The script blueprint baked into this asset, the way a level blueprint is baked into its level:
	 * it is not a content browser asset and belongs to this quest alone. Only its generated class
	 * (QuestScriptClass above) cooks; the blueprint itself is editor-only.
	 */
	UPROPERTY(Meta = (QuestNoExport))
	TObjectPtr<UBlueprint> QuestScriptBlueprint;
#endif // WITH_EDITORONLY_DATA

	// Root nodes, compiler output sorted by UEGQuestNode_Start::EntryPriority. A quest starts from the
	// first one with a child that enters.
	// (node itself works like the SelectorFirst node, first satisfied child will be picked)
	UPROPERTY(Instanced, VisibleAnywhere, Category = "Quest")
	TArray<UEGQuestNode*> StartNodes;

	// The new list of all nodes that belong to this Quest. Each nodes has children (edges) that have indices that point
	// to other nodes in this array.
	// NOTE: Add VisibleAnywhere to make it easier to debug
	UPROPERTY(AdvancedDisplay, EditFixedSize, Instanced, Meta = (QuestWriteIndex))
	TArray<UEGQuestNode*> Nodes;

	// Maps Node GUID => Node Index
	UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = "Quest", DisplayName = "Nodes GUID To Index Map")
	TMap<FGuid, int32> NodesGUIDToIndexMap;

	// Useful for syncing on the first run with the text file.
	bool bIsSyncedWithTextFile = false;

#if WITH_EDITORONLY_DATA
	// EdGraph based representation of the QuestGraph class
	UPROPERTY(Meta = (QuestNoExport))
	TObjectPtr<UEdGraph> QuestGraph;

	// Ptr to interface to quest editor operations. See function SetQuestEditorAccess for more details.
	static TSharedPtr<IEGQuestEditorAccess> QuestEditorAccess;

	// Flag used for optimization, used to enable/disable compiling of the quest for bulk operations.
	bool bCompileQuest = true;

	// Flag indicating if this Quest was compiled at least once in the current runtime.
	bool bWasCompiledAtLeastOnce = false;

	// Used to build the change event and broadcast it to the children
	int32 BroadcastPropertyNodeIndexChanged = INDEX_NONE;

	// Set by PostLoad when this asset was serialized before AddAuthoredRoutePriorities; cleared by
	// ConsumePriorityMigration. See the accessors above.
	//
	// Not a UPROPERTY, and deliberately not saved: the custom version already records durably whether
	// this asset has authored priorities, so PostLoad re-derives this on every load and the flag can
	// never drift out of sync with the thing it describes. Saving it would introduce a second source
	// of truth that a failed compile could strand in the wrong state.
	bool bNeedsPriorityMigration = false;
#endif

	// Flag that indicates that This Was Loaded was called
	bool bWasLoaded = false;

public:
	/** Array of user data stored with the asset (for IInterface_AssetUserData implementation) */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Instanced, Category = "Asset User Data")
	TArray<TObjectPtr<UAssetUserData>> AssetUserData;
};
