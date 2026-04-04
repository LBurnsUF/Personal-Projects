#NoEnv
#InstallKeybdHook
#UseHook
SendMode Input
SetWorkingDir %A_ScriptDir%

; ---------------- Config ----------------
keys := "a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z," 
       . "0,1,2,3,4,5,6,7,8,9,"
       . "Space"

MaxHeld  := 4      ; max simultaneous keys we will interleave
PeriodMs := 20     ; time between outputs (ms)

; ---------------- Globals ----------------
global HeldKeys     := []   ; dense array of currently held keys
global CurrentIndex := 1
global TimerRunning := false
global ScriptEnabled := true

; -------- AUTO-EXECUTE (runs once at startup) --------
Gosub InitHotkeys
TrayTip, Interleave, Script enabled, 1000
return
; -----------------------------------------------------


; ---------------- Optional debug / control hotkeys ----------------
F12::  ; toggle script on/off
    ScriptEnabled := !ScriptEnabled
    if (!ScriptEnabled) {
        SetTimer, InterleaveTimer, Off
        TimerRunning := false
        HeldKeys := []
        CurrentIndex := 1
        TrayTip, Interleave, Script disabled, 1000
    } else {
        TrayTip, Interleave, Script enabled, 1000
    }
return

F11::  ; panic clear
    HeldKeys := []
    CurrentIndex := 1
    if (TimerRunning) {
        SetTimer, InterleaveTimer, Off
        TimerRunning := false
    }
    TrayTip, Interleave, State cleared, 700
return


; ---------------- Init dynamic hotkeys ----------------
InitHotkeys:
    keysArr := StrSplit(keys, ",")

    for index, thisKey in keysArr
    {
        thisKey := Trim(thisKey, " `t`r`n")
        if (thisKey = "")
            continue

        hot   := "$*" . thisKey
        hotUp := "$*" . thisKey . " up"

        Hotkey, %hot%,   KeyDownHandler, On
        Hotkey, %hotUp%, KeyUpHandler,   On
    }
return


; ---------------- Helpers ----------------
HasKey(arr, key) {
    for idx, val in arr
        if (val = key)
            return idx
    return 0
}

SendKey(key) {
    if (StrLen(key) = 1) {
        SendInput, % key
    } else {
        SendInput, % "{" . key . "}"
    }
}

StartMainTimer() {
    global TimerRunning, PeriodMs
    if (!TimerRunning) {
        TimerRunning := true
        SetTimer, InterleaveTimer, %PeriodMs%
    }
}

StopMainTimer() {
    global TimerRunning
    if (TimerRunning) {
        TimerRunning := false
        SetTimer, InterleaveTimer, Off
    }
}

GetKeyNameFromHotkey(hk) {
    key := hk
    key := RegExReplace(key, "^\$\*")   ; strip $*
    key := RegExReplace(key, "^\*")    ; strip * if any
    key := RegExReplace(key, " up$")   ; strip " up"
    return key
}


; ---------------- Key down handler ----------------
KeyDownHandler:
    if (!ScriptEnabled)
        return

    key := GetKeyNameFromHotkey(A_ThisHotkey)

    ; ignore auto-repeat from OS
    idx := HasKey(HeldKeys, key)
    if (idx)
        return

    keysCount := HeldKeys.MaxIndex()
    if (keysCount >= MaxHeld)
        return

    HeldKeys.Push(key)

    if (HeldKeys.MaxIndex() = 1) {
        CurrentIndex := 1
    }

    StartMainTimer()
return


; ---------------- Key up handler ----------------
KeyUpHandler:
    key := GetKeyNameFromHotkey(A_ThisHotkey)

    idx := HasKey(HeldKeys, key)
    if (idx) {
        HeldKeys.Remove(idx)
    }

    keysCount := HeldKeys.MaxIndex()
    if (!keysCount) {
        CurrentIndex := 1
        StopMainTimer()
    } else if (CurrentIndex > keysCount) {
        CurrentIndex := 1
    }
return


; ---------------- Main periodic timer ----------------
InterleaveTimer:
    if (!ScriptEnabled) {
        StopMainTimer()
        return
    }

    ; --- self-heal HeldKeys using physical state ---
    i := HeldKeys.MaxIndex()
    while (i >= 1) {
        k := HeldKeys[i]
        if !GetKeyState(k, "P") {   ; "P" = physical state
            HeldKeys.Remove(i)
        }
        i--
    }

    keysCount := HeldKeys.MaxIndex()
    if (!keysCount) {
        CurrentIndex := 1
        StopMainTimer()
        return
    }

    if (CurrentIndex < 1 || CurrentIndex > keysCount)
        CurrentIndex := 1

    key := HeldKeys[CurrentIndex]
    SendKey(key)

    CurrentIndex++
    if (CurrentIndex > keysCount)
        CurrentIndex := 1
return
