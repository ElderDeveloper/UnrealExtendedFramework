// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "StringUtils.h"
#include "AccountHelpers.h"
#include "Input.h"
#include "Game.h"
#include "Main.h"
#include "Console.h"
#include "Friends.h"
#include "TextLabel.h"
#include "TextField.h"
#include "TextView.h"
#include "Button.h"
#include "UIEvent.h"
#include "FriendInfo.h"
#include "FriendList.h"

namespace
{
	constexpr float LabelHeight = 25.f;
	constexpr float InputFieldHeight = LabelHeight;
	constexpr float FriendInfoHeight = 1.8f * InputFieldHeight;
	constexpr float ScrollerWidth = 10.f;
}

constexpr FColor FFriendListWidget::EnabledCol;
constexpr FColor FFriendListWidget::DisabledCol;

FFriendListWidget::FFriendListWidget(Vector2 FriendListPos,
									 Vector2 FriendLisSize,
									 UILayer FriendListLayer,
									 FontPtr FriendListNormalFont,
									 FontPtr FriendListTitleFont,
									 FontPtr FriendListSmallFont,
									 FontPtr FriendListTinyFont) :
	IWidget(FriendListPos, FriendLisSize, FriendListLayer),
	NormalFont(FriendListNormalFont),
	TitleFont(FriendListTitleFont),
	SmallFont(FriendListSmallFont),
	TinyFont(FriendListTinyFont)
{
	BackgroundImage = std::make_shared<FSpriteWidget>(Vector2(0.f, 0.f), Vector2(200.f, 100.f), FriendListLayer, L"Assets/friends.dds");

	TitleLabel = std::make_shared<FTextLabelWidget>(
		Vector2(Position.x, Position.y),
		Vector2(FriendLisSize.x, 30.f),
		FriendListLayer - 1,
		L"FRIENDS",
		L"Assets/dialog_title.dds",
		FColor(1.f, 1.f, 1.f, 1.f),
		FColor(1.f, 1.f, 1.f, 1.f),
		EAlignmentType::Left);
	TitleLabel->SetBorderColor(Color::UIBorderGrey);
}

void FFriendListWidget::Create()
{
	BackgroundImage->Create();

	// Title
	TitleLabel->Create();
	TitleLabel->SetFont(TitleFont);

	// Scroller
	Vector2 scrollerPosition = Vector2(Position.x + Size.x - ScrollerWidth - 4.f, Position.y + LabelHeight + InputFieldHeight + 20.f);
	Scroller = std::make_unique<FScroller>(std::static_pointer_cast<FFriendListWidget>(shared_from_this()),
		scrollerPosition,
		Vector2(ScrollerWidth, Size.y - InputFieldHeight - LabelHeight - BottomOffset - 50.0f),
		Layer - 1,
		L"Assets/scrollbar.dds");
	if (Scroller)
	{
		Scroller->Create();
		Scroller->Hide();
	}

	// Input Field
	SearchFriendWidget = std::make_shared<FTextFieldWidget>(
		Vector2(Position.x + 5.f, Position.y + LabelHeight + 15.f),
		Vector2(Size.x - 10.f, InputFieldHeight),
		Layer - 1,
		L"Search friends...",
		L"Assets/textfield.dds",
		TitleFont,
		FTextFieldWidget::EInputType::Normal,
		EAlignmentType::Left);
	if (SearchFriendWidget)
	{
		SearchFriendWidget->Create();
		SearchFriendWidget->SetOnEnterPressedCallback([this](const std::wstring& value)
		{
			if (SearchFriendWidget->IsFocused())
			{
				this->SetFilter(value);
			}
		});
		SearchFriendWidget->SetBorderColor(Color::UIBorderGrey);
	}

	Vector2 SearchPosition = Vector2(Position.x + Size.x - 20.f, Position.y + 48.f);
	Vector2 SearchSize = Vector2(10.f, 10.f);

	SearchButtonWidget = std::make_shared<FButtonWidget>(SearchPosition, SearchSize, Layer - 1, L"", std::vector<std::wstring>({ L"Assets/search.dds" }), nullptr);
	if (SearchButtonWidget)
	{
		SearchButtonWidget->Create();
		SearchButtonWidget->SetOnPressedCallback([this]()
		{
			if (IsFocused() && this->SearchFriendWidget)
			{
				this->SetFilter(this->SearchFriendWidget->GetText());
			}
		});
	}

	CancelSearchButtonWidget = std::make_shared<FButtonWidget>(SearchPosition, SearchSize, Layer - 1, L"", std::vector<std::wstring>({ L"Assets/nobutton.dds" }), nullptr);
	if (CancelSearchButtonWidget)
	{
		CancelSearchButtonWidget->Create();
		CancelSearchButtonWidget->SetOnPressedCallback([this]()
		{
			if (IsFocused() && this->SearchFriendWidget)
			{
				ClearFilter();
				this->SearchFriendWidget->Clear();
			}
		});
		CancelSearchButtonWidget->Hide();
	}

	CreateFriends();
}

void FFriendListWidget::Release()
{
	if (BackgroundImage) BackgroundImage->Release();

	NormalFont.reset();
	TitleFont.reset();
	SmallFont.reset();
	TinyFont.reset();

	TitleLabel->Release();
	TitleLabel.reset();

	Scroller->Release();
	Scroller.reset();

	for (auto& friendInfo : FriendWidgets)
	{
		friendInfo->Release();
		friendInfo.reset();
	}

	if (SearchFriendWidget)
	{
		SearchFriendWidget->Release();
		SearchFriendWidget.reset();
	}

	if (SearchButtonWidget)
	{
		SearchButtonWidget->Release();
		SearchButtonWidget.reset();
	}

	if (CancelSearchButtonWidget)
	{
		CancelSearchButtonWidget->Release();
		CancelSearchButtonWidget.reset();
	}
}

void FFriendListWidget::Update()
{
	if (bEnabled)
	{
		if (!bFriendInfoVisible)
			return;

		if (!FGame::Get().GetFriends()->GetCurrentUser().IsValid())
		{
			return;
		}

		if (BackgroundImage) BackgroundImage->Update();

		// Do we need to refresh data?
		uint64_t NewDirtyCounter = FGame::Get().GetFriends()->GetDirtyCounter();
		if (NewDirtyCounter != FriendsDirtyCounter)
		{
			const std::vector<FFriendData>& Friends = FGame::Get().GetFriends()->GetFriends();
			RefreshFriendData(Friends);
		}

		if (SearchFriendWidget && SearchButtonWidget && CancelSearchButtonWidget)
		{
			if (bIsFilterSet)
			{
				FilteredData.resize(0);
				std::wstring NameFilterUpper = FStringUtils::ToUpper(NameFilter);
				for (size_t i = 0; i < FriendData.size(); ++i)
				{
					std::wstring NameUpper = FStringUtils::ToUpper(FriendData[i].Name);
					if (NameUpper.find(NameFilterUpper) != std::wstring::npos)
					{
						FilteredData.push_back(FriendData[i]);
					}
				}

				if (FStringUtils::ToUpper(SearchFriendWidget->GetText()) == NameFilterUpper)
				{
					//show cancel search icon
					CancelSearchButtonWidget->Show();
					SearchButtonWidget->Hide();
				}
				else
				{
					//text is different, we can perform a different search, show search icon
					CancelSearchButtonWidget->Hide();
					SearchButtonWidget->Show();
				}
			}
			else
			{
				//add placeholders for Friend section labels
				const std::pair<EOS_EFriendsStatus, EOS_Presence_EStatus> FriendStatuses[] =
				{
					{ EOS_EFriendsStatus::EOS_FS_InviteReceived, EOS_Presence_EStatus::EOS_PS_Offline },
					{ EOS_EFriendsStatus::EOS_FS_Friends, EOS_Presence_EStatus::EOS_PS_Online },
					{ EOS_EFriendsStatus::EOS_FS_Friends, EOS_Presence_EStatus::EOS_PS_Away },
					{ EOS_EFriendsStatus::EOS_FS_Friends, EOS_Presence_EStatus::EOS_PS_DoNotDisturb },
					{ EOS_EFriendsStatus::EOS_FS_Friends, EOS_Presence_EStatus::EOS_PS_ExtendedAway },
					{ EOS_EFriendsStatus::EOS_FS_Friends, EOS_Presence_EStatus::EOS_PS_Offline },
					// Don't show 'not friends' normally	{ EOS_EFriendsStatus::EOS_FS_NotFriends, EOS_Presence_EStatus::EOS_PS_Offline },
					{ EOS_EFriendsStatus::EOS_FS_InviteSent, EOS_Presence_EStatus::EOS_PS_Offline}
				};

				FilteredData.clear();
				for (const auto& FriendStatus : FriendStatuses)
				{
					//Find matches
					auto FindMatchingFriendStatusesLambda = [FriendStatus](const FFriendData& Friend)
					{
						if (Friend.Status != FriendStatus.first)
						{
							return false;
						}

						if (Friend.Status == EOS_EFriendsStatus::EOS_FS_Friends)
						{
							if (Friend.Presence.Status != FriendStatus.second)
							{
								return false;
							}
						}
						return true;
					};

					size_t MatchesCount = std::count_if(FriendData.begin(), FriendData.end(), FindMatchingFriendStatusesLambda);

					if (FriendStatus.first == EOS_EFriendsStatus::EOS_FS_InviteReceived && MatchesCount != 0)
					{
						wchar_t Buffer[20] = {};
						wsprintf(Buffer, L"%d", MatchesCount);
						FilteredData.emplace_back(L"FRIEND INVITES - " + std::wstring(Buffer));
					}
					else if (FriendStatus.first == EOS_EFriendsStatus::EOS_FS_Friends && FriendStatus.second == EOS_Presence_EStatus::EOS_PS_Online)
					{
						FilteredData.emplace_back(L"ONLINE");
					}
					else if (FriendStatus.first == EOS_EFriendsStatus::EOS_FS_Friends && FriendStatus.second == EOS_Presence_EStatus::EOS_PS_Offline && MatchesCount != 0)
					{
						wchar_t Buffer[20] = {};
						wsprintf(Buffer, L"%d", MatchesCount);
						FilteredData.emplace_back(L"OFFLINE - " + std::wstring(Buffer));
					}
					else if (FriendStatus.first == EOS_EFriendsStatus::EOS_FS_InviteSent && MatchesCount != 0)
					{
						wchar_t Buffer[20] = {};
						wsprintf(Buffer, L"%d", MatchesCount);
						FilteredData.emplace_back(L"INVITES SENT - " + std::wstring(Buffer));
					}
					else if (FriendStatus.first == EOS_EFriendsStatus::EOS_FS_NotFriends && MatchesCount != 0)
					{
						FilteredData.emplace_back(L"NOT A FRIEND");
					}

					std::copy_if(FriendData.begin(), FriendData.end(), std::back_inserter(FilteredData), FindMatchingFriendStatusesLambda);
				}

				//filter is unset, we can perform search
				CancelSearchButtonWidget->Hide();
				SearchButtonWidget->Show();
			}
		}

		if (bCanPerformSearch && FilteredData.empty())
		{
			//we need to perform search
			FGame::Get().GetFriends()->QueryUserInfo(NameFilter);
			bCanPerformSearch = false;
		}

		if (TitleLabel) TitleLabel->Update();

		if (FirstFriendToView > (FilteredData.size() - FriendWidgets.size()))
		{
			FirstFriendToView = (FilteredData.size() - FriendWidgets.size());
		}

		for (size_t i = 0; i < FriendWidgets.size(); ++i)
		{
			auto& Widget = FriendWidgets[i];
			if (Widget)
			{
				if (FirstFriendToView + i < FilteredData.size())
				{
					Widget->Show();
					Widget->SetFriendData(FilteredData[FirstFriendToView + i]);
				}
				else
				{
					Widget->SetFriendData(FFriendData());
					Widget->Hide();
				}
				Widget->Update();
			}
		}

		if (SearchFriendWidget) SearchFriendWidget->Update();

		if (Scroller)
		{
			if (FilteredData.size() <= FriendWidgets.size())
			{
				Scroller->Hide();
			}
			else
			{
				Scroller->Show();
			}
			Scroller->Update();
		}
	}
}

void FFriendListWidget::Render(FSpriteBatchPtr& Batch)
{
	if (!bShown)
		return;

	IWidget::Render(Batch);

	if (BackgroundImage) BackgroundImage->Render(Batch);

	if (TitleLabel) TitleLabel->Render(Batch);

	if (bFriendInfoVisible && FGame::Get().GetFriends()->GetCurrentUser().IsValid())
	{
		for (auto& widget : FriendWidgets)
		{
			widget->Render(Batch);
		}

		if (Scroller) Scroller->Render(Batch);

		if (SearchFriendWidget) SearchFriendWidget->SetTextColor(EnabledCol);

		if (SearchButtonWidget) SearchButtonWidget->SetBackgroundColor(EnabledCol);
		if (CancelSearchButtonWidget) CancelSearchButtonWidget->SetBackgroundColor(EnabledCol);
	}
	else
	{
		if (SearchFriendWidget) SearchFriendWidget->SetTextColor(DisabledCol);

		if (SearchButtonWidget) SearchButtonWidget->SetBackgroundColor(DisabledCol);
		if (CancelSearchButtonWidget) CancelSearchButtonWidget->SetBackgroundColor(EnabledCol);
	}

	if (SearchFriendWidget) SearchFriendWidget->Render(Batch);

	if (SearchButtonWidget) SearchButtonWidget->Render(Batch);
	if (CancelSearchButtonWidget) CancelSearchButtonWidget->Render(Batch);
}

#ifdef _DEBUG
void FFriendListWidget::DebugRender()
{
	IWidget::DebugRender();

	if (BackgroundImage) BackgroundImage->DebugRender();
	if (TitleLabel) TitleLabel->DebugRender();

	if (bFriendInfoVisible && FGame::Get().GetFriends()->GetCurrentUser().IsValid())
	{
		for (auto& widget : FriendWidgets)
		{
			widget->DebugRender();
		}

		Scroller->DebugRender();
	}

	if (SearchFriendWidget) SearchFriendWidget->DebugRender();
	if (SearchButtonWidget) SearchButtonWidget->DebugRender();
	if (CancelSearchButtonWidget) CancelSearchButtonWidget->DebugRender();
}
#endif

void FFriendListWidget::SetPosition(Vector2 Pos)
{
	Vector2 OldPos = GetPosition();

	IWidget::SetPosition(Pos);

	if (BackgroundImage) BackgroundImage->SetPosition(Vector2(Position.x, Position.y));

	// Title Label
	if (TitleLabel) TitleLabel->SetPosition(Pos);

	if (Scroller && SearchFriendWidget && TitleLabel)
	{
		// Scroller
		Vector2 ScrollerPos = Vector2(Pos.x + Size.x - Scroller->GetSize().x - 5.f,
			Pos.y + TitleLabel->GetSize().y + SearchFriendWidget->GetSize().y + 15.f);
		Scroller->SetPosition(ScrollerPos);
	}

	// Friends
	for (auto& friendInfo : FriendWidgets)
	{
		Vector2 FriendOldOffset = friendInfo->GetPosition() - OldPos;
		friendInfo->SetPosition(Pos + FriendOldOffset);
	}

	// Search Input Field
	if (SearchFriendWidget && TitleLabel)
	{
		Vector2 SearchFieldPos = Vector2(Pos.x + 5.f, Pos.y + TitleLabel->GetSize().y + (SearchFriendWidget->GetSize().y * 0.35f));
		SearchFriendWidget->SetPosition(SearchFieldPos);
	}

	Vector2 SearchPosition = Vector2(Position.x + Size.x - 20.f, Position.y + 48.f);
	if (SearchButtonWidget) SearchButtonWidget->SetPosition(SearchPosition);
	if (CancelSearchButtonWidget) CancelSearchButtonWidget->SetPosition(SearchPosition);
}

void FFriendListWidget::SetSize(Vector2 NewSize)
{
	IWidget::SetSize(NewSize);

	if (TitleLabel) TitleLabel->SetSize(Vector2(NewSize.x, 30.0f));

	if (BackgroundImage) BackgroundImage->SetSize(Vector2(NewSize.x, NewSize.y));

	if (SearchFriendWidget) SearchFriendWidget->SetSize(Vector2(NewSize.x - 8.f, 30.f));

	if (Scroller) Scroller->SetSize(Vector2(Scroller->GetSize().x, Size.y - InputFieldHeight - LabelHeight - BottomOffset - 50.0f));

	// Reset to add or remove friends
	Reset();
}

void FFriendListWidget::OnUIEvent(const FUIEvent& Event)
{
	if (!bEnabled)
		return;

	if (!bFriendInfoVisible)
		return;

	if (!FGame::Get().GetFriends()->GetCurrentUser().IsValid())
		return;

	if (Event.GetType() == EUIEventType::MousePressed || Event.GetType() == EUIEventType::MouseReleased)
	{
		for (auto& friendInfo : FriendWidgets)
		{
			if (friendInfo && friendInfo->CheckCollision(Event.GetVector()))
			{
				friendInfo->OnUIEvent(Event);
			}
		}

		if (Scroller && Scroller->CheckCollision(Event.GetVector()))
		{
			Scroller->OnUIEvent(Event);
		}

		bool bSearchButtonClicked = false;
		if (SearchButtonWidget && SearchButtonWidget->CheckCollision(Event.GetVector()))
		{
			SearchButtonWidget->OnUIEvent(Event);
			bSearchButtonClicked = true;
		}

		if (CancelSearchButtonWidget && CancelSearchButtonWidget->CheckCollision(Event.GetVector()))
		{
			CancelSearchButtonWidget->OnUIEvent(Event);
			bSearchButtonClicked = true;
		}

		if (!bSearchButtonClicked && SearchFriendWidget && SearchFriendWidget->CheckCollision(Event.GetVector()))
		{
			SearchFriendWidget->Clear();
			SearchFriendWidget->OnUIEvent(Event);
		}
	}
	else if (Event.GetType() == EUIEventType::KeyPressed ||
		Event.GetType() == EUIEventType::MouseWheelScrolled ||
		Event.GetType() == EUIEventType::TextInput)
	{
		if (SearchFriendWidget) SearchFriendWidget->OnUIEvent(Event);
		if (Scroller) Scroller->OnUIEvent(Event);
	}
	else if (Event.GetType() == EUIEventType::FriendInviteSent)
	{
		ClearFilter();
		if (SearchFriendWidget) SearchFriendWidget->Clear();
	}
	else
	{
		for (auto& friendInfo : FriendWidgets)
		{
			friendInfo->OnUIEvent(Event);
		}
	}
}

void FFriendListWidget::SetFocused(bool bValue)
{
	IWidget::SetFocused(bValue);

	if (!bValue)
	{
		if (TitleLabel) TitleLabel->SetFocused(false);
		if (SearchFriendWidget) SearchFriendWidget->SetFocused(false);
		for (auto NextFriendWidget : FriendWidgets)
		{
			if (NextFriendWidget)
			{
				NextFriendWidget->SetFocused(false);
			}
		}
	}
}

void FFriendListWidget::ScrollUp(size_t Length)
{
	if (FirstFriendToView < Length)
	{
		FirstFriendToView = 0;
	}
	else
	{
		FirstFriendToView -= Length;
	}
}

void FFriendListWidget::ScrollDown(size_t Length)
{
	FirstFriendToView += Length;
	if (FriendWidgets.size() > FilteredData.size())
	{
		FirstFriendToView = 0;
		return;
	}

	if (FirstFriendToView > (FilteredData.size() - FriendWidgets.size()))
	{
		FirstFriendToView = (FilteredData.size() - FriendWidgets.size());
	}
}

void FFriendListWidget::ScrollToTop()
{
	ScrollUp(FilteredData.size());
}

void FFriendListWidget::ScrollToBottom()
{
	ScrollDown(FilteredData.size());
}

size_t FFriendListWidget::NumEntries() const
{
	return FilteredData.size();
}


size_t FFriendListWidget::GetNumLinesPerPage() const
{
	return FriendWidgets.size();
}

size_t FFriendListWidget::FirstViewedEntry() const
{
	return FirstFriendToView;
}

size_t FFriendListWidget::LastViewedEntry() const
{
	return FirstFriendToView + FriendWidgets.size() - 1;
}

void FFriendListWidget::RefreshFriendData(const std::vector<FFriendData>& friends)
{
	FriendData = friends;

	if (friends.empty())
	{
		for (auto& NextWidget : FriendWidgets)
		{
			NextWidget->SetFriendData(FFriendData());
		}
	}

	if (FGame::Get().GetFriends())
	{
		FriendsDirtyCounter = FGame::Get().GetFriends()->GetDirtyCounter();
	}
}

void FFriendListWidget::Clear()
{
	for (auto& FriendInfo : FriendWidgets)
	{
		if (FriendInfo)
		{
			FriendInfo->Release();
			FriendInfo.reset();
		}
	}

	FriendWidgets.clear();
}

void FFriendListWidget::Reset()
{
	ClearFilter();
	Clear();
	CreateFriends();
}

void FFriendListWidget::CreateFriends()
{
	// Friends
	const size_t NumFriendsOnScreen = size_t((Size.y - LabelHeight - InputFieldHeight - BottomOffset) / FriendInfoHeight);

	FriendWidgets.resize(NumFriendsOnScreen);
	for (size_t i = 0; i < NumFriendsOnScreen; ++i)
	{
		FriendWidgets[i] = std::make_shared<FFriendInfoWidget>(
			Vector2(Position.x, Position.y + LabelHeight + InputFieldHeight + (FriendInfoHeight * i) + 20.f),
			Vector2(Size.x - ScrollerWidth - 2.f, FriendInfoHeight),
			Layer - 1,
			FFriendData(),
			SmallFont,
			TinyFont);

		if (FriendWidgets[i])
		{
			FriendWidgets[i]->Create();
			FriendWidgets[i]->Hide();

			if (Scroller)
			{
				Vector2 FriendSize = Vector2(Size.x - Scroller->GetSize().x - 5.f, FriendWidgets[i]->GetSize().y);
				FriendWidgets[i]->SetSize(FriendSize);
			}
		}
	}
}
