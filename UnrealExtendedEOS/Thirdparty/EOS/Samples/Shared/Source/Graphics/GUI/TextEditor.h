// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TextView.h"

class FTextFieldWidget;

/**
 * Text editor widget that can view and edit large amount of scrollable text
 */
class FTextEditorWidget : public FTextViewWidget
{
public:
	/**
	 * Constructor
	 */
	FTextEditorWidget(Vector2 TextEditorPos,
		Vector2 TextEditorSize,
		UILayer TextEditorLayer,
		const std::wstring& InitialText,
		const std::wstring& TextEditorTextureFile,
		FontPtr TextEditorFont,
		FColor BackCol = FColor(0.f, 0.f, 0.f, 1.f),
		FColor TextCol = FColor(1.f, 1.f, 1.f, 1.f));

	/**
	 * Destructor
	 */
	virtual ~FTextEditorWidget() {};

	/** IGfxComponent */
	virtual void Create() override;
	virtual void Release() override;
	virtual void Update() override;
	virtual void Render(FSpriteBatchPtr& Batch) override;

	/** IWidget */
	virtual void OnUIEvent(const FUIEvent& event) override;
	virtual void SetPosition(Vector2 Pos) override;
	virtual void SetSize(Vector2 NewSize) override;
	virtual void SetFocused(bool bValue) override;
	void Clear();

	std::wstring GetText();
	void SetText(const std::wstring& Text, FColor = Color::White);

	void SetOnEnterPressedCallback(std::function<void(const std::wstring&)> Callback)
	{
		OnEnterPressedCallback = Callback;
	}

	void ClearOnEnterPressedCallback()
	{
		OnEnterPressedCallback = nullptr;
	}

	void SetEditingEnabled(bool bValue)
	{
		bEditingEnabled = bValue;
	}

protected:
	void UpdateEditingFieldTransform();
	void ChangeEditingLine(size_t NewLineIndex);
	void StopEditing();
	void SaveEditedLine();
	void CheckAndScroll();
	void PasteText();
	void InsertTextAfterSelectedLine(std::wstring&& InsertedText);

	// Text input widget for editing currently selected line
	std::shared_ptr<FTextFieldWidget> EditingLine;

	std::function<void(const std::wstring&)> OnEnterPressedCallback;

	bool bEditingEnabled = true;
};
