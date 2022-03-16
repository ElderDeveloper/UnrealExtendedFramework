// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "XmlFile.h"
#include "JsonObjectConverter.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UnrealExtendedFramework/Data/EFEnums.h"
#include "EFFileLibrary.generated.h"


USTRUCT(BlueprintType)
struct FCustomNodeStat
{
	GENERATED_BODY();

public:
	UPROPERTY(BlueprintReadOnly, Category = "FileSystem")
	bool IsDirectory;
	UPROPERTY(BlueprintReadOnly, Category = "FileSystem")
	bool IsReadOnly;
	UPROPERTY(BlueprintReadOnly, Category = "FileSystem")
	FDateTime LastAccessTime;
	UPROPERTY(BlueprintReadOnly, Category = "FileSystem")
	FDateTime CreationTime;
	UPROPERTY(BlueprintReadOnly, Category = "FileSystem")
	FDateTime ModificationTime;
	UPROPERTY(BlueprintReadOnly, Category = "FileSystem")
	int64 FileSize;
	FCustomNodeStat() : IsDirectory(false), IsReadOnly(false), LastAccessTime(FDateTime::MinValue()), CreationTime(FDateTime::MinValue()), ModificationTime(FDateTime::MinValue()), FileSize(0) {}
};

USTRUCT(BlueprintType)
struct FProjectPath
{
	GENERATED_BODY();
public:
	UPROPERTY(BlueprintReadOnly, Category = "Path")
	FString Directory;
	UPROPERTY(BlueprintReadOnly, Category = "Path")
	FString Config;
	UPROPERTY(BlueprintReadOnly, Category = "Path")
	FString Content;
	UPROPERTY(BlueprintReadOnly, Category = "Path")
	FString Intermediate;
	UPROPERTY(BlueprintReadOnly, Category = "Path")
	FString Log;
	UPROPERTY(BlueprintReadOnly, Category = "Path")
	FString Mods;
	UPROPERTY(BlueprintReadOnly, Category = "Path")
	FString Plugins;
	UPROPERTY(BlueprintReadOnly, Category = "Path")
	FString Saved;
	UPROPERTY(BlueprintReadOnly, Category = "Path")
	FString User;
	UPROPERTY(BlueprintReadOnly, Category = "Path")
	FString PersistentDownload;
	UPROPERTY(BlueprintReadOnly, Category = "Path")
	FString PlatformExtensions;
};

USTRUCT(BlueprintType)
struct FEnginePath
{
	GENERATED_BODY();
public:
	UPROPERTY(BlueprintReadOnly, Category = "Path")
	FString Directory;
	UPROPERTY(BlueprintReadOnly, Category = "Path")
	FString Config;
	UPROPERTY(BlueprintReadOnly, Category = "Path")
	FString Content;
	UPROPERTY(BlueprintReadOnly, Category = "Path")
	FString Intermediate;
	UPROPERTY(BlueprintReadOnly, Category = "Path")
	FString Plugins;
	UPROPERTY(BlueprintReadOnly, Category = "Path")
	FString Saved;
	UPROPERTY(BlueprintReadOnly, Category = "Path")
	FString User;
	UPROPERTY(BlueprintReadOnly, Category = "Path")
	FString DefaultLayout;
	UPROPERTY(BlueprintReadOnly, Category = "Path")
	FString PlatformExtensions;
	UPROPERTY(BlueprintReadOnly, Category = "Path")
	FString UserLayout;
};



class UDataTable;

UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFFileLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure)
	static FString ExtendedProjectDirectory(TEnumAsByte<EFProjectDirectory> DirectoryType);

		/* Paths */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetEngineDirectories", CompactNodeTitle = "EngineDirs", Keywords = "File plugin path engine directory", ToolTip = "Gets the engine directories"), Category = "Path")
	static FEnginePath GetEngineDirectories();
	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetProjectDirectories", CompactNodeTitle = "ProjectDirs", Keywords = "File plugin path project directory", ToolTip = "Gets the project directories"), Category = "Path")
	static FProjectPath GetProjectDirectories();
	/* Text file */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ReadTextFile", CompactNodeTitle = "ReadText", Keywords = "File plugin read text", ToolTip = "Read a standard text file"), Category = "File|Text")
	static bool ReadText(FString Path, FString& Output);
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "WriteTextFile", CompactNodeTitle = "WriteText", Keywords = "File plugin write text", ToolTip = "Save a standard text file"), Category = "File|Text")
	static bool SaveText(FString Path, FString Text, FString& Error, bool Append = false, bool Force = false);
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ReadLineFile", CompactNodeTitle = "ReadLine", Keywords = "File plugin read text lines pattern", ToolTip = "Read the lines of a standard text file"), Category = "File|Text")
	static bool ReadLine(FString Path, FString Pattern, TArray<FString>& Lines);
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "WriteLineFile", CompactNodeTitle = "WriteLine", Keywords = "File plugin write text lines", ToolTip = "Save lines in a standard text file"), Category = "File|Text")
	static bool SaveLine(FString Path, const TArray<FString>& Text, FString& Error, bool Append = false, bool Force = false);
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ReadByteFile", CompactNodeTitle = "ReadByte", Keywords = "File plugin read byte", ToolTip = "Read byte file"), Category = "File|Byte")
	static bool ReadByte(FString Path, TArray<uint8>& Bytes);
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "WriteByteFile", CompactNodeTitle = "WriteByte", Keywords = "File plugin write byte", ToolTip = "Save byte to file"), Category = "File|Byte")
	static bool SaveByte(FString Path, const TArray<uint8>& Bytes, FString& Error, bool Append = false, bool Force = false);
	/* Base64 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "StrToBase64", CompactNodeTitle = "ToBase64", Keywords = "File plugin string convert base64 encode", ToolTip = "Encodes a string to base64"), Category = "File|Text")
	static FString StringToBase64(const FString Source);
	UFUNCTION(BlueprintPure, meta = (DisplayName = "StrFromBase64", CompactNodeTitle = "FromBase64", Keywords = "File plugin string convert decode base64", ToolTip = "Decodes a string from base64"), Category = "File|Text")
	static bool StringFromBase64(FString Base64Str, FString& Result);
	UFUNCTION(BlueprintPure, meta = (DisplayName = "BytesToBase64", CompactNodeTitle = "ToBase64", Keywords = "File plugin bytes convert base64 encode", ToolTip = "Encodes a byte array to base64"), Category = "File|Byte")
	static FString BytesToBase64(const TArray<uint8> Bytes);
	UFUNCTION(BlueprintPure, meta = (DisplayName = "BytesFromBase64", CompactNodeTitle = "FromBase64", Keywords = "File plugin bytes convert base64 decode", ToolTip = "Decodes a byte array from base64"), Category = "File|Byte")
	static bool BytesFromBase64(const FString Source, TArray<uint8>& Out);
	/* CSV file */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "WriteCSVFile", CompactNodeTitle = "WriteCSV", Keywords = "File plugin write csv", ToolTip = "Save a csv file"), Category = "File|CSV")
	static bool SaveCSV(FString Path, TArray<FString> Headers, TArray<FString> Data, int32& Total, bool Force = false);
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ReadCSVFile", CompactNodeTitle = "ReadCSV", Keywords = "File plugin read csv", ToolTip = "Read a csv file"), Category = "File|CSV")
	static bool ReadCSV(FString Path, TArray<FString>& Headers, TArray<FString>& Data, int32& Total, bool HeaderFirst = true);
	/* CSV convert */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "StringToCSV", CompactNodeTitle = "StrToCSV", Keywords = "File plugin string csv", ToolTip = "convert a string to csv"), Category = "CSV")
	static bool StringToCSV(FString Content, TArray<FString>& Headers, TArray<FString>& Data, int32& Total, bool HeaderFirst = true);
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "CSVToString", CompactNodeTitle = "CSVToStr", Keywords = "File plugin csv string", ToolTip = "convert a csv to string"), Category = "CSV")
	static bool CSVToString(FString& Result, TArray<FString> Headers, TArray<FString> Data, int32& Total);
	/* XML file */
	UFUNCTION(BlueprintCallable, Category = "XML", CustomThunk, meta = (CustomStructureParam = "InStruct"))
	static void StructToXML(FString& XML, bool& Success, const UStruct* InStruct);
	DECLARE_FUNCTION(execStructToXML)
	{
		P_GET_PROPERTY_REF(FStrProperty, XML);
		P_GET_UBOOL_REF(Success);

		Stack.Step(Stack.Object, NULL);

		FProperty* Property = Stack.MostRecentProperty;
		void* Ptr = Stack.MostRecentPropertyAddress;

		P_FINISH;

		Success = UEFFileLibrary::AnyStructToXmlString(Property, Ptr, XML);
	}
	UFUNCTION(BlueprintCallable, Category = "JSON", CustomThunk, meta = (CustomStructureParam = "OutStruct"))
	static void XMLToStruct(const FString& Xml, bool& Success, UStruct* & OutStruct);
	DECLARE_FUNCTION(execXMLToStruct)
	{
		// P_GET_SOFTCLASS_REF(int32, Total);
		P_GET_PROPERTY(FStrProperty, Xml);
		P_GET_UBOOL_REF(Success);

		Stack.Step(Stack.Object, NULL);

		FProperty* Property = Stack.MostRecentProperty;
		void* Ptr = Stack.MostRecentPropertyAddress;

		P_FINISH;

		Success = UEFFileLibrary::XmlStringToAnyStruct(Property, Ptr, Xml);
	}
	/* JSON file */
	UFUNCTION(BlueprintCallable, Category = "JSON", CustomThunk, meta = (CustomStructureParam = "InStruct"))
	static void StructToJson(FString& Json, bool& Success, const UStruct* InStruct);
	DECLARE_FUNCTION(execStructToJson)
	{
		P_GET_PROPERTY_REF(FStrProperty, Json);
		P_GET_UBOOL_REF(Success);

		Stack.Step(Stack.Object, NULL);

		FProperty* Prop = Stack.MostRecentProperty;
		void* Ptr = Stack.MostRecentPropertyAddress;

		P_FINISH;

		Success = UEFFileLibrary::AnyStructToJsonString(Prop, Ptr, Json);
	}
	UFUNCTION(BlueprintCallable, Category = "JSON", CustomThunk, meta = (CustomStructureParam = "OutStruct"))
	static void JsonToStruct(const FString& Json, bool& Success, UStruct* & OutStruct);
	DECLARE_FUNCTION(execJsonToStruct)
	{
		// P_GET_SOFTCLASS_REF(int32, Total);
		P_GET_PROPERTY(FStrProperty, Json);
		P_GET_UBOOL_REF(Success);

		Stack.Step(Stack.Object, NULL);

		FProperty* Prop = Stack.MostRecentProperty;
		void* Ptr = Stack.MostRecentPropertyAddress;

		P_FINISH;

		Success = UEFFileLibrary::JsonStringToAnyStruct(Prop, Ptr, Json);
	}
	/* File system */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "IsFile", CompactNodeTitle = "IsFile", Keywords = "File plugin check file exist", ToolTip = "Check whether a file exists"), Category = "FileSystem")
	static bool IsFile(FString Path);
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "IsDirectory", CompactNodeTitle = "IsDir", Keywords = "File plugin check directory exist", ToolTip = "Check whether a directory exists"), Category = "FileSystem")
	static bool IsDirectory(FString Path);
	UFUNCTION(BlueprintPure, meta = (DisplayName = "IsValidFilename", CompactNodeTitle = "IsValidName", Keywords = "File plugin check path valid", ToolTip = "Check whether a filename is valid and can be used"), Category = "FileSystem")
	static bool IsValidFilename(FString Filename);
	UFUNCTION(BlueprintPure, meta = (DisplayName = "ValidateFilename", CompactNodeTitle = "ValidateName", Keywords = "File plugin validate path", ToolTip = "Validate a filename to be used on this file system"), Category = "FileSystem")
	static bool ValidateFilename(FString Filename, FString& ValidName);
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SetReadOnlyFlag", CompactNodeTitle = "SetReadOnly", Keywords = "File plugin read only path", ToolTip = "Updates the read only property on file"), Category = "FileSystem")
	static bool SetReadOnlyFlag(FString FilePath, bool Flag);
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "GetReadOnlyFlag", CompactNodeTitle = "IsReadOnly", Keywords = "File plugin read only path", ToolTip = "Gets the read only property on file"), Category = "FileSystem")
	static bool GetReadOnlyFlag(FString FilePath);
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "GetFileSize", CompactNodeTitle = "GetSize", Keywords = "File plugin size directory", ToolTip = "Gets the size of a file"), Category = "FileSystem")
	static int64 GetFileSize(FString FilePath);
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ListDirectory", CompactNodeTitle = "LsDir", Keywords = "File plugin list directory pattern regex recursive", ToolTip = "List nodes from directory"), Category = "FileSystem")
	static bool ListDirectory(FString Path, FString Pattern, TArray<FString>& Nodes, bool ShowFile = true, bool ShowDirectory = true, bool Recursive = false);
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "MakeDirectory", CompactNodeTitle = "MkDir", Keywords = "File plugin make directory recursive", ToolTip = "Create a new directory"), Category = "FileSystem")
	static bool MakeDirectory(FString Path, bool Recursive = true);
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "RemoveDirectory", CompactNodeTitle = "RmDir", Keywords = "File plugin remove directory recursive", ToolTip = "Removes a directory"), Category = "FileSystem")
	static bool RemoveDirectory(FString Path, bool Recursive = false);
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "CopyDirectory", CompactNodeTitle = "CpDir", Keywords = "File plugin copy directory recursive", ToolTip = "Copies a directory"), Category = "FileSystem")
	static bool CopyDirectory(FString Source, FString Dest);
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "MoveDirectory", CompactNodeTitle = "MvDir", Keywords = "File plugin move directory recursive", ToolTip = "Moves a directory"), Category = "FileSystem")
	static bool MoveDirectory(FString Source, FString Dest);
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "NodeStats", CompactNodeTitle = "NodeStats", Keywords = "File plugin stats directory node", ToolTip = "Gets the stats of a node"), Category = "FileSystem")
	static bool NodeStats(FString Path, FCustomNodeStat& Stats);
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "RemoveFile", CompactNodeTitle = "RmFile", Keywords = "File plugin remove file recursive", ToolTip = "Removes a file"), Category = "FileSystem")
	static bool RemoveFile(FString Path);
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "CopyFile", CompactNodeTitle = "CpFile", Keywords = "File plugin copy file recursive", ToolTip = "Copies a file"), Category = "FileSystem")
	static bool CopyFile(FString Source, FString Dest, bool Force = false);
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "MoveFile", CompactNodeTitle = "MvFile", Keywords = "File plugin move file recursive", ToolTip = "Moves a file"), Category = "FileSystem")
	static bool MoveFile(FString Source, FString Dest, bool Force = false);
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "RenameFile", CompactNodeTitle = "RenameFile", Keywords = "File plugin rename file recursive", ToolTip = "Renames a file"), Category = "FileSystem")
	static bool RenameFile(FString Path, FString NewName);
	UFUNCTION(BlueprintPure, meta = (DisplayName = "PathParts", Keywords = "File plugin path parts", ToolTip = "Gets the parts of a path"), Category = "FileSystem")
	static void GetPathParts(FString Path, FString& PathPart, FString& BasePart, FString& ExtensionPart, FString& FileName);
	/* Screenshot */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "TakeScreenshot", CompactNodeTitle = "Screenshot", Keywords = "File plugin screenshot save image", ToolTip = "Take a screenshot and save"), Category = "Screenshot")
	static bool TakeScreenShot(FString Filename, FString& Path, bool PrefixTimestamp = true, bool ShowUI = true);
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "LoadScreenshot", CompactNodeTitle = "LoadScreenshot", Keywords = "File plugin texture read screenshot", ToolTip = "load a screenshot from image file"), Category = "Screenshot")
	static UTexture2D* LoadScreenshot(FString Path, bool& Success);
	/* Datatable */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "DataTableToCSV", Keywords = "File plugin datatable csv convert export", ToolTip = "Converts a datatable to csv string"), Category = "Datatable")
	static bool DatatableToCSV(UDataTable* Table, FString& Output);
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "DataTableToJSON", Keywords = "File plugin datatable json convert export", ToolTip = "Converts a datatable to json string"), Category = "Datatable")
	static bool DataTableToJSON(UDataTable* Table, FString& Output);
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "CSVToDataTable", Keywords = "File plugin datatable csv convert import", ToolTip = "Converts a csv string to datatable"), Category = "Datatable")
	static UDataTable* CSVToDataTable(FString CSV, UScriptStruct* Struct, bool& Success);
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "JSONToDataTable", Keywords = "File plugin datatable json convert import", ToolTip = "Converts a json string to datatable"), Category = "Datatable")
	static UDataTable* JSONToDataTable(FString JSON, UScriptStruct* Struct, bool& Success);
	// static bool SaveDatatableToCSV(UDataTable* Table, FString Path, TArray<FString> Rows, TArray<FString> Columns, int32& Count, bool Force = false);
	/* Config file ini */
	UFUNCTION(BlueprintCallable, Category = "Config", CustomThunk, meta = (CustomStructureParam = "OutValue"))
	static void ReadConfig(FString FilePath, FString Section, FString Key, bool& Success, bool SingleLineArrayRead, UStruct*& OutValue);
	DECLARE_FUNCTION(execReadConfig)
	{
		P_GET_PROPERTY(FStrProperty, FilePath);
		P_GET_PROPERTY(FStrProperty, Section);
		P_GET_PROPERTY(FStrProperty, Key);
		P_GET_UBOOL_REF(Success);
		P_GET_UBOOL(SingleLineArrayRead);

		Stack.Step(Stack.Object, NULL);

		FProperty* Property = Stack.MostRecentProperty;
		void* ValuePtr = Stack.MostRecentPropertyAddress;

		P_FINISH;

		Success = UEFFileLibrary::ReadConfigFile(FilePath, Section, Key, Property, ValuePtr, SingleLineArrayRead);
	}
	UFUNCTION(BlueprintCallable, Category = "Config", CustomThunk, meta = (CustomStructureParam = "Value"))
	static void WriteConfig(FString FilePath, FString Section, FString Key, bool& Success, bool SingleLineArrayWrite, const UStruct* Value);
	DECLARE_FUNCTION(execWriteConfig)
	{
		P_GET_PROPERTY(FStrProperty, FilePath);
		P_GET_PROPERTY(FStrProperty, Section);
		P_GET_PROPERTY(FStrProperty, Key);
		P_GET_UBOOL_REF(Success);
		P_GET_UBOOL(SingleLineArrayWrite);

		Stack.Step(Stack.Object, NULL);

		FProperty* Property = Stack.MostRecentProperty;
		void* ValuePtr = Stack.MostRecentPropertyAddress;

		P_FINISH;

		Success = UEFFileLibrary::WriteConfigFile(FilePath, Section, Key, Property, ValuePtr, SingleLineArrayWrite);
	}
	UFUNCTION(BlueprintCallable, Category = "Config")
	static bool RemoveConfig(FString FilePath, FString Section, FString Key);
	/* Utility */
	static TArray<FString> SplitString(FString String, FString Separator, ESearchCase::Type SearchCase);
	static bool StringArrayToCSV(TArray<FString> Lines, TArray<FString>& Headers, TArray<FString>& Data, int32& Total, FString Delimiter = ",", bool HeaderFirst = true);
	// json v2
	static bool AnyStructToJsonString(FProperty* Property, void* ValuePtr, FString& Json);
	static TSharedRef<FJsonValue> AnyStructToJsonValue(FProperty* Property, void* ValuePtr);
	static bool JsonStringToAnyStruct(FProperty* Property, void* ValuePtr, const FString& Json);
	static TSharedRef<FJsonValue> JsonValueToAnyStruct(FProperty* Property, TSharedPtr<FJsonValue> Value);
	// convert jsonvalue to any structure 
	static bool JsonValueToAnyStruct(TSharedPtr<FJsonValue> JsonValue, FProperty* Property, void* ValuePtr);
	// xml v2
	static bool AnyStructToXmlString(FProperty* Property, void* ValuePtr, FString& Xml);
	static FString AnyStructToXmlValue(FProperty* Property, TSharedPtr<FJsonValue> Value, int32 Depth);
	static FString& CreateTagNode(FString Tag, FString& Content, int32 Depth, bool WithCR);
	static bool XmlStringToAnyStruct(FProperty* Property, void* ValuePtr, const FString& Xml);
	static TSharedRef<FJsonValue> XmlNodeToAnyStruct(FProperty* Property, FXmlNode* Node);
	// xml escape
	static FString XmlEscapeChars(FString Source);
	static FString XmlConvertChars(FString Source);
	// config ini
	static bool WriteConfigFile(FString Filename, FString Section, FString Key, FProperty* Type, void* Value, bool SingleLineArray);
	static bool ReadConfigFile(FString Filename, FString Section, FString Key, FProperty* Type, void* Value, bool SingleLineArray);
	// datatable csv
	static bool WriteTableToCSV(const UDataTable& InDataTable, FString& Output);
	static bool WriteRowToCSV(const UScriptStruct* InRowStruct, const void* InRowData, FString& ExportedText);
	static bool WriteStructEntryToCSV(const void* InRowData, FProperty* InProperty, const void* InPropertyData, FString& ExportedText);
	// datatable json
	static FString GetKeyFieldName(const UDataTable& InDataTable);
	static bool WriteTableToJSON(const UDataTable& InDataTable, FString& OutExportText);
	static bool WriteTableAsObjectToJSON(const UDataTable& InDataTable, TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> JsonWriter);
	static bool WriteRowToJSON(const UScriptStruct* InRowStruct, const void* InRowData, TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> JsonWriter);
	static bool WriteStructToJSON(const UScriptStruct* InStruct, const void* InStructData, TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> JsonWriter);
	static bool WriteStructEntryToJSON(const void* InRowData, const FProperty* InProperty, const void* InPropertyData, TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> JsonWriter);
	static bool WriteContainerEntryToJSON(const FProperty* InProperty, const void* InPropertyData, const FString* InIdentifier, TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> JsonWriter);
	static void WriteJSONObjectStartWithOptionalIdentifier(TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> JsonWriter, const FString* InIdentifier);
	static void WriteJSONValueWithOptionalIdentifier(TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> JsonWriter, const FString* InIdentifier, const TCHAR* InValue);
};
