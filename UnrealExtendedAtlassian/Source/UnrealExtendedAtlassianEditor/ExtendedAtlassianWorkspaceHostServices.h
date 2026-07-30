// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExtendedAtlassianContextCapture.h"
#include "ExtendedAtlassianTypes.h"

/**
 * Injectable boundary for operating-system and Unreal Editor actions initiated
 * by the Backlot workspace. Tests provide an in-memory implementation.
 */
class IExtendedAtlassianWorkspaceHostServices
{
public:
	virtual ~IExtendedAtlassianWorkspaceHostServices() = default;

	virtual double NowSeconds() const = 0;
	/** True when the host OS asks applications to reduce or disable client-area animation. */
	virtual bool ShouldReduceMotion() const = 0;
	/** True when the host OS is using a high-contrast accessibility theme. */
	virtual bool ShouldUseHighContrast() const = 0;
	virtual bool CaptureViewport(TArray<uint8>& OutPngData, FIntPoint& OutSize) = 0;
	virtual FExtendedAtlassianCapturedContext CaptureContext() = 0;
	virtual void CopyText(const FString& Text) = 0;
	virtual void OpenExternal(const FString& Url) = 0;
	virtual bool ResolveCurrentTarget(
		EExtendedAtlassianPinKind Kind,
		const FString& SelectedPageId,
		const FString& SelectedPageTitle,
		FExtendedAtlassianPinTarget& OutTarget,
		FText& OutError) = 0;
	virtual bool RevealTarget(
		const FExtendedAtlassianPinTarget& Target,
		FText& OutError) = 0;
};

/** Production adapter for native editor and operating-system services. */
class FExtendedAtlassianSystemWorkspaceHostServices final
	: public IExtendedAtlassianWorkspaceHostServices
{
public:
	virtual double NowSeconds() const override;
	virtual bool ShouldReduceMotion() const override;
	virtual bool ShouldUseHighContrast() const override;
	virtual bool CaptureViewport(
		TArray<uint8>& OutPngData,
		FIntPoint& OutSize) override;
	virtual FExtendedAtlassianCapturedContext CaptureContext() override;
	virtual void CopyText(const FString& Text) override;
	virtual void OpenExternal(const FString& Url) override;
	virtual bool ResolveCurrentTarget(
		EExtendedAtlassianPinKind Kind,
		const FString& SelectedPageId,
		const FString& SelectedPageTitle,
		FExtendedAtlassianPinTarget& OutTarget,
		FText& OutError) override;
	virtual bool RevealTarget(
		const FExtendedAtlassianPinTarget& Target,
		FText& OutError) override;
};
