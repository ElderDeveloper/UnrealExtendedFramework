// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGTimeDilationSubsystem.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"

UEGTimeDilationSubsystem* UEGTimeDilationSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UEGTimeDilationSubsystem>() : nullptr;
}

bool UEGTimeDilationSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UEGTimeDilationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UEGTimeDilationSubsystem, STATGROUP_Tickables);
}

// ---------------------------------------------------------------------------------------------
// Targets
// ---------------------------------------------------------------------------------------------

float UEGTimeDilationSubsystem::ReadValue(ETarget Kind, UObject* Object) const
{
	switch (Kind)
	{
	case ETarget::World:
		return UGameplayStatics::GetGlobalTimeDilation(GetWorld());
	case ETarget::Actor:
		if (const AActor* Actor = Cast<AActor>(Object))
		{
			return Actor->CustomTimeDilation;
		}
		break;
	case ETarget::AnimRate:
		if (const USkeletalMeshComponent* Mesh = Cast<USkeletalMeshComponent>(Object))
		{
			return Mesh->GlobalAnimRateScale;
		}
		break;
	}
	return 1.0f;
}

void UEGTimeDilationSubsystem::WriteValue(ETarget Kind, UObject* Object, float Value) const
{
	switch (Kind)
	{
	case ETarget::World:
		UGameplayStatics::SetGlobalTimeDilation(GetWorld(), Value);
		break;
	case ETarget::Actor:
		if (AActor* Actor = Cast<AActor>(Object))
		{
			Actor->CustomTimeDilation = Value;
		}
		break;
	case ETarget::AnimRate:
		if (USkeletalMeshComponent* Mesh = Cast<USkeletalMeshComponent>(Object))
		{
			Mesh->GlobalAnimRateScale = Value;
		}
		break;
	}
}

UEGTimeDilationSubsystem::FChannel* UEGTimeDilationSubsystem::FindChannel(ETarget Kind, const UObject* Object)
{
	return Channels.FindByPredicate([Kind, Object](const FChannel& Channel)
	{
		return Channel.Kind == Kind && (Kind == ETarget::World || Channel.Object.Get() == Object);
	});
}

UEGTimeDilationSubsystem::FChannel& UEGTimeDilationSubsystem::FindOrAddChannel(ETarget Kind, UObject* Object)
{
	if (FChannel* Existing = FindChannel(Kind, Object))
	{
		return *Existing;
	}

	FChannel& Channel = Channels.AddDefaulted_GetRef();
	Channel.Kind = Kind;
	Channel.Object = Object;
	const float Base = ReadValue(Kind, Object);
	Channel.Stack.Reset(Base);
	Channel.LastApplied = Base;
	return Channel;
}

void UEGTimeDilationSubsystem::ApplyChannel(FChannel& Channel, bool bForce)
{
	UObject* const Object = Channel.Object.Get();
	if (Channel.Kind != ETarget::World && !Object)
	{
		return;
	}

	const float Value = Channel.Stack.Advance(RealTime);
	if (bForce || !FMath::IsNearlyEqual(Value, Channel.LastApplied))
	{
		WriteValue(Channel.Kind, Object, Value);
		Channel.LastApplied = Value;
	}
}

void UEGTimeDilationSubsystem::ApplyAndPrune()
{
	for (int32 Index = Channels.Num() - 1; Index >= 0; --Index)
	{
		FChannel& Channel = Channels[Index];
		const bool bTargetGone = Channel.Kind != ETarget::World && !Channel.Object.IsValid();
		if (!bTargetGone)
		{
			ApplyChannel(Channel, false);
		}
		if (bTargetGone || !Channel.Stack.IsActive())
		{
			Channels.RemoveAtSwap(Index);
		}
	}

	// Forget handles whose request expired or whose target went away.
	for (auto It = Handles.CreateIterator(); It; ++It)
	{
		const FChannel* Channel = FindChannel(It->Value.Kind, It->Value.Object.Get());
		if (!Channel || !Channel->Stack.Contains(It->Key))
		{
			It.RemoveCurrent();
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Requests
// ---------------------------------------------------------------------------------------------

FEGTimeDilationHandle UEGTimeDilationSubsystem::PushInternal(ETarget Kind, UObject* Object, float Value, const FEGTimeDilationParams& Params)
{
	FEGTimeDilationHandle Handle;
	if (Kind != ETarget::World && !IsValid(Object))
	{
		return Handle;
	}

	NextHandleId = (NextHandleId == MAX_int32) ? 1 : NextHandleId + 1;
	Handle.Id = NextHandleId;

	FEGTimeDilationStack::FRequest Request;
	Request.Id = Handle.Id;
	Request.Value = FMath::Max(0.0f, Value);
	Request.Priority = Params.Priority;
	Request.BlendIn = Params.BlendInTime;
	Request.BlendOut = Params.BlendOutTime;
	Request.ExpireTime = Params.Duration > 0.0f ? RealTime + Params.Duration : -1.0;

	FChannel& Channel = FindOrAddChannel(Kind, Object);
	Channel.Stack.Push(Request, RealTime);
	ApplyChannel(Channel, true);

	FHandleTarget& Target = Handles.Add(Handle.Id);
	Target.Kind = Kind;
	Target.Object = Object;
	return Handle;
}

FEGTimeDilationHandle UEGTimeDilationSubsystem::PushGlobalTimeDilation(float Dilation, FEGTimeDilationParams Params)
{
	// Time dilation replicates from the server; a client's own copy would only drift.
	const UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return FEGTimeDilationHandle();
	}
	return PushInternal(ETarget::World, nullptr, Dilation, Params);
}

FEGTimeDilationHandle UEGTimeDilationSubsystem::PushActorTimeDilation(AActor* Actor, float Dilation, FEGTimeDilationParams Params)
{
	return PushInternal(ETarget::Actor, Actor, Dilation, Params);
}

FEGTimeDilationHandle UEGTimeDilationSubsystem::PushAnimRateScale(USkeletalMeshComponent* Mesh, float RateScale, FEGTimeDilationParams Params)
{
	return PushInternal(ETarget::AnimRate, Mesh, RateScale, Params);
}

void UEGTimeDilationSubsystem::RemoveTimeDilation(FEGTimeDilationHandle& Handle)
{
	if (const FHandleTarget* Target = Handles.Find(Handle.Id))
	{
		if (FChannel* Channel = FindChannel(Target->Kind, Target->Object.Get()))
		{
			if (Channel->Stack.Remove(Handle.Id, RealTime))
			{
				ApplyChannel(*Channel, false);
			}
		}
		Handles.Remove(Handle.Id);
	}
	Handle.Invalidate();
}

bool UEGTimeDilationSubsystem::IsTimeDilationActive(const FEGTimeDilationHandle& Handle) const
{
	return Handle.IsValid() && Handles.Contains(Handle.Id);
}

void UEGTimeDilationSubsystem::SetGlobalRestoreValue(float Dilation, float BlendTime)
{
	const UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	if (FChannel* Channel = FindChannel(ETarget::World, nullptr))
	{
		Channel->Stack.SetBase(FMath::Max(0.0f, Dilation), RealTime, BlendTime);
		ApplyChannel(*Channel, false);
	}
	else if (BlendTime <= 0.0f)
	{
		WriteValue(ETarget::World, nullptr, FMath::Max(0.0f, Dilation));
	}
	else
	{
		FChannel& NewChannel = FindOrAddChannel(ETarget::World, nullptr);
		NewChannel.Stack.SetBase(FMath::Max(0.0f, Dilation), RealTime, BlendTime);
	}
}

bool UEGTimeDilationSubsystem::HasGlobalRequests() const
{
	const FChannel* Channel = Channels.FindByPredicate([](const FChannel& Candidate) { return Candidate.Kind == ETarget::World; });
	return Channel && Channel->Stack.Num() > 0;
}

void UEGTimeDilationSubsystem::HitPause(const TArray<USkeletalMeshComponent*>& Meshes, float Duration, float AnimRateScale)
{
	if (Duration <= 0.0f)
	{
		return;
	}

	FEGTimeDilationParams Params;
	Params.Duration = Duration;
	Params.Priority = 100;
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		PushAnimRateScale(Mesh, AnimRateScale, Params);
	}
}

// ---------------------------------------------------------------------------------------------
// Lifetime
// ---------------------------------------------------------------------------------------------

void UEGTimeDilationSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Real seconds: a blend or a hit pause must not slow down with the dilation it applies.
	RealTime += FApp::GetDeltaTime();
	if (Channels.Num() > 0)
	{
		ApplyAndPrune();
	}
}

void UEGTimeDilationSubsystem::Deinitialize()
{
	// Hand every target back its base value.
	for (FChannel& Channel : Channels)
	{
		UObject* const Object = Channel.Object.Get();
		if (Channel.Kind == ETarget::World || Object)
		{
			WriteValue(Channel.Kind, Object, Channel.Stack.GetBase());
		}
	}
	Channels.Reset();
	Handles.Reset();
	Super::Deinitialize();
}
