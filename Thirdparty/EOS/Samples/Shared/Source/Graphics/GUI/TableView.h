// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widget.h"
#include "Font.h"
#include "ListView.h"
#include "Dialog.h"

class FTextLabelWidget;

/**
 * Table view widget to show rows of data.
 */
template <typename FTableRowDataType, typename FTableRowViewType>
class FTableView : public FDialog
{
public:

	using TableRowDataType = FTableRowDataType;
	using TableRowViewType = FTableRowViewType;

	/**
	 * Constructor
	 */
	FTableView(Vector2 TablePosition,
		Vector2 TableSize,
		UILayer Layer,
		const std::wstring& LabelText,
		float ScrollerWidth,
		const std::vector<FTableRowDataType>& Data,
		const FTableRowDataType& LabelRow,
		float TableViewEntryHeight = 25.0f);

	/**
	* Destructor
	*/
	virtual ~FTableView() {};

	/** IWidget */
	virtual void OnUIEvent(const FUIEvent& Event) override;
	virtual void SetPosition(Vector2 Pos) override;
	virtual void SetSize(Vector2 NewSize) override;
	virtual void Render(FSpriteBatchPtr& Batch) override;

	/** Callback that is called when user selects something from the list. */
	void SetOnSelectionCallback(std::function<void(const std::wstring& Element1, const std::string& Element2)> Callback);

	void SetFonts(FontPtr TitleFont, FontPtr NormalFont);
	
	void SetLabelText(const std::wstring& LabelText);
	std::wstring GetLabelText();

	void RefreshData(std::vector<FTableRowDataType>&& Data);
	void RefreshLabels(FTableRowDataType&& Labels);
	void Clear();
	void ScrollToTop();

protected:
	void OnEntrySelected(size_t Index);

	/** Label */
	std::shared_ptr<FTextLabelWidget> Label;

	/** Table data (rows) */
	std::vector<FTableRowDataType> Data;

	/** Column labels (top row) */
	std::shared_ptr<FTableRowViewType> ColumnLabels;

	using FTableListView = FListViewWidget<FTableRowDataType, FTableRowViewType>;

	/** List of rows */
	std::shared_ptr<FTableListView> TableList;

	/** Option that is currently selected (can be none) */
	std::wstring CurrentlySelectedOption;

	/** Callback called when user selects something from the list. */
	std::function<void(const std::wstring&, const std::string&)> OnSelectionCallback;

	float ScrollerWidth = 0.f;

	/** Table entry height */
	float TableViewEntryHeight;
};

#include "TableView.inl"

#ifdef EOS_SAMPLE_SESSIONS
#include "SessionsTableRowView.h"
using FSessionsTableView = FTableView<FSessionsTableRowData, FSessionsTableRowView>;
#elif defined(EOS_SAMPLE_LOBBIES)
#include "LobbyMemberTableRowView.h"
#include "LobbySearchResultTableRowView.h"
using FLobbyMemberTableView = FTableView<FLobbyMemberTableRowData, FLobbyMemberTableRowView>;
using FLobbySearchResultTableView = FTableView<FLobbySearchResultTableRowData, FLobbySearchResultTableRowView>;
#elif EOS_SAMPLE_MODS
#include "ModsTableRowView.h"
using FModsTableView = FTableView<FModsTableRowData, FModsTableRowView>;
#elif defined(EOS_SAMPLE_VOICE)
#include "VoiceRoomMemberTableRowView.h"
using FVoiceRoomMemberTableView = FTableView<FVoiceRoomMemberTableRowData, FVoiceRoomMemberTableRowView>;
#elif defined(EOS_SAMPLE_WORLDINVENTORY)
#include "InventoryTableRowView.h"
using FInventoryTableView = FTableView<FInventoryTableRowData, FInventoryTableRowView>;
#else
#include "TableRowView.h"
using FTableViewWidget = FTableView<FTableRowData, FTableRowView>;
#endif