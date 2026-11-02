// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "UnrealExtendedGameplayEditor.h"

#include "Editor/UnrealEdEngine.h"
#include "Misc/CoreDelegates.h"
#include "Placement/EGPlacementComponentVisualizers.h"
#include "UnrealEdGlobals.h"
#include "UnrealExtendedGameplay/Systems/Placement/Spatial/EGPlacementAnchorComponent.h"
#include "UnrealExtendedGameplay/Systems/Placement/Spatial/EGPlacementRegionComponent.h"

void FUnrealExtendedGameplayEditorModule::StartupModule()
{
	if (GUnrealEd)
	{
		RegisterVisualizers();
	}
	else
	{
		PostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddRaw(this, &FUnrealExtendedGameplayEditorModule::RegisterVisualizers);
	}
}

void FUnrealExtendedGameplayEditorModule::ShutdownModule()
{
	FCoreDelegates::OnPostEngineInit.Remove(PostEngineInitHandle);
	if (GUnrealEd)
	{
		for (const FName& Name : RegisteredVisualizers)
		{
			GUnrealEd->UnregisterComponentVisualizer(Name);
		}
	}
	RegisteredVisualizers.Reset();
}

void FUnrealExtendedGameplayEditorModule::RegisterVisualizers()
{
	if (!GUnrealEd || RegisteredVisualizers.Num() > 0)
	{
		return;
	}
	auto Register = [this](UClass* Class, TSharedPtr<FComponentVisualizer> Visualizer)
	{
		GUnrealEd->RegisterComponentVisualizer(Class->GetFName(), Visualizer);
		Visualizer->OnRegister();
		RegisteredVisualizers.Add(Class->GetFName());
	};
	Register(UEGPlacementRegionComponent::StaticClass(), MakeShared<FEGPlacementRegionVisualizer>());
	Register(UEGPlacementAnchorComponent::StaticClass(), MakeShared<FEGPlacementAnchorVisualizer>());
}

IMPLEMENT_MODULE(FUnrealExtendedGameplayEditorModule, UnrealExtendedGameplayEditor)
