# In-Game Currency & Store Blueprint Guide

This guide shows how to use the Extended EOS subsystems to build a complete in-game economy: **earn currency → buy items → unlock content**, all synced to cloud via EOS.

---

## Architecture Overview

| Component | EOS Subsystem | Purpose |
|-----------|---------------|---------|
| Currency Balance | `EEOSStatsSubsystem` | Track gold/coins as an EOS stat |
| Store Catalog | `EEOSTitleStorageSubsystem` | Read-only JSON shop data from DevPortal |
| Player Inventory | `EEOSPlayerStorageSubsystem` | Save owned items to cloud |
| Entitlements | `EEOSEcomSubsystem` | Optional: real-money DLC checks |

```
┌──────────────┐     ┌──────────────────┐     ┌─────────────────────┐
│  Stats       │     │  Title Storage   │     │  Player Storage     │
│  "GoldCoins" │────▶│  store.json      │────▶│  inventory.sav      │
│  IngestStat  │     │  (shop catalog)  │     │  (owned items)      │
│  GetStatValue│     │  ReadTitleFile   │     │  WriteSaveGame      │
└──────────────┘     └──────────────────┘     └─────────────────────┘
```

---

## Step 1: Set Up Stats in EOS DevPortal

In the [EOS DevPortal](https://dev.epicgames.com), create a stat:

- **Stat Name:** `GoldCoins`
- **Aggregation Type:** `SUM` (each IngestStat adds to the total)

> ⚠️ **Important:** EOS Stats with `SUM` aggregation only support **positive** values via `IngestStat`. To handle spending, we track purchases in Player Storage and compute `Available = Total Earned - Total Spent` in game logic.

---

## Step 2: Reward Currency (Blueprint)

When the player completes a quest, wins a match, etc.:

```
[Quest Complete Event]
        │
        ▼
┌─────────────────────────────┐
│ Get EEOSStatsSubsystem      │
│ (Get Game Instance Subsystem)│
└─────────────┬───────────────┘
              │
              ▼
┌─────────────────────────────┐
│ IngestStat                   │
│   StatName: "GoldCoins"      │
│   Amount: 100                │
└─────────────────────────────┘
```

**Blueprint nodes:**
1. `Get Game Instance Subsystem` → class: `EEOSStatsSubsystem`
2. Call `IngestStat` with `StatName = "GoldCoins"` and `Amount = 100`
3. Bind to `OnStatIngested` delegate to confirm success

---

## Step 3: Query Balance (Blueprint)

To display the player's currency:

```
┌─────────────────────────────┐
│ QueryLocalStats              │
│   StatNames: ["GoldCoins"]   │
└─────────────┬───────────────┘
              │
              ▼
┌─────────────────────────────┐
│ Bind: OnStatsQueried         │
│   → GetStatValue("GoldCoins")│
│   → Update UI Widget         │
└─────────────────────────────┘
```

**Blueprint nodes:**
1. Call `QueryLocalStats` with array `["GoldCoins"]`
2. On `OnStatsQueried` event → call `GetStatValue("GoldCoins")` → returns `int32`
3. Display in your HUD widget

---

## Step 4: Store Catalog via Title Storage

### 4a. Create the catalog JSON

Upload a file called `store_catalog.json` to **Title Storage** in the EOS DevPortal:

```json
{
  "items": [
    {
      "id": "sword_flame",
      "name": "Flame Sword",
      "description": "A sword engulfed in flames",
      "price": 500,
      "category": "weapons"
    },
    {
      "id": "armor_shadow",
      "name": "Shadow Armor",
      "description": "Armor that blends with darkness",
      "price": 800,
      "category": "armor"
    },
    {
      "id": "potion_health_xl",
      "name": "XL Health Potion",
      "description": "Restores 500 HP",
      "price": 50,
      "category": "consumables"
    }
  ]
}
```

### 4b. Load catalog in game (Blueprint)

```
[Game Start / Shop Open]
        │
        ▼
┌─────────────────────────────┐
│ Get EEOSTitleStorageSubsystem│
└─────────────┬───────────────┘
              │
              ▼
┌─────────────────────────────┐
│ ReadTitleFileAsString        │
│   FileName: "store_catalog"  │
└─────────────┬───────────────┘
              │
              ▼
┌─────────────────────────────┐
│ Bind: OnTitleFileRead        │
│   → Parse JSON string        │
│   → Populate shop UI         │
└─────────────────────────────┘
```

**Blueprint nodes:**
1. `Get Game Instance Subsystem` → `EEOSTitleStorageSubsystem`
2. Call `ReadTitleFileAsString` with `"store_catalog"`
3. On `OnTitleFileRead` → convert `Data` bytes to string → parse JSON → build shop widgets

---

## Step 5: Player Inventory (Save Owned Items)

Use `EEOSPlayerStorageSubsystem` with a `USaveGame` object:

### 5a. Create a SaveGame class

Create a new Blueprint class inheriting from `SaveGame`:

```
BP_PlayerInventory (parent: SaveGame)
  Variables:
    - OwnedItemIds : Array<String>
    - TotalSpent : Integer
```

### 5b. Purchase Flow (Blueprint)

```
[Player clicks "Buy" on Flame Sword (500 gold)]
        │
        ▼
┌─────────────────────────────┐
│ GetStatValue("GoldCoins")    │  ← current total earned
│ Read Inventory from cloud    │  ← get TotalSpent
│ Available = Total - Spent    │
└─────────────┬───────────────┘
              │
       Available >= 500?
         ┌────┴────┐
        Yes       No → Show "Not enough gold"
         │
         ▼
┌─────────────────────────────┐
│ Add "sword_flame" to         │
│   OwnedItemIds array         │
│ TotalSpent += 500            │
│ WriteSaveGame("inventory",   │
│   BP_PlayerInventory)        │
└─────────────┬───────────────┘
              │
              ▼
┌─────────────────────────────┐
│ Bind: OnPlayerDataWritten    │
│   → Show "Purchase Success!" │
│   → Refresh currency display │
│   → Enable content           │
└─────────────────────────────┘
```

### 5c. Load Inventory on Login (Blueprint)

```
[Login Complete]
        │
        ▼
┌─────────────────────────────┐
│ Get EEOSPlayerStorageSubsystem│
│ ReadSaveGame("inventory")    │
└─────────────┬───────────────┘
              │
              ▼
┌─────────────────────────────┐
│ Bind: OnSaveGameRead         │
│   → Cast to BP_PlayerInventory│
│   → Cache OwnedItemIds      │
│   → Cache TotalSpent         │
│   → Enable owned content     │
└─────────────────────────────┘
```

---

## Step 6: Enable Content Based on Ownership

In your game logic (any Blueprint), check owned items:

```
┌─────────────────────────────┐
│ Get BP_PlayerInventory       │
│ Does OwnedItemIds contain    │
│   "sword_flame"?             │
└─────────────┬───────────────┘
         ┌────┴────┐
        Yes       No
         │         │
         ▼         ▼
  Equip Flame   Show "Locked"
  Sword mesh    + "Buy for 500g"
  + particles
```

---

## Full Purchase Blueprint Pseudocode

```
Event: OnBuyButtonClicked(ItemId, Price)
│
├── QueryLocalStats(["GoldCoins"])
│   └── OnStatsQueried →
│       ├── TotalEarned = GetStatValue("GoldCoins")
│       ├── Available = TotalEarned - Inventory.TotalSpent
│       │
│       ├── IF Available >= Price:
│       │   ├── Inventory.OwnedItemIds.Add(ItemId)
│       │   ├── Inventory.TotalSpent += Price
│       │   ├── WriteSaveGame("inventory", Inventory)
│       │   └── OnPlayerDataWritten → Show Success, Refresh UI
│       │
│       └── ELSE:
│           └── Show "Not enough gold! Need {Price - Available} more."
```

---

## Security Notes

> **⚠️ Client-side only:** This approach trusts the client. For competitive games, consider:
> - Validating purchases on a **dedicated server** or **backend service**
> - Using EOS **Server-to-Server API** for stat ingestion
> - Signing inventory data before writing to Player Storage

For single-player or co-op games, this client-side approach works well.

---

## Quick Reference: Blueprint Nodes Used

| Node | Subsystem | What it does |
|------|-----------|-------------|
| `IngestStat` | Stats | Add currency (e.g., +100 gold) |
| `QueryLocalStats` | Stats | Fetch current stat values |
| `GetStatValue` | Stats | Read a specific stat (e.g., "GoldCoins") |
| `ReadTitleFileAsString` | Title Storage | Load shop catalog JSON |
| `ReadSaveGame` | Player Storage | Load player inventory from cloud |
| `WriteSaveGame` | Player Storage | Save inventory to cloud |
| `HasEntitlement` | Ecom | Check real-money DLC ownership |
