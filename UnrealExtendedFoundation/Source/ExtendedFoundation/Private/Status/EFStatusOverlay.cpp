// Copyright Moon Punch Games. All Rights Reserved.

#include "EFStatusOverlay.h"

#if !UE_BUILD_SHIPPING

#include "Debug/DebugDrawService.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Status/EFServiceStatus.h"

namespace EFStatusOverlayPrivate
{
	constexpr double RefreshIntervalSeconds = 0.5;
	constexpr float LineHeight = 14.f;

	FDelegateHandle DrawHandle;
	double LastRefreshSeconds = -1.0;
	TArray<FEFServiceStatus> CachedStatuses;
	int32 OverlayVisible = 0;

	FColor ColorFor(EEFServiceState State)
	{
		switch (State)
		{
		case EEFServiceState::Ready:         return FColor(110, 220, 110);
		case EEFServiceState::Starting:      return FColor(120, 180, 255);
		case EEFServiceState::Degraded:      return FColor(255, 200, 80);
		case EEFServiceState::Error:         return FColor(255, 90, 90);
		case EEFServiceState::NotConfigured: return FColor(255, 160, 60);
		case EEFServiceState::Unavailable:
		default:                             return FColor(160, 160, 160);
		}
	}

	void Draw(UCanvas* Canvas, APlayerController* /*PlayerController*/)
	{
		if (!Canvas || !GEngine)
		{
			return;
		}

		// Collecting runs every reporter; twice a second is plenty for a status display.
		const double Now = FPlatformTime::Seconds();
		if (LastRefreshSeconds < 0.0 || Now - LastRefreshSeconds >= RefreshIntervalSeconds)
		{
			CachedStatuses = FEFServiceStatusRegistry::Collect();
			LastRefreshSeconds = Now;
		}

		UFont* Font = GEngine->GetSmallFont();
		const float X = 24.f;
		float Y = Canvas->ClipY * 0.2f;

		Canvas->SetDrawColor(FColor::White);
		Canvas->DrawText(Font, TEXT("Extended.Status"), X, Y);
		Y += LineHeight;

		if (CachedStatuses.Num() == 0)
		{
			Canvas->SetDrawColor(ColorFor(EEFServiceState::Unavailable));
			Canvas->DrawText(Font, TEXT("  no services registered"), X, Y);
			return;
		}

		for (const FEFServiceStatus& Status : CachedStatuses)
		{
			Canvas->SetDrawColor(ColorFor(Status.State));
			const FString Line = FString::Printf(TEXT("  %-10s %-13s %s"),
				*Status.Service.ToString(), FEFServiceStatusRegistry::StateToString(Status.State), *Status.Summary);
			Canvas->DrawText(Font, Line, X, Y);
			Y += LineHeight;
		}
	}

	void Apply(bool bVisible)
	{
		if (bVisible && !DrawHandle.IsValid())
		{
			LastRefreshSeconds = -1.0;
			DrawHandle = UDebugDrawService::Register(TEXT("Game"), FDebugDrawDelegate::CreateStatic(&Draw));
		}
		else if (!bVisible && DrawHandle.IsValid())
		{
			UDebugDrawService::Unregister(DrawHandle);
			DrawHandle.Reset();
			CachedStatuses.Reset();
		}
	}

	FAutoConsoleVariableRef CVarOverlay(
		TEXT("Extended.Status.Overlay"),
		OverlayVisible,
		TEXT("1 draws Extended.Status over the game view, 0 hides it. Development builds only."),
		FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* Variable)
		{
			Apply(Variable && Variable->GetInt() != 0);
		}));
}

void EFStatusOverlay::SetVisible(bool bVisible)
{
	if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("Extended.Status.Overlay")))
	{
		Variable->Set(bVisible ? 1 : 0, ECVF_SetByCode);
	}
}

bool EFStatusOverlay::IsVisible()
{
	return EFStatusOverlayPrivate::DrawHandle.IsValid();
}

void EFStatusOverlay::Shutdown()
{
	EFStatusOverlayPrivate::Apply(false);
}

#else

void EFStatusOverlay::SetVisible(bool) {}
bool EFStatusOverlay::IsVisible() { return false; }
void EFStatusOverlay::Shutdown() {}

#endif // !UE_BUILD_SHIPPING
