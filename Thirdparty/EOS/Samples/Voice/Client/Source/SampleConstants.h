// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

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
	static constexpr char GameName[] = "Voice";

	/** Encryption key. Not used by this sample. */
	static constexpr char EncryptionKey[] = "1111111111111111111111111111111111111111111111111111111111111111";

	/** Trusted Server URL */
	static constexpr char ServerURL[] = "http://127.0.0.1";

	/** Trusted Server Port */
	static constexpr int ServerPort = 1234;

	/** The Minimum window Width for this sample. */
	static constexpr int32_t MinimumWindowWidth = 1024;

	/** The Minimum window Height for this sample. */
	static constexpr int32_t MinimumWindowHeight = 800;

	/** The Default window Width for this sample. */
	static constexpr int32_t DefaultWindowWidth = 1024;

	/** The Default window Height for this sample. */
	static constexpr int32_t DefaultWindowHeight = 800;
};