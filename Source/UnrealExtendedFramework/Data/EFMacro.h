// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#define PRINT_STRING(Time , Color , String) 	GEngine->AddOnScreenDebugMessage(-1, Time , FColor::Color , String);

#define CREATE_COMPONENT(Component , Class , Name) Component = CreateDefaultSubobject<Class>(TEXT(Name));

#define ELSE_LOG(CategoryName, Verbosity, Format) 	else  UE_LOG(CategoryName,Verbosity,Format)

#define IF_VERSION_FIVE()	#if ENGINE_MAJOR_VERSION == 5
