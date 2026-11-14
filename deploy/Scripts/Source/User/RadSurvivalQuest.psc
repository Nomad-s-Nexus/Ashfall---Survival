ScriptName RadSurvivalQuest extends Quest

float Property HourlyHunger = 0.05 Auto
float Property HourlyThirst = 0.10 Auto
float Property HourlySleepDebt = 0.025 Auto
float Property SleepRecoveryPerHour = 0.12 Auto
float Property InterruptedScale = 0.5 Auto
float Property FoodScale = 0.20 Auto
float Property DrinkScale = 0.25 Auto
int Property kTuneRate = 0 Auto Const
int Property kTuneHunger = 1 Auto Const
int Property kTuneThirst = 2 Auto Const
int Property kTuneSleep = 3 Auto Const
int Property kTuneFood = 4 Auto Const
int Property kTuneDrink = 5 Auto Const
int Property kTuneSleepRecovery = 6 Auto Const
float Property AlcoholHydrationScale = 0.25 Auto
float Property TickGameHours = 0.25 Auto
float Property MaxElapsedHours = 240.0 Auto
float Property PairWindowSeconds = 2.0 Auto
float Property StageHysteresis = 0.04 Auto
int Property kTimerID = 10 Auto Const
int Property kVanillaSurvivalDifficulty = 6 Auto Const

bool Property Enabled = true Auto Const
bool Property SkipWhenVanillaSurvival = true Auto Const
bool Property ShowNotifications = true Auto Const

bool Property PenaltiesEnabled = true Auto Const
string Property PenaltyPlugin = "RadSurvival.esp" Auto Const
int Property HungerPenaltySpellBase = 2061 Auto Const
int Property ThirstPenaltySpellBase = 2064 Auto Const
int Property SleepPenaltySpellBase = 2067 Auto Const
int Property ColdSpellBase = 2148 Auto Const
int Property ColdPerkBase = 2153 Auto Const
int Property HotFoodListFormId = 2158 Auto Const
float Property HotFoodWarmHours = 2.0 Auto

GlobalVariable Property RadSurvival_Hunger Auto
GlobalVariable Property RadSurvival_Thirst Auto
GlobalVariable Property RadSurvival_Sleep Auto

RadSurvivalDiseases Property Diseases Auto

float HungerDebt = 0.0
float ThirstDebt = 0.0
float SleepDebt = 0.0
float LastGameTime = -1.0
float SleepStartTime = -1.0
float WaitStartTime = -1.0
bool ConsumptionArraysReady = false
Form[] EquipCandidateForms
float[] EquipCandidateTimes
Form[] RemoveCandidateForms
float[] RemoveCandidateTimes

int HungerStage = 0
int ThirstStage = 0
int SleepStage = 0

Spell AppliedHungerSpell = None
Spell AppliedThirstSpell = None
Spell AppliedSleepSpell = None
Spell AppliedColdSpell = None
Perk AppliedColdPerk = None
int ColdStage = 0

int CachedHungerStage = -1
Spell CachedHungerSpell = None
int CachedThirstStage = -1
Spell CachedThirstSpell = None
int CachedSleepStage = -1
Spell CachedSleepSpell = None

Event OnInit()
    Actor player = Game.GetPlayer()
    if !player
        return
    endif

    LastGameTime = Utility.GetCurrentGameTime()
    ClearConsumptionCandidates()
    ArmSleepWaitEvents()
    ArmPlayerEvents(player)
    MirrorAll()
    ResyncPenalties()
    ResyncDiseases()
    RestartTimer()
    Debug.Trace("[RadSurvivalQuest] ready at game day " + LastGameTime)
EndEvent

Function ArmSleepWaitEvents()
    UnregisterForPlayerSleep()
    UnregisterForPlayerWait()
    RegisterForPlayerSleep()
    RegisterForPlayerWait()
EndFunction

Function ArmPlayerEvents(Actor player)
    UnregisterForRemoteEvent(player, "OnItemEquipped")
    RegisterForRemoteEvent(player, "OnItemEquipped")
    RemoveAllInventoryEventFilters()
    AddInventoryEventFilter(None)
    UnregisterForRemoteEvent(player, "OnItemRemoved")
    RegisterForRemoteEvent(player, "OnItemRemoved")
    UnregisterForRemoteEvent(player, "OnPlayerLoadGame")
    RegisterForRemoteEvent(player, "OnPlayerLoadGame")
    UnRegisterForHitEvent(player)
    RegisterForHitEvent(player)
    UnregisterForRemoteEvent(player, "OnPlayerSwimming")
    RegisterForRemoteEvent(player, "OnPlayerSwimming")
EndFunction

Event Actor.OnPlayerLoadGame(Actor akSender)
    Actor player = Game.GetPlayer()
    if !player
        return
    endif
    LastGameTime = Utility.GetCurrentGameTime()
    SleepStartTime = -1.0
    WaitStartTime = -1.0
    ClearConsumptionCandidates()
    ArmPlayerEvents(player)
    RestartTimer()
    MirrorAll()
    ResyncPenalties()
    ResyncDiseases()
    Debug.Trace("[RadSurvivalQuest] post-load anchor H/T/S=" + HungerDebt + "/" + ThirstDebt + "/" + SleepDebt)
EndEvent

Event OnTimerGameTime(int aiTimerID)
    if aiTimerID != kTimerID
        return
    endif

    if IsActive()
        AdvanceTo(Utility.GetCurrentGameTime())
    else
        LastGameTime = Utility.GetCurrentGameTime()
    endif

    UpdatePenaltyStages("")
    ReconcileDiseases()

    StartTimerGameTime(TickGameHours, kTimerID)
EndEvent

Function RestartTimer()
    CancelTimerGameTime(kTimerID)
    StartTimerGameTime(TickGameHours, kTimerID)
EndFunction

Event Actor.OnItemEquipped(Actor akSender, Form akBaseObject, ObjectReference akReference)
    if !akBaseObject || (akReference != None && akReference.IsQuestItem())
        return
    endif
    if !IsConsumable(akBaseObject)
        return
    endif
    if akSender != Game.GetPlayer() || !IsActive()
        return
    endif
    Debug.Trace("[RadSurvivalQuest] consume-arm equip " + akBaseObject)
    PairEquip(akBaseObject)
EndEvent

Event ObjectReference.OnItemRemoved(ObjectReference akSender, Form akBaseItem, int aiItemCount, ObjectReference akItemReference, ObjectReference akDestContainer)
    if !akBaseItem || akDestContainer != None || aiItemCount <= 0
        return
    endif
    if (akBaseItem as Ammo)
        return
    endif
    if !IsConsumable(akBaseItem)
        return
    endif
    if akSender != Game.GetPlayer() || !IsActive()
        return
    endif
    Debug.Trace("[RadSurvivalQuest] consume-confirm remove " + akBaseItem + " x" + aiItemCount)
    PairRemove(akBaseItem)
EndEvent

Event OnHit(ObjectReference akTarget, ObjectReference akAggressor, Form akSource, Projectile akProjectile, bool abPowerAttack, bool abSneakAttack, bool abBashAttack, bool abHitBlocked, string apMaterial)
    Actor player = Game.GetPlayer()
    if !player
        return
    endif
    RegisterForHitEvent(player)
    if Diseases == None
        return
    endif
    if Diseases.QualifiesAsWound(akTarget, akAggressor, akSource, player)
        Diseases.ExposureHit(akSource, player, IsActive())
    endif
EndEvent

Event Actor.OnPlayerSwimming(Actor akSender)
    if Diseases == None
        return
    endif
    Diseases.ExposureSwim(IsActive())
EndEvent

bool Function IsConsumable(Form akItem)
    if IsFoodOrDrink(akItem) || IsExtraReliefItem(akItem)
        return true
    endif
    return Diseases != None && Diseases.IsCureItem(akItem)
EndFunction
Function PairEquip(Form akItem)
    PruneConsumptionCandidates()
    int match = RemoveCandidateForms.Find(akItem)
    if match >= 0
        RemoveCandidateForms.Remove(match)
        RemoveCandidateTimes.Remove(match)
        ApplyRelief(akItem, 1)
        return
    endif
    EquipCandidateForms.Add(akItem)
    EquipCandidateTimes.Add(Utility.GetCurrentRealTime())
EndFunction

Function PairRemove(Form akItem)
    PruneConsumptionCandidates()
    int match = EquipCandidateForms.Find(akItem)
    if match >= 0
        EquipCandidateForms.Remove(match)
        EquipCandidateTimes.Remove(match)
        ApplyRelief(akItem, 1)
        return
    endif
    RemoveCandidateForms.Add(akItem)
    RemoveCandidateTimes.Add(Utility.GetCurrentRealTime())
EndFunction

Function PruneConsumptionCandidates()
    if !ConsumptionArraysReady || EquipCandidateForms == None || RemoveCandidateForms == None
        ClearConsumptionCandidates()
    endif
    float now = Utility.GetCurrentRealTime()
    int i = EquipCandidateTimes.Length - 1
    while i >= 0
        if now - EquipCandidateTimes[i] > PairWindowSeconds
            EquipCandidateForms.Remove(i)
            EquipCandidateTimes.Remove(i)
        endif
        i -= 1
    endwhile
    i = RemoveCandidateTimes.Length - 1
    while i >= 0
        if now - RemoveCandidateTimes[i] > PairWindowSeconds
            RemoveCandidateForms.Remove(i)
            RemoveCandidateTimes.Remove(i)
        endif
        i -= 1
    endwhile
EndFunction

Function ClearConsumptionCandidates()
    EquipCandidateForms = new Form[0]
    EquipCandidateTimes = new float[0]
    RemoveCandidateForms = new Form[0]
    RemoveCandidateTimes = new float[0]
    ConsumptionArraysReady = true
EndFunction

float Function Tuned(int aiKey, float afDefault)
    float raw = RadNatives.TuningOverride(aiKey)
    if raw > 0.0
        return raw - 1.0
    endif
    return afDefault
EndFunction

bool Function IsExtraReliefItem(Form akItem)
    if !(akItem as Potion)
        return false
    endif
    return RadNatives.GetReliefFlags(akItem) != 0
EndFunction

Function ApplyRelief(Form akItem, int aiCount)
    if aiCount <= 0
        return
    endif
    ReturnEmptyBottle(akItem, aiCount)
    if Diseases != None
        Diseases.ConsumeItem(akItem, aiCount, IsActive())
    endif
    WarmFromFood(akItem, aiCount)
    ResolveKeywords()
    int extra = 0
    if (akItem as Potion)
        extra = RadNatives.GetReliefFlags(akItem)
    endif
    bool ate = (_FoodKeyword != None && akItem.HasKeyword(_FoodKeyword)) || extra == 1 || extra == 3
    bool drank = (_NukaKeyword != None && akItem.HasKeyword(_NukaKeyword)) || (_DrinkKeyword != None && akItem.HasKeyword(_DrinkKeyword)) || (_WaterKeyword != None && akItem.HasKeyword(_WaterKeyword)) || extra == 2 || extra == 3
    float hydration = 1.0
    if _AlcoholKeyword != None && akItem.HasKeyword(_AlcoholKeyword)
        hydration = AlcoholHydrationScale
    endif

    float hungerBefore = HungerDebt
    float thirstBefore = ThirstDebt
    float portion = PortionFor(akItem)
    if ate
        AddDebt("hunger", -1.0 * portion * Tuned(kTuneFood, FoodScale))
    endif
    if drank
        AddDebt("thirst", -1.0 * portion * Tuned(kTuneDrink, DrinkScale) * hydration)
    endif
    if ShowNotifications && ate && HungerDebt <= hungerBefore - 0.01
        Debug.Notification("Hunger eased")
    endif
    if ShowNotifications && drank && ThirstDebt <= thirstBefore - 0.01
        if hydration < 1.0
            Debug.Notification("Thirst eased")
        else
            Debug.Notification("Thirst quenched")
        endif
    endif
    Debug.Trace("[RadSurvivalQuest] consume-apply " + akItem + " x" + aiCount + " ate=" + ate + " drank=" + drank)
EndFunction

Function ReturnEmptyBottle(Form akItem, int aiCount)
    if akItem != Game.GetFormFromFile(0x000366BF, "Fallout4.esm") && akItem != Game.GetFormFromFile(0x000366C0, "Fallout4.esm")
        return
    endif
    Form bottle = Game.GetFormFromFile(0x000DF221, "Fallout4.esm")
    if bottle != None
        Game.GetPlayer().AddItem(bottle, aiCount, false)
    endif
EndFunction

float Function PortionFor(Form akItem)
    float portion = 0.8
    int value = akItem.GetGoldValue()
    if value > 0
        portion = portion + (value as float) / 100.0
    endif
    if portion > 1.5
        portion = 1.5
    endif
    return portion
EndFunction

Event OnPlayerSleepStart(float afSleepStartTime, float afDesiredSleepEndTime, ObjectReference akBed)
    SleepStartTime = Utility.GetCurrentGameTime()
EndEvent

Event OnPlayerSleepStop(bool abInterrupted, ObjectReference akBed)
    float now = Utility.GetCurrentGameTime()
    float started = SleepStartTime
    SleepStartTime = -1.0

    if !IsActive()
        LastGameTime = now
        RestartTimer()
        return
    endif

    float hours = 0.0
    if started >= 0.0
        hours = (now - started) * 24.0
    endif
    if hours < 0.0
        hours = 0.0
    endif
    float recovery = hours * Tuned(kTuneSleepRecovery, SleepRecoveryPerHour)
    if abInterrupted
        recovery = recovery * InterruptedScale
    endif
    AddDebt("sleep", -1.0 * recovery)
    RestCureDiseases(hours, abInterrupted)
    AccrueElapsed(hours)
    RestartTimer()
    Debug.Trace("[RadSurvivalQuest] sleep ended hours=" + hours + " interrupted=" + abInterrupted + " recovery=" + recovery)
EndEvent

Event OnPlayerWaitStart(float afWaitStartTime, float afDesiredWaitEndTime)
    WaitStartTime = Utility.GetCurrentGameTime()
EndEvent

Event OnPlayerWaitStop(bool abInterrupted)
    float now = Utility.GetCurrentGameTime()
    float started = WaitStartTime
    WaitStartTime = -1.0

    if !IsActive()
        LastGameTime = now
        RestartTimer()
        return
    endif

    float hours = 0.0
    if started >= 0.0
        hours = (now - started) * 24.0
    endif
    if hours < 0.0
        hours = 0.0
    endif

    AccrueElapsed(hours)
    RestartTimer()
    Debug.Trace("[RadSurvivalQuest] wait ended hours=" + hours + " sleep=" + SleepDebt)
EndEvent

Function AdvanceTo(float nowDays)
    if LastGameTime < 0.0
        LastGameTime = nowDays
        return
    endif
    float hours = (nowDays - LastGameTime) * 24.0
    if hours <= 0.0
        LastGameTime = nowDays
        return
    endif
    if hours > MaxElapsedHours
        hours = MaxElapsedHours
    endif
    LastGameTime = nowDays
    Accrue(hours)
EndFunction

Function AccrueElapsed(float afHours)
    if afHours <= 0.0
        return
    endif
    if afHours > MaxElapsedHours
        afHours = MaxElapsedHours
    endif
    LastGameTime = Utility.GetCurrentGameTime()
    Accrue(afHours)
EndFunction

Function Accrue(float afHours)
    float rate = Tuned(kTuneRate, 1.0)
    AddDebt("hunger", Tuned(kTuneHunger, HourlyHunger) * rate * afHours)
    AddDebt("thirst", Tuned(kTuneThirst, HourlyThirst) * rate * afHours * DiseaseThirstScale())
    AddDebt("sleep", Tuned(kTuneSleep, HourlySleepDebt) * rate * afHours)
    TickDiseases(afHours)
EndFunction

Function AddDebt(string asPool, float afDelta)
    if asPool == "hunger"
        HungerDebt = Clamp01(HungerDebt + afDelta)
        MirrorToGlobal(RadSurvival_Hunger, HungerDebt)
    elseif asPool == "thirst"
        ThirstDebt = Clamp01(ThirstDebt + afDelta)
        MirrorToGlobal(RadSurvival_Thirst, ThirstDebt)
    elseif asPool == "sleep"
        SleepDebt = Clamp01(SleepDebt + afDelta)
        MirrorToGlobal(RadSurvival_Sleep, SleepDebt)
    endif
    if afDelta < 0.0
        UpdatePenaltyStages(asPool)
    else
        UpdatePenaltyStages("")
    endif
EndFunction

float Function Clamp01(float afValue)
    if afValue < 0.0
        return 0.0
    elseif afValue > 1.0
        return 1.0
    endif
    return afValue
EndFunction

Function MirrorAll()
    MirrorToGlobal(RadSurvival_Hunger, HungerDebt)
    MirrorToGlobal(RadSurvival_Thirst, ThirstDebt)
    MirrorToGlobal(RadSurvival_Sleep, SleepDebt)
EndFunction

Function MirrorToGlobal(GlobalVariable akTarget, float afValue)
    if akTarget
        akTarget.SetValue(afValue)
    endif
EndFunction

Function DebugSetNeeds(float afHunger, float afThirst, float afSleep, int aiDisease, int aiStage)
    if afHunger >= 0.0
        HungerDebt = Clamp01(afHunger)
    endif
    if afThirst >= 0.0
        ThirstDebt = Clamp01(afThirst)
    endif
    if afSleep >= 0.0
        SleepDebt = Clamp01(afSleep)
    endif
    MirrorAll()
    UpdatePenaltyStages("hunger")
    UpdatePenaltyStages("thirst")
    UpdatePenaltyStages("sleep")
    if Diseases != None && aiDisease >= 0
        Diseases.DebugSetDisease(aiDisease, aiStage, IsActive())
    endif
    Debug.Trace("[RadSurvivalQuest] showcase H/T/S=" + HungerDebt + "/" + ThirstDebt + "/" + SleepDebt + " disease=" + aiDisease + " stage=" + aiStage)
EndFunction

Function UpdatePenaltyStages(string asReliefPool)
    if !IsActive()
        ClearAllPenalties()
        return
    endif
    int hunger = StageForHeld(HungerDebt, HungerStage, asReliefPool == "hunger")
    bool hungerRose = hunger > HungerStage
    HungerStage = hunger
    ApplyStagePenalty("hunger", hunger)
    if hungerRose
        NotifyStage("hunger", hunger)
    endif

    int thirst = StageForHeld(ThirstDebt, ThirstStage, asReliefPool == "thirst")
    bool thirstRose = thirst > ThirstStage
    ThirstStage = thirst
    ApplyStagePenalty("thirst", thirst)
    if thirstRose
        NotifyStage("thirst", thirst)
    endif

    int sleep = StageForHeld(SleepDebt, SleepStage, asReliefPool == "sleep")
    bool sleepRose = sleep > SleepStage
    SleepStage = sleep
    ApplyStagePenalty("sleep", sleep)
    if sleepRose
        NotifyStage("sleep", sleep)
    endif
EndFunction

int Function StageForHeld(float afDebt, int aiCurrent, bool abImmediate)
    if abImmediate
        return StageFor(afDebt)
    endif
    if aiCurrent >= 3 && afDebt >= 0.90 - StageHysteresis
        return 3
    endif
    if aiCurrent >= 2 && afDebt >= 0.67 - StageHysteresis
        return 2
    endif
    if aiCurrent >= 1 && afDebt >= 0.34 - StageHysteresis
        return 1
    endif
    return StageFor(afDebt)
EndFunction

int Function StageFor(float afDebt)
    if afDebt < 0.34
        return 0
    endif
    if afDebt < 0.67
        return 1
    endif
    if afDebt < 0.90
        return 2
    endif
    return 3
EndFunction

Function ApplyStagePenalty(string asPool, int aiStage)
    Actor player = Game.GetPlayer()
    if !player
        return
    endif

    Spell wanted = None
    if PenaltiesEnabled && aiStage > 0
        wanted = PenaltySpellFor(asPool, aiStage)
    endif

    Spell applied = AppliedPenalty(asPool)
    if applied == wanted && (!wanted || player.HasSpell(wanted))
        return
    endif
    if applied
        player.RemoveSpell(applied)
    endif
    if wanted
        player.AddSpell(wanted, false)
    endif
    SetAppliedPenalty(asPool, wanted)
    Debug.Trace("[RadSurvivalQuest] penalty " + asPool + " stage " + aiStage + " -> " + wanted)
EndFunction

Function ClearAllPenalties()
    ApplyStagePenalty("hunger", 0)
    ApplyStagePenalty("thirst", 0)
    ApplyStagePenalty("sleep", 0)
EndFunction

Function ResyncPenalties()
    ClearPenaltyCache()
    HungerStage = StageFor(HungerDebt)
    ThirstStage = StageFor(ThirstDebt)
    SleepStage = StageFor(SleepDebt)
    ApplyStagePenalty("hunger", HungerStage)
    ApplyStagePenalty("thirst", ThirstStage)
    ApplyStagePenalty("sleep", SleepStage)
EndFunction

Spell Function PenaltySpellFor(string asPool, int aiStage)
    if aiStage < 1
        return None
    endif
    if aiStage > 3
        aiStage = 3
    endif

    int first = 0
    int cachedStage = -1
    Spell cachedSpell = None
    if asPool == "hunger"
        first = HungerPenaltySpellBase
        cachedStage = CachedHungerStage
        cachedSpell = CachedHungerSpell
    elseif asPool == "thirst"
        first = ThirstPenaltySpellBase
        cachedStage = CachedThirstStage
        cachedSpell = CachedThirstSpell
    elseif asPool == "sleep"
        first = SleepPenaltySpellBase
        cachedStage = CachedSleepStage
        cachedSpell = CachedSleepSpell
    else
        return None
    endif

    if cachedStage == aiStage
        if cachedSpell
            return cachedSpell
        endif
    endif

    Spell resolved = Game.GetFormFromFile(first + aiStage - 1, PenaltyPlugin) as Spell
    if !resolved
        Debug.Trace("[RadSurvivalQuest] missing penalty spell " + (first + aiStage - 1) + " in " + PenaltyPlugin)
        return None
    endif
    if asPool == "hunger"
        CachedHungerStage = aiStage
        CachedHungerSpell = resolved
    elseif asPool == "thirst"
        CachedThirstStage = aiStage
        CachedThirstSpell = resolved
    elseif asPool == "sleep"
        CachedSleepStage = aiStage
        CachedSleepSpell = resolved
    endif
    return resolved
EndFunction

Function ClearPenaltyCache()
    CachedHungerStage = -1
    CachedHungerSpell = None
    CachedThirstStage = -1
    CachedThirstSpell = None
    CachedSleepStage = -1
    CachedSleepSpell = None
EndFunction

Spell Function AppliedPenalty(string asPool)
    if asPool == "hunger"
        return AppliedHungerSpell
    elseif asPool == "thirst"
        return AppliedThirstSpell
    elseif asPool == "sleep"
        return AppliedSleepSpell
    endif
    return None
EndFunction

Function SetAppliedPenalty(string asPool, Spell akSpell)
    if asPool == "hunger"
        AppliedHungerSpell = akSpell
    elseif asPool == "thirst"
        AppliedThirstSpell = akSpell
    elseif asPool == "sleep"
        AppliedSleepSpell = akSpell
    endif
EndFunction

Function NotifyStage(string asPool, int aiStage)
    if !ShowNotifications
        return
    endif
    string label = "HUNGER"
    if asPool == "thirst"
        label = "THIRST"
    elseif asPool == "sleep"
        label = "EXHAUSTION"
    endif
    string text = "Rad survival: " + label + " stage " + aiStage
    if aiStage >= 3
        text = text + " (critical)"
    endif
    Debug.Notification(text)
EndFunction

Function TickDiseases(float afHours)
    if Diseases == None
        return
    endif
    Diseases.Tick(afHours, IsActive(), StageFor(HungerDebt), StageFor(ThirstDebt), StageFor(SleepDebt))
    Diseases.ExposureWeather(afHours, IsActive())
EndFunction

float Function DiseaseThirstScale()
    if Diseases == None
        return 1.0
    endif
    return Diseases.ThirstScale()
EndFunction

Function WarmFromFood(Form akItem, int aiCount)
    if !IsActive() || !akItem
        return
    endif
    FormList hot = Game.GetFormFromFile(HotFoodListFormId, "RadSurvival.esp") as FormList
    if hot && hot.HasForm(akItem)
        RadNatives.WarmBody(HotFoodWarmHours * (aiCount as float))
        if ShowNotifications
            Debug.Notification("The hot meal warms you")
        endif
    endif
EndFunction

Function ApplyColdStage(int aiStage)
    if aiStage < 0 || aiStage > 5 || !IsActive()
        aiStage = 0
    endif
    bool changed = ColdStage != aiStage
    ColdStage = aiStage
    if Diseases != None
        Diseases.SetColdStage(aiStage)
    endif
    Actor player = Game.GetPlayer()
    if !player
        return
    endif
    Spell wanted = None
    Perk wantedPerk = None
    if aiStage > 0
        wanted = Game.GetFormFromFile(ColdSpellBase + aiStage - 1, "RadSurvival.esp") as Spell
        wantedPerk = Game.GetFormFromFile(ColdPerkBase + aiStage - 1, "RadSurvival.esp") as Perk
    endif
    if AppliedColdSpell != wanted || (wanted && !player.HasSpell(wanted))
        if AppliedColdSpell
            player.RemoveSpell(AppliedColdSpell)
        endif
        if wanted
            player.AddSpell(wanted, false)
        endif
        AppliedColdSpell = wanted
    endif
    if AppliedColdPerk != wantedPerk || (wantedPerk && !player.HasPerk(wantedPerk))
        if AppliedColdPerk
            player.RemovePerk(AppliedColdPerk)
        endif
        if wantedPerk
            player.AddPerk(wantedPerk, false)
        endif
        AppliedColdPerk = wantedPerk
    endif
    if changed
        Debug.Trace("[RadSurvivalQuest] cold stage " + aiStage + " ability " + wanted)
    endif
EndFunction

Function RestCureDiseases(float afHours, bool abInterrupted)
    if Diseases == None
        return
    endif
    Diseases.TryRestCure(afHours, abInterrupted, IsActive())
EndFunction

Function ReconcileDiseases()
    if Diseases == None
        return
    endif
    Diseases.Reconcile(IsActive())
EndFunction

Function ResyncDiseases()
    if Diseases == None
        return
    endif
    Diseases.Resync(IsActive())
EndFunction

bool Function IsActive()
    if !Enabled
        return false
    endif
    if SkipWhenVanillaSurvival && Game.GetDifficulty() == kVanillaSurvivalDifficulty
        return false
    endif
    return true
EndFunction

Keyword _FoodKeyword
Keyword _NukaKeyword
Keyword _DrinkKeyword
Keyword _WaterKeyword
Keyword _AlcoholKeyword

Function ResolveKeywords()
    if _FoodKeyword && _NukaKeyword && _DrinkKeyword && _WaterKeyword && _AlcoholKeyword
        return
    endif
    _FoodKeyword = Game.GetFormFromFile(0x00055ECC, "Fallout4.esm") as Keyword
    _NukaKeyword = Game.GetFormFromFile(0x00249F30, "Fallout4.esm") as Keyword
    _DrinkKeyword = Game.GetFormFromFile(0x000F4AEC, "Fallout4.esm") as Keyword
    _WaterKeyword = Game.GetFormFromFile(0x000F4AED, "Fallout4.esm") as Keyword
    _AlcoholKeyword = Game.GetFormFromFile(0x0010C416, "Fallout4.esm") as Keyword
EndFunction

bool Function IsFoodOrDrink(Form akItem)
    if !akItem
        return false
    endif
    ResolveKeywords()
    if _FoodKeyword && akItem.HasKeyword(_FoodKeyword)
        return true
    endif
    if _NukaKeyword && akItem.HasKeyword(_NukaKeyword)
        return true
    endif
    if _DrinkKeyword && akItem.HasKeyword(_DrinkKeyword)
        return true
    endif
    if _WaterKeyword && akItem.HasKeyword(_WaterKeyword)
        return true
    endif
    return false
EndFunction
