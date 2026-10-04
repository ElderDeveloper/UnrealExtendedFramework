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
	struct FRetired : TSharedFromThis<FRetired>
	{
		IOnlineIdentityPtr Identity;
		FEEOSNativeOperationLease Lease;
		FName Instance;
		FDelegateHandle Handle;
		FTSTicker::FDelegateHandle Watcher;
		bool bLogout = false;
		void Finish()
		{
			const auto Self = AsShared();
			if (Identity.IsValid())
			{
				if (bLogout) Identity->ClearOnLogoutCompleteDelegate_Handle(0, Handle);
				else Identity->ClearOnLoginCompleteDelegate_Handle(0, Handle);
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
			Self->Watcher = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self](float)
			{
				IOnlineSubsystem* OSS = IOnlineSubsystem::DoesInstanceExist(Self->Instance) ? IOnlineSubsystem::Get(Self->Instance) : nullptr;
				if (!OSS || OSS->GetIdentityInterface() != Self->Identity) { Self->Finish(); return false; }
				return true;
			}), 1.0f);
		}
	};
}
