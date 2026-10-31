# Unreal Extended Foundation

The shared base every Extended plugin builds on. It depends on no other plugin, only on engine modules
every install has, so even the standalone plugins (Steam, Quest, PerfSentinel, Atlassian) can use it.

## Modules

| Module | What it holds |
|:--|:--|
| `ExtendedFoundation` | HTTP, JSON, results, async nodes, settings base, credentials, status, test helpers |
| `UnrealExtendedFrameworkLog` | `EF_LOG` category log files, the Extended Log settings and viewer feed |

Add `ExtendedFoundation` (and `UnrealExtendedFrameworkLog` if you log with `EF_LOG`) to your module's
dependencies and `UnrealExtendedFoundation` to your plugin's `.uplugin`.

## At a glance

```cpp
// HTTP with retries, against a base URL set once per game instance.
UEFHttpSubsystem* Http = EF::GetGameInstanceSubsystem<UEFHttpSubsystem>(this);
FEFHttpRequest Request = UEFHttpLibrary::MakeHttpRequest(TEXT("leaderboards/top"));
Request.MaxRetries = 2;
Http->Send(Request, [](const FEFHttpResponse& Response)
{
	const FEFResult Result = FEFResult::FromHttp(Response, TEXT("MyService"));
	FEFJsonObject Body;
	if (Result.IsSuccess() && UEFHttpLibrary::GetResponseJson(Response, Body)) { /* ... */ }
});

// The same code, offline, in an automation test.
FEFScopedHttpMock Mock;
Mock->AddResponse(EEFHttpVerb::Get, TEXT("leaderboards/top"), 200, TEXT("{\"entries\":[]}"));

// A developer's token, never written to the project.
FEFCredentialStore::Save(TEXT("MyPlugin"), Account, Token);
```

| Header | Use it for |
|:--|:--|
| `Http/EFHttpClient.h` | `FEFHttpOperation`: one request with retries, timeouts, cancellation; transport override |
| `Http/EFHttpSubsystem.h` | Base URL, default headers, cancellable handles per game instance |
| `Http/EFAsyncHttpRequest.h` | The **Http Request** Blueprint node |
| `Http/EFHttpMock.h` | `FEFScopedHttpMock`: canned answers and request recording |
| `Json/EFJsonLibrary.h` | JSON read/write, path lookup, struct conversion, files |
| `Result/EFResult.h` | `FEFResult`, `EFResultCodes` |
| `Async/EFAsyncActionBase.h` | Base class for async Blueprint nodes |
| `Async/EFPendingCalls.h` | Per-call completion callbacks in a subsystem |
| `Async/EFCallContext.h` | Telling results apart on delegates every caller shares |
| `Settings/EFDeveloperSettings.h` | Base for Project Settings pages; `EFSecret` warnings |
| `Credentials/EFCredentialStore.h` | Per-user secrets (DPAPI / Keychain) |
| `Subsystems/EFSubsystems.h` | Subsystem lookup from any world context |
| `Status/EFServiceStatus.h` | Reporting to the `Extended.Status` console command |
| `Testing/EFTestHelpers.h` | Test game instance, wait-until latent command, fixtures |
| `EFFoundationVersion.h` | `EF_REQUIRE_FOUNDATION_VERSION` compile-time check |

The rules for using these across the suite are in [ARCHITECTURE.md](../ARCHITECTURE.md).

## Tests

`Automation RunTests ExtendedFoundation` (all offline).
