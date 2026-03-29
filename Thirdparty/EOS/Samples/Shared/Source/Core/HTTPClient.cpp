// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "DebugLog.h"
#include "StringUtils.h"
#include "HTTPClient.h"

#ifdef EOS_LIBCURL_ENABLED
#include <curl/curl.h>

struct FCURLHTTPRequestData
{
	CURL* CurlHandle = nullptr;
	std::string URL;
	std::vector<char> ErrorBuffer; //CURL_ERROR_SIZE
	std::vector<char> ResponseData;

	FHTTPClient::OnHTTPRequestFinishCallback FinishCallback;

	FCURLHTTPRequestData()
	{
		ErrorBuffer.resize(CURL_ERROR_SIZE);
		ErrorBuffer[0] = '\0';
	}

	~FCURLHTTPRequestData()
	{
		if (CurlHandle)
		{
			curl_easy_cleanup(CurlHandle);
		}
	}

	bool operator==(const FCURLHTTPRequestData& Other) const
	{
		return CurlHandle == Other.CurlHandle && URL == Other.URL;
	}
};

//Curl Implementation
class FHTTPClient::FImpl
{
public:
	FImpl()
	{
		curl_global_init(CURL_GLOBAL_ALL);
		CurlMHandle = curl_multi_init();
	}

	~FImpl()
	{
		if (CurlMHandle)
		{
			curl_multi_cleanup(CurlMHandle);
			CurlMHandle = nullptr;
		}

		//clean all active requests
		ActiveRequests.clear();

		curl_global_cleanup();
	}

	void Update();
	void PerformHTTPRequest(const std::string& URL, const EHttpRequestMethod Method, const std::string& Body, OnHTTPRequestFinishCallback&& Callback);
	static size_t WriteHTTPResponseDataCallback(char *Data, size_t DataSize, size_t NumDataItems, void *UserData);

	CURLM* CurlMHandle = nullptr;

	std::list<FCURLHTTPRequestData> ActiveRequests;
};

void FHTTPClient::FImpl::PerformHTTPRequest(const std::string& URL, const EHttpRequestMethod Method, const std::string& Body, OnHTTPRequestFinishCallback&& Callback)
{
	CURL *CurlHandle = nullptr;

	/* init the curl session */
	CurlHandle = curl_easy_init();

	struct curl_slist *Headers = NULL;
	Headers = curl_slist_append(Headers, "Accept: */*");
	Headers = curl_slist_append(Headers, "Content-Type: text/plain");
	curl_easy_setopt(CurlHandle, CURLOPT_HTTPHEADER, Headers);

	/* set URL to get here */
	curl_easy_setopt(CurlHandle, CURLOPT_URL, URL.c_str());

	if (Method == EHttpRequestMethod::POST)
	{
		curl_easy_setopt(CurlHandle, CURLOPT_POST, 1L);
	}
	else if (Method == EHttpRequestMethod::PUT)
	{
		curl_easy_setopt(CurlHandle, CURLOPT_CUSTOMREQUEST, 1L);
	}
	else if (Method == EHttpRequestMethod::DEL)
	{
		curl_easy_setopt(CurlHandle, CURLOPT_CUSTOMREQUEST, "DELETE");
	}

	if (!Body.empty())
	{
		curl_easy_setopt(CurlHandle, CURLOPT_POSTFIELDS, Body.c_str());
	}

	/* Switch on full protocol/debug output while testing */
	curl_easy_setopt(CurlHandle, CURLOPT_VERBOSE, 1L);

	/* disable progress meter, set to 0L to enable it */
	curl_easy_setopt(CurlHandle, CURLOPT_NOPROGRESS, 1L);

	/* send all data to this function  */
	curl_easy_setopt(CurlHandle, CURLOPT_WRITEFUNCTION, WriteHTTPResponseDataCallback);

	ActiveRequests.push_back(FCURLHTTPRequestData());
	FCURLHTTPRequestData& RequestData = ActiveRequests.back();

	RequestData.CurlHandle = CurlHandle;
	RequestData.URL = URL;
	RequestData.FinishCallback = std::move(Callback);

	curl_easy_setopt(CurlHandle, CURLOPT_WRITEDATA, &RequestData);
	curl_easy_setopt(CurlHandle, CURLOPT_ERRORBUFFER, RequestData.ErrorBuffer.data());

	curl_multi_add_handle(CurlMHandle, CurlHandle);
}

size_t FHTTPClient::FImpl::WriteHTTPResponseDataCallback(char *Data, size_t DataSize, size_t NumDataItems, void *UserData)
{
	FHTTPClient& HttpClient = FHTTPClient::GetInstance();

	FCURLHTTPRequestData* RequestData = static_cast<FCURLHTTPRequestData*>(UserData);
	if (RequestData)
	{
		const size_t PrevSize = RequestData->ResponseData.size();
		const size_t SizeToAdd = DataSize * NumDataItems;
		RequestData->ResponseData.resize(PrevSize + SizeToAdd);
		for (size_t Index = 0; Index < SizeToAdd; ++Index)
		{
			RequestData->ResponseData[PrevSize + Index] = Data[Index];
		}
	}
	return DataSize * NumDataItems;
}

void FHTTPClient::FImpl::Update()
{
	if (!ActiveRequests.empty())
	{
		int RunningRequests = -1;
		{
			curl_multi_perform(CurlMHandle, &RunningRequests);
		}

		if (RunningRequests == 0 || RunningRequests != ActiveRequests.size())
		{
			for (;;)
			{
				int MsgsStillInQueue = 0;
				CURLMsg * Message = curl_multi_info_read(CurlMHandle, &MsgsStillInQueue);

				if (Message == NULL)
				{
					break;
				}

				// Find out which requests have completed
				if (Message->msg == CURLMSG_DONE)
				{
					CURL* CompletedHandle = Message->easy_handle;
					curl_multi_remove_handle(CurlMHandle, CompletedHandle);

					// Find request data to call the callback and remove the request.
					for (FCURLHTTPRequestData& NextRequest : ActiveRequests)
					{
						if (NextRequest.CurlHandle == CompletedHandle)
						{
							if (Message->data.result == CURLE_OK)
							{
								NextRequest.FinishCallback(200, NextRequest.ResponseData);
							}
							else
							{
								// Error case
								// Shrink error buffer (there can be extra zeros at the end; there can be no zeros as well)
								for (size_t ErrorBufferIndex = 0; ErrorBufferIndex < NextRequest.ErrorBuffer.size(); ++ErrorBufferIndex)
								{
									if (NextRequest.ErrorBuffer[ErrorBufferIndex] == '\0')
									{
										NextRequest.ErrorBuffer.resize(ErrorBufferIndex);
										break;
									}
								}
								NextRequest.FinishCallback(HTTPErrorCode(0), NextRequest.ErrorBuffer);
							}
							ActiveRequests.remove(NextRequest);
							break;
						}
					}
				}
			}
		}
	}
}

#endif //EOS_LIBCURL_ENABLED

std::unique_ptr<FHTTPClient> FHTTPClient::Instance;

FHTTPClient::FHTTPClient():
	Impl(new FImpl())
{

}

FHTTPClient::~FHTTPClient()
{

}

FHTTPClient& FHTTPClient::GetInstance()
{
	if (!Instance)
	{
		Instance = std::unique_ptr<FHTTPClient>(new FHTTPClient());
	}

	return *Instance;
}

void FHTTPClient::ClearInstance()
{
	Instance.reset();
}

void FHTTPClient::Update()
{
#ifdef EOS_LIBCURL_ENABLED
	Impl->Update();
#endif
}

void FHTTPClient::PerformHTTPRequest(const std::string& URL, const EHttpRequestMethod Method, const std::string& Body, OnHTTPRequestFinishCallback&& Callback)
{
#ifdef EOS_LIBCURL_ENABLED
	Impl->PerformHTTPRequest(URL, Method, Body, std::move(Callback));
#else
	Callback(HTTPErrorCode(0), "Error HTTP client not implemented!");
#endif
}