// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "EGQuestGraph.h"

#include "UObject/DevObjectVersion.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"

#if WITH_EDITOR
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "Misc/DataValidation.h"
#include "EGQuestDiagnostics.h"
#endif

#include "EGQuestPluginModule.h"
#include "IO/EGQuestJsonWriter.h"
#include "IO/EGQuestJsonParser.h"
#include "Nodes/EGQuestNode_Objective.h"
#include "Nodes/EGQuestNode_End.h"
#include "Nodes/EGQuestNode_Start.h"
#include "EGQuestManager.h"
#include "Logging/EGQuestLogger.h"
#include "EGQuestHelper.h"

#define LOCTEXT_NAMESPACE "QuestGraph"

// Unique QuestGraph Object version id, generated with random
const FGuid FEGQuestGraphObjectVersion::GUID(0x2B8E5105, 0x6F66348F, 0x2A8A0B25, 0x9047A071);
// Register Quest custom version with Core
FDevVersionRegistration GRegisterQuestGraphObjectVersion(FEGQuestGraphObjectVersion::GUID,
														  FEGQuestGraphObjectVersion::LatestVersion, TEXT("Dev-QuestGraph"));


// Update quest up to the ConvertedNodesToUObject version
void UpdateQuestToVersion_ConvertedNodesToUObject(UEGQuestGraph* Quest)
{
	// No Longer supported, get data from text file, and reconstruct everything
	Quest->InitialSyncWithTextFile();
#if WITH_EDITOR
	// Force clear the old graph
	Quest->ClearGraph();
#endif
}

// Update quest up to the UseOnlyOneOutputAndInputPin version
void UpdateQuestToVersion_UseOnlyOneOutputAndInputPin(UEGQuestGraph* Quest)
{
#if WITH_EDITOR
	Quest->GetQuestEditorAccess()->UpdateQuestToVersion_UseOnlyOneOutputAndInputPin(Quest);
#endif
}

// The catalog key existing content gets for free, so that no designer has to touch an asset to make
// it validate. Keyed on the mount point rather than the full package path: moving a quest between
// folders must not change its id (that is the whole point of not using the path), while the mount
// point is the one boundary a project and a plugin can never share, which is what makes the id
// collision-free by construction.
//
// Returns NAME_None for an unmounted package (/Temp, /Engine/Transient duplicates, graphs built in
// a test): there is no namespace such a quest can honestly claim, and inventing one would hand out
// an id that resolves to nothing.
static FName MakeSeededQuestDefinitionId(const UEGQuestGraph* Quest)
{
	const UPackage* Package = Quest->GetOutermost();
	if (!Package)
	{
		return NAME_None;
	}

	const FName MountPoint = FPackageName::GetPackageMountPoint(Package->GetName());
	if (MountPoint.IsNone())
	{
		return NAME_None;
	}

	return FName(*FString::Printf(TEXT("%s.%s"), *MountPoint.ToString(), *Quest->GetName()));
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Begin UObject interface
#if NY_ENGINE_VERSION >= 500
void UEGQuestGraph::PreSave(FObjectPreSaveContext SaveContext)
{
	Super::PreSave(SaveContext);
#else
void UEGQuestGraph::PreSave(const class ITargetPlatform* TargetPlatform)
{
	Super::PreSave(TargetPlatform);
#endif

	Name = GetQuestFName();
	bWasLoaded = true;

	// Version is the text file's copy of what the .uasset records in its custom version, and the text
	// file is written by OnPreAssetSaved below, after the compile that brings this asset up to date.
	// Loading an old asset leaves this field at the old value, so without this the export would stamp
	// a stale version onto a file that has just been written in the current format - and
	// ImportFromFileFormat reads this field back to decide whether the file it parsed predates
	// authored route priorities. A file that lies about its version would make that import re-seed
	// priorities from canvas layout and silently overwrite authored ones.
	Version = FEGQuestGraphObjectVersion::LatestVersion;

	OnPreAssetSaved();
}

void UEGQuestGraph::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FEGQuestGraphObjectVersion::GUID);
	Super::Serialize(Ar);
}

void UEGQuestGraph::PostLoad()
{
	Super::PostLoad();
	const int32 QuestVersion = GetLinkerCustomVersion(FEGQuestGraphObjectVersion::GUID);
	// Old files, UEGQuestNode used to be a FEGQuestNode
	if (QuestVersion < FEGQuestGraphObjectVersion::ConvertedNodesToUObject)
	{
		UpdateQuestToVersion_ConvertedNodesToUObject(this);
	}

	// Simplified and reduced the number of pins (only one input/output pin), used for the new visualization
	if (QuestVersion < FEGQuestGraphObjectVersion::UseOnlyOneOutputAndInputPin)
	{
		UpdateQuestToVersion_UseOnlyOneOutputAndInputPin(this);
	}

	// Simply the number of nodes, VirtualParent Node is merged into Objective Node and SelectRandom and SelectorFirst are merged into one Selector Node
	if (QuestVersion < FEGQuestGraphObjectVersion::MergeVirtualParentAndSelectorTypes)
	{
		FEGQuestLogger::Get().Warningf(
			TEXT("Quest = `%s` with Version MergeVirtualParentAndSelectorTypes will not be converted. See https://gitlab.com/snippets/1691704 for manual conversion"),
			*GetTextFilePathName()
		);
	}

	// Refresh the data, so that it is valid after loading.
	if (QuestVersion < FEGQuestGraphObjectVersion::AddTextFormatArguments ||
		QuestVersion < FEGQuestGraphObjectVersion::AddCustomObjectsToContextData)
	{
		UpdateAndRefreshData();
	}

	// Create thew new GUID
	if (!HasGUID())
	{
		RegenerateGUID();
		FEGQuestLogger::Get().Debugf(
			TEXT("Creating new GUID = `%s` for Quest = `%s` because of of invalid GUID."),
			*GUID.ToString(), *GetPathName()
		);
	}

	// Seeded here, not by the compiler: the validate-all commandlet runs over assets nobody has
	// resaved, and MissingDefinitionId is an Error, so seeding on compile would turn CI red on day one
	// for every quest in the project. This costs no dirty flag and cooks the same value it validates.
	if (DefinitionId.IsNone())
	{
		DefinitionId = MakeSeededQuestDefinitionId(this);
	}

#if WITH_EDITORONLY_DATA
	// Above the AreQuestNodesInSyncWithGraphNodes bail-out below on purpose. That early return fires
	// for exactly the oldest, least-maintained assets - the ones whose arbitration order has only ever
	// existed as canvas layout, and so the ones that most need this seed. Re-derived every load rather
	// than saved: the custom version already records durably whether this asset has authored
	// priorities, so this flag cannot drift and a failed compile cannot strand it set.
	bNeedsPriorityMigration = QuestVersion < FEGQuestGraphObjectVersion::AddAuthoredRoutePriorities;
#endif

#if WITH_EDITOR
	const bool bHasQuestEditorModule = GetQuestEditorAccess().IsValid();
	// If this is false it means the graph nodes are not even created? Check for old files that were saved
	// before graph editor was even implemented. The editor will popup a prompt from FEGQuestEditorUtilities::TryToCreateDefaultGraph
	if (bHasQuestEditorModule && !GetQuestEditorAccess()->AreQuestNodesInSyncWithGraphNodes(this))
	{
		return;
	}
#endif

	// Check Nodes for validity
	const int32 NodesNum = Nodes.Num();
	for (int32 NodeIndex = 0; NodeIndex < NodesNum; NodeIndex++)
	{
		UEGQuestNode* Node = Nodes[NodeIndex];
#if WITH_EDITOR
		if (bHasQuestEditorModule)
		{
			checkf(Node->GetGraphNode(), TEXT("Expected QuestVersion = %d to have a valid GraphNode for Node index = %d :("), QuestVersion, NodeIndex);
		}
#endif
		// Check children point to the right Node
		const TArray<FEGQuestEdge>& NodeEdges = Node->GetNodeChildren();
		const int32 EdgesNum = NodeEdges.Num();
		for (int32 EdgeIndex = 0; EdgeIndex < EdgesNum; EdgeIndex++)
		{
			const FEGQuestEdge& Edge = NodeEdges[EdgeIndex];
			if (!Edge.IsValid())
			{
				continue;
			}

			if (!Nodes.IsValidIndex(Edge.TargetIndex))
			{
				UE_LOG(
					LogEGQuestPlugin,
					Fatal,
					TEXT("Node with index = %d does not have a valid Edge index = %d with TargetIndex = %d"),
					NodeIndex, EdgeIndex, Edge.TargetIndex
				);
			}
		}
	}

	bWasLoaded = true;
}

void UEGQuestGraph::PostInitProperties()
{
	Super::PostInitProperties();

	// Ignore these cases
	if (HasAnyFlags(RF_ClassDefaultObject | RF_NeedLoad))
	{
		return;
	}

	const int32 QuestVersion = GetLinkerCustomVersion(FEGQuestGraphObjectVersion::GUID);

#if WITH_EDITOR
	// Wait for the editor module to be set by the editor in UEGQuestEdGraph constructor
	if (GetQuestEditorAccess().IsValid())
	{
		CreateGraph();
	}
#endif // #if WITH_EDITOR

	// Keep Name in sync with the file name
	Name = GetQuestFName();

	// Used when creating new Quests
	// Initialize with a valid GUID
	if (QuestVersion >= FEGQuestGraphObjectVersion::AddGUID && !HasGUID())
	{
		RegenerateGUID();
		FEGQuestLogger::Get().Debugf(
			TEXT("Creating new GUID = `%s` for Quest = `%s` because of new created Quest."),
			*GUID.ToString(), *GetPathName()
		);
	}
}

void UEGQuestGraph::PostRename(UObject* OldOuter, const FName OldName)
{
	Super::PostRename(OldOuter, OldName);
	Name = GetQuestFName();
}

void UEGQuestGraph::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	// Used when duplicating quests.
	// Make new guid for this copied Quest.
	RegenerateGUID();
	FEGQuestLogger::Get().Debugf(
		TEXT("Creating new GUID = `%s` for Quest = `%s` because Quest was copied."),
		*GUID.ToString(), *GetPathName()
	);

	// The copy inherited the original's DefinitionId, and two assets holding one catalog key is the
	// Quest.Graph.DuplicateDefinitionId error - which a designer would hit merely by duplicating a
	// quest to branch it. Re-seeded rather than cleared, because nothing would ever seed it again:
	// PostLoad does not run on an asset created in memory.
	//
	// Not for a PIE duplicate: that is the same definition, temporarily living somewhere else, and
	// must keep answering to the same key.
	if (!bDuplicateForPIE)
	{
		DefinitionId = MakeSeededQuestDefinitionId(this);
	}

#if WITH_EDITOR
	// The embedded script blueprint was copied with the asset, but its generated class still lives
	// in the source package until it recompiles here.
	if (!bDuplicateForPIE && GetQuestEditorAccess().IsValid())
	{
		GetQuestEditorAccess()->RefreshQuestScriptBlueprint(this);
	}
#endif // WITH_EDITOR
}

void UEGQuestGraph::PostEditImport()
{
	Super::PostEditImport();

	// Used when duplicating quests.
	// Make new guid for this copied Quest
	RegenerateGUID();
	FEGQuestLogger::Get().Debugf(
		TEXT("Creating new GUID = `%s` for Quest = `%s` because Quest was copied."),
		*GUID.ToString(), *GetPathName()
	);

	// Same reason as PostDuplicate: the pasted copy must not claim the original's catalog key.
	DefinitionId = MakeSeededQuestDefinitionId(this);
}

#if WITH_EDITOR
TSharedPtr<IEGQuestEditorAccess> UEGQuestGraph::QuestEditorAccess = nullptr;

bool UEGQuestGraph::Modify(bool bAlwaysMarkDirty)
{
	if (!CanModify())
	{
		return false;
	}

	const bool bWasSaved = Super::Modify(bAlwaysMarkDirty);
	// if (StartNode)
	// {
	// 	bWasSaved = bWasSaved && StartNode->Modify(bAlwaysMarkDirty);
	// }

	// for (UEGQuestNode* Node : Nodes)
	// {
	// 	bWasSaved = bWasSaved && Node->Modify(bAlwaysMarkDirty);
	// }

	return bWasSaved;
}

void UEGQuestGraph::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Signal to the listeners
	check(OnQuestPropertyChanged.IsBound());
	OnQuestPropertyChanged.Broadcast(PropertyChangedEvent);
}

void UEGQuestGraph::PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent)
{
	UpdateAndRefreshData();

	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}

void UEGQuestGraph::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	// Add the graph to the list of referenced objects
	UEGQuestGraph* This = CastChecked<UEGQuestGraph>(InThis);
	Collector.AddReferencedObject(This->QuestGraph, This);
	Super::AddReferencedObjects(InThis, Collector);
}

EDataValidationResult UEGQuestGraph::IsDataValid(FDataValidationContext& Context) const
{
	const TSharedPtr<IEGQuestEditorAccess> EditorAccess = GetQuestEditorAccess();
	if (!EditorAccess.IsValid())
	{
		// Every rule needs the EdGraph pins to evaluate, so without the editor module there is nothing
		// to check - which is not the same as having checked and found a problem.
		return EDataValidationResult::Valid;
	}

	// Read-only, all the way down. See the comment on the declaration: this runs on every asset that
	// validate-on-save touches, so it must not compile, mint, seed or Modify().
	FEGQuestDiagnostics Diagnostics;
	EditorAccess->ValidateQuest(this, Diagnostics);

	for (const FEGQuestDiagnostic& Diagnostic : Diagnostics.Items)
	{
		const FText Text = Diagnostic.FixHint.IsEmpty()
			? Diagnostic.Message
			: FText::Format(LOCTEXT("QuestDiagnosticWithFixHint", "{0} ({1})"), Diagnostic.Message, Diagnostic.FixHint);

		switch (Diagnostic.Severity)
		{
			case EEGQuestDiagnosticSeverity::Error:
				Context.AddError(Text);
				break;
			case EEGQuestDiagnosticSeverity::Warning:
				Context.AddWarning(Text);
				break;
			default:
				Context.AddMessage(EMessageSeverity::Info, Text);
				break;
		}
	}

	// Only an Error blocks. A Warning is reported and still saves; -Strict in the validate-all
	// commandlet is what escalates it, and that is a CI policy rather than a property of the asset.
	return Diagnostics.HasErrors() ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}

#endif

// End UObject interface
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Begin AssetUserData interface


void UEGQuestGraph::AddAssetUserData(UAssetUserData* InUserData)
{
	if (InUserData != nullptr)
	{
		UAssetUserData* ExistingData = GetAssetUserDataOfClass(InUserData->GetClass());
		if (ExistingData != nullptr)
		{
			AssetUserData.Remove(ExistingData);
		}
		AssetUserData.Add(InUserData);
	}
}

UAssetUserData* UEGQuestGraph::GetAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for (int32 DataIdx = 0; DataIdx < AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if (Datum != nullptr && Datum->IsA(InUserDataClass))
		{
			return Datum;
		}
	}
	return nullptr;
}

void UEGQuestGraph::RemoveUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for (int32 DataIdx = 0; DataIdx < AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if (Datum != nullptr && Datum->IsA(InUserDataClass))
		{
			AssetUserData.RemoveAt(DataIdx);
			return;
		}
	}
}

const TArray<UAssetUserData*>* UEGQuestGraph::GetAssetUserDataArray() const
{
	return &ToRawPtrTArrayUnsafe(AssetUserData);
}
// End AssetUserData interface
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Begin own functions

bool UEGQuestGraph::IsDefinitionIdWellFormed() const
{
	const FString Id = DefinitionId.ToString();

	FString IdNamespace, IdName;
	if (!Id.Split(TEXT("."), &IdNamespace, &IdName))
	{
		return false;
	}

	// Exactly one separator: "A.B.C" is rejected rather than read as namespace "A" name "B.C", so that
	// there is one spelling of an id and not several that compare unequal.
	return !IdNamespace.IsEmpty() && !IdName.IsEmpty() && !IdName.Contains(TEXT("."));
}

#if WITH_EDITOR

void UEGQuestGraph::CreateGraph()
{
	// The Graph will only be null if this is the first time we are creating the graph for the Quest.
	// After the Quest asset is saved, the Quest will get the quest from the serialized uasset.
	if (QuestGraph != nullptr)
	{
		return;
	}

	if (StartNodes.Num() == 0 || !IsValid(StartNodes[0]))
	{
		StartNodes.Add(ConstructQuestNode<UEGQuestNode_Start>());
	}

	FEGQuestLogger::Get().Debugf(TEXT("Creating graph for Quest = `%s`"), *GetPathName());
	QuestGraph = GetQuestEditorAccess()->CreateNewQuestEdGraph(this);

	// Give the schema a chance to fill out any required nodes
	QuestGraph->GetSchema()->CreateDefaultNodesForGraph(*QuestGraph);
	MarkPackageDirty();
}

void UEGQuestGraph::ClearGraph()
{
	if (!IsValid(QuestGraph))
	{
		return;
	}

	FEGQuestLogger::Get().Debugf(TEXT("Clearing graph for Quest = `%s`"), *GetPathName());
	GetQuestEditorAccess()->RemoveAllGraphNodes(this);

	// Give the schema a chance to fill out any required nodes
	QuestGraph->GetSchema()->CreateDefaultNodesForGraph(*QuestGraph);
	MarkPackageDirty();
}

void UEGQuestGraph::CompileQuestNodesFromGraphNodes()
{
	if (!bCompileQuest)
	{
		return;
	}

	FEGQuestLogger::Get().Infof(TEXT("Compiling Quest = `%s` (Graph data -> Quest data)`"), *GetPathName());
	GetQuestEditorAccess()->CompileQuestNodesFromGraphNodes(this);
}
#endif // #if WITH_EDITOR

void UEGQuestGraph::ImportFromFile()
{
	// Simply ignore reloading
	const EEGQuestGraphTextFormat TextFormat = GetDefault<UEGQuestPluginSettings>()->QuestTextFormat;
	if (TextFormat == EEGQuestGraphTextFormat::None)
	{
		UpdateAndRefreshData();
		return;
	}

	ImportFromFileFormat(TextFormat);
}

void UEGQuestGraph::ImportFromFileFormat(EEGQuestGraphTextFormat TextFormat)
{
	const bool bHasExtension = UEGQuestPluginSettings::HasTextFileExtension(TextFormat);
	const FString& TextFileName = GetTextFilePathName(TextFormat);

	// Nothing to do
	IFileManager& FileManager = IFileManager::Get();
	if (!bHasExtension)
	{
		// Useful For debugging
		if (TextFormat == EEGQuestGraphTextFormat::All)
		{
			// Import from all
			const int32 TextFormatsNum = static_cast<int32>(EEGQuestGraphTextFormat::NumTextFormats);
			for (int32 TextFormatIndex = static_cast<int32>(EEGQuestGraphTextFormat::StartTextFormats);
					   TextFormatIndex < TextFormatsNum; TextFormatIndex++)
			{
				const EEGQuestGraphTextFormat CurrentTextFormat = static_cast<EEGQuestGraphTextFormat>(TextFormatIndex);
				const FString& CurrentTextFileName = GetTextFilePathName(CurrentTextFormat);
				if (FileManager.FileExists(*CurrentTextFileName))
				{
					ImportFromFileFormat(CurrentTextFormat);
				}
			}
		}
		return;
	}

	// File does not exist abort
	if (!FileManager.FileExists(*TextFileName))
	{
		FEGQuestLogger::Get().Errorf(TEXT("Reloading data for Quest = `%s` FROM file = `%s` FAILED, because the file does not exist"), *GetPathName(), *TextFileName);
		return;
	}

	// Clear data first
	Nodes.Empty();
	StartNodes.Empty();

	// TODO handle Name == NAME_None or invalid filename
	FEGQuestLogger::Get().Infof(TEXT("Reloading data for Quest = `%s` FROM file = `%s`"), *GetPathName(), *TextFileName);

	// TODO(vampy): Check for errors
	check(TextFormat != EEGQuestGraphTextFormat::None);
	switch (TextFormat)
	{
		case EEGQuestGraphTextFormat::JSON:
		{
			FEGQuestJsonParser JsonParser;
			JsonParser.InitializeParser(TextFileName);
			JsonParser.ReadAllProperty(GetClass(), this, this);
			break;
		}
		default:
			checkNoEntry();
			break;
	}

	if (StartNodes.Num() == 0)
	{
		StartNodes.Add(ConstructQuestNode<UEGQuestNode_Start>());
	}

	// TODO(vampy): validate if data is legit, indicies exist and that sort.
	// Check if Guid is not a duplicate
	const TArray<UEGQuestGraph*> DuplicateQuests = UEGQuestManager::GetQuestsWithDuplicateGUIDs();
	if (DuplicateQuests.Num() > 0)
	{
		if (DuplicateQuests.Contains(this))
		{
			// found duplicate of this Quest
			RegenerateGUID();
			FEGQuestLogger::Get().Warningf(
				TEXT("Creating new GUID = `%s` for Quest = `%s` because the input file contained a duplicate GUID."),
				*GUID.ToString(), *GetPathName()
			);
		}
		else
		{
			// We have bigger problems on our hands
			FEGQuestLogger::Get().Errorf(
				TEXT("Found Duplicate Quest that does not belong to this Quest = `%s`, DuplicateQuests.Num = %d"),
				*GetPathName(),  DuplicateQuests.Num()
			);
		}
	}

	Name = GetQuestFName();

	if (DefinitionId.IsNone())
	{
		// A text file written before DefinitionId existed carries no id, and this asset's own id (if it
		// had one) was just wiped along with everything else the parser overwrote.
		DefinitionId = MakeSeededQuestDefinitionId(this);
	}

#if WITH_EDITORONLY_DATA
	// An import replaces Nodes and StartNodes wholesale, so whatever PostLoad concluded about *this
	// asset* no longer describes the nodes now in it: the flag has to be re-derived from what was
	// actually parsed, in both directions. An old file dropped into a migrated asset needs seeding
	// even though the .uasset was current (PostLoad would have cleared the flag); a current file
	// parsed into an old asset must NOT be re-seeded, or the priorities the designer authored in the
	// file would be overwritten by canvas layout on the next compile.
	//
	// Version is the discriminator because the parser just read it out of the file, so it describes
	// the parsed data rather than the asset - and it is exact, not a guess: priorities did not exist
	// before AddAuthoredRoutePriorities, so a file below that version provably has none. PreSave keeps
	// it honest by stamping LatestVersion before the export.
	bNeedsPriorityMigration = Version < FEGQuestGraphObjectVersion::AddAuthoredRoutePriorities;
#endif

	UpdateAndRefreshData(true);
}

void UEGQuestGraph::OnPreAssetSaved()
{
#if WITH_EDITOR
	// Compile, graph data -> quest data
	CompileQuestNodesFromGraphNodes();

	// Saving the quest also saves its embedded script blueprint (it lives in this package), so make
	// sure what saves is compiled and QuestScriptClass points at it.
	if (GetQuestEditorAccess().IsValid())
	{
		GetQuestEditorAccess()->RefreshQuestScriptBlueprint(this);
	}
#endif

	// Save file, quest data -> text file (.quest)
	UpdateAndRefreshData(true);
	ExportToFile();
}

void UEGQuestGraph::ExportToFile() const
{
	const EEGQuestGraphTextFormat TextFormat = GetDefault<UEGQuestPluginSettings>()->QuestTextFormat;
	if (TextFormat == EEGQuestGraphTextFormat::None)
	{
		// Simply ignore saving
		return;
	}

	ExportToFileFormat(TextFormat);
}

void UEGQuestGraph::ExportToFileFormat(EEGQuestGraphTextFormat TextFormat) const
{
	// TODO(vampy): Check for errors
	const bool bHasExtension = UEGQuestPluginSettings::HasTextFileExtension(TextFormat);
	const FString& TextFileName = GetTextFilePathName(TextFormat);
	if (bHasExtension)
	{
		FEGQuestLogger::Get().Infof(TEXT("Exporting data for Quest = `%s` TO file = `%s`"), *GetPathName(), *TextFileName);
	}

	switch (TextFormat)
	{
		case EEGQuestGraphTextFormat::JSON:
		{
			FEGQuestJsonWriter JsonWriter;
			JsonWriter.Write(GetClass(), this);
			JsonWriter.ExportToFile(TextFileName);
			break;
		}
		case EEGQuestGraphTextFormat::All:
		{
			// Useful for debugging
			// Export to all  formats
			const int32 TextFormatsNum = static_cast<int32>(EEGQuestGraphTextFormat::NumTextFormats);
			for (int32 TextFormatIndex = static_cast<int32>(EEGQuestGraphTextFormat::StartTextFormats);
					   TextFormatIndex < TextFormatsNum; TextFormatIndex++)
			{
				const EEGQuestGraphTextFormat CurrentTextFormat = static_cast<EEGQuestGraphTextFormat>(TextFormatIndex);
				ExportToFileFormat(CurrentTextFormat);
			}
			break;
		}
		default:
			// It Should not have any extension
			check(!bHasExtension);
			break;
	}
}

void UEGQuestGraph::RebuildAndUpdateNode(UEGQuestNode* Node, const UEGQuestPluginSettings& Settings, bool bUpdateTextsNamespacesAndKeys)
{
	static constexpr bool bUpdateGraphNode = false;

	Node->RebuildTextArguments(bUpdateGraphNode);
	Node->UpdateTextsValuesFromDefaultsAndRemappings(Settings, bUpdateGraphNode);
	if (bUpdateTextsNamespacesAndKeys)
	{
		Node->UpdateTextsNamespacesAndKeys(Settings, bUpdateGraphNode);
	}
	Node->UpdateGraphNode();
}

void UEGQuestGraph::UpdateAndRefreshData(bool bUpdateTextsNamespacesAndKeys)
{
	FEGQuestLogger::Get().Infof(TEXT("Refreshing data for Quest = `%s`"), *GetPathName());
	const UEGQuestPluginSettings* Settings = GetDefault<UEGQuestPluginSettings>();

	for (UEGQuestNode* StartNode : StartNodes)
	{
		RebuildAndUpdateNode(StartNode, *Settings, bUpdateTextsNamespacesAndKeys);
	}

	for (UEGQuestNode* Node : Nodes)
	{
		RebuildAndUpdateNode(Node, *Settings, bUpdateTextsNamespacesAndKeys);
	}
}
FGuid UEGQuestGraph::GetNodeGUIDForIndex(int32 NodeIndex) const
{
	if (IsValidNodeIndex(NodeIndex))
	{
		return Nodes[NodeIndex]->GetGUID();
	}

	// Invalid GUID
	return FGuid{};
}

int32 UEGQuestGraph::GetNodeIndexForGUID(const FGuid& NodeGUID) const
{
	if (const int32* NodeIndexPtr = NodesGUIDToIndexMap.Find(NodeGUID))
	{
		return *NodeIndexPtr;
	}

	return INDEX_NONE;
}

void UEGQuestGraph::SetStartNodes(TArray<UEGQuestNode*> InStartNodes)
{
	StartNodes = InStartNodes;
	// UpdateGUIDToIndexMap(StartNode, INDEX_NONE);
}

void UEGQuestGraph::SetNodes(const TArray<UEGQuestNode*>& InNodes)
{
	Nodes = InNodes;
	for (int32 NodeIndex = 0; NodeIndex < Nodes.Num(); NodeIndex++)
	{
		UpdateGUIDToIndexMap(Nodes[NodeIndex], NodeIndex);
	}
}

void UEGQuestGraph::SetNode(int32 NodeIndex, UEGQuestNode* InNode)
{
	if (!IsValidNodeIndex(NodeIndex) || !InNode)
	{
		return;
	}

	Nodes[NodeIndex] = InNode;
	UpdateGUIDToIndexMap(InNode, NodeIndex);
}

void UEGQuestGraph::UpdateGUIDToIndexMap(const UEGQuestNode* Node, int32 NodeIndex)
{
	if (!Node || !IsValidNodeIndex(NodeIndex) || !Node->HasGUID())
	{
		return;
	}

	NodesGUIDToIndexMap.Add(Node->GetGUID(), NodeIndex);
}

bool UEGQuestGraph::IsEndNode(int32 NodeIndex) const
{
	if (!Nodes.IsValidIndex(NodeIndex))
	{
		return false;
	}

	return Nodes[NodeIndex]->IsA<UEGQuestNode_End>();
}

FString UEGQuestGraph::GetTextFilePathName(bool bAddExtension/* = true*/) const
{
	return GetTextFilePathName(GetDefault<UEGQuestPluginSettings>()->QuestTextFormat, bAddExtension);
}

FString UEGQuestGraph::GetTextFilePathName(EEGQuestGraphTextFormat TextFormat, bool bAddExtension/* = true*/) const
{
	// Extract filename from path
	// NOTE: this is not a filesystem path, it is an unreal path 'Outermost.[Outer:]Name'
	// Usually GetPathName works, but the path name might be weird.
	// FSoftObjectPath(this).ToString(); which does call this function GetPathName() but it returns a legit clean path
	// if it is in the wrong format
	FString TextFileName = GetTextFilePathNameFromAssetPathName(FSoftObjectPath(this).ToString());
	if (bAddExtension)
	{
		// Modify the extension of the base text file depending on the extension
		TextFileName += UEGQuestPluginSettings::GetTextFileExtension(TextFormat);
	}

	return TextFileName;
}

bool UEGQuestGraph::DeleteTextFileForTextFormat(EEGQuestGraphTextFormat TextFormat) const
{
	return DeleteTextFileForExtension(UEGQuestPluginSettings::GetTextFileExtension(TextFormat));
}

bool UEGQuestGraph::DeleteTextFileForExtension(const FString& FileExtension) const
{
	const FString TextFilePathName = GetTextFilePathName(false);
	if (TextFilePathName.IsEmpty())
	{
		// Memory corruption? tread carefully here
		FEGQuestLogger::Get().Errorf(
			TEXT("Can't delete text file for Quest = `%s` because the file path name is empty :O"),
			*GetPathName()
		);
		return false;
	}

	const FString FullPathName = TextFilePathName + FileExtension;
	return FEGQuestHelper::DeleteFile(FullPathName);
}

bool UEGQuestGraph::DeleteAllTextFiles() const
{
	bool bStatus = true;
	for (const FString& FileExtension : GetDefault<UEGQuestPluginSettings>()->GetAllTextFileExtensions())
	{
		bStatus &= DeleteTextFileForExtension(FileExtension);
	}
	return bStatus;
}

bool UEGQuestGraph::IsInProjectDirectory() const
{
	return FEGQuestHelper::IsPathInProjectDirectory(GetPathName());
}

FString UEGQuestGraph::GetTextFilePathNameFromAssetPathName(const FString& AssetPathName)
{
	static const TCHAR* Separator = TEXT("/");

	// Get rid of the extension from `filename.extension` from the end of the path
	FString PathName = FPaths::GetBaseFilename(AssetPathName, false);

	// Get rid of the first folder, Game/ or Name/ (if in the plugins dir) from the beginning of the path.
	// Are we in the game directory?
	FString ContentDir = FPaths::ProjectContentDir();
	if (!PathName.RemoveFromStart(TEXT("/Game/")))
	{
		// We are in the plugins dir
		TArray<FString> PathParts;
		PathName.ParseIntoArray(PathParts, Separator);
		if (PathParts.Num() > 0)
		{
			const FString PluginName = PathParts[0];
			const FString PluginDir = FPaths::ProjectPluginsDir() / PluginName;

			// Plugin exists
			if (FPaths::DirectoryExists(PluginDir))
			{
				ContentDir = PluginDir / TEXT("Content/");
			}

			// remove plugin name
			PathParts.RemoveAt(0);
			PathName = FString::Join(PathParts, Separator);
		}
	}

	return ContentDir + PathName;
}


// End own functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
