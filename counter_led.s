;-----------------------------------------------------------
; Title      : RAM counter
; Written by : Michele Fabbri
; Date       : November 1 2002
; Description: In memory counter shown on the LED port
;-----------------------------------------------------------
; D1 pause counter

; Memory map
RAM_START       EQU $00000
VEC_TABLE_END   EQU $00100  ; last vector table address + 1
RAM_END         EQU $20000  ; last RAM address + 1
LED_PORT        EQU $7C000
ROM             EQU $80000

; Directives
DELAY       EQU 32000

; ROM
    ORG ROM
; Constant Vector Table (2 entries)
    ; During the first 8 reads the glue logic move the following
    ; two entries of the vector table to the address $00000
    DC.L RAM_END    ; Reset Stack Pointer (SP) (end of RAM1 + 1)
    DC.L START      ; Reset Program counter (PC) (point to the beginning of code)

; Code
START:
    MOVE.B  #0, COUNTER
LOOP:
    MOVE.B  COUNTER, LED_PORT
    MOVE.L  #DELAY, D1
PAUSE:
    SUB.L   #1,D1
    BNE.L   PAUSE
    ADD.B   #1,COUNTER
    BRA     LOOP


; RAM after vector table
    ORG VEC_TABLE_END

; Variables and constants
COUNTER: DS.B 1

    END    START            ; Last line of source
