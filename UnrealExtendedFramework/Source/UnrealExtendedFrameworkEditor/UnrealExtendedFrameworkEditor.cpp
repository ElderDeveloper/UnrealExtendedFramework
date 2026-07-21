// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealExtendedFrameworkEditor.h"

#if WITH_EDITOR
#include "K2Node_DynamicCast.h"
#include "SGraphNode.h"
#include "SourceCodeNavigation.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "KismetNodes/SGraphNodeK2Base.h"
#include "Preferences/UnrealEdOptions.h"
#include "Localization/EELocalizationWorkbenchFeature.h"
#include "UILab/EEUILabFeature.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#endif

#define LOCTEXT_NAMESPACE "FUnrealExtendedFrameworkEditorModule"



#if WITH_EDITOR
FDoubleClickCastInputProcessor& FDoubleClickCastInputProcessor::Get()
{
	static FDoubleClickCastInputProcessor Instance;
	return Instance;
}


bool FDoubleClickCastInputProcessor::HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp,const FPointerEvent& MouseEvent)
{
    const FVector2D ScreenPos = MouseEvent.GetScreenSpacePosition();
    const TArray<TSharedRef<SWindow>>& TopLevelWindows = FSlateApplication::Get().GetInteractiveTopLevelWindows();
    const int32 UserIndex = MouseEvent.GetUserIndex();
    
    FWidgetPath WidgetsUnderCursor = FSlateApplication::Get().LocateWindowUnderMouse(ScreenPos, TopLevelWindows, false, UserIndex);

    for (int32 i = 0; i < WidgetsUnderCursor.Widgets.Num(); ++i)
    {
        TSharedPtr<SWidget> CurrentWidget = WidgetsUnderCursor.Widgets[i].GetWidgetPtr()->AsShared();
        if (CurrentWidget->GetType() != TEXT("SGraphNodeK2Default"))
            continue;

        const TSharedPtr<SGraphNode> GraphNodeWidget = StaticCastSharedPtr<SGraphNode>(CurrentWidget);
        UEdGraphNode* EdGraphNode = GraphNodeWidget->GetNodeObj();
        const UK2Node_DynamicCast* DynamicCastNode = Cast<UK2Node_DynamicCast>(EdGraphNode);
        
        if (!DynamicCastNode || !DynamicCastNode->TargetType)
            continue;

        const UClass* TargetType = DynamicCastNode->TargetType;

        if (TargetType->IsInBlueprint())
        {
            if (UAssetEditorSubsystem* AssetEditorSubsystem = GUnrealEd->GetEditorSubsystem<UAssetEditorSubsystem>())
            {
                AssetEditorSubsystem->OpenEditorForAsset(TargetType->ClassGeneratedBy);
            }
            continue;
        }

        if (!GUnrealEd || !GUnrealEd->GetUnrealEdOptions()->IsCPPAllowed())
            continue;

        FString NativeHeaderPath;
        const bool bFoundHeader = FSourceCodeNavigation::FindClassHeaderPath(TargetType, NativeHeaderPath);

        if (const bool bHeaderExists = IFileManager::Get().FileSize(*NativeHeaderPath) != INDEX_NONE; bFoundHeader && bHeaderExists)
        {
            const FString AbsPath = FPaths::ConvertRelativePathToFull(NativeHeaderPath);
            FSourceCodeNavigation::OpenSourceFile(AbsPath);
        }
    }
    return IInputProcessor::HandleMouseButtonDoubleClickEvent(SlateApp, MouseEvent);
}
#endif // WITH_EDITOR

void FUnrealExtendedFrameworkEditorModule::StartupModule()
{
#if WITH_EDITOR
    InputProcessor = MakeShared<FDoubleClickCastInputProcessor>();
    if (FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor.ToSharedRef());
    }

    RegisterEditorFeatures();
#endif
}


void FUnrealExtendedFrameworkEditorModule::ShutdownModule()
{
#if WITH_EDITOR
    UnregisterEditorFeatures();
#endif
}


#if WITH_EDITOR
void FUnrealExtendedFrameworkEditorModule::RegisterEditorFeatures()
{
    // Tabs require a live Slate application; commandlets and headless runs skip registration.
    if (!FSlateApplication::IsInitialized())
    {
        return;
    }

    const TSharedRef<FWorkspaceItem> ExtendedFrameworkGroup =
        WorkspaceMenu::GetMenuStructure().GetToolsCategory()->AddGroup(
            LOCTEXT("ExtendedFrameworkGroup", "Extended Framework"),
            LOCTEXT("ExtendedFrameworkGroupTooltip", "Extended Framework editor tools"),
            FSlateIcon(),
            true);

    // Each feature registers independently; one failing feature must never block the module.
    UILabFeature = MakeUnique<FEEUILabFeature>();
    UILabFeature->Register(ExtendedFrameworkGroup);

    LocalizationWorkbenchFeature = MakeUnique<FEELocalizationWorkbenchFeature>();
    LocalizationWorkbenchFeature->Register(ExtendedFrameworkGroup);
}


void FUnrealExtendedFrameworkEditorModule::UnregisterEditorFeatures()
{
    if (UILabFeature)
    {
        UILabFeature->Unregister();
        UILabFeature.Reset();
    }

    if (LocalizationWorkbenchFeature)
    {
        LocalizationWorkbenchFeature->Unregister();
        LocalizationWorkbenchFeature.Reset();
    }
}
#endif // WITH_EDITOR


#undef LOCTEXT_NAMESPACE
	
#if WITH_EDITOR
IMPLEMENT_MODULE(FUnrealExtendedFrameworkEditorModule, UnrealExtendedFrameworkEditor)
#endif // WITH_EDITOR
