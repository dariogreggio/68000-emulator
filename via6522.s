;-----------------------------------------------------------
; Title      : Subroutine Test
; Written by : Michele Fabbri
; Date       : November 4 2002
; Description: In memory counter shown on the LED port
;-----------------------------------------------------------

; Memory map
RAM_START       EQU $00000
VEC_TABLE_END   EQU $00100  ; last vector table address + 1
RAM_END         EQU $20000  ; last RAM address + 1
VIA_IORB        EQU $7A000
VIA_IORA        EQU $7A001
VIA_DDRB        EQU $7A002
VIA_DDRA        EQU $7A003
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
    JSR     INIT_VIA
    MOVE.B  #0, COUNTER
LOOP:
    MOVE.B  COUNTER, LED_PORT
    MOVE.B  VIA_IORA, VIA_IORB
    JSR     PAUSE
    ADD.B   #1, COUNTER
    BRA     LOOP

INIT_VIA:
    MOVE.B  #$00, VIA_DDRA ; PA input
    MOVE.B  #$FF, VIA_DDRB ; PB output
    RTS

PAUSE:
    MOVE.L  D0,-(SP)    ; push D0
    MOVE.L  #DELAY, D0
PAUSE_LOOP:
    SUB.L   #1, D0
    BNE.L   PAUSE_LOOP
    MOVE.L  (SP)+, D0   ; pop D0
    RTS


; RAM after vector table
    ORG VEC_TABLE_END

; Variables and constants
COUNTER: DS.B 1

    END    START            ; Last line of source
