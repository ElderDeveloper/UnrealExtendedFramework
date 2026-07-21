// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/ESteamSubsystem.h"
#include "Shared/ESteamTypes.h"
#include "ESteamRemotePlaySubsystem.generated.h"

/** Form factor of a Remote Play client device (mirrors ESteamDeviceFormFactor). */
UENUM(BlueprintType)
enum class EESteamRemotePlayFormFactor : uint8
{
	Unknown,
	Phone,
	Tablet,
	Computer,
	TV,
	VRHeadset
};

/** Details of one connected Remote Play session. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDSTEAM_API FESteamRemotePlaySessionInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Steam|RemotePlay")
	int32 SessionId = 0;

	/** SteamID of the connected user driving this session (0 when unavailable). */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|RemotePlay")
	FESteamId UserId;

	UPROPERTY(BlueprintReadOnly, Category = "Steam|RemotePlay")
	FString ClientName;

	/** Form factor of the client device. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|RemotePlay")
	EESteamRemotePlayFormFactor FormFactor = EESteamRemotePlayFormFactor::Unknown;

	/** Client display resolution in pixels; (0, 0) when not available. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|RemotePlay")
	FIntPoint Resolution = FIntPoint::ZeroValue;
};

/** Fired when a Remote Play session connects. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteamRemotePlaySessionConnected, int32, SessionId);

/** Fired when a Remote Play session disconnects. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteamRemotePlaySessionDisconnected, int32, SessionId);

/**
 * Wraps ISteamRemotePlay: information about connected Steam Remote Play sessions
 * and Remote Play Together invites.
 */
UCLASS()
class UNREALEXTENDEDSTEAM_API UESteamRemotePlaySubsystem : public UESteamSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	/** Number of currently connected Remote Play sessions. */
	UFUNCTION(BlueprintPure, Category = "Steam|RemotePlay")
	int32 GetSessionCount() const;

	/** Session id at the given index (0 when out of bounds). */
	UFUNCTION(BlueprintPure, Category = "Steam|RemotePlay")
	int32 GetSessionId(int32 Index) const;

	/** Name of the session's client device (empty when the session id is invalid). */
	UFUNCTION(BlueprintPure, Category = "Steam|RemotePlay")
	FString GetSessionClientName(int32 SessionId) const;

	/** SteamID of the user driving a session (invalid id when the session id is not valid). */
	UFUNCTION(BlueprintPure, Category = "Steam|RemotePlay")
	FESteamId GetSessionSteamID(int32 SessionId) const;

	/** Full details of a session. Returns false when the session id is invalid. */
	UFUNCTION(BlueprintCallable, Category = "Steam|RemotePlay")
	bool GetSessionInfo(int32 SessionId, FESteamRemotePlaySessionInfo& OutInfo) const;

	/** Invites a friend to Remote Play Together. Fails when the game is not configured for it. */
	UFUNCTION(BlueprintCallable, Category = "Steam|RemotePlay")
	bool SendRemotePlayTogetherInvite(FESteamId Friend);

	/**
	 * Opens the Remote Play Together UI in the game overlay. Returns false when the game is
	 * not configured for Remote Play Together.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|RemotePlay")
	bool ShowRemotePlayTogetherUI();

	UPROPERTY(BlueprintAssignable, Category = "Steam|RemotePlay")
	FOnSteamRemotePlaySessionConnected OnRemotePlaySessionConnected;

	UPROPERTY(BlueprintAssignable, Category = "Steam|RemotePlay")
	FOnSteamRemotePlaySessionDisconnected OnRemotePlaySessionDisconnected;

protected:
	virtual void HandleSteamClientInitialized() override;
	virtual void HandleSteamClientShutdown() override;

private:
	friend class FESteamRemotePlayCallbacks;
	TSharedPtr<class FESteamRemotePlayCallbacks> Callbacks;
};
