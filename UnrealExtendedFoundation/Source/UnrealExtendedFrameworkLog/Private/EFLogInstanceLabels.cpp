// Copyright Moon Punch Games. All Rights Reserved.

#include "EFLogInstanceLabels.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/Package.h"

namespace EFLogInstanceLabelsPrivate
{
	static FName LabelForNetMode(ENetMode NetMode, int32 PIEInstance)
	{
		static const FName ServerLabel(TEXT("Server"));
		static const FName StandaloneLabel(TEXT("Standalone"));

		switch (NetMode)
		{
		case NM_DedicatedServer:
		case NM_ListenServer:
			return ServerLabel;

		case NM_Client:
		{
			// Game thread only (see the header), so a plain static cache is safe.
			static TMap<int32, FName> ClientLabels;
			if (const FName* Cached = ClientLabels.Find(PIEInstance))
			{
				return *Cached;
			}
			return ClientLabels.Add(PIEInstance, FName(*FString::Printf(TEXT("Client %d"), PIEInstance)));
		}

		default:
			return StandaloneLabel;
		}
	}
}

FName EFLogInstanceLabels::ForPIEInstance(int32 PIEInstance)
{
	if (PIEInstance < 0 || !IsInGameThread() || !GEngine)
	{
		return NAME_None;
	}

	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.PIEInstance != PIEInstance)
		{
			continue;
		}

		// The net mode is read per line rather than cached: a listen server's world only becomes
		// one once it starts listening, after the world itself was created.
		const UWorld* World = Context.World();
		if (IsValid(World))
		{
			return EFLogInstanceLabelsPrivate::LabelForNetMode(World->GetNetMode(), PIEInstance);
		}
	}

	return NAME_None;
}

FName EFLogInstanceLabels::ForWorld(const UWorld* World, int32& OutPIEInstance)
{
	OutPIEInstance = INDEX_NONE;

	if (!IsValid(World) || !IsInGameThread())
	{
		return NAME_None;
	}

	OutPIEInstance = World->GetPackage()->GetPIEInstanceID();
	if (OutPIEInstance == INDEX_NONE)
	{
		return NAME_None;
	}

	return EFLogInstanceLabelsPrivate::LabelForNetMode(World->GetNetMode(), OutPIEInstance);
}
