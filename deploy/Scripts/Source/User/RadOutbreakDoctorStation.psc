ScriptName RadOutbreakDoctorStation extends WorkshopObjectScript

FormList Property DoctorsTier1 Auto
FormList Property DoctorsTier2 Auto

int Function GetTier()
    Actor doctor = GetActorRefOwner()
    if doctor == None || doctor.IsDead()
        return -1
    endif
    ActorBase base = doctor.GetActorBase()
    if DoctorsTier2 != None && DoctorsTier2.HasForm(base)
        return 2
    elseif DoctorsTier1 != None && DoctorsTier1.HasForm(base)
        return 1
    endif
    return 0
EndFunction
