// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#define PRINT_STRING(Time , Color , String) 	GEngine->AddOnScreenDebugMessage(-1, Time , FColor::Color , String);

#define CREATE_COMPONENT(Component , Class , Name) Component = CreateDefaultSubobject<Class>(TEXT(Name));

#define ELSE_LOG(CategoryName, Verbosity, Format) 	else  UE_LOG(CategoryName,Verbosity,Format)


#define CREATE_TIMER(Handle,Object,Function,Time,Loop) if(GetWorld()) GetWorld()->GetTimerManager().SetTimer(Handle,Object,Function,Time,Loop)

#define CLEAR_TIMER(Handle) if(GetWorld()) GetWorld()->GetTimerManager().ClearTimer(Handle)
