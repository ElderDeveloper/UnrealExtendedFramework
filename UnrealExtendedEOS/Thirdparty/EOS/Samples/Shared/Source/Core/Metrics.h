// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "eos_sdk.h"
#include "eos_metrics.h"

/**
* Manages all player session metrics
*/
class FMetrics
{
public:
	/**
	* Constructor
	*/
	FMetrics() noexcept(false);

	/**
	* No copying or copy assignment allowed for this class.
	*/
	FMetrics(FMetrics const&) = delete;
	FMetrics& operator=(FMetrics const&) = delete;

	/**
	* Destructor
	*/
	virtual ~FMetrics();

	/**
	* Initialization
	*/
	void Init();

	/**
	* Logs metrics for a player session beginning
	*
	* @param UserId - User id for player beginning session
	*/
	void BeginPlayerSession(FEpicAccountId UserId);

	/**
	* Logs metrics for a player session ending
	*
	* @param UserId - User id for player ending session
	*/
	void EndPlayerSession(FEpicAccountId UserId);

	/**
	* Receives game event
	*
	* @param Event - Game event to act on
	*/
	void OnGameEvent(const FGameEvent& Event);

private:
	/** Handle to EOS SDK metrics system */
	EOS_HMetrics MetricsHandle;		
};