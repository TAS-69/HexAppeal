OPTION CASEMAP:NONE
EXTERN HexFrameOriginal:QWORD
EXTERN HexFrameCallOriginalA:QWORD
EXTERN HexFrameCallOriginalB:QWORD
EXTERN HexFrameCallback:PROC
EXTERN HexWardrobeHudOriginal:QWORD
EXTERN HexWardrobeHideHud:DWORD
EXTERN HexWardrobeApplyContinuation:QWORD
PUBLIC HexFrameEntry
PUBLIC HexFrameCallA
PUBLIC HexFrameCallB
PUBLIC HexWardrobeHudEntry
PUBLIC HexWardrobeApplyStock
.code
FrameEntry MACRO entry, original, frameSize
entry PROC FRAME
    sub rsp,frameSize
    .allocstack frameSize
    .endprolog
    mov [rsp+20h],rax
    mov [rsp+28h],rcx
    mov [rsp+30h],rdx
    mov [rsp+38h],r8
    mov [rsp+40h],r9
    mov [rsp+48h],r10
    mov [rsp+50h],r11
    pushfq
    pop rax
    mov [rsp+58h],rax
    movdqu [rsp+60h],xmm0
    movdqu [rsp+70h],xmm1
    movdqu [rsp+80h],xmm2
    movdqu [rsp+90h],xmm3
    movdqu [rsp+0A0h],xmm4
    movdqu [rsp+0B0h],xmm5
    stmxcsr [rsp+0C0h]
    fnstcw [rsp+0C4h]
    call HexFrameCallback
    fldcw [rsp+0C4h]
    ldmxcsr [rsp+0C0h]
    movdqu xmm0,[rsp+60h]
    movdqu xmm1,[rsp+70h]
    movdqu xmm2,[rsp+80h]
    movdqu xmm3,[rsp+90h]
    movdqu xmm4,[rsp+0A0h]
    movdqu xmm5,[rsp+0B0h]
    mov rcx,[rsp+28h]
    mov rdx,[rsp+30h]
    mov r8,[rsp+38h]
    mov r9,[rsp+40h]
    mov r10,[rsp+48h]
    mov r11,[rsp+50h]
    push QWORD PTR [rsp+58h]
    popfq
    mov rax,[rsp+20h]
    lea rsp,[rsp+frameSize]
    jmp QWORD PTR [original]
entry ENDP
ENDM

FrameEntry HexFrameEntry, HexFrameOriginal, 0D8h
; The native frame call has returned, so RSP is aligned in the caller.
FrameEntry HexFrameCallA, HexFrameCallOriginalA, 0E0h
FrameEntry HexFrameCallB, HexFrameCallOriginalB, 0E0h

HexWardrobeHudEntry PROC
    pushfq
    cmp DWORD PTR [HexWardrobeHideHud],0
    je hudRangeReady
    mov r9d,r8d
hudRangeReady:
    popfq
    jmp QWORD PTR [HexWardrobeHudOriginal]
HexWardrobeHudEntry ENDP

HexWardrobeApplyStock PROC
    mov [rsp+10h],rbx
    mov [rsp+20h],r9b
    mov [rsp+18h],r8
    jmp QWORD PTR [HexWardrobeApplyContinuation]
HexWardrobeApplyStock ENDP
END
