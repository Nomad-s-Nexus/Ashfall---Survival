ScriptName RadOutbreakEffect extends ActiveMagicEffect

RadOutbreakQuest Property OutbreakQuest Auto
Faction Property SickFaction Auto
Spell Property InfectionSpell Auto
Spell Property Weakness2Spell Auto
Spell Property Weakness3Spell Auto
FormList Property MedicineList Auto
Keyword Property SleepFurnitureKeyword Auto
Idle Property CoughIdle Auto
Sound Property MaleCoughLight Auto
Sound Property MaleCoughWet Auto
Sound Property MaleSneeze Auto
Sound Property MaleGroan Auto
Sound Property FemaleCoughLight Auto
Sound Property FemaleCoughWet Auto
Sound Property FemaleSneeze Auto
Sound Property FemaleGroan Auto
float Property IncubationHours = 6.0 Auto
float Property EarlyUntilHours = 18.0 Auto
float Property ProgressUntilHours = 36.0 Auto
float Property FinalUntilHours = 60.0 Auto
float Property ImmunityHours = 72.0 Auto
float Property DeathChance = 0.35 Auto
float Property SignIntervalSeconds = 20.0 Auto
float Property SignRadius = 1500.0 Auto
float Property BedSearchRadius = 3000.0 Auto
float Property DeathNoticeRadius = 2000.0 Auto
bool Property BedRestEnabled = true Auto
int Property BedMaxTries = 3 Auto
int Property kTuneDeaths = 18 Auto Const
bool Property LogProbe = false Auto Const

int Stage = -1
float StartedAt = 0.0
bool Started = false
Actor Patient = None
ObjectReference Bed = None
int BedTries = 0

Event OnEffectStart(Actor akTarget, Actor akCaster)
    Patient = akTarget
    Stage = -1
    if OutbreakQuest == None || Patient == None || !OutbreakQuest.CanBeSick(Patient)
        if Patient != None
            Patient.RemoveSpell(InfectionSpell)
            if OutbreakQuest != None
                ClearInfection()
            endif
        endif
        return
    endif
    Started = true
    StartedAt = OutbreakQuest.GetStart(Patient, Utility.GetCurrentGameTime())
    RegisterForCustomEvent(OutbreakQuest, "OutbreakCommand")
    if MedicineList != None
        AddInventoryEventFilter(MedicineList)
    endif
    Probe("start")
    StartTimer(SignIntervalSeconds, 1)
    if Advance()
        StartTimerGameTime(1.0, 2)
    endif
EndEvent

Event OnEffectFinish(Actor akTarget, Actor akCaster)
    CancelTimer(1)
    CancelTimerGameTime(2)
    if OutbreakQuest != None
        UnregisterForCustomEvent(OutbreakQuest, "OutbreakCommand")
    endif
    if Started && Patient != None && !Patient.HasSpell(InfectionSpell)
        ClearInfection()
    endif
    Bed = None
    Started = false
EndEvent

Function ClearInfection()
    SetWeakness(-1)
    Patient.RemoveFromFaction(SickFaction)
    OutbreakQuest.Unregister(Patient)
    OutbreakQuest.RefreshCompanionNotice()
    Probe("cleared")
EndFunction

Event OnTimer(int aiTimerID)
    if aiTimerID != 1 || Patient == None
        return
    endif
    StartTimer(SignIntervalSeconds, 1)
    Sign()
EndEvent

Event OnTimerGameTime(int aiTimerID)
    if aiTimerID != 2 || Patient == None
        return
    endif
    if Advance()
        CancelTimerGameTime(2)
        StartTimerGameTime(1.0, 2)
    endif
EndEvent

Event OnItemAdded(Form akBaseItem, int aiItemCount, ObjectReference akItemReference, ObjectReference akSourceContainer)
    if Patient == None || Stage < 0 || Stage > 3
        return
    endif
    int covers = OutbreakQuest.CoverageOf(akBaseItem)
    if covers < Stage
        return
    endif
    Patient.RemoveItem(akBaseItem, 1, true)
    Probe("medicine")
    Recover()
EndEvent

Event RadOutbreakQuest.OutbreakCommand(RadOutbreakQuest akSender, Var[] akArgs)
    if akArgs == None || akArgs.Length < 2
        return
    endif
    Actor target = akArgs[0] as Actor
    if target != Patient
        return
    endif
    int command = akArgs[1] as int
    if command == 2
        AdvanceOneStage()
    elseif command == 3
        Recover()
    elseif command == 6
        if Advance()
            CancelTimerGameTime(2)
            StartTimerGameTime(1.0, 2)
        endif
    endif
EndEvent

float Function Tuned(int aiKey, float afDefault)
    float raw = RadNatives.TuningOverride(aiKey)
    if raw > 0.0
        return raw - 1.0
    endif
    return afDefault
EndFunction

int Function StageForHours(float afHours)
    if afHours < IncubationHours
        return 0
    elseif afHours < EarlyUntilHours
        return 1
    elseif afHours < ProgressUntilHours
        return 2
    endif
    return 3
EndFunction

bool Function Advance()
    if Patient == None || Patient.IsDead()
        return false
    endif
    float now = Utility.GetCurrentGameTime()
    OutbreakQuest.Touch(Patient)
    float recoveredAt = OutbreakQuest.GetRecovered(Patient)
    if recoveredAt >= 0.0
        ApplyStage(4)
        if (now - recoveredAt) * 24.0 >= ImmunityHours
            Patient.RemoveSpell(InfectionSpell)
            return false
        endif
        return true
    endif
    float hours = (now - StartedAt) * 24.0
    if hours >= FinalUntilHours
        return Resolve()
    endif
    ApplyStage(StageForHours(hours))
    return true
EndFunction

Function AdvanceOneStage()
    float hours = (Utility.GetCurrentGameTime() - StartedAt) * 24.0
    int current = StageForHours(hours)
    float target = FinalUntilHours
    if current == 0
        target = IncubationHours
    elseif current == 1
        target = EarlyUntilHours
    elseif current == 2
        target = ProgressUntilHours
    endif
    StartedAt = Utility.GetCurrentGameTime() - target / 24.0
    OutbreakQuest.SetStart(Patient, StartedAt)
    Advance()
EndFunction

bool Function Resolve()
    if !OutbreakQuest.IsResilient(Patient) && Tuned(kTuneDeaths, 1.0) >= 0.5 && Utility.RandomFloat(0.0, 1.0) < DeathChance
        Die()
        return false
    endif
    Recover()
    return true
EndFunction

Function Die()
    Actor victim = Patient
    Actor player = Game.GetPlayer()
    if victim.Is3DLoaded() && player.GetDistance(victim) <= DeathNoticeRadius
        Debug.Notification("Someone nearby has died of the sickness")
    endif
    Probe("death")
    victim.RemoveSpell(InfectionSpell)
    victim.Kill()
    OutbreakQuest.RefreshCompanionNotice()
EndFunction

Function Recover()
    if Stage == 4
        return
    endif
    OutbreakQuest.SetRecovered(Patient, Utility.GetCurrentGameTime())
    ApplyStage(4)
    Probe("recovered")
EndFunction

Function ApplyStage(int aiStage)
    bool changed = aiStage != Stage
    Stage = aiStage
    if !Patient.IsInFaction(SickFaction)
        Patient.AddToFaction(SickFaction)
    endif
    Patient.SetFactionRank(SickFaction, Stage)
    if changed
        SetWeakness(Stage)
        OutbreakQuest.RefreshCompanionNotice()
        BedTries = 0
        Probe("stage")
    endif
    if (Stage == 2 || Stage == 3) && !OutbreakQuest.IsResilient(Patient)
        EnsureInBed()
    endif
EndFunction

Function SetWeakness(int aiStage)
    Spell wanted = None
    if aiStage == 2
        wanted = Weakness2Spell
    elseif aiStage == 3
        wanted = Weakness3Spell
    endif
    if Weakness2Spell != wanted && Patient.HasSpell(Weakness2Spell)
        Patient.RemoveSpell(Weakness2Spell)
    endif
    if Weakness3Spell != wanted && Patient.HasSpell(Weakness3Spell)
        Patient.RemoveSpell(Weakness3Spell)
    endif
    if wanted != None && !Patient.HasSpell(wanted)
        Patient.AddSpell(wanted, false)
    endif
EndFunction

Function Sign()
    if Stage < 0 || Stage > 3 || !Patient.Is3DLoaded()
        return
    endif
    if Game.GetPlayer().GetDistance(Patient) > SignRadius || Utility.RandomInt(0, 2) != 0
        return
    endif
    bool female = Patient.GetActorBase().GetSex() == 1
    Sound s = None
    if Stage == 0
        if Utility.RandomInt(0, 2) == 0
            s = MaleSneeze
            if female
                s = FemaleSneeze
            endif
        endif
    elseif Stage == 1
        if Utility.RandomInt(0, 3) == 0
            s = MaleSneeze
            if female
                s = FemaleSneeze
            endif
        else
            s = MaleCoughLight
            if female
                s = FemaleCoughLight
            endif
        endif
    elseif Stage == 2
        s = MaleCoughWet
        if female
            s = FemaleCoughWet
        endif
    else
        if Utility.RandomInt(0, 2) == 0
            s = MaleGroan
            if female
                s = FemaleGroan
            endif
        else
            s = MaleCoughWet
            if female
                s = FemaleCoughWet
            endif
        endif
    endif
    if s != None
        s.Play(Patient)
        if CoughIdle != None && (s == MaleCoughLight || s == MaleCoughWet || s == FemaleCoughLight || s == FemaleCoughWet)
            if !Patient.IsInCombat() && !Patient.IsInScene() && Patient.GetSleepState() == 0 && Patient.GetSitState() == 0
                Patient.PlayIdle(CoughIdle)
            endif
        endif
    endif
EndFunction

Function EnsureInBed()
    if !BedRestEnabled || !Patient.Is3DLoaded() || Patient.IsInCombat() || Patient.IsInScene()
        return
    endif
    if Patient.GetSleepState() == 3
        BedTries = 0
        return
    endif
    if BedTries >= BedMaxTries
        return
    endif
    BedTries += 1
    if Bed == None || Bed.IsFurnitureInUse()
        Bed = NearestFreeBed()
    endif
    if Bed != None
        Bed.Activate(Patient)
        Probe("bed")
    endif
EndFunction

ObjectReference Function NearestFreeBed()
    if SleepFurnitureKeyword == None
        return None
    endif
    ObjectReference[] beds = Patient.FindAllReferencesWithKeyword(SleepFurnitureKeyword, BedSearchRadius)
    if beds == None
        return None
    endif
    ObjectReference best = None
    float bestDist = 99999999.0
    int i = 0
    while i < beds.Length
        if !beds[i].IsFurnitureInUse()
            float d = Patient.GetDistance(beds[i])
            if d < bestDist
                best = beds[i]
                bestDist = d
            endif
        endif
        i += 1
    endwhile
    return best
EndFunction

Function Probe(string asTag)
    if LogProbe && Patient != None
        Debug.Trace("RadOutbreak " + asTag + " actor=" + Patient + " stage=" + Stage + " bed=" + Bed + " sleep=" + Patient.GetSleepState())
    endif
EndFunction
