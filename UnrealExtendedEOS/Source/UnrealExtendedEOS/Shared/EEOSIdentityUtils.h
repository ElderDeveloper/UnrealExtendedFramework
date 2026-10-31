// Copyright Kemal Erdem YILMAZ. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "EEOSBlueprintLibrary.h"
#include "Shared/EEOSNativeOperation.h"

namespace EEOSIdentity
{
	inline bool HasLocalEpicAccount(const IOnlineIdentityPtr& Identity)
	{
		const auto Local = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : FUniqueNetIdPtr();
		return Local.IsValid() && Local->IsValid() && Identity->GetLoginStatus(0) == ELoginStatus::LoggedIn
			&& !UEEOSBlueprintLibrary::ExtractEpicAccountId(Local->ToString()).IsEmpty();
	}
	inline FString SafeNickname(const IOnlineIdentityPtr& Identity, const FUniqueNetId& User)
	{
		return HasLocalEpicAccount(Identity) && User.IsValid() ? Identity->GetPlayerNickname(User) : FString();
	}
	inline FString SafeLocalNickname(const IOnlineIdentityPtr& Identity)
	{
		return HasLocalEpicAccount(Identity) ? Identity->GetPlayerNickname(0) : FString();
	}
	/** Keeps the lease of a native login/logout that outlives the subsystem which submitted it.
	 * The identity is held WEAKLY on purpose: FOnlineSubsystemEOS::Shutdown drops its user manager
	 * expecting it to die there, and the engine's EOS callbacks check nothing but that owner
	 * (TEOSCallback::Owner). A strong reference kept the user manager alive past its online
	 * subsystem, so a login answered after a quick PIE stop ran FullLoginCallback on freed memory. */
	struct FRetired : TSharedFromThis<FRetired>
	{
		TWeakPtr<IOnlineIdentity, ESPMode::ThreadSafe> Identity;
		FEEOSNativeOperationLease Lease;
		FName Instance;
		FDelegateHandle Handle;
		FTSTicker::FDelegateHandle Watcher;
		bool bLogout = false;
		void Finish()
		{
			const auto Self = AsShared();
			if (const IOnlineIdentityPtr Live = Identity.Pin())
			{
				if (bLogout) Live->ClearOnLogoutCompleteDelegate_Handle(0, Handle);
				else Live->ClearOnLoginCompleteDelegate_Handle(0, Handle);
			}
			if (Watcher.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(Watcher);
			Watcher.Reset(); Handle.Reset(); Lease.Reset(); Identity.Reset();
		}
		static void Hold(const IOnlineIdentityPtr& Identity, const FEEOSNativeOperationLease& Lease, FName Instance, bool bLogout)
		{
			if (!Identity.IsValid() || !Lease.IsValid()) return;
			const auto Self = MakeShared<FRetired>();
			Self->Identity = Identity; Self->Lease = Lease; Self->Instance = Instance; Self->bLogout = bLogout;
			if (bLogout) Self->Handle = Identity->AddOnLogoutCompleteDelegate_Handle(0, FOnLogoutCompleteDelegate::CreateLambda(
				[Self](int32 User, bool) { if (User == 0) Self->Finish(); }));
			else Self->Handle = Identity->AddOnLoginCompleteDelegate_Handle(0, FOnLoginCompleteDelegate::CreateLambda(
				[Self](int32 User, bool, const FUniqueNetId&, const FString&) { if (User == 0) Self->Finish(); }));
			// Every frame: the lease is keyed on the identity's address, which is free for reuse once it is gone.
			Self->Watcher = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self](float)
			{
				const IOnlineIdentityPtr Live = Self->Identity.Pin();
				IOnlineSubsystem* OSS = Live.IsValid() && IOnlineSubsystem::DoesInstanceExist(Self->Instance) ? IOnlineSubsystem::Get(Self->Instance) : nullptr;
				if (!OSS || OSS->GetIdentityInterface() != Live) { Self->Finish(); return false; }
				return true;
			}));
		}
	};
}
