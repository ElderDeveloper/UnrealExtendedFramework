// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "Json/EFJsonTypes.h"
#include "UObject/StrongObjectPtr.h"

class UWorld;

/**
 * Shared automation-test helpers for Extended plugins. Compiled only when dev automation tests are.
 *
 * Plugins' HTTP clients become testable offline with FEFScopedHttpMock (Http/EFHttpMock.h); the
 * helpers here cover the rest: a game instance whose subsystems are initialized, waiting for an
 * asynchronous result, and loading fixtures that ship with a plugin.
 */
namespace EFTest
{
	/**
	 * A standalone game instance with its subsystems initialized and an empty world, torn down when
	 * this goes out of scope. Use it to test UGameInstanceSubsystems and async nodes:
	 *
	 *     EFTest::FScopedGameInstance Game;
	 *     UEFHttpSubsystem* Http = Game.GetSubsystem<UEFHttpSubsystem>();
	 *
	 * Keep it alive until any latent commands that use it have finished, e.g. by holding it in a
	 * TSharedPtr captured by the last command.
	 */
	class EXTENDEDFOUNDATION_API FScopedGameInstance
	{
	public:
		FScopedGameInstance();
		~FScopedGameInstance();

		FScopedGameInstance(const FScopedGameInstance&) = delete;
		FScopedGameInstance& operator=(const FScopedGameInstance&) = delete;

		UGameInstance* Get() const { return GameInstance.Get(); }
		UWorld* GetWorld() const;

		template <typename TSubsystem>
		TSubsystem* GetSubsystem() const
		{
			return GameInstance.IsValid() ? GameInstance->GetSubsystem<TSubsystem>() : nullptr;
		}

	private:
		TStrongObjectPtr<UGameInstance> GameInstance;
	};

	/** Reads a text fixture shipped with a plugin, e.g. ("UnrealExtendedPlayFab", "Tests/Fixtures/Login.json"). */
	EXTENDEDFOUNDATION_API bool LoadPluginFixture(const FString& PluginName, const FString& RelativePath, FString& OutText);

	/** Reads and parses a JSON object fixture shipped with a plugin. */
	EXTENDEDFOUNDATION_API bool LoadPluginJsonFixture(const FString& PluginName, const FString& RelativePath, FEFJsonObject& OutObject);
}

/**
 * Latent command that waits until Condition returns true, polling every frame. After TimeoutSeconds
 * it records an error on Test naming Description and stops, so a hung callback fails the test
 * instead of stalling the run.
 *
 *     ADD_LATENT_AUTOMATION_COMMAND(FEFWaitUntilCommand(this, TEXT("login callback"), [State] { return State->bDone; }));
 */
class EXTENDEDFOUNDATION_API FEFWaitUntilCommand : public IAutomationLatentCommand
{
public:
	FEFWaitUntilCommand(FAutomationTestBase* InTest, const FString& InDescription, TFunction<bool()> InCondition, float InTimeoutSeconds = 10.f);

	virtual bool Update() override;

private:
	FAutomationTestBase* Test;
	FString Description;
	TFunction<bool()> Condition;
	float TimeoutSeconds;
	double FirstUpdateSeconds = -1.0;
};

/** Latent command that runs Body once, for checks that must happen after earlier latent commands. */
class EXTENDEDFOUNDATION_API FEFRunLatentCommand : public IAutomationLatentCommand
{
public:
	explicit FEFRunLatentCommand(TFunction<void()> InBody) : Body(MoveTemp(InBody)) {}

	virtual bool Update() override
	{
		if (Body)
		{
			Body();
		}
		return true;
	}

private:
	TFunction<void()> Body;
};

#endif // WITH_DEV_AUTOMATION_TESTS
