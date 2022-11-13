//https://www.google.com/url?sa=t&rct=j&q=&esrc=s&source=web&cd=&ved=2ahUKEwjc3b6Lwar2AhUPTcAKHRVnCmUQFnoECAMQAQ&url=http%3A%2F%2Fgoldencrystal.free.fr%2FM68kOpcodes-v2.3.pdf&usg=AOvVaw3a6qPsk5K_kpQd1MnlD07r
//https://www.google.com/url?sa=t&rct=j&q=&esrc=s&source=web&cd=&ved=2ahUKEwjc3b6Lwar2AhUPTcAKHRVnCmUQFnoECAUQAQ&url=https%3A%2F%2Fweb.njit.edu%2F~rosensta%2Fclasses%2Farchitecture%2F252software%2Fcode.pdf&usg=AOvVaw0awr9hRKXycE2-kghhbC3Y
//https://onlinedisassembler.com/odaweb/

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
//#include <graph.h>
//#include <dos.h>
//#include <malloc.h>
//#include <memory.h>
//#include <fcntl.h>
//#include <io.h>
#include <xc.h>

#include "Adafruit_ST77xx.h"
#include "Adafruit_ST7735.h"
#include "adafruit_gfx.h"

#include "68000_PIC.h"


#pragma check_stack(off)
// #pragma check_pointer( off )
#pragma intrinsic( _enable, _disable )

#undef MC68008

BYTE fExit=0;
BYTE debug=0;
#ifdef QL
BYTE ram_seg[RAM_SIZE];
BYTE rom_seg[ROM_SIZE];
BYTE rom_seg2[ROM_SIZE2];
#define VIDEO_ADDRESS_BASE 0x20000
DWORD VideoAddress=VIDEO_ADDRESS_BASE;
BYTE VideoMode;
SWORD VICRaster=0;
union {
  BYTE b[4];
  DWORD d;
  } RTC;
#else
BYTE ram_seg[RAM_SIZE];
BYTE rom_seg[ROM_SIZE];			
BYTE i8255RegR[4],i8255RegW[4];
BYTE LCDram[256 /*4*40*/],LCDCGDARAM=0,LCDCGram[64],LCDCGptr=0,LCDfunction,LCDdisplay,
	LCDentry=2 /* I/D */,LCDcursor;			// emulo LCD text come Z80net
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
BYTE KBDataI,KBDataO,KBControl,/*KBStatus,*/ KBRAM[32];   // https://wiki.osdev.org/%228042%22_PS/2_Controller
#define KBStatus KBRAM[0]   // pare...
BYTE Keyboard[1]={0};
volatile BYTE TIMIRQ,VIDIRQ,KBDIRQ,SERIRQ,RTCIRQ;

extern volatile BYTE keysFeedPtr;

BYTE DoReset=0,IPL=0,DoHalt=0;
#define MAX_WATCHDOG 100      // x30mS v. sotto
WORD WDCnt=MAX_WATCHDOG;
BYTE ColdReset=1;
SWORD Pipe1;
union __attribute__((__packed__)) PIPE {
	DWORD d;
	BYTE bd[8];
	WORD wd[4];
	struct __attribute__((__packed__)) {
		WORD l;
		WORD h;
		} x;
	struct __attribute__((__packed__)) {
		BYTE dummy;
		WORD w;
		} xm;
	struct __attribute__((__packed__)) {
		BYTE l;
		BYTE h;
		BYTE u;
		BYTE u2;		 // oppure spostare la pipe quando ci sono le istruzioni lunghe 4+...
//		BYTE u3;
//		BYTE u4;
		} b;
	} Pipe2;


#define IO_BASE_ADDRESS 0xc000
#define LCD_BOARD_ADDRESS 0x10
// emulo display LCD testo (4x20, 4x20 o altro) come su scheda Z80:
//  all'indirizzo+0 c'è la porta dati (in/out), a +2 i fili C/D, RW e E (E2)
#define IO_BOARD_ADDRESS 0x0

BYTE GetValue(DWORD t) {
	register BYTE i;

	if(t < ROM_SIZE) {			// 
		i=rom_seg[t];
		}
	else if(t < ROM_SIZE2) {			// cartridge
		i=rom_seg[t-ROM_SIZE];
		}
	else if(t >= RAM_START && t < (RAM_START+RAM_SIZE)) {
		i=ram_seg[t-RAM_START];
		}
  
#ifdef QL
  
  else if(t==0x18023) {        //   video mode (dice 063 o anche 023...
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
    }
  else if(t>=0x18000 && t<0x18004) {        //   RTC
    i=RTC.b[t-0x18000];
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
  
	return i;
	}

SWORD GetShortValue(DWORD t) {
	register SWORD i;

	if(t < ROM_SIZE) {			// 
		i=MAKEWORD(rom_seg[t],rom_seg[t+1]);
		}
	else if(t < ROM_SIZE2) {			// cartridge
		t-=ROM_SIZE;
		i=MAKEWORD(rom_seg[t],rom_seg[t+1]);
		}
	else if(t >= RAM_START && t < (RAM_START+RAM_SIZE)) {
		t-=RAM_START;
		i=MAKEWORD(ram_seg[t],ram_seg[t+1]);
		}
#ifdef QL
#endif
	return i;
	}

DWORD GetIntValue(DWORD t) {
	register DWORD i;

	if(t < ROM_SIZE) {			// 
		i=MAKELONG(MAKEWORD(rom_seg[t],rom_seg[t+1]),MAKEWORD(rom_seg[t+2],rom_seg[t+3]));
		}
	else if(t < ROM_SIZE2) {			// cartridge
		t-=ROM_SIZE;
		i=MAKELONG(MAKEWORD(rom_seg[t],rom_seg[t+1]),MAKEWORD(rom_seg[t+2],rom_seg[t+3]));
		}
	else if(t >= RAM_START && t < (RAM_START+RAM_SIZE)) {
		t-=RAM_START;
		i=MAKELONG(MAKEWORD(ram_seg[t],ram_seg[t+1]),MAKEWORD(ram_seg[t+2],ram_seg[t+3]));
		}
#ifdef QL
#endif
	return i;
	}

SWORD GetPipe(DWORD t) {
  SWORD *p;

	if(t < ROM_SIZE) {			// 
    if(!(t &3 ))
      ;
#warning se indirizzo è long aligned, si potrebbero leggere 2 SWORD in un colpo solo VERIFICARE test e velocità
    p=(SWORD *)&rom_seg[t/2];
		Pipe1=*p++;
		Pipe2.x.l=*p++;
//		Pipe2.b.h=rom_seg[t++];
//		Pipe2.b.u=rom_seg[t];
		Pipe2.x.h=*p;
		}
	else if(t >= RAM_START && t < (RAM_START+RAM_SIZE)) {
    p=(SWORD *)&ram_seg[(t-RAM_START)/2];
    if(!(t &3 ))
      ;
	  Pipe1=*p++;
		Pipe2.x.l=*p++;
//		Pipe2.b.h=ram_seg[t++];
//		Pipe2.b.u=ram_seg[t];
		Pipe2.x.h=*p;
		}
	return Pipe1;
	}

void PutValue(DWORD t,BYTE t1) {
	register SWORD i;

// printf("rom_seg: %04x, p: %04x\n",rom_seg,p);

	if(t >= RAM_START && t < (RAM_START+RAM_SIZE)) {		// ZX80,81
	  ram_seg[t-RAM_START]=t1;
		}
#ifdef QL
  
  else if(t==0x18023) {        //   video mode
    VideoMode=t1;
    if(VideoMode & 0b10000000)
      VideoAddress=0x28000;
    else
      VideoAddress=0x20000;
    }
  else if(t>=0x18000 && t<0x18004) {        //   RTC
    RTC.b[t-0x18000]=t1;
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

	}

void PutShortValue(DWORD t,SWORD t1) {
	register SWORD i;

// printf("rom_seg: %04x, p: %04x\n",rom_seg,p);

	if(t >= RAM_START && t < (RAM_START+RAM_SIZE)) {		// ZX80,81
	  ram_seg[t++-RAM_START]=LOBYTE(t1);
	  ram_seg[t-RAM_START]=HIBYTE(t1);
		}
#ifdef QL
#endif
	}

void PutIntValue(DWORD t,DWORD t1) {
	register SWORD i;

// printf("rom_seg: %04x, p: %04x\n",rom_seg,p);

	if(t >= RAM_START && t < (RAM_START+RAM_SIZE)) {		// ZX80,81
	  ram_seg[t++-RAM_START]=LOBYTE(LOWORD(t1));
	  ram_seg[t++-RAM_START]=HIBYTE(LOWORD(t1));
	  ram_seg[t++-RAM_START]=LOBYTE(HIWORD(t1));
	  ram_seg[t-RAM_START]=HIBYTE(HIWORD(t1));
		}
#ifdef QL
#endif
  
	}



int Emulate(int mode) {
  union __attribute__((__packed__)) REG {
    DWORD x;
    struct __attribute__((__packed__)) { 
      WORD l;
      WORD h;
      } w;
    struct __attribute__((__packed__)) { 
      BYTE b0;
      BYTE b1;
      BYTE b2;
      BYTE b3;
      } b;
    };
	union __attribute__((__packed__)) D_REGISTERS {
		BYTE  b[64];
	  union REG r[8];
		} regsD;
	union __attribute__((__packed__)) A_REGISTERS {
		BYTE  b[56];
	  union REG r[7];
		} regsA;
#define WORKING_REG_A (regsA.r[((LOBYTE(Pipe1)) & 7)])
#define WORKING_REG_D (regsD.r[((LOBYTE(Pipe1)) & 7)])
#define DEST_REG_A (regsA.r[(((HIBYTE(Pipe1)) >> 1) & 7)])
#define DEST_REG_D (regsD.r[(((HIBYTE(Pipe1)) >> 1) & 7)])
#define ADDRESSING (LOBYTE(Pipe1) & 0b00111000)
#define ADDRESSING2 (Pipe1 & 0b0000000111000000)      // per MOVE/MOVEA...
enum ADDRESSING_MODES {
  DATAREGADD = 0b00000000,
  ADDRREGADD = 0b00001000,
  ADDRADD =    0b00010000,
  ADDRPINCADD =0b00011000,
  ADDRPDECADD =0b00100000,
  ADDRDISPADD =0b00101000,
  ADDRIDXADD = 0b00110000,
  PCDISPADD  = 0b00111000,
  PCIDXADD   = 0b00111000,
  ABSOLUTESADD=0b00111000,
  ABSOLUTELADD=0b00111000,
  IMMEDIATEADD=0b00111000
  };
#define OPERAND_SIZE (LOBYTE(Pipe1) & 0x11000000)
#define	BYTE_SIZE 0x00
#define	WORD_SIZE 0x40
#define	DWORD_SIZE 0x80
#define DISPLACEMENT_REG (regsD.r[((HIBYTE(Pipe2.b.h) >> 4) & 7)])
#define DISPLACEMENT_REG2 (regsD.r[((HIBYTE(Pipe2.b.h) >> 4) & 7)])
#warning VERIFICARE DISPLACEMENT_REG2 dove e, e SIZE2
#define DISPLACEMENT_SIZE (Pipe2.b.h & 0b00010000)
#define DISPLACEMENT_SIZE2 (Pipe2.b.h & 0b00010000)
#warning  vERIFICARE dov e il valore immediato vs- il displacement...
#define ABSOLUTEADD_SIZE (Pipe1 & 0b0000000000000001)
#define ABSOLUTEADD_SIZE2 (Pipe1 & 0b0000001000000000)    // per MOVE/MOVEA

	DWORD _pc=0;
	DWORD _sp=0,_ssp=0;
#define ID_CARRY 0x1
#define ID_OVF 0x2
#define ID_ZERO 0x4
#define ID_SIGN 0x8
#define ID_EXT 0x10
	union __attribute__((__packed__)) REGISTRO_F {
    SWORD w;
    union __attribute__((__packed__)) {
      BYTE b;
      struct __attribute__((__packed__)) {
        unsigned int Carry: 1;
        unsigned int Ovf: 1;
        unsigned int Zero: 1;
        unsigned int Sign: 1;
        unsigned int Ext: 1;
        unsigned int unused: 11;
        };
      } CCR;
    union __attribute__((__packed__)) {
      WORD w;
      struct __attribute__((__packed__)) {
        unsigned int Carry: 1;
        unsigned int Ovf: 1;
        unsigned int Zero: 1;
        unsigned int Sign: 1;
        unsigned int Ext: 1;
        unsigned int unused: 3;
        unsigned int IRQMask: 3;
        unsigned int unused2: 2;
        unsigned int Supervisor: 1;
        unsigned int unused3: 1;
        unsigned int Trace: 1;
        };
      } SR;
		} _fIRQ;
	register union REGISTRO_F _f;
	union REGISTRO_F _f1;
  BYTE FC;
	register SWORD i;
	register union __attribute__((__packed__)) OPERAND {
    BYTE *reg8;
    WORD *reg16;
		WORD mem;
    } op1,op2;
register union __attribute__((__packed__)) RESULT {
  struct __attribute__((__packed__)) {
    BYTE l;
    BYTE h;
    } b;
  WORD x;
  DWORD d;
  } res1,res2,res3;
int c=0;


	_pc=0;
  
  
 
  
	do {

		c++;
		if(!(c & 16383)) {
      ClrWdt();
// yield()
#ifdef QL
      UpdateScreen(VICRaster,VICRaster+8);
      VICRaster+=8;     // TROVARE registro HW!!
      if(VICRaster>=256)
        VICRaster=0;
#else
			UpdateScreen(1);
#endif
      LED1^=1;    // 42mS~ con SKYNET 7/6/20; 10~mS con Z80NE 10/7/21
      
      WDCnt--;
      if(!WDCnt) {
        WDCnt=MAX_WATCHDOG;
#ifndef QL
        if(IOPortO & 0b00001000) {     // WDEn
          DoReset=1;
          }
#endif
        }
      
    }
		if(ColdReset) {
      DoReset=1;
			continue;
      }


    if(RTCIRQ) {
      IPL=0b110;
//      ExtIRQNum=0x70;      // IRQ RTC
#ifndef QL
      LCDram[0x40+20+19]++;
#endif
      RTCIRQ=0;
      }
#ifndef QL
    if(!(IOPortI & 1)) {
//      DoNMI=1;
      }
#endif
    
    
		/*
		if((_pc >= 0xa000) && (_pc <= 0xbfff)) {
			printf("%04x    %02x\n",_pc,GetValue(_pc));
			}
			*/
		if(debug) {
//			printf("%04x    %02x\n",_pc,GetValue(_pc));
			}
		/*if(kbhit()) {
			getch();
			printf("%04x    %02x\n",_pc,GetValue(_pc));
			printf("281-284: %02x %02x %02x %02x\n",*(p1+0x281),*(p1+0x282),*(p1+0x283),*(p1+0x284));
			printf("2b-2c: %02x %02x\n",*(p1+0x2b),*(p1+0x2c));
			printf("33-34: %02x %02x\n",*(p1+0x33),*(p1+0x34));
			printf("37-38: %02x %02x\n",*(p1+0x37),*(p1+0x38));
			}*/
		if(DoReset) {
      IPL=0b000;  // credo..
      FC=0b110;   // supervisor, area programma VERIFICARE
			_pc=GetIntValue(0x00000004);
			_f.w=GetShortValue(0x00000000);
			DoReset=0;
			}
		if(IPL > _f.SR.IRQMask) {
      FC=0b111;   // supervisor, area programma, eccezione
      DoHalt=0;
      PutIntValue(_f.SR.Supervisor ? --_ssp : --_sp,_pc); // low word prima
      _f1=_f;
      PutIntValue(_f.SR.Supervisor ? --_ssp : --_sp,_f1.w);
      _f.SR.IRQMask=IPL;
      _f.SR.Supervisor=1;     // non mi è chiaro se sono nesteed o no...
        
    // su A1..A3 esce IPL e poi si legge dal bus dati il valore...
/* 	(a) Supply a vector number.
? Place 8-bit vector number of D 7 -D 0 .
? pull DTACK low.
? µP will read D 7 -D 0 .
(b) Request an ?auto-vector".
? Pull VPA low. Leave DTACK high.
? µP generates its own vector based on interrupt level first supplied to IPL inputs.
? autovectors point to locations $064 through $07F in vector table
? Autovectors should be used whenever 7 or less interrupt types are needed*/
      _pc=GetIntValue(/* 24*4 */ 0x00000060+IPL*4);   // ovviamente faciam b ;)
			}

		if(DoHalt)
			continue;		// esegue cmq IRQ FORSE qua

//printf("Pipe1: %02x, Pipe2w: %04x, Pipe2b1: %02x,%02x\n",Pipe1,Pipe2.word,Pipe2.bytes.byte1,Pipe2.bytes.byte2);
    
/*    
      LCDram[0x40+20+3]=(_pc & 0xf) + 0x30;
      if(LCDram[0x40+20+3] >= 0x3a)
        LCDram[0x40+20+3]+=7;
      LCDram[0x40+20+2]=((_pc/16) & 0xf) + 0x30;
      if(LCDram[0x40+20+2] >= 0x3a)
        LCDram[0x40+20+2]+=7;
      LCDram[0x40+20+1]=((_pc/256) & 0xf) + 0x30;
      if(LCDram[0x40+20+1] >= 0x3a)
        LCDram[0x40+20+1]+=7;
      LCDram[0x40+20+0]=((_pc/4096) & 0xf) + 0x30;
      if(LCDram[0x40+20+0] >= 0x3a)
        LCDram[0x40+20+0]+=7;
  */    
    
    
      if(!SW2) {        // test tastiera, me ne frego del repeat/rientro :)
       // continue;
        __delay_ms(100); ClrWdt();
        }
      if(!SW1)        // test tastiera
        keysFeedPtr=0;

      LED2^=1;    // ~700nS 7/6/20, ~600 con 32bit 10/7/21 MA NON FUNZIONA/visualizza!! verificare

    
/*      if(_pc >= 0x4000) {
        ClrWdt();
        }*/
  
  
  
		switch(HIBYTE(GetPipe(_pc))) {
#warning IDEM FINIRE pc++ !!!!
			case BYTE_SIZE:   // ORI
        switch(ADDRESSING) {
          case ABSOLUTELADD:
            switch(LOBYTE(Pipe1)) {
              case 0x3c:
                _f.CCR.b |= Pipe2.b.l;
    						_pc+=4;
                break;
              case 0x7c:
                _f.SR.w |= Pipe2.x.l;
    						_pc+=4;
	              break;
              default:
                if(!ABSOLUTEADD_SIZE)
                  switch(OPERAND_SIZE) {
                    case BYTE_SIZE:
                      res3.b.l = GetValue(Pipe2.x.h) | Pipe2.b.l;
                      PutValue(Pipe2.x.h, res3.b.l);
                      _pc+=6;
                      break;
                    case WORD_SIZE:
                      res3.x = GetShortValue(Pipe2.x.h) | Pipe2.x.l;
                      PutShortValue(Pipe2.x.h, res3.x);
                      _pc+=6;
                      break;
                    case DWORD_SIZE:
                      res3.d = Pipe2.d;
                      _pc+=2;
                      GetPipe(_pc);
                      res3.d |= GetIntValue(Pipe2.x.h);
                      PutIntValue(Pipe2.x.h, res3.d);
                      _pc+=6;
                      break;
                    }
                else
                  switch(OPERAND_SIZE) {
                    case BYTE_SIZE:
                      res3.b.l = Pipe2.b.l;
                      _pc+=2;
                      GetPipe(_pc);
                      res3.b.l |= GetValue(Pipe2.x.h);
                      PutValue(Pipe2.x.h, res3.b.l);
                      _pc+=6;
                      break;
                    case WORD_SIZE:
                      res3.x = Pipe2.x.l;
                      _pc+=2;
                      GetPipe(_pc);
                      res3.x |= GetShortValue(Pipe2.x.h);
                      PutShortValue(Pipe2.x.h, res3.x);
                      _pc+=6;
                      break;
                    case DWORD_SIZE:
                      res3.d = Pipe2.d;
                      _pc+=4;
                      GetPipe(_pc);
                      res3.d |= GetIntValue(Pipe2.d); 
                      PutIntValue(Pipe2.x.h, res3.d);
                      _pc+=6;
                      break;
                    }
                break;
              }
            goto noAggFlag;
            break;
          case DATAREGADD:   // Dn
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = WORKING_REG_D.b.b0 |= Pipe2.b.l;
								_pc+=4;
                break;
              case 0x40:
                res3.x = WORKING_REG_D.w.l |= Pipe2.x.l;
								_pc+=4;
                break;
              case DWORD_SIZE:
                res3.d = WORKING_REG_D.x |= Pipe2.d;
								_pc+=6;
                break;
              }
            break;
          case ADDRREGADD:   // An NO!
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GetValue(WORKING_REG_A.x) | Pipe2.b.l;
                PutValue(WORKING_REG_A.x, res3.b.l);
								_pc+=4;
                break;
              case WORD_SIZE:
                res3.x = GetShortValue(WORKING_REG_A.x) | Pipe2.x.l;
                PutShortValue(WORKING_REG_A.x, res3.x);
								_pc+=4;
                break;
              case DWORD_SIZE:
                res3.d = GetIntValue(WORKING_REG_A.x) | Pipe2.d;
                PutIntValue(WORKING_REG_A.x, res3.d);
								_pc+=6;
                break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GetValue(WORKING_REG_A.x) | Pipe2.b.l;
                PutValue(WORKING_REG_A.x, res3.b.l);
                WORKING_REG_A.x++;
								_pc+=4;
                break;
              case WORD_SIZE:
                res3.x = GetShortValue(WORKING_REG_A.x) | Pipe2.x.l;
                PutShortValue(WORKING_REG_A.x, res3.x);
                WORKING_REG_A.x+=2;
								_pc+=4;
                break;
              case DWORD_SIZE:
                res3.d = GetIntValue(WORKING_REG_A.x) | Pipe2.d;
                PutIntValue(WORKING_REG_A.x, res3.d);
                WORKING_REG_A.x+=4;
								_pc+=6;
                break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                WORKING_REG_A.x--;
                res3.b.l = GetValue(WORKING_REG_A.x) | Pipe2.b.l;
                PutValue(WORKING_REG_A.x, res3.b.l);
								_pc+=4;
                break;
              case WORD_SIZE:
                WORKING_REG_A.x-=2;
                res3.x = GetShortValue(WORKING_REG_A.x) | Pipe2.x.l;
                PutShortValue(WORKING_REG_A.x, res3.x);
								_pc+=4;
                break;
              case DWORD_SIZE:
                WORKING_REG_A.x-=4;
                res3.d = GetIntValue(WORKING_REG_A.x) | Pipe2.d;
                PutIntValue(WORKING_REG_A.x, res3.d);
								_pc+=6;
                break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) | Pipe2.b.l;
                PutValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h, res3.b.l);
								_pc+=6;
                break;
              case WORD_SIZE:
                res3.x = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) | Pipe2.x.l;
                PutShortValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h, res3.x);
								_pc+=6;
                break;
              case DWORD_SIZE:
                res3.d = GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) | Pipe2.d;
                PutIntValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h, res3.d);
								_pc+=8;
                break;
              }
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.b.l = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) | Pipe2.b.l;
                  PutValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l, res3.b.l);
                  }
                else {
                  res3.b.l = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) | Pipe2.b.l;
                  PutValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l, res3.b.l);
                  }
								_pc+=6;
                break;
              case WORD_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.x = GetShortValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) | Pipe2.x.l;
                  PutShortValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l, res3.x);
                  }
                else {
                  res3.x = GetShortValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) | Pipe2.x.l;
                  PutShortValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l, res3.x);
                  }
								_pc+=6;
                break;
              case DWORD_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.d = GetIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) | Pipe2.d;
                  PutIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l, res3.d);
                  }
                else {
                  res3.d = GetIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) | Pipe2.d;
                  PutIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l, res3.d);
                  }
								_pc+=8;
                break;
              }
            break;
          }
aggFlag:
        switch(OPERAND_SIZE) {
          case BYTE_SIZE:
            _f.CCR.Zero=!res3.b.l;
            _f.CCR.Sign=!!(res3.b.l & 0x80);
            break;
          case WORD_SIZE:
            _f.CCR.Zero=!res3.x;
            _f.CCR.Sign=!!(res3.x & 0x8000);
            break;
          case DWORD_SIZE:
            _f.CCR.Zero=!res3.d;
            _f.CCR.Sign=!!(res3.d & 0x80000000);
            break;
          }
        _f.CCR.Ovf=_f.CCR.Carry=0;
noAggFlag:
				break;

			case 0x01:   // BTST BCHG BCLR BSET MOVEP
			case 0x03:
			case 0x05:
			case 0x07:
			case 0x09:
			case 0x0b:
			case 0x0d:
			case 0x0f:
        switch(OPERAND_SIZE) {
          case BYTE_SIZE:
            break;
          case WORD_SIZE:
            break;
          case DWORD_SIZE:
            break;
          }
				break;
        
			case 0x02:   // ANDI
        switch(ADDRESSING) {
          case ABSOLUTELADD:    // abs
            switch(LOBYTE(Pipe1)) {
              case 0x3c:
                _f.CCR.b &= Pipe2.b.l;
      					_pc+=4;
                break;
              case 0x7c:
                _f.SR.w &= Pipe2.x.l;
      					_pc+=4;
                break;
              default:
#warning verificare signextension dell address short
                if(!ABSOLUTEADD_SIZE)
                  switch(OPERAND_SIZE) {
                    case BYTE_SIZE:
                      res3.b.l = GetValue(Pipe2.x.h) & Pipe2.b.l;
                      PutValue(Pipe2.x.h, res3.b.l);
                      _pc+=6;
                      break;
                    case WORD_SIZE:
                      res3.x = GetShortValue(Pipe2.x.h) & Pipe2.x.l;
                      PutShortValue(Pipe2.x.h, res3.x);
                      _pc+=6;
                      break;
                    case DWORD_SIZE:
                      res3.d = Pipe2.d;
                      _pc+=2;
                      GetPipe(_pc);
                      res3.d &= GetIntValue(Pipe2.x.h);
                      PutIntValue(Pipe2.x.h, res3.d);
                      _pc+=6;
                      break;
                    }
                else
                  switch(OPERAND_SIZE) {
                    case BYTE_SIZE:
                      res3.b.l = Pipe2.b.l;
                      _pc+=2;
                      GetPipe(_pc);
                      res3.b.l &= GetValue(Pipe2.x.h);
                      PutValue(Pipe2.x.h, res3.b.l);
                      _pc+=6;
                      break;
                    case WORD_SIZE:
                      res3.x = Pipe2.x.l;
                      _pc+=2;
                      GetPipe(_pc);
                      res3.x &= GetShortValue(Pipe2.x.h);
                      PutShortValue(Pipe2.x.h, res3.x);
                      _pc+=6;
                      break;
                    case DWORD_SIZE:
                      res3.d = Pipe2.d;
                      _pc+=4;
                      GetPipe(_pc);
                      res3.d &= GetIntValue(Pipe2.d); 
                      PutIntValue(Pipe2.x.h, res3.d);
                      _pc+=6;
                      break;
                    }
                break;
              }
            goto noAggFlag;
            break;
          case DATAREGADD:   // Dn
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = WORKING_REG_D.b.b0 &= Pipe2.b.l;
								_pc+=4;
                break;
              case WORD_SIZE:
                res3.x = WORKING_REG_D.w.l &= Pipe2.x.l;
								_pc+=4;
                break;
              case DWORD_SIZE:
                res3.d = WORKING_REG_D.x &= Pipe2.d;
								_pc+=6;
                break;
              }
            break;
          case ADDRREGADD:   // An
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GetValue(WORKING_REG_A.x) & Pipe2.b.l;
                PutValue(WORKING_REG_A.x, res3.b.l);
								_pc+=4;
                break;
              case WORD_SIZE:
                res3.x = GetShortValue(WORKING_REG_A.x) & Pipe2.x.l;
                PutShortValue(WORKING_REG_A.x, res3.x);
								_pc+=4;
                break;
              case DWORD_SIZE:
                res3.d = GetIntValue(WORKING_REG_A.x) & Pipe2.d;
                PutIntValue(WORKING_REG_A.x, res3.d);
								_pc+=6;
                break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GetValue(WORKING_REG_A.x) & Pipe2.b.l;
                PutValue(WORKING_REG_A.x, res3.b.l);
                WORKING_REG_A.x++;
								_pc+=4;
                break;
              case WORD_SIZE:
                res3.x = GetShortValue(WORKING_REG_A.x) & Pipe2.x.l;
                PutShortValue(WORKING_REG_A.x, res3.x);
                WORKING_REG_A.x+=2;
								_pc+=4;
                break;
              case DWORD_SIZE:
                res3.d = GetIntValue(WORKING_REG_A.x) & Pipe2.d;
                PutIntValue(WORKING_REG_A.x, res3.d);
                WORKING_REG_A.x+=4;
								_pc+=6;
                break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                WORKING_REG_A.x--;
                res3.b.l = GetValue(WORKING_REG_A.x) & Pipe2.b.l;
                PutValue(WORKING_REG_A.x, res3.b.l);
								_pc+=4;
                break;
              case WORD_SIZE:
                WORKING_REG_A.x-=2;
                res3.x = GetShortValue(WORKING_REG_A.x) & Pipe2.x.l;
                PutShortValue(WORKING_REG_A.x, res3.x);
								_pc+=4;
                break;
              case DWORD_SIZE:
                WORKING_REG_A.x-=4;
                res3.d = GetIntValue(WORKING_REG_A.x) & Pipe2.d;
                PutIntValue(WORKING_REG_A.x, res3.d);
								_pc+=6;
                break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) & Pipe2.b.l;
                PutValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h, res3.b.l);
								_pc+=6;
                break;
              case WORD_SIZE:
                res3.x = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) & Pipe2.x.l;
                PutShortValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h, res3.x);
								_pc+=6;
                break;
              case DWORD_SIZE:
                res3.d = GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) & Pipe2.d;
                PutIntValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h, res3.d);
								_pc+=8;
                break;
              }
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.b.l = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) & Pipe2.b.l;
                  PutValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l, res3.b.l);
                  }
                else {
                  res3.b.l = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) & Pipe2.b.l;
                  PutValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l, res3.b.l);
                  }
								_pc+=6;
                break;
              case WORD_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.x = GetShortValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) & Pipe2.x.l;
                  PutShortValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l, res3.x);
                  }
                else {
                  res3.x = GetShortValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) & Pipe2.x.l;
                  PutShortValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l, res3.x);
                  }
								_pc+=6;
                break;
              case DWORD_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.d = GetIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) & Pipe2.d;
                  PutIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l, res3.d);
                  }
                else {
                  res3.d = GetIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) & Pipe2.d;
                  PutIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l, res3.d);
                  }
								_pc+=8;
                break;
              }
            break;
          }
        goto aggFlag;
				break;

			case 0x04:   // SUBI
        switch(ADDRESSING) {
          case ABSOLUTELADD:
            switch(LOBYTE(Pipe1)) {
              case 0x3c:
                _f.CCR.b ^= Pipe2.b.l;
      					_pc+=4;
                break;
              case 0x7c:
                _f.SR.w ^= Pipe2.x.l;
      					_pc+=4;
                break;
              default:
                if(!ABSOLUTEADD_SIZE)
                  switch(OPERAND_SIZE) {
                    case BYTE_SIZE:
                      res3.b.l = GetValue(Pipe2.x.h) - Pipe2.b.l;
                      PutValue(Pipe2.x.h, res3.b.l);
                      _pc+=6;
                      break;
                    case WORD_SIZE:
                      res3.x = GetShortValue(Pipe2.x.h) - Pipe2.x.l;
                      PutShortValue(Pipe2.x.h, res3.x);
                      _pc+=6;
                      break;
                    case DWORD_SIZE:
                      res3.d = Pipe2.d;
                      _pc+=2;
                      GetPipe(_pc);
                      res3.d -= GetIntValue(Pipe2.x.h);
                      PutIntValue(Pipe2.x.h, res3.d);
                      _pc+=6;
                      break;
                    }
                else
                  switch(OPERAND_SIZE) {
                    case BYTE_SIZE:
                      res3.b.l = Pipe2.b.l;
                      _pc+=2;
                      GetPipe(_pc);
                      res3.b.l -= GetValue(Pipe2.x.h);
                      PutValue(Pipe2.x.h, res3.b.l);
                      _pc+=6;
                      break;
                    case WORD_SIZE:
                      res3.x = Pipe2.x.l;
                      _pc+=2;
                      GetPipe(_pc);
                      res3.x -= GetShortValue(Pipe2.x.h);
                      PutShortValue(Pipe2.x.h, res3.x);
                      _pc+=6;
                      break;
                    case DWORD_SIZE:
                      res3.d = Pipe2.d;
                      _pc+=4;
                      GetPipe(_pc);
                      res3.d -= GetIntValue(Pipe2.d); 
                      PutIntValue(Pipe2.x.h, res3.d);
                      _pc+=6;
                      break;
                    }
                break;
              }
            break;
          case DATAREGADD:   // Dn
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = WORKING_REG_D.b.b0 -= Pipe2.b.l;
								_pc+=4;
                break;
              case WORD_SIZE:
                res3.x = WORKING_REG_D.w.l -= Pipe2.x.l;
								_pc+=4;
                break;
              case DWORD_SIZE:
                res3.d = WORKING_REG_D.x -= Pipe2.d;
								_pc+=6;
                break;
              }
            break;
          case ADDRREGADD:   // An
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GetValue(WORKING_REG_A.x) - Pipe2.b.l;
                PutValue(WORKING_REG_A.x, res3.b.l);
								_pc+=4;
                break;
              case WORD_SIZE:
                res3.x = GetShortValue(WORKING_REG_A.x) - Pipe2.x.l;
                PutShortValue(WORKING_REG_A.x, res3.x);
								_pc+=4;
                break;
              case DWORD_SIZE:
                res3.d = GetIntValue(WORKING_REG_A.x) - Pipe2.d;
                PutIntValue(WORKING_REG_A.x, res3.d);
								_pc+=6;
                break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GetValue(WORKING_REG_A.x) - Pipe2.b.l;
                PutValue(WORKING_REG_A.x, res3.b.l);
                WORKING_REG_A.x++;
								_pc+=4;
                break;
              case WORD_SIZE:
                res3.x = GetShortValue(WORKING_REG_A.x) - Pipe2.x.l;
                PutShortValue(WORKING_REG_A.x, res3.x);
                WORKING_REG_A.x+=2;
								_pc+=4;
                break;
              case DWORD_SIZE:
                res3.d = GetIntValue(WORKING_REG_A.x) - Pipe2.d;
                PutIntValue(WORKING_REG_A.x, res3.d);
                WORKING_REG_A.x+=4;
								_pc+=6;
                break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                WORKING_REG_A.x--;
                res3.b.l = GetValue(WORKING_REG_A.x) - Pipe2.b.l;
                PutValue(WORKING_REG_A.x, res3.b.l);
								_pc+=4;
                break;
              case WORD_SIZE:
                WORKING_REG_A.x-=2;
                res3.x = GetShortValue(WORKING_REG_A.x) - Pipe2.x.l;
                PutShortValue(WORKING_REG_A.x, res3.x);
								_pc+=4;
                break;
              case DWORD_SIZE:
                WORKING_REG_A.x-=4;
                res3.d = GetIntValue(WORKING_REG_A.x) - Pipe2.d;
                PutIntValue(WORKING_REG_A.x, res3.d);
								_pc+=6;
                break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) - Pipe2.b.l;
                PutValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h, res3.b.l);
								_pc+=6;
                break;
              case WORD_SIZE:
                res3.x = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) - Pipe2.x.l;
                PutShortValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h, res3.x);
								_pc+=6;
                break;
              case DWORD_SIZE:
                res3.d = GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) - Pipe2.d;
                PutIntValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h, res3.d);
								_pc+=6;
                break;
              }
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.b.l = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) - Pipe2.b.l;
                  PutValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l, res3.b.l);
                  }
                else {
                  res3.b.l = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) - Pipe2.b.l;
                  PutValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l, res3.b.l);
                  }
								_pc+=6;
                break;
              case WORD_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.x = GetShortValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) - Pipe2.x.l;
                  PutShortValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l, res3.x);
                  }
                else {
                  res3.x = GetShortValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) - Pipe2.x.l;
                  PutShortValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l, res3.x);
                  }
								_pc+=6;
                break;
              case DWORD_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.d = GetIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) - Pipe2.d;
                  PutIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l, res3.d);
                  }
                else {
                  res3.d = GetIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) - Pipe2.d;
                  PutIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l, res3.d);
                  }
								_pc+=8;
                break;
              }
            break;
          }
        goto aggFlagC;
				break;

			case 0x06:   // ADDI
        switch(ADDRESSING) {
          case ABSOLUTELADD:
            switch(LOBYTE(Pipe1)) {
              case 0x3c:
                _f.CCR.b ^= Pipe2.b.l;
      					_pc+=4;
                break;
              case 0x7c:
                _f.SR.w ^= Pipe2.x.l;
      					_pc+=4;
                break;
              default:
                if(!ABSOLUTEADD_SIZE)
                  switch(OPERAND_SIZE) {
                    case BYTE_SIZE:
                      res3.b.l = GetValue(Pipe2.x.h) + Pipe2.b.l;
                      PutValue(Pipe2.x.h, res3.b.l);
                      _pc+=6;
                      break;
                    case WORD_SIZE:
                      res3.x = GetShortValue(Pipe2.x.h) + Pipe2.x.l;
                      PutShortValue(Pipe2.x.h, res3.x);
                      _pc+=6;
                      break;
                    case DWORD_SIZE:
                      res3.d = Pipe2.d;
                      _pc+=2;
                      GetPipe(_pc);
                      res3.d += GetIntValue(Pipe2.x.h);
                      PutIntValue(Pipe2.x.h, res3.d);
                      _pc+=6;
                      break;
                    }
                else
                  switch(OPERAND_SIZE) {
                    case BYTE_SIZE:
                      res3.b.l = Pipe2.b.l;
                      _pc+=2;
                      GetPipe(_pc);
                      res3.b.l += GetValue(Pipe2.x.h);
                      PutValue(Pipe2.x.h, res3.b.l);
                      _pc+=6;
                      break;
                    case WORD_SIZE:
                      res3.x = Pipe2.x.l;
                      _pc+=2;
                      GetPipe(_pc);
                      res3.x += GetShortValue(Pipe2.x.h);
                      PutShortValue(Pipe2.x.h, res3.x);
                      _pc+=6;
                      break;
                    case DWORD_SIZE:
                      res3.d = Pipe2.d;
                      _pc+=4;
                      GetPipe(_pc);
                      res3.d += GetIntValue(Pipe2.d); 
                      PutIntValue(Pipe2.x.h, res3.d);
                      _pc+=6;
                      break;
                    }
                break;
              }
            break;
          case DATAREGADD:   // Dn
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = WORKING_REG_D.b.b0 += Pipe2.b.l;
								_pc+=4;
                break;
              case WORD_SIZE:
                res3.x = WORKING_REG_D.w.l += Pipe2.x.l;
								_pc+=4;
                break;
              case DWORD_SIZE:
                res3.d = WORKING_REG_D.x += Pipe2.d;
								_pc+=6;
                break;
              }
            break;
          case ADDRREGADD:   // An
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GetValue(WORKING_REG_A.x) + Pipe2.b.l;
                PutValue(WORKING_REG_A.x, res3.b.l);
								_pc+=4;
                break;
              case WORD_SIZE:
                res3.x = GetShortValue(WORKING_REG_A.x) + Pipe2.x.l;
                PutShortValue(WORKING_REG_A.x, res3.x);
								_pc+=4;
                break;
              case DWORD_SIZE:
                res3.d = GetIntValue(WORKING_REG_A.x) + Pipe2.d;
                PutIntValue(WORKING_REG_A.x, res3.d);
								_pc+=6;
                break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GetValue(WORKING_REG_A.x) + Pipe2.b.l;
                PutValue(WORKING_REG_A.x, res3.b.l);
                WORKING_REG_A.x++;
								_pc+=4;
                break;
              case WORD_SIZE:
                res3.x = GetShortValue(WORKING_REG_A.x) + Pipe2.x.l;
                PutShortValue(WORKING_REG_A.x, res3.x);
                WORKING_REG_A.x+=2;
								_pc+=4;
                break;
              case DWORD_SIZE:
                res3.d = GetIntValue(WORKING_REG_A.x) + Pipe2.d;
                PutIntValue(WORKING_REG_A.x, res3.d);
                WORKING_REG_A.x+=4;
								_pc+=6;
                break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                WORKING_REG_A.x--;
                res3.b.l = GetValue(WORKING_REG_A.x) + Pipe2.b.l;
                PutValue(WORKING_REG_A.x, res3.b.l);
								_pc+=4;
                break;
              case WORD_SIZE:
                WORKING_REG_A.x-=2;
                res3.x = GetShortValue(WORKING_REG_A.x) + Pipe2.x.l;
                PutShortValue(WORKING_REG_A.x, res3.x);
								_pc+=4;
                break;
              case DWORD_SIZE:
                WORKING_REG_A.x-=4;
                res3.d = GetIntValue(WORKING_REG_A.x) + Pipe2.d;
                PutIntValue(WORKING_REG_A.x, res3.d);
								_pc+=6;
                break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) + Pipe2.b.l;
                PutValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h, res3.b.l);
								_pc+=6;
                break;
              case WORD_SIZE:
                res3.x = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) + Pipe2.x.l;
                PutShortValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h, res3.x);
								_pc+=6;
                break;
              case DWORD_SIZE:
                res3.d = GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) + Pipe2.d;
                PutIntValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h, res3.d);
								_pc+=6;
                break;
              }
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.b.l = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) + Pipe2.b.l;
                  PutValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l, res3.b.l);
                  }
                else {
                  res3.b.l = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) + Pipe2.b.l;
                  PutValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l, res3.b.l);
                  }
								_pc+=6;
                break;
              case WORD_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.x = GetShortValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) + Pipe2.x.l;
                  PutShortValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l, res3.x);
                  }
                else {
                  res3.x = GetShortValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) + Pipe2.x.l;
                  PutShortValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l, res3.x);
                  }
								_pc+=6;
                break;
              case DWORD_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.d = GetIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) + Pipe2.d;
                  PutIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l, res3.d);
                  }
                else {
                  res3.d = GetIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) + Pipe2.d;
                  PutIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l, res3.d);
                  }
								_pc+=8;
                break;
              }
            break;
          }
        goto aggFlagC;
				break;

			case 0x0a:   // EORI
        switch(ADDRESSING) {
          case ABSOLUTELADD:
            switch(LOBYTE(Pipe1)) {
              case 0x3c:
                _f.CCR.b ^= Pipe2.b.l;
      					_pc+=4;
                break;
              case 0x7c:
                _f.SR.w ^= Pipe2.x.l;
      					_pc+=4;
                break;
              default:
                if(!ABSOLUTEADD_SIZE)
                  switch(OPERAND_SIZE) {
                    case BYTE_SIZE:
                      res3.b.l = GetValue(Pipe2.x.h) ^ Pipe2.b.l;
                      PutValue(Pipe2.x.h, res3.b.l);
                      _pc+=6;
                      break;
                    case WORD_SIZE:
                      res3.x = GetShortValue(Pipe2.x.h) ^ Pipe2.x.l;
                      PutShortValue(Pipe2.x.h, res3.x);
                      _pc+=6;
                      break;
                    case DWORD_SIZE:
                      res3.d = Pipe2.d;
                      _pc+=2;
                      GetPipe(_pc);
                      res3.d ^= GetIntValue(Pipe2.x.h);
                      PutIntValue(Pipe2.x.h, res3.d);
                      _pc+=6;
                      break;
                    }
                else
                  switch(OPERAND_SIZE) {
                    case BYTE_SIZE:
                      res3.b.l = Pipe2.b.l;
                      _pc+=2;
                      GetPipe(_pc);
                      res3.b.l ^= GetValue(Pipe2.x.h);
                      PutValue(Pipe2.x.h, res3.b.l);
                      _pc+=6;
                      break;
                    case WORD_SIZE:
                      res3.x = Pipe2.x.l;
                      _pc+=2;
                      GetPipe(_pc);
                      res3.x ^= GetShortValue(Pipe2.x.h);
                      PutShortValue(Pipe2.x.h, res3.x);
                      _pc+=6;
                      break;
                    case DWORD_SIZE:
                      res3.d = Pipe2.d;
                      _pc+=4;
                      GetPipe(_pc);
                      res3.d ^= GetIntValue(Pipe2.d); 
                      PutIntValue(Pipe2.x.h, res3.d);
                      _pc+=6;
                      break;
                    }
                break;
              }
            goto noAggFlag;
            break;
          case DATAREGADD:   // Dn
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = WORKING_REG_D.b.b0 ^= Pipe2.b.l;
								_pc+=4;
                break;
              case WORD_SIZE:
                res3.x = WORKING_REG_D.w.l ^= Pipe2.x.l;
								_pc+=4;
                break;
              case DWORD_SIZE:
                res3.d = WORKING_REG_D.x ^= Pipe2.d;
								_pc+=6;
                break;
              }
            break;
          case ADDRREGADD:   // An
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GetValue(WORKING_REG_A.x) ^ Pipe2.b.l;
                PutValue(WORKING_REG_A.x, res3.b.l);
								_pc+=4;
                break;
              case WORD_SIZE:
                res3.x = GetShortValue(WORKING_REG_A.x) ^ Pipe2.x.l;
                PutShortValue(WORKING_REG_A.x, res3.x);
								_pc+=4;
                break;
              case DWORD_SIZE:
                res3.d = GetIntValue(WORKING_REG_A.x) ^ Pipe2.d;
                PutIntValue(WORKING_REG_A.x, res3.d);
								_pc+=6;
                break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GetValue(WORKING_REG_A.x) ^ Pipe2.b.l;
                PutValue(WORKING_REG_A.x, res3.b.l);
                WORKING_REG_A.x++;
								_pc+=4;
                break;
              case WORD_SIZE:
                res3.x = GetShortValue(WORKING_REG_A.x) ^ Pipe2.x.l;
                PutShortValue(WORKING_REG_A.x, res3.x);
                WORKING_REG_A.x+=2;
								_pc+=4;
                break;
              case DWORD_SIZE:
                res3.d = GetIntValue(WORKING_REG_A.x) ^ Pipe2.d;
                PutIntValue(WORKING_REG_A.x, res3.d);
                WORKING_REG_A.x+=4;
								_pc+=6;
                break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                WORKING_REG_A.x--;
                res3.b.l = GetValue(WORKING_REG_A.x) ^ Pipe2.b.l;
                PutValue(WORKING_REG_A.x, res3.b.l);
								_pc+=4;
                break;
              case WORD_SIZE:
                WORKING_REG_A.x-=2;
                res3.x = GetShortValue(WORKING_REG_A.x) ^ Pipe2.x.l;
                PutShortValue(WORKING_REG_A.x, res3.x);
								_pc+=4;
                break;
              case DWORD_SIZE:
                WORKING_REG_A.x-=4;
                res3.d = GetIntValue(WORKING_REG_A.x) ^ Pipe2.d;
                PutIntValue(WORKING_REG_A.x, res3.d);
								_pc+=6;
                break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) ^ Pipe2.b.l;
                PutValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h, res3.b.l);
								_pc+=6;
                break;
              case WORD_SIZE:
                res3.x = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) ^ Pipe2.x.l;
                PutShortValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h, res3.x);
								_pc+=6;
                break;
              case DWORD_SIZE:
                res3.d = GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) ^ Pipe2.d;
                PutIntValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h, res3.d);
								_pc+=6;
                break;
              }
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.b.l = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) ^ Pipe2.b.l;
                  PutValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l, res3.b.l);
                  }
                else {
                  res3.b.l = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) ^ Pipe2.b.l;
                  PutValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l, res3.b.l);
                  }
								_pc+=6;
                break;
              case WORD_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.x = GetShortValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) ^ Pipe2.x.l;
                  PutShortValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l, res3.x);
                  }
                else {
                  res3.x = GetShortValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) ^ Pipe2.x.l;
                  PutShortValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l, res3.x);
                  }
								_pc+=6;
                break;
              case DWORD_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.d = GetIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) ^ Pipe2.d;
                  PutIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l, res3.d);
                  }
                else {
                  res3.d = GetIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) ^ Pipe2.d;
                  PutIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l, res3.d);
                  }
								_pc+=8;
                break;
              }
            break;
          }
        goto aggFlag;
				break;

			case 0x0c:   // CMPI
        switch(ADDRESSING) {
          case ABSOLUTELADD:
            if(!ABSOLUTEADD_SIZE)
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  res3.b.l = GetValue(Pipe2.x.h) - Pipe2.b.l;
                  _pc+=6;
                  break;
                case WORD_SIZE:
                  res3.x = GetShortValue(Pipe2.x.h) - Pipe2.x.l;
                  _pc+=6;
                  break;
                case DWORD_SIZE:
                  res3.d = Pipe2.d;
                  _pc+=2;
                  GetPipe(_pc);
                  res3.d -= GetIntValue(Pipe2.x.h);
                  _pc+=6;
                  break;
                }
            else
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  res3.b.l = Pipe2.b.l;
                  _pc+=2;
                  GetPipe(_pc);
                  res3.b.l -= GetValue(Pipe2.x.h);
                  _pc+=6;
                  break;
                case WORD_SIZE:
                  res3.x = Pipe2.x.l;
                  _pc+=2;
                  GetPipe(_pc);
                  res3.x -= GetShortValue(Pipe2.x.h);
                  _pc+=6;
                  break;
                case DWORD_SIZE:
                  res3.d = Pipe2.d;
                  _pc+=4;
                  GetPipe(_pc);
                  res3.d -= GetIntValue(Pipe2.d); 
                  _pc+=6;
                  break;
                }
            break;
          case DATAREGADD:   // Dn
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = WORKING_REG_D.b.b0 - Pipe2.b.l;
								_pc+=4;
                break;
              case WORD_SIZE:
                res3.x = WORKING_REG_D.w.l - Pipe2.x.l;
								_pc+=4;
                break;
              case DWORD_SIZE:
                res3.d = WORKING_REG_D.x - Pipe2.d;
								_pc+=6;
                break;
              }
            break;
          case ADDRREGADD:   // An
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GetValue(WORKING_REG_A.x) - Pipe2.b.l;
								_pc+=4;
                break;
              case WORD_SIZE:
                res3.x = GetShortValue(WORKING_REG_A.x) - Pipe2.x.l;
								_pc+=4;
                break;
              case DWORD_SIZE:
                res3.d = GetIntValue(WORKING_REG_A.x) - Pipe2.d;
								_pc+=6;
                break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GetValue(WORKING_REG_A.x) - Pipe2.b.l;
                WORKING_REG_A.x++;
								_pc+=4;
                break;
              case WORD_SIZE:
                res3.x = GetShortValue(WORKING_REG_A.x) - Pipe2.x.l;
                WORKING_REG_A.x+=2;
								_pc+=4;
                break;
              case DWORD_SIZE:
                res3.d = GetIntValue(WORKING_REG_A.x) - Pipe2.d;
                WORKING_REG_A.x+=4;
								_pc+=6;
                break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                WORKING_REG_A.x--;
                res3.b.l = GetValue(WORKING_REG_A.x) - Pipe2.b.l;
								_pc+=4;
                break;
              case WORD_SIZE:
                WORKING_REG_A.x-=2;
                res3.x = GetShortValue(WORKING_REG_A.x) - Pipe2.x.l;
								_pc+=4;
                break;
              case DWORD_SIZE:
                WORKING_REG_A.x-=4;
                res3.d = GetIntValue(WORKING_REG_A.x) - Pipe2.d;
								_pc+=6;
                break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) - Pipe2.b.l;
								_pc+=6;
                break;
              case WORD_SIZE:
                res3.x = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) - Pipe2.x.l;
								_pc+=6;
                break;
              case DWORD_SIZE:
                res3.d = GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) - Pipe2.d;
								_pc+=8;
                break;
              }
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.b.l = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) - Pipe2.b.l;
                  }
                else {
                  res3.b.l = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) - Pipe2.b.l;
                  }
								_pc+=6;
                break;
              case WORD_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.x = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) - Pipe2.x.l;
                  }
                else {
                  res3.x = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) - Pipe2.x.l;
                  }
								_pc+=6;
                break;
              case DWORD_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.d = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) - Pipe2.d;
                  }
                else {
                  res3.d = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) - Pipe2.d;
                  }
								_pc+=8;
                break;
              }
            break;
          }
aggFlagC:
#warning SERVONO res1 e res2 per Ovf!
        switch(OPERAND_SIZE) {
          case BYTE_SIZE:
            _f.CCR.Zero=!res3.b.l;
            _f.CCR.Sign=!!(res3.b.l & 0x80);
            _f.CCR.Carry=!!(res3.b.l & 0x80);
		        _f.CCR.Ovf = !!(((res1.b.l & 0x40) + (res2.b.l & 0x40)) & 0x80) != !!(((res1.x & 0x80) + (res2.x & 0x80)) & 0x100);
            break;
          case WORD_SIZE:
            _f.CCR.Zero=!res3.x;
            _f.CCR.Sign=!!(res3.x & 0x8000);
            _f.CCR.Carry=!!(res3.x & 0x8000);
            _f.CCR.Ovf = !!(((res1.b.h & 0x40) + (res2.b.h & 0x40)) & 0x80) != !!(((res1.d & 0x8000) + (res2.d & 0x8000)) & 0x10000);
            break;
          case DWORD_SIZE:
            _f.CCR.Zero=!res3.d;
            _f.CCR.Sign=!!(res3.d & 0x80000000);
            _f.CCR.Carry=!!(res3.d & 0x80000000);
            _f.CCR.Ovf = !!(((res1.d & 0x40000000) + (res2.d & 0x40000000)) & 0x80000000) != 
                    !!((((res1.d >> 16) & 0x8000) + ((res2.d >> 16) & 0x8000)) & 0x10000);
            // credo sia meglio di usare QWORD longlong...
            break;
          }
				break;
			
			case 0x10:   // MOVEA MOVE.B
      case 0x11:
      case 0x12:
      case 0x13:
      case 0x14:
      case 0x15:
      case 0x16:
      case 0x17:
      case 0x18:
      case 0x19:
      case 0x1a:
      case 0x1b:
      case 0x1c:
      case 0x1d:
      case 0x1e:
      case 0x1f:
        switch(ADDRESSING) {
          case ABSOLUTELADD:
            if(!ABSOLUTEADD_SIZE) {
              res3.b.l = GetValue(Pipe2.x.h);
              _pc+=6;
              }
            else {
              _pc+=2;
              GetPipe(_pc);
              res3.b.l = GetValue(Pipe2.x.h);
              _pc+=6;
              }
            break;
          case DATAREGADD:   // Dn
            res3.b.l = WORKING_REG_D.b.b0;
            _pc+=4;
            break;
          case ADDRREGADD:   // An
            break;
          case ADDRADD:   // (An)
            res3.b.l = GetValue(WORKING_REG_A.x);
            _pc+=4;
            break;
          case ADDRPINCADD:   // (An)+
            res3.b.l = GetValue(WORKING_REG_A.x);
            WORKING_REG_A.x++;
            _pc+=4;
            break;
          case ADDRPDECADD:   // -(An)
            WORKING_REG_A.x--;
            res3.b.l = GetValue(WORKING_REG_A.x);
            _pc+=4;
            break;
          case ADDRDISPADD:   // (D16,An)
            res3.b.l = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h);
            _pc+=6;
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            if(!DISPLACEMENT_SIZE) {
              res3.b.l = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l);
              }
            else {
              res3.b.l = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l);
              }
            _pc+=6;
            break;
          }
        switch(ADDRESSING2) {
          case ABSOLUTELADD:
#warning finire!
            if(!ABSOLUTEADD_SIZE2) {
              PutValue(Pipe2.x.h,res3.b.l);
              _pc+=6;
              }
            else {
              _pc+=2;
              GetPipe(_pc);
              PutValue(Pipe2.x.h,res3.b.l);
              _pc+=6;
              }
            break;
          case DATAREGADD:   // Dn
            DEST_REG_D.b.b0 = res3.b.l;
            break;
          case ADDRREGADD:   // An
            // qua (byte) no!
            break;
          case ADDRADD:   // (An)
            PutValue(DEST_REG_A.x,res3.b.l);
            break;
          case ADDRPINCADD:   // (An)+
            PutValue(DEST_REG_A.x,res3.b.l);
            WORKING_REG_A.x++;
            break;
          case ADDRPDECADD:   // -(An)
            DEST_REG_A.x--;
            PutValue(DEST_REG_A.x,res3.b.l);
            break;
          case ADDRDISPADD:   // (D16,An)
            PutValue(DEST_REG_A.x + (signed short int)Pipe2.x.l,res3.b.l);
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            if(!DISPLACEMENT_SIZE)
              PutValue(DEST_REG_A.x + DISPLACEMENT_REG2.w.l + (signed char)Pipe2.b.l,res3.b.l);
            else
              PutValue(DEST_REG_A.x + DISPLACEMENT_REG2.x + (signed char)Pipe2.b.l,res3.b.l);
            break;
          }
        goto aggFlag;
				break;
        
			case 0x20:   // MOVEA MOVE.D
      case 0x21:
      case 0x22:
      case 0x23:
      case 0x24:
      case 0x25:
      case 0x26:
      case 0x27:
      case 0x28:
      case 0x29:
      case 0x2a:
      case 0x2b:
      case 0x2c:
      case 0x2d:
      case 0x2e:
      case 0x2f:
        switch(ADDRESSING) {
          case ABSOLUTELADD:
            if(!ABSOLUTEADD_SIZE) {
              _pc+=2;
              GetPipe(_pc);
              res3.d = GetIntValue(Pipe2.x.h);
              _pc+=6;
              }
            else {
              _pc+=4;
              GetPipe(_pc);
              res3.d = GetIntValue(Pipe2.d); 
              _pc+=6;
              }
            break;
          case DATAREGADD:   // Dn
            res3.d = WORKING_REG_D.x;
            _pc+=6;
            break;
          case ADDRREGADD:   // An
            res3.d = WORKING_REG_A.x;
            break;
          case ADDRADD:   // (An)
            res3.d = GetIntValue(WORKING_REG_A.x);
            _pc+=6;
            break;
          case ADDRPINCADD:   // (An)+
            res3.d = GetIntValue(WORKING_REG_A.x);
            WORKING_REG_A.x+=4;
            _pc+=6;
            break;
          case ADDRPDECADD:   // -(An)
            WORKING_REG_A.x-=4;
            res3.d = GetIntValue(WORKING_REG_A.x);
            _pc+=6;
            break;
          case ADDRDISPADD:   // (D16,An)
            res3.d = GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h);
            _pc+=6;
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            if(!DISPLACEMENT_SIZE) {
              res3.d = GetIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l);
              }
            else {
              res3.d = GetIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l);
              }
            _pc+=8;
            break;
          }
        switch(ADDRESSING2) {
          case ABSOLUTELADD:
  #warning finire!
            if(!ABSOLUTEADD_SIZE2) {
              PutValue(Pipe2.x.h,res3.d);
              _pc+=6;
              }
            else {
              _pc+=2;
              GetPipe(_pc);
              PutValue(Pipe2.x.h,res3.d);
              _pc+=6;
              }
            break;
          case DATAREGADD:   // Dn
            DEST_REG_D.x = res3.d;
            break;
          case ADDRREGADD:   // An
            DEST_REG_A.x = res3.d;
            break;
          case ADDRADD:   // (An)
            PutIntValue(DEST_REG_A.x,res3.d);
            break;
          case ADDRPINCADD:   // (An)+
            PutIntValue(DEST_REG_A.x,res3.d);
            DEST_REG_A.x+=4;
            break;
          case ADDRPDECADD:   // -(An)
            DEST_REG_A.x-=4;
            PutIntValue(DEST_REG_A.x,res3.d);
            break;
          case ADDRDISPADD:   // (D16,An)
            PutIntValue(DEST_REG_A.x + (signed short int)Pipe2.x.l,res3.d);
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            if(!DISPLACEMENT_SIZE2)
              PutIntValue(DEST_REG_A.x + DISPLACEMENT_REG2.w.l + (signed char)Pipe2.b.l,res3.d);
            else
              PutIntValue(DEST_REG_A.x + DISPLACEMENT_REG2.x + (signed char)Pipe2.b.l,res3.d);
            break;
          }
        goto aggFlag;
				break;
        
			case 0x30:   // MOVEA MOVE.W
      case 0x31:
      case 0x32:
      case 0x33:
      case 0x34:
      case 0x35:
      case 0x36:
      case 0x37:
      case 0x38:
      case 0x39:
      case 0x3a:
      case 0x3b:
      case 0x3c:
      case 0x3d:
      case 0x3e:
      case 0x3f:
        switch(ADDRESSING) {
          case ABSOLUTELADD:
            if(!ABSOLUTEADD_SIZE) {
              res3.x = GetShortValue(Pipe2.x.h);
              _pc+=6;
              }
            else {
              _pc+=2;
              GetPipe(_pc);
              res3.x = GetShortValue(Pipe2.x.h);
              _pc+=6;
               }
            break;
          case DATAREGADD:   // Dn
            res3.x = WORKING_REG_D.w.l;
            _pc+=4;
            break;
          case ADDRREGADD:   // An
            res3.x = WORKING_REG_A.w.l;
            break;
          case ADDRADD:   // (An)
            res3.x = GetShortValue(WORKING_REG_A.x);
            _pc+=4;
            break;
          case ADDRPINCADD:   // (An)+
            res3.x = GetShortValue(WORKING_REG_A.x);
            WORKING_REG_A.x+=2;
            _pc+=4;
            break;
          case ADDRPDECADD:   // -(An)
            WORKING_REG_A.x-=2;
            res3.x = GetShortValue(WORKING_REG_A.x);
            _pc+=4;
            break;
          case ADDRDISPADD:   // (D16,An)
            res3.x = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h);
            _pc+=6;
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            if(!DISPLACEMENT_SIZE) {
              res3.x = GetShortValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l);
              }
            else {
              res3.x = GetShortValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l);
              }
            _pc+=6;
            break;
          }
        switch(ADDRESSING2) {
          case ABSOLUTELADD:
  #warning finire!
            if(!ABSOLUTEADD_SIZE2) {
              PutValue(Pipe2.x.h,res3.x);
              _pc+=6;
              }
            else {
              _pc+=2;
              GetPipe(_pc);
              PutValue(Pipe2.x.h,res3.x);
              _pc+=6;
              }
            break;
          case DATAREGADD:   // Dn
            DEST_REG_D.w.l = res3.x;
            break;
          case ADDRREGADD:   // An
            DEST_REG_A.w.l = res3.x;
            break;
          case ADDRADD:   // (An)
            PutShortValue(DEST_REG_A.x,res3.x);
            break;
          case ADDRPINCADD:   // (An)+
            PutShortValue(DEST_REG_A.x,res3.x);
            DEST_REG_A.x+=2;
            break;
          case ADDRPDECADD:   // -(An)
            DEST_REG_A.x-=2;
            PutShortValue(DEST_REG_A.x,res3.x);
            break;
          case ADDRDISPADD:   // (D16,An)
            PutShortValue(DEST_REG_A.x + (signed short int)Pipe2.x.l,res3.x);
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            if(!DISPLACEMENT_SIZE2)
              PutShortValue(DEST_REG_A.x + DISPLACEMENT_REG2.w.l + (signed char)Pipe2.b.l,res3.x);
            else
              PutShortValue(DEST_REG_A.x + DISPLACEMENT_REG2.x + (signed char)Pipe2.b.l,res3.x);
            break;
          }
        goto aggFlag;
				break;
        
			case 0x40:   // MOVE from SR, NEGX
        switch(ADDRESSING) {
          case ABSOLUTELADD:
            break;
          case DATAREGADD:   // Dn
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRREGADD:   // An
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          }
				break;
        
			case 0x42:   // CLR
				break;
        
			case 0x41:   // LEA CHK
			case 0x43:
			case 0x45:
			case 0x47:
			case 0x49:
			case 0x4b:
			case 0x4d:
			case 0x4f:
				break;
        
			case 0x44:   // MOVE to CCR
        switch(ADDRESSING) {
          case ABSOLUTELADD:
            break;
          case DATAREGADD:   // Dn
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRREGADD:   // An
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          }
				break;
        
			case 0x46:   // MOVE to SR, NOT
        switch(ADDRESSING) {
          case ABSOLUTELADD:
            break;
          case DATAREGADD:   // Dn
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRREGADD:   // An
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          }
				break;
        
			case 0x48:   // EXT NBCD SWAP PEA MOVEM
				break;
        
			case 0x4a:   // ILLEGAL TAS TST 
        switch(ADDRESSING) {
          case ABSOLUTELADD:
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                break;
              case WORD_SIZE:
                break;
              case DWORD_SIZE:
                break;
              case 0xc0:      // ILLEGAL
                // 100
                break;
              }
            break;
          case DATAREGADD:   // Dn
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = WORKING_REG_D.b.b0 & Pipe2.b.l;
                
                break;
              case WORD_SIZE:
                res3.x = WORKING_REG_D.w.l & Pipe2.x.l;
                
                break;
              case DWORD_SIZE:
                res3.d = WORKING_REG_D.x & Pipe2.d;
                
								_pc+=2;
                break;
              case 0xc0:    // TAS
                res3.b.l = WORKING_REG_D.b.b0 & Pipe2.b.l;
                WORKING_REG_D.b.b0 |= 0x80;
                
								_pc++;
                break;
              }
            break;
          case ADDRREGADD:   // An
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = WORKING_REG_A.b.b0 & Pipe2.b.l;
                
                break;
              case WORD_SIZE:
                res3.x = WORKING_REG_A.w.l & Pipe2.x.l;
                
                break;
              case DWORD_SIZE:
                res3.d = WORKING_REG_A.x & Pipe2.d;
                
								_pc+=2;
                break;
              case 0xc0:    // TAS
                res3.b.l = WORKING_REG_A.b.b0 & Pipe2.b.l;
                WORKING_REG_A.b.b0 |= 0x80;
                
								_pc++;
                break;
              }
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GetValue(WORKING_REG_A.x) & Pipe2.b.l;
                
                break;
              case WORD_SIZE:
                res3.x = GetShortValue(WORKING_REG_A.x) & Pipe2.x.l;
                
                break;
              case DWORD_SIZE:
                res3.d = GetIntValue(WORKING_REG_A.x) & Pipe2.d;
                
								_pc+=2;
                break;
              case 0xc0:    // TAS
                res3.b.l = GetValue(WORKING_REG_A.x) & Pipe2.b.l;
                res3.b.l |= 0x80;
                PutValue(WORKING_REG_A.x, res3.b.l);
                
								_pc++;
                break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GetValue(WORKING_REG_A.x) & Pipe2.b.l;
                WORKING_REG_A.x++;                
                
                break;
              case WORD_SIZE:
                res3.x = GetShortValue(WORKING_REG_A.x) & Pipe2.x.l;
                WORKING_REG_A.x+=2;
                
                break;
              case DWORD_SIZE:
                res3.d = GetIntValue(WORKING_REG_A.x) & Pipe2.d;
                WORKING_REG_A.x+=4;
                
								_pc+=2;
                break;
              case 0xc0:    // TAS
                res3.b.l = GetValue(WORKING_REG_A.x) & Pipe2.b.l;
                res3.b.l |= 0x80;
                PutValue(WORKING_REG_A.x, res3.b.l);
                WORKING_REG_A.x++;
                
								_pc++;
                break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                WORKING_REG_A.x--;
                res3.b.l = GetValue(WORKING_REG_A.x) & Pipe2.b.l;
                
                break;
              case WORD_SIZE:
                WORKING_REG_A.x-=2;
                res3.x = GetShortValue(WORKING_REG_A.x) & Pipe2.x.l;
                
                break;
              case DWORD_SIZE:
                WORKING_REG_A.x-=4;
                res3.d = GetIntValue(WORKING_REG_A.x) & Pipe2.d;
                
								_pc+=2;
                break;
              case 0xc0:    // TAS
                WORKING_REG_A.x--;
                res3.b.l = GetValue(WORKING_REG_A.x) & Pipe2.b.l;
                res3.b.l |= 0x80;
                PutValue(WORKING_REG_A.x, res3.b.l);
                
								_pc++;
                break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) & Pipe2.b.l;

                break;
              case WORD_SIZE:
                res3.x = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) & Pipe2.x.l;

                break;
              case DWORD_SIZE:
                res3.d = GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) & Pipe2.d;
                
								_pc+=3;
                break;
              case 0xc0:    // TAS
                res3.b.l = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) & Pipe2.b.l;
                res3.b.l |= 0x80;
                PutValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h, res3.b.l);
                
								_pc+=2;
                break;
              }
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.b.l = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) & Pipe2.b.l;
                  }
                else {
                  res3.b.l = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) & Pipe2.b.l;
                  }

                break;
              case WORD_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.x = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) & Pipe2.x.l;
                  }
                else {
                  res3.x = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) & Pipe2.x.l;
                  }

                break;
              case DWORD_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.d = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) & Pipe2.d;
                  }
                else {
                  res3.d = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) & Pipe2.d;
                  }
                
								_pc+=3;
                break;
              case 0xc0:    // TAS
                if(!DISPLACEMENT_SIZE) {
                  res3.b.l = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) & Pipe2.d;
                  }
                else {
                  res3.b.l = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) & Pipe2.d;
                  }
                res3.b.l |= 0x80;
                if(!DISPLACEMENT_SIZE) {
                  PutValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l, res3.b.l);
                  }
                else {
                  PutValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l, res3.b.l);
                  }
                
								_pc+=3;
                break;
              }
            break;
          }
				break;
        
			case 0x4c:   // MOVEM
				break;
        
			case 0x4e:   // TRAP LINK UNLK MOVEUSP RESET NOP STOP RTE RTS TRAPV RTR JSR JMP
        switch(LOBYTE(Pipe1)) {
          case 0x70:   // RESET
            DoReset=1;
            break;
          case 0x71:   // NOP

            break;
          case 0x72:   // STOP

            break;
          case 0x73:   // RTE
            if(_f.SR.Supervisor) {
              _f.w=GetIntValue(_f.SR.Supervisor ? _ssp : _sp);
              // FINIREEEEEEEE
              _ssp+=2;
              _pc=GetIntValue(_f.SR.Supervisor ? _ssp : _sp);
              _ssp+=4;
              }
            else {
              //ECCEZIONE!
              }

            break;
          case 0x75:   // RTS
            _pc=GetIntValue(_f.SR.Supervisor ? _ssp : _sp);
            if(_f.SR.Supervisor) 
              _ssp+=4;
            else
              _sp+=4;

            break;
          case 0x76:   // TRAPV
            if(_f.CCR.Ovf) {
              PutIntValue(_f.SR.Supervisor ? --_ssp : --_sp,_pc); // low word prima
              _f1=_f;
              PutIntValue(_f.SR.Supervisor ? --_ssp : --_sp,_f1.w);
              _f.SR.Supervisor=1;
              _pc=GetIntValue(0x0000001c);
              }
            break;
          case 0x77:   // RTR
            _f.CCR.b=GetIntValue(_f.SR.Supervisor ? _ssp : _sp);
            // FINIREEEEEEEE
            if(_f.SR.Supervisor) 
              _ssp+=2;
            else
              _sp+=2;
            _pc=GetIntValue(_f.SR.Supervisor ? _ssp : _sp);
            if(_f.SR.Supervisor) 
              _ssp+=4;
            else
              _sp+=4;

            break;
          case 0x40:   // TRAP
          case 0x41:
          case 0x42:
          case 0x43:
          case 0x44:
          case 0x45:
          case 0x46:
          case 0x47:
          case 0x48:
          case 0x49:
          case 0x4a:
          case 0x4b:
          case 0x4c:
          case 0x4d:
          case 0x4e:
          case 0x4f:
            PutIntValue(_f.SR.Supervisor ? --_ssp : --_sp,_pc); // low word prima
            _f1=_f;
            PutIntValue(_f.SR.Supervisor ? --_ssp : --_sp,_f1.w);
            _f.SR.Supervisor=1;
            _pc=GetIntValue(0x00000080+(LOBYTE(Pipe1) & 0xf)*4);
            break;
          case 0x50:   // LINK
          case 0x51:
          case 0x52:
          case 0x53:
          case 0x54:
          case 0x55:
          case 0x56:
          case 0x57:
            break;
          case 0x58:    //UNLK
          case 0x59:
          case 0x5a:
          case 0x5b:
          case 0x5c:
          case 0x5d:
          case 0x5e:
          case 0x5f:
            break;
          case 0x60:   // MOVE USP
          case 0x61:
          case 0x62:
          case 0x63:
          case 0x64:
          case 0x65:
          case 0x66:
          case 0x67:
          case 0x68:
          case 0x69:
          case 0x6a:
          case 0x6b:
          case 0x6c:
          case 0x6d:
          case 0x6e:
          case 0x6f:
            break;
          default:
            if(!(LOBYTE(Pipe1) & 0b01000000)) {   // JSR
              if(_f.SR.Supervisor) 
                _ssp-=4;
              else
                _sp-=4;
              switch(ADDRESSING) {
                case ABSOLUTELADD:
                  if(LOBYTE(Pipe1) & 1) {
                    PutIntValue(_f.SR.Supervisor ? _ssp : _sp,_pc+6);  // low word prima
                    _pc=Pipe2.d;
                    }
                  else {
                    PutIntValue(_f.SR.Supervisor ? _ssp : _sp,_pc+4);  // low word prima
                    _pc=Pipe2.x.l;
                    }
                  break;
                case DATAREGADD:   // Dn
                  break;
                case ADDRREGADD:   // An
                  break;
                case ADDRADD:   // (An)
                  PutIntValue(_f.SR.Supervisor ? _ssp : _sp,_pc+2);  // 
                  _pc=GetIntValue(WORKING_REG_A.x);
                  break;
                case ADDRPINCADD:   // (An)+
                  break;
                case ADDRPDECADD:   // -(An)
                  break;
                case ADDRDISPADD:   // (D16,An)
                  PutIntValue(_f.SR.Supervisor ? _ssp : _sp,_pc+4);  // 
                  _pc=GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h);
                  break;
                case ADDRIDXADD:   // (D8,An,Xn)
                  PutIntValue(_f.SR.Supervisor ? _ssp : _sp,_pc+4);  // 
                  if(!DISPLACEMENT_SIZE)
                    _pc=GetIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l);
                  else
                    _pc=GetIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l);
                  break;
                }
              }
            else {    // JMP
              switch(ADDRESSING) {
                case ABSOLUTELADD:
                  if(LOBYTE(Pipe1) & 1) {
                    _pc=Pipe2.d;
                    }
                  else {
                    _pc=Pipe2.x.l;
                    }
                  break;
                case DATAREGADD:   // Dn
                  break;
                case ADDRREGADD:   // An
                  break;
                case ADDRADD:   // (An)
                  _pc=GetIntValue(WORKING_REG_A.x);
                  break;
                case ADDRPINCADD:   // (An)+
                  break;
                case ADDRPDECADD:   // -(An)
                  break;
                case ADDRDISPADD:   // (D16,An)
                  _pc=GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h);
                  break;
                case ADDRIDXADD:   // (D8,An,Xn)
                  if(!DISPLACEMENT_SIZE)
                    _pc=GetIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l);
                  else
                    _pc=GetIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l);
                  break;
                }
              break;
            }
          }
				break;

			case 0x50:   // ADDQ Scc DBcc
			case 0x52:
			case 0x54:
			case 0x56:
			case 0x58:
			case 0x5a:
			case 0x5c:
			case 0x5e:
        switch(OPERAND_SIZE) {
          case 0xc0:
            switch(Pipe1 & 0xf) {   // Scc DBcc
              case 0x0:
                break;
              case 0x1:
                break;
              case 0x2:
                break;
              case 0x3:
                break;
              case 0x4:
                break;
              case 0x5:
                break;
              case 0x6:
                break;
              case 0x7:
                break;
              case 0x8:
                break;
              case 0x9:
                break;
              case 0xa:
                break;
              case 0xb:
                break;
              case 0xc:
                break;
              case 0xd:
                break;
              case 0xe:
                break;
              case 0xf:
                break;
              }
            break;
          default:
            res2.b.l=(Pipe1 & 0b0000111000000000) >> 9;
            if(!res2.b.l)
              res2.b.l=8;
            switch(ADDRESSING) {
              case ABSOLUTELADD:    // abs
                if(!ABSOLUTEADD_SIZE)
                  switch(OPERAND_SIZE) {
                    case BYTE_SIZE:
                      res3.b.l = GetValue(Pipe2.x.h) + res2.b.l;
                      PutValue(Pipe2.x.h, res3.b.l);
                      _pc+=6;
                      break;
                    case WORD_SIZE:
                      res3.x = GetShortValue(Pipe2.x.h) + res2.b.l;
                      PutShortValue(Pipe2.x.h, res3.x);
                      _pc+=6;
                      break;
                    case DWORD_SIZE:
                      res3.d = Pipe2.d;
                      _pc+=2;
                      GetPipe(_pc);
                      res3.d + res2.b.l;
                      PutIntValue(Pipe2.x.h, res3.d);
                      _pc+=6;
                      break;
                    }
                else
                  switch(OPERAND_SIZE) {
                    case BYTE_SIZE:
                      res3.b.l = Pipe2.b.l;
                      _pc+=2;
                      GetPipe(_pc);
                      res3.b.l + res2.b.l;
                      PutValue(Pipe2.x.h, res3.b.l);
                      _pc+=6;
                      break;
                    case WORD_SIZE:
                      res3.x = Pipe2.x.l;
                      _pc+=2;
                      GetPipe(_pc);
                      res3.x + res2.b.l;
                      PutShortValue(Pipe2.x.h, res3.x);
                      _pc+=6;
                      break;
                    case DWORD_SIZE:
                      res3.d = Pipe2.d;
                      _pc+=4;
                      GetPipe(_pc);
                      res3.d + res2.b.l;
                      PutIntValue(Pipe2.x.h, res3.d);
                      _pc+=6;
                      break;
                    }
                break;
              case DATAREGADD:   // Dn
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res3.b.l = WORKING_REG_D.b.b0 += res2.b.l;
                    break;
                  case WORD_SIZE:
                    res3.x = WORKING_REG_D.w.l += res2.b.l;
                    break;
                  case DWORD_SIZE:
                    res3.d = WORKING_REG_D.x += res2.b.l;
                    break;
                  }
                _pc+=2;
                goto aggFlagC;
                break;
              case ADDRREGADD:   // An
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    break;
                  case WORD_SIZE:
                    res3.x = WORKING_REG_A.w.l += res2.b.l;
                    break;
                  case DWORD_SIZE:
                    res3.d = WORKING_REG_A.x += res2.b.l;
                    break;
                  }// NO flag!
                _pc+=2;
                break;
              case ADDRADD:   // (An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res3.b.l = GetValue(WORKING_REG_A.x) + res2.b.l;
                    PutValue(WORKING_REG_A.x,res3.b.l);
                    break;
                  case WORD_SIZE:
                    res3.x = GetShortValue(WORKING_REG_A.x) + res2.b.l;
                    PutShortValue(WORKING_REG_A.x,res3.x);
                    break;
                  case DWORD_SIZE:
                    res3.d = GetIntValue(WORKING_REG_A.x) + res2.b.l;
                    PutIntValue(WORKING_REG_A.x,res3.d);
                    break;
                  }
                _pc+=2;
                goto aggFlagC;
                break;
              case ADDRPINCADD:   // (An)+
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res3.b.l = GetValue(WORKING_REG_A.x) + res2.b.l;
                    PutValue(WORKING_REG_A.x,res3.b.l);
                    WORKING_REG_A.x++;
                    break;
                  case WORD_SIZE:
                    res3.x = GetShortValue(WORKING_REG_A.x) + res2.b.l;
                    PutShortValue(WORKING_REG_A.x,res3.x);
                    WORKING_REG_A.x+=2;
                    break;
                  case DWORD_SIZE:
                    res3.d = GetIntValue(WORKING_REG_A.x) + res2.b.l;
                    PutIntValue(WORKING_REG_A.x,res3.d);
                    WORKING_REG_A.x+=4;
                    break;
                  }
                _pc+=2;
                goto aggFlagC;
                break;
              case ADDRPDECADD:   // -(An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    WORKING_REG_A.x--;
                    res3.b.l = GetValue(WORKING_REG_A.x) + res2.b.l;
                    break;
                  case WORD_SIZE:
                    WORKING_REG_A.x-=2;
                    res3.x = GetShortValue(WORKING_REG_A.x) + res2.b.l;
                    PutShortValue(WORKING_REG_A.x,res3.x);
                    break;
                  case DWORD_SIZE:
                    WORKING_REG_A.x-=4;
                    res3.d = GetIntValue(WORKING_REG_A.x) + res2.b.l;
                    PutIntValue(WORKING_REG_A.x,res3.d);
                    break;
                  }
                _pc+=2;
                goto aggFlagC;
                break;
              case ADDRDISPADD:   // (D16,An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res3.b.l = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) - Pipe2.b.l;
                    PutValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h,res3.b.l);
                    _pc+=6;
                    break;
                  case WORD_SIZE:
                    res3.x = GetShortValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) - Pipe2.x.l;
                    PutShortValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h,res3.x);
                    _pc+=6;
                    break;
                  case DWORD_SIZE:
                    res3.d = GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) - Pipe2.d;
                    PutIntValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h, res3.d);
                    _pc+=8;
                    break;
                  }
                break;
              case ADDRIDXADD:   // (D8,An,Xn)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    if(!DISPLACEMENT_SIZE) {
                      res3.b.l = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) - Pipe2.b.l;
                      PutValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l,res3.b.l);
                      }
                    else {
                      res3.b.l = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) - Pipe2.b.l;
                      PutValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l, res3.b.l);
                      }
                    _pc+=6;
                    break;
                  case WORD_SIZE:
                    if(!DISPLACEMENT_SIZE) {
                      res3.x = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) - Pipe2.x.l;
                      PutValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l, res3.x);
                      }
                    else {
                      res3.x = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) - Pipe2.x.l;
                      PutValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l, res3.x);
                      }
                    _pc+=6;
                    break;
                  case DWORD_SIZE:
                    if(!DISPLACEMENT_SIZE) {
                      res3.d = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) - Pipe2.d;
                      PutValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l, res3.d);
                      }
                    else {
                      res3.d = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) - Pipe2.d;
                      PutValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l, res3.d);
                      }
                    _pc+=8;
                    break;
                  }
                break;
              }
            break;
          }
				break;
        
			case 0x51:   // SUBQ Scc DBcc
			case 0x53:
			case 0x55:
			case 0x57:
			case 0x59:
			case 0x5b:
			case 0x5d:
			case 0x5f:
        switch(OPERAND_SIZE) {
          case 0xc0:
            switch(Pipe1 & 0xf) {   // Scc DBcc
              case 0x0:
                break;
              case 0x1:
                break;
              case 0x2:
                break;
              case 0x3:
                break;
              case 0x4:
                break;
              case 0x5:
                break;
              case 0x6:
                break;
              case 0x7:
                break;
              case 0x8:
                break;
              case 0x9:
                break;
              case 0xa:
                break;
              case 0xb:
                break;
              case 0xc:
                break;
              case 0xd:
                break;
              case 0xe:
                break;
              case 0xf:
                break;
              }
            break;
          default:
            res2.b.l=(Pipe1 & 0b0000111000000000) >> 9;
            if(!res2.b.l)
              res2.b.l=8;
            switch(ADDRESSING) {
              case ABSOLUTELADD:    // abs
                if(!ABSOLUTEADD_SIZE)
                  switch(OPERAND_SIZE) {
                    case BYTE_SIZE:
                      res3.b.l = GetValue(Pipe2.x.h) - Pipe2.b.l;
                      PutValue(Pipe2.x.h,res3.b.l);
                      _pc+=6;
                      break;
                    case WORD_SIZE:
                      res3.x = GetShortValue(Pipe2.x.h) - Pipe2.x.l;
                      PutShortValue(Pipe2.x.h,res3.x);
                      _pc+=6;
                      break;
                    case DWORD_SIZE:
                      res3.d = Pipe2.d;
                      _pc+=2;
                      GetPipe(_pc);
                      res3.d -= GetIntValue(Pipe2.x.h);
                      PutIntValue(Pipe2.d,res3.d); 
                      _pc+=6;
                      break;
                    }
                else
                  switch(OPERAND_SIZE) {
                    case BYTE_SIZE:
                      res3.b.l = Pipe2.b.l;
                      _pc+=2;
                      GetPipe(_pc);
                      res3.b.l -= GetValue(Pipe2.x.h);
                      PutValue(Pipe2.x.h,res3.b.l);
                      _pc+=6;
                      break;
                    case WORD_SIZE:
                      res3.x = Pipe2.x.l;
                      _pc+=2;
                      GetPipe(_pc);
                      res3.x -= GetShortValue(Pipe2.x.h);
                      PutShortValue(Pipe2.x.h,res3.x);
                      _pc+=6;
                      break;
                    case DWORD_SIZE:
                      res3.d = Pipe2.d;
                      _pc+=4;
                      GetPipe(_pc);
                      res3.d -= GetIntValue(Pipe2.d); 
                      PutIntValue(Pipe2.d,res3.d); 
                      _pc+=6;
                      break;
                    }
                goto aggFlagC;
                break;
              case DATAREGADD:   // Dn
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res3.b.l = (WORKING_REG_D.b.b0 -= res2.b.l);
                    break;
                  case WORD_SIZE:
                    res3.x = (WORKING_REG_D.w.l -= res2.b.l);
                    break;
                  case DWORD_SIZE:
                    res3.d = (WORKING_REG_D.x -= res2.b.l);
                    break;
                  }
                _pc+=2;
                goto aggFlagC;
                break;
              case ADDRREGADD:   // An
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    break;
                  case WORD_SIZE:
                    res3.x = (WORKING_REG_A.w.l -= res2.b.l);
                    break;
                  case DWORD_SIZE:
                    res3.d = (WORKING_REG_A.x -= res2.b.l);
                    break;
                  }// NO flag!
                _pc+=2;
                break;
              case ADDRADD:   // (An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res3.b.l = GetValue(WORKING_REG_A.x) - res2.b.l;
                    PutValue(WORKING_REG_A.x,res3.b.l);
                    break;
                  case WORD_SIZE:
                    res3.x = GetShortValue(WORKING_REG_A.x) - res2.b.l;
                    PutShortValue(WORKING_REG_A.x,res3.x);
                    break;
                  case DWORD_SIZE:
                    res3.d = GetIntValue(WORKING_REG_A.x) - res2.b.l;
                    PutIntValue(WORKING_REG_A.x,res3.d);
                    break;
                  }
                _pc+=2;
                goto aggFlagC;
                break;
              case ADDRPINCADD:   // (An)+
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res3.b.l = GetValue(WORKING_REG_A.x) - res2.b.l;
                    PutValue(WORKING_REG_A.x,res3.b.l);
                    WORKING_REG_A.x++;
                    break;
                  case WORD_SIZE:
                    res3.x = GetShortValue(WORKING_REG_A.x) - res2.b.l;
                    PutShortValue(WORKING_REG_A.x,res3.x);
                    WORKING_REG_A.x+=2;
                    break;
                  case DWORD_SIZE:
                    res3.d = GetIntValue(WORKING_REG_A.x) - res2.b.l;
                    PutIntValue(WORKING_REG_A.x,res3.d);
                    WORKING_REG_A.x+=4;
                    break;
                  }
                _pc+=2;
                goto aggFlagC;
                break;
              case ADDRPDECADD:   // -(An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    WORKING_REG_A.x--;
                    res3.b.l = GetValue(WORKING_REG_A.x) - res2.b.l;
                    PutValue(WORKING_REG_A.x,res3.b.l);
                    break;
                  case WORD_SIZE:
                    WORKING_REG_A.x-=2;
                    res3.x = GetShortValue(WORKING_REG_A.x) - res2.b.l;
                    PutShortValue(WORKING_REG_A.x,res3.x);
                    break;
                  case DWORD_SIZE:
                    WORKING_REG_A.x-=4;
                    res3.d = GetIntValue(WORKING_REG_A.x) - res2.b.l;
                    PutIntValue(WORKING_REG_A.x,res3.d);
                    break;
                  }
                _pc+=2;
                goto aggFlagC;
                break;
              case ADDRDISPADD:   // (D16,An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res3.b.l = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) - Pipe2.b.l;
                    PutValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h, res3.b.l);
                    _pc+=6;
                    break;
                  case WORD_SIZE:
                    res3.x = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) - Pipe2.x.l;
                    PutValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h, res3.x);
                    _pc+=6;
                    break;
                  case DWORD_SIZE:
                    res3.d = GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) - Pipe2.d;
                    PutIntValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h, res3.d);
                    _pc+=8;
                    break;
                  }
                break;
              case ADDRIDXADD:   // (D8,An,Xn)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    if(!DISPLACEMENT_SIZE) {
                      res3.b.l = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) - Pipe2.b.l;
                      PutValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l, res3.b.l);
                      }
                    else {
                      res3.b.l = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) - Pipe2.b.l;
                      PutValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l, res3.b.l);
                      }
                    _pc+=6;
                    break;
                  case WORD_SIZE:
                    if(!DISPLACEMENT_SIZE) {
                      res3.x = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) - Pipe2.x.l;
                      PutValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l, res3.x);
                      }
                    else {
                      res3.x = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) - Pipe2.x.l;
                      PutValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l, res3.x);
                      }
                    _pc+=6;
                    break;
                  case DWORD_SIZE:
                    if(!DISPLACEMENT_SIZE) {
                      res3.d = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) - Pipe2.d;
                      PutValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l, res3.d);
                      }
                    else {
                      res3.d = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) - Pipe2.d;
                      PutValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l, res3.d);
                      }
                    _pc+=8;
                    break;
                  }
                break;
              }
            break;
          }
				break;
        
			case 0x60:   // BRA
do_bra:
        if(LOBYTE(Pipe1))
          _pc+=LOBYTE(Pipe1)+2;
        else
          _pc+=Pipe2.x.l+4;
				break;
        
			case 0x61:   // BSR
        if(_f.SR.Supervisor) 
          _ssp-=4;
        else
          _sp-=4;
        if(LOBYTE(Pipe1)) {
          PutIntValue(_f.SR.Supervisor ? _ssp : _sp,_pc+2);  // low word prima
          _pc+=LOBYTE(Pipe1)+2;
          }
        else {
          PutIntValue(_f.SR.Supervisor ? _ssp : _sp,_pc+4);  // low word prima
          _pc+=Pipe2.x.l+4;
          }
				break;
        
			case 0x62:   // Bc
        if(!_f.CCR.Carry && !_f.CCR.Zero)     // higher !C !Z
          goto do_bra;
				break;
			case 0x63:
        if(_f.CCR.Zero || (_f.CCR.Sign ^ _f.CCR.Ovf))     // lower or equal
          goto do_bra;
				break;
			case 0x64:
        if(!_f.CCR.Carry)     // carry clear
          goto do_bra;
				break;
			case 0x65:
        if(_f.CCR.Carry)      // carry set
          goto do_bra;
				break;
			case 0x66:
        if(!_f.CCR.Zero)      // not equal
          goto do_bra;
				break;
			case 0x67:
        if(_f.CCR.Zero)      // equal
          goto do_bra;
				break;
			case 0x68:
        if(!_f.CCR.Ovf)      // overflow clear
          goto do_bra;
				break;
			case 0x69:
        if(_f.CCR.Ovf)       // overflow set
          goto do_bra;
				break;
			case 0x6a:
        if(!_f.CCR.Sign)     // plus
          goto do_bra;
				break;
			case 0x6b:
        if(_f.CCR.Sign)      // minus
          goto do_bra;
				break;
			case 0x6c:
        if(!(_f.CCR.Sign ^ _f.CCR.Ovf))     // greater or equal
          goto do_bra;
				break;
			case 0x6d:
        if(_f.CCR.Sign ^ _f.CCR.Ovf)     // less than
          goto do_bra;
				break;
			case 0x6e:
        if(_f.CCR.Zero && !(_f.CCR.Sign ^ _f.CCR.Ovf))     // greater than
          goto do_bra;
				break;
			case 0x6f:
        if(_f.CCR.Carry || _f.CCR.Zero)     // less or equal
          goto do_bra;
				break;
        /*
        switch(Pipe1 & 0xf) {
    			case 0x2:
            break;
    			case 0x3:
            break;
    			case 0x4:
            break;
    			case 0x5:
            break;
    			case 0x6:
            break;
    			case 0x7:
            break;
    			case 0x8:
            break;
    			case 0x9:
            break;
    			case 0xa:
            break;
    			case 0xb:
            break;
    			case 0xc:
            break;
    			case 0xd:
            break;
    			case 0xe:
            break;
    			case 0xf:
            break;
          }*/
        
			case 0x70:   // MOVEQ
			case 0x72:
			case 0x74:
			case 0x76:
			case 0x78:
			case 0x7a:
			case 0x7c:
			case 0x7e:
        DEST_REG_D.b.b0 = LOBYTE(Pipe1);
				break;
        
			case 0x80:   // DIVU OR
			case 0x82:
			case 0x84:
			case 0x86:
			case 0x88:
			case 0x8a:
			case 0x8c:
			case 0x8e:
//        DEST_REG_D.b.b0 = LOBYTE(Pipe1);
        switch(ADDRESSING) {
          case ABSOLUTELADD:
            break;
          case DATAREGADD:   // Dn
            switch(OPERAND_SIZE) {
              case 0xc0:
                DEST_REG_D.x = WORKING_REG_D.x / Pipe2.x.l;
                break;
              default:
                break;
              }
            break;
          case ADDRREGADD:   // An
            switch(OPERAND_SIZE) {
              case 0xc0:
                DEST_REG_D.x = WORKING_REG_A.x / Pipe2.x.l;
                break;
              default:
                break;
              }
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
              case 0xc0:
                DEST_REG_D.x = GetIntValue(WORKING_REG_A.x) / Pipe2.x.l;
                break;
              default:
                break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case 0xc0:
                DEST_REG_D.x = GetIntValue(WORKING_REG_A.x) / Pipe2.x.l;
                WORKING_REG_A.x+=4;
                break;
              default:
                break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case 0xc0:
                WORKING_REG_A.x-=4;
                DEST_REG_D.x = GetIntValue(WORKING_REG_A.x) / Pipe2.x.l;
                break;
              default:
                break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
              case 0xc0:
                DEST_REG_D.x = GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) / Pipe2.x.l;
                break;
              default:
                break;
              }
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
              case 0xc0:
                if(!DISPLACEMENT_SIZE)
                  DEST_REG_D.x = GetIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) / Pipe2.x.l;
                else
                  DEST_REG_D.x = GetIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) / Pipe2.x.l;
                break;
              default:
                break;
              }
            break;
          }
				break;
        
			case 0x81:   // DIVS SBCD OR
			case 0x83:
			case 0x85:
			case 0x87:
			case 0x89:
			case 0x8b:
			case 0x8d:
			case 0x8f:
  //      DEST_REG_D.b.b0 = LOBYTE(Pipe1);
#warning finire DIVU DIVS
        switch(ADDRESSING) {
          case ABSOLUTELADD:
            break;
          case DATAREGADD:   // Dn
            switch(OPERAND_SIZE) {
              case 0xc0:
                DEST_REG_D.x = WORKING_REG_D.x / Pipe2.x.l;
                break;
              default:
                break;
              }
            break;
          case ADDRREGADD:   // An
            switch(OPERAND_SIZE) {
              case 0xc0:
                DEST_REG_D.x = WORKING_REG_A.x / Pipe2.x.l;
                break;
              default:
                break;
              }
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
              case 0xc0:
                DEST_REG_D.x = GetIntValue(WORKING_REG_A.x) / Pipe2.x.l;
                break;
              default:
                break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case 0xc0:
                DEST_REG_D.x = GetIntValue(WORKING_REG_A.x) / Pipe2.x.l;
                WORKING_REG_A.x+=4;
                break;
              default:
                break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case 0xc0:
                WORKING_REG_A.x-=4;
                DEST_REG_D.x = GetIntValue(WORKING_REG_A.x) / Pipe2.x.l;
                break;
              default:
                break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
              case 0xc0:
                DEST_REG_D.x = GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) / Pipe2.x.l;
                break;
              default:
                break;
              }
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
              case 0xc0:
                if(!DISPLACEMENT_SIZE)
                  DEST_REG_D.x = GetIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) / Pipe2.x.l;
                else
                  DEST_REG_D.x = GetIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) / Pipe2.x.l;
                break;
              }
            break;
          }
				break;
        
			case 0x90:   // SUB SUBX SUBA
			case 0x91:
			case 0x92:
			case 0x93:
			case 0x94:
			case 0x95:
			case 0x96:
			case 0x97:
			case 0x98:
			case 0x99:
			case 0x9a:
			case 0x9b:
			case 0x9c:
			case 0x9d:
			case 0x9e:
			case 0x9f:
        switch(ADDRESSING) {
          case ABSOLUTELADD:
            break;
          case DATAREGADD:   // Dn
            switch(OPERAND_SIZE) {
                if(Pipe1 & 0b0000000100000000)  // direction... FARE
                  ;
              case BYTE_SIZE:
                DEST_REG_D.b.b0 = WORKING_REG_D.b.b0 - Pipe2.b.l;
                break;
              case WORD_SIZE:
                DEST_REG_D.w.l = WORKING_REG_D.w.l - Pipe2.x.l;
                break;
              case DWORD_SIZE:
                DEST_REG_D.x = WORKING_REG_D.x - Pipe2.d;
                break;
              case 0xc0:
                break;
              }
            break;
          case ADDRREGADD:   // An
            switch(OPERAND_SIZE) {
                if(Pipe1 & 0b0000000100000000)  // direction... FARE
                  ;
              case BYTE_SIZE:
                DEST_REG_D.b.b0 = WORKING_REG_A.b.b0 - Pipe2.b.l;
                break;
              case WORD_SIZE:
                DEST_REG_D.w.l = WORKING_REG_A.w.l - Pipe2.x.l;
                break;
              case DWORD_SIZE:
                DEST_REG_D.x = WORKING_REG_A.x - Pipe2.d;
                break;
              case 0xc0:
                break;
              }
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
                if(Pipe1 & 0b0000000100000000)  // direction... FARE
                  ;
              case BYTE_SIZE:
                DEST_REG_D.b.b0 = GetValue(WORKING_REG_A.x) - Pipe2.b.l;
                break;
              case WORD_SIZE:
                DEST_REG_D.w.l = GetShortValue(WORKING_REG_A.x) - Pipe2.x.l;
                break;
              case DWORD_SIZE:
                DEST_REG_D.x = GetValue(WORKING_REG_A.x) - Pipe2.d;
                break;
              case 0xc0:
                break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
                if(Pipe1 & 0b0000000100000000)  // direction... FARE
                  ;
              case BYTE_SIZE:
                DEST_REG_D.b.b0 = GetValue(WORKING_REG_A.x) - Pipe2.b.l;
                WORKING_REG_A.x++;
                break;
              case WORD_SIZE:
                DEST_REG_D.w.l = GetShortValue(WORKING_REG_A.x) - Pipe2.x.l;
                WORKING_REG_A.x+=2;
                break;
              case DWORD_SIZE:
                DEST_REG_D.x = GetValue(WORKING_REG_A.x) - Pipe2.d;
                WORKING_REG_A.x+=4;
                break;
              case 0xc0:
                break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
                if(Pipe1 & 0b0000000100000000)  // direction... FARE
                  ;
              case BYTE_SIZE:
                WORKING_REG_A.x--;
                DEST_REG_D.b.b0 = GetValue(WORKING_REG_A.x) - Pipe2.b.l;
                break;
              case WORD_SIZE:
                WORKING_REG_A.x-=2;
                DEST_REG_D.w.l = GetShortValue(WORKING_REG_A.x) - Pipe2.x.l;
                break;
              case DWORD_SIZE:
                WORKING_REG_A.x-=4;
                DEST_REG_D.x = GetValue(WORKING_REG_A.x) - Pipe2.d;
                break;
              case 0xc0:
                break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
                if(Pipe1 & 0b0000000100000000)  // direction... FARE
                  ;
              case BYTE_SIZE:
                DEST_REG_D.b.b0 = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) - Pipe2.b.l;
                break;
              case WORD_SIZE:
                DEST_REG_D.w.l = GetShortValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) - Pipe2.x.l;
                break;
              case DWORD_SIZE:
                DEST_REG_D.x = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) - Pipe2.d;
                break;
              case 0xc0:
                break;
              }
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
                if(Pipe1 & 0b0000000100000000)  // direction... FARE
                  ;
              case BYTE_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  DEST_REG_D.b.b0 = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) - Pipe2.b.l;
                  }
                else {
                  DEST_REG_D.b.b0 = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) - Pipe2.b.l;
                  }
                break;
              case WORD_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  DEST_REG_D.w.l = GetShortValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) - Pipe2.x.l;
                  }
                else {
                  DEST_REG_D.w.l = GetShortValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) - Pipe2.x.l;
                  }
                break;
              case DWORD_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  DEST_REG_D.x = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) - Pipe2.d;
                  }
                else {
                  DEST_REG_D.x = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) - Pipe2.d;
                  }
                break;
              case 0xc0:
                break;
              }
            break;
          }
				break;
        
			case 0xb1:   // EOR CMPM
			case 0xb3:
			case 0xb5:
			case 0xb7:
			case 0xb9:
			case 0xbb:
			case 0xbd:
			case 0xbf:
        switch(ADDRESSING) {
          case ABSOLUTELADD:
            break;
          case DATAREGADD:   // Dn
            if(Pipe1 & 0b0000000100000000) {
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  DEST_REG_D.b.b0 = WORKING_REG_D.b.b0 ^ Pipe2.b.l;
                  break;
                case WORD_SIZE:
                  DEST_REG_D.w.l = WORKING_REG_D.w.l ^ Pipe2.x.l;
                  break;
                case DWORD_SIZE:
                  DEST_REG_D.x = WORKING_REG_D.x ^ Pipe2.d;
                  break;
                case 0xc0:
                  break;
                }
              }
            else {
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  DEST_REG_D.b.b0 = WORKING_REG_D.b.b0 - Pipe2.b.l;
                  break;
                case WORD_SIZE:
                  DEST_REG_D.w.l = WORKING_REG_D.w.l - Pipe2.x.l;
                  break;
                case DWORD_SIZE:
                  DEST_REG_D.x = WORKING_REG_D.x - Pipe2.d;
                  break;
                case 0xc0:
                  break;
                }
              }
            break;
          case ADDRREGADD:   // An
            if(Pipe1 & 0b0000000100000000) {
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  DEST_REG_D.b.b0 = WORKING_REG_A.b.b0 ^ Pipe2.b.l;
                  break;
                case WORD_SIZE:
                  DEST_REG_D.w.l = WORKING_REG_A.w.l ^ Pipe2.x.l;
                  break;
                case DWORD_SIZE:
                  DEST_REG_D.x = WORKING_REG_A.x ^ Pipe2.d;
                  break;
                case 0xc0:
                  break;
                }
              }
            else {
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  DEST_REG_D.b.b0 = WORKING_REG_A.b.b0 - Pipe2.b.l;
                  break;
                case WORD_SIZE:
                  DEST_REG_D.w.l = WORKING_REG_A.w.l - Pipe2.x.l;
                  break;
                case DWORD_SIZE:
                  DEST_REG_D.x = WORKING_REG_A.x - Pipe2.d;
                  break;
                case 0xc0:
                  break;
                }
              }
            break;
          case ADDRADD:   // (An)
            if(Pipe1 & 0b0000000100000000) {
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  DEST_REG_D.b.b0 = GetValue(WORKING_REG_A.x) ^ Pipe2.b.l;
                  break;
                case WORD_SIZE:
                  DEST_REG_D.w.l = GetShortValue(WORKING_REG_A.x) ^ Pipe2.x.l;
                  break;
                case DWORD_SIZE:
                  DEST_REG_D.x = GetIntValue(WORKING_REG_A.x) ^ Pipe2.d;
                  break;
                case 0xc0:
                  break;
                }
              }
            else {
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  DEST_REG_D.b.b0 = GetValue(WORKING_REG_A.x) - Pipe2.b.l;
                  break;
                case WORD_SIZE:
                  DEST_REG_D.w.l = GetShortValue(WORKING_REG_A.x) - Pipe2.x.l;
                  break;
                case DWORD_SIZE:
                  DEST_REG_D.x = GetIntValue(WORKING_REG_A.x) - Pipe2.d;
                  break;
                case 0xc0:
                  break;
                }
              }
            break;
          case ADDRPINCADD:   // (An)+
            if(Pipe1 & 0b0000000100000000) {
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  DEST_REG_D.b.b0 = GetValue(WORKING_REG_A.x) ^ Pipe2.b.l;
                  WORKING_REG_A.x++;
                  break;
                case WORD_SIZE:
                  DEST_REG_D.w.l = GetShortValue(WORKING_REG_A.x) ^ Pipe2.x.l;
                  WORKING_REG_A.x+=2;
                  break;
                case DWORD_SIZE:
                  DEST_REG_D.x = GetIntValue(WORKING_REG_A.x) ^ Pipe2.d;
                  WORKING_REG_A.x+=4;
                  break;
                case 0xc0:
                  break;
                }
              }
            else {
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  DEST_REG_D.b.b0 = GetValue(WORKING_REG_A.x) - Pipe2.b.l;
                  WORKING_REG_A.x++;
                  break;
                case WORD_SIZE:
                  DEST_REG_D.w.l = GetShortValue(WORKING_REG_A.x) - Pipe2.x.l;
                  WORKING_REG_A.x+=2;
                  break;
                case DWORD_SIZE:
                  DEST_REG_D.x = GetIntValue(WORKING_REG_A.x) - Pipe2.d;
                  WORKING_REG_A.x+=4;
                  break;
                case 0xc0:
                  break;
                }
              }
            break;
          case ADDRPDECADD:   // -(An)
            if(Pipe1 & 0b0000000100000000) {
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  WORKING_REG_A.x--;
                  DEST_REG_D.b.b0 = GetValue(WORKING_REG_A.x) ^ Pipe2.b.l;
                  break;
                case WORD_SIZE:
                  WORKING_REG_A.x-=2;
                  DEST_REG_D.w.l = GetShortValue(WORKING_REG_A.x) ^ Pipe2.x.l;
                  break;
                case DWORD_SIZE:
                  WORKING_REG_A.x-=4;
                  DEST_REG_D.x = GetIntValue(WORKING_REG_A.x) ^ Pipe2.d;
                  break;
                case 0xc0:
                  break;
                }
              }
            else {
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  WORKING_REG_A.x--;
                  DEST_REG_D.b.b0 = GetValue(WORKING_REG_A.x) - Pipe2.b.l;
                  break;
                case WORD_SIZE:
                  WORKING_REG_A.x-=2;
                  DEST_REG_D.w.l = GetShortValue(WORKING_REG_A.x) - Pipe2.x.l;
                  break;
                case DWORD_SIZE:
                  WORKING_REG_A.x-=4;
                  DEST_REG_D.x = GetIntValue(WORKING_REG_A.x) - Pipe2.d;
                  break;
                case 0xc0:
                  break;
                }
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            if(Pipe1 & 0b0000000100000000) {
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  DEST_REG_D.b.b0 = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) ^ Pipe2.b.l;
                  break;
                case WORD_SIZE:
                  DEST_REG_D.w.l = GetShortValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) ^ Pipe2.x.l;
                  break;
                case DWORD_SIZE:
                  DEST_REG_D.x = GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) ^ Pipe2.d;
                  break;
                case 0xc0:
                  break;
                }
              }
            else {
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  DEST_REG_D.b.b0 = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) - Pipe2.b.l;
                  break;
                case WORD_SIZE:
                  DEST_REG_D.w.l = GetShortValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) - Pipe2.x.l;
                  break;
                case DWORD_SIZE:
                  DEST_REG_D.x = GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) - Pipe2.d;
                  break;
                case 0xc0:
                  break;
                }
              }
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            if(Pipe1 & 0b0000000100000000) {
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  if(!DISPLACEMENT_SIZE) {
                    DEST_REG_D.b.b0 = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) ^ Pipe2.b.l;
                    }
                  else {
                    DEST_REG_D.b.b0 = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) ^ Pipe2.b.l;
                    }
                  break;
                case WORD_SIZE:
                  if(!DISPLACEMENT_SIZE) {
                    DEST_REG_D.w.l = GetShortValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) ^ Pipe2.x.l;
                    }
                  else {
                    DEST_REG_D.w.l = GetShortValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) ^ Pipe2.x.l;
                    }
                  break;
                case DWORD_SIZE:
                  if(!DISPLACEMENT_SIZE) {
                    DEST_REG_D.x = GetIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) ^ Pipe2.d;
                    }
                  else {
                    DEST_REG_D.x = GetIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) ^ Pipe2.d;
                    }
                  break;
                case 0xc0:
                  break;
                }
              }
            else {
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  if(!DISPLACEMENT_SIZE) {
                    DEST_REG_D.b.b0 = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) - Pipe2.b.l;
                    }
                  else {
                    DEST_REG_D.b.b0 = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) - Pipe2.b.l;
                    }
                  break;
                case WORD_SIZE:
                  if(!DISPLACEMENT_SIZE) {
                    DEST_REG_D.w.l = GetShortValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) - Pipe2.x.l;
                    }
                  else {
                    DEST_REG_D.w.l = GetShortValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) - Pipe2.x.l;
                    }
                  break;
                case DWORD_SIZE:
                  if(!DISPLACEMENT_SIZE) {
                    DEST_REG_D.x = GetIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) - Pipe2.d;
                    }
                  else {
                    DEST_REG_D.x = GetIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) - Pipe2.d;
                    }
                  break;
                }
              }
            break;
          }
				break;
        
			case 0xb0:   // CMP
			case 0xb2:
			case 0xb4:
			case 0xb6:
			case 0xb8:
			case 0xba:
			case 0xbc:
			case 0xbe:
        switch(ADDRESSING) {
          case ABSOLUTELADD:
            break;
          case DATAREGADD:   // Dn
            if(Pipe1 & 0b0000000100000000) {
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  DEST_REG_D.b.b0 - WORKING_REG_D.b.b0;
                  break;
                case WORD_SIZE:
                  DEST_REG_D.w.l - WORKING_REG_D.w.l;
                  break;
                case DWORD_SIZE:
                  DEST_REG_D.x - WORKING_REG_D.x;
                  break;
                case 0xc0:
#warning verificare c0
                  DEST_REG_D.x - WORKING_REG_D.x;
                  break;
                }
              }
            else {
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  regsA.r[(Pipe1 >> 9) & 7].b.b0 - WORKING_REG_D.b.b0;
                  break;
                case WORD_SIZE:
                  regsA.r[(Pipe1 >> 9) & 7].w.l - WORKING_REG_D.w.l;
                  break;
                case DWORD_SIZE:
                  regsA.r[(Pipe1 >> 9) & 7].x - WORKING_REG_D.x;
                  break;
                case 0xc0:
                  regsA.r[(Pipe1 >> 9) & 7].w.l - WORKING_REG_D.w.l;
                  break;
                }
              }
            break;
          case ADDRREGADD:   // An
            if(Pipe1 & 0b0000000100000000) {
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  DEST_REG_D.b.b0 - WORKING_REG_A.b.b0;
                  break;
                case WORD_SIZE:
                  DEST_REG_D.w.l - WORKING_REG_A.w.l;
                  break;
                case DWORD_SIZE:
                  DEST_REG_D.x - WORKING_REG_A.x;
                  break;
                case 0xc0:
#warning verificare c0
                  DEST_REG_D.x - WORKING_REG_A.x;
                  break;
                }
              }
            else {
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  regsA.r[(Pipe1 >> 9) & 7].b.b0 - WORKING_REG_A.b.b0;
                  break;
                case WORD_SIZE:
                  regsA.r[(Pipe1 >> 9) & 7].w.l - WORKING_REG_A.w.l;
                  break;
                case DWORD_SIZE:
                  regsA.r[(Pipe1 >> 9) & 7].x - WORKING_REG_A.x;
                  break;
                case 0xc0:
                  regsA.r[(Pipe1 >> 9) & 7].w.l - WORKING_REG_A.w.l;
                  break;
                }
              }
            break;
          case ADDRADD:   // (An)
            if(Pipe1 & 0b0000000100000000) {
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  DEST_REG_D.b.b0 - GetValue(WORKING_REG_A.x);
                  break;
                case WORD_SIZE:
                  DEST_REG_D.w.l - GetShortValue(WORKING_REG_A.x);
                  break;
                case DWORD_SIZE:
                  DEST_REG_D.x - GetIntValue(WORKING_REG_A.x);
                  break;
                case 0xc0:
#warning verificare c0
                  DEST_REG_D.x - GetIntValue(WORKING_REG_A.x);
                  break;
                }
              }
            else {
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  regsA.r[(Pipe1 >> 9) & 7].b.b0 - GetValue(WORKING_REG_A.x);
                  break;
                case WORD_SIZE:
                  regsA.r[(Pipe1 >> 9) & 7].w.l - GetShortValue(WORKING_REG_A.x);
                  break;
                case DWORD_SIZE:
                  regsA.r[(Pipe1 >> 9) & 7].x - GetIntValue(WORKING_REG_A.x);
                  break;
                case 0xc0:
                  regsA.r[(Pipe1 >> 9) & 7].w.l - GetIntValue(WORKING_REG_A.x);
                  break;
                }
              }
            break;
          case ADDRPINCADD:   // (An)+
            if(Pipe1 & 0b0000000100000000) {
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  DEST_REG_D.b.b0 - GetValue(WORKING_REG_A.x);
                  WORKING_REG_A.x++;
                  break;
                case WORD_SIZE:
                  DEST_REG_D.w.l - GetShortValue(WORKING_REG_A.x);
                  WORKING_REG_A.x+=2;
                  break;
                case DWORD_SIZE:
                  DEST_REG_D.x - GetIntValue(WORKING_REG_A.x);
                  WORKING_REG_A.x+=4;
                  break;
                case 0xc0:
#warning verificare c0
                  DEST_REG_D.x - GetIntValue(WORKING_REG_A.x);
                  WORKING_REG_A.x+=4;
                  break;
                }
              }
            else {
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  regsA.r[(Pipe1 >> 9) & 7].b.b0 - GetValue(WORKING_REG_A.x);
                  WORKING_REG_A.x++;
                  break;
                case WORD_SIZE:
                  regsA.r[(Pipe1 >> 9) & 7].w.l - GetShortValue(WORKING_REG_A.x);
                  WORKING_REG_A.x+=2;
                  break;
                case DWORD_SIZE:
                  regsA.r[(Pipe1 >> 9) & 7].x - GetIntValue(WORKING_REG_A.x);
                  WORKING_REG_A.x+=4;
                  break;
                case 0xc0:
                  regsA.r[(Pipe1 >> 9) & 7].w.l - GetIntValue(WORKING_REG_A.x);
                  WORKING_REG_A.x+=4;
                  break;
                }
              }
            break;
          case ADDRPDECADD:   // -(An)
            if(Pipe1 & 0b0000000100000000) {
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  WORKING_REG_A.x--;
                  DEST_REG_D.b.b0 - GetValue(WORKING_REG_A.x);
                  break;
                case WORD_SIZE:
                  WORKING_REG_A.x-=2;
                  DEST_REG_D.w.l - GetShortValue(WORKING_REG_A.x);
                  break;
                case DWORD_SIZE:
                  WORKING_REG_A.x-=4;
                  DEST_REG_D.x - GetIntValue(WORKING_REG_A.x);
                  break;
                case 0xc0:
#warning verificare c0
                  WORKING_REG_A.x-=4;
                  DEST_REG_D.x - GetIntValue(WORKING_REG_A.x);
                  break;
                }
              }
            else {
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  WORKING_REG_A.x--;
                  regsA.r[(Pipe1 >> 9) & 7].b.b0 - GetValue(WORKING_REG_A.x);
                  break;
                case WORD_SIZE:
                  WORKING_REG_A.x-=2;
                  regsA.r[(Pipe1 >> 9) & 7].w.l - GetShortValue(WORKING_REG_A.x);
                  break;
                case DWORD_SIZE:
                  WORKING_REG_A.x-=4;
                  regsA.r[(Pipe1 >> 9) & 7].x - GetIntValue(WORKING_REG_A.x);
                  break;
                case 0xc0:
                  WORKING_REG_A.x-=4;
                  regsA.r[(Pipe1 >> 9) & 7].w.l - GetIntValue(WORKING_REG_A.x);
                  break;
                }
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            if(Pipe1 & 0b0000000100000000) {
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  DEST_REG_D.b.b0 - GetValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h);
                  break;
                case WORD_SIZE:
                  DEST_REG_D.w.l - GetShortValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h);
                  break;
                case DWORD_SIZE:
                  DEST_REG_D.x - GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h);
                  break;
                case 0xc0:
#warning verificare c0
                  DEST_REG_D.x - GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h);
                  break;
                }
              }
            else {
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  WORKING_REG_A.x--;
                  regsA.r[(Pipe1 >> 9) & 7].b.b0 - GetValue(WORKING_REG_A.x);
                  break;
                case WORD_SIZE:
                  WORKING_REG_A.x-=2;
                  regsA.r[(Pipe1 >> 9) & 7].w.l - GetShortValue(WORKING_REG_A.x);
                  break;
                case DWORD_SIZE:
                  WORKING_REG_A.x-=4;
                  regsA.r[(Pipe1 >> 9) & 7].x - GetIntValue(WORKING_REG_A.x);
                  break;
                case 0xc0:
                  WORKING_REG_A.x-=4;
                  regsA.r[(Pipe1 >> 9) & 7].w.l - GetIntValue(WORKING_REG_A.x);
                  break;
                }
              }
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            if(Pipe1 & 0b0000000100000000) {
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  if(!DISPLACEMENT_SIZE) {
                    DEST_REG_D.b.b0 - GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l);
                    }
                  else {
                    DEST_REG_D.b.b0 - GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l);
                    }
                  break;
                case WORD_SIZE:
                  if(!DISPLACEMENT_SIZE) {
                    DEST_REG_D.w.l - GetShortValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l);
                    }
                  else {
                    DEST_REG_D.w.l - GetShortValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l);
                    }
                  break;
                case DWORD_SIZE:
                  if(!DISPLACEMENT_SIZE) {
                    DEST_REG_D.x - GetIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l);
                    }
                  else {
                    DEST_REG_D.x - GetIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l);
                    }
                  break;
                case 0xc0:
#warning verificare c0
                  if(!DISPLACEMENT_SIZE) {
                    DEST_REG_D.x - GetIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) ^ Pipe2.b.l;
                    }
                  else {
                    DEST_REG_D.x - GetIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) ^ Pipe2.b.l;
                    }
                  break;
                }
              }
            else {
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  if(!DISPLACEMENT_SIZE) {
                    regsA.r[(Pipe1 >> 9) & 7].b.b0 - GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l);
                    }
                  else {
                    regsA.r[(Pipe1 >> 9) & 7].b.b0 - GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l);
                    }
                  break;
                case WORD_SIZE:
                  if(!DISPLACEMENT_SIZE) {
                    regsA.r[(Pipe1 >> 9) & 7].w.l - GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l);
                    }
                  else {
                    regsA.r[(Pipe1 >> 9) & 7].w.l - GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l);
                    }
                  break;
                case DWORD_SIZE:
                  if(!DISPLACEMENT_SIZE) {
                    regsA.r[(Pipe1 >> 9) & 7].x - GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l);
                    }
                  else {
                    regsA.r[(Pipe1 >> 9) & 7].x - GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l);
                    }
                  break;
                case 0xc0:
                  if(!DISPLACEMENT_SIZE) {
                    regsA.r[(Pipe1 >> 9) & 7].w.l - GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l);
                    }
                  else {
                    regsA.r[(Pipe1 >> 9) & 7].w.l - GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l);
                    }
                  break;
                }
              }
            break;
          }
				break;
        
			case 0xc0:   // MULU AND
			case 0xc2:
			case 0xc4:
			case 0xc6:
			case 0xc8:
			case 0xca:
			case 0xcc:
			case 0xce:
        switch(ADDRESSING) {
          case ABSOLUTELADD:
            break;
          case DATAREGADD:   // Dn
            switch(OPERAND_SIZE) {
              case 0xc0:
                DEST_REG_D.x = WORKING_REG_D.x * Pipe2.x.l;
                break;
              default:
                break;
              }
            break;
          case ADDRREGADD:   // An
            switch(OPERAND_SIZE) {
              case 0xc0:
                DEST_REG_D.x = WORKING_REG_A.x * Pipe2.x.l;
                break;
              default:
                break;
              }
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
              case 0xc0:
                DEST_REG_D.x = GetIntValue(WORKING_REG_A.x) * Pipe2.x.l;
                break;
              default:
                break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case 0xc0:
                DEST_REG_D.x = GetIntValue(WORKING_REG_A.x) * Pipe2.x.l;
                WORKING_REG_A.x+=4;
                break;
              default:
                break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case 0xc0:
                WORKING_REG_A.x-=4;
                DEST_REG_D.x = GetIntValue(WORKING_REG_A.x) * Pipe2.x.l;
                break;
              default:
                break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
              case 0xc0:
                DEST_REG_D.x = GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) * Pipe2.x.l;
                break;
              default:
                break;
              }
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
              case 0xc0:
                if(!DISPLACEMENT_SIZE) {
                  DEST_REG_D.x = GetIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) * Pipe2.x.l;
                  }
                else {
                  DEST_REG_D.x = GetIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) * Pipe2.x.l;
                  }
                break;
              default:
                break;
              }
            break;
          }
				break;
        
			case 0xc1:   // MULS ABCD EXG AND
			case 0xc3:
			case 0xc5:
			case 0xc7:
			case 0xc9:
			case 0xcb:
			case 0xcd:
			case 0xcf:
#warning finire MULU MULS
        switch(ADDRESSING) {
          case ABSOLUTELADD:
            break;
          case DATAREGADD:   // Dn
            switch(OPERAND_SIZE) {
              case 0xc0:
                DEST_REG_D.x = WORKING_REG_D.x * Pipe2.x.l;
                break;
              default:
                break;
              }
            break;
          case ADDRREGADD:   // An
            switch(OPERAND_SIZE) {
              case 0xc0:
                DEST_REG_D.x = WORKING_REG_A.x * Pipe2.x.l;
                break;
              default:
                break;
              }
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
              case 0xc0:
                DEST_REG_D.x = GetIntValue(WORKING_REG_A.x) * Pipe2.x.l;
                break;
              default:
                break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case 0xc0:
                DEST_REG_D.x = GetIntValue(WORKING_REG_A.x) * Pipe2.x.l;
                WORKING_REG_A.x+=4;
                break;
              default:
                break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case 0xc0:
                WORKING_REG_A.x-=4;
                DEST_REG_D.x = GetIntValue(WORKING_REG_A.x) * Pipe2.x.l;
                break;
              default:
                break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
              case 0xc0:
                DEST_REG_D.x = GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.x.h) * Pipe2.x.l;
                break;
              default:
                break;
              }
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
              case 0xc0:
                if(!DISPLACEMENT_SIZE) {
                  DEST_REG_D.x = GetIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l) * Pipe2.x.l;
                  }
                else {
                  DEST_REG_D.x = GetIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l) * Pipe2.x.l;
                  }
                break;
              default:
                break;
              }
            break;
          }
				break;
        
			case 0xd0:   // ADD ADDX ADDA
			case 0xd1:
			case 0xd2:
			case 0xd3:
			case 0xd4:
			case 0xd5:
			case 0xd6:
			case 0xd7:
			case 0xd8:
			case 0xd9:
			case 0xda:
			case 0xdb:
			case 0xdc:
			case 0xdd:
			case 0xde:
			case 0xdf:
        switch(ADDRESSING) {
          case IMMEDIATEADD:    // #imm
            switch(OPERAND_SIZE) {
                if(Pipe1 & 0b0000000100000000)  // direction... FARE
                  ;
              case BYTE_SIZE:
                DEST_REG_D.b.b0 += Pipe2.b.l;
                break;
              case WORD_SIZE:
                DEST_REG_D.w.l += Pipe2.x.l;
                break;
              case DWORD_SIZE:
                DEST_REG_D.x += Pipe2.d;
                break;
              case 0xc0:
#warning finire word long
                regsA.r[(Pipe1 >> 9) & 7].x = WORKING_REG_D.x + Pipe2.d;
                break;
              }
            break;
          case DATAREGADD:   // Dn
            switch(OPERAND_SIZE) {
                if(Pipe1 & 0b0000000100000000)  // direction... FARE
                  ;
              case BYTE_SIZE:
                DEST_REG_D.b.b0 += WORKING_REG_D.b.b0;
                break;
              case WORD_SIZE:
                DEST_REG_D.w.l += WORKING_REG_D.w.l;
                break;
              case DWORD_SIZE:
                DEST_REG_D.x += WORKING_REG_D.x;
                break;
              case 0xc0:
#warning finire word long
                regsA.r[(Pipe1 >> 9) & 7].x = WORKING_REG_D.x + Pipe2.d;
                break;
              }
            break;
          case ADDRREGADD:   // An
            switch(OPERAND_SIZE) {
                if(Pipe1 & 0b0000000100000000)  // direction... FARE
                  ;
              case BYTE_SIZE:
                DEST_REG_D.b.b0 += WORKING_REG_A.b.b0;
                break;
              case WORD_SIZE:
                DEST_REG_D.w.l += WORKING_REG_A.w.l;
                break;
              case DWORD_SIZE:
                DEST_REG_D.x += WORKING_REG_A.x;
                break;
              case 0xc0:
#warning finire word long
                regsA.r[(Pipe1 >> 9) & 7].x = WORKING_REG_A.x + Pipe2.d;
                break;
              }
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
                if(Pipe1 & 0b0000000100000000)  // direction... FARE
                  ;
              case BYTE_SIZE:
                DEST_REG_D.b.b0 += GetValue(WORKING_REG_A.x);
                break;
              case WORD_SIZE:
                DEST_REG_D.w.l += GetShortValue(WORKING_REG_A.x);
                break;
              case DWORD_SIZE:
                DEST_REG_D.x += GetIntValue(WORKING_REG_A.x);
                break;
              case 0xc0:
#warning finire word long
                regsA.r[(Pipe1 >> 9) & 7].x = GetIntValue(WORKING_REG_A.x) + Pipe2.d;
                break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
                if(Pipe1 & 0b0000000100000000)  // direction... FARE
                  ;
              case BYTE_SIZE:
                DEST_REG_D.b.b0 += GetValue(WORKING_REG_A.x);
                WORKING_REG_A.x++;
                break;
              case WORD_SIZE:
                DEST_REG_D.w.l += GetShortValue(WORKING_REG_A.x);
                WORKING_REG_A.x+=2;
                break;
              case DWORD_SIZE:
                DEST_REG_D.x += GetIntValue(WORKING_REG_A.x);
                WORKING_REG_A.x+=4;
                break;
              case 0xc0:
#warning finire word long
                regsA.r[(Pipe1 >> 9) & 7].x = GetIntValue(WORKING_REG_A.x) + Pipe2.d;
                WORKING_REG_A.x+=4;
                break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
                if(Pipe1 & 0b0000000100000000)  // direction... FARE
                  ;
              case BYTE_SIZE:
                WORKING_REG_A.x--;
                DEST_REG_D.b.b0 += GetValue(WORKING_REG_A.x);
                break;
              case WORD_SIZE:
                WORKING_REG_A.x-=2;
                DEST_REG_D.w.l += GetShortValue(WORKING_REG_A.x);
                break;
              case DWORD_SIZE:
                WORKING_REG_A.x-=4;
                DEST_REG_D.x += GetIntValue(WORKING_REG_A.x);
                break;
              case 0xc0:
#warning finire word long
                WORKING_REG_A.x-=4;
                regsA.r[(Pipe1 >> 9) & 7].x = GetIntValue(WORKING_REG_A.x) + Pipe2.d;
                break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
                if(Pipe1 & 0b0000000100000000)  // direction... FARE
                  ;
              case BYTE_SIZE:
                DEST_REG_D.b.b0 += GetValue(WORKING_REG_A.x + (signed short int)Pipe2.x.l);
                break;
              case WORD_SIZE:
                DEST_REG_D.w.l += GetShortValue(WORKING_REG_A.x + (signed short int)Pipe2.x.l);
                break;
              case DWORD_SIZE:
                DEST_REG_D.x += GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.x.l);
                break;
              case 0xc0:
#warning finire word long
                WORKING_REG_A.x-=4;
                regsA.r[(Pipe1 >> 9) & 7].x = GetIntValue(WORKING_REG_A.x+ Pipe2.x.l) + Pipe2.d;
                break;
              }
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
                if(Pipe1 & 0b0000000100000000)  // direction... FARE
                  ;
              case BYTE_SIZE:
                if(!DISPLACEMENT_SIZE)
                  DEST_REG_D.b.b0 += GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l);
                else
                  DEST_REG_D.b.b0 += GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l);
                break;
              case WORD_SIZE:
                if(!DISPLACEMENT_SIZE)
                  DEST_REG_D.w.l += GetShortValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l);
                else
                  DEST_REG_D.w.l += GetShortValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l);
                break;
              case DWORD_SIZE:
                if(!DISPLACEMENT_SIZE)
                  DEST_REG_D.x += GetIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l);
                else
                  DEST_REG_D.x += GetIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l);
                break;
              case 0xc0:
#warning finire word long
                WORKING_REG_A.x-=4;
                regsA.r[(Pipe1 >> 9) & 7].x = GetIntValue(WORKING_REG_A.x+ Pipe2.x.l) + Pipe2.d;
                break;
              }
            break;
          }
				break;
        
			case 0xe0:   // ASd
			case 0xe1:
        switch(ADDRESSING) {
          case ABSOLUTELADD:
            break;
          case DATAREGADD:   // Dn
                if(Pipe1 & 0b0000000100000000)  // rotate direction... FARE
                  ;
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRREGADD:   // An
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          }
				break;
        
			case 0xe2:   // LSd
			case 0xe3:
        switch(ADDRESSING) {
          case ABSOLUTELADD:
            break;
          case DATAREGADD:   // Dn
                if(Pipe1 & 0b0000000100000000)  // rotate direction... FARE
                  ;
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRREGADD:   // An
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          }
				break;
        
			case 0xe4:   // ROXd
			case 0xe5:
        switch(ADDRESSING) {
          case ABSOLUTELADD:
            break;
          case DATAREGADD:   // Dn
                if(Pipe1 & 0b0000000100000000)  // rotate direction... FARE
                  ;
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRREGADD:   // An
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          }
				break;
        
			case 0xe6:   // ROd
			case 0xe7:
        switch(ADDRESSING) {
          case ABSOLUTELADD:
            break;
                if(Pipe1 & 0b0000000100000000)  // rotate direction... FARE
                  ;
          case DATAREGADD:   // Dn
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRREGADD:   // An
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
              case 0xc0:
                break;
              }
            break;
          }
				break;
        
			case 0xe8:   // ASd LSd ROXd ROd
			case 0xe9:
			case 0xea:
			case 0xeb:
			case 0xec:
			case 0xed:
			case 0xee:
			case 0xef:
        switch(Pipe1 & 0b0000111000000000) { // rotate direction... FARE
#warning USARE HIBYTE per codice migliore!
          case 0b0000000000000000:
            break;
          case 0b0000001000000000:
            break;
          case 0b0000010000000000:
            break;
          case 0b0000011000000000:
            break;
          case 0b0000100000000000:
            break;
          case 0b0000101000000000:
            break;
          case 0b0000110000000000:
            break;
          case 0b0000111000000000:
            break;
          }
        switch(ADDRESSING) {
          case ABSOLUTELADD:
            break;
          case DATAREGADD:   // Dn
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                WORKING_REG_D.b.b0 <<= Pipe2.b.l;
                break;
              case WORD_SIZE:
                WORKING_REG_D.w.l <<= Pipe2.x.l;
                break;
              case DWORD_SIZE:
                WORKING_REG_D.x <<= Pipe2.d;
                break;
              }
            break;
          case ADDRREGADD:   // An
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                WORKING_REG_A.b.b0  <<= Pipe2.b.l;
                break;
              case WORD_SIZE:
                WORKING_REG_A.w.l  <<= Pipe2.x.l;
                break;
              case DWORD_SIZE:
                WORKING_REG_A.x  <<= Pipe2.d;
                break;
              }
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GetValue(WORKING_REG_A.x)  << Pipe2.d;
                PutValue(WORKING_REG_A.x,res3.b.l);
                break;
              case WORD_SIZE:
                res3.x = GetShortValue(WORKING_REG_A.x)  << Pipe2.d;
                PutShortValue(WORKING_REG_A.x,res3.x);
                break;
              case DWORD_SIZE:
                res3.d = GetIntValue(WORKING_REG_A.x)  << Pipe2.d;
                PutIntValue(WORKING_REG_A.x,res3.d);
                break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GetValue(WORKING_REG_A.x)  << Pipe2.d;
                PutValue(WORKING_REG_A.x,res3.b.l);
                WORKING_REG_A.x++;
                break;
              case WORD_SIZE:
                res3.x = GetShortValue(WORKING_REG_A.x)  << Pipe2.d;
                PutShortValue(WORKING_REG_A.x,res3.x);
                WORKING_REG_A.x+=2;
                break;
              case DWORD_SIZE:
                res3.d = GetIntValue(WORKING_REG_A.x)  << Pipe2.d;
                PutIntValue(WORKING_REG_A.x,res3.d);
                WORKING_REG_A.x+=4;
                break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                WORKING_REG_A.x--;
                res3.b.l = GetValue(WORKING_REG_A.x)  << Pipe2.d;
                PutValue(WORKING_REG_A.x,res3.b.l);
                break;
              case WORD_SIZE:
                WORKING_REG_A.x-=2;
                res3.x = GetShortValue(WORKING_REG_A.x)  << Pipe2.d;
                PutShortValue(WORKING_REG_A.x,res3.x);
                break;
              case DWORD_SIZE:
                WORKING_REG_A.x-=4;
                res3.d = GetIntValue(WORKING_REG_A.x)  << Pipe2.d;
                PutIntValue(WORKING_REG_A.x,res3.d);
                break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.x.l)  << Pipe2.d;
                PutValue(WORKING_REG_A.x,res3.b.l);
                break;
              case WORD_SIZE:
                res3.x = GetShortValue(WORKING_REG_A.x + (signed short int)Pipe2.x.l)  << Pipe2.d;
                PutShortValue(WORKING_REG_A.x,res3.x);
                break;
              case DWORD_SIZE:
                res3.d = GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.x.l)  << Pipe2.d;
                PutIntValue(WORKING_REG_A.x,res3.d);
                break;
              }
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.b.l = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l)  << Pipe2.d;
                  PutValue(WORKING_REG_A.x,res3.b.l);
                  }
                else {
                  res3.b.l = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l)  << Pipe2.d;
                  PutValue(WORKING_REG_A.x,res3.b.l);
                  }
                break;
              case WORD_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.x = GetShortValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l)  << Pipe2.d;
                  PutShortValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l,res3.x);
                  }
                else {
                  res3.x = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l)  << Pipe2.d;
                  PutValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l,res3.x);
                  }
                break;
              case DWORD_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.d = GetIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l)  << Pipe2.d;
                  PutIntValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l,res3.d);
                  }
                else {
                  res3.d = GetValue(WORKING_REG_A.x + DISPLACEMENT_REG.x + (signed char)Pipe2.b.l)  << Pipe2.d;
                  PutValue(WORKING_REG_A.x + DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l,res3.d);
                  }
                break;
              }
            break;
          }
				break;
        
			}
    
		} while(!fExit);
	}



#if 0
main(int argc, char *argv[]) {
	int i,j,k;
	unsigned char _based(rom_seg) *s;

	if(argc >=2) {
		debug=1;
		}
	if((rom_seg=_bheapseg(0x6000)) == _NULLSEG)
		goto fine;
	if((p=_bmalloc(rom_seg,0x6000)) == _NULLOFF)
		goto fine;
	if((ram_seg=_bheapseg(0xffe8)) == _NULLSEG)
		goto fine;
	if((p1=_bmalloc(ram_seg,0xffe8)) == _NULLOFF)
		goto fine;
	_fmemset(p,0,0x6000);
	_fmemset(p1,0,0xffe8);
	stack_seg=ram_seg+10;
/*
		for(i=0; i< 8192; i++) {
			printf("Indirizzo %04x, valore %02x\n",s,*s);
			s++;
			}
		*/
		close(i);
		OldTimer=_dos_getvect(0x8);
		OldCtrlC=_dos_getvect(0x23);
		OldKeyb = _dos_getvect( 9 );
		_dos_setvect(0x8,NewTimer);
		_dos_setvect(0x23,NewCtrlC);
		_dos_setvect( 9, NewKeyb );
		_setvideomode(_MRES16COLOR);
		_clearscreen(_GCLEARSCREEN);
		_displaycursor( _GCURSOROFF );
		Emulate();
		_dos_setvect(0x8,OldTimer);
		_dos_setvect(0x23,OldCtrlC);
		_dos_setvect( 9, OldKeyb );
		_displaycursor( _GCURSORON );
		_setvideomode(_DEFAULTMODE);
		}
fine:
	if(p1 != _NULLOFF)
		_bfree(ram_seg,p1);
	else
		puts("no off");
	if(ram_seg != _NULLSEG)
		_bfreeseg(ram_seg);
	else
		puts("no seg");
	if(p != _NULLOFF)
		_bfree(rom_seg,p);
	else
		puts("no off");
	if(rom_seg != _NULLSEG)
		_bfreeseg(rom_seg);
	else
		puts("no seg");
	}
#endif

