// Copyright Moon Punch Games. All Rights Reserved.

#include "EFUIFixture.h"

void UEFUIFixture::GetChainRootFirst(TArray<const UEFUIFixture*>& OutChain) const
{
	TSet<const UEFUIFixture*> Visited;
	const UEFUIFixture* Current = this;
	while (Current && !Visited.Contains(Current))
	{
		Visited.Add(Current);
		OutChain.Insert(Current, 0);
		Current = Current->ParentFixture;
	}
}

TSoftClassPtr<UUserWidget> UEFUIFixture::GetEffectiveWidgetClass() const
{
	TArray<const UEFUIFixture*> Chain;
	GetChainRootFirst(Chain);
	for (int32 i = Chain.Num() - 1; i >= 0; --i)
	{
		if (!Chain[i]->WidgetClass.IsNull())
		{
			return Chain[i]->WidgetClass;
		}
	}
	return nullptr;
}

EEFUIFixtureHostMode UEFUIFixture::GetEffectiveHostMode() const
{
	// Sandbox is the "stickier" requirement: if any fixture in the chain needs it, use it.
	TArray<const UEFUIFixture*> Chain;
	GetChainRootFirst(Chain);
	for (const UEFUIFixture* Fixture : Chain)
	{
		if (Fixture->HostMode == EEFUIFixtureHostMode::RuntimeFaithfulSandbox)
		{
			return EEFUIFixtureHostMode::RuntimeFaithfulSandbox;
		}
	}
	return EEFUIFixtureHostMode::FastHost;
}

FIntPoint UEFUIFixture::GetEffectiveResolution() const
{
	TArray<const UEFUIFixture*> Chain;
	GetChainRootFirst(Chain);
	for (int32 i = Chain.Num() - 1; i >= 0; --i)
	{
		if (Chain[i]->bOverrideResolution)
		{
			return Chain[i]->Resolution;
		}
	}
	return FIntPoint(1920, 1080);
}

float UEFUIFixture::GetEffectiveDPIScale() const
{
	TArray<const UEFUIFixture*> Chain;
	GetChainRootFirst(Chain);
	for (int32 i = Chain.Num() - 1; i >= 0; --i)
	{
		if (Chain[i]->bOverrideDPIScale)
		{
			return Chain[i]->DPIScale;
		}
	}
	return 1.0f;
}

bool UEFUIFixture::HasEffectiveDPIOverride() const
{
	TArray<const UEFUIFixture*> Chain;
	GetChainRootFirst(Chain);
	for (const UEFUIFixture* Fixture : Chain)
	{
		if (Fixture->bOverrideDPIScale)
		{
			return true;
		}
	}
	return false;
}

float UEFUIFixture::GetEffectiveSafeZonePercent() const
{
	TArray<const UEFUIFixture*> Chain;
	GetChainRootFirst(Chain);
	for (int32 i = Chain.Num() - 1; i >= 0; --i)
	{
		if (Chain[i]->bOverrideSafeZonePercent)
		{
			return Chain[i]->SafeZonePercent;
		}
	}
	return 0.0f;
}

FName UEFUIFixture::GetEffectivePlatformProfile() const
{
	TArray<const UEFUIFixture*> Chain;
	GetChainRootFirst(Chain);
	for (int32 i = Chain.Num() - 1; i >= 0; --i)
	{
		if (!Chain[i]->PlatformProfile.IsNone())
		{
			return Chain[i]->PlatformProfile;
		}
	}
	return NAME_None;
}

FString UEFUIFixture::GetEffectiveCulture() const
{
	TArray<const UEFUIFixture*> Chain;
	GetChainRootFirst(Chain);
	for (int32 i = Chain.Num() - 1; i >= 0; --i)
	{
		if (!Chain[i]->Culture.IsEmpty())
		{
			return Chain[i]->Culture;
		}
	}
	return FString();
}

FName UEFUIFixture::GetEffectiveInitialInputDevice() const
{
	TArray<const UEFUIFixture*> Chain;
	GetChainRootFirst(Chain);
	for (int32 i = Chain.Num() - 1; i >= 0; --i)
	{
		if (!Chain[i]->InitialInputDevice.IsNone())
		{
			return Chain[i]->InitialInputDevice;
		}
	}
	return NAME_None;
}

FName UEFUIFixture::GetEffectiveInitialFocusTarget() const
{
	TArray<const UEFUIFixture*> Chain;
	GetChainRootFirst(Chain);
	for (int32 i = Chain.Num() - 1; i >= 0; --i)
	{
		if (!Chain[i]->InitialFocusTarget.IsNone())
		{
			return Chain[i]->InitialFocusTarget;
		}
	}
	return NAME_None;
}

TSoftClassPtr<UGameInstance> UEFUIFixture::GetEffectiveSandboxGameInstanceClass() const
{
	TArray<const UEFUIFixture*> Chain;
	GetChainRootFirst(Chain);
	for (int32 i = Chain.Num() - 1; i >= 0; --i)
	{
		if (!Chain[i]->SandboxGameInstanceClass.IsNull())
		{
			return Chain[i]->SandboxGameInstanceClass;
		}
	}
	return nullptr;
}

TSoftClassPtr<AGameModeBase> UEFUIFixture::GetEffectiveSandboxGameModeClass() const
{
	TArray<const UEFUIFixture*> Chain;
	GetChainRootFirst(Chain);
	for (int32 i = Chain.Num() - 1; i >= 0; --i)
	{
		if (!Chain[i]->SandboxGameModeClass.IsNull())
		{
			return Chain[i]->SandboxGameModeClass;
		}
	}
	return nullptr;
}

void UEFUIFixture::GetEffectiveRequiredSubsystems(TArray<TSoftClassPtr<UGameInstanceSubsystem>>& OutSubsystems) const
{
	TArray<const UEFUIFixture*> Chain;
	GetChainRootFirst(Chain);
	for (const UEFUIFixture* Fixture : Chain)
	{
		for (const TSoftClassPtr<UGameInstanceSubsystem>& Subsystem : Fixture->RequiredSubsystems)
		{
			OutSubsystems.AddUnique(Subsystem);
		}
	}
}

void UEFUIFixture::GetEffectiveInputMappingContexts(TArray<TSoftObjectPtr<UInputMappingContext>>& OutContexts) const
{
	TArray<const UEFUIFixture*> Chain;
	GetChainRootFirst(Chain);
	for (const UEFUIFixture* Fixture : Chain)
	{
		for (const TSoftObjectPtr<UInputMappingContext>& Context : Fixture->InputMappingContexts)
		{
			OutContexts.AddUnique(Context);
		}
	}
}

void UEFUIFixture::GetEffectiveProviders(TArray<UEFUIFixtureProviderConfig*>& OutProviders) const
{
	TArray<const UEFUIFixture*> Chain;
	GetChainRootFirst(Chain);
	for (const UEFUIFixture* Fixture : Chain)
	{
		for (const TObjectPtr<UEFUIFixtureProviderConfig>& Provider : Fixture->Providers)
		{
			if (Provider)
			{
				OutProviders.Add(Provider);
			}
		}
	}
}

void UEFUIFixture::GetEffectiveTags(TArray<FName>& OutTags) const
{
	TArray<const UEFUIFixture*> Chain;
	GetChainRootFirst(Chain);
	for (const UEFUIFixture* Fixture : Chain)
	{
		for (const FName& Tag : Fixture->Tags)
		{
			OutTags.AddUnique(Tag);
		}
	}
}
