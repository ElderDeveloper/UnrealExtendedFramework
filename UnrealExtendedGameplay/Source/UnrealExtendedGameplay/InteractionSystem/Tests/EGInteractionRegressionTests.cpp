// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "NativeGameplayTags.h"
#include "UObject/UnrealType.h"
#include "UnrealExtendedGameplay/InteractionSystem/EGInteractionProviderComponent.h"
#include "UnrealExtendedGameplay/InteractionSystem/EGInteractionSystemComponent.h"
#include "UnrealExtendedGameplay/InteractionSystem/Interactions/EGInteraction_Hold.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EGTestInteraction, "ExtendedGameplay.Test.Interaction");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EGTestOriginalInput, "ExtendedGameplay.Test.OriginalInput");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EGTestUpdatedInput, "ExtendedGameplay.Test.UpdatedInput");

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGInteractionRegressionTest,
	"UnrealExtendedGameplay.InteractionSystem.Regression",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGInteractionRegressionTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues WorldOptions = UWorld::InitializationValues()
		.AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false)
		.SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
		true, ERHIFeatureLevel::Num, &WorldOptions);
	if (!TestNotNull(TEXT("Test world"), World))
	{
		return false;
	}

	AActor* Interactor = World->SpawnActor<AActor>();
	UEGInteractionSystemComponent* System = NewObject<UEGInteractionSystemComponent>(Interactor);
	Interactor->AddInstanceComponent(System);
	System->RegisterComponent();

	const auto MakeBox = [World](const FVector& Location, float Extent)
	{
		AActor* Actor = World->SpawnActor<AActor>();
		UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
		Actor->SetRootComponent(Box);
		Actor->AddInstanceComponent(Box);
		Box->SetBoxExtent(FVector(Extent));
		Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Box->SetCollisionResponseToAllChannels(ECR_Block);
		Box->RegisterComponent();
		Actor->SetActorLocation(Location);
		return Box;
	};
	UBoxComponent* TargetBox = MakeBox(FVector(150, 0, 0), 25);
	AActor* Target = TargetBox->GetOwner();
	UEGInteractionProviderComponent* Provider = NewObject<UEGInteractionProviderComponent>(Target);
	Target->AddInstanceComponent(Provider);
	Provider->RegisterComponent();

	FInstancedStruct Definition;
	Definition.InitializeAs<FEGInteractionDef_Hold>();
	FEGInteractionDef_Hold& HoldDefinition = Definition.GetMutable<FEGInteractionDef_Hold>();
	HoldDefinition.Id = TAG_EGTestInteraction;
	HoldDefinition.InputTag = TAG_EGTestOriginalInput;
	HoldDefinition.InteractionClass = UEGInteraction_Hold::StaticClass();
	HoldDefinition.HoldTime = 10.0f;
	const int32 Revision = Provider->GetDefinitionsRevision();
	Provider->AddDefinition(Definition);
	TestTrue(TEXT("Adding a definition bumps its revision"), Provider->GetDefinitionsRevision() > Revision);
	const FProperty* DefinitionsProperty = FindFProperty<FProperty>(Provider->GetClass(), TEXT("Definitions"));
	TestTrue(TEXT("Blueprint cannot bypass definition mutation functions"),
		DefinitionsProperty && DefinitionsProperty->HasAnyPropertyFlags(CPF_BlueprintReadOnly));

	System->bRequiresLineOfSight = false;
	FHitResult ServerHit;
	FInstancedStruct ServerDefinition;
	const FHitResult ForgedHit(Target, TargetBox, FVector(999, 20, 0), FVector::UpVector);
	TestTrue(TEXT("Valid aim still resolves when the client reports a forged distance"),
		System->ValidateServerRequest(Provider, TAG_EGTestInteraction, ForgedHit, TargetBox, ServerHit, ServerDefinition));
	TestTrue(TEXT("Authority hit is on the actual target collision"),
		ServerHit.GetComponent() == TargetBox && FMath::IsNearlyEqual(ServerHit.ImpactPoint.X, 125.0, 0.1));
	TestFalse(TEXT("Client impact point is not forwarded"), ServerHit.ImpactPoint.Equals(ForgedHit.ImpactPoint));

	const FHitResult Miss(Target, TargetBox, FVector(150, 500, 0), FVector::UpVector);
	TestFalse(TEXT("Sideways forged aim is rejected without occlusion checks"),
		System->ValidateServerRequest(Provider, TAG_EGTestInteraction, Miss, TargetBox, ServerHit, ServerDefinition));
	Target->SetActorLocation(FVector(-150, 0, 0));
	const FHitResult Behind(Target, TargetBox, FVector(-125, 0, 0), FVector::UpVector);
	TestFalse(TEXT("A real target behind the interactor is rejected"),
		System->ValidateServerRequest(Provider, TAG_EGTestInteraction, Behind, TargetBox, ServerHit, ServerDefinition));
	Target->SetActorLocation(FVector(150, 0, 0));

	UBoxComponent* Blocker = MakeBox(FVector(75, 0, 0), 10);
	const FHitResult ValidHit(Target, TargetBox, FVector(125, 0, 0), FVector::UpVector);
	TestTrue(TEXT("Occlusion disabled permits a target behind a blocker"),
		System->ValidateServerRequest(Provider, TAG_EGTestInteraction, ValidHit, TargetBox, ServerHit, ServerDefinition));
	System->bRequiresLineOfSight = true;
	TestFalse(TEXT("Occlusion enabled rejects the same blocked target"),
		System->ValidateServerRequest(Provider, TAG_EGTestInteraction, ValidHit, TargetBox, ServerHit, ServerDefinition));
	Blocker->GetOwner()->Destroy();
	TestTrue(TEXT("Unobstructed target is accepted with occlusion enabled"),
		System->ValidateServerRequest(Provider, TAG_EGTestInteraction, ValidHit, TargetBox, ServerHit, ServerDefinition));

	FEGInteractionSpec Spec;
	Spec.Handle = 1;
	Spec.Id = TAG_EGTestInteraction;
	Spec.Provider = Provider;
	Spec.Definition = Definition;
	UEGInteraction_Hold* Hold = NewObject<UEGInteraction_Hold>(System);
	Hold->bSendProviderEvents = false;
	Hold->InitInteraction(System, Spec);
	Hold->BeginActivation(1, false);
	TestTrue(TEXT("Hold is active before definition edit"), Hold->IsActive());
	Definition.GetMutable<FEGInteractionDef_Hold>().InputTag = TAG_EGTestUpdatedInput;
	Hold->UpdateSpecDefinition(Definition);
	System->ReleaseSlot(TAG_EGTestOriginalInput);
	TestFalse(TEXT("Original input release cancels hold after definition edit"), Hold->IsActive());
	TestFalse(TEXT("Cancellation releases the target claim"), Provider->IsClaimed(TAG_EGTestInteraction));

	Hold->UpdateSpecDefinition(Spec.Definition);
	Hold->BeginServerPending(2);
	Hold->UpdateSpecDefinition(Definition);
	TestTrue(TEXT("Pending request also retains its original input slot"),
		Hold->GetActivationInputTag() == TAG_EGTestOriginalInput);
	Hold->HandleRevoked();

	TargetBox->SetRenderCustomDepth(true);
	TargetBox->SetCustomDepthStencilValue(17);
	Provider->NotifyFocus(true, TargetBox);
	TestEqual(TEXT("Focus applies its stencil"), TargetBox->CustomDepthStencilValue, Provider->HighlightStencilValue);
	Provider->bHighlightOnFocus = false;
	Provider->NotifyFocus(false, TargetBox);
	TestTrue(TEXT("Blur restores preexisting custom depth after option is disabled"), bool(TargetBox->bRenderCustomDepth));
	TestEqual(TEXT("Blur restores preexisting stencil after option is disabled"), TargetBox->CustomDepthStencilValue, 17);

	World->DestroyWorld(false);
	return true;
}

#endif
