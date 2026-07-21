// Copyright © W2.Wizard 2020 All Rights Reserved.

#include "EECheatManager.h"
#include "Engine/Console.h"
#include "Logging/MessageLog.h" 
#include "Misc/MapErrors.h"
#include "Misc/UObjectToken.h"
#include "Misc/OutputDeviceNull.h"

DEFINE_LOG_CATEGORY_STATIC(EECheatManagerLog, Log, All);

#define LOCTEXT_NAMESPACE "ConsoleCheatManager"

/*===========================================================================*\
|                                 Functions                                   |
\*===========================================================================*/

void UEECheatManager::InitCheatManager()
{
    // Init event
    ReceiveInitCheatManager();
    
    // Do we have any commands?
    if (Commands.Num() >= 1)
    {
        uint16 Count = 0; // Assume that there won't be more than 65535 commands (hopefully)
        for (auto Command : Commands)
        {
            if (Command.Key.IsEmpty())
            {
                if (EnableLogging)
                {                    
                    FFormatNamedArguments Arguments;
                        
                    Arguments.Add(TEXT("ActorName"), FText::FromString( GetPathName() ));
                    Arguments.Add(TEXT("Index"), FText::AsNumber(Count));
                      
                    FMessageLog("PIE").Error()
                        ->AddToken(FUObjectToken::Create(this))
                        ->AddToken(FTextToken::Create(FText::Format(LOCTEXT( "ConsoleCheatManager", "{ActorName}: Failure retrieving command name at index: {Index}" ), Arguments )));
                }
                continue;
            }
            
            // Is command registered and set to not enabled? Unregister it. This avoids constant logging of a warning and ensure that the function is updated.
            if (IConsoleManager::Get().IsNameRegistered(*Command.Key) && !Command.Value.Enable)
                IConsoleManager::Get().UnregisterConsoleObject(IConsoleManager::Get().FindConsoleObject(*Command.Key));

            if (Command.Value.Enable)
            {                
                IConsoleManager::Get().RegisterConsoleCommand
                (
                    /*Name*/ *Command.Key,
                    /*Info*/ *Command.Value.Info,
                    /*Func*/ FConsoleCommandWithArgsDelegate::CreateUFunction(this, GET_FUNCTION_NAME_CHECKED(UEECheatManager, ExecConsoleWithArgs), Command.Value.Function)
                );
            }

            Count++;
        }
    }
    else if(EnableLogging)
    {
        FFormatNamedArguments Arguments;
        
        Arguments.Add(TEXT("ActorName"), FText::FromString(GetPathName()));
        
        FMessageLog("PIE").Warning()
            ->AddToken(FUObjectToken::Create(this))
            ->AddToken(FTextToken::Create(FText::Format(LOCTEXT("ConsoleCheatManager", "{ActorName}: No Commands present despite using UEECheatManager class"), Arguments )));
    }
}

void UEECheatManager::DeleteCommand(const FString CommandName)
{
    if (IConsoleManager::Get().IsNameRegistered(*CommandName))
    {
        IConsoleManager::Get().UnregisterConsoleObject(IConsoleManager::Get().FindConsoleObject(*CommandName));
        
        if (EnableLogging)
            UE_LOG(EECheatManagerLog, Display , TEXT("Command un-registered!"));
        return;
    }
    
    if (EnableLogging)
        UE_LOG(EECheatManagerLog, Warning , TEXT("Command couldn't be unregistered as it wasn't registered in the first place!"));
}

void UEECheatManager::ExecConsoleWithArgs(const TArray<FString>& Args, const FString Command)
{
    OnExecute.ExecuteIfBound(Args, IConsoleManager::Get().FindConsoleObject(*Command,true));
    
    FOutputDeviceNull ar;
    
    FString command = Command;
    const int32 n = Args.Num();
    
    // Do we have any arguments?
    if (n >= 1)
    {
        for (int32 i = 0; i < n; i++)
        {
            if (i == 0) command += " ";
                
            command += Args[i];
                
            if(i != n - 1) command += "|";
        }
    }

    this->CallFunctionByNameWithArguments(*command, ar, this, true);
}

/*===========================================================================*\
|                             Blueprint Functions                             |
\*===========================================================================*/

TArray<FString> UEECheatManager::ToArgs(const FString InArgs) const
{
    TArray<FString> Out;
    InArgs.ParseIntoArray(Out,TEXT("|"));
    return Out;
}

void UEECheatManager::PrintToConsole(const FText InText) const
{
    UConsole* Console = GetConsole();
    if (Console)
    {
        Console->OutputText(InText.ToString());
        return;
    }
    
    if (EnableLogging)
        UE_LOG(EECheatManagerLog, Warning , TEXT("Console is null, cannot print!"));
}

void UEECheatManager::ClearConsole() const
{
    UConsole* Console = GetConsole();
    if (Console)
    {
        Console->ClearOutput();
        return;
    }
    
    if (EnableLogging)
        UE_LOG(EECheatManagerLog, Warning , TEXT("Console is null, cannot clear!"));
}

UConsole* UEECheatManager::GetConsole() const
{
    return (GEngine->GameViewport != nullptr) ? GEngine->GameViewport->ViewportConsole : nullptr;    
}

#undef LOCTEXT_NAMESPACE
