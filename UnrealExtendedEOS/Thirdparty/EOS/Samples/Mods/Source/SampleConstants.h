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

	/** Client id of the service permissions entry (found on the dev portal) */
	static constexpr char ClientCredentialsId[] = "";

	/** Client secret for accessing the set of permissions (found on the dev portal) */
	static constexpr char ClientCredentialsSecret[] = "";

	/** Game name */
	static constexpr char GameName[] = "Mods";

	/** Encryption key. It must match the one you are using on Dev Portal when uploading files. 64 hex characters. */
	static constexpr char EncryptionKey[] = "1111111111111111111111111111111111111111111111111111111111111111";

	/** The Minimum window Width for this sample. */
	static constexpr int32_t MinimumWindowWidth = 1024;

	/** The Minimum window Height for this sample. */
	static constexpr int32_t MinimumWindowHeight = 800;

	/** The Default window Width for this sample. */
	static constexpr int32_t DefaultWindowWidth = 1024;

	/** The Default window Height for this sample. */
	static constexpr int32_t DefaultWindowHeight = 800;
};