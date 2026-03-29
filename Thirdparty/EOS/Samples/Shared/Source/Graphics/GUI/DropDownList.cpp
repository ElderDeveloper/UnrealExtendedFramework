// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "DebugLog.h"
#include "StringUtils.h"
#include "Main.h"
#include "Game.h"
#include "TextLabel.h"
#include "DropDownList.h"

FDropDownList::FDropDownList(Vector2 DropDownPosition,
	Vector2 DropDownInitialSize,
	Vector2 DropDownExpandedSize,
	UILayer Layer,
	const std::wstring& InitialText,
	const std::vector<std::wstring>& OptionsList,
	FontPtr Font,
	EAlignmentType AlignmentType,
	FColor BackCol,
	FColor TextCol) :
	FDialog(DropDownPosition, DropDownInitialSize, Layer),
	SelectionText(InitialText),
	OptionEntries(OptionsList),
	CurrentlySelectedOption(OptionsList[0]),
	ExpandedSize(DropDownExpandedSize)
{
	const Vector2 DownArrowSize = Vector2(DropDownInitialSize.y, DropDownInitialSize.y);

	Label = std::make_shared<FTextLabelWidget>(
		DropDownPosition,
		DropDownInitialSize - Vector2(DownArrowSize.x, 0.f),
		Layer,
		InitialText,
		L"",
		BackCol,
		TextCol,
		AlignmentType);
	Label->SetFont(Font);

	ArrowSprite = std::make_shared<FSpriteWidget>(
		DropDownPosition + Vector2(DropDownInitialSize.x - DownArrowSize.x, 0.0f),
		DownArrowSize,
		Layer,
		std::vector<std::wstring>({ L"Assets/dropdown_arrow.dds", L"Assets/dropdown_arrow.dds" })
		);
	
	DropDownList = std::make_shared<FDropDownListView>(
		DropDownPosition + Vector2(0.0f, Label->GetSize().y),
		DropDownExpandedSize - Vector2(0.0f, Label->GetSize().y),
		Layer - 1,
		20.0f, //entry height
		0.0f, //label height (no label)
		15.0f, //scroller width
		L"", //background
		L"", //Title text
		Font,
		Font,
		Font,
		Font
		);

	DropDownList->RefreshData(OptionEntries);
	DropDownList->SetOnEntrySelectedCallback([this](size_t Index) { this->OnEntrySelected(Index); });
	DropDownList->Hide();
	bExpanded = false;

	AddWidget(Label);
	AddWidget(ArrowSprite);
	AddWidget(DropDownList);
}

void FDropDownList::OnUIEvent(const FUIEvent& Event)
{
	if (!bEnabled)
		return;

	//Expand/unexpand when the label is clicked (or user presses enter while it's in focus)
	if ((Event.GetType() == EUIEventType::MousePressed && (Label->CheckCollision(Event.GetVector()) || ArrowSprite->CheckCollision(Event.GetVector()))) ||
		((IsWidgetFocused(DropDownList) || IsWidgetFocused(Label)) && Event.GetType() == EUIEventType::KeyPressed && Event.GetKey() == FInput::Enter))
	{
		ExpandList(!bExpanded);
	}
	else if (bExpanded)
	{
		DropDownList->OnUIEvent(Event);
	}
}

void FDropDownList::SetPosition(Vector2 Pos)
{
	FDialog::SetPosition(Pos);

	const Vector2 DownArrowSize = Vector2(Size.y, Size.y);

	Label->SetPosition(Pos);
	ArrowSprite->SetPosition(Pos + Vector2(Size.x - DownArrowSize.x, 0.0f));
	DropDownList->SetPosition(Pos + Vector2(0.0f, Label->GetSize().y));
}

void FDropDownList::SetSize(Vector2 NewSize)
{
	FDialog::SetSize(NewSize);

	const Vector2 DownArrowSize = Vector2(NewSize.y, NewSize.y);

	Label->SetSize(NewSize - Vector2(DownArrowSize.x, 0.f));
	ArrowSprite->SetSize(DownArrowSize);
	DropDownList->SetSize(ExpandedSize - Vector2(0.0f, Label->GetSize().y));
}

void FDropDownList::SetOnSelectionCallback(std::function<void(const std::wstring&)> Callback)
{
	OnSelectionCallback = Callback;
}

void FDropDownList::SetOnExpandedCallback(std::function<void()> Callback)
{
	OnExpandedCallback = Callback;
}

void FDropDownList::SetOnCollapsedCallback(std::function<void()> Callback)
{
	OnCollapsedCallback = Callback;
}

void FDropDownList::UpdateOptionsList(const std::vector<std::wstring>& OptionsList, int SelectIndex)
{
	if (DropDownList)
	{
		DropDownList->RefreshData(OptionsList);
	}

	OptionEntries = OptionsList;

	if (!OptionsList.empty())
	{
		OnEntrySelected(SelectIndex);
	}
}

void FDropDownList::SelectEntry(size_t Index)
{
	OnEntrySelected(Index);
}

void FDropDownList::SetSelectedEntry(size_t Index)
{
	if (OptionEntries.size() > Index)
	{
		CurrentlySelectedOption = OptionEntries[Index];
		Label->SetText(SelectionText + CurrentlySelectedOption);
	}
}

void FDropDownList::ExpandList(bool bExpandFlag /*= true*/)
{
	if (bExpandFlag == bExpanded)
		return;

	bExpanded = bExpandFlag;

	if (bExpanded)
	{
		DropDownList->Show();
		FDialog::SetSize(ExpandedSize);
		ArrowSprite->GetAnimatedTexture()->SetFrame(1);

		if (auto ParentDialog = Parent.lock())
		{
			ParentDialog->DisableOtherWidgetsAndSave(std::static_pointer_cast<IWidget>(shared_from_this()));
		}

		if (OnExpandedCallback)
		{
			OnExpandedCallback();
		}
	}
	else
	{
		DropDownList->Hide();
		FDialog::SetSize(Vector2(Label->GetSize().x + ArrowSprite->GetSize().x, Label->GetSize().y));
		ArrowSprite->GetAnimatedTexture()->SetFrame(0);

		bEnablingWidgetsDelayed = true;

		if (OnCollapsedCallback)
		{
			OnCollapsedCallback();
		}
	}
}

void FDropDownList::OnEntrySelected(size_t Index)
{
	if (OptionEntries.size() > Index)
	{
		CurrentlySelectedOption = OptionEntries[Index];
		Label->SetText(SelectionText + CurrentlySelectedOption);
		if (OnSelectionCallback)
		{
			OnSelectionCallback(CurrentlySelectedOption);
		}

		ExpandList(false);
	}
}

void FDropDownList::SetFocused(bool bValue)
{
	FDialog::SetFocused(bValue);

	//Hide when we lose focus
	if (!bValue)
	{
		ExpandList(false);
	}
}

void FDropDownList::Render(FSpriteBatchPtr& Batch)
{
	FDialog::Render(Batch);
}

void FDropDownList::Update()
{
	FDialog::Update();

	if (bEnablingWidgetsDelayed)
	{
		bEnablingWidgetsDelayed = false;
		if (auto ParentDialog = Parent.lock())
		{
			ParentDialog->ReEnableWidgets();
		}
	}
}
