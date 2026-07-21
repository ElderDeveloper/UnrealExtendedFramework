// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widget.h"
#include "Font.h"
#include "ListView.h"
#include "Dialog.h"
#include "StringViewListEntry.h"

class FTextLabelWidget;

/**
 * Drop down list widget (user can pick one of the options from the list).
 */
class FDropDownList : public FDialog
{
public:
	/**
	 * Constructor
	 */
	FDropDownList(Vector2 DropDownPosition,
		Vector2 DropDownInitialSize,
		Vector2 DropDownExpandedSize,
		UILayer TextFieldLayer,
		const std::wstring& InitialText,
		const std::vector<std::wstring>& OptionsList,
		FontPtr TextFieldFont,
		EAlignmentType AlignmentType = EAlignmentType::Center,
		FColor BackCol = FColor(1.f, 1.f, 1.f, 1.f),
		FColor TextCol = FColor(1.f, 1.f, 1.f, 1.f));

	/**
	* Destructor
	*/
	virtual ~FDropDownList() {};

	/** IWidget */
	virtual void OnUIEvent(const FUIEvent& Event) override;
	virtual void SetPosition(Vector2 Pos) override;
	virtual void SetSize(Vector2 NewSize) override;
	virtual void SetFocused(bool bValue) override;
	virtual void Render(FSpriteBatchPtr& Batch) override;
	virtual void Update() override;

	/** Callback that is called when user selects something from the list. */
	void SetOnSelectionCallback(std::function<void(const std::wstring&)> Callback);
	const std::wstring& GetCurrentSelection() const { return CurrentlySelectedOption; }

	/** Callback that is called when user expands the dropdown list. */
	void SetOnExpandedCallback(std::function<void()> Callback);

	/** Callback that is called when user collapses the dropdown list. */
	void SetOnCollapsedCallback(std::function<void()> Callback);

	/**
	 * Sets the list, and sets the specified entry as selected (by index)
	 */
	void UpdateOptionsList(const std::vector<std::wstring>& OptionsList, int SelectIndex = 0);

	void SelectEntry(size_t Index);
	void SetSelectedEntry(size_t Index);

protected:
	void ExpandList(bool bExpandFlag = true);
	void OnEntrySelected(size_t Index);

	/** Label */
	std::shared_ptr<FTextLabelWidget> Label;

	/** Arrow down/up sprite */
	std::shared_ptr<FSpriteWidget> ArrowSprite;

	using FDropDownListView = FListViewWidget<std::wstring, FStringViewListEntry>;

	/** List view that expands on click */
	std::shared_ptr<FDropDownListView> DropDownList;

	/** What is user selecting? */
	std::wstring SelectionText;

	/** Options to pick from */
	std::vector<std::wstring> OptionEntries;

	/** Option that is currently selected (can be none) */
	std::wstring CurrentlySelectedOption;

	/** Is the list expanded (dropped down)? */
	bool bExpanded = false;

	/** Flag to enable widgets of parent dialog on the next frame. */
	bool bEnablingWidgetsDelayed = false;

	/** Size of the list when expanded */
	Vector2 ExpandedSize;

	/** Callback called when user selects something from the list. */
	std::function<void(const std::wstring&)> OnSelectionCallback;

	/** Callback called when user expands the dropdown list. */
	std::function<void()> OnExpandedCallback;

	/** Callback called when user collapses the dropdown list. */
	std::function<void()> OnCollapsedCallback;
};