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
VIA             EQU $7A000
LED_PORT        EQU $7C000
DUART           EQU $7E000
ROM             EQU $80000

; VIA
; REGISTERS ADDRESSES
VIA_IORB        EQU 0
VIA_IORA        EQU 1
VIA_DDRB        EQU 2
VIA_DDRA        EQU 3

; DUART
; REGISTERS ADDRESSES
DUART_MR1A      EQU 0
DUART_MR2A      EQU 0
DUART_SRA       EQU 1
DUART_CSRA      EQU 1
DUART_CRA       EQU 2
DUART_RBA       EQU 3
DUART_TBA       EQU 3
DUART_ACR       EQU 4
; REGISTERS BITS
DUART_RXRDY     EQU 0
DUART_TXRDY     EQU 2

; Directives
CR              EQU $0D
LF              EQU $0A
DELAY           EQU 32000

; ROM
    ORG ROM
; Constant Vector Table (2 entries)
    ; During the first 8 reads the glue logic move the following
    ; two entries of the vector table to the address $00000
    DC.L RAM_END    ; Reset Stack Pointer (SP) (end of RAM1 + 1)
    DC.L START      ; Reset Program counter (PC) (point to the beginning of code)

; Code
START:
    JSR     INIT_DUART
    JSR     INIT_VIA
    LEA     HELLO_STR,A1
    JSR     PUT_STR
    LEA     READY_STR,A1
    JSR     PUT_STR
    MOVE.B  #0,COUNTER
LOOP:
    JSR     GET_CHAR
    JSR     PUT_CHAR
    JSR     MOVE_COUNTER_LED
    ;JSR     MOVE_PA_PB
    ;JSR     PAUSE
    ADD.B   #1,COUNTER
    BRA     LOOP

MOVE_PA_PB:
    LEA     VIA,A0
    MOVE.B  VIA_IORA(A0),VIA_IORB(A0)
    RTS

MOVE_COUNTER_LED:
    MOVE.B  COUNTER,LED_PORT
    RTS

PAUSE:
    MOVE.L  D0,-(SP)    ; push D0
    MOVE.L  #DELAY,D0
PAUSE_LOOP:
    SUB.L   #1,D0
    BNE.L   PAUSE_LOOP
    MOVE.L  (SP)+,D0   ; pop D0
    RTS

; BIOS

; Initiliaze
INIT_VIA:
    LEA     VIA,A0
    MOVE.B  #$00,VIA_DDRA(A0) ; PA input
    MOVE.B  #$FF,VIA_DDRB(A0) ; PB output
    RTS

; Initialize DUART port A to
; 9600 baud, 8-bits, no parity, no flow control
INIT_DUART:
    LEA     DUART,A0

    ; The following three instruction are are not necessary
    ; after a hardware resert to the DUART    ;
    MOVE.B  #$30,DUART_CRA(A0)  ; Reset port A transmitter
    MOVE.B  #$20,DUART_CRA(A0)  ; Reset port A receiver
    MOVE.B  #$10,DUART_CRA(A0)  ; Reset port A MR (mode register) pointer

    ; Select baud rate, data format and operationg mode
    ; by setting up the ACR, MR1 and MR2 registers
    MOVE.B  #$00,DUART_ACR(A0)  ; Select baud rate set 1
    MOVE.B  #$BB,DUART_CSRA(A0) ; Set both Rx and Tx speed to 9600 baud
    MOVE.B  #$13,DUART_MR1A(A0) ; Set port A to 8-bits, no parity,
                                ; disable RxRTS output
    MOVE.B  #$07,DUART_MR2A(A0) ; Select normal operating mode, disable
                                ; TxRTS, TxCTS, one stop bit
    ; Enable transmission
    MOVE.B  #$05,DUART_CRA(A0)  ; Enable port A transmitter and receiver
    RTS

; Input a single character from port A (polled mode) into D0
GET_CHAR:
    MOVE.L  D1,-(SP)            ; Save working register
    LEA     DUART,A0            ; A0 points to DUART base address
GETCR_POLL:
    MOVE.B  DUART_SRA(A0),D1    ; Read the port A status register
    BTST    #DUART_RXRDY,D1     ; Test receiver ready status
    BEQ     GETCR_POLL          ; Until character received
    MOVE.B  DUART_RBA(A0),D0    ; Read the character received by port A
    MOVE.L  (SP)+,D1            ; Restore working register
    RTS

; Transmit a single character in D0 from Port A (polled mode)
PUT_CHAR:
    MOVE.L  D1,-(SP)            ; Save working register
    LEA     DUART,A0            ; A0 points to DUART base address
PUTCR_POLL:
    MOVE.B  DUART_SRA(A0),D1    ; Read port A status register
    BTST    #DUART_TXRDY,D1     ; Test transmitter ready status
    BEQ     PUTCR_POLL          ; Until transmitter ready
    MOVE.B  D0,DUART_TBA(A0)    ; Transmit charachter from port A
    MOVE.L  (SP)+,D1            ; Restore working register
    RTS

; Transmit the NULL terminated string at (A1) without CR, LF
PUT_STR:
    MOVE.L  D0,-(SP)            ; Save working register
PUTSTR_LOOP:
    MOVE.B  (A1)+,D0
    BEQ     PUTSTR_END
    JSR     PUT_CHAR
    BRA     PUTSTR_LOOP
PUTSTR_END:
    MOVE.L  (SP)+,D0            ; Restore working register
    RTS

; Constants must be after code in order to
; keep instructions at the right alignment
HELLO_STR:  DC.B 'Rotol68k',CR,0
READY_STR:  DC.B 'Ready',CR,0


; RAM after vector table
    ORG VEC_TABLE_END

; Variables and constants
COUNTER: DS.B 1

    END    START            ; Last line of source
