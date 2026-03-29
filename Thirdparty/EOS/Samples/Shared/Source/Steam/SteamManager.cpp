// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"

#ifdef EOS_STEAM_ENABLED
#include "DebugLog.h"
#include "StringUtils.h"
#include "Game.h"
#include "GameEvent.h"
#include "Authentication.h"
#include "Users.h"
#include "SteamManager.h"
#include "CommandLine.h"

#include "steam/steam_api.h"

/**
 * Steam Manager Implementation
 */
class FSteamManager::FImpl
{
public:
	FImpl();
	~FImpl();

	void Init();
	void Update();
	void RetrieveAuthSessionTicket();
	void OnGetAuthSessionTicket(GetTicketForWebApiResponse_t* pTicketResponse);

	void OnLoginComplete();
	void StartLogin();

private:

	void CleanupResources();

	CCallbackManual<FSteamManager::FImpl, GetTicketForWebApiResponse_t> SteamCallbackGetTicketForWebApi;
	/** Auth session ticket converted to Hex, to pass to the EOS SDK */
	std::wstring AuthSessionTicket;
	/** We need to keep hold of this and then call ISteamUser::CancelAuthTicket when done with the login */
	HAuthTicket AuthSessionTicketHandle;
	bool bIsInitialized = false;
};

FSteamManager::FImpl::FImpl()
{
	SteamCallbackGetTicketForWebApi.Register(this, &FImpl::OnGetAuthSessionTicket);
}

FSteamManager::FImpl::~FImpl()
{
	SteamCallbackGetTicketForWebApi.Unregister();
}

void FSteamManager::FImpl::Init()
{
	if (SteamAPI_Init())
	{
		FDebugLog::Log(L"Steam - Initialized");

		bIsInitialized = true;

		RetrieveAuthSessionTicket();
	}
	else
	{
		FDebugLog::LogError(L"Steam must be running to play this game (SteamAPI_Init() failed)");
	}
}

void FSteamManager::FImpl::Update()
{
	if (!bIsInitialized)
	{
		return;
	}

	// Run Steam client callbacks
	SteamAPI_RunCallbacks();
}

void FSteamManager::FImpl::RetrieveAuthSessionTicket()
{
	if (!bIsInitialized)
	{
		return;
	}

	FDebugLog::Log(L"Steam - Requesting Auth Session Ticket ...");

	AuthSessionTicketHandle = SteamUser()->GetAuthTicketForWebApi("epiconlineservices");
	if (AuthSessionTicketHandle == k_HAuthTicketInvalid)
	{
		FDebugLog::LogError(L"Steam - Unable to get the auth session ticket.");
		return;
	}
}

void FSteamManager::FImpl::CleanupResources()
{
	AuthSessionTicket.clear();

	// Once login finished(successfully or not), or the actual steam callback returned an error we need to call Steamwork's ISteamUser::CancelAuthTicket.
	if (AuthSessionTicketHandle != k_HAuthTicketInvalid)
	{
		SteamUser()->CancelAuthTicket(AuthSessionTicketHandle);
		AuthSessionTicketHandle = k_HAuthTicketInvalid;
	}
}

void FSteamManager::FImpl::OnGetAuthSessionTicket(GetTicketForWebApiResponse_t* pTicketResponse)
{
	// GetAuthSessionTicketResponse_t is broadcast to all listeners, so if we get the wrong handle, it doesn't necessarily mean it's an error,
	// since maybe some other code is also processing auth session tickets.
	if (pTicketResponse->m_hAuthTicket != AuthSessionTicketHandle)
	{
		FDebugLog::LogWarning(L"Steam - Ignoring unexpected GetTicketForWebApiResponse_t callback");
		return;
	}

	if (pTicketResponse->m_eResult != k_EResultOK)
	{
		FDebugLog::LogError(L"Steam - GetTicketForWebApiResponse_t callback failed. Error %d", (int)pTicketResponse->m_eResult);
		CleanupResources();
		return;
	}

	const uint32 StringBufSize = ((uint32)pTicketResponse->m_cubTicket * 2) + 1;
	std::vector<char> NarrowAuthSessionTicket(StringBufSize);
	uint32_t OutLen = StringBufSize;
	const EOS_EResult ConvResult = EOS_ByteArray_ToString(pTicketResponse->m_rgubTicket, (uint32_t)pTicketResponse->m_cubTicket, NarrowAuthSessionTicket.data(), &OutLen);
	if (ConvResult != EOS_EResult::EOS_Success)
	{
		FDebugLog::LogError(L"Steam - OnGetAuthSessionTicket - EOS_ByteArray_ToString failed - Result: %ls", FStringUtils::Widen(EOS_EResult_ToString(ConvResult)).c_str());
		CleanupResources();
		return;
	}

	assert(OutLen == StringBufSize);
	AuthSessionTicket = FStringUtils::Widen(std::string(NarrowAuthSessionTicket.data(), NarrowAuthSessionTicket.size()));

	StartLogin();
}

void FSteamManager::FImpl::StartLogin()
{
	if (!AuthSessionTicket.empty())
	{
		FDebugLog::Log(L"Steam - StartLogin - Auth Session Ticket: %ls", AuthSessionTicket.c_str());

		FGameEvent Event(EGameEventType::StartUserLogin, AuthSessionTicket, (int)ELoginMode::ExternalAuth, (int)ELoginExternalType::Steam);
		FGame::Get().OnGameEvent(Event);
	}
	else
	{
		FDebugLog::LogError(L"Steam - StartLogin - Invalid Steam Auth Session Ticket");
	}
}

void FSteamManager::FImpl::OnLoginComplete()
{
	CleanupResources();
}

std::unique_ptr<FSteamManager> FSteamManager::Instance;

FSteamManager::FSteamManager()
	: Impl(new FImpl())
{

}

FSteamManager::~FSteamManager()
{

}

FSteamManager& FSteamManager::GetInstance()
{
	if (!Instance)
	{
		Instance = std::unique_ptr<FSteamManager>(new FSteamManager());
	}

	return *Instance;
}

void FSteamManager::ClearInstance()
{
	Instance.reset();
}

void FSteamManager::Init()
{
	Impl->Init();
}

void FSteamManager::Update()
{
	Impl->Update();
}

void FSteamManager::RetrieveAuthSessionTicket()
{
	Impl->RetrieveAuthSessionTicket();
}

void FSteamManager::StartLogin()
{
	Impl->StartLogin();
}

void FSteamManager::OnGameEvent(const FGameEvent& Event)
{
	if (Event.GetType() == EGameEventType::UserInfoRetrieved)
	{
		FEpicAccountId UserId = Event.GetUserId();

		// Log Steam Display Name
		std::wstring DisplayName = FGame::Get().GetUsers()->GetExternalAccountDisplayName(UserId, UserId, EOS_EExternalAccountType::EOS_EAT_STEAM);
		if (!DisplayName.empty())
		{
			FDebugLog::Log(L"[EOS SDK] External Account Display Name: %ls", DisplayName.c_str());
		}
		else
		{
			FDebugLog::LogError(L"[EOS SDK] External Account Display Name Not Found");
		}
	}
	else if ((Event.GetType() == EGameEventType::UserLoggedIn) || (Event.GetType() == EGameEventType::UserLoginFailed))
	{
		Impl->OnLoginComplete();
	}
}

#endif //EOS_STEAM_ENABLED