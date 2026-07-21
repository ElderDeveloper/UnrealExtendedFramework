// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

/**
* Manages HTTP requests. Works asynchronously and supports multiple requests at a time.
*/
class FHTTPClient
{
	/**
	* Constructor
	*/
	FHTTPClient();

	/**
	* No copying or copy assignment allowed for this class.
	*/
	FHTTPClient(FHTTPClient const&) = delete;
	FHTTPClient& operator=(FHTTPClient const&) = delete;

public:
	/**
	* Destructor
	*/
	virtual ~FHTTPClient();

	using HTTPErrorCode = uint32_t;
	using OnHTTPRequestFinishCallback = std::function<void(HTTPErrorCode, const std::vector<char>&)>;

	/**
	 * Method used for Http requests
	 */
	enum class EHttpRequestMethod : uint8_t
	{
		GET = 0,
		POST,
		PUT,
		DEL
	};

	static FHTTPClient& GetInstance();
	static void ClearInstance();

	void Update();

	/**
	 * Performs HTTP request and saves response to memory.
	 */
	void PerformHTTPRequest(const std::string& URL, const EHttpRequestMethod Method, const std::string& Body, OnHTTPRequestFinishCallback&& Callback);

private:
	static std::unique_ptr<FHTTPClient> Instance;

	//Private implementation:
	class FImpl;
	std::unique_ptr<FImpl> Impl;
};