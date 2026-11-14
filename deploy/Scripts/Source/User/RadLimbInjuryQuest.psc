ScriptName RadLimbInjuryQuest extends Quest

Spell Property Ability Auto Const
LeveledItem Property FirstAidLoot Auto Const
LeveledItem Property MedsLoot Auto Const
LeveledItem Property VendorMeds Auto Const

Bool lootAdded

Event OnQuestInit()
	Actor player = Game.GetPlayer()
	RegisterForRemoteEvent(player, "OnPlayerLoadGame")
	GiveAbility(player)
	; Leveled list additions persist in the save, so they run once only.
	If FirstAidLoot && !lootAdded
		lootAdded = true
		If MedsLoot
			MedsLoot.AddForm(FirstAidLoot, 1, 1)
		EndIf
		If VendorMeds
			VendorMeds.AddForm(FirstAidLoot, 1, 3)
		EndIf
	EndIf
EndEvent

Event Actor.OnPlayerLoadGame(Actor akSender)
	GiveAbility(akSender)
EndEvent

Function GiveAbility(Actor akPlayer)
	If Ability && akPlayer && !akPlayer.HasSpell(Ability)
		akPlayer.AddSpell(Ability, false)
	EndIf
EndFunction
