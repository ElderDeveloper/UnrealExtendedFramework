// Copyright Moon Punch Games. All Rights Reserved.

#include "Status/EFServiceStatus.h"

#include "ExtendedFoundationModule.h"
#include "HAL/IConsoleManager.h"

namespace EFServiceStatusPrivate
{
	struct FEntry
	{
		int32 Handle = 0;
		FName Service;
		FEFServiceStatusRegistry::FReporter Reporter;
	};

	TArray<FEntry>& Entries()
	{
		static TArray<FEntry> Registered;
		return Registered;
	}

	int32 LastHandle = 0;

	FAutoConsoleCommand StatusCommand(
		TEXT("Extended.Status"),
		TEXT("Prints the state of every Extended service (EOS, PlayFab, Steam, Http, ...). Extended.Status brief prints one line each."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			const bool bBrief = Args.Num() > 0 && Args[0].Equals(TEXT("brief"), ESearchCase::IgnoreCase);
			const FString Report = FEFServiceStatusRegistry::FormatReport(!bBrief);
			TArray<FString> Lines;
			Report.ParseIntoArrayLines(Lines, /*bCullEmpty*/ false);
			for (const FString& Line : Lines)
			{
				UE_LOG(LogExtendedFoundation, Display, TEXT("%s"), *Line);
			}
		}));
}

int32 FEFServiceStatusRegistry::Register(FName Service, FReporter Reporter)
{
	check(IsInGameThread());
	EFServiceStatusPrivate::FEntry& Entry = EFServiceStatusPrivate::Entries().AddDefaulted_GetRef();
	Entry.Handle = ++EFServiceStatusPrivate::LastHandle;
	Entry.Service = Service;
	Entry.Reporter = MoveTemp(Reporter);
	return Entry.Handle;
}

void FEFServiceStatusRegistry::Unregister(int32 Handle)
{
	check(IsInGameThread());
	EFServiceStatusPrivate::Entries().RemoveAll([Handle](const EFServiceStatusPrivate::FEntry& Entry) { return Entry.Handle == Handle; });
}

TArray<FEFServiceStatus> FEFServiceStatusRegistry::Collect()
{
	TArray<FEFServiceStatus> Statuses;
	for (const EFServiceStatusPrivate::FEntry& Entry : EFServiceStatusPrivate::Entries())
	{
		if (!Entry.Reporter)
		{
			continue;
		}
		FEFServiceStatus Status = Entry.Reporter();
		if (Status.Service.IsNone())
		{
			Status.Service = Entry.Service;
		}
		Statuses.Add(MoveTemp(Status));
	}
	Statuses.Sort([](const FEFServiceStatus& A, const FEFServiceStatus& B) { return A.Service.LexicalLess(B.Service); });
	return Statuses;
}

FString FEFServiceStatusRegistry::FormatReport(bool bIncludeDetails)
{
	const TArray<FEFServiceStatus> Statuses = Collect();
	if (Statuses.Num() == 0)
	{
		return TEXT("Extended.Status: no services registered.");
	}

	FString Report = TEXT("Extended.Status");
	for (const FEFServiceStatus& Status : Statuses)
	{
		Report += FString::Printf(TEXT("\n  %-10s %-13s %s"), *Status.Service.ToString(), StateToString(Status.State), *Status.Summary);
		if (bIncludeDetails)
		{
			for (const TPair<FString, FString>& Detail : Status.Details)
			{
				Report += FString::Printf(TEXT("\n      %s: %s"), *Detail.Key, *Detail.Value);
			}
		}
	}
	return Report;
}

const TCHAR* FEFServiceStatusRegistry::StateToString(EEFServiceState State)
{
	switch (State)
	{
	case EEFServiceState::Unavailable:   return TEXT("Unavailable");
	case EEFServiceState::NotConfigured: return TEXT("NotConfigured");
	case EEFServiceState::Starting:      return TEXT("Starting");
	case EEFServiceState::Ready:         return TEXT("Ready");
	case EEFServiceState::Degraded:      return TEXT("Degraded");
	case EEFServiceState::Error:         return TEXT("Error");
	default:                             return TEXT("Unknown");
	}
}
