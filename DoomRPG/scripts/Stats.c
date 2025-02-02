#include "Defs.h"

#include "RPG.h"
#include "Utils.h"
#include "ItemData.h"
#include "Shield.h"
#include "Map.h"
#include "Arena.h"
#include "Outpost.h"
#include "Skills.h"
#include "Augs.h"

// Globals
long int XPCurve;

// Stat Caps
int const SoftStatCap = 100;
int HardStatCap;

// XP & Rank Tables
long int XPTable[MAX_LEVEL];
long int RealStatTable[MAX_LEVEL * 3 + 1];
long int *StatTable;
long int RankTable[MAX_RANK];

// Rank strings
str const Ranks[MAX_RANK + 1] =
{
    "Rec",
    "Pvt", "Pvt +",
    "PFC", "PFC +",
    "LCpl", "LCpl +",
    "Cpl", "Cpl +",
    "Sgt", "Sgt +",
    "SSgt", "SSgt +",
    "GySgt", "GySgt +",
    "MSgt", "MSgt +",
    "1stSgt", "1stSgt +",
    "MGySgt", "MGySgt +",
    "SgtMaj", "SgtMaj +",
    "SgtMajMarCor", "MSgtMarCor"
};

str const LongRanks[MAX_RANK + 1] =
{
    "Recruit",
    "Private", "Private +",
    "Private First Class", "Private First Class +",
    "Lance Corporal", "Lance Corporal +",
    "Corporal", "Corporal +",
    "Sergeant", "Sergeant +",
    "Staff Sergeant", "Staff Sergeant +",
    "Gunnery Sergeant", "Gunnery Sergeant +",
    "Master Sergeant", "Master Sergeant +",
    "First Sergeant", "First Sergeant +",
    "Master Gunnery Sergeant", "Master Gunnery Sergeant +",
    "Sergeant Major", "Sergeant Major +",
    "Sergeant Major of the Marine Corps", "Master Sergeant Major of the Marine Corps",
};

NamedScript DECORATE void StatusEffect(int Type, int Time, int Intensity)
{
    // Can't get Status Effects if Pink Aura is active
    if (Player.Aura.Type[AURA_PINK].Active && Player.Aura.Type[AURA_PINK].Level >= 1 || (GetCVar("drpg_invulnerability_plus") && GetActorPowerupTics(0, "PowerInvulnerable") > 0))
        return;

    Intensity = Clamp(1, Intensity, 5);

    if (Player.StatusType[Type])
    {
        if (Intensity > Player.StatusIntensity[Type])
            Player.StatusIntensity[Type] = Intensity;

        Player.StatusTimer[Type] += Time * 35;

        if (Player.StatusTimer[Type] > Player.StatusTimerMax[Type])
            Player.StatusTimerMax[Type] = Player.StatusTimer[Type];
    }
    else
    {
        Player.StatusType[Type] = true;
        Player.StatusIntensity[Type] = Intensity;
        Player.StatusTimer[Type] = Time * 35;
        Player.StatusTimerMax[Type] = Player.StatusTimer[Type];
    }
}

NamedScript DECORATE void TryStatusEffect(int Type, int Time, int Intensity)
{
    // Can't get Status Effects if Pink Aura is active
    if (Player.Aura.Type[AURA_PINK].Active && Player.Aura.Type[AURA_PINK].Level >= 1) return;

    // Status Effect Resist check
    if (RandomFixed(0.0, 100.0) <= Player.StatusEffectResist)
    {
        ActivatorSound("health/statuseffect2", 127);
    }
    else // Apply status effect
    {
        ActivatorSound("health/statuseffect", 127);
        Player.StatusTypeHUD = Type;
        StatusEffect(Type, Time, Intensity);
    }
}

NamedScript void AddXP(int PlayerNum, long int XP, long int Rank)
{
    // Scale XP/Rank Gains using the XP Scaling Option
    // Don't forget to remove stupid fixed-point avoidance code after migration (if it happens)
    XP = (XP * (int)(GetCVarFixed("drpg_scalexp") * 100)) / 100;

    // Caps for add XP and Rank
    if (XP > Players(PlayerNum).XPNext / 10l) XP = Players(PlayerNum).XPNext / 10l;
    if (Rank > Players(PlayerNum).RankNext / 40l) Rank = Players(PlayerNum).RankNext / 40l;

    Players(PlayerNum).ComboTimer = COMBO_MAX;
    Players(PlayerNum).Combo++;

    Players(PlayerNum).XPGained += XP;
    Players(PlayerNum).RankGained += Rank;

    if (Players(PlayerNum).Aura.Type[AURA_WHITE].Active && Players(PlayerNum).Aura.Type[AURA_WHITE].Level >= 1)
    {
        if (Players(PlayerNum).Aura.Time > 0)
            Players(PlayerNum).Combo++;
    }
}

NamedScript DECORATE void AddMedkit(int Amount)
{
    Player.Medkit += Amount;

    if (Player.Medkit > Player.MedkitMax)
        Player.Medkit = Player.MedkitMax;
}

NamedScript DECORATE bool CheckMedkitMax()
{
    return (Player.Medkit >= Player.MedkitMax);
}

NamedScript KeyBind void UseMedkit()
{
    if (Player.Medkit <= 0 || Player.ActualHealth >= Player.HealthMax) return;

    int HealAmount = Player.HealthMax - Player.ActualHealth;

    if (Player.Medkit < HealAmount)
        HealAmount = Player.Medkit;
    else if (HealAmount + Player.ActualHealth > Player.HealthMax)
        HealAmount = Player.HealthMax - Player.ActualHealth;

    Player.ActualHealth += HealAmount;
    Player.Medkit -= HealAmount;

    // Add Vitality XP for using healing items
    if (GetCVar("drpg_levelup_natural"))
    {
        fixed Scale = GetCVarFixed("drpg_vitality_scalexp");
        if (GetCVar("drpg_allow_spec"))
        {
            if (GetActivatorCVar("drpg_character_spec") == 3)
                Scale *= 2;
        }

        int Factor = CalcPercent(HealAmount, Player.HealthMax);
        Player.VitalityXP += (int)(Factor * Scale * 10);
    }

    ActivatorSound("items/healthuse", 127);
}

void InitXPTable()
{
    XPTable[0] = 475l + XPCurve;
    for (int i = 1; i < MAX_LEVEL; i++)
        XPTable[i] = (((long int)(((long long int)XPTable[i - 1] * (long fixed)(1.0 + i / (i * 8.0))) + ((i * (long long int)XPCurve) * (long fixed)(1.0 + i / (101.0 - i))))) + 50) / 100 * 100;
}

void InitStatXPTable()
{
    StatTable = &RealStatTable[101];

    StatTable[0] = 475 + XPCurve;
    for (int i = 1; i < MAX_LEVEL * 2; i++)
        StatTable[i] = (((long int)(((long long int)StatTable[i - 1] * (long fixed)(1.0 + i / (i * 8.0))) + ((i * (long long int)XPCurve) * (long fixed)(1.0 + i / (201.0 - i * 2))))) + 50) / 100 * 100;
}

void InitNegativeStatXPTable()
{
    for (int i = 1; i <= MAX_LEVEL + 1; i++)
        StatTable[-i] = (((long int)(((long long int)StatTable[i - 1] * (long fixed)(1.0 + i / (i * 8.0)) * -1ll) + ((i * (long long int)XPCurve) * (long fixed)(1.0 + i / (201.0 - i * 2))))) + 50) / 100 * 100;
}

void InitRankTable()
{
    RankTable[0] = 5000 + (XPCurve * 600);
    for (int i = 1; i < MAX_RANK; i++)
        RankTable[i] = (((long int)((long long int)RankTable[i - 1] + ((i * (long long int)XPCurve * 600ll) * (1 + i)))) + 500) / 1000 * 1000;
}

void CheckCombo()
{
    // Time Freeze special handling
    if (Player.Combo > 0 && (CheckInventory("PowerTimeFreezer") || CheckInventory("DRPGPowerStimChrono")) && Player.ComboTimer >= COMBO_MAX)
        Player.ComboTimer = COMBO_MAX - 1;

    // Subtract the combo timer each tic
    if ((Player.Combo > 0 || Player.ComboTimer < COMBO_MAX) && !CheckInventory("PowerTimeFreezer") && !CheckInventory("DRPGPowerStimChrono"))
        Player.ComboTimer--;

    // Reset values if you start the combo again while it was cooling down
    if (Player.ComboTimer == COMBO_MAX)
    {
        Player.BonusGained = 0;
        Player.XPGained = 0;
        Player.RankGained = 0;
    }

    // Add up and randomize the XP and Rank gain
    if (Player.ComboTimer == COMBO_STOP || (Player.Aura.Type[AURA_WHITE].Active && Player.Aura.Type[AURA_WHITE].Level >= 2 && Timer() == 4))
    {
        long int ComboBonus = ((Player.XPGained + Player.RankGained) / 100 * Player.Combo);
        // Don't forget to remove stupid fixed-point avoidance code after migration (if it happens)
        ComboBonus = (ComboBonus * (int)(GetCVarFixed("drpg_scalecomboxp") * 100)) / 100;

        // You cannot gain Negative XP, but you can lose Rank
        if (Player.XPGained < 0) Player.XPGained = 0;

        if (Player.Combo > 1)
        {
            Player.BonusGained += ComboBonus;
            Player.XP += Player.XPGained + ComboBonus;
            Player.Rank += Player.RankGained + ComboBonus;
        }
        else
        {
            Player.XP += Player.XPGained;
            Player.Rank += Player.RankGained;
        }

        Player.XPGained = 0;
        Player.RankGained = 0;
    }

    if (Player.ComboTimer < 0)
    {
        Player.Combo = 0;
        Player.ComboTimer = COMBO_MAX;
        Player.BonusGained = 0;
        Player.XPGained = 0;
        Player.RankGained = 0;
    }
}

// Keeps current level and XP updated
void CheckLevel()
{
    if (Player.Level < MAX_LEVEL)
        Player.XPNext = XPTable[Player.Level];

    if (Player.Level >= MAX_LEVEL)
    {
        Player.XP = XPTable[MAX_LEVEL - 1];
        Player.XPNext = XPTable[MAX_LEVEL - 1];
    }

    // Now check for a level up
    if (Player.XP >= XPTable[Player.Level] && Player.Level < MAX_LEVEL)
    {
        int Modules = (int)((((fixed)Player.Level + 1) * 100.0) * GetCVarFixed("drpg_module_levelfactor"));

        // Take XP
        Player.XP -= XPTable[Player.Level];

        // Level Up
        Player.Level++;
        GiveInventory("DRPGModule", Modules);

        if (GetCVar("drpg_levelup_heal"))
        {
            HealThing(MAX_HEALTH);

            if (Player.EP < 0)
                Player.EP = 0;
            else
                Player.EP = Player.EPMax;
        }

        FadeRange(255, 255, 255, 0.5, 255, 255, 255, 0, 2.0);
        PrintMessage(StrParam("You have reached level %d", Player.Level), LEVELUP_ID, -32);

        ActivatorSound("misc/levelup", 96);
        SpawnForced("DRPGLevelUpArrow", GetActorX(0), GetActorY(0), GetActorZ(0) + GetActorPropertyFixed(Player.TID, APROP_Height), 0, 0);
    }
}

// Keeps current Rank updated
void CheckRank()
{
    bool RankPromotion;

    Player.RankString = Ranks[Player.RankLevel];

    if (Player.RankLevel < MAX_RANK)
        Player.RankNext = RankTable[Player.RankLevel];

    if (Player.RankLevel >= MAX_RANK)
    {
        Player.Rank = RankTable[MAX_RANK - 1];
        Player.RankNext = RankTable[MAX_RANK - 1];
    }

    // Rank Demotion
    if (!RankPromotion && Player.RankLevel > 0 && Player.Rank < 0)
    {
        // Take Rank Level
        Player.RankLevel--;

        // Take negative Rank
        Player.Rank = RankTable[Player.RankLevel] + Player.Rank;

        FadeRange(255, 0, 64, 0.25, 255, 0, 64, 0, 2.0);

        PrintMessage(StrParam("\CaYou have been demoted to rank %d: %S", Player.RankLevel, LongRanks[Player.RankLevel]), RANKUP_ID, 32);
    }

    // Necessary check the status Rank level
    if (RankPromotion && Player.Rank > 0) RankPromotion = false;

    // Rank Promotion
    if (Player.Rank >= RankTable[Player.RankLevel] && Player.RankLevel < MAX_RANK)
    {
        int NewItems;

        // Take Rank
        Player.Rank -= RankTable[Player.RankLevel];

        // Rank Up
        Player.RankLevel++;

        ActivatorSound("misc/rankup", 96);
        FadeRange(255, 255, 0, 0.5, 255, 255, 0, 0, 2.0);

        // Determine how many new items you've unlocked in the shop
        for (int i = 0; i < ItemCategories; i++)
            for (int j = 0; j < ItemMax[i]; j++)
                if (ItemData[i][j].Rank == Player.RankLevel)
                    NewItems++;

        PrintMessage(StrParam("\CkYou have been promoted to rank %d: %S", Player.RankLevel, LongRanks[Player.RankLevel]), RANKUP_ID, 32);

        SpawnForced("DRPGRankUpArrow", GetActorX(0), GetActorY(0), GetActorZ(0) + GetActorPropertyFixed(Player.TID, APROP_Height), 0, 0);

        // Tells you if you've unlocked new items in the Shop
        if (NewItems > 0)
            PrintMessage(StrParam("\CcYou have unlocked \Cf%d\Cc new items in the shop", NewItems), RANKUP_ID + 1, 96);

        RankPromotion = true;
    }
}

void CheckHealth()
{
    // If you're dead, return
    if (GetActorProperty(0, APROP_Health) <= 0) return;

    // Over-Heal behavior
    if (Player.OverHeal)
    {
        if ((Timer() % 35) == 0)
            Player.ActualHealth--;
        if (Player.ActualHealth <= Player.HealthMax)
            Player.OverHeal = false;
    }

    // < 10% Health
    if (Player.ActualHealth <= Player.HealthMax / 10 && !Player.Perks[STAT_VITALITY])
    {
        // Fade Effect
        FadeRange(255, 0, 0, 0.15 + (Sin(Timer() / 64.0) * 0.1), 255, 0, 0, 0.0, 1.0);

        // Heartbeat
        if ((Timer() % 64) == 0 && Player.ActualHealth > 0)
            ActivatorSound("health/low", 64);

        // Halve Movement Speed and Jump Height
        Player.Speed /= 2;
        Player.JumpHeight /= 2;
    }
}

void CheckStats()
{
    // VERY IMPORTANT CODE RIGHT HERE
    // If you're reading this and you'd like to modify stat curves, this is where you'd do it
    Player.StrengthTotal = Player.Strength + Player.StrengthNat + Player.SoulRedCount + Player.StrengthBonus;
    Player.DefenseTotal = Player.Defense + Player.DefenseNat + Player.SoulGreenCount + Player.DefenseBonus;
    Player.VitalityTotal = Player.Vitality + Player.VitalityNat + Player.SoulPinkCount + Player.VitalityBonus;
    Player.EnergyTotal = Player.Energy + Player.EnergyNat + Player.SoulBlueCount + Player.EnergyBonus;
    Player.RegenerationTotal = Player.Regeneration + Player.RegenerationNat + Player.SoulPurpleCount + Player.RegenerationBonus;
    Player.AgilityTotal = Player.Agility + Player.AgilityNat + Player.SoulOrangeCount + Player.AgilityBonus;
    Player.CapacityTotal = Player.Capacity + Player.CapacityNat + Player.SoulDarkBlueCount + Player.CapacityBonus;
    Player.LuckTotal = Player.Luck + Player.LuckNat + Player.SoulYellowCount + Player.LuckBonus;

    Player.LevelDamage = Player.Level;
    Player.BonusDamage = Player.StrengthTotal;
    Player.DamageMult = 1.0;
    Player.TotalDamage = Player.LevelDamage + Player.BonusDamage;
    if (Player.DefenseTotal > 0)
        Player.DamageFactor = (Player.DefenseTotal > 200 ? 0.25 : 1.0 - Curve((fixed)Player.DefenseTotal, 0, 200, 0.01, 0.75));
    else
        Player.DamageFactor = 1.0 + AbsFixed(((fixed)Player.DefenseTotal / 100.0));
    Player.Mass = 100 + (Player.DefenseTotal * 10);
    Player.HealthMax = 50 + ((Player.Level + 1) / 2) * 5 + Player.VitalityTotal * 5;
    Player.StatusEffectResist = (fixed)Player.VitalityTotal * 0.5;
    Player.EPMax = 50 + ((Player.Level + 1) / 2) * 5 + Player.EnergyTotal * 5;
    Player.Aura.Range = Player.EnergyTotal * 16;
    Player.ToxicityRegenBonus = Player.RegenerationTotal / 10;
    Player.Speed = 1.0 + 0.25 * ((fixed)Player.AgilityTotal / 100);
    Player.JumpHeight = 8.0;
    Player.WeaponSpeed = Player.AgilityTotal / 2;
    SetAmmoCapacity("Clip", (int)(115 + Player.CapacityTotal * 7.5) / 10 * 10);
    SetAmmoCapacity("Shell", 20 + Player.CapacityTotal * 2);
    SetAmmoCapacity("RocketAmmo", 2 + Player.CapacityTotal * 0.6);
    SetAmmoCapacity("Cell", (75 + Player.CapacityTotal * 5) / 10 * 10);
    Player.Stim.VialMax = Player.CapacityTotal * 2.5;
    Player.SurvivalBonus = (fixed)Player.AgilityTotal / 5.0;
    if (CompatMode == COMPAT_DRLA) // DRLA - Total Armors/Boots, Skulls
    {
        SetAmmoCapacity("RLArmorInInventory", DRLA_ARMOR_MAX);
        SetAmmoCapacity("RLSkullLimit", DRLA_SKULL_MAX);
        SetAmmoCapacity("RLPhaseDeviceLimit", DRLA_DEVICE_MAX);
    }
    Player.MedkitMax = Player.CapacityTotal * 5;

    // Determine current stat cap
    Player.StatCap = SoftStatCap + Player.Level * GetCVar("drpg_soft_stat_cap_mult");

    // Per-stat leveling
    if (GetCVar("drpg_levelup_natural"))
    {
        if (Player.StrengthXP >= StatTable[Player.StrengthNat] && Player.StrengthNat < NATURALCAP)
        {
            Player.StrengthXP -= StatTable[Player.StrengthNat];
            Player.StrengthNat++;
            ActivatorSound("misc/statup", 127);
            DrawStatUp(STAT_STRENGTH);
        }

        if (Player.DefenseXP >= StatTable[Player.DefenseNat] && Player.DefenseNat < NATURALCAP)
        {
            Player.DefenseXP -= StatTable[Player.DefenseNat];
            Player.DefenseNat++;
            ActivatorSound("misc/statup", 127);
            DrawStatUp(STAT_DEFENSE);
        }

        if (Player.VitalityXP >= StatTable[Player.VitalityNat] && Player.VitalityNat < NATURALCAP)
        {
            Player.VitalityXP -= StatTable[Player.VitalityNat];
            Player.VitalityNat++;
            ActivatorSound("misc/statup", 127);
            DrawStatUp(STAT_VITALITY);
        }

        if (Player.EnergyXP >= StatTable[Player.EnergyNat] && Player.EnergyNat < NATURALCAP)
        {
            Player.EnergyXP -= StatTable[Player.EnergyNat];
            Player.EnergyNat++;
            ActivatorSound("misc/statup", 127);
            DrawStatUp(STAT_ENERGY);
        }

        if (Player.RegenerationXP >= StatTable[Player.RegenerationNat] && Player.RegenerationNat < NATURALCAP)
        {
            Player.RegenerationXP -= StatTable[Player.RegenerationNat];
            Player.RegenerationNat++;
            ActivatorSound("misc/statup", 127);
            DrawStatUp(STAT_REGENERATION);
        }

        if (Player.AgilityXP >= StatTable[Player.AgilityNat] && Player.AgilityNat < NATURALCAP)
        {
            Player.AgilityXP -= StatTable[Player.AgilityNat];
            Player.AgilityNat++;
            ActivatorSound("misc/statup", 127);
            DrawStatUp(STAT_AGILITY);
        }

        if (Player.CapacityXP >= StatTable[Player.CapacityNat] && Player.CapacityNat < NATURALCAP)
        {
            Player.CapacityXP -= StatTable[Player.CapacityNat];
            Player.CapacityNat++;
            ActivatorSound("misc/statup", 127);
            DrawStatUp(STAT_CAPACITY);
        }

        if (Player.LuckXP >= StatTable[Player.LuckNat] && Player.LuckNat < NATURALCAP)
        {
            Player.LuckXP -= StatTable[Player.LuckNat];
            Player.LuckNat++;
            ActivatorSound("misc/statup", 127);
            DrawStatUp(STAT_LUCK);
        }

        // Add Regeneration XP for having low Health and/or EP
        if ((Player.ActualHealth < Player.HealthMax || Player.EP < Player.EPMax || Player.Focusing))
        {
            if (Timer() % 7 == 0)
            {
                fixed Scale = GetCVarFixed("drpg_regeneration_scalexp");
                if (GetCVar("drpg_allow_spec"))
                {
                    if (GetActivatorCVar("drpg_character_spec") == 5)
                        Scale *= 2;
                }
                fixed Factor = 2.0 - AbsFixed(((Player.ActualHealth <= Player.HealthMax ? ((fixed)Player.ActualHealth / (fixed)Player.HealthMax) : 1.0) + (Player.EP < Player.EPMax && Player.EPMax > 0 ? ((fixed)Player.EP / Player.EPMax) : 0)));
                Factor += Player.Focusing ? 1 : 0;
                Player.RegenerationXP += (int)(Factor * (8.25 * Scale));
            }
        }

        // Add Defense XP for decreases in Health
        if (Player.PrevHealth != Player.ActualHealth)
        {
            if (Player.ActualHealth < Player.PrevHealth)
            {
                fixed Scale = GetCVarFixed("drpg_defense_scalexp");
                if (GetCVar("drpg_allow_spec"))
                {
                    if (GetActivatorCVar("drpg_character_spec") == 2)
                        Scale *= 2;
                }
                int Factor = Player.PrevHealth - Player.ActualHealth;
                Player.DefenseXP += (int)(Factor * (11.25 * Scale));
            }

            Player.PrevHealth = Player.ActualHealth;
        }

        // Add Luck XP for increases in Credits
        if (Player.PrevCredits != CheckInventory("DRPGCredits"))
        {
            if (CheckInventory("DRPGCredits") > Player.PrevCredits)
            {
                fixed Scale = GetCVarFixed("drpg_luck_scalexp");
                if (GetCVar("drpg_allow_spec"))
                {
                    if (GetActivatorCVar("drpg_character_spec") == 8)
                        Scale *= 2;
                }
                Player.LuckXP += (int)((CheckInventory("DRPGCredits") - Player.PrevCredits) * (2.25 * Scale));
            }

            Player.PrevCredits = CheckInventory("DRPGCredits");
        }

        // Add Agility XP for maintaining high X/Y velocity
        fixed Velocity = AbsFixed(GetActorVelX(0)) + AbsFixed(GetActorVelY(0));
        if (Timer() % 7 == 0)
        {
            fixed Scale = GetCVarFixed("drpg_agility_scalexp");
            if (GetCVar("drpg_allow_spec"))
            {
                if (GetActivatorCVar("drpg_character_spec") == 6)
                    Scale *= 2;
            }
            Player.AgilityXP += (int)(Velocity * (0.333 * Scale));
        }
    }

    // Scale Health with Vitality changes
    if (Player.PrevVitality != Player.VitalityTotal)
    {
        if (Player.ActualHealth <= Player.HealthMax && Player.VitalityTotal > Player.PrevVitality)
        {
            Player.ActualHealth += Player.VitalityTotal * 5 - Player.PrevVitality * 5;
        }

        Player.PrevVitality = Player.VitalityTotal;
    }

    // Scale EP with Energy changes
    if (Player.PrevEnergy != Player.EnergyTotal && Player.EP > 0)
    {
        if (Player.EP <= Player.EPMax && Player.EnergyTotal > Player.PrevEnergy)
        {
            Player.EP += Player.EnergyTotal * 5 - Player.PrevEnergy * 5;
        }

        Player.PrevEnergy = Player.EnergyTotal;
    }

    // Status Effect Checking
    if (Player.StatusType[SE_FATIGUE]) // Fatigue
    {
        Player.Speed = Player.Speed * (1.0 / (((fixed)Player.StatusIntensity[SE_FATIGUE] / 1.5) + 1));
        Player.JumpHeight = Player.JumpHeight * (1.0 / (((fixed)Player.StatusIntensity[SE_FATIGUE] / 1.5) + 1));

        if (Player.StatusIntensity[SE_FATIGUE] > 3)
            SetMugShotState("Rampage");
    }

    // Souls checking
    Player.SoulWhiteCount = CheckInventory("DRPGSoulWhiteToken");
    Player.SoulRedCount = CheckInventory("DRPGSoulRedToken") + Player.SoulWhiteCount;
    Player.SoulGreenCount = CheckInventory("DRPGSoulGreenToken") + Player.SoulWhiteCount;
    Player.SoulPinkCount = CheckInventory("DRPGSoulPinkToken") + Player.SoulWhiteCount;
    Player.SoulBlueCount = CheckInventory("DRPGSoulBlueToken") + Player.SoulWhiteCount;
    Player.SoulPurpleCount = CheckInventory("DRPGSoulPurpleToken") + Player.SoulWhiteCount;
    Player.SoulOrangeCount = CheckInventory("DRPGSoulOrangeToken") + Player.SoulWhiteCount;
    Player.SoulDarkBlueCount = CheckInventory("DRPGSoulDarkBlueToken") + Player.SoulWhiteCount;
    Player.SoulYellowCount = CheckInventory("DRPGSoulYellowToken") + Player.SoulWhiteCount;

    Player.SoulsCount = (Player.SoulRedCount + Player.SoulGreenCount + Player.SoulPinkCount + Player.SoulBlueCount + Player.SoulPurpleCount + Player.SoulOrangeCount + Player.SoulDarkBlueCount + Player.SoulYellowCount);

    if (Player.SoulsCount > 0 && Player.Aura.Time <= 0)
    {
        TakeInventory("DRPGSoulRedToken", CheckInventory("DRPGSoulRedToken"));
        TakeInventory("DRPGSoulGreenToken", CheckInventory("DRPGSoulGreenToken"));
        TakeInventory("DRPGSoulWhiteToken", CheckInventory("DRPGSoulWhiteToken"));
        TakeInventory("DRPGSoulPinkToken", CheckInventory("DRPGSoulPinkToken"));
        TakeInventory("DRPGSoulBlueToken", CheckInventory("DRPGSoulBlueToken"));
        TakeInventory("DRPGSoulPurpleToken", CheckInventory("DRPGSoulPurpleToken"));
        TakeInventory("DRPGSoulOrangeToken", CheckInventory("DRPGSoulOrangeToken"));
        TakeInventory("DRPGSoulDarkBlueToken", CheckInventory("DRPGSoulDarkBlueToken"));
        TakeInventory("DRPGSoulYellowToken", CheckInventory("DRPGSoulYellowToken"));
    }

    // DRLA Checking
    if (CompatMode == COMPAT_DRLA)
    {
        if (CheckInventory("RLTacticalBootsToken"))
            Player.SurvivalBonus += 10.0;
        if (PlayerClass(PlayerNumber()) == 1)
            Player.JumpHeight *= 1.25;
    }
}

void CheckRegen()
{
    // Determine the max timer amounts
    Player.HPTime = (int)(350k - ((fixed)Player.RegenerationTotal * 1.575k) - ((fixed)Player.AgilityTimer * 0.025k) * 2k);
    Player.EPTime = (int)(350k - ((fixed)Player.RegenerationTotal * 1.575k) - ((fixed)Player.AgilityTimer * 0.025k) * 2k);

    // Cap Times
    if (Player.HPTime < 35)
        Player.HPTime = 35;
    if (Player.EPTime < 35)
        Player.EPTime = 35;

    // Determine the max regen amounts
    Player.HPAmount = 1 + (Player.RegenerationTotal + Player.VitalityTotal) / 25;
    Player.EPAmount = 1 + (Player.RegenerationTotal + Player.EnergyTotal) / 25;
}

// Regeneration
void DoRegen()
{
    int HPAmount = 1;
    int EPAmount = 1;
    int Overflow = 0;

    // HP Regen
    if (Player.HPRate >= Player.HPTime && ClassifyActor(Player.TID) & ACTOR_ALIVE)
        HealThing(Player.HPAmount);

    // EP Regen
    if (Player.EPRate >= Player.EPTime && Player.EP < Player.EPMax)
    {
        Player.EP += Player.EPAmount;
        Overflow = 0;

        if (Player.EP > Player.EPMax)
        {
            Overflow = Player.EP - Player.EPMax;
            Player.EP = Player.EPMax;
        }

        if (Player.Shield.Active && Player.Shield.Accessory && Player.Shield.Accessory->PassiveEffect == SHIELD_PASS_EPOVERFLOW)
        {
            HealThing(Overflow);
            Player.Shield.Charge += Overflow;
        }
    }

    // Check and Reset timers
    if (Player.HPRate >= Player.HPTime)
        Player.HPRate = 0;
    if (Player.EPRate >= Player.EPTime)
        Player.EPRate = 0;

    // Regen Boost
    if (Player.RegenBoostTimer > 0)
    {
        fixed X = GetActorX(0);
        fixed Y = GetActorY(0);
        fixed Z = GetActorZ(0);
        int Angle = GetActorAngle(0) * 256;
        SpawnForced("DRPGRegenSphereEffect", X, Y, Z + 32.0, AuraTID, Angle);

        Player.HPRate += 1.0 + (fixed)Player.RegenerationTotal / (2.0 + (fixed)Player.RegenerationTotal / 30.0);
        Player.EPRate += 1.0 + (fixed)Player.RegenerationTotal / (2.0 + (fixed)Player.RegenerationTotal / 30.0);
        Player.RegenBoostTimer--;

        // Pass Radius and Height to the Auras for DECORATE usage
        SetUserVariable(AuraTID, "user_radius", (int)GetActorPropertyFixed(Player.TID, APROP_Radius));
        SetUserVariable(AuraTID, "user_height", (int)GetActorPropertyFixed(Player.TID, APROP_Height));
        Thing_ChangeTID(AuraTID, 0);
    }

    // Regeneration Perk - Exponentially increase Health/Energy regeneration rates up to 4x as they lower
    if (Player.Perks[STAT_REGENERATION])
    {
        // Health
        fixed Health = Player.ActualHealth;
        fixed MaxHealth = Player.HealthMax;

        fixed Multiplier = (1.0 - ((fixed)Health / (fixed)MaxHealth)) * 1.77;
        HPAmount += Multiplier * Multiplier;

        // EP
        fixed EP = Abs(Player.EP);
        fixed MaxEP = Player.EPMax;

        Multiplier = (1.0 - ((fixed)EP / (fixed)MaxEP)) * 1.77;
        EPAmount += Multiplier * Multiplier;
    }

    // Movement/Crouching/Idling mechanics
    if (GetCVar("drpg_regen_movement"))
    {
        if (IsPlayerMoving()) // Movement - 50% Regen Rate
        {
            Player.HPRate += HPAmount;
            Player.EPRate += EPAmount;
        }
        else if (CheckInput(BT_CROUCH, KEY_HELD, true, PlayerNumber())) // Crouch - 150% Regen Rate
        {
            Player.HPRate += HPAmount + 3;
            Player.EPRate += EPAmount + 3;
        }
        else // Idle - 100% Regen Rate
        {
            Player.HPRate += HPAmount + 2;
            Player.EPRate += EPAmount + 2;
        }
    }
    else
    {
        Player.HPRate += HPAmount;
        Player.EPRate += EPAmount;
    }
}

void ResetRegen()
{
    // If damaged, reset the Healing timer
    Player.HPRate = 0;
}

// Keep stats capped at certain values
void CheckStatCaps()
{
    // Current Cap (Soft + level)
    if (Player.Strength > Player.StatCap)       Player.Strength = Player.StatCap;
    if (Player.Defense > Player.StatCap)        Player.Defense = Player.StatCap;
    if (Player.Vitality > Player.StatCap)       Player.Vitality = Player.StatCap;
    if (Player.Energy > Player.StatCap)         Player.Energy = Player.StatCap;
    if (Player.Regeneration > Player.StatCap)   Player.Regeneration = Player.StatCap;
    if (Player.Agility > Player.StatCap)        Player.Agility = Player.StatCap;
    if (Player.Capacity > Player.StatCap)       Player.Capacity = Player.StatCap;
    if (Player.Luck > Player.StatCap)           Player.Luck = Player.StatCap;

    // Negative Cap
    if (Player.Strength < -100)                 Player.Strength = -100;
    if (Player.Defense < -100)                  Player.Defense = -100;
    if (Player.Vitality < -100)                 Player.Vitality = -100;
    if (Player.Energy < -100)                   Player.Energy = -100;
    if (Player.Regeneration < -100)             Player.Regeneration = -100;
    if (Player.Agility < -100)                  Player.Agility = -100;
    if (Player.Capacity < -100)                 Player.Capacity = -100;
    if (Player.Luck < -100)                     Player.Luck = -100;
}

// Keep stats capped at the hard value
void CheckHardStatCaps()
{
    HardStatCap = GetCVar("drpg_hard_stat_cap");
    if (Player.Strength > HardStatCap)      Player.Strength = HardStatCap;
    if (Player.Defense > HardStatCap)       Player.Defense = HardStatCap;
    if (Player.Vitality > HardStatCap)      Player.Vitality = HardStatCap;
    if (Player.Energy > HardStatCap)        Player.Energy = HardStatCap;
    if (Player.Regeneration > HardStatCap)  Player.Regeneration = HardStatCap;
    if (Player.Agility > HardStatCap)       Player.Agility = HardStatCap;
    if (Player.Capacity > HardStatCap)      Player.Capacity = HardStatCap;
    if (Player.Luck > HardStatCap)          Player.Luck = HardStatCap;
}

void CheckStatBounds()
{
    // Cap Defense/DamageFactor
    if (Player.DamageFactor < 0.1k)
        Player.DamageFactor = 0.1k;

    // Prevent Mass from underflowing
    if (Player.Mass < 0k)
        Player.Mass = 0k;

    // Cap the max regen timer amounts to 1 second
    if (Player.HPTime < 35)
        Player.HPTime = 35;
    if (Player.EPTime < 35)
        Player.EPTime = 35;

    // Cap Status Effect Resistance
    if (Player.StatusEffectResist > 100.0k)
        Player.StatusEffectResist = 100.0k;

    // Cap Toxicity Regen Bonus
    if (Player.ToxicityRegenBonus > 25)
        Player.ToxicityRegenBonus = 25;

    // Prevent Shield Capacity from under/overflowing
    if (Player.Shield.Charge < 0)
        Player.Shield.Charge = 0;
    if (Player.Shield.Charge > Player.Shield.Capacity)
        Player.Shield.Charge = Player.Shield.Capacity;

    // Cap Shield stats
    if (CheckShieldValid())
    {
        if (Player.Shield.Capacity < 0)
            Player.Shield.Capacity = 0;
        if (Player.Shield.ChargeRate < 0)
            Player.Shield.ChargeRate = 0;
        if (Player.Shield.DelayRate < 1.0k)
            Player.Shield.DelayRate = 1.0k;
    };

    // Cap Weapon Speed
    if (Player.WeaponSpeed < 0k)
        Player.WeaponSpeed = 0k;
    if (Player.WeaponSpeed > 100k)
        Player.WeaponSpeed = 100k;

    // Cap Survival Bonus
    if (Player.SurvivalBonus < 0k)
        Player.SurvivalBonus = 0k;
    if (Player.SurvivalBonus > 75k && (!Player.Shield.Accessory || Player.Shield.Accessory->PassiveEffect != SHIELD_PASS_ROULETTE))
        Player.SurvivalBonus = 75k;

    // Cap chances at 100%
    if (Player.HealthChance > 100k)  Player.HealthChance = 100k;
    if (Player.EPChance > 100k)      Player.EPChance = 100k;
    if (Player.PowerupChance > 100k) Player.PowerupChance = 100k;
    if (Player.ArmorChance > 100k)   Player.ArmorChance = 100k;
    if (Player.WeaponChance > 100k)  Player.WeaponChance = 100k;
    if (Player.ModuleChance > 100k)  Player.ModuleChance = 100k;
    if (Player.AugChance > 100k)     Player.AugChance = 100k;
    if (Player.ShieldChance > 100k)  Player.ShieldChance = 100k;
    if (Player.StimChance > 100k)    Player.StimChance = 100k;

    // Speed capping CVAR
    if (Player.Speed > GetActivatorCVarFixed("drpg_maxspeed"))
        Player.Speed = GetActivatorCVarFixed("drpg_maxspeed");

    // Jump Height capping CVAR
    if (Player.JumpHeight > GetActivatorCVarFixed("drpg_maxjump"))
        Player.JumpHeight = GetActivatorCVarFixed("drpg_maxjump");
}

// Luck Chances
void CheckLuck()
{
    Player.HealthDrop =     (Player.LuckTotal >= LUCK_HEALTHDROP ? true : false);
    Player.EPDrop =         (Player.LuckTotal >= LUCK_EPDROP ? true : false);
    Player.ArmorDrop =      (Player.LuckTotal >= LUCK_ARMORDROP ? true : false);
    Player.WeaponDrop =     (Player.LuckTotal >= LUCK_WEAPONDROP ? true : false);
    Player.PowerupDrop =    (Player.LuckTotal >= LUCK_POWERUPDROP ? true : false);
    Player.StimDrop =       (Player.LuckTotal >= LUCK_STIMDROP ? true : false);
    Player.ModuleDrop =     (Player.LuckTotal >= LUCK_MODULEDROP ? true : false);
    Player.AugDrop =        (Player.LuckTotal >= LUCK_AUGDROP ? true : false);
    Player.ShieldDrop =     (Player.LuckTotal >= LUCK_SHIELDDROP ? true : false);

    Player.HealthChance =   Curve(Player.LuckTotal, LUCK_HEALTHDROP, 1000, LUCK_HEALTHCHANCE, LUCK_MAXHEALTHCHANCE);
    Player.EPChance =       Curve(Player.LuckTotal, LUCK_EPDROP, 1000, LUCK_EPCHANCE, LUCK_MAXEPCHANCE);
    Player.ArmorChance =    Curve(Player.LuckTotal, LUCK_ARMORDROP, 1000, LUCK_ARMORCHANCE, LUCK_MAXARMORCHANCE);
    Player.WeaponChance =   Curve(Player.LuckTotal, LUCK_WEAPONDROP, 1000, LUCK_WEAPONCHANCE, LUCK_MAXWEAPONCHANCE);
    Player.PowerupChance =  Curve(Player.LuckTotal, LUCK_POWERUPDROP, 1000, LUCK_POWERUPCHANCE, LUCK_MAXPOWERUPCHANCE);
    Player.StimChance =     Curve(Player.LuckTotal, LUCK_STIMDROP, 1000, LUCK_STIMCHANCE, LUCK_MAXSTIMCHANCE);
    Player.ModuleChance =   Curve(Player.LuckTotal, LUCK_MODULEDROP, 1000, LUCK_MODULECHANCE, LUCK_MAXMODULECHANCE);
    Player.ShieldChance =   Curve(Player.LuckTotal, LUCK_SHIELDDROP, 1000, LUCK_SHIELDCHANCE, LUCK_MAXSHIELDCHANCE);
    Player.AugChance =      Curve(Player.LuckTotal, LUCK_AUGDROP, 1000, LUCK_AUGCHANCE, LUCK_MAXAUGCHANCE);

    // Hell Unleashed Map Event
    if (CurrentLevel->Event == MAPEVENT_HELLUNLEASHED)
    {
        Player.HealthChance += CurrentLevel->RareAdd;
        Player.EPChance += CurrentLevel->RareAdd;
        Player.ArmorChance += CurrentLevel->RareAdd;
        Player.WeaponChance += CurrentLevel->RareAdd;
        Player.PowerupChance += CurrentLevel->RareAdd;
        Player.StimChance += CurrentLevel->RareAdd;
        Player.ModuleChance += CurrentLevel->RareAdd;
        Player.AugChance += CurrentLevel->RareAdd;
        Player.ShieldChance += CurrentLevel->RareAdd;
    }
}

void CheckBurnout()
{
    // Energy Burnout Effect
    if (Player.EP < 0)
    {
        // Dynamic Intensity
        fixed Intensity = -((fixed)Player.EP / (fixed)Player.EPMax);
        if (Intensity > 0.25) Intensity = 0.25;

        // Screen Effect
        FadeRange(0, 128, 255, Intensity + (Sin(Timer() / 256.0) * 0.1), 0, 128, 255, 0, 0.25);

        // Penalties
        Player.TotalDamage /= 2;
        Player.DamageFactor *= 2;
        Player.Mass /= 2;
        Player.HealthMax /= 2;
        Player.EPMax /= 2;
        Player.Speed /= 2;
        Player.JumpHeight /= 2;
        Player.SurvivalBonus /= 2;

        // Energy Perk
        if (Player.Perks[STAT_ENERGY])
            Player.EPTime /= 2;
    }
}

void CheckPerks()
{
    // If you're dead, return
    if (GetActorProperty(Player.TID, APROP_Health) <= 0) return;

    if (Player.StrengthTotal >= 75)     Player.Perks[STAT_STRENGTH] = true;
    else Player.Perks[STAT_STRENGTH] = false;
    if (Player.DefenseTotal >= 75)      Player.Perks[STAT_DEFENSE] = true;
    else Player.Perks[STAT_DEFENSE] = false;
    if (Player.VitalityTotal >= 75)     Player.Perks[STAT_VITALITY] = true;
    else Player.Perks[STAT_VITALITY] = false;
    if (Player.EnergyTotal >= 50)       Player.Perks[STAT_ENERGY] = true;
    else Player.Perks[STAT_ENERGY] = false;
    if (Player.RegenerationTotal >= 75) Player.Perks[STAT_REGENERATION] = true;
    else Player.Perks[STAT_REGENERATION] = false;
    if (Player.AgilityTotal >= 75)      Player.Perks[STAT_AGILITY] = true;
    else Player.Perks[STAT_AGILITY] = false;
    if (Player.CapacityTotal >= 100)     Player.Perks[STAT_CAPACITY] = true;
    else Player.Perks[STAT_CAPACITY] = false;
    if (Player.LuckTotal >= 100)         Player.Perks[STAT_LUCK] = true;
    else Player.Perks[STAT_LUCK] = false;

    fixed StrengthPercent = ((fixed)Player.ActualHealth / (fixed)Player.HealthMax * 100);
    fixed DefensePercent = ((fixed)Player.ActualHealth / (fixed)Player.HealthMax);

    // Cap Strength Percent
    if (StrengthPercent > 100)
        StrengthPercent = 100;

    // Cap Defense Percent
    if (DefensePercent < 0.01)
        DefensePercent = 0.01;
    if (DefensePercent > 1.0)
        DefensePercent = 1.0;

    // Strength Perk - Exponentially increase Strength as Health Decreases
    if (Player.Perks[STAT_STRENGTH] && StrengthPercent < 100)
        Player.DamageMult += ((100 - StrengthPercent) / 250);

    // Defense Perk - Exponentially increase Defense as Health Decreases
    if (Player.Perks[STAT_DEFENSE] && DefensePercent > 0)
        Player.DamageFactor *= DefensePercent;

    // Vitality Perk - Halve Health regeneration time
    if (Player.Perks[STAT_VITALITY] && Player.ActualHealth < Player.HealthMax / 5 + 1)
        Player.HPTime /= 2;

    // Agility Perk
    if (Player.Perks[STAT_AGILITY])
    {
        // Increment Agility timer to effect regeneration times the longer you are moving
        if (GetActorVelX(Player.TID) != 0 || GetActorVelY(Player.TID) != 0)
            Player.AgilityTimer += 1;
        else
        {
            if (Player.AgilityTimer > 0)
                Player.AgilityTimer -= 25;

            if (Player.AgilityTimer <= 0)
                Player.AgilityTimer = 0;
        }

        // +15% Survival Bonus
        Player.SurvivalBonus += 15;
    }
    else
        Player.AgilityTimer = 0;

    // Capacity Perk - Regenerate ammo
    if (Player.Perks[STAT_CAPACITY] && (Timer() % (35 * 30)) == 0)
    {
        GiveInventory("Clip", GetAmmoAmount("Clip"));
        GiveInventory("Shell", GetAmmoAmount("Shell"));
        GiveInventory("RocketAmmo", GetAmmoAmount("RocketAmmo"));
        GiveInventory("Cell", GetAmmoAmount("Cell"));
    }

    // Luck Perk - Always have Automap/Scanner
    // TODO: Ability to see secrets
    if (Player.Perks[STAT_LUCK])
    {
        GiveInventory("DRPGAllMapRevealer", 1);
    }
}

void CheckToxicity()
{
    // Prevent Underflow
    if (Player.Toxicity < 0)
        Player.Toxicity = 0;

    // Slowly decrease Toxicity
    if (Player.Toxicity > 0 && (Timer() % (35 * (30 - Player.ToxicityRegenBonus)) == 0))
    {
        Player.Toxicity--;
        Player.ToxicTimer = 0;
        Player.ToxicOffset = 0;
        Player.ToxicStage = 0;
    }

    // Toxicity Penalties
    if (GetCVar("drpg_toxicity_penalties"))
    {
        if (Player.Toxicity >= 25 && Timer() % 35 == 0)
        {
            Player.HPRate = 0;
            Player.EPRate = 0;
        }
        if (Player.Toxicity >= 50 && Timer() % 35 == 0 && Player.EP > 0)
            Player.EP--;
        if (Player.Toxicity >= 75)
        {
            Player.Speed /= 2;
            Player.JumpHeight /= 2;
        }
    }

    // Death at 100% Toxicity
    if (Player.Toxicity >= 100)
    {
        SetActorProperty(Player.TID, APROP_Health, 0);
        SetActorState(Player.TID, "Death.Toxicity", false);
        Player.Toxicity = 0;
    }
}

void CheckStimImmunity()
{
    // Prevent Overflow
    if (Player.StimImmunity > 100)
        Player.StimImmunity = 100;

    if (!CurrentLevel->UACBase || ArenaActive || MarinesHostile)
        if ((Timer() % (35 * 6)) == 0 && Player.StimImmunity > 0)
            Player.StimImmunity--;
}

void ToxicityDamage()
{
    // Check Damage Type for Toxicity
    if ((Player.DamageType == DT_TOXIC || Player.DamageType == DT_RADIATION) && !CheckInventory("PowerIronFeet") && !(Player.Shield.Accessory && Player.Shield.Accessory->PassiveEffect == SHIELD_PASS_NOTOXIC && Player.Shield.Active))
        AddToxicity(Random(1, GameSkill()));
}

void CheckStatusEffects()
{
    if (Player.StatusType[SE_BLIND]) // Blind
        FadeRange(0, 0, 0, (0.5 + (Player.StatusIntensity[SE_BLIND] * 0.1)) - (Sin(Timer() / 128.0) * 0.25), 0, 0, 0, 0.0, 0.5);

    if (Player.StatusType[SE_CONFUSION]) // Confusion
    {
        SetHudSize(640, 480, true);
        SetFont(StrParam("P%iVIEW", PlayerNumber() + 1));

        fixed ViewCycle = Timer() / (120.0 - (10.0 * Player.StatusIntensity[SE_CONFUSION]));
        fixed ViewDist = 9.6 * Player.StatusIntensity[SE_CONFUSION] * ((fixed)Player.StatusTimer[SE_CONFUSION] / (fixed)Player.StatusTimerMax[SE_CONFUSION]);

        // View Spinning
        HudMessage("A");
        EndHudMessage(HUDMSG_PLAIN | HUDMSG_LAYER_UNDERHUD | HUDMSG_NOTWITHFULLMAP, CONFUSION_ID + 3, "Untranslated", 320, 240, 0.029);
        HudMessage("A");
        EndHudMessage(HUDMSG_PLAIN | HUDMSG_ALPHA | HUDMSG_LAYER_UNDERHUD | HUDMSG_NOTWITHFULLMAP, CONFUSION_ID, "Untranslated",
                      320 + (int)(Cos(ViewCycle) * ViewDist),
                      240 + (int)(Sin(ViewCycle) * ViewDist),
                      0.029, 0.3);
        HudMessage("A");
        EndHudMessage(HUDMSG_PLAIN | HUDMSG_ALPHA | HUDMSG_LAYER_UNDERHUD | HUDMSG_NOTWITHFULLMAP, CONFUSION_ID + 1, "Untranslated",
                      320 + (int)(Cos(ViewCycle + 0.333) * ViewDist),
                      240 + (int)(Sin(ViewCycle + 0.333) * ViewDist),
                      0.029, 0.6);
        HudMessage("A");
        EndHudMessage(HUDMSG_PLAIN | HUDMSG_ALPHA | HUDMSG_LAYER_UNDERHUD | HUDMSG_NOTWITHFULLMAP, CONFUSION_ID + 2, "Untranslated",
                      320 + (int)(Cos(ViewCycle + 0.667) * ViewDist),
                      240 + (int)(Sin(ViewCycle + 0.667) * ViewDist),
                      0.029, 0.9);

        SetHudSize(0, 0, false);
    }

    if (Player.StatusType[SE_POISON]) // Poison
        if ((Timer() % 35) == 0)
            if (Player.ActualHealth - Player.StatusIntensity[SE_POISON] > 0)
            {
                Player.ActualHealth -= Player.StatusIntensity[SE_POISON];
                FadeRange(0, 255, 0, 0.25, 0, 255, 0, 0.0, Player.StatusIntensity[SE_POISON] * 0.25);
            }

    if (Player.StatusType[SE_CORROSION]) // Corrosion
        if ((Timer() % 35) == 0 && CheckInventory("Armor") > 0)
            if (CheckInventory("Armor") - Player.StatusIntensity[SE_CORROSION] > 0)
            {
                TakeInventory("BasicArmor", Player.StatusIntensity[SE_CORROSION]);
                FadeRange(0, 255, 0, 0.25, 0, 255, 0, 0.0, Player.StatusIntensity[SE_CORROSION] * 0.25);
            }

    if (Player.StatusType[SE_VIRUS]) // Virus
    {
        if (Player.HPRate > 0) Player.HPRate -= 3 + Player.StatusIntensity[SE_VIRUS];
        if (Player.EPRate > 0) Player.EPRate -= 3 + Player.StatusIntensity[SE_VIRUS];
    }

    if (Player.StatusType[SE_SILENCE]) // Silence
        RemoveAura();

    if (Player.StatusType[SE_CURSE]) // Curse
        Player.DamageFactor = Player.DamageFactor * (1.0 * (Player.StatusIntensity[SE_CURSE] + 1));

    if (Player.StatusType[SE_EMP]) // EMP
    {
        if (Player.Shield.Active)
            DeactivateShield();
        if (Player.Augs.SlotsUsed > 0)
            DisableAugs(true);
        if ((Timer() % 35) == 0)
            Player.Augs.Battery -= Player.StatusIntensity[SE_EMP];
    }
    else if (GetActivatorCVar("drpg_augs_autoreactivate"))
        ReactivateDisabledAugs();

    if (Player.StatusType[SE_RADIATION]) // Radiation
    {
        // Geiger Sound Loop
        PlaySound(0, "misc/radiation", 7, 1.0, true, ATTN_NORM);

        if ((Timer() % 70) == 0)
        {
            // Trail Light
            SpawnForced("DRPGRadiationGlow2", GetActorX(0), GetActorY(0), GetActorZ(0) + (GetActorProperty(0, APROP_Height) / 2.0));

            // Poisoning
            if (Player.Toxicity < 85 && !(Player.Shield.Accessory && Player.Shield.Accessory->PassiveEffect == SHIELD_PASS_NOTOXIC && Player.Shield.Active))
            {
                int i = Random(1, Player.StatusIntensity[SE_RADIATION]);
                if (Player.Toxicity + i > 85)
                    i = 85 - Player.Toxicity;
                AddToxicity(i);
                FadeRange(0, 255, 0, 0.25, 0, 255, 0, 0.0, i * 0.25);
            }
        }

        // Actor Light
        SpawnForced("DRPGRadiationGlow", GetActorX(0), GetActorY(0), GetActorZ(0) + (GetActorProperty(0, APROP_Height) / 2.0));
    }

    for (int i = 0; i < SE_MAX; i++)
    {
        // Disable an effect if the timer is 0
        if (Player.StatusType[i] && Player.StatusTimer[i] <= 0)
        {
            Player.StatusType[i] = false;
            Player.StatusIntensity[i] = 0;
            Player.StatusTimer[i] = 0;
            Player.StatusTimerMax[i] = 0;

            if (i == SE_RADIATION)
                StopSound(Player.TID, 7);
        }
        else if (Player.StatusTimer[i] > 0)
            Player.StatusTimer[i] -= 1 + (Player.RegenBoostTimer > 0 ? Player.RegenBoostTimer / 500 : 0);
    }
}

void StatusDamage(int Amount, fixed Chance, bool Critical)
{
    int Type = SE_NONE;
    int StatChance = 0;
    int Intensity = 0;
    int Time = 0;
    int LevelNum = CurrentLevel->LevelNum;

    // Criticals boost chance 2.0x
    if (Critical)
        Chance *= 2.0;

    // We lucked out this time and won't get hit with a status effect
    if (RandomFixed(0.0, 100.0) >= Chance) return;

    // Calculate the intensity
    Intensity = ((LevelNum / 8.0) / (1.0 + (Player.VitalityTotal / 10.0) + (Player.Level / 10.0)));
    if (Intensity < 1)
        Intensity = 1;
    if (Intensity > 5)
        Intensity = 5;

    // Damage Types
    switch (Player.DamageType)
    {
    case DT_TOXIC:
        switch (Random(1, 3))
        {
        case 1:
            Type = SE_POISON;
            Time = Random(1, 10);
            break;
        case 2:
            Type = SE_CORROSION;
            Time = Random(1, 10);
            break;
        case 3:
            Type = SE_RADIATION;
            Time = Random(1, 10);
            break;
        }
        break;
    case DT_RADIATION:
        Type = SE_RADIATION;
        Time = Random(1, 10);
        break;
    case DT_MELEE:
        switch (Random(1, 3))
        {
        case 1:
            Type = SE_BLIND;
            Time = Random(5, 10);
            break;
        case 2:
            Type = SE_CONFUSION;
            Time = Random(5, 10);
            break;
        case 3:
            Type = SE_FATIGUE;
            Time = Random(5, 30);
            break;
        }
        break;
    case DT_PLASMA:
        switch (Random(1, 5))
        {
        case 1:
            Type = SE_POISON;
            Time = Random(5, 20);
            break;
        case 2:
            Type = SE_CORROSION;
            Time = Random(5, 20);
            break;
        case 3:
            Type = SE_VIRUS;
            Time = Random(60, 180);
            break;
        case 4:
            Type = SE_SILENCE;
            Intensity = 1; // Only has one level of intensity
            Time = Random(30, 90);
            break;
        case 5:
            Type = SE_CURSE;
            Time = Random(10, 60);
            break;
        }
        break;
    case DT_LIGHTNING:
        switch (Random(1, 4))
        {
        case 1:
            Type = SE_BLIND;
            Time = Random(5, 10);
            break;
        case 2:
            Type = SE_SILENCE;
            Intensity = 1; // Only has one level of intensity
            Time = Random(30, 90);
            break;
        case 3:
            Type = SE_EMP;
            Time = Random(5, 30);
            break;
        case 4:
            Type = SE_RADIATION;
            Time = Random(5, 20);
            break;
        }
        break;
    case DT_FIRE:
        switch (Random(1, 2))
        {
        case 1:
            Type = SE_BLIND;
            Time = Random(5, 10);
            break;
        case 2:
            Type = SE_FATIGUE;
            Time = Random(5, 30);
            break;
        }
        break;
    }

    if (Type >= 0)
        TryStatusEffect(Type, Time, Intensity);
}
