ScriptName RadOutbreakQuest extends Quest

CustomEvent OutbreakCommand

Keyword Property ActorTypeNPC Auto
Keyword Property ActorTypeRobot Auto
Faction Property SickFaction Auto
Faction Property CompanionFaction Auto
Spell Property InfectionSpell Auto
GlobalVariable Property PlayerDisease Auto
RadSurvivalDiseases Property Diseases Auto
RadSurvivalQuest Property Survival Auto
Keyword Property StationKeyword Auto
Spell Property CompanionNotice1 Auto
Spell Property CompanionNotice2 Auto
Spell Property CompanionNotice3 Auto
FormList Property MedicineList Auto
Potion Property RadAway Auto
Potion Property Addictol Auto
Potion Property CureItem Auto
Keyword Property KeywordCureItem Auto
int Property RadAwayMaxStage = 1 Auto Const
int Property AddictolMaxStage = 2 Auto Const
float Property SyncStaleHours = 2.0 Auto
float Property StationRadius = 3000.0 Auto
float Property PreventBase = 0.15 Auto
float Property PreventPerTier = 0.15 Auto
float Property TreatPerHour = 0.05 Auto
float Property ScanRadius = 3000.0 Auto
float Property SpreadRadius = 400.0 Auto
float Property SpreadPerHour0 = 0.02 Auto
float Property SpreadPerHour1 = 0.05 Auto
float Property SpreadPerHour2 = 0.15 Auto
float Property SpreadPerHour3 = 0.15 Auto
float Property AmbientPerHour = 0.002 Auto
float Property ContactRiskPerHour = 0.02 Auto
float Property MaxCatchUpHours = 24.0 Auto
int Property MaxActors = 96 Auto
int Property kTuneEnabled = 16 Auto Const
int Property kTuneSpread = 17 Auto Const
int Property kTuneDeaths = 18 Auto Const
int Property kTuneAmbient = 19 Auto Const
bool Property LogProbe = false Auto Const

Actor[] RegActors
float[] RegStarted
float[] RegRecovered
float[] RegSeen
float LastTick = -1.0

Event OnInit()
    EnsureRegistry()
    LastTick = Utility.GetCurrentGameTime()
    StartTimerGameTime(1.0, 1)
EndEvent

Event OnTimerGameTime(int aiTimerID)
    if aiTimerID != 1
        return
    endif
    StartTimerGameTime(1.0, 1)
    float now = Utility.GetCurrentGameTime()
    float hours = 1.0
    if LastTick >= 0.0
        hours = (now - LastTick) * 24.0
    endif
    LastTick = now
    if hours <= 0.0
        return
    endif
    if hours > MaxCatchUpHours
        hours = MaxCatchUpHours
    endif
    Tick(hours)
    RefreshCompanionNotice()
EndEvent

Function SetNotice(Actor akPlayer, Spell akSpell, bool abWanted)
    if akSpell == None
        return
    endif
    if abWanted
        if !akPlayer.HasSpell(akSpell)
            akPlayer.AddSpell(akSpell, false)
        endif
    elseif akPlayer.HasSpell(akSpell)
        akPlayer.RemoveSpell(akSpell)
    endif
EndFunction

Function RefreshCompanionNotice()
    Actor player = Game.GetPlayer()
    if player == None
        return
    endif
    EnsureRegistry()
    int worst = 0
    int i = 0
    while i < RegActors.Length
        Actor a = RegActors[i]
        if a != None && !a.IsDead() && IsCompanion(a)
            int r = StageOf(a)
            if r > worst && r <= 3
                worst = r
            endif
        endif
        i += 1
    endwhile
    SetNotice(player, CompanionNotice1, worst == 1)
    SetNotice(player, CompanionNotice2, worst == 2)
    SetNotice(player, CompanionNotice3, worst == 3)
EndFunction

float Function Tuned(int aiKey, float afDefault)
    float raw = RadNatives.TuningOverride(aiKey)
    if raw > 0.0
        return raw - 1.0
    endif
    return afDefault
EndFunction

Function EnsureRegistry()
    if RegActors == None
        RegActors = new Actor[0]
        RegStarted = new float[0]
        RegRecovered = new float[0]
    endif
    if RegSeen == None
        RegSeen = new float[0]
    endif
    while RegSeen.Length < RegActors.Length
        RegSeen.Add(-1.0)
    endwhile
EndFunction

int Function RegIndex(Actor akActor)
    EnsureRegistry()
    int i = 0
    while i < RegActors.Length
        if RegActors[i] == akActor
            return i
        endif
        i += 1
    endwhile
    return -1
EndFunction

float Function GetStart(Actor akActor, float afNow)
    int i = RegIndex(akActor)
    if i < 0
        RegActors.Add(akActor)
        RegStarted.Add(afNow)
        RegRecovered.Add(-1.0)
        RegSeen.Add(afNow)
        return afNow
    endif
    return RegStarted[i]
EndFunction

Function SetStart(Actor akActor, float afStart)
    int i = RegIndex(akActor)
    if i < 0
        GetStart(akActor, afStart)
        return
    endif
    RegStarted[i] = afStart
EndFunction

float Function GetRecovered(Actor akActor)
    int i = RegIndex(akActor)
    if i < 0
        return -1.0
    endif
    return RegRecovered[i]
EndFunction

Function SetRecovered(Actor akActor, float afWhen)
    int i = RegIndex(akActor)
    if i < 0
        GetStart(akActor, afWhen)
        i = RegIndex(akActor)
    endif
    RegRecovered[i] = afWhen
EndFunction

Function Unregister(Actor akActor)
    int i = RegIndex(akActor)
    if i >= 0
        RegActors.Remove(i)
        RegStarted.Remove(i)
        RegRecovered.Remove(i)
        RegSeen.Remove(i)
    endif
EndFunction

Function Touch(Actor akActor)
    int i = RegIndex(akActor)
    if i >= 0
        RegSeen[i] = Utility.GetCurrentGameTime()
    endif
EndFunction

Function SyncStale(float afNow)
    EnsureRegistry()
    Actor[] stale = new Actor[0]
    int i = 0
    while i < RegActors.Length
        if RegActors[i] != None && !RegActors[i].IsDead() && (RegSeen[i] < 0.0 || (afNow - RegSeen[i]) * 24.0 > SyncStaleHours)
            stale.Add(RegActors[i])
            RegSeen[i] = afNow
        endif
        i += 1
    endwhile
    i = 0
    while i < stale.Length
        Command(stale[i], 6)
        i += 1
    endwhile
    if stale.Length > 0
        Probe("sync stale=" + stale.Length)
    endif
EndFunction

Function Prune()
    EnsureRegistry()
    int i = RegActors.Length - 1
    while i >= 0
        Actor a = RegActors[i]
        if a == None
            RegActors.Remove(i)
            RegStarted.Remove(i)
            RegRecovered.Remove(i)
            RegSeen.Remove(i)
        elseif a.IsDead()
            a.RemoveFromFaction(SickFaction)
            RegActors.Remove(i)
            RegStarted.Remove(i)
            RegRecovered.Remove(i)
            RegSeen.Remove(i)
        endif
        i -= 1
    endwhile
EndFunction

bool Function IsCompanion(Actor akActor)
    return akActor.IsPlayerTeammate() || akActor.IsInFaction(CompanionFaction)
EndFunction

bool Function IsResilient(Actor akActor)
    return IsCompanion(akActor) || akActor.IsEssential() || akActor.GetActorBase().IsProtected() || RadNatives.IsQuestHeld(akActor)
EndFunction

int Function CoverageOf(Form akItem)
    if akItem == None
        return -1
    endif
    if akItem == RadAway
        return RadAwayMaxStage
    elseif akItem == Addictol
        return AddictolMaxStage
    elseif akItem == CureItem
        return 3
    elseif KeywordCureItem != None && akItem.HasKeyword(KeywordCureItem)
        return 3
    endif
    return -1
EndFunction

bool Function TryMedicate(Actor akActor, int aiStage)
    if MedicineList == None
        return false
    endif
    int k = 0
    while k < MedicineList.GetSize()
        Form f = MedicineList.GetAt(k)
        if f != None && akActor.GetItemCount(f) > 0 && CoverageOf(f) >= aiStage
            akActor.RemoveItem(f, 1, true)
            Cure(akActor)
            return true
        endif
        k += 1
    endwhile
    return false
EndFunction

bool Function CanBeSick(Actor akActor)
    if akActor == None || akActor == Game.GetPlayer() || akActor.IsDead() || akActor.IsChild()
        return false
    endif
    if ActorTypeRobot && akActor.HasKeyword(ActorTypeRobot)
        return false
    endif
    return IsCompanion(akActor) || !akActor.IsEssential()
EndFunction

int Function StageOf(Actor akActor)
    if !akActor.IsInFaction(SickFaction)
        return -1
    endif
    return akActor.GetFactionRank(SickFaction)
EndFunction

float Function SpreadFor(int aiStage)
    if aiStage <= 0
        return SpreadPerHour0
    elseif aiStage == 1
        return SpreadPerHour1
    elseif aiStage == 2
        return SpreadPerHour2
    endif
    return SpreadPerHour3
EndFunction

int Function PlayerStage()
    if PlayerDisease == None
        return -1
    endif
    int packed = PlayerDisease.GetValue() as int
    int id = packed / 10
    if id != 1 && id != 2 && id != 3 && id != 6
        return -1
    endif
    return packed % 10
EndFunction

Actor[] Function Scan()
    Actor[] pool = new Actor[0]
    Actor player = Game.GetPlayer()
    if player == None || ActorTypeNPC == None
        return pool
    endif
    ObjectReference[] found = player.FindAllReferencesWithKeyword(ActorTypeNPC, ScanRadius)
    if found == None
        return pool
    endif
    int i = 0
    while i < found.Length && pool.Length < MaxActors
        Actor a = found[i] as Actor
        if a != None && a.Is3DLoaded() && CanBeSick(a)
            pool.Add(a)
        endif
        i += 1
    endwhile
    return pool
EndFunction

Function Command(Actor akActor, int aiCommand)
    Var[] args = new Var[2]
    args[0] = akActor
    args[1] = aiCommand
    SendCustomEvent("OutbreakCommand", args)
EndFunction

Function Cure(Actor akActor)
    Command(akActor, 3)
EndFunction

int Function CoverTier(ObjectReference[] akStations, int[] aiTiers, ObjectReference akRef)
    int best = -1
    int i = 0
    while i < akStations.Length
        if aiTiers[i] > best && akStations[i].GetDistance(akRef) <= StationRadius
            best = aiTiers[i]
        endif
        i += 1
    endwhile
    return best
EndFunction

float Function PreventFactor(int aiTier)
    if aiTier < 0
        return 1.0
    endif
    float f = 1.0 - (PreventBase + PreventPerTier * aiTier)
    if f < 0.0
        f = 0.0
    endif
    return f
EndFunction

Function Infect(Actor akActor)
    akActor.AddSpell(InfectionSpell, false)
EndFunction

Function Tick(float afHours)
    Prune()
    SyncStale(Utility.GetCurrentGameTime())
    if Tuned(kTuneEnabled, 1.0) < 0.5
        return
    endif
    Actor player = Game.GetPlayer()
    Actor[] pool = Scan()
    int n = pool.Length
    if n == 0
        return
    endif
    int[] stage = new int[n]
    int i = 0
    while i < n
        stage[i] = StageOf(pool[i])
        i += 1
    endwhile

    ObjectReference[] stations = new ObjectReference[0]
    int[] tiers = new int[0]
    if StationKeyword != None
        ObjectReference[] raw = player.FindAllReferencesWithKeyword(StationKeyword, ScanRadius + StationRadius)
        int s = 0
        while raw != None && s < raw.Length
            RadOutbreakDoctorStation station = raw[s] as RadOutbreakDoctorStation
            if station != None
                int t = station.GetTier()
                if t >= 0
                    stations.Add(raw[s])
                    tiers.Add(t)
                endif
            endif
            s += 1
        endwhile
    endif
    int[] cover = new int[n]
    i = 0
    while i < n
        cover[i] = CoverTier(stations, tiers, pool[i])
        i += 1
    endwhile
    int playerCover = CoverTier(stations, tiers, player)

    float scale = Tuned(kTuneSpread, 1.0)
    float ambient = Tuned(kTuneAmbient, AmbientPerHour)
    int playerStage = PlayerStage()

    float risk = 0.0
    i = 0
    while i < n
        if stage[i] >= 1 && stage[i] <= 3 && player.GetDistance(pool[i]) <= SpreadRadius
            risk += ContactRiskPerHour * afHours
        endif
        i += 1
    endwhile
    risk *= PreventFactor(playerCover)
    if risk > 0.0 && Diseases != None && Survival != None
        Diseases.AddRisk(risk, Survival.IsActive())
    endif

    int sourceCount = 0
    int treated = 0
    int medicated = 0
    int j = 0
    while j < n
        if stage[j] == -1
            float p = ambient * afHours
            if playerStage >= 0 && player.GetDistance(pool[j]) <= SpreadRadius
                p += SpreadFor(playerStage) * afHours * scale
            endif
            int k = 0
            while k < n
                if stage[k] >= 0 && stage[k] <= 3 && pool[k].GetDistance(pool[j]) <= SpreadRadius
                    p += SpreadFor(stage[k]) * afHours * scale
                    sourceCount += 1
                endif
                k += 1
            endwhile
            p *= PreventFactor(cover[j])
            if p > 0.0 && !pool[j].IsInScene() && (IsCompanion(pool[j]) || !RadNatives.IsQuestHeld(pool[j])) && Utility.RandomFloat(0.0, 1.0) < p
                Infect(pool[j])
                stage[j] = 0
            endif
        elseif stage[j] >= 0 && stage[j] <= 3
            if TryMedicate(pool[j], stage[j])
                medicated += 1
            elseif cover[j] >= 1 && stage[j] <= cover[j] + 1
                if Utility.RandomFloat(0.0, 1.0) < TreatPerHour * cover[j] * afHours
                    Cure(pool[j])
                    treated += 1
                endif
            endif
        endif
        j += 1
    endwhile
    Probe("tick hours=" + afHours + " pool=" + n + " contacts=" + sourceCount + " stations=" + stations.Length + " treated=" + treated + " medicated=" + medicated)
EndFunction

int Function RunDebug(int aiCommand)
    EnsureRegistry()
    CancelTimerGameTime(1)
    StartTimerGameTime(1.0, 1)
    Actor[] pool = Scan()
    int n = pool.Length
    int count = 0
    int i = 0
    if aiCommand == 1
        Actor player = Game.GetPlayer()
        Actor best = None
        float bestDist = 99999999.0
        while i < n
            if StageOf(pool[i]) == -1
                float d = player.GetDistance(pool[i])
                if d < bestDist
                    best = pool[i]
                    bestDist = d
                endif
            endif
            i += 1
        endwhile
        if best != None
            Infect(best)
            count = 1
        endif
    elseif aiCommand == 2 || aiCommand == 3
        while i < n
            int s = StageOf(pool[i])
            if s >= 0 && s <= 3
                Command(pool[i], aiCommand)
                count += 1
            endif
            i += 1
        endwhile
    elseif aiCommand == 4
        int[] counts = new int[6]
        while i < n
            int s = StageOf(pool[i]) + 1
            counts[s] = counts[s] + 1
            i += 1
        endwhile
        string line = "Outbreak: healthy " + counts[0] + ", incubating " + counts[1] + ", early " + counts[2] + ", progress " + counts[3] + ", final " + counts[4] + ", immune " + counts[5]
        Debug.Trace(line)
        Debug.Notification(line)
        count = n
    endif
    if aiCommand >= 1 && aiCommand <= 3
        RefreshCompanionNotice()
    endif
    return count
EndFunction

Function Probe(string asText)
    if LogProbe
        Debug.Trace("RadOutbreakQuest " + asText)
    endif
EndFunction

int Function DebugCommand(int aiCommand) global
    RadOutbreakQuest outbreak = Game.GetFormFromFile(0x00000803, "RadOutbreak.esp") as RadOutbreakQuest
    if outbreak == None
        Debug.Trace("RadOutbreakQuest.DebugCommand: quest not found")
        return 0
    endif
    return outbreak.RunDebug(aiCommand)
EndFunction
