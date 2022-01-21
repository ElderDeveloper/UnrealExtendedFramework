// Copyright © W2.Wizard 2020 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "Engine/GameViewportClient.h"
#include "HAL/ConsoleManager.h"
#include "EECheatManager.generated.h"

// Delegate: Executes when a comand gets executed, passes Arguments and name of the command
DECLARE_DELEGATE_TwoParams(OnCommandExecute, const TArray<FString>&, IConsoleObject*)

USTRUCT(BlueprintType)
struct FExtendedCommandData
{
	GENERATED_BODY()

public:

	/** Should this command be useable? */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=EECheatManager)
	bool Enable = true;
	
	/**
	* The name of the function thats supposed to be executed.
	* 
	* @note Make sure its a unique name!
	*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=EECheatManager,  meta=(EditCondition="Enable"))
	FString Function;

	/**
	* Provides additional information about the command, like what it does and what each argument is.
	*
	* The first line will be shown the console, any additional multiline will be visible by writing the command and adding
	* the keyword '?' after it, this will show all the additional lines.
	*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=EECheatManager,  meta=(EditCondition="Enable", MultiLine="true"))
	FString Info;
	
	FExtendedCommandData() { }

	// Constructor using TPair
	FExtendedCommandData(TPair<FString, FString> NameAndInfo)
	{
		Function = NameAndInfo.Key;
		Info = NameAndInfo.Value;
	}

	// Constructor using separate Strings for name and info
	FExtendedCommandData(FString InName, FString InInfo)
	{
		Function = InName;
		Info = InInfo;
	}
};

/**
* The command manager allows developers to create completely new commands from scratch and handle their execution
* via blueprints using Custom Events or Functions.
* 
* @note Is not included in shipping builds due to inherting from UCheatManager, however is still available in development
* builds.
*/
UCLASS(Blueprintable)
class UNREALEXTENDEDEDITOR_API UEECheatManager : public UCheatManager
{
	GENERATED_BODY()

public:

    
	/**
	* Delegate to subscribe in order to handle console execution
	* @note Param 1: Arguments of the command.
	* @note Param 2: The name of the command.
	*/
	OnCommandExecute OnExecute;

	// Should any errors or warnings be logged?
	UPROPERTY(BlueprintReadWrite,EditAnywhere, Category=EECheatManager)
	bool EnableLogging = true;
    
	/**
	* Contains all the commands present and that are supposed to be registered.
	*
	* @note If you want to delete a command use DeleteCommand in the console, to fully unregister it!
	* !!! Won't update the console itself, meaning that the command will still show up until a engine restart has occured. !!!
	*/
	UPROPERTY(BlueprintReadWrite,EditAnywhere, Category=EECheatManager)
	TMap<FString, FExtendedCommandData> Commands;
	
protected:



	// Begin Play
	virtual void InitCheatManager() override;

	// To clear left over commands, restarting the engine removes them all eitherway, this is just incase.
	UFUNCTION(Exec)
    virtual void DeleteCommand(const FString CommandName);
    
	/**
	* Executes the named function inside the Virtual machine and passes arguments as a single string as other types are not supported...
	* Is not meant to be called within blueprints, its just to provide a UFUNCTION to be binded to so that the cheat manager can manage each console function
	*/
	UFUNCTION()
    virtual void ExecConsoleWithArgs(const TArray<FString>& Args, const FString Command);

public:
	
    
	/** Formats the argument string into an Array for easy processing. */
	UFUNCTION(BlueprintPure, Category = "EECheatManager", meta=(DisplayName="To Args", CompactNodeTitle="ARGS", Keywords = "to args"))
    TArray<FString> ToArgs(const FString InArgs) const;

	/**
	* Prints a (potentially multi-line) FText to the console.
	* @param InText Text to display on the console.
	*/
	UFUNCTION(BlueprintCallable, Category = "EECheatManager", meta=(DisplayName="Print to Console", Keywords = "print to console"))
    void PrintToConsole(const FText InText) const;

	/** Clears the console buffer */
	UFUNCTION(BlueprintCallable, Category = "EECheatManager", meta=(DisplayName="Clear Console", Keywords = "clear console"))
    void ClearConsole() const;	

private:

	// Gets the console itself, validating its existence.
	UConsole* GetConsole() const;
};
