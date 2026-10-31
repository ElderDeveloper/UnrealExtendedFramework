// Copyright Moon Punch Games. All Rights Reserved.

#include "Platform/EFPlatformSubsystem.h"

#include "Assets/EFAssetLoading.h"
#include "ExtendedFoundationModule.h"
#include "Messages/EFMessageSubsystem.h"
#include "Platform/EFPlatformCatalog.h"
#include "Platform/EFPlatformMessages.h"
#include "Platform/EFPlatformSettings.h"
#include "Status/EFServiceStatus.h"
#include "Subsystems/EFSubsystems.h"
#include "Templates/Tuple.h"
#include <type_traits>

namespace EFPlatformPrivate
{
	const TCHAR* const Source = TEXT("Platform");
	constexpr int32 MaxLeaderboardEntries = 100;

	/** Keeps a function parameter out of template argument deduction. */
	template <typename T>
	struct TNonDeduced
	{
		using Type = T;
	};

	/**
	 * Wraps a callback so that an answer arriving before the call that started it returned
	 * (bCallReturned still false) waits for the next tick. No caller then ever sees its callback
	 * run inside the call. The callback parameter is not deduced: callers name ArgTypes and may
	 * pass a lambda.
	 */
	template <typename... ArgTypes>
	TFunction<void(ArgTypes...)> Deferred(typename TNonDeduced<TFunction<void(ArgTypes...)>>::Type OnDone, TSharedRef<bool> bCallReturned)
	{
		return [OnDone = MoveTemp(OnDone), bCallReturned](ArgTypes... Args) mutable
		{
			if (!OnDone)
			{
				return;
			}
			TFunction<void(ArgTypes...)> Callback = MoveTemp(OnDone);
			OnDone = nullptr;
			if (*bCallReturned)
			{
				Callback(Args...);
				return;
			}

			TTuple<std::decay_t<ArgTypes>...> Saved(Args...);
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
				[Callback = MoveTemp(Callback), Saved = MoveTemp(Saved)](float) mutable
				{
					Saved.ApplyAfter(Callback);
					return false;
				}));
		};
	}
}

using EFPlatformPrivate::Deferred;

UEFPlatformSubsystem* UEFPlatformSubsystem::Get(const UObject* WorldContextObject)
{
	return EF::GetGameInstanceSubsystem<UEFPlatformSubsystem>(WorldContextObject);
}

void UEFPlatformSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Unlock and score messages go out on the bus, so it must outlive this subsystem.
	Collection.InitializeDependency<UEFMessageSubsystem>();

	// GetCatalog runs when a game first unlocks or submits something; have the catalog ready by then.
	const TSoftObjectPtr<UEFPlatformCatalog>& ConfiguredCatalog = UEFPlatformSettings::Get()->Catalog;
	if (!ConfiguredCatalog.IsNull() && !ConfiguredCatalog.Get())
	{
		CatalogPreloadHandle = EFAssets::Preload({ ConfiguredCatalog.ToSoftObjectPath() });
	}

	TWeakObjectPtr<UEFPlatformSubsystem> WeakThis(this);
	StatusHandle = FEFServiceStatusRegistry::Register(TEXT("Platform"), [WeakThis]
	{
		FEFServiceStatus Status;
		const UEFPlatformSubsystem* This = WeakThis.Get();
		if (!This)
		{
			Status.State = EEFServiceState::Unavailable;
			return Status;
		}

		const TArray<UEFPlatformProviderSubsystem*> Registered = This->GetProviders();
		if (Registered.Num() == 0)
		{
			Status.State = EEFServiceState::NotConfigured;
			Status.Summary = TEXT("no provider registered (enable a store plugin, or bEnableLocalProvider)");
			return Status;
		}

		TArray<FString> Routes;
		int32 Served = 0;
		for (const EEFPlatformFeature Feature : EFPlatform::AllFeatures())
		{
			const UEFPlatformProviderSubsystem* Provider = This->GetProvider(Feature);
			Served += Provider ? 1 : 0;
			Routes.Add(FString::Printf(TEXT("%s: %s"), EFPlatform::FeatureToString(Feature), Provider ? *Provider->GetProviderName().ToString() : TEXT("none")));
		}
		Status.State = Served > 0 ? EEFServiceState::Ready : EEFServiceState::Starting;
		Status.Summary = FString::Join(Routes, TEXT(", "));

		for (const UEFPlatformProviderSubsystem* Provider : Registered)
		{
			TArray<FString> Available;
			for (const EEFPlatformFeature Feature : EFPlatform::AllFeatures())
			{
				if (Provider->IsFeatureAvailable(Feature))
				{
					Available.Add(EFPlatform::FeatureToString(Feature));
				}
			}
			const FEFPlatformUser User = Provider->GetLocalUser();
			Status.Details.Add(Provider->GetProviderName().ToString(), FString::Printf(TEXT("%s; user %s"),
				Available.Num() ? *FString::Join(Available, TEXT(", ")) : TEXT("nothing available now"),
				User.bSignedIn ? *User.DisplayName : TEXT("not signed in")));
		}
		if (This->GetQueuedWriteCount() > 0)
		{
			Status.State = EEFServiceState::Degraded;
			Status.Details.Add(TEXT("Queued writes"), LexToString(This->GetQueuedWriteCount()));
		}
		return Status;
	});
}

void UEFPlatformSubsystem::Deinitialize()
{
	FEFServiceStatusRegistry::Unregister(StatusHandle);
	StatusHandle = 0;

	if (QueueTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(QueueTickerHandle);
		QueueTickerHandle.Reset();
	}

	// Callers may be tearing down too; answer them, with a clear Cancelled.
	TArray<FQueuedWrite> Pending = MoveTemp(QueuedWrites);
	QueuedWrites.Reset();
	for (FQueuedWrite& Write : Pending)
	{
		if (Write.OnDone)
		{
			Write.OnDone(FEFResult::Cancelled(EFPlatformPrivate::Source));
		}
	}

	Providers.Reset();
	OnProvidersChanged.Clear();
	CatalogPreloadHandle.Reset();
	Super::Deinitialize();
}

// Providers

void UEFPlatformSubsystem::RegisterProvider(UEFPlatformProviderSubsystem* Provider)
{
	if (!Provider)
	{
		return;
	}

	Providers.RemoveAll([Provider](const TWeakObjectPtr<UEFPlatformProviderSubsystem>& Existing) { return !Existing.IsValid() || Existing.Get() == Provider; });
	if (FindProvider(Provider->GetProviderName()))
	{
		UE_LOG(LogExtendedFoundation, Warning, TEXT("Platform: two providers are named %s; %s is ignored."),
			*Provider->GetProviderName().ToString(), *Provider->GetClass()->GetName());
		return;
	}

	Providers.Add(Provider);
	UE_LOG(LogExtendedFoundation, Log, TEXT("Platform: provider %s registered."), *Provider->GetProviderName().ToString());
	OnProvidersChanged.Broadcast();

	// Providers often become available a moment after they register (SDK start, sign-in).
	if (QueuedWrites.Num() > 0)
	{
		EnsureQueueTicker();
	}
}

void UEFPlatformSubsystem::UnregisterProvider(UEFPlatformProviderSubsystem* Provider)
{
	const int32 Removed = Providers.RemoveAll([Provider](const TWeakObjectPtr<UEFPlatformProviderSubsystem>& Existing) { return !Existing.IsValid() || Existing.Get() == Provider; });
	if (Removed > 0)
	{
		OnProvidersChanged.Broadcast();
	}
}

TArray<UEFPlatformProviderSubsystem*> UEFPlatformSubsystem::GetProviders() const
{
	TArray<UEFPlatformProviderSubsystem*> Registered;
	for (const TWeakObjectPtr<UEFPlatformProviderSubsystem>& Weak : Providers)
	{
		if (UEFPlatformProviderSubsystem* Provider = Weak.Get())
		{
			Registered.Add(Provider);
		}
	}

	TArray<UEFPlatformProviderSubsystem*> Ordered;
	for (const FName& Name : UEFPlatformSettings::Get()->ProviderOrder)
	{
		for (UEFPlatformProviderSubsystem* Provider : Registered)
		{
			if (Provider->GetProviderName() == Name)
			{
				Ordered.AddUnique(Provider);
			}
		}
	}
	for (UEFPlatformProviderSubsystem* Provider : Registered)
	{
		Ordered.AddUnique(Provider);
	}
	return Ordered;
}

UEFPlatformProviderSubsystem* UEFPlatformSubsystem::FindProvider(FName ProviderName) const
{
	for (const TWeakObjectPtr<UEFPlatformProviderSubsystem>& Weak : Providers)
	{
		UEFPlatformProviderSubsystem* Provider = Weak.Get();
		if (Provider && Provider->GetProviderName() == ProviderName)
		{
			return Provider;
		}
	}
	return nullptr;
}

UEFPlatformProviderSubsystem* UEFPlatformSubsystem::GetProvider(EEFPlatformFeature Feature) const
{
	if (const FName* Preferred = UEFPlatformSettings::Get()->FeatureProviders.Find(Feature))
	{
		UEFPlatformProviderSubsystem* Provider = FindProvider(*Preferred);
		if (Provider && Provider->IsFeatureAvailable(Feature))
		{
			return Provider;
		}
	}

	const TArray<UEFPlatformProviderSubsystem*> Ordered = GetProviders();

	// A store that supports the feature but is not ready yet (SDK starting, user not signed in) must
	// not lose writes to the Local provider; they wait in the queue instead. In the editor that store
	// is often simply not running, so Local may stand in (bLocalFallbackInEditor).
	const bool bEditorFallback = GIsEditor && UEFPlatformSettings::Get()->bLocalFallbackInEditor;
	const bool bStoreSupportsFeature = !bEditorFallback && Ordered.ContainsByPredicate([Feature](const UEFPlatformProviderSubsystem* Provider)
	{
		return !Provider->IsFallbackProvider() && Provider->SupportsFeature(Feature);
	});

	for (UEFPlatformProviderSubsystem* Provider : Ordered)
	{
		if (Provider->IsFallbackProvider() && bStoreSupportsFeature)
		{
			continue;
		}
		if (Provider->IsFeatureAvailable(Feature))
		{
			return Provider;
		}
	}
	return nullptr;
}

FName UEFPlatformSubsystem::GetProviderName(EEFPlatformFeature Feature) const
{
	const UEFPlatformProviderSubsystem* Provider = GetProvider(Feature);
	return Provider ? Provider->GetProviderName() : NAME_None;
}

FEFPlatformUser UEFPlatformSubsystem::GetLocalUser() const
{
	const TArray<UEFPlatformProviderSubsystem*> Ordered = GetProviders();
	for (const UEFPlatformProviderSubsystem* Provider : Ordered)
	{
		const FEFPlatformUser User = Provider->GetLocalUser();
		if (User.bSignedIn)
		{
			return User;
		}
	}
	return Ordered.Num() > 0 ? Ordered[0]->GetLocalUser() : FEFPlatformUser();
}

// Helpers

void UEFPlatformSubsystem::AnswerLater(FOnResult OnDone, const FEFResult& Result)
{
	Deferred<const FEFResult&>(MoveTemp(OnDone), MakeShared<bool>(false))(Result);
}

FEFResult UEFPlatformSubsystem::NoProvider(EEFPlatformFeature Feature) const
{
	TArray<FString> Names;
	for (const UEFPlatformProviderSubsystem* Provider : GetProviders())
	{
		Names.Add(Provider->GetProviderName().ToString());
	}
	return FEFResult::NotAvailable(EFPlatformPrivate::Source, FString::Printf(TEXT("no provider can serve %s now (registered: %s)"),
		EFPlatform::FeatureToString(Feature), Names.Num() ? *FString::Join(Names, TEXT(", ")) : TEXT("none")));
}

void UEFPlatformSubsystem::RouteWrite(EEFPlatformFeature Feature, const FString& Description,
	TFunction<void(UEFPlatformProviderSubsystem& Provider, FOnResult Done)> Write, FOnResult OnDone)
{
	if (UEFPlatformProviderSubsystem* Provider = GetProvider(Feature))
	{
		const TSharedRef<bool> bReturned = MakeShared<bool>(false);
		Write(*Provider, Deferred<const FEFResult&>(MoveTemp(OnDone), bReturned));
		*bReturned = true;
		return;
	}

	const UEFPlatformSettings* Settings = UEFPlatformSettings::Get();
	if (!Settings->bQueueWritesWhileUnavailable)
	{
		AnswerLater(MoveTemp(OnDone), NoProvider(Feature));
		return;
	}

	while (QueuedWrites.Num() >= FMath::Max(Settings->MaxQueuedWrites, 1))
	{
		FQueuedWrite Dropped = MoveTemp(QueuedWrites[0]);
		QueuedWrites.RemoveAt(0);
		UE_LOG(LogExtendedFoundation, Warning, TEXT("Platform: the write queue is full; dropped %s."), *Dropped.Description);
		AnswerLater(MoveTemp(Dropped.OnDone), FEFResult::Failure(EFPlatformPrivate::Source, EFPlatformResultCodes::QueueFull,
			FString::Printf(TEXT("%s was queued and dropped because the queue is full"), *Dropped.Description)));
	}

	UE_LOG(LogExtendedFoundation, Log, TEXT("Platform: no provider can serve %s now; queued %s."), EFPlatform::FeatureToString(Feature), *Description);
	FQueuedWrite& Queued = QueuedWrites.AddDefaulted_GetRef();
	Queued.Feature = Feature;
	Queued.Description = Description;
	Queued.Send = MoveTemp(Write);
	Queued.OnDone = MoveTemp(OnDone);
	EnsureQueueTicker();
}

void UEFPlatformSubsystem::FlushQueuedWrites()
{
	if (QueuedWrites.Num() == 0)
	{
		return;
	}

	TArray<FQueuedWrite> Ready;
	TArray<FQueuedWrite> Waiting;
	for (FQueuedWrite& Write : QueuedWrites)
	{
		(GetProvider(Write.Feature) ? Ready : Waiting).Add(MoveTemp(Write));
	}
	QueuedWrites = MoveTemp(Waiting);

	for (FQueuedWrite& Write : Ready)
	{
		UEFPlatformProviderSubsystem* Provider = GetProvider(Write.Feature);
		if (!Provider)
		{
			QueuedWrites.Add(MoveTemp(Write));
			continue;
		}
		UE_LOG(LogExtendedFoundation, Log, TEXT("Platform: sending queued %s to %s."), *Write.Description, *Provider->GetProviderName().ToString());
		const TSharedRef<bool> bReturned = MakeShared<bool>(false);
		Write.Send(*Provider, Deferred<const FEFResult&>(MoveTemp(Write.OnDone), bReturned));
		*bReturned = true;
	}
}

void UEFPlatformSubsystem::EnsureQueueTicker()
{
	if (!QueueTickerHandle.IsValid())
	{
		QueueTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UEFPlatformSubsystem::TickQueue),
			FMath::Max(UEFPlatformSettings::Get()->QueueRetryIntervalSeconds, 0.5f));
	}
}

bool UEFPlatformSubsystem::TickQueue(float DeltaTime)
{
	FlushQueuedWrites();
	if (QueuedWrites.Num() == 0)
	{
		QueueTickerHandle.Reset();
		return false;
	}
	return true;
}

void UEFPlatformSubsystem::BroadcastAchievementUnlocked(FName AchievementId, FName ProviderName)
{
	OnAchievementUnlocked.Broadcast(AchievementId, ProviderName);
	if (UEFPlatformSettings::Get()->bBroadcastMessages)
	{
		if (UEFMessageSubsystem* Messages = GetGameInstance() ? GetGameInstance()->GetSubsystem<UEFMessageSubsystem>() : nullptr)
		{
			FEFAchievementUnlockedMessage Message;
			Message.AchievementId = AchievementId;
			Message.Provider = ProviderName;
			Messages->BroadcastMessage(EFPlatformMessages::AchievementUnlocked, Message);
		}
	}
}

void UEFPlatformSubsystem::BroadcastScoreSubmitted(FName LeaderboardId, int64 Score, FName ProviderName)
{
	if (UEFPlatformSettings::Get()->bBroadcastMessages)
	{
		if (UEFMessageSubsystem* Messages = GetGameInstance() ? GetGameInstance()->GetSubsystem<UEFMessageSubsystem>() : nullptr)
		{
			FEFScoreSubmittedMessage Message;
			Message.LeaderboardId = LeaderboardId;
			Message.Score = Score;
			Message.Provider = ProviderName;
			Messages->BroadcastMessage(EFPlatformMessages::ScoreSubmitted, Message);
		}
	}
}

// Catalog

const UEFPlatformCatalog* UEFPlatformSubsystem::GetCatalog() const
{
	if (CatalogOverride)
	{
		return CatalogOverride.Get();
	}

	if (!bCatalogLoadAttempted)
	{
		UEFPlatformSubsystem* Mutable = const_cast<UEFPlatformSubsystem*>(this);
		Mutable->bCatalogLoadAttempted = true;

		const TSoftObjectPtr<UEFPlatformCatalog>& Configured = UEFPlatformSettings::Get()->Catalog;
		if (!Configured.IsNull())
		{
			Mutable->LoadedCatalog = EFAssets::Resolve(Configured, TEXT("UEFPlatformSubsystem::GetCatalog"));
			if (!LoadedCatalog)
			{
				UE_LOG(LogExtendedFoundation, Warning, TEXT("Platform: the catalog %s could not be loaded; ids are sent to providers unchanged."), *Configured.ToString());
			}
			else
			{
				for (const FString& Problem : LoadedCatalog->FindProblems())
				{
					UE_LOG(LogExtendedFoundation, Warning, TEXT("Platform catalog %s: %s"), *LoadedCatalog->GetName(), *Problem);
				}
			}
		}
	}
	return LoadedCatalog.Get();
}

void UEFPlatformSubsystem::SetCatalogOverride(const UEFPlatformCatalog* InCatalog)
{
	CatalogOverride = const_cast<UEFPlatformCatalog*>(InCatalog);
}

FString UEFPlatformSubsystem::ToProviderId(EEFPlatformCatalogKind Kind, FName Id, FName ProviderName) const
{
	const UEFPlatformCatalog* Catalog = GetCatalog();
	return Catalog ? Catalog->ToProviderId(Kind, Id, ProviderName) : Id.ToString();
}

FName UEFPlatformSubsystem::FromProviderId(EEFPlatformCatalogKind Kind, const FString& ProviderId, FName ProviderName) const
{
	const UEFPlatformCatalog* Catalog = GetCatalog();
	return Catalog ? Catalog->FromProviderId(Kind, ProviderId, ProviderName) : FName(*ProviderId);
}

// Achievements

void UEFPlatformSubsystem::UnlockAchievement(FName AchievementId, FOnResult OnDone)
{
	if (AchievementId.IsNone())
	{
		AnswerLater(MoveTemp(OnDone), FEFResult::InvalidArgument(EFPlatformPrivate::Source, TEXT("AchievementId is empty")));
		return;
	}

	TWeakObjectPtr<UEFPlatformSubsystem> WeakThis(this);
	RouteWrite(EEFPlatformFeature::Achievements, FString::Printf(TEXT("UnlockAchievement %s"), *AchievementId.ToString()),
		[WeakThis, AchievementId](UEFPlatformProviderSubsystem& Provider, FOnResult Done)
		{
			const FName ProviderName = Provider.GetProviderName();
			const UEFPlatformSubsystem* This = WeakThis.Get();
			const FString ProviderId = This ? This->ToProviderId(EEFPlatformCatalogKind::Achievement, AchievementId, ProviderName) : AchievementId.ToString();
			Provider.UnlockAchievement(ProviderId, [WeakThis, AchievementId, ProviderName, Done = MoveTemp(Done)](const FEFResult& Result)
			{
				if (Result.IsSuccess())
				{
					if (UEFPlatformSubsystem* Platform = WeakThis.Get())
					{
						Platform->BroadcastAchievementUnlocked(AchievementId, ProviderName);
					}
				}
				Done(Result);
			});
		}, MoveTemp(OnDone));
}

void UEFPlatformSubsystem::SetAchievementProgress(FName AchievementId, float Progress, FOnResult OnDone)
{
	if (AchievementId.IsNone())
	{
		AnswerLater(MoveTemp(OnDone), FEFResult::InvalidArgument(EFPlatformPrivate::Source, TEXT("AchievementId is empty")));
		return;
	}

	const float Clamped = FMath::Clamp(Progress, 0.f, 1.f);
	TWeakObjectPtr<UEFPlatformSubsystem> WeakThis(this);
	RouteWrite(EEFPlatformFeature::Achievements, FString::Printf(TEXT("SetAchievementProgress %s %.2f"), *AchievementId.ToString(), Clamped),
		[WeakThis, AchievementId, Clamped](UEFPlatformProviderSubsystem& Provider, FOnResult Done)
		{
			const FName ProviderName = Provider.GetProviderName();
			const UEFPlatformSubsystem* This = WeakThis.Get();
			const FString ProviderId = This ? This->ToProviderId(EEFPlatformCatalogKind::Achievement, AchievementId, ProviderName) : AchievementId.ToString();
			Provider.SetAchievementProgress(ProviderId, Clamped, [WeakThis, AchievementId, ProviderName, Clamped, Done = MoveTemp(Done)](const FEFResult& Result)
			{
				if (Result.IsSuccess() && Clamped >= 1.f)
				{
					if (UEFPlatformSubsystem* Platform = WeakThis.Get())
					{
						Platform->BroadcastAchievementUnlocked(AchievementId, ProviderName);
					}
				}
				Done(Result);
			});
		}, MoveTemp(OnDone));
}

void UEFPlatformSubsystem::QueryAchievements(FOnAchievements OnDone)
{
	UEFPlatformProviderSubsystem* Provider = GetProvider(EEFPlatformFeature::Achievements);
	if (!Provider)
	{
		Deferred<const FEFResult&, const TArray<FEFAchievementState>&>(MoveTemp(OnDone), MakeShared<bool>(false))(NoProvider(EEFPlatformFeature::Achievements), TArray<FEFAchievementState>());
		return;
	}

	const FName ProviderName = Provider->GetProviderName();
	TWeakObjectPtr<UEFPlatformSubsystem> WeakThis(this);
	const TSharedRef<bool> bReturned = MakeShared<bool>(false);
	Provider->QueryAchievements(Deferred<const FEFResult&, const TArray<FEFAchievementState>&>(
		[WeakThis, ProviderName, OnDone = MoveTemp(OnDone)](const FEFResult& Result, const TArray<FEFAchievementState>& Achievements)
		{
			TArray<FEFAchievementState> Mapped = Achievements;
			if (const UEFPlatformSubsystem* This = WeakThis.Get())
			{
				const UEFPlatformCatalog* Catalog = This->GetCatalog();
				for (FEFAchievementState& State : Mapped)
				{
					State.Id = This->FromProviderId(EEFPlatformCatalogKind::Achievement, State.ProviderId, ProviderName);
					const FEFPlatformCatalogEntry* Entry = Catalog ? Catalog->FindEntry(EEFPlatformCatalogKind::Achievement, State.Id) : nullptr;
					if (State.DisplayName.IsEmpty() && Entry)
					{
						State.DisplayName = Entry->DisplayName;
					}
				}
			}
			if (OnDone)
			{
				OnDone(Result, Mapped);
			}
		}, bReturned));
	*bReturned = true;
}

// Stats

void UEFPlatformSubsystem::UpdateStat(FName StatId, int64 Value, EEFStatUpdateMode Mode, FOnResult OnDone)
{
	FEFStatUpdate Update;
	Update.Id = StatId;
	Update.Value = Value;
	Update.Mode = Mode;
	UpdateStats({ Update }, MoveTemp(OnDone));
}

void UEFPlatformSubsystem::UpdateStats(const TArray<FEFStatUpdate>& Updates, FOnResult OnDone)
{
	if (Updates.Num() == 0 || Updates.ContainsByPredicate([](const FEFStatUpdate& Update) { return Update.Id.IsNone(); }))
	{
		AnswerLater(MoveTemp(OnDone), FEFResult::InvalidArgument(EFPlatformPrivate::Source, Updates.Num() == 0 ? TEXT("no stat updates") : TEXT("a stat update has no Id")));
		return;
	}

	TWeakObjectPtr<UEFPlatformSubsystem> WeakThis(this);
	RouteWrite(EEFPlatformFeature::Stats, FString::Printf(TEXT("UpdateStats (%d)"), Updates.Num()),
		[WeakThis, Updates](UEFPlatformProviderSubsystem& Provider, FOnResult Done)
		{
			const FName ProviderName = Provider.GetProviderName();
			const UEFPlatformSubsystem* This = WeakThis.Get();
			TArray<FEFStatUpdate> Mapped = Updates;
			for (FEFStatUpdate& Update : Mapped)
			{
				Update.ProviderId = This ? This->ToProviderId(EEFPlatformCatalogKind::Stat, Update.Id, ProviderName) : Update.Id.ToString();
			}
			Provider.UpdateStats(Mapped, MoveTemp(Done));
		}, MoveTemp(OnDone));
}

void UEFPlatformSubsystem::QueryStats(const TArray<FName>& StatIds, FOnStats OnDone)
{
	UEFPlatformProviderSubsystem* Provider = GetProvider(EEFPlatformFeature::Stats);
	if (!Provider)
	{
		Deferred<const FEFResult&, const TArray<FEFStatValue>&>(MoveTemp(OnDone), MakeShared<bool>(false))(NoProvider(EEFPlatformFeature::Stats), TArray<FEFStatValue>());
		return;
	}

	const FName ProviderName = Provider->GetProviderName();
	TArray<FString> ProviderIds;
	for (const FName& Id : StatIds)
	{
		ProviderIds.Add(ToProviderId(EEFPlatformCatalogKind::Stat, Id, ProviderName));
	}

	TWeakObjectPtr<UEFPlatformSubsystem> WeakThis(this);
	const TSharedRef<bool> bReturned = MakeShared<bool>(false);
	Provider->QueryStats(ProviderIds, Deferred<const FEFResult&, const TArray<FEFStatValue>&>(
		[WeakThis, ProviderName, OnDone = MoveTemp(OnDone)](const FEFResult& Result, const TArray<FEFStatValue>& Stats)
		{
			TArray<FEFStatValue> Mapped = Stats;
			if (const UEFPlatformSubsystem* This = WeakThis.Get())
			{
				for (FEFStatValue& Stat : Mapped)
				{
					Stat.Id = This->FromProviderId(EEFPlatformCatalogKind::Stat, Stat.ProviderId, ProviderName);
				}
			}
			if (OnDone)
			{
				OnDone(Result, Mapped);
			}
		}, bReturned));
	*bReturned = true;
}

// Leaderboards

void UEFPlatformSubsystem::SubmitScore(FName LeaderboardId, int64 Score, FOnResult OnDone)
{
	if (LeaderboardId.IsNone())
	{
		AnswerLater(MoveTemp(OnDone), FEFResult::InvalidArgument(EFPlatformPrivate::Source, TEXT("LeaderboardId is empty")));
		return;
	}

	TWeakObjectPtr<UEFPlatformSubsystem> WeakThis(this);
	RouteWrite(EEFPlatformFeature::Leaderboards, FString::Printf(TEXT("SubmitScore %s %lld"), *LeaderboardId.ToString(), Score),
		[WeakThis, LeaderboardId, Score](UEFPlatformProviderSubsystem& Provider, FOnResult Done)
		{
			const FName ProviderName = Provider.GetProviderName();
			const UEFPlatformSubsystem* This = WeakThis.Get();
			const FString ProviderId = This ? This->ToProviderId(EEFPlatformCatalogKind::Leaderboard, LeaderboardId, ProviderName) : LeaderboardId.ToString();
			Provider.SubmitScore(ProviderId, Score, [WeakThis, LeaderboardId, Score, ProviderName, Done = MoveTemp(Done)](const FEFResult& Result)
			{
				if (Result.IsSuccess())
				{
					if (UEFPlatformSubsystem* Platform = WeakThis.Get())
					{
						Platform->BroadcastScoreSubmitted(LeaderboardId, Score, ProviderName);
					}
				}
				Done(Result);
			});
		}, MoveTemp(OnDone));
}

void UEFPlatformSubsystem::QueryLeaderboard(FName LeaderboardId, EEFLeaderboardRange Range, int32 Count, FOnLeaderboard OnDone)
{
	using FDeferredLeaderboard = TFunction<void(const FEFResult&, const TArray<FEFLeaderboardEntry>&)>;
	UEFPlatformProviderSubsystem* Provider = GetProvider(EEFPlatformFeature::Leaderboards);
	if (LeaderboardId.IsNone() || !Provider)
	{
		const FEFResult Failure = LeaderboardId.IsNone() ? FEFResult::InvalidArgument(EFPlatformPrivate::Source, TEXT("LeaderboardId is empty")) : NoProvider(EEFPlatformFeature::Leaderboards);
		Deferred<const FEFResult&, const TArray<FEFLeaderboardEntry>&>(FDeferredLeaderboard(MoveTemp(OnDone)), MakeShared<bool>(false))(Failure, TArray<FEFLeaderboardEntry>());
		return;
	}

	const FString ProviderId = ToProviderId(EEFPlatformCatalogKind::Leaderboard, LeaderboardId, Provider->GetProviderName());
	const TSharedRef<bool> bReturned = MakeShared<bool>(false);
	Provider->QueryLeaderboard(ProviderId, Range, FMath::Clamp(Count, 1, EFPlatformPrivate::MaxLeaderboardEntries),
		Deferred<const FEFResult&, const TArray<FEFLeaderboardEntry>&>(MoveTemp(OnDone), bReturned));
	*bReturned = true;
}

// Cloud storage

void UEFPlatformSubsystem::WriteCloudFile(const FString& FileName, const TArray<uint8>& Data, FOnResult OnDone)
{
	UEFPlatformProviderSubsystem* Provider = GetProvider(EEFPlatformFeature::CloudStorage);
	if (FileName.IsEmpty() || !Provider)
	{
		AnswerLater(MoveTemp(OnDone), FileName.IsEmpty() ? FEFResult::InvalidArgument(EFPlatformPrivate::Source, TEXT("FileName is empty")) : NoProvider(EEFPlatformFeature::CloudStorage));
		return;
	}
	const TSharedRef<bool> bReturned = MakeShared<bool>(false);
	Provider->WriteCloudFile(FileName, Data, Deferred<const FEFResult&>(MoveTemp(OnDone), bReturned));
	*bReturned = true;
}

void UEFPlatformSubsystem::ReadCloudFile(const FString& FileName, FOnCloudFile OnDone)
{
	UEFPlatformProviderSubsystem* Provider = GetProvider(EEFPlatformFeature::CloudStorage);
	if (FileName.IsEmpty() || !Provider)
	{
		const FEFResult Failure = FileName.IsEmpty() ? FEFResult::InvalidArgument(EFPlatformPrivate::Source, TEXT("FileName is empty")) : NoProvider(EEFPlatformFeature::CloudStorage);
		Deferred<const FEFResult&, const TArray<uint8>&>(MoveTemp(OnDone), MakeShared<bool>(false))(Failure, TArray<uint8>());
		return;
	}
	const TSharedRef<bool> bReturned = MakeShared<bool>(false);
	Provider->ReadCloudFile(FileName, Deferred<const FEFResult&, const TArray<uint8>&>(MoveTemp(OnDone), bReturned));
	*bReturned = true;
}

void UEFPlatformSubsystem::DeleteCloudFile(const FString& FileName, FOnResult OnDone)
{
	UEFPlatformProviderSubsystem* Provider = GetProvider(EEFPlatformFeature::CloudStorage);
	if (FileName.IsEmpty() || !Provider)
	{
		AnswerLater(MoveTemp(OnDone), FileName.IsEmpty() ? FEFResult::InvalidArgument(EFPlatformPrivate::Source, TEXT("FileName is empty")) : NoProvider(EEFPlatformFeature::CloudStorage));
		return;
	}
	const TSharedRef<bool> bReturned = MakeShared<bool>(false);
	Provider->DeleteCloudFile(FileName, Deferred<const FEFResult&>(MoveTemp(OnDone), bReturned));
	*bReturned = true;
}

void UEFPlatformSubsystem::ListCloudFiles(FOnCloudFiles OnDone)
{
	UEFPlatformProviderSubsystem* Provider = GetProvider(EEFPlatformFeature::CloudStorage);
	if (!Provider)
	{
		Deferred<const FEFResult&, const TArray<FEFCloudFileInfo>&>(MoveTemp(OnDone), MakeShared<bool>(false))(NoProvider(EEFPlatformFeature::CloudStorage), TArray<FEFCloudFileInfo>());
		return;
	}
	const TSharedRef<bool> bReturned = MakeShared<bool>(false);
	Provider->ListCloudFiles(Deferred<const FEFResult&, const TArray<FEFCloudFileInfo>&>(MoveTemp(OnDone), bReturned));
	*bReturned = true;
}

// Presence

void UEFPlatformSubsystem::SetPresence(const FString& Status, const TMap<FString, FString>& Properties, FOnResult OnDone)
{
	UEFPlatformProviderSubsystem* Provider = GetProvider(EEFPlatformFeature::Presence);
	if (!Provider)
	{
		AnswerLater(MoveTemp(OnDone), NoProvider(EEFPlatformFeature::Presence));
		return;
	}
	const TSharedRef<bool> bReturned = MakeShared<bool>(false);
	Provider->SetPresence(Status, Properties, Deferred<const FEFResult&>(MoveTemp(OnDone), bReturned));
	*bReturned = true;
}
