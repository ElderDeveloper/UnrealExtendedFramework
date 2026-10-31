// Copyright Moon Punch Games. All Rights Reserved.

#include "ExtendedFoundationModule.h"

#include "Http/EFHttpClient.h"
#include "Modules/ModuleManager.h"
#include "Status/EFServiceStatus.h"
#include "Status/EFStatusOverlay.h"

DEFINE_LOG_CATEGORY(LogExtendedFoundation);

void FExtendedFoundationModule::StartupModule()
{
	HttpStatusHandle = FEFServiceStatusRegistry::Register(TEXT("Http"), []
	{
		FEFServiceStatus Status;
		Status.State = EEFServiceState::Ready;
		const int32 Active = FEFHttpOperation::GetActiveCount();
		Status.Summary = FString::Printf(TEXT("%d request%s in flight"), Active, Active == 1 ? TEXT("") : TEXT("s"));
		if (EFHttp::GetTransportOverride().IsValid())
		{
			Status.State = EEFServiceState::Degraded;
			Status.Details.Add(TEXT("Transport"), TEXT("override installed (mock or test): requests do not reach the network"));
		}
		return Status;
	});
}

void FExtendedFoundationModule::ShutdownModule()
{
	EFStatusOverlay::Shutdown();
	FEFServiceStatusRegistry::Unregister(HttpStatusHandle);
	HttpStatusHandle = 0;
}

IMPLEMENT_MODULE(FExtendedFoundationModule, ExtendedFoundation)
