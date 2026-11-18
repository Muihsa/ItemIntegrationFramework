ItemIntegrationFramework release notes

Install path:
  Data\F4SE\Plugins\ItemIntegrationFramework\

Required files in this folder:
  font.ttf
  lang\en_US.json
  lang\zh_CN.json

Optional/default rule files:
  BWS_IIF.json
  Card_APCost.json
  Card_CritCharge.json
  Card_CritDamage.json
  Card_FireMode.json
  Card_VATS.json
  Card_SecondaryDamage.json
  Card_AmmoCapacity.json
  Card_NumProjectiles.json
  Card_AimModelRecoilMaxDegPerShot.json
  Card_AimModelRecoilMinDegPerShot.json
  Card_AimModelRecoilArcDeg.json
  Card_AimModelRecoilArcRotateDeg.json
  Card_AimModelMaxConeDegrees.json
  Card_AimModelMinConeDegrees.json
  Card_AimModelConeIronSightsMultiplier.json
  Card_SightedTransitionSeconds.json
  Card_ZoomDataFOVMult.json
  Card_ReloadSpeed.json
  Card_ReloadTimeEmpty.json
  Card_ReloadTimeReserve.json

Rule loading:
  ItemIntegrationFramework loads .json and .jsonc rule files only from:
    Data\F4SE\Plugins\ItemIntegrationFramework\

  CPP_Overrides.json is reserved for editor output and C++ card overrides.
  Do not use CPP_Overrides.json as a normal user rule file.

WeaponStat data source IDs:
  APCost (legacy alias), AttackActionPointCost, Speed, Reach, SecondaryDamage,
  AmmoCapacity, NumProjectiles, AimModelRecoilMaxDegPerShot,
  AimModelRecoilMinDegPerShot, AimModelRecoilArcDeg, AimModelRecoilArcRotateDeg,
  AimModelMaxConeDegrees, AimModelMinConeDegrees,
  AimModelConeIronSightsMultiplier,
  SightedTransitionSeconds, ZoomDataFOVMult, ReloadSpeed, CritCharge, CritDamage.

Weapon reload time (WeaponReloadTime):
  Reads the total duration of the first-person reload clip and converts it with
  the weapon's reload speed:
    "DataSource": { "Type": "WeaponReloadTime", "ID": "ReloadEmpty", "roundDigits": 2, "Suffix": "s" }
  Accepted ID values:
    ReloadEmpty   -> WPNReload.hkx         (empty-magazine reload)
    ReloadReserve -> WPNReloadReserve.hkx  (tactical / reserve-ammo reload)
    (WPNReload, WPNReloadReserve, Empty, Reserve, Tactical also work)
  The animation directory is resolved from the active RACE subgraph data
  (HumanRace -> Subgraph Data) with the weapon keyword set, i.e. the base
  weapon keywords plus InstanceData/OMOD keywords. The card is skipped when no
  subgraph entry matches, the behavior graph does not assign that clip, or the
  .hkx file cannot be read - so vanilla-style weapons show only the row their
  behavior actually defines. A rule with a fallback (see below) keeps its card
  and shows the fallback text instead.
  Speed conversion (default is a division: the bigger reloadSpeed, the shorter
  the displayed time):
    "SpeedMode": "divide"    -> clip duration / reloadSpeed   (default, omitted)
    "SpeedMode": "multiply"  -> clip duration * reloadSpeed
  Bundled cards: Card_ReloadTimeEmpty.json, Card_ReloadTimeReserve.json.

Projectile count (NumProjectiles):
  "Type": "WeaponStat", "ID": "NumProjectiles" reads RangedData::numProjectiles
  from the weapon instance (OMOD changes such as splitter barrels are included).

  A NumProjectiles rule normally renders its own card. Set inlineDamage to fold
  the count into the vanilla Damage row instead; the bundled
  Card_NumProjectiles.json ships with inlineDamage enabled. Add these keys to
  "global" or to a DataRules entry:
    "inlineDamage": true                         // false/omitted = standalone card
    "inlineDamageFormat": "{damage} x {count}"   // optional, this is the default
    "inlineDamageDigits": 0                      // optional, decimals for {damage}

  With inlineDamage enabled, a weapon that fires more than one projectile shows
  "per-shot x count" instead of the total damage, e.g. total 100 with 8 pellets
  becomes "13 x 8". Single-projectile weapons are left untouched. The numeric
  card value stays the original total damage, so the red/green comparison still
  compares total damage. Placeholders: {damage} = per-projectile damage,
  {count} = projectile count, {total} = original total damage.
  Inline mode is only honoured when the rule's DataSource is
  WeaponStat/NumProjectiles.

Menu visibility (menus):
  By default a card shows in every menu. Add "menus" to "global" (or to a single
  DataRules entry) to whitelist menus:
    "menus": ["ExamineMenu"]                       // array form
    "menus": { "ExamineMenu": true, "PipboyMenu": false,
               "BarterMenu": false, "ContainerMenu": false }   // object form
  Recognised names: ExamineMenu, PipboyMenu, BarterMenu, ContainerMenu
  (short forms "Examine", "Pipboy", ... also work; matching is case-insensitive).
  Any other menu name is passed through as-is, so "CraftingMenu" also works.
  A DataRules entry with its own "menus" overrides the file-level "global" one.
  Omit the key to show the card everywhere.

Display decimals (roundDigits):
  Controls the decimals of the final displayed value (applied after Offset and
  Multiplier, and to the red/green difference as well):
    -1 (or omitted) = auto: whole numbers stay integers, everything else 1 decimal
    0..6            = round to that many decimals, e.g. 2 -> 1.5, 0 -> 2
  Rounding only, not zero padding: the game formatter drops trailing zeros
  (a value of 1.5 is shown as "1.5", not "1.50").
  Can be set on the rule ("global" or a DataRules entry) and/or inside a
  DataSource, where the DataSource value wins:
    "DataSource": { "Type": "WeaponStat", "ID": "AimModelRecoilArcDeg", "roundDigits": 2 }

Broadcast-only rules (broadcastOnly):
  Add "broadcastOnly": true to "global" or to a single DataRules entry to keep
  the rule as a pure data source:
    "broadcastOnly": true
  The rule still matches and reads its value exactly like a normal card, and the
  value is still published through ItemCardDataReady (kMessage_ItemCardDataReady).
  Nothing is inserted into the item card list, so the entry is invisible in every
  menu. Its broadcast entry carries kItemCardDataFlag_BroadcastOnly instead of
  kItemCardDataFlag_Injected. Anchors cannot target a broadcast-only rule.
  Display-only options (anchorTarget, inlineDamage, ...) have no visible effect.

Fallback display (fallback):
  A normal rule drops its whole card when a DataSource cannot produce a value
  (or when no ValuesMapping entry matches), and shows 0/negative/float-dust
  numbers as they are. Add a "fallback" block to "global" (or to a single
  DataRules entry, where it replaces the global block) to keep the card and show
  fixed text instead:
    "fallback": {
      "enabled": true,          // false/omitted = old behavior
      "value": "--",            // text shown instead of the value (localized tags work)
      "whenNoData": true,       // default true: missing animation/OMOD, unreadable
                                // pointer, non-finite value, or no ValuesMapping
                                // entry matched -> still draw the card
      "whenLessOrEqual": 0      // optional: use the fallback when the final number
                                // (after Offset/Multiplier) is <= this threshold;
                                // omit the key to ignore numbers
    }
  The fallback replaces the value text only. When it fires there is no raw value
  and no red/green comparison difference for that box, and a configured
  DataSource "Suffix" is not appended (include it in "value" if needed).
  Example: show "--" both for unreadable data and for values at or below 0.
  A ValuesMapping rule whose entries are all absent from the item still draws
  its card with the fallback text (e.g. "no maintenance OMOD attached"), but
  only when at least one mapped FormID resolves - a missing plugin or a typo in
  every key does not flood the inventory with fallback cards.
  Values are inherited by every DataRules entry in the file; a rule-level block
  replaces the inherited one instead of merging with it.

Documentation files:
  The Type0/Type1/Type2 example files are installed next to this folder:
    Data\F4SE\Plugins\IIF_Type0_Doc.json
    Data\F4SE\Plugins\IIF_Type1_Doc.json
    Data\F4SE\Plugins\IIF_Type2_Doc.json

  Chinese versions are also included:
    Data\F4SE\Plugins\IIF_Type0_文档.json
    Data\F4SE\Plugins\IIF_Type1_文档.json
    Data\F4SE\Plugins\IIF_Type2_文档.json

Editor:
  The in-game editor uses ImGui.
  Default hotkey: Shift+F11
  Hotkey settings are saved to:
    Data\F4SE\Plugins\ItemIntegrationFramework\IIF_EditorSettings.json

Card positioning:
  Lower priority numbers appear earlier/higher.
  anchorTarget can point to a vanilla card tag, such as $ammo or $rng, or to a custom card id.
  anchorMode supports after, before, and replace.
