;-----------------------------------------------------------
; Title      : RAM counter
; Written by : Michele Fabbri, modificato da GD per ROM/RAM mapping
; Date       : November 1-17 2002
; Description: In memory counter shown on the LED port
;-----------------------------------------------------------
; D1 pause counter		;	https://oldwww.nvg.ntnu.no/amiga/MC680x0_Sections/mc68000timing.HTML

; Memory map
RAM_START       EQU $10000
VEC_TABLE_END   EQU $00100  ; last vector table address + 1
RAM_END         EQU $20000  ; last RAM address + 1
LED_PORT        EQU $7C000
ROM             EQU $00000

; Directives
DELAY       EQU 32000		; ~145mS @ 4MHz

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
    SUB.L   #1,D1           ; 8 cicli
    BNE.L   PAUSE           ; 8/10 cicli
    ADD.B   #1,COUNTER
    BRA     LOOP


; RAM after vector table
    ORG VEC_TABLE_END

; Variables and constants
COUNTER: DS.B 1

    END    START            ; Last line of source
