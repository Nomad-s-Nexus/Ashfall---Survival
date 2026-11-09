ScriptName RadShowcase

Function Apply(float afHunger, float afThirst, float afSleep, int aiDisease, int aiStage) Global
    RadSurvivalQuest quest = Game.GetFormFromFile(0x00000800, "RadSurvival.esp") as RadSurvivalQuest
    if quest == None
        Debug.Trace("[RadShowcase] RadSurvivalQuest not found")
        return
    endif
    quest.DebugSetNeeds(afHunger, afThirst, afSleep, aiDisease, aiStage)
EndFunction
