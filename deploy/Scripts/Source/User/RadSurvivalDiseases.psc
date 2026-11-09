ScriptName RadSurvivalDiseases extends Quest

float Property HourlyRisk = 0.012 Auto
float Property RiskPerWeakStage = 0.008 Auto
float Property RiskPerHit = 0.01 Auto
float Property RiskPerSwim = 0.03 Auto
float Property RiskPerRadHour = 0.08 Auto
float Property RiskPerRainHour = 0.02 Auto
float Property SwimIntervalGameDays = 0.1 Auto
float Property HitCooldownSeconds = 2.0 Auto
float Property IncubationHours = 4.0 Auto
float Property Stage2Hours = 10.0 Auto
float Property Stage3Hours = 24.0 Auto
float Property RestCureBaseHours = 4.0 Auto
float Property ImmunityHours = 24.0 Auto
bool Property DiseasesEnabled = true Auto Const
int Property kTuneDiseases = 7 Auto Const
int Property kTuneBaseRisk = 8 Auto Const
int Property kTuneRadWeather = 9 Auto Const
int Property kTuneRain = 10 Auto Const
int Property kTuneWound = 11 Auto Const
int Property kTuneSwim = 12 Auto Const
int Property kTuneTaintedFood = 13 Auto Const
int Property kTuneTaintedWater = 14 Auto Const
int Property kTuneContaminated = 15 Auto Const
bool Property ShowNotifications = true Auto Const
int Property CurrentStateVersion = 2 Auto Const
string Property DiseasePlugin = "RadSurvival.esp" Auto Const
int Property FeverSpellBase = 2076 Auto Const
int Property WastingSpellBase = 2079 Auto Const
string Property StormWeatherPlugin = "RadiationOverhaul.esp" Auto Const
int Property StormWeatherBase = 2112 Auto Const
int Property StormWeatherCount = 5 Auto Const
int Property VanillaRadStormFormId = 1850718 Auto Const
int Property RainClassification = 2 Auto Const

GlobalVariable Property RadSurvival_Risk Auto
GlobalVariable Property RadSurvival_Disease Auto

FormList Property TaintedFoodList Auto
FormList Property TaintedWaterList Auto
FormList Property ContaminatedList Auto
Keyword Property KeywordTaintedFood Auto
Keyword Property KeywordTaintedWater Auto
Keyword Property KeywordContaminated Auto
Keyword Property KeywordCureItem Auto
float Property RiskPerTaintedFood = 0.10 Auto
float Property RiskPerTaintedWater = 0.15 Auto
float Property RiskPerContaminated = 0.05 Auto
int Property CureItemFormId = 2092 Auto Const
int Property FeverPerkBase = 2093 Auto Const
int Property WastingPerkBase = 2096 Auto Const
int Property RadAwayFormId = 145218 Auto Const
int Property MaxDiseaseId = 7 Auto Const
int Property IllnessSpellBase = 2114 Auto Const
int Property IllnessPerkBase = 2129 Auto Const
int Property PurifiedWaterFormId = 222912 Auto Const
int Property MedXFormId = 210809 Auto Const
int Property StimpakFormId = 145206 Auto Const
int Property RefreshingBeverageFormId = 363192 Auto Const
int Property MeleeOneHandKeywordId = 303268 Auto Const
int Property MeleeTwoHandKeywordId = 303269 Auto Const
float Property DysenteryChance = 0.20 Auto
float Property FoodPoisoningChance = 0.25 Auto
float Property TetanusChance = 0.04 Auto
float Property WoundInfectionChance = 0.05 Auto
float Property PneumoniaChancePerHour = 0.02 Auto
float Property WoundHealthFraction = 0.5 Auto
float Property FoodPoisoningClearHours = 24.0 Auto
float Property LethalDelayHours = 24.0 Auto
float Property TerminalHours = 12.0 Auto
float Property TerminalWarnHours = 3.0 Auto
float Property TerminalDrainPerHour = 0.03 Auto

int DiseaseId = 0
int ColdStage = 0
int DiseaseStage = 0
int DiseaseCount = 0
int SavedStateVersion = 0
int TerminalPhase = 0
float DiseaseProgress = 0.0
float RiskDebt = 0.0
float ImmunityUntil = -1.0
float LastHitRealTime = -1.0
float NextSwimAllowed = 0.0
Spell AppliedDiseaseSpell = None
Perk AppliedDiseasePerk = None
Form[] RadWeatherForms

float Function Tuned(int aiKey, float afDefault)
    float raw = RadNatives.TuningOverride(aiKey)
    if raw > 0.0
        return raw - 1.0
    endif
    return afDefault
EndFunction

bool Function DiseasesOn()
    return DiseasesEnabled && Tuned(kTuneDiseases, 1.0) >= 0.5
EndFunction

float Function CauseScale()
    float raw = RadNatives.TuningByName("DiseaseCauseScale")
    if raw > 0.0
        return raw - 1.0
    endif
    return 1.0
EndFunction

float Function TunedByName(string asKey, float afDefault)
    float raw = RadNatives.TuningByName(asKey)
    if raw > 0.0
        return raw - 1.0
    endif
    return afDefault
EndFunction

bool Function LethalOn()
    return DiseasesOn() && TunedByName("DiseaseLethal", 1.0) >= 0.5
EndFunction

float Function LethalDelay()
    return TunedByName("DiseaseLethalHours", LethalDelayHours)
EndFunction

Function Tick(float afHours, bool abActive, int aiHungerStage, int aiThirstStage, int aiSleepStage)
    AccrueRisk(afHours, abActive, aiHungerStage, aiThirstStage, aiSleepStage)
    RollCold(afHours, abActive)
    AdvanceDisease(afHours, abActive)
EndFunction

Function SetColdStage(int aiStage)
    ColdStage = aiStage
EndFunction

float Function ThirstScale()
    if DiseaseId != 3 || DiseaseStage <= 0
        return 1.0
    elseif DiseaseStage == 1
        return 1.25
    elseif DiseaseStage == 2
        return 1.5
    endif
    return 2.0
EndFunction

bool Function Roll(float afChance)
    float chance = afChance * CauseScale()
    if chance <= 0.0
        return false
    endif
    return Utility.RandomFloat(0.0, 1.0) < chance
EndFunction

Function RollCause(int aiDisease, float afChance, bool abActive)
    if !abActive || !DiseasesOn() || DiseaseId > 0 || IsImmune()
        return
    endif
    if Roll(afChance)
        StartSpecific(aiDisease, abActive)
    endif
EndFunction

Function RollCold(float afHours, bool abActive)
    if ColdStage < 2 || afHours <= 0.0
        return
    endif
    float chance = PneumoniaChancePerHour * ((ColdStage - 1) as float) * afHours
    if chance > 1.0
        chance = 1.0
    endif
    RollCause(6, chance, abActive)
EndFunction

Function RollWound(Form akSource, Actor akPlayer, bool abActive)
    if akSource && IsMeleeWeapon(akSource)
        RollCause(5, TetanusChance, abActive)
    endif
    if akPlayer && akPlayer.GetValuePercentage(Game.GetHealthAV()) < WoundHealthFraction
        RollCause(7, WoundInfectionChance, abActive)
    endif
EndFunction

bool Function IsMeleeWeapon(Form akSource)
    if !(akSource as Weapon)
        return false
    endif
    Keyword oneHand = Game.GetFormFromFile(MeleeOneHandKeywordId, "Fallout4.esm") as Keyword
    Keyword twoHand = Game.GetFormFromFile(MeleeTwoHandKeywordId, "Fallout4.esm") as Keyword
    return (oneHand && akSource.HasKeyword(oneHand)) || (twoHand && akSource.HasKeyword(twoHand))
EndFunction

float Function IncubationFor(int aiDisease)
    if aiDisease == 3
        return 8.0
    elseif aiDisease == 4
        return 2.0
    elseif aiDisease == 5
        return 24.0
    elseif aiDisease == 6
        return 12.0
    elseif aiDisease == 7
        return 6.0
    endif
    return IncubationHours
EndFunction

float Function Stage2For(int aiDisease)
    if aiDisease == 3
        return 20.0
    elseif aiDisease == 4
        return 6.0
    elseif aiDisease == 5
        return 48.0
    elseif aiDisease == 6
        return 30.0
    elseif aiDisease == 7
        return 16.0
    endif
    return Stage2Hours
EndFunction

float Function Stage3For(int aiDisease)
    if aiDisease == 3
        return 40.0
    elseif aiDisease == 4
        return 12.0
    elseif aiDisease == 5
        return 72.0
    elseif aiDisease == 6
        return 54.0
    elseif aiDisease == 7
        return 30.0
    endif
    return Stage3Hours
EndFunction

float Function ProgressForStage(int aiDisease, int aiStage)
    if aiStage == 1
        return IncubationFor(aiDisease)
    elseif aiStage == 2
        return Stage2For(aiDisease)
    elseif aiStage >= 3
        return Stage3For(aiDisease)
    endif
    return 0.0
EndFunction

Function DebugSetDisease(int aiDisease, int aiStage, bool abActive)
    if aiDisease < 0 || aiDisease > MaxDiseaseId
        return
    endif
    DiseaseId = aiDisease
    DiseaseStage = 0
    DiseaseProgress = 0.0
    TerminalPhase = 0
    if aiDisease > 0
        DiseaseStage = aiStage
        if DiseaseStage < 0
            DiseaseStage = 0
        elseif DiseaseStage > 3
            DiseaseStage = 3
        endif
        DiseaseProgress = ProgressForStage(aiDisease, DiseaseStage)
    endif
    RiskDebt = 0.0
    Reconcile(abActive)
    MirrorRisk()
    MirrorDisease()
EndFunction

Function Resync(bool abActive)
    EnsureStateVersion()
    if DiseaseId < 0 || DiseaseId > MaxDiseaseId
        DiseaseId = 0
    endif
    if DiseaseStage < 0
        DiseaseStage = 0
    elseif DiseaseStage > 3
        DiseaseStage = 3
    endif
    if DiseaseProgress < 0.0
        DiseaseProgress = 0.0
    endif
    RiskDebt = Clamp01(RiskDebt)
    RadWeatherForms = new Form[0]
    if NextSwimAllowed < 0.0
        NextSwimAllowed = 0.0
    endif
    Reconcile(abActive)
    MirrorRisk()
    MirrorDisease()
    TraceTaintLists()
EndFunction

Function Reconcile(bool abActive)
    Spell wanted = None
    if abActive && DiseasesOn() && DiseaseId > 0 && DiseaseStage > 0
        wanted = DiseaseSpellFor(DiseaseId, DiseaseStage)
    endif
    Perk wantedPerk = None
    if wanted && DiseaseId > 0 && DiseaseStage > 0
        wantedPerk = DiseasePerkFor(DiseaseId, DiseaseStage)
    endif
    ApplySpell(wanted)
    ApplyPerk(wantedPerk)
EndFunction

Function TryRestCure(float afHours, bool abInterrupted, bool abActive)
    if DiseaseId <= 0 || abInterrupted || !abActive
        return
    endif
    if afHours < RestCureBaseHours + (DiseaseStage as float)
        return
    endif
    if DiseaseId == 5 && DiseaseStage > 1
        NotifyDisease("tetanus needs Med-X, rest is not enough")
        return
    endif
    if DiseaseId == 6 && ColdStage > 0
        NotifyDisease("too cold to shake pneumonia, get warm first")
        return
    endif
    CureDisease()
EndFunction

Function AccrueRisk(float afHours, bool abActive, int aiHungerStage, int aiThirstStage, int aiSleepStage)
    if !abActive || !DiseasesOn() || DiseaseId > 0 || afHours <= 0.0
        return
    endif

    if IsImmune()
        RiskDebt = Clamp01(RiskDebt - Tuned(kTuneBaseRisk, HourlyRisk) * afHours)
        MirrorRisk()
        return
    endif

    float hourly = Tuned(kTuneBaseRisk, HourlyRisk) + RiskPerWeakStage * ((aiHungerStage + aiThirstStage + aiSleepStage) as float)
    RiskDebt = Clamp01(RiskDebt + hourly * afHours)
    MirrorRisk()
    if RiskDebt >= 1.0
        StartDisease(abActive)
    endif
EndFunction

bool Function QualifiesAsWound(ObjectReference akTarget, ObjectReference akAggressor, Form akSource, Actor akPlayer)
    Actor hurt = akTarget as Actor
    if !akPlayer || !hurt || hurt != akPlayer
        return false
    endif
    Actor attacker = akAggressor as Actor
    if !attacker || attacker == akPlayer
        return false
    endif
    if akSource as Spell
        return false
    endif
    return true
EndFunction

Function ExposureHit(Form akSource, Actor akPlayer, bool abActive)
    float now = Utility.GetCurrentRealTime()
    if LastHitRealTime > 0.0 && now - LastHitRealTime < HitCooldownSeconds
        return
    endif
    LastHitRealTime = now
    RollWound(akSource, akPlayer, abActive)
    AddRisk(Tuned(kTuneWound, RiskPerHit), abActive)
EndFunction

Function ExposureSwim(bool abActive)
    float now = Utility.GetCurrentGameTime()
    if NextSwimAllowed > 0.0 && now < NextSwimAllowed
        return
    endif
    NextSwimAllowed = now + SwimIntervalGameDays
    AddRisk(Tuned(kTuneSwim, RiskPerSwim), abActive)
EndFunction

Function ExposureWeather(float afHours, bool abActive)
    if afHours <= 0.0 || !abActive || !DiseasesOn() || DiseaseId > 0
        return
    endif
    Actor player = Game.GetPlayer()
    if !player || player.IsInInterior()
        return
    endif
    Form sky = Weather.GetCurrentWeather() as Form
    if !sky
        return
    endif
    if IsRadWeather(sky)
        AddRisk(Tuned(kTuneRadWeather, RiskPerRadHour) * afHours, abActive)
        return
    endif
    Weather current = sky as Weather
    if current && current.GetClassification() >= RainClassification
        AddRisk(Tuned(kTuneRain, RiskPerRainHour) * afHours, abActive)
    endif
EndFunction

Function ConsumeItem(Form akItem, int aiCount, bool abActive)
    if !akItem || aiCount <= 0
        return
    endif
    if CuresOutright(akItem)
        if DiseaseId > 0
            CureDisease()
        elseif IsFormFrom(akItem, CureItemFormId, DiseasePlugin)
            NotifyDisease("nothing to cure")
        endif
        return
    endif
    if DiseaseId > 0 && TreatsDisease(akItem, DiseaseId)
        TreatStep(abActive)
        return
    endif
    ExposureConsume(akItem, aiCount, abActive)
EndFunction

bool Function IsFormFrom(Form akItem, int aiFormId, string asPlugin)
    Form wanted = Game.GetFormFromFile(aiFormId, asPlugin)
    return wanted && akItem == wanted
EndFunction

bool Function CuresOutright(Form akItem)
    if !akItem || !(akItem as Potion)
        return false
    endif
    if IsFormFrom(akItem, CureItemFormId, DiseasePlugin) || IsFormFrom(akItem, RefreshingBeverageFormId, "Fallout4.esm")
        return true
    endif
    if IsFormFrom(akItem, RadAwayFormId, "Fallout4.esm")
        return DiseaseId <= 2
    endif
    return KeywordCureItem && akItem.HasKeyword(KeywordCureItem)
EndFunction

bool Function TreatsDisease(Form akItem, int aiDisease)
    if aiDisease == 3 || aiDisease == 4
        return IsFormFrom(akItem, PurifiedWaterFormId, "Fallout4.esm")
    elseif aiDisease == 5
        return IsFormFrom(akItem, MedXFormId, "Fallout4.esm")
    elseif aiDisease == 7
        return IsFormFrom(akItem, StimpakFormId, "Fallout4.esm")
    endif
    return false
EndFunction

bool Function IsCureItem(Form akItem)
    if !akItem || !(akItem as Potion)
        return false
    endif
    if CuresOutright(akItem) || IsFormFrom(akItem, RadAwayFormId, "Fallout4.esm")
        return true
    endif
    return TreatsDisease(akItem, 3) || TreatsDisease(akItem, 5) || TreatsDisease(akItem, 7)
EndFunction

Function TreatStep(bool abActive)
    if DiseaseStage <= 1
        CureDisease()
        return
    endif
    DiseaseStage -= 1
    DiseaseProgress = ProgressForStage(DiseaseId, DiseaseStage)
    TerminalPhase = 0
    Reconcile(abActive)
    MirrorDisease()
    NotifyDisease(DiseaseLabel(DiseaseId) + " eased to stage " + DiseaseStage)
EndFunction

int Function ExposureCategory(Form akItem)
    if ListHas(TaintedFoodList, akItem) || akItem.HasKeyword(KeywordTaintedFood)
        return 1
    endif
    if ListHas(TaintedWaterList, akItem) || akItem.HasKeyword(KeywordTaintedWater)
        return 2
    endif
    if ListHas(ContaminatedList, akItem) || akItem.HasKeyword(KeywordContaminated)
        return 3
    endif
    return 0
EndFunction

bool Function ListHas(FormList akList, Form akItem)
    if !akList || !akItem
        return false
    endif
    return akList.HasForm(akItem)
EndFunction

float Function CategoryRisk(int aiCategory)
    if aiCategory == 1
        return Tuned(kTuneTaintedFood, RiskPerTaintedFood)
    elseif aiCategory == 2
        return Tuned(kTuneTaintedWater, RiskPerTaintedWater)
    elseif aiCategory == 3
        return Tuned(kTuneContaminated, RiskPerContaminated)
    endif
    return 0.0
EndFunction

Function ExposureConsume(Form akItem, int aiCount, bool abActive)
    if aiCount <= 0 || !abActive || !DiseasesOn() || DiseaseId > 0
        return
    endif
    int category = ExposureCategory(akItem)
    if category <= 0
        return
    endif
    if category == 1
        RollCause(4, FoodPoisoningChance, abActive)
    elseif category == 2
        RollCause(3, DysenteryChance, abActive)
    endif
    AddRisk(CategoryRisk(category) * (aiCount as float), abActive)
    Debug.Trace("[RadSurvivalDiseases] tainted " + akItem + " x" + aiCount + " category " + category)
EndFunction

Function TraceTaintLists()
    if !TaintedFoodList || !TaintedWaterList || !ContaminatedList
        Debug.Trace("[RadSurvivalDiseases] a taint form list is unbound; that category charges nothing")
    endif
EndFunction

bool Function IsRadWeather(Form akWeather)
    if !akWeather
        return false
    endif
    Form[] weathers = RadWeatherForms
    if !weathers || weathers.Length == 0
        weathers = ResolveRadWeather()
    endif
    int i = 0
    while i < weathers.Length
        if weathers[i] == akWeather
            return true
        endif
        i += 1
    endwhile
    return false
EndFunction

Form[] Function ResolveRadWeather()
    Form[] resolved = new Form[0]
    Weather vanilla = Game.GetFormFromFile(VanillaRadStormFormId, "Fallout4.esm") as Weather
    if vanilla
        resolved.Add(vanilla)
    else
        Debug.Trace("[RadSurvivalDiseases] missing vanilla rad weather " + VanillaRadStormFormId)
    endif
    int i = 0
    while i < StormWeatherCount
        Weather storm = Game.GetFormFromFile(StormWeatherBase + i, StormWeatherPlugin) as Weather
        if storm
            resolved.Add(storm)
        endif
        i += 1
    endwhile
    if resolved.Length == 0
        Debug.Trace("[RadSurvivalDiseases] no rad weather resolved from " + StormWeatherPlugin)
        return resolved
    endif
    RadWeatherForms = resolved
    return resolved
EndFunction

Function AddRisk(float afAmount, bool abActive)
    if afAmount <= 0.0 || !abActive || !DiseasesOn() || DiseaseId > 0
        return
    endif
    if IsImmune()
        return
    endif
    RiskDebt = Clamp01(RiskDebt + afAmount)
    MirrorRisk()
    if RiskDebt >= 1.0
        StartDisease(abActive)
    endif
EndFunction

bool Function IsImmune()
    if ImmunityUntil < 0.0
        return false
    endif
    return Utility.GetCurrentGameTime() < ImmunityUntil
EndFunction

float Function Clamp01(float afValue)
    if afValue < 0.0
        return 0.0
    elseif afValue > 1.0
        return 1.0
    endif
    return afValue
EndFunction

Function StartDisease(bool abActive)
    DiseaseId = 1
    if DiseaseCount % 2 == 1
        DiseaseId = 2
    endif
    DiseaseCount += 1
    DiseaseStage = 0
    DiseaseProgress = 0.0
    RiskDebt = 0.0
    MirrorRisk()
    MirrorDisease()
    Reconcile(abActive)
    NotifyDisease(DiseaseLabel(DiseaseId) + ": something is wrong, rest before it takes hold")
EndFunction

Function StartSpecific(int aiDisease, bool abActive)
    DiseaseId = aiDisease
    DiseaseCount += 1
    DiseaseStage = 0
    DiseaseProgress = 0.0
    MirrorDisease()
    Reconcile(abActive)
    NotifyDisease(DiseaseLabel(DiseaseId) + ": you caught something, treat it before it takes hold")
    Debug.Trace("[RadSurvivalDiseases] caught " + DiseaseLabel(DiseaseId))
EndFunction

Function AdvanceDisease(float afHours, bool abActive)
    if !abActive || !DiseasesOn() || DiseaseId <= 0 || afHours <= 0.0
        return
    endif

    DiseaseProgress += afHours
    int before = DiseaseStage
    if DiseaseId == 4 && DiseaseProgress >= FoodPoisoningClearHours
        CureDisease()
        return
    endif
    if DiseaseStage == 0 && DiseaseProgress >= IncubationFor(DiseaseId)
        DiseaseStage = 1
    elseif DiseaseStage == 1 && DiseaseProgress >= Stage2For(DiseaseId)
        DiseaseStage = 2
    elseif DiseaseStage == 2 && DiseaseProgress >= Stage3For(DiseaseId)
        DiseaseStage = 3
    endif

    if DiseaseStage != before
        Reconcile(abActive)
        MirrorDisease()
        NotifyDisease(DiseaseLabel(DiseaseId) + " stage " + DiseaseStage)
        Debug.Trace("[RadSurvivalDiseases] stage " + DiseaseStage + " progress " + DiseaseProgress)
    elseif DiseaseStage == 0
        MirrorDisease()
    endif
    AdvanceTerminal(afHours)
EndFunction

Function AdvanceTerminal(float afHours)
    float start = Stage3For(DiseaseId) + LethalDelay()
    if DiseaseStage < 3 || DiseaseId == 4 || !LethalOn() || DiseaseProgress < start
        TerminalPhase = 0
        return
    endif
    string label = DiseaseLabel(DiseaseId)
    float warn = start + TerminalHours - TerminalWarnHours
    if TerminalPhase < 1
        TerminalPhase = 1
        DiseaseProgress = start
        Debug.Notification(label + " is killing you, cure it now")
        Debug.Trace("[RadSurvivalDiseases] terminal " + label)
        return
    endif
    if TerminalPhase < 2 && DiseaseProgress >= warn
        TerminalPhase = 2
        DiseaseProgress = warn
        Debug.Notification(label + ": you are close to death, cure it now")
        return
    endif
    if RadNatives.IsGodMode()
        if DiseaseProgress > warn
            DiseaseProgress = warn
        endif
        return
    endif
    Actor player = Game.GetPlayer()
    if !player
        return
    endif
    float finish = start + TerminalHours
    if TerminalPhase >= 2 && DiseaseProgress >= finish && DiseaseProgress - afHours < finish
        DiseaseProgress = finish
    elseif TerminalPhase >= 2 && DiseaseProgress >= finish
        Debug.Notification("You died of " + label)
        Debug.Trace("[RadSurvivalDiseases] killed by " + label)
        player.Kill()
        return
    endif
    ActorValue health = Game.GetHealthAV()
    float t = Clamp01((DiseaseProgress - start) / TerminalHours)
    float drain = player.GetBaseValue(health) * TerminalDrainPerHour * (1.0 + 3.0 * t) * afHours
    float room = player.GetValue(health) - 1.0
    if drain > room
        drain = room
    endif
    if drain > 0.0
        player.DamageValue(health, drain)
    endif
EndFunction

Function CureDisease()
    if DiseaseId <= 0
        return
    endif
    string label = DiseaseLabel(DiseaseId)
    ImmunityUntil = Utility.GetCurrentGameTime() + ImmunityHours / 24.0
    DiseaseId = 0
    DiseaseStage = 0
    DiseaseProgress = 0.0
    TerminalPhase = 0
    RiskDebt = 0.0
    ApplySpell(None)
    ApplyPerk(None)
    MirrorRisk()
    MirrorDisease()
    NotifyDisease("recovered from " + label + ", but vulnerable for a while")
EndFunction

Function ApplySpell(Spell akWanted)
    Actor player = Game.GetPlayer()
    if !player
        return
    endif
    if AppliedDiseaseSpell == akWanted && (!akWanted || player.HasSpell(akWanted))
        return
    endif
    if AppliedDiseaseSpell
        player.RemoveSpell(AppliedDiseaseSpell)
    endif
    if akWanted
        player.AddSpell(akWanted, false)
    endif
    AppliedDiseaseSpell = akWanted
    Debug.Trace("[RadSurvivalDiseases] ability -> " + akWanted)
EndFunction

Function ApplyPerk(Perk akWanted)
    Actor player = Game.GetPlayer()
    if !player
        return
    endif
    if AppliedDiseasePerk == akWanted && (!akWanted || player.HasPerk(akWanted))
        return
    endif
    if AppliedDiseasePerk
        player.RemovePerk(AppliedDiseasePerk)
    endif
    if akWanted
        player.AddPerk(akWanted, false)
    endif
    AppliedDiseasePerk = akWanted
EndFunction

Perk Function DiseasePerkFor(int aiDisease, int aiStage)
    int first = 0
    if aiDisease == 1
        first = FeverPerkBase
    elseif aiDisease == 2
        first = WastingPerkBase
    elseif aiDisease >= 3 && aiDisease <= MaxDiseaseId
        first = IllnessPerkBase + (aiDisease - 3) * 3
    else
        return None
    endif
    if aiStage < 1 || aiStage > 3
        return None
    endif
    Form perkForm = Game.GetFormFromFile(first + aiStage - 1, DiseasePlugin)
    return perkForm as Perk
EndFunction

Spell Function DiseaseSpellFor(int aiDisease, int aiStage)
    int first = 0
    if aiDisease == 1
        first = FeverSpellBase
    elseif aiDisease == 2
        first = WastingSpellBase
    elseif aiDisease >= 3 && aiDisease <= MaxDiseaseId
        first = IllnessSpellBase + (aiDisease - 3) * 3
    else
        return None
    endif
    if aiStage < 1
        return None
    endif
    if aiStage > 3
        aiStage = 3
    endif
    Spell resolved = Game.GetFormFromFile(first + aiStage - 1, DiseasePlugin) as Spell
    if !resolved
        Debug.Trace("[RadSurvivalDiseases] missing ability " + (first + aiStage - 1) + " in " + DiseasePlugin)
    endif
    return resolved
EndFunction

string Function DiseaseLabel(int aiDisease)
    if aiDisease == 1
        return "fever"
    elseif aiDisease == 2
        return "wasting"
    elseif aiDisease == 3
        return "dysentery"
    elseif aiDisease == 4
        return "food poisoning"
    elseif aiDisease == 5
        return "tetanus"
    elseif aiDisease == 6
        return "pneumonia"
    elseif aiDisease == 7
        if DiseaseStage >= 3
            return "sepsis"
        endif
        return "wound infection"
    endif
    return "illness"
EndFunction

Function NotifyDisease(string asText)
    if !ShowNotifications
        return
    endif
    Debug.Notification("Rad survival: " + asText)
EndFunction

Function EnsureStateVersion()
    if SavedStateVersion >= CurrentStateVersion
        return
    endif
    if AppliedDiseaseSpell
        Actor player = Game.GetPlayer()
        if player
            player.RemoveSpell(AppliedDiseaseSpell)
        endif
    endif
    DiseaseId = 0
    DiseaseStage = 0
    DiseaseCount = 0
    DiseaseProgress = 0.0
    RiskDebt = 0.0
    ImmunityUntil = -1.0
    AppliedDiseaseSpell = None
    SavedStateVersion = CurrentStateVersion
EndFunction

Function MirrorRisk()
    MirrorToGlobal(RadSurvival_Risk, RiskDebt)
EndFunction

Function MirrorDisease()
    float packed = 0.0
    if DiseaseId > 0
        packed = (DiseaseId * 10 + DiseaseStage) as float
        float incubation = IncubationFor(DiseaseId)
        if DiseaseStage == 0 && incubation > 0.0
            packed += Clamp01(DiseaseProgress / incubation) * 0.45
        endif
    endif
    MirrorToGlobal(RadSurvival_Disease, packed)
EndFunction

Function MirrorToGlobal(GlobalVariable akTarget, float afValue)
    if akTarget
        akTarget.SetValue(afValue)
    endif
EndFunction
