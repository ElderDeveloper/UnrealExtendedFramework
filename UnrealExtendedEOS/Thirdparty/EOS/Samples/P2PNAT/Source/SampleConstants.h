// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <eos_p2p_types.h>

struct SampleConstants
{
	/** The product id for the running application, found on the dev portal */
	static constexpr char ProductId[] = "";

	/** The sandbox id for the running application, found on the dev portal */
	static constexpr char SandboxId[] = "";

	/** The deployment id for the running application, found on the dev portal */
	static constexpr char DeploymentId[] = "";

	/** Client id of the service permissions entry, found on the dev portal */
	static constexpr char ClientCredentialsId[] = "";

	/** Client secret for accessing the set of permissions, found on the dev portal */
	static constexpr char ClientCredentialsSecret[] = "";

	/** Game name */
	static constexpr char GameName[] = "P2P NAT";

	/** Encryption key. Not used by this sample. */
	static constexpr char EncryptionKey[] = "1111111111111111111111111111111111111111111111111111111111111111";

	/** Relay settings for P2P connects */
	static const EOS_ERelayControl RelaySetting = EOS_ERelayControl::EOS_RC_AllowRelays;

	/** Packet reliability for P2P communication */
	static const EOS_EPacketReliability PacketReliability = EOS_EPacketReliability::EOS_PR_ReliableOrdered;

	/** The Minimum window Width for this sample. */
	static constexpr int32_t MinimumWindowWidth = 1024;

	/** The Minimum window Height for this sample. */
	static constexpr int32_t MinimumWindowHeight = 800;

	/** The Default window Width for this sample. */
	static constexpr int32_t DefaultWindowWidth = 1024;

	/** The Default window Height for this sample. */
	static constexpr int32_t DefaultWindowHeight = 800;
};