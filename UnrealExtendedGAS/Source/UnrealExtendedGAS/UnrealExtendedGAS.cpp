#include "UnrealExtendedGAS.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealExtendedGAS, Log, All);

namespace UnrealExtendedGASConsole
{
	void DumpAbilitySystems(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;

		if (!World)
		{
			UE_LOG(LogUnrealExtendedGAS, Warning, TEXT("EGAS.DumpAbilitySystems: no world."));
			return;
		}

		int32 DumpedCount = 0;
		for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
		{
			AActor* Actor = *ActorIt;
			UAbilitySystemComponent* AbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor);
			if (!AbilitySystemComponent)
			{
				continue;
			}

			FGameplayTagContainer OwnedTags;
			AbilitySystemComponent->GetOwnedGameplayTags(OwnedTags);

			UE_LOG(LogUnrealExtendedGAS, Display, TEXT("[%s] ASC=%s Attributes=%d Tags=%s"),
				*Actor->GetName(),
				*AbilitySystemComponent->GetName(),
				AbilitySystemComponent->GetSpawnedAttributes().Num(),
				*OwnedTags.ToStringSimple());

			++DumpedCount;
		}

		UE_LOG(LogUnrealExtendedGAS, Display, TEXT("EGAS.DumpAbilitySystems: dumped %d ability system component(s)."), DumpedCount);
	}

	FAutoConsoleCommandWithWorldAndArgs DumpAbilitySystemsCommand(
		TEXT("EGAS.DumpAbilitySystems"),
		TEXT("Logs ability system components in the current world."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DumpAbilitySystems));
}

IMPLEMENT_MODULE(FDefaultModuleImpl, UnrealExtendedGAS);
