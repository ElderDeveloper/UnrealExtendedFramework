// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Input.h"
#include "AccountHelpers.h"
#include <eos_sdk.h>

struct FPresenceInfo;

enum class EUIEventType : unsigned char
{
	None,
	MousePressed,
	MouseReleased,
	MouseWheelScrolled,
	KeyPressed,
	KeyReleased,
	TextInput,
	CopyText,
	SelectAll,
	PasteText,
	FullscreenToggle,
	SearchText,
	FocusGained,
	FocusLost,
	FriendInviteSent,
	InviteToSession,
	Last = FriendInviteSent
};

// Generic UI event
class FUIEvent
{
public:
	FUIEvent() = default;
	explicit FUIEvent(EUIEventType type, Vector2 vector = Vector2()) :
		Type(type), Vector(vector)
	{}

	explicit FUIEvent(EUIEventType type, FInput::Keys inputKey, bool bKeyRepeat = false) :
		Type(type), InputKey(inputKey), bRepeat(bKeyRepeat)
	{}

#ifdef EOS_DEMO_SDL
	explicit FUIEvent(const char* Text) :
		Type(EUIEventType::TextInput), InputText(Text)
	{}
#endif

	explicit FUIEvent(EUIEventType type, FInput::InputCommands inputCmd) :
		Type(type), InputCmd(inputCmd)
	{}

	explicit FUIEvent(EUIEventType type, FEpicAccountId userId, FPresenceInfo* presenceInfo) :
		Type(type), UserId(userId), PresenceInfo(presenceInfo)
	{}

	EUIEventType GetType() const { return Type; }
	Vector2 GetVector() const { return Vector; }
	FInput::Keys GetKey() const { return InputKey; }
	FInput::InputCommands GetCommand() const { return InputCmd; }

#ifdef EOS_DEMO_SDL
	const std::string& GetInputText() const { return InputText; }
#endif

	/**
	* Accessor for user id
	*/
	FEpicAccountId GetUserId() const { return UserId; }

	/**
	* Accessor for user id
	*/
	FPresenceInfo* GetPresenceInfo() const { return PresenceInfo; }

private:
	EUIEventType Type = EUIEventType::None;

	// optional parameter for some events. Coordinate of event in world space.
	Vector2 Vector = Vector2();

	// optional parameter for some events. Input key.
	FInput::Keys InputKey = FInput::Keys::None;

	// optional parameter. Is the key repeated?
	bool bRepeat = false;

#ifdef EOS_DEMO_SDL
	std::string InputText;
#endif

	// optional parameter for some events. Input command.
	FInput::InputCommands InputCmd = FInput::InputCommands::None;

	/** User Id */
	FEpicAccountId UserId;

	/** Presence Info */
	FPresenceInfo* PresenceInfo;
};