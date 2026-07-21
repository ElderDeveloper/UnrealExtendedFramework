// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Dialog.h"
#include "Font.h"

/**
 * Forward declarations
 */
class FTextLabelWidget;
class FButtonWidget;
class FProgressBar;

/**
 * Transfer progress dialog
 */
class FTransferProgressDialog : public FDialog
{
public:
	/** Delegate for transfer control and info. */
	class FDelegate
	{
	public:
		virtual ~FDelegate() {}
		virtual void CancelTransfer() = 0;
		virtual const std::wstring& GetCurrentTransferName() = 0;
		virtual float GetCurrentTransferProgress() = 0;
	};

	/**
	 * Constructor
	 */
	FTransferProgressDialog(Vector2 DialogPos,
		Vector2 DialogSize,
		UILayer DialogLayer,
		std::wstring TransferName,
		FontPtr DialogNormalFont,
		FontPtr DialogSmallFont,
		std::shared_ptr<FTransferProgressDialog::FDelegate> Delegate);

	/**
	 * Destructor
	 */
	virtual ~FTransferProgressDialog() {};

	/** IWidget */
	virtual void SetPosition(Vector2 Pos) override;

	virtual void Update() override;

	virtual void OnEscapePressed() override;

	const std::wstring& GetTransferName() const { return TransferName; }
	void SetTransferName(const std::wstring& NewName) { TransferName = NewName; }

private:
	/** The name of file being transferred */
	std::wstring TransferName;

	/** Progress Dialog Background */
	WidgetPtr ProgressBackground;

	/** Progress Dialog Label */
	std::shared_ptr<FTextLabelWidget> ProgressLabel;

	/** Progress Bar */
	std::shared_ptr<FProgressBar> ProgressBar;

	/** Progress Dialog Cancel Button */
	std::shared_ptr<FButtonWidget> CancelTransferButton;

	/** Delegate for transfer control and info. */
	std::shared_ptr<FTransferProgressDialog::FDelegate> Delegate;
};
