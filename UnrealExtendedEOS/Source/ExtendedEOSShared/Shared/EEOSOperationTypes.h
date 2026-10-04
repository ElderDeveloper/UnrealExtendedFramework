// Copyright Kemal Erdem YILMAZ. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "EEOSOperationTypes.generated.h"

UENUM(BlueprintType)
enum class EEOSOperationCode : uint8
{
	None, Started, Succeeded, Busy, InvalidTarget, InvalidInput, StaleResult, AlreadyInLobby,
	LobbyFull, IdentityUnavailable, UnsupportedCapability, NativeStartRejected,
	ExistingLobbyCloseFailed, NativeFailure, Canceled, Superseded, AlreadyLoggedIn
};
UENUM(BlueprintType)
enum class EEOSResultSource : uint8 { Plugin, NativeCallback, SDKCallback, SDKCall };
UENUM(BlueprintType)
enum class EEOSSessionBackend : uint8 { Unknown, Lobby, Session, LAN };
UENUM(BlueprintType)
enum class EEOSReadinessState : uint8 { InstanceAbsent, Initializing, Ready, ShuttingDown, CreationExhausted };
UENUM(BlueprintType)
enum class EEOSLobbyPermission : uint8 { Unknown, Public, Presence, InviteOnly };

/** Observed capability only; Epic profile/presence and Connect membership are independent. */
USTRUCT(BlueprintType)
struct EXTENDEDEOSSHARED_API FEEOSReadinessSnapshot
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") EEOSReadinessState State = EEOSReadinessState::InstanceAbsent;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") FName Instance;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") int64 Generation = 0;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") bool bPlatformAvailable = false;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") bool bNativeLoggedIn = false;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") bool bHasEpicAccount = false;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") bool bHasProductUserId = false;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") bool bConnectLoggedIn = false;
};

/** Unknown capacity is not zero occupancy. Open-slot mapping depends on the backend. */
USTRUCT(BlueprintType)
struct EXTENDEDEOSSHARED_API FEEOSCapacitySnapshot
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") EEOSSessionBackend Backend = EEOSSessionBackend::Unknown;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") bool bKnown = false;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") bool bConsistent = false;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") int32 Maximum = -1;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") int32 Members = -1;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") int32 AvailableSlots = -1;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") int32 RawOpenPublic = -1;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") int32 RawOpenPrivate = -1;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") FString Source;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") FString UnknownReason;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") int64 SearchGeneration = 0;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") double SnapshotAgeSeconds = 0;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") EEOSLobbyPermission Permission = EEOSLobbyPermission::Unknown;
};

/** Native name-only callbacks do not supply the underlying SDK result. */
USTRUCT(BlueprintType)
struct EXTENDEDEOSSHARED_API FEEOSOperationOutcome
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") int64 RequestId = 0;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") int64 ParentRequestId = 0;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") int64 ContextGeneration = 0;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") int64 MembershipGeneration = 0;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") int64 SearchGeneration = 0;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") FString OriginalId;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") FDateTime StartedUtc;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") FDateTime CompletedUtc;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") FDateTime LastTransitionUtc;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") FName Operation;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") EEOSOperationCode Code = EEOSOperationCode::None;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") EEOSResultSource Source = EEOSResultSource::Plugin;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") bool bSuccess = false;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") FString Message;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") FString NativeResult;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") FString SDKResult;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") FString TargetId;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") FString CurrentId;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") FName Phase;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") int64 BlockingRequestId = 0;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") FName BlockingOperation;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") FName BlockingPhase;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") double ElapsedMilliseconds = 0;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEEOSOperationOutcome, const FEEOSOperationOutcome&, Outcome);

/** Confirmed native advertisement state, not an application admission/password rule. */
USTRUCT(BlueprintType)
struct EXTENDEDEOSSHARED_API FEEOSLobbyVisibilitySnapshot
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") bool bKnown = false;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") bool bShouldAdvertise = false;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") FString LobbyId;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Diagnostics") int64 Generation = 0;
};

USTRUCT(BlueprintType)
struct EXTENDEDEOSSHARED_API FEEOSChatCapabilitySnapshot
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Chat") bool bNativeInterfaceAvailable = false;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Chat") EEOSOperationCode Code = EEOSOperationCode::UnsupportedCapability;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Chat") bool bLocalIdentityReady = false;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Chat") bool bCanSubmitNativeMessage = false;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Chat") bool bLocalHistoryAvailable = true;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Chat") FName Route = TEXT("Unavailable");
};

/** The void adapter setters do not confirm device acceptance or post-AGC gain/sample rate. */
USTRUCT(BlueprintType)
struct EXTENDEDEOSSHARED_API FEEOSAudioSettingsSnapshot
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Voice") bool bVoiceUserAvailable = false;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Voice") bool bInputSubmissionMade = false;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Voice") bool bOutputSubmissionMade = false;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Voice") float RequestedInputVolume = 1;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Voice") float RequestedOutputVolume = 1;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Voice") bool bInputDeviceSubmissionMade = false;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Voice") bool bOutputDeviceSubmissionMade = false;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Voice") bool bAcceptanceKnown = false;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Voice") bool bEffectiveGainKnown = false;
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Voice") bool bEffectiveSampleRateKnown = false;
};
