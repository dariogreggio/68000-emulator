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
LCD             EQU $78000
VIA             EQU $7A000
LED_PORT        EQU $7C000
DUART           EQU $7E000
ROM             EQU $80000


; LCD (ST7920)
; REGISTERS OFFSET
LCD_IR              EQU 0
LCD_DR              EQU 1

; LCD BITS
LCD_BF              EQU 7   ; BUSY FLAG


; ROM
    ORG ROM
; Constant Vector Table (2 entries)
    ; During the first 8 reads the glue logic move the following
    ; two entries of the vector table to the address $00000
    DC.L RAM_END    ; Reset Stack Pointer (SP) (end of RAM1 + 1)
    DC.L START      ; Reset Program counter (PC) (point to the beginning of code)

; Code
START:
    JSR     INIT_LCD
    LEA     HELLO_STR,A1
    JSR     LCD_PUT_STR
LOOP:
    BRA     LOOP


; BIOS

; Set mode, lines and font
; this is according to the hitachi HD44780 datasheet
; figure 24, pg 46
INIT_LCD:
    ; Function set 8 bit, 2 line, 5x8 font
    MOVE.B  #%00111000,D0
    JSR     LCD_CMD
    ; Turns on display and cursor
	MOVE.B  #%00001110,D0
    JSR     LCD_CMD
    ; Clear display
	MOVE.B  #%00000001,D0
    JSR     LCD_CMD
    ; Sets mode to increment the address by one
    ; and to shift the cursor to the right at
    ; the time of write to the DD/CGRAM.
    ; Display is not shifted.
	MOVE.B  #%00000110,D0
    JSR     LCD_CMD
    RTS

; Print the NULL terminated string at (A1) without CR, LF
LCD_PUT_STR:
    MOVE.L  D0,-(SP)            ; Save working register
LCDSTR_LOOP:
    MOVE.B  (A1)+,D0
    BEQ     LCDSTR_END
    JSR     LCD_DATA
    BRA     LCDSTR_LOOP
LCDSTR_END:
    MOVE.L  (SP)+,D0            ; Restore working register
    RTS

LCD_CMD:
    JSR     LCD_BUSY
    MOVE.B  D0,LCD_IR(A0)
    RTS

LCD_DATA:
    JSR     LCD_BUSY
    MOVE.B  D0,LCD_DR(A0)
    RTS

LCD_BUSY:
    LEA     LCD,A0
LCDBUSY_LOOP:
    BTST    #LCD_BF,LCD_IR(A0)  ; Test BF
    BNE     LCDBUSY_LOOP        ; Until LCD is ready (BF zero -> ready)
    RTS

; Constants must be after code in order to
; keep instructions at the right alignment
HELLO_STR:  DC.B 'Hello World!',0


; RAM after vector table
    ORG VEC_TABLE_END

; Variables and constants

    END    START            ; Last line of source
