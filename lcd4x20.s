;-----------------------------------------------------------
; Title      : LCD tst + RAM counter
; Written by : GD; Michele Fabbri
; Date       : November 1-18 2022
; Description: 
;-----------------------------------------------------------
; D1 pause counter		;	https://oldwww.nvg.ntnu.no/amiga/MC680x0_Sections/mc68000timing.HTML

; Memory map
RAM_START       EQU $10000
VEC_TABLE_END   EQU $00100  ; last vector table address + 1
RAM_END         EQU $20000  ; last RAM address + 1
LED_PORT        EQU $7C000
LCD_CMDPORT     EQU $7C100
LCD_DATAPORT    EQU $7C101
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

		move.l #LCD_CMDPORT,a0
		move.b #7,d0
		move.b (a0),d0

LCD_CLS:
    MOVE.B  #40,d2
		move.b  ' ',d1
		jsr  LCD_OUT_DATA
		DBNE d2, LCD_CLS

    MOVE.B  #40,d2
		move.b  ' ',d1
		jsr  LCD_OUT_DATA
		DBNE d2, LCD_CLS


    MOVE.B  #0, COUNTER
LOOP:
    MOVE.B  COUNTER, LED_PORT
    MOVE.L  #DELAY, D1
PAUSE:
    SUB.L   #1,D1           ; 8 cicli
    BNE.L   PAUSE           ; 8/10 cicli
    ADD.B   #1,COUNTER
    BRA     LOOP


LCD_OUT_CMD:
		move.l #LCD_CMDPORT,a0
		move.b (a0),d0
		move.b #7,d0
		move.b 1(a0),d1
		rts
LCD_OUT_DATA:
		move.l #LCD_CMDPORT,a0
		move.b (a0),d0
		move.b #7,d0
		move.b 1(a0),d1
		rts

LCDOutPort: push bc                    ; scrivo D0 nel display
                                       ; dato se CY=1, comando se CY=0
		move.l #LCD_CMDPORT,a0							; B punta al port comandi, C al dato
		move.b d0,1(a0)										; scrivo il dato

            ld  c,b
            ld  a,2h                   ; alzo E
            rla                        ; bit 0 = CY
		move.b d1,1(a0)
            
            and 0fbh                   ; abbasso E
		move.b d1,(a0)
            
            ld  a,6
		move.b d1,1(a0)
LCDDelay1:  move.b (a0),d1
            bm  LCDDelay1

LCDOut2:    pop  bc
            rts

; ---------------------------------------------------------------------
LCDPutChar: push af
            cmpi ' ' 
            bm  NoPrint

        ;    cp  80h
        ;    jp  p,NoPrint2             ; stampo tutti i char alti

            scf
            call LCDOutPort
            ld  a,(LCDX)
            inc a
            ld  (LCDX),a
            push hl
            ld  hl,LCDCols
            sub (hl)
            pop  hl
            bnz LCDPut1

            bra LCDPrintCR

NoPrint:    cp  7
            bnz NoPrint1

            call StdBeep
            bra  LCDPut1

NoPrint1:   cp  9
            bnz NoPrint12

LCDtab:     ld  a,(LCDX)
            and 7
            bz  LCDtab2

LCDtab1:    and 7
            bz  LCDPut1

LCDtab2:    inc a                       ; scrive spazi fino al prossimo TAB 8
            ld  (LCDX),a
            scf
            ld  a,' '
            call LCDOutPort
            bra  LCDtab1

NoPrint12:  cmpi  LF
            bnz NoPrint13

LCDPrintCR: xor a
            ld  (LCDX),a
            ld  a,(LCDY)
            inc a
            ld  (LCDY),a
            call LCDScroll
            bra  LCDPut1

NoPrint13:  cp  20                      ; Backspace
            jr  nz,NoPrint14

            ld  a,(LCDX)
            or  a
            bz  LCDPut1

            dec a
            ld  (LCDX),a
            ld  a,16
            or  a
            call LCDOutPort
            scf
            ld  a,' '
            call LCDOutPort
            ld  a,16
            or  a
            call LCDOutPort
            bra  LCDPut1

NoPrint14:  nop
NoPrint2:   nop

LCDPut1:    pop  af
            rts

; ---------------------------------------------------------------------
LCDXY:      push de
            push hl
            ld  (LCDY),a
            ld   de,(LCDCols)         ; A = Y, B = X
            ld   e,64                 ; PATCH per il display 4042!!
            ld   d,0
            rst  28h                  ; Moltiplica
            ld  a,b
            ld  (LCDX),a
            add a,l
            or  080h                  ; set XY
            call LCDOutPort           ; mando comando, occhio a CY!
            pop  hl
            pop  de
            rts

; ---------------------------------------------------------------------
LCDInit:    ld  a,38h
            or  a
            call LCDOutPort            ; function set, 2 linee
            ld  a,0ch
            or  a
            call LCDOutPort            ; cursore, blink e ON/OFF
            bra  LCDCls

; ---------------------------------------------------------------------
LCDCls:     xor a
            ld  (LCDX),a
            ld  (LCDY),a
            inc a
            jmp  LCDOutPort             ; cls & home (comando 1)

; ---------------------------------------------------------------------
LCDWrite:   push hl                    ; HL punta alla stringa (0-term)
LCDWrite1:  ld  a,(hl)                 
            or  a
            bz  LCDWrite2

            call LCDPutChar
            inc hl
            bra  LCDWrite1
            
LCDWrite2:  pop  hl
            rts

; ---------------------------------------------------------------------
LCDScroll:  push bc
            ld  a,(LCDY)
            push hl
            ld  hl,(LCDRows)
            cp  (hl)                       
            pop  hl
            jr  z,LCDScroll2

            jr  LCDScroll1

LCDScroll2: nop

            
            
LCDScroll1: ld  a,(LCDX)
            ld  b,a
            ld  a,(LCDY)
            call LCDXY
            
            pop  bc
            rts

StdBeep:		; fare..
            rts



; RAM after vector table
    ORG VEC_TABLE_END

; Variables and constants
COUNTER: DS.B 1
LCDX: DS.B 1
LCDY: DS.B 1

    END    START            ; Last line of source

