// Copyright Kemal Erdem YILMAZ. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "Shared/EEOSNativeOperation.h"
#include "Shared/EEOSLog.h"

/** Holds a native named-operation lease after its UObject stops receiving callbacks. */
struct FEEOSRetiredSessionOperation : TSharedFromThis<FEEOSRetiredSessionOperation>
{
	enum class EKind { Create, Join, Destroy, Start, End, Update };
	IOnlineSessionPtr Sessions;
	FEEOSNativeOperationLease Lease;
	FName Instance, Name;
	EKind Kind = EKind::Destroy;
	FDelegateHandle Handle;
	FTSTicker::FDelegateHandle Watcher;
	double Started = 0;
	bool bWarned = false;
	bool bCleanupMembership = false;

	static void Hold(const IOnlineSessionPtr& InSessions, const FEEOSNativeOperationLease& InLease,
		FName InInstance, FName InName, EKind InKind, bool bCleanup = false)
	{
		if (!InSessions.IsValid() || !InLease.IsValid()) return;
		const auto Self = MakeShared<FEEOSRetiredSessionOperation>();
		Self->Sessions = InSessions; Self->Lease = InLease; Self->Instance = InInstance;
		Self->Name = InName; Self->Kind = InKind; Self->bCleanupMembership = bCleanup;
		Self->Started = FPlatformTime::Seconds(); Self->Bind();
		Self->Watcher = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self](float)
		{
			IOnlineSubsystem* OSS = IOnlineSubsystem::DoesInstanceExist(Self->Instance) ? IOnlineSubsystem::Get(Self->Instance) : nullptr;
			if (!OSS || OSS->GetSessionInterface() != Self->Sessions) { Self->Finish(); return false; }
			if (!Self->bWarned && FPlatformTime::Seconds() - Self->Started > 15)
			{
				Self->bWarned = true;
				UE_LOG(LogExtendedEOS, Warning, TEXT("EOSRetiredOperation Request=%lld Name=%s Phase=WaitingForNativeCompletion"), Self->Lease.GetRequestId(), *Self->Name.ToString());
			}
			return true;
		}), 1.0f);
	}
	void Bind()
	{
		const auto Self = AsShared();
		const auto Callback = [Self](FName N, bool bSuccess) { if (N == Self->Name) Self->Complete(bSuccess); };
		switch (Kind)
		{
		case EKind::Create: Handle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateLambda(Callback)); break;
		case EKind::Join: Handle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateLambda([Self](FName N, EOnJoinSessionCompleteResult::Type R) { if (N == Self->Name) Self->Complete(R == EOnJoinSessionCompleteResult::Success); })); break;
		case EKind::Destroy: Handle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateLambda(Callback)); break;
		case EKind::Start: Handle = Sessions->AddOnStartSessionCompleteDelegate_Handle(FOnStartSessionCompleteDelegate::CreateLambda(Callback)); break;
		case EKind::End: Handle = Sessions->AddOnEndSessionCompleteDelegate_Handle(FOnEndSessionCompleteDelegate::CreateLambda(Callback)); break;
		case EKind::Update: Handle = Sessions->AddOnUpdateSessionCompleteDelegate_Handle(FOnUpdateSessionCompleteDelegate::CreateLambda(Callback)); break;
		}
	}
	void Unbind()
	{
		if (!Sessions.IsValid()) return;
		switch (Kind)
		{
		case EKind::Create: Sessions->ClearOnCreateSessionCompleteDelegate_Handle(Handle); break;
		case EKind::Join: Sessions->ClearOnJoinSessionCompleteDelegate_Handle(Handle); break;
		case EKind::Destroy: Sessions->ClearOnDestroySessionCompleteDelegate_Handle(Handle); break;
		case EKind::Start: Sessions->ClearOnStartSessionCompleteDelegate_Handle(Handle); break;
		case EKind::End: Sessions->ClearOnEndSessionCompleteDelegate_Handle(Handle); break;
		case EKind::Update: Sessions->ClearOnUpdateSessionCompleteDelegate_Handle(Handle); break;
		}
		Handle.Reset();
	}
	void Complete(bool bSuccess)
	{
		const auto Self = AsShared(); Unbind();
		if (bCleanupMembership && bSuccess && (Kind == EKind::Create || Kind == EKind::Join) && Sessions->GetNamedSession(Name))
		{
			Kind = EKind::Destroy; bCleanupMembership = false; Bind();
			if (!Sessions->DestroySession(Name) && Handle.IsValid()) Finish();
			return;
		}
		Finish();
	}
	void Finish()
	{
		const auto Self = AsShared(); Unbind();
		if (Watcher.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(Watcher);
		Watcher.Reset(); Lease.Reset(); Sessions.Reset();
	}
};
