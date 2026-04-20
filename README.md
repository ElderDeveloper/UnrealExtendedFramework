# Unreal Extended Framework
Kemal Erdem YILMAZ Blueprint and C++ framework plugin for Unreal Engine.

This repository contains runtime and editor modules for gameplay systems, backend utilities, online service integrations, and typed SQLite tooling.

Current SQL documentation starts in the Unreal Extended SQL section below and is backed by:

- `ExtendedSQL.md`
- `ExtendedSQLBlueprint.md`
- `ExtendedSQLImplementation.md`
- `ExtendedSQLiteFeasibility.md`



<a name="table-of-contents"></a>
## Table of Contents
> 1. [Unreal Extended Backend](#extended-backend)   
>    1.1 [HTTP](#extended-backend-http)   
>    1.2 [Socket](#extended-backend-socket)   
>    1.3 [Json](#extended-backend-json)   
> 2. [Unreal Extended Editor](#extended-editor)   
>    2.1 [Animation Nodes](#extended-editor-animation)    
>    2.1 [Cheat Manager](#extended-asc-cheat)   
> 3. [Unreal Extended Framework](#extended-framework)   
>    3.1 [AI](#extended-framework-AI)   
>    &nbsp;&nbsp;&nbsp;&nbsp; 3.1.1 [Black Board Decorators](#extended-framework-AI-btd)   
>    &nbsp;&nbsp;&nbsp;&nbsp; 3.1.2 [Black Board Services](#extended-framework-AI-bts)    
>    &nbsp;&nbsp;&nbsp;&nbsp; 3.1.3 [Black Board Tasks](#extended-framework-AI-btt)   
>    &nbsp;&nbsp;&nbsp;&nbsp; 3.1.3 [Environment Query System Contexts](#extended-framework-AI-eqsc)    
>    &nbsp;&nbsp;&nbsp;&nbsp; 3.1.4 [Environment Query System Generators](#extended-framework-AI-eqsg)    
>    &nbsp;&nbsp;&nbsp;&nbsp; 3.1.5 [Environment Query System Queries](#extended-framework-AI-eqsq)   
>    &nbsp;&nbsp;&nbsp;&nbsp; 3.1.6 [Environment Query System Tests](#extended-framework-AI-eqst)   
>    3.2 [Asyc Libraries](#extended-framework-asnc)   
>    &nbsp;&nbsp;&nbsp;&nbsp; 3.2.1 [İnput](#extended-framework-asnc-input)   
>    &nbsp;&nbsp;&nbsp;&nbsp; 3.2.2 [Loop](#extended-framework-asnc-loop)   
>    &nbsp;&nbsp;&nbsp;&nbsp; 3.2.3 [Timeline](#extended-framework-asnc-timeline)   
>    3.3 [Framework Datas](#extended-framework-data)    
>    3.4 [Libraries](#extended-framework-library)   
>    &nbsp;&nbsp;&nbsp;&nbsp; 3.4.1 [AI](#extended-framework-library-aş)   
>    &nbsp;&nbsp;&nbsp;&nbsp; 3.4.2 [Actor](#extended-framework-library-actor)   
>    &nbsp;&nbsp;&nbsp;&nbsp; 3.4.3 [Array](#extended-framework-library-array)   
>    &nbsp;&nbsp;&nbsp;&nbsp; 3.4.4 [Condition](#extended-framework-library-condition)   
>    &nbsp;&nbsp;&nbsp;&nbsp; 3.4.5 [Conversion](#extended-framework-library-conversion)   
>    &nbsp;&nbsp;&nbsp;&nbsp; 3.4.6 [Data Table](#extended-framework-library-datatable)   
>    &nbsp;&nbsp;&nbsp;&nbsp; 3.4.7 [File](#extended-framework-library-file)   
>    &nbsp;&nbsp;&nbsp;&nbsp; 3.4.8 [Global](#extended-framework-library-global)   
>    &nbsp;&nbsp;&nbsp;&nbsp; 3.4.9 [Input](#extended-framework-library-input)   
>    &nbsp;&nbsp;&nbsp;&nbsp; 3.4.10 [Math](#extended-framework-library-math)   
>    &nbsp;&nbsp;&nbsp;&nbsp; 3.4.11 [Mobile](#extended-framework-library-mobile)   
>    &nbsp;&nbsp;&nbsp;&nbsp; 3.4.12 [Monitor](#extended-framework-library-monitor)   
>    &nbsp;&nbsp;&nbsp;&nbsp; 3.4.13 [Perception](#extended-framework-library-perception)   
>    &nbsp;&nbsp;&nbsp;&nbsp; 3.4.14 [Print Screen](#extended-framework-library-printscreen)   
>    &nbsp;&nbsp;&nbsp;&nbsp; 3.4.15 [Property](#extended-framework-library-property)   
>    &nbsp;&nbsp;&nbsp;&nbsp; 3.4.16 [Trace](#extended-framework-library-trace)   
>    &nbsp;&nbsp;&nbsp;&nbsp; 3.4.17 [Variable](#extended-framework-library-variable)   
>    &nbsp;&nbsp;&nbsp;&nbsp; 3.4.18 [Widget](#extended-framework-library-widget)   
> 4. [Unreal Extended Gameplay](#extended-gameplay)  
>    4.1 [AI](#extended-gameplay-ai)   
>    4.2 [Damage](#extended-gameplay-damage)   
>    4.3 [Animation](#extended-gameplay-animation)   
>    4.4 [GAS](#extended-gameplay-gas)   
>    4.5 [Library](#extended-gameplay-library)   
>    4.6 [Systems](#extended-gameplay-systems)   
>    &nbsp;&nbsp;&nbsp;&nbsp; 4.6.1 [Combat](#extended-gameplay-systems-combat)   
>    &nbsp;&nbsp;&nbsp;&nbsp; 4.6.2 [Cover](#extended-gameplay-systems-cover)   
>    &nbsp;&nbsp;&nbsp;&nbsp; 4.6.3 [Footstep](#extended-gameplay-systems-footstep)   
>    &nbsp;&nbsp;&nbsp;&nbsp; 4.6.4 [Input Buffer](#extended-gameplay-systems-inputbuffer)   
>    &nbsp;&nbsp;&nbsp;&nbsp; 4.6.5 [Ladder](#extended-gameplay-systems-ladder)   
>    &nbsp;&nbsp;&nbsp;&nbsp; 4.6.6 [Mantle Ledge](#extended-gameplay-systems-mantleledge)   
>    &nbsp;&nbsp;&nbsp;&nbsp; 4.6.7 [State Machine](#extended-gameplay-systems-statemachine)   
>    &nbsp;&nbsp;&nbsp;&nbsp; 4.6.8 [Targeting](#extended-gameplay-systems-targeting)   
>    &nbsp;&nbsp;&nbsp;&nbsp; 4.6.9 [Weapon](#extended-gameplay-systems-weapon)   
> 5. [Unreal Extended Settings](#extended-settings)  
>    5.1 [Data](#extended-settings-data)  
>    5.2 [Subsystems](#extended-settings-subsystem)  
>    5.3 [Subtitles](#extended-settings-subtitle)  
>    5.4 [Widgets](#extended-settings-widgets)  
> 6. [Unreal Extended Steam](#extended-steam)  
>    6.1 [Subsystems](#extended-steam-subsystem)  
> 7. [Unreal Extended SQL](#extended-sql)  
>    7.1 [Runtime Model](#extended-sql-runtime)  
>    7.2 [Table Assets](#extended-sql-assets)  
>    7.3 [Authoring Workflow](#extended-sql-authoring)  
>    7.4 [Documentation](#extended-sql-docs)  




<a name="extended-backend"></a>
### 1.Unreal Extended Backend
The framework that connect external data bases using TCP-UDP or HTTP calls.


<a name="extended-editor"></a>
### 2.Unreal Extended Editor
Any editor extension that can be used during development.


<a name="extended-framework"></a>
### 3.Unreal Extended Framework
Collection of libraries that can be used during development


<a name="extended-gameplay"></a>
### 4.Unreal Extended Gameplay
Collection of gameplay systems that can be used during development


<a name="extended-settings"></a>
### 5.Unreal Extended Settings
All settings options that can be used for game settings and some settings releted gameplay systems


<a name="extended-steam"></a>
### 6.Unreal Extended Steam
Steam sdk integration for Unreal Engine


<a name="extended-sql"></a>
### 7.Unreal Extended SQL
Typed SQLite runtime and editor tooling for Unreal Engine.

The current SQL module is built around three public ideas:

- `UESQLSubsystem` is the runtime front door for database lifecycle, typed row queries, raw SQL, async work, transactions, and snapshots.
- `UESQLTableAsset` is metadata-first and defines row struct, database name, table name, primary key, label column, scope, and persistence.
- `.usqlite/` is the authored source of truth for team workflows, while `.db` files are treated as derived runtime or preview artifacts.


<a name="extended-sql-runtime"></a>
#### 7.1 Runtime Model

Use `UESQLSubsystem` for runtime behavior.

- Open, close, and delete logical databases.
- Load, save, find, count, page, and delete typed rows through a `UESQLTableAsset`.
- Run raw SQL only for advanced or tooling-focused cases.
- Use the same subsystem model for async queries, transactions, backups, and snapshots.


<a name="extended-sql-assets"></a>
#### 7.2 Table Assets

`UESQLTableAsset` is the typed schema contract between editor authoring and runtime execution.

- It describes the row struct and SQL table identity.
- It provides picker-friendly metadata such as primary key and default label column.
- It validates authoring problems in the editor before they turn into runtime failures.
- It is an input to subsystem calls, not the main runtime service surface.


<a name="extended-sql-authoring"></a>
#### 7.3 Authoring Workflow

The intended authoring flow is:

1. Define a row struct.
2. Create a `UESQLTableAsset`.
3. Save and validate the sibling `.usqlite/` project.
4. Build or preview a derived test `.db` through the editor and subsystem-backed tools.

This keeps SQL data merge-friendly and consistent with the current architecture reset.


<a name="extended-sql-docs"></a>
#### 7.4 Documentation

For the current SQL guides, start with:

- `ExtendedSQL.md`
- `ExtendedSQLBlueprint.md`
- `ExtendedSQLImplementation.md`
- `ExtendedSQLiteFeasibility.md`

For a fast path:

1. read `ExtendedSQL.md` for the product overview
2. read `ExtendedSQLBlueprint.md` for the supported Blueprint workflow
3. read `ExtendedSQLImplementation.md` for current runtime and authoring responsibilities

