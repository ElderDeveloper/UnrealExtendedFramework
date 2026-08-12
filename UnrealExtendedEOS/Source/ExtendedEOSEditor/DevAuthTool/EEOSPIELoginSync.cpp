// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSPIELoginSync.h"

#include "Shared/EEOSLog.h"

#include "OnlineAccountStoredCredentials.h"
#include "UObject/UnrealType.h"

namespace
{
	/** Credential type string. The EOS OSS parses it in ToEOS_ELoginCredentialType as
	 *  "developer" → EOS_LCT_Developer; FString comparison is case-insensitive, so this also
	 *  matches the engine's "+LoginTypesAllowingDuplicates=Developer" entry. */
	const TCHAR* GDeveloperLoginType = TEXT("developer");

	/**
	 * Resolved lazily and cached ONLY on success. A `static UClass* C = FindObject(...)`
	 * one-liner would be a latent bug: this module can be asked for the bridge before
	 * OnlineSubsystemUtils' reflection data is registered, and caching that first null would
	 * disable the bridge for the whole editor session.
	 */
	UClass* GetOnlinePIESettingsClass()
	{
		static UClass* Cached = nullptr;
		if (!Cached)
		{
			Cached = FindObject<UClass>(nullptr, TEXT("/Script/OnlineSubsystemUtils.OnlinePIESettings"));
		}
		return Cached;
	}

	/** CDO + the two properties this bridge touches. Returns false (with a reason) if the
	 *  engine's layout ever changes under us, rather than silently doing nothing. */
	bool ResolveSettings(UObject*& OutCDO, FBoolProperty*& OutEnabledProp, FArrayProperty*& OutLoginsProp, FString& OutError)
	{
		OutCDO = nullptr;
		OutEnabledProp = nullptr;
		OutLoginsProp = nullptr;

		UClass* Cls = GetOnlinePIESettingsClass();
		if (!Cls)
		{
			OutError = TEXT("UOnlinePIESettings class not found (OnlineSubsystemUtils editor code missing?)");
			return false;
		}

		OutCDO = Cls->GetDefaultObject();
		if (!OutCDO)
		{
			OutError = TEXT("UOnlinePIESettings has no default object");
			return false;
		}

		OutEnabledProp = CastField<FBoolProperty>(Cls->FindPropertyByName(TEXT("bOnlinePIEEnabled")));
		OutLoginsProp = CastField<FArrayProperty>(Cls->FindPropertyByName(TEXT("Logins")));
		if (!OutEnabledProp || !OutLoginsProp)
		{
			OutError = TEXT("UOnlinePIESettings is missing bOnlinePIEEnabled/Logins — engine layout changed");
			return false;
		}

		// The array is written with the real struct type, so verify the assumption instead of
		// reinterpret-casting whatever the engine happens to hold there.
		const FStructProperty* InnerStruct = CastField<FStructProperty>(OutLoginsProp->Inner);
		if (!InnerStruct || InnerStruct->Struct != FOnlineAccountStoredCredentials::StaticStruct())
		{
			OutError = TEXT("UOnlinePIESettings::Logins is not an array of FOnlineAccountStoredCredentials");
			return false;
		}

		return true;
	}
}

bool FEEOSPIELoginSync::IsAvailable()
{
	return GetOnlinePIESettingsClass() != nullptr;
}

bool FEEOSPIELoginSync::WriteDeveloperLogins(const TArray<FString>& CredentialNames, const FString& Address, FString& OutError)
{
	UObject* CDO = nullptr;
	FBoolProperty* EnabledProp = nullptr;
	FArrayProperty* LoginsProp = nullptr;
	if (!ResolveSettings(CDO, EnabledProp, LoginsProp, OutError))
	{
		return false;
	}

	FScriptArrayHelper ArrayHelper(LoginsProp, LoginsProp->ContainerPtrToValuePtr<void>(CDO));
	ArrayHelper.Resize(CredentialNames.Num());

	for (int32 Index = 0; Index < CredentialNames.Num(); ++Index)
	{
		FOnlineAccountStoredCredentials& Entry = *reinterpret_cast<FOnlineAccountStoredCredentials*>(ArrayHelper.GetRawPtr(Index));
		Entry.Type = GDeveloperLoginType;
		Entry.Id = Address;
		Entry.Token = CredentialNames[Index];

		// Token is UPROPERTY(Transient) — only TokenBytes is serialised, and PostInitProperties
		// decrypts it back on load. Skipping this would persist an entry whose Token is empty,
		// which IsValid() rejects and GetPIELogins() then silently drops.
		Entry.Encrypt();
	}

	EnabledProp->SetPropertyValue_InContainer(CDO, CredentialNames.Num() > 0);

	// config = EditorPerProjectUserSettings, i.e. a per-user file — SaveConfig is correct here
	// (this is not a checked-in project default).
	CDO->SaveConfig();

	UE_LOG(LogExtendedEOS, Log, TEXT("DevAuthTool: wrote %d PIE login(s) pointing at '%s'."), CredentialNames.Num(), *Address);
	return true;
}

bool FEEOSPIELoginSync::FLoginEntry::IsDeveloperType() const
{
	return Type == GDeveloperLoginType;
}

bool FEEOSPIELoginSync::ReadAllLogins(TArray<FLoginEntry>& OutLogins, bool& bOutLoginsEnabled)
{
	OutLogins.Reset();
	bOutLoginsEnabled = false;

	UObject* CDO = nullptr;
	FBoolProperty* EnabledProp = nullptr;
	FArrayProperty* LoginsProp = nullptr;
	FString Error;
	if (!ResolveSettings(CDO, EnabledProp, LoginsProp, Error))
	{
		UE_LOG(LogExtendedEOS, Verbose, TEXT("DevAuthTool: cannot read PIE logins — %s"), *Error);
		return false;
	}

	bOutLoginsEnabled = EnabledProp->GetPropertyValue_InContainer(CDO);

	FScriptArrayHelper ArrayHelper(LoginsProp, LoginsProp->ContainerPtrToValuePtr<void>(CDO));
	OutLogins.Reserve(ArrayHelper.Num());

	for (int32 Index = 0; Index < ArrayHelper.Num(); ++Index)
	{
		const FOnlineAccountStoredCredentials& Entry = *reinterpret_cast<const FOnlineAccountStoredCredentials*>(ArrayHelper.GetRawPtr(Index));

		// Every entry is reported, whatever its type — the index IS the PIE instance number, so
		// skipping entries here would misreport which client gets which account.
		FLoginEntry& Login = OutLogins.AddDefaulted_GetRef();
		Login.InstanceIndex = Index;
		Login.Type = Entry.Type;
		Login.Id = Entry.Id;
		Login.Token = Entry.Token;
		Login.bValid = Entry.IsValid();
	}

	return true;
}

int32 FEEOSPIELoginSync::GetValidLoginCount()
{
	TArray<FLoginEntry> Logins;
	bool bEnabled = false;
	if (!ReadAllLogins(Logins, bEnabled))
	{
		return 0;
	}

	int32 Count = 0;
	for (const FLoginEntry& Login : Logins)
	{
		Count += Login.bValid ? 1 : 0;
	}
	return Count;
}
