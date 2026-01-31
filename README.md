# Glow Be Gone Redone - NG

**Brief:**
Dynamically removes annoying edge glow from shader effects via an SKSE plugin. Original mod by [Ryan (Fudgyduff)](https://www.nexusmods.com/skyrimspecialedition/users/5687342) and then updated by [dukethedropkicker](https://www.nexusmods.com/skyrimspecialedition/users/58293366). This rewrite of the mod now works with **1.6.1170** and **1.5.97**.

---

### Configuration

* **`RemoveActorFX = true`**: Set to true to disable character glows. Set to false if you prefer to keep original actor shader effects.
* **`RemoveWeaponFX = true`**: Set to true to disable weapon glows. Set to false to keep weapon-specific shader effects active.
* **`ExclusionList = []`**: Enter the names of specific plugins here to exempt them from these changes and retain their original effects.

**New Feature:**
* **`MagicEffectExclusionList = []`**: Add specific Magic Effect IDs to this list if you want certain spells or enchantments to remain visible.

---

### Incompatible Mods

* [Glow Be Gone - SkyPatcher](https://www.nexusmods.com/skyrimspecialedition/mods/119891): Does almost the same thing but does not have any **Exclusion** support. 
    > **Note:** I prefer not to use SkyPatcher as it can inherit issues ranging from CTDs to slow loading times.
* [Glow be Gone SKSE Updated GhostFX Workaround](https://www.nexusmods.com/skyrimspecialedition/mods/36112): **Built into the mod itself.**

---

### Permission
Just like the OG mod, this is released under the **MIT license**.

---

### Credits
* [Ryan (Fudgyduff)](https://www.nexusmods.com/skyrimspecialedition/users/5687342)
* [dukethedropkicker](https://www.nexusmods.com/skyrimspecialedition/users/58293366)
* [mrowrpurr](https://www.youtube.com/@SkyrimScripting)
* [nithog](https://next.nexusmods.com/profile/nithog?gameId=1704)
* [MarkVII_](https://www.twitch.tv/markvii_)
* [YzaraHQ](https://www.twitch.tv/yzarahq)

---
