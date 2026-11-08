ScriptName RadStormFX extends Quest

Int Property BurstSteps = 6 Auto

Float Property RumbleStrength = 0.12 Auto

explosion Property NukeExplosion Auto

Float Property NukeMaxDistance = 20000.0 Auto

Float Property RumbleInterval = 0.6 Auto

form Property NukeMarker Auto

Bool Property DebugNotify = false Auto

Float Property NukeZOffset = 22000.0 Auto

Float Property BurstStrength = 0.75 Auto

Float Property NukeMinDistance = 12000.0 Auto

Bool _shaking = false
Int _kRumbleTimer = 1

Event OnStageSet(Int aiStage, Int aiItem)
	_Note(("OnStageSet stage=" + (aiStage as String)))
	If ((aiStage == 10))
		_StartShake()
	ElseIf ((aiStage == 11))
		_SpawnNuke()
		_StartShake()
	ElseIf ((aiStage == 12))
	ElseIf ((aiStage == 20))
		_StopShake()
	Else
		_Log(("OnStageSet: unhandled stage " + (aiStage as String)))
	EndIf
EndEvent

Function _Note(String msg)
	debug.Trace(("[RadStormFX] " + msg), 0)
	If (DebugNotify)
		debug.Notification(("[StormFX] " + msg))
	EndIf
EndFunction

Function _StopShake()
	_shaking = false
	_Log("_StopShake")
EndFunction

Event OnTimer(Int aiTimerID)
	If ((aiTimerID == _kRumbleTimer))
		If (_shaking)
			Game.ShakeCamera(None, RumbleStrength, (RumbleInterval + 0.1))
			Game.ShakeController((RumbleStrength * 0.5), RumbleStrength, (RumbleInterval + 0.1))
			StartTimer(RumbleInterval, _kRumbleTimer)
		EndIf
	EndIf
EndEvent

Function _Log(String msg)
	debug.Trace(("[RadStormFX] " + msg), 0)
EndFunction

Function _SpawnNuke()
	actor player
	Float ang
	Float dist
	Float ox
	Float oy
	objectreference marker
	objectreference boom
	If ((NukeExplosion == None))
		NukeExplosion = (Game.GetFormFromFile(2181, "RadiationOverhaul.esp") as explosion)
	EndIf
	If ((NukeExplosion == None))
		NukeExplosion = (Game.GetFormFromFile(1186408, "Fallout4.esm") as explosion)
	EndIf
	If ((NukeMarker == None))
		NukeMarker = Game.GetFormFromFile(59, "Fallout4.esm")
	EndIf
	_Log(((("_SpawnNuke: explosion=" + (NukeExplosion as String)) + " marker=") + (NukeMarker as String)))
	If (((NukeExplosion == None) as Bool) || ((NukeMarker == None) as Bool))
		_Note("NUKE FAILED: explosion or marker form is None")
		Return
	EndIf
	player = Game.GetPlayer()
	ang = utility.RandomFloat(0.0, 360.0)
	dist = utility.RandomFloat(NukeMinDistance, NukeMaxDistance)
	ox = (math.Sin(ang) * dist)
	oy = (math.Cos(ang) * dist)
	_Log((((((((((("_SpawnNuke: ang=" + (ang as String)) + " dist=") + (dist as String)) + " offset=(") + (ox as String)) + ",") + (oy as String)) + ",") + (NukeZOffset as String)) + ")"))
	marker = player.PlaceAtMe(NukeMarker, 1, false, false, true)
	If (marker)
		marker.MoveTo((player as objectreference), ox, oy, NukeZOffset, true)
		boom = marker.PlaceAtMe((NukeExplosion as form), 1, false, false, true)
		_Log(("_SpawnNuke: detonation ref=" + (boom as String)))
		marker.Delete()
		_Note(("NUKE detonated at dist " + (dist as String)))
	Else
		_Note("NUKE FAILED: marker PlaceAtMe returned None")
	EndIf
EndFunction

Function _StartShake()
	Int I
	Float t
	Float mag
	_shaking = true
	_Note("SHAKE burst start")
	I = 0
	If (((I < BurstSteps) as Bool))
	EndIf
	While (((_shaking as Bool) && (I < BurstSteps)))
		t = (((BurstSteps - I) as Float) / (BurstSteps as Float))
		mag = (BurstStrength * t)
		Game.ShakeCamera(None, mag, 0.4)
		Game.ShakeController(mag, mag, 0.4)
		utility.Wait(0.35)
		I = (I + 1)
	EndWhile
	If (_shaking)
		StartTimer(RumbleInterval, _kRumbleTimer)
		_Log("_StartShake: burst done, rumble loop started")
	EndIf
EndFunction
