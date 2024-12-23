//http://www.dilwyn.me.uk/docs/ebooks/olqlug/QL%20Manual%20-%20Concepts.htm#memorymap



#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <xc.h>

#include "Adafruit_ST77xx.h"
#include "Adafruit_ST7735.h"
#include "adafruit_gfx.h"

#include "68000_PIC.h"


extern struct ADDRESS_EXCEPTION AddressExcep,BusExcep;
extern union WORD_BYTE Pipe1;
extern union __attribute__((__packed__)) PIPE Pipe2;

#ifdef QL
BYTE __attribute((aligned(65536))) ram_seg[RAM_SIZE];
#warning RAM_SEG ALIGNED x debug!
BYTE *rom_seg;
BYTE *rom_seg2;
#define VIDEO_ADDRESS_BASE 0x20000
DWORD VideoAddress=VIDEO_ADDRESS_BASE;
BYTE VideoMode;
SWORD VICRaster=0;
union __attribute__((__packed__)) DWORD_BYTE RTC;
BYTE IPCW=255,IPCR=0, IPCData=0,IPCCnt=4,IPCState=0;
BYTE MDVmotor=0;
#elif MICHELEFABBRI
BYTE ram_seg[RAM_SIZE];
BYTE *rom_seg;
BYTE i68681RegR[16],i68681RegW[16];
BYTE LCDPortR[2],LCDPortW[2];
BYTE LCDram[256 /*4*40*/],LCDCGDARAM=0,LCDCGram[64],LCDCGptr=0,LCDfunction=0,LCDdisplay=0,
	LCDentry=2 /* I/D */,LCDcursor=0;			// emulo LCD text come Z80net
signed char LCDptr=0;
BYTE IOExtPortI[4],LedPort[1];
BYTE i146818RegR[2],i146818RegW[2],i146818RAM[64];
BYTE i8042RegR[2],i8042RegW[2];
#else
BYTE ram_seg[RAM_SIZE];
BYTE rom_seg[ROM_SIZE];			
BYTE i8255RegR[4],i8255RegW[4];
BYTE LCDram[256 /*4*40*/],LCDCGDARAM=0,LCDCGram[64],LCDCGptr=0,LCDfunction,LCDdisplay,
	LCDentry=2 /* I/D */,LCDcursor;			// emulo LCD text 
signed char LCDptr=0;
BYTE IOExtPortI[4],IOExtPortO[4];
BYTE IOPortI,IOPortO,ClIRQPort,ClWDPort;
/*Led1Bit        equ 7
Led2Bit        equ 6
SpeakerBit     equ 5
WDEnBit        equ 3
ComOutBit2     equ 2
NMIEnBit       equ 1
ComOutBit      equ 0
Puls1Bit       equ 7
ComInBit2      equ 5
DipSwitchBit   equ 1
ComInBit       equ 0*/
BYTE i146818RegR[2],i146818RegW[2],i146818RAM[64];
BYTE i8042RegR[2],i8042RegW[2];
#endif
//extern BYTE KBDataI,KBDataO,KBControl,/*KBStatus,*/ KBRAM[32];   // https://wiki.osdev.org/%228042%22_PS/2_Controller
//#define KBStatus KBRAM[0]   // pare...
extern BYTE Keyboard[3];
extern volatile BYTE TIMIRQ,VIDIRQ,KBDIRQ,SERIRQ,RTCIRQ,MDVIRQ;
BYTE IRQenable=0;




#ifdef QL
#define IO_BASE_ADDRESS 0x18000
#else
#define IO_BASE_ADDRESS 0x7c000
#define LCD_BOARD_ADDRESS 0x7c100
// emulo display LCD testo (4x20, 4x20 o altro) come su scheda Z80:
//  all'indirizzo+0 c'è la porta comandi (fili C/D, RW e E (E2)), a indirizzo+1 la porta dati (in/out)
#define IO_BOARD_ADDRESS 0x0
#endif

BYTE GetValue(DWORD t) {
	register BYTE i;

	if(t < ROM_SIZE) {			// 
		i=rom_seg[t];
		}
#ifdef ROM_SIZE2
	else if(t < ROM_SIZE2) {			// cartridge
		i=rom_seg[t-ROM_SIZE];
		}
#endif
	else if(t >= RAM_START && t < (RAM_START+RAM_SIZE)) {
		i=ram_seg[t-RAM_START];
		}
  
#ifdef QL
  
  else if(t>=IO_BASE_ADDRESS && t<IO_BASE_ADDRESS+0x100) {       // I/O
    switch(t & 0x000ff) {
      case 0: //   RTC SOLO LONG!
        break;
        
      case 2:        //   Transmit control;
        break;
        
      case 3:        //   IPC Rd
        i=IPCW;     // mah dice qua solo WR..  scrive 0x0e, pare
        // (tastiera ecc va qua...)  scrive 4 o 8 bit, data 1=$0E or 0=$0C
//				IPCCnt++;
        
        break;
        // 0x0d è IPC per seriale_baud_rate, 0x01 è report input status, 6=read ser1, 7=read ser2, 8=read keyboard

      case 0x20:        //   microdrive/RS232 link status
//        i=IPCR;
        // (tastiera, beep ecc va qua...) https://www.sinclairql.net/srv/qlsm1.html#s1-p4

        i=0b01000000 | (IPCData & 0b10000000);      // finto handshake :) in effeti servirebbe SOLO dopo una write
        // la lettura non aspetta (giustamente)
        
        i=0b00000000 | (IPCData & 0b10000000);      // handshake b6=0; b7 è il dato, 

				//ne legge 4 o 8 (dopo aver scritto in 18003) di seguito e accumula (MSB first)
        break;

      case 0x21:        //   interrupt/IPC link status
//    B 	M 	R 	X 	F 	T 	 I  	G 	 B=Baud state, M=Microdrive inactive, R=Rtc state, X=eXternal Interrupt, F=Frame vsync, T=Transmit interrupt, I=IPC Interface interrupt G=Gap interrupt (microdrive)
        i=VIDIRQ ? 0b00001000 : 0; 
        i |= KBDIRQ ? 0b00000010 : 0; 
//        i |= SERIRQ ? 0b00000100 : 0; 
        i |= MDVIRQ ? 0b00000001 : 0; 
        
//        i |= EXTIRQ ? 0b00010000 : 0; 

//        IPL=0;    //boh
        
        
        break;
    
      case 0x22:
      case 0x23:        //   microdrive
        break;

      case 0x63:        //   video mode (dice 063 o anche 023...
    /*
$18023  Microdrive data (track 2) Display control
$18022  Microdrive data (track 1)  Microdrive/RS-232-C data
$18021  Interrupt/lPC link status Interrupt control
$18020  Microdrive/RS-232-C status  Microdrive control
$18003  Real-time clock byte 3  IPC link control
$18002  Real-time clock byte 2  Transmit control
$18001  Real-time clock byte 1  Real-time clock step
$18000  Real-time clock byte 0  Real-time clock reset*/

        i=VideoMode;
        break;
      }
    }

  
#elif MICHELEFABBRI
  
  else if(t==IO_BASE_ADDRESS) {        //   led
    i=LedPort[0];      // OCCHIO nel suo hw non c'è! io lo metto per test lettura/rotate
    }
  else if(t==LCD_BOARD_ADDRESS) {        //   lcd
    i=LCDPortR[0];
    }
  else if(t==LCD_BOARD_ADDRESS+1) {        //
    i=LCDPortR[1];
    }
  
#else
  
  else if(t>=IO_BASE_ADDRESS && t<(IO_BASE_ADDRESS+0x20)) {        //   display mappato qua (su 68000 non ci sono IN & OUT)
    t-=IO_BASE_ADDRESS;
    switch(t) {
      case IO_BOARD_ADDRESS:
      case IO_BOARD_ADDRESS+1:
      case IO_BOARD_ADDRESS+2:
      case IO_BOARD_ADDRESS+3:
        i=IOExtPortI[t-IO_BOARD_ADDRESS];
        break;
  //    case 0x0e:      // board signature...
  //#warning TOGLIERE QUA!
  //      return 0x68;      // LCD
  //      break;

      case LCD_BOARD_ADDRESS:
        // per motivi che ora non ricordo, il BIOS indirizza 0..3 mentre la 2°EEprom (casanet) aveva indirizzi doppi o meglio bit 0..1 messi a 2-0
        // potrei ricompilare casanet per andare "dritto" 0..3, per ora lascio così (unico problema è conflitto a +6 con la tastiera... amen! tanto scrive solo all'inizio)
        // if(i8255RegR[2] & 0b01000000) fare...?
        // else 			i=i8255RegW[0];
        if(!(i8255RegR[1] & 1))			// se status...
          i8255RegR[0]=0 | (LCDptr & 0x7f); //sempre ready!
        else {
          if(!LCDCGDARAM) {
            i8255RegR[0]=LCDram[LCDptr]; //
            if(LCDentry & 2) {
              LCDptr++;
              if(LCDptr >= 0x7f)			// parametrizzare! o forse no, fisso max 127
                LCDptr=0;
              }
            else {
              LCDptr--;
              if(LCDptr < 0)			// parametrizzare!
                LCDptr=0x7f;    // CONTROLLARE
              }
            }
          else {
            i8255RegR[0]=LCDCGram[LCDptr++]; //
            LCDCGptr &= 0x3f;
            }
          }
        i=i8255RegR[0];
        break;
      case LCD_BOARD_ADDRESS+2:
        i=i8255RegR[1];
        break;
      case LCD_BOARD_ADDRESS+4:
        // qua c'è la 3° porta del 8255
        // il b6 mette a in o out la porta dati A (1=input)
        i=i8255RegR[2];
        break;
      case LCD_BOARD_ADDRESS+5 /* sarebbe 6 ma ovviamente non si può! v. sopra*/:
        // 8255 settings
        i=i8255RegR[3];
        break;

      case LCD_BOARD_ADDRESS+6:   // 
        if(i8042RegW[1]==0xAA) {      // self test
          i8042RegR[0]=0x55;
          }
        else if(i8042RegW[1]==0xAB) {      // diagnostics
          i8042RegR[0]=0b00000000;
          }
        else if(i8042RegW[1]>=0x20 && i8042RegW[1]<0x40) {
          i8042RegR[0]=KBRAM[i8042RegW[1] & 0x1f];
          }
        else if(i8042RegW[1]>=0x60 && i8042RegW[1]<0x80) {
          //KBRAM[i8042RegW[1] & 0x1f]
          }
        else if(i8042RegW[1]==0xC0) {
          i8042RegR[0]=KBStatus;
          }
        else if(i8042RegW[1]==0xD0) {
          i8042RegR[0]=KBControl;
          }
        else if(i8042RegW[1]==0xD1) {
          }
        else if(i8042RegW[1]==0xD2) {
          }
        else if(i8042RegW[1]>=0xF0 && i8042RegW[1]<=0xFF) {
          }
        i=i8042RegR[0];
        break;
      case LCD_BOARD_ADDRESS+7:
        i=i8042RegR[1];
        i=0; // non-busy
        break;
      case LCD_BOARD_ADDRESS+0xe:
        i=0x68; // LCD signature
        break;
      }
		}
  else if(t==0xe000) {        //
//    IOPortI = 0b00011100;      // dip-switch=0001; v. main
//    IOPortI |= 0b00000001;      // ComIn
    if(PORTDbits.RD2)
      IOPortI |= 0x80;      // pulsante
    else
      IOPortI &= ~0x80;
    i=IOPortI;
    }
  else if(t>=0xe002 && t<=0xe003) {        //   CMOS RAM/RTC (Real Time Clock  MC146818)
    t &= 0x1;
    switch(t) {
      case 0:
        i=i146818RegR[0];
        break;
      case 1:     // il bit 7 attiva/disattiva NMI boh??
        switch(i146818RegW[0] & 0x3f) {
          case 0:
            i146818RegR[1]=currentTime.sec;
            break;
            // in mezzo c'è Alarm...
          case 2:
            i146818RegR[1]=currentTime.min;
            break;
          case 4:
            i146818RegR[1]=currentTime.hour;
            break;
            // 6 è day of week...
          case 7:
            i146818RegR[1]=currentDate.mday;
            break;
          case 8:
            i146818RegR[1]=currentDate.mon;
            break;
          case 9:
            i146818RegR[1]=currentDate.year;
            break;
          case 12:
						i146818RegR[1]=i146818RAM[12] = 0;   // flag IRQ
            break;
          default:      // qua ci sono i 4 registri e poi la RAM
            i146818RegR[1]=i146818RAM[i146818RegW[0] & 0x3f];
            break;
          }
        i=i146818RegR[1];
        break;
      }
    }
  else if(t==0xe004) {
    ClIRQPort;
    }
  else if(t==0xe005) {
    ClWDPort;
    }
  
#endif
  else {
    BusExcep.active=1;
    BusExcep.addr=t;
    BusExcep.descr.in=1;
    BusExcep.descr.rw=1;
    // finire...
    }
  
	return i;
	}

SWORD GetShortValue(DWORD t) {
	union PIPE pipe;

  if(t & 1) {
    AddressExcep.active=1;
    AddressExcep.addr=t;
    AddressExcep.descr.in=1;
    AddressExcep.descr.rw=1;
    return 0;
    }
	if(t < ROM_SIZE-1) {			// 
    pipe.bd[0]=rom_seg[t+1];
    pipe.bd[1]=rom_seg[t+0];
		}
#ifdef ROM_SIZE2
	else if(t < ROM_SIZE2-1) {			// cartridge
		t-=ROM_SIZE;
		i=MAKEWORD(rom_seg[t+1],rom_seg[t]);
		}
#endif
	else if(t >= RAM_START && t < (RAM_START+RAM_SIZE-1)) {
		t-=RAM_START;
    pipe.bd[0]=ram_seg[t+1];
    pipe.bd[1]=ram_seg[t+0];
		}
#ifdef QL
  else if(t>=IO_BASE_ADDRESS && t<IO_BASE_ADDRESS+0x100) {        //   I/O
    }
#elif MICHELEFABBRI
#endif
  else {
    BusExcep.active=1;
    BusExcep.addr=t;
    BusExcep.descr.in=1;
    BusExcep.descr.rw=1;
    // finire...
    }
	return pipe.wd[0];
	}

DWORD GetIntValue(DWORD t) {
	union PIPE pipe;

  if(t & 1) {
    AddressExcep.active=1;
    AddressExcep.addr=t;
    AddressExcep.descr.in=1;
    AddressExcep.descr.rw=1;
    return 0;
    }
	if(t < ROM_SIZE-3) {			// 
    pipe.bd[0]=rom_seg[t+3];
    pipe.bd[1]=rom_seg[t+2];
    pipe.bd[2]=rom_seg[t+1];
    pipe.bd[3]=rom_seg[t+0];
		}
#ifdef ROM_SIZE2
	else if(t < ROM_SIZE2-3) {			// cartridge
		t-=ROM_SIZE;
		i=MAKELONG(MAKEWORD(rom_seg[t+3],rom_seg[t+2]),MAKEWORD(rom_seg[t+1],rom_seg[t+0]));
		}
#endif
#ifdef QL
  else if(t>=IO_BASE_ADDRESS && t<IO_BASE_ADDRESS+0x100) {       // I/O
    switch(t & 0x000ff) {
      case 0:
        pipe.bd[0]=RTC.b[3];
        pipe.bd[1]=RTC.b[2];
        pipe.bd[2]=RTC.b[1];
        pipe.bd[3]=RTC.b[0];
        break;
      }
    }
#elif MICHELEFABBRI
#endif
	else if(t >= RAM_START && t < (RAM_START+RAM_SIZE-3)) {
		t-=RAM_START;
    pipe.bd[0]=ram_seg[t+3];
    pipe.bd[1]=ram_seg[t+2];
    pipe.bd[2]=ram_seg[t+1];
    pipe.bd[3]=ram_seg[t+0];
		}
  else {
    BusExcep.active=1;
    BusExcep.addr=t;
    BusExcep.descr.in=1;
    BusExcep.descr.rw=1;
    // finire...
    }
	return pipe.d;
	}

SWORD GetPipe(DWORD t) {
  SWORD *p;

  if(t & 1) {
    AddressExcep.active=1;
    AddressExcep.addr=t;
    AddressExcep.descr.in=0;
    AddressExcep.descr.rw=1;
    return;
    }
	if(t < ROM_SIZE-3) {			// 
    if(!(t & 3))
      ;
#warning se indirizzo è long aligned, si potrebbero leggere 2 SWORD in un colpo solo VERIFICARE test e velocità
		Pipe1.b[0]=rom_seg[t+1];
    Pipe1.b[1]=rom_seg[t];
		Pipe2.bd[0]=rom_seg[t+5];
    Pipe2.bd[1]=rom_seg[t+4];
		Pipe2.bd[2]=rom_seg[t+3];
    Pipe2.bd[3]=rom_seg[t+2];
//		Pipe2.ww.l=MAKEWORD(rom_seg[t+1],rom_seg[t]);
		}
	else if(t >= RAM_START && t < (RAM_START+RAM_SIZE-3)) {
    if(!(t & 3))
      ;
		Pipe1.b[0]=ram_seg[t+1];
    Pipe1.b[1]=ram_seg[t];
		Pipe2.bd[0]=ram_seg[t+5];
    Pipe2.bd[1]=ram_seg[t+4];
		Pipe2.bd[2]=ram_seg[t+3];
    Pipe2.bd[3]=ram_seg[t+2];
		}
  else {
    BusExcep.active=1;
    BusExcep.addr=t;
    BusExcep.descr.in=0;
    BusExcep.descr.rw=1;
    // finire...
    }
	return Pipe1.w;
	}

BYTE AdvPipe(DWORD t,BYTE n) {    // questa va usata quando il primo parm dell'istruzione è esplicito! NON per ils econdo (verificare)
  
  if(t & 1) {
    AddressExcep.active=1;
    AddressExcep.addr=t;
    AddressExcep.descr.in=0;
    AddressExcep.descr.rw=1;
    return;
    }
  switch(n) {
    case 2:
  		Pipe2.wd[1]=Pipe2.wd[0];
      if(t < ROM_SIZE-5) {			// 
        Pipe2.bd[0]=rom_seg[t+5];
        Pipe2.bd[1]=rom_seg[t+4];
        }
      else if(t >= RAM_START && t < (RAM_START+RAM_SIZE-5)) {
        Pipe2.bd[0]=ram_seg[t+5];
        Pipe2.bd[1]=ram_seg[t+4];
        }
      else {
        BusExcep.active=1;
        BusExcep.addr=t;
        BusExcep.descr.in=0;
        BusExcep.descr.rw=1;
        // finire...
        }
      break;
    case 4:
      if(t < ROM_SIZE-7) {			// 
        if(!(t & 3))
          ;
    #warning se indirizzo è long aligned, si potrebbero leggere 2 SWORD in un colpo solo VERIFICARE test e velocità
        Pipe2.bd[0]=rom_seg[t+7];
        Pipe2.bd[1]=rom_seg[t+6];
        Pipe2.bd[2]=rom_seg[t+5];
        Pipe2.bd[3]=rom_seg[t+4];
        }
      else if(t >= RAM_START && t < (RAM_START+RAM_SIZE-7)) {
        if(!(t & 3))
          ;
        Pipe2.bd[0]=ram_seg[t+7];
        Pipe2.bd[1]=ram_seg[t+6];
        Pipe2.bd[2]=ram_seg[t+5];
        Pipe2.bd[3]=ram_seg[t+4];
        }
      else {
        BusExcep.active=1;
        BusExcep.addr=t;
        BusExcep.descr.in=0;
        BusExcep.descr.rw=1;
        // finire...
        }
      break;
    }
	return n;
	}

#if 0
DWORD GetPipeExt(DWORD t) {
// NON è sempre 6 byte dopo! ma uso cmq le word aggiuntive di Pipe2, e restituisco...
  // diciamo che la uso sempre dopo il +2 iniziale, quindi mi posto a ulteriori +4 e leggo
  if(t & 1) {
    DoAddressExcep=1;
    return;
    }
	if(t < ROM_SIZE) {			// 
    if(!(t & 3))
      ;
#warning se indirizzo è long aligned, si potrebbero leggere 2 SWORD in un colpo solo VERIFICARE test e velocità
    t+=4;
		Pipe2.wd[3]=MAKEWORD(rom_seg[t+1],rom_seg[t]);
    t+=2;
		Pipe2.wd[2]=MAKEWORD(rom_seg[t+1],rom_seg[t]);
		}
	else if(t >= RAM_START && t < (RAM_START+RAM_SIZE)) {
    if(!(t & 3))
      ;
		Pipe2.wd[3]=MAKEWORD(ram_seg[t+1],ram_seg[t]);
    t+=2;
		Pipe2.wd[2]=MAKEWORD(ram_seg[t+1],ram_seg[t]);
		}
	return Pipe2.dd[1];
	}
#endif

void PutValue(DWORD t,BYTE t1) {
	register SWORD i;

// printf("rom_seg: %04x, p: %04x\n",rom_seg,p);

	if(t >= RAM_START && t < (RAM_START+RAM_SIZE)) {		// 
	  ram_seg[t-RAM_START]=t1;
		}
#ifdef QL
  
  else if(t>=IO_BASE_ADDRESS && t<IO_BASE_ADDRESS+0x100) {       // I/O
    switch(t & 0x000ff) {
      case 0: //   RTC SOLO LONG!
        break;
        
      case 2:        //   Transmit control; port 2, P21 è BEEP MA FORSE a 32bit!
        break;
        
      case 3:        //   IPC Wr
//        IPCW=IPCR=t1;
        // (tastiera ecc va qua...)  https://www.sinclairql.net/srv/qlsm1.html#s1-p4
        // (no! scrive 0x0c per video irq frame)
        // 0x0d è IPC per seriale_baud_rate, 0x01 è report input status, 6=read ser1, 7=read ser2, 8=read keyboard
        // scrive 4 o 8 bit, data 1=$0E or 0=$0C
        if(t1==1) {      // boh lo fa al boot.. io ripristino
          IPCW=255; IPCR=IPCData=0; IPCCnt=4; IPCState=0;
          }
        else {
          switch(IPCW) {
            case 255:   // scrittura cmd
              if(IPCCnt==4)     // pulisco ora, non avendo potuto pulire a fine evento prec. (v. sotto)
                IPCData=0;
              if((t1 & 0x0c) == 0x0c) {
//                IPCData <<= 1;
                IPCCnt--;
                IPCData |= t1 & 2 ? (1 << IPCCnt) : 0;
        //				IPCW <<= 1;
        //				IPCW |= t1 & 2 ? 1 : 0;
                IPCData &= 0xf; //patch perché ho IPCdata in canna da prima...
                if(!IPCCnt) {
                  switch(IPCData) {  // if(!IPCCnt MA LO SHIFT è IN READ...
                    case 0x01:
                      IPCW=0x01;      // read status
                      IPCData=0x01;       // b6=microdrive write protect, b1=sound, b0=keyboard, b4=seriale1, b5=seriale2
                      IPCCnt=8;
                      IPCState=0;
                      break;
                    case 0x06:
                      IPCW=0x06;
                      IPCCnt=8;
                      IPCState=0;
                      break;
                    case 0x07:
                      IPCW=0x07;
                      IPCCnt=8;
                      IPCState=0;
                      break;
                    case 0x08:      // read keyboard
                      IPCW=0x08;
                      IPCData=Keyboard[2] << 4;       // num tasti, 4bit
                      IPCCnt=4;
                      IPCState=0;
                      break;
                    case 0x0d:      // write baud rate
                      IPCW=0x0d;
                      IPCCnt=4;       // ma QUA DEVO RICEVERE ALTRI 4 BIT!! fare!
                      IPCData=0;
                      IPCState=0;
                      break;
                    default:
                      IPCW=255; IPCData=0;    // in caso di robe strane, mi rimetto a leggere!
                      IPCCnt=4;
                      IPCState=0;
                      break;
                    }
                  IPCData = (IPCData >> 1) | (IPCData & 0x1 ? 0x80 : 0);  // ROTate perché sono avanti di 1 (v.sopra)
                  }
                }
              break;
            case 0x0d:
              if((t1 & 0x0c) == 0x0c) {
                IPCData <<= 1;
                IPCCnt--;
                IPCData |= t1 & 2 ? (1 << IPCCnt) : 0;
                if(!IPCCnt) {
                  // usare il valore... gestire                  IPCData &= 0xf; //patch perché ho IPCdata in canna da prima...
                  IPCW=255; IPCCnt=4;
                  }
                }
              break;
            default:    // lettura dato
              if((t1 & 0x0e) == 0x0e) {     // preparo lettura (v.)
                if(!IPCCnt) {
                  switch(IPCW) {
                    case 0x08:      // read keyboard
          //				PRIMA 4bit cnt (b3=repeat key, b2-0 number of keys), POI 4 bit modifier (b4=overflow, b2=SHIFT, b1=CTRL, b0=ALT) e POI 8 bit tasto(i)!
                      IPCState++;
                      switch(IPCState) {
                        case 0:     // v. sopra, qua ci ripasso forse... NO!
                          IPCData=Keyboard[2] << 4;       // num tasti, 4bit
                          IPCCnt=4-1;
                          break;
                        case 1:
                          IPCData=Keyboard[1] << 4;       // modifier, 4bit
                          IPCCnt=4-1;
                          break;
                        case 2:
                          IPCData=Keyboard[0];       // tasti, 8bit, per ora solo 1!
                          IPCCnt=8-1;
                          break;
                        }
                      break;
                    case 0:
                      break;
                    default /*255*/:      // qua ci passiamo anche quando scrive (primo giro) per creare IPCW..
                      break;
                    }
//                  IPCData = (IPCData >> 1) | (IPCData & 0x1 ? 0x80 : 0);  // ROTate perché sono avanti di 1 (v.sopra)
                  }
                else {
                  IPCData = (IPCData << 1) | (IPCData & 0x80 ? 1 : 0);  // ROTate perché sono avanti di 1 (v.sopra)
                  switch(IPCW) {
                    case 0x01:      // read status
                      IPCCnt--;
                      if(!IPCCnt) {
                        IPCW=255; IPCCnt=4; // troppo presto! IPCData=0;
                        }
                      break;
                    case 0x06:      // read serial 1
                      IPCCnt--;
                      if(!IPCCnt) {
                        IPCW=255; IPCCnt=4; // troppo presto! IPCData=0;
                        IPCState++;     // GESTIRE Stati poi
                        }
                      break;
                    case 0x07:      // read serial 2
                      IPCCnt--;
                      if(!IPCCnt) {
                        IPCW=255; IPCCnt=4; // troppo presto! IPCData=0;
                        IPCState++;     // GESTIRE Stati poi
                        }
                      break;
                    case 0x08:      // read keyboard
                      switch(IPCState) {
                        case 0:     // num tasti e modifier 4 bit
                          IPCCnt--;
                          if(!IPCCnt) {
                            if(Keyboard[2] & 7) {    // se non ci sono tasti, smetto subito
//                              IPCState++;
//                              IPCCnt=4;
                              }
                            else {
                        IPCW=255; IPCCnt=4; // troppo presto! IPCData=0;
                              IPCState=0;
  //                              Keyboard[0]=Keyboard[1]=Keyboard[2]=0;
                              }
                            }
                          break;
                        case 1:
                          IPCCnt--;
                          if(!IPCCnt) {
//                            IPCState++;
//                            IPCCnt=8;
                            }
                          break;
                        case 2:
                          IPCCnt--;
                          if(!IPCCnt) {
                            Keyboard[2]--;
                            if(Keyboard[2] & 7) {    // se non ci sono + tasti, smetto 
                              IPCState=0;     // rimando anche modifier
//                              IPCCnt=8;
                              }
                            else {
                        IPCW=255; IPCCnt=4; // troppo presto.. IPCData=0;
                            IPCState=0;
  //                              Keyboard[0]=Keyboard[1]=Keyboard[2]=0;
                            }
                            }
                          break;
                        default:
                          IPCState=0;
                          break;
                        }
                      break;
                    case 0x0d:      // set baud rate
                      IPCCnt--;
                      if(!IPCCnt) {
                        IPCW=255; IPCCnt=4; // troppo presto! IPCData=0;
                        IPCState=0;
                        }
                      break;
                    case 255:
                      break;
                    default:      // safety...?
                      if(IPCCnt) {
                        IPCCnt--;
                        if(!IPCCnt) {
                          IPCW=255; IPCCnt=4; IPCData=0;
                          IPCState=0;
                          }
                        }
                      break;
                    }
                  }
                }
              break;
            }
          }

//          VIDIRQ=0; KBDIRQ=0;
//          IPL=0;  //DOVE metterlo??
          
        break;
        
      case 0x20:        //   microdrive/RS232 link status
//2; 0; 3; 1; 2 0 2 

				// scrive 3 e altro, a L5478 e L2D74 e L2B82 L2C56(motor)
        MDVmotor=t1;
        break;

      case 0x21:        //   interrupt/IPC link status
//  R 	F 	M 	X 	F 	T 	I  	G 	 R=tRansmit mask, F=interFace mask, M=gap Mask,
        //X=reset eXternal Interrupt,F=reset Frame vsync, T=reset Transmit interrupt, I=reset IPC Interface interrupt G=reset Gap interrupt 
//	verificare pc.intre quanto vale! scrive per pulire ;  pc.intri=2, pc.intrf=8, pc.intrt=4
        if(t1 & 0b00001000)
          VIDIRQ=0; 
        if(t1 & 0b00000010)
          KBDIRQ=0; 
        if(t1 & 0b00000100)
          SERIRQ=0; 
        if(t1 & 0b00000001)
          MDVIRQ=0;
        // i 3 bit alti sono "enable"... seriale, interface, microdrive
        IRQenable=t1 & 0xe0;
        break;
    
      case 0x22:
      case 0x23:        //   microdrive
        break;

      case 0x63:        //   video mode
        VideoMode=t1;
        if(VideoMode & 0b10000000)
          VideoAddress=0x28000;
        else
          VideoAddress=VIDEO_ADDRESS_BASE;
        break;
      }
    }
  
#elif MICHELEFABBRI
  else if(t==IO_BASE_ADDRESS) {        //   led
    LedPort[0]=t1;
    }
  else if(t==LCD_BOARD_ADDRESS) {        //   lcd
    LCDPortW[0]=LCDPortR[0]=t1;
    }
  else if(t==LCD_BOARD_ADDRESS+1) {        //
    if(LCDPortW[0] & 4 && !(t1 & 4)) {   // quando E scende
      // in teoria dovremmo salvare in R[0] solo in questo istante, la lettura... ma ok! (v. sopra)
      LCDPortR[1] |= 0x80;   // Busy, tanto per :)
      if(LCDPortW[0] & 1)	{		// se dati
        if(!LCDCGDARAM) {
          LCDram[LCDptr]=LCDPortW[1];
          if(LCDentry & 2) {
            LCDptr++;
            if(LCDptr >= 0x7f)			// parametrizzare! o forse no, fisso max 127
              LCDptr=0;
            }
          else {
            LCDptr--;
            if(LCDptr < 0)			// parametrizzare!
              LCDptr=0x7f;    // CONTROLLARE
            }
          }
        else {
          LCDCGram[LCDCGptr++]=LCDPortW[1];
          LCDCGptr &= 0x3f;
          }
        }
      else {									// se comandi https://www.sparkfun.com/datasheets/LCD/HD44780.pdf
        if(LCDPortW[1] <= 1) {			// CLS/home
          LCDptr=0;
          LCDentry |= 2;		// ripristino INCremento, dice
          memset(LCDram,' ',sizeof(LCDram) /* 4*40 */);
          }
        else if(LCDPortW[1] <= 3) {			// home
          LCDptr=0;
          }
        else if(LCDPortW[1] <= 7) {			// entry mode ecc
          LCDentry=LCDPortW[1] & 3;
          }
        else if(LCDPortW[1] <= 15) {			// display on-off ecc
          LCDdisplay=LCDPortW[1] & 7;
          }
        else if(LCDPortW[1] <= 31) {			// cursor set, increment & shift off
          LCDcursor=LCDPortW[1] & 15;
          }
        else if(LCDPortW[1] <= 63) {			// function set, 2-4 linee
          LCDfunction=LCDPortW[1] & 31;
          }
        else if(LCDPortW[1] <= 127) {			// CG RAM addr set
          LCDCGptr=LCDPortW[1] & 0x3f;			// 
          LCDPortR[1] = LCDram[LCDCGptr] | 0x80;    // finire!
          LCDCGDARAM=1;// user defined char da 0x40
          }
        else {				// >0x80 = cursor set
          LCDptr=LCDPortW[1] & 0x7f;			// PARAMETRIZZARE e gestire 2 Enable x 4x40!
          LCDPortR[1] = LCDram[LCDptr] | 0x80;
          LCDCGDARAM=0;
          }
        }
      }
    LCDPortR[1] &= 0x7f;
    LCDPortW[1]=LCDPortW[1]=t1;
    }

#else
  
  else if(t>=IO_BASE_ADDRESS && t<(IO_BASE_ADDRESS+0x20)) {        //   display mappato qua (su 68000 non ci sono IN & OUT)
    t-=IO_BASE_ADDRESS;
    switch(t) {
      case IO_BOARD_ADDRESS:
      case IO_BOARD_ADDRESS+1:
      case IO_BOARD_ADDRESS+2:
      case IO_BOARD_ADDRESS+3:
        IOExtPortO[t-IO_BOARD_ADDRESS]=t1;
        break;

      case LCD_BOARD_ADDRESS:
        // per motivi che ora non ricordo, il BIOS indirizza 0..3 mentre la 2°EEprom (casanet) aveva indirizzi doppi o meglio bit 0..1 messi a 2-0
        // potrei ricompilare casanet per andare "dritto" 0..3, per ora lascio così (unico problema è conflitto a +6 con la tastiera... amen! tanto scrive solo all'inizio)
        i8255RegW[0]=t1;
        break;
      case LCD_BOARD_ADDRESS+2:
        if(i8255RegW[1] & 4 && !(t1 & 4)) {   // quando E scende
          // in teoria dovremmo salvare in R[0] solo in questo istante, la lettura... ma ok! (v. sopra)
          if(i8255RegW[1] & 1)	{		// se dati
            if(!LCDCGDARAM) {
              LCDram[LCDptr]=i8255RegW[0];
              if(LCDentry & 2) {
                LCDptr++;
                if(LCDptr >= 0x7f)			// parametrizzare! o forse no, fisso max 127
                  LCDptr=0;
                }
              else {
                LCDptr--;
                if(LCDptr < 0)			// parametrizzare!
                  LCDptr=0x7f;    // CONTROLLARE
                }
              }
            else {
              LCDCGram[LCDCGptr++]=i8255RegW[0];
              LCDCGptr &= 0x3f;
              }
            }
          else {									// se comandi https://www.sparkfun.com/datasheets/LCD/HD44780.pdf
            if(i8255RegW[0] <= 1) {			// CLS/home
              LCDptr=0;
              LCDentry |= 2;		// ripristino INCremento, dice
              memset(LCDram,' ',sizeof(LCDram) /* 4*40 */);
              }
            else if(i8255RegW[0] <= 3) {			// home
              LCDptr=0;
              }
            else if(i8255RegW[0] <= 7) {			// entry mode ecc
              LCDentry=i8255RegW[0] & 3;
              }
            else if(i8255RegW[0] <= 15) {			// display on-off ecc
              LCDdisplay=i8255RegW[0] & 7;
              }
            else if(i8255RegW[0] <= 31) {			// cursor set, increment & shift off
              LCDcursor=i8255RegW[0] & 15;
              }
            else if(i8255RegW[0] <= 63) {			// function set, 2-4 linee
              LCDfunction=i8255RegW[0] & 31;
              }
            else if(i8255RegW[0] <= 127) {			// CG RAM addr set
              LCDCGptr=i8255RegW[0] & 0x3f;			// 
              LCDCGDARAM=1;// user defined char da 0x40
              }
            else {				// >0x80 = cursor set
              LCDptr=i8255RegW[0] & 0x7f;			// PARAMETRIZZARE e gestire 2 Enable x 4x40!
              LCDCGDARAM=0;
              }
            }
          }
        i8255RegW[1]=i8255RegR[1]=t1;
        break;
      case LCD_BOARD_ADDRESS+4:
        // qua c'è la 3° porta del 8255
        // il b6 mette a in o out la porta dati A (1=input)
        i8255RegW[2]=i8255RegR[2]=t1;
        break;
      case LCD_BOARD_ADDRESS+5 /* sarebbe 6 ma ovviamente non si può! v. sopra*/:
        // 8255 settings
        i8255RegW[3]=i8255RegR[3]=t1;
        break;

      case LCD_BOARD_ADDRESS+6:   
        i8042RegR[0]=i8042RegW[0]=t1;
        if(i8042RegW[1]==0xAA) {
          }
        else if(i8042RegW[1]>=0x20 && i8042RegW[1]<0x40) {
          //KBRAM[i8042RegW[1] & 0x1f]
          }
        else if(i8042RegW[1]>=0x60 && i8042RegW[1]<0x80) {
          KBRAM[i8042RegW[1] & 0x1f]=t1;
          KBRAM[0] &= 0x7f;     // dice...
          }
        else if(i8042RegW[1]==0xC0) {
          }
        else if(i8042RegW[1]==0xD0) {
          }
        else if(i8042RegW[1]==0xD1) {
          KBControl=t1;
          }
        else if(i8042RegW[1]==0xD2) {
          Keyboard[0]=t1;
          }
        else if(i8042RegW[1]>=0xF0 && i8042RegW[1]<=0xFF) {
          }
        break;
  //    case 7:     // keyboard...
  //#warning togliere!
      case LCD_BOARD_ADDRESS+7:
        i8042RegR[1]=i8042RegW[1]=t1;
        if(i8042RegW[1]==0xAA) {
          }
        else if(i8042RegW[1]>=0x20 && i8042RegW[1]<0x40) {
          //KBRAM[i8042RegW[1] & 0x1f]
          }
        else if(i8042RegW[1]>=0x60 && i8042RegW[1]<0x80) {
          //KBRAM[i8042RegW[1] & 0x1f]
          }
        else if(i8042RegW[1]==0xC0) {
          }
        else if(i8042RegW[1]==0xD0) {
          }
        else if(i8042RegW[1]==0xD1) {
          }
        else if(i8042RegW[1]==0xD2) {
          }
        else if(i8042RegW[1]>=0xF0 && i8042RegW[1]<=0xFF) {
          }
        break;
  		}
		}
  else if(t==0xe000) {        //
    IOPortO=t1;      // b5 è speaker...
    LATDbits.LATD1= t1 & 0b00100000 ? 1 : 0;
//    LATEbits.LATE2= t1 & 0b10000000 ? 1 : 0;  // led, fare se si vuole
//    LATEbits.LATE3= t1 & 0b01000000 ? 1 : 0;
// finire volendo...    if(IOPortO & 0x1)
//      IOPortI &= ~0x5;
    }
  else if(t>=0xe002 && t<=0xe003) {        //   CMOS RAM/RTC (Real Time Clock  MC146818)
    t &= 0x1;
    switch(t) {
      case 0:
        i146818RegR[0]=i146818RegW[0]=t1;
//        time_t;
        break;
      case 1:     // il bit 7 attiva/disattiva NMI
        i146818RegW[t]=t1;
        switch(i146818RegW[0] & 0x3f) {
          case 0:
            currentTime.sec=t1;
            break;
            // in mezzo c'è Alarm...
          case 2:
            currentTime.min=t1;;
            break;
          case 4:
            currentTime.hour=t1;
            break;
          // 6 è day of week...
          case 7:
            currentDate.mday=t1;
            break;
          case 8:
            currentDate.mon=t1;
            break;
          case 9:
            currentDate.year=t1;
            break;
          case 10:
            t1 &= 0x7f;     // vero hardware!
            t1 |= i146818RAM[10] & 0x80;
            goto writeRegRTC;
            break;
          case 11:
            if(t1 & 0x80)
              i146818RAM[10] &= 0x7f;
            goto writeRegRTC;
            break;
          case 12:
            t1 &= 0xf0;     // vero hardware!
            goto writeRegRTC;
            break;
          case 13:
            t1 &= 0x80;     // vero hardware!
            goto writeRegRTC;
            break;
          default:      // in effetti ci sono altri 4 registri... RAM da 14 in poi 
writeRegRTC:            
            i146818RAM[i146818RegW[0] & 0x3f] = t1;
            break;
          }
        break;
      }
		}
  else if(t==0xe004) {
    ClIRQPort;
    }
  else if(t==0xe005) {
		WDCnt=MAX_WATCHDOG;
    ClWDPort;
    }
#endif
  else {
    BusExcep.active=1;
    BusExcep.addr=t;
    BusExcep.descr.in=1;
    BusExcep.descr.rw=0;
    // finire...
    }

	}

void PutShortValue(DWORD t,SWORD t1) {
	register SWORD i;

// printf("rom_seg: %04x, p: %04x\n",rom_seg,p);

  if(t & 1) {
    AddressExcep.active=1;
    AddressExcep.addr=t;
    AddressExcep.descr.in=1;
    AddressExcep.descr.rw=0;
    return;
    }
	if(t >= RAM_START && t < (RAM_START+RAM_SIZE-1)) {		// 
		t-=RAM_START;
	  ram_seg[t++]=HIBYTE(t1);
	  ram_seg[t]=LOBYTE(t1);
		}
#ifdef QL
  else if(t>=IO_BASE_ADDRESS && t<IO_BASE_ADDRESS+0x100) {        //   
    }
#elif MICHELEFABBRI
#endif
  else {
    BusExcep.active=1;
    BusExcep.addr=t;
    BusExcep.descr.in=1;
    BusExcep.descr.rw=0;
    // finire...
    }
	}

void PutIntValue(DWORD t,DWORD t1) {
	register SWORD i;

// printf("rom_seg: %04x, p: %04x\n",rom_seg,p);

  if(t & 1) {
    AddressExcep.active=1;
    AddressExcep.addr=t;
    AddressExcep.descr.in=1;
    AddressExcep.descr.rw=0;
    }
	if(t >= RAM_START && t < (RAM_START+RAM_SIZE-3)) {		// 
		t-=RAM_START;
	  ram_seg[t++]=HIBYTE(HIWORD(t1));
	  ram_seg[t++]=LOBYTE(HIWORD(t1));
	  ram_seg[t++]=HIBYTE(LOWORD(t1));
	  ram_seg[t]=LOBYTE(LOWORD(t1));
		}
#ifdef QL
  else if(t>=IO_BASE_ADDRESS && t<IO_BASE_ADDRESS+0x100) {       // I/O
    switch(t & 0x000ff) {
      case 0:     
        RTC.b[0]=HIBYTE(HIWORD(t1));
        RTC.b[1]=LOBYTE(HIWORD(t1));
        RTC.b[2]=HIBYTE(LOWORD(t1));
        RTC.b[3]=LOBYTE(LOWORD(t1));
        break;
      case 2:        //   Transmit control; 
        //port 2, P21 è BEEP MA FORSE a 32bit!
        break;
      }
    }
#elif MICHELEFABBRI
#endif
  else {
    BusExcep.active=1;
    BusExcep.addr=t;
    BusExcep.descr.in=1;
    BusExcep.descr.rw=0;
    // finire...
    }
  
	}

