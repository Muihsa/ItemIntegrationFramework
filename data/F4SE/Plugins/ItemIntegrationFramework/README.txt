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
  Card_AimModelRecoilMaxDegPerShot.json
  Card_AimModelRecoilMinDegPerShot.json
  Card_SightedTransitionSeconds.json
  Card_ZoomDataFOVMult.json
  Card_ReloadSpeed.json

Rule loading:
  ItemIntegrationFramework loads .json and .jsonc rule files only from:
    Data\F4SE\Plugins\ItemIntegrationFramework\

  CPP_Overrides.json is reserved for editor output and C++ card overrides.
  Do not use CPP_Overrides.json as a normal user rule file.

WeaponStat data source IDs:
  APCost (legacy alias), AttackActionPointCost, Speed, Reach, SecondaryDamage,
  AmmoCapacity, AimModelRecoilMaxDegPerShot, AimModelRecoilMinDegPerShot,
  SightedTransitionSeconds, ZoomDataFOVMult, ReloadSpeed, CritCharge, CritDamage.

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
