// Copyright Moon Punch Games. All Rights Reserved.

#include "Testing/EFTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

EFTest::FScopedGameInstance::FScopedGameInstance()
{
	if (!GEngine)
	{
		return;
	}

	GameInstance.Reset(NewObject<UGameInstance>(GEngine));
	// Creates a world context and an empty world, then runs Init(), which initializes subsystems.
	GameInstance->InitializeStandalone();
}

EFTest::FScopedGameInstance::~FScopedGameInstance()
{
	if (!GameInstance.IsValid())
	{
		return;
	}

	UWorld* World = GameInstance->GetWorld();
	GameInstance->Shutdown();

	if (World && GEngine)
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(/*bInformEngineOfWorld*/ false);
	}

	GameInstance.Reset();
}

UWorld* EFTest::FScopedGameInstance::GetWorld() const
{
	return GameInstance.IsValid() ? GameInstance->GetWorld() : nullptr;
}

bool EFTest::LoadPluginFixture(const FString& PluginName, const FString& RelativePath, FString& OutText)
{
	OutText.Reset();
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName);
	if (!Plugin.IsValid())
	{
		return false;
	}
	return FFileHelper::LoadFileToString(OutText, *FPaths::Combine(Plugin->GetBaseDir(), RelativePath));
}

bool EFTest::LoadPluginJsonFixture(const FString& PluginName, const FString& RelativePath, FEFJsonObject& OutObject)
{
	OutObject = FEFJsonObject();
	FString Text;
	TSharedPtr<FJsonObject> Parsed;
	if (!LoadPluginFixture(PluginName, RelativePath, Text) || !EFJson::ParseObject(Text, Parsed))
	{
		return false;
	}
	OutObject = FEFJsonObject(Parsed);
	return true;
}

FEFWaitUntilCommand::FEFWaitUntilCommand(FAutomationTestBase* InTest, const FString& InDescription, TFunction<bool()> InCondition, float InTimeoutSeconds)
	: Test(InTest)
	, Description(InDescription)
	, Condition(MoveTemp(InCondition))
	, TimeoutSeconds(InTimeoutSeconds)
{
}

bool FEFWaitUntilCommand::Update()
{
	const double Now = FPlatformTime::Seconds();
	if (FirstUpdateSeconds < 0.0)
	{
		FirstUpdateSeconds = Now;
	}

	if (Condition && Condition())
	{
		return true;
	}

	if (Now - FirstUpdateSeconds > TimeoutSeconds)
	{
		if (Test)
		{
			Test->AddError(FString::Printf(TEXT("Timed out after %.1f seconds waiting for %s."), TimeoutSeconds, *Description));
		}
		return true;
	}
	return false;
}

#endif // WITH_DEV_AUTOMATION_TESTS
