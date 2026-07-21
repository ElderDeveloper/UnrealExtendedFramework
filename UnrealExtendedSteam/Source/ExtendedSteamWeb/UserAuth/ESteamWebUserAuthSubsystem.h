// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ESteamWebSubsystem.h"
#include "ESteamWebUserAuthSubsystem.generated.h"

/**
 * ISteamUserAuth — server-side validation of Steam auth session tickets.
 */
UCLASS()
class EXTENDEDSTEAMWEB_API UESteamWebUserAuthSubsystem : public UESteamWebSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * ISteamUserAuth/AuthenticateUserTicket/v1 — the server-side validation counterpart of the
	 * client's GetAuthTicketForWebApi / GetAuthSessionTicket: the client sends its ticket to your
	 * backend, which calls this to verify it and receive the owning SteamID64.
	 *
	 * The official docs place this on the partner host with a publisher key (used here), though
	 * it is also known to accept a regular user key on the public host. HexTicket is the binary
	 * ticket converted to a hexadecimal string. Identity is the identifying string passed to
	 * GetAuthTicketForWebApi when the ticket was created (SDK >= 1.57 tickets); it is optional
	 * and omitted when empty.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|UserAuth", meta = (AutoCreateRefTerm = "OnResponse"))
	void AuthenticateUserTicket(int32 AppId, FString HexTicket, FString Identity, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamUserAuth/AuthenticateUser/v1 (POST) — the RSA/encrypted-login-key handshake counterpart to
	 * AuthenticateUserTicket, used to open an authenticated Web API session for a user. The caller
	 * generates a random session key, RSA-encrypts it with Steam's public key, and encrypts the login
	 * key with that session key; SessionKeyHex and EncryptedLoginKeyHex are those two raw-binary blobs
	 * hex-encoded (sent as "sessionkey" / "encrypted_loginkey"). Unlike the rest of this interface the
	 * documented parameters carry NO api key — the encrypted session key is the credential — so no key
	 * is injected here. Partner host.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|UserAuth", meta = (AutoCreateRefTerm = "OnResponse"))
	void AuthenticateUser(FString SteamId, FString SessionKeyHex, FString EncryptedLoginKeyHex, const FOnSteamWebResponse& OnResponse);
};
