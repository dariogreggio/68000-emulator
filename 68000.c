//https://www.google.com/url?sa=t&rct=j&q=&esrc=s&source=web&cd=&ved=2ahUKEwjc3b6Lwar2AhUPTcAKHRVnCmUQFnoECAMQAQ&url=http%3A%2F%2Fgoldencrystal.free.fr%2FM68kOpcodes-v2.3.pdf&usg=AOvVaw3a6qPsk5K_kpQd1MnlD07r
//https://www.google.com/url?sa=t&rct=j&q=&esrc=s&source=web&cd=&ved=2ahUKEwjc3b6Lwar2AhUPTcAKHRVnCmUQFnoECAUQAQ&url=https%3A%2F%2Fweb.njit.edu%2F~rosensta%2Fclasses%2Farchitecture%2F252software%2Fcode.pdf&usg=AOvVaw0awr9hRKXycE2-kghhbC3Y
//https://onlinedisassembler.com/odaweb/

#warning i fattori di AND EOR OR ADD SUB sono TUTTI invertiti! (corretto in ADD SUB)...

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
#undef MC68010

BYTE fExit=0;
BYTE debug=0;
#ifdef QL
BYTE ram_seg[RAM_SIZE];
BYTE *rom_seg;
BYTE *rom_seg2;
#define VIDEO_ADDRESS_BASE 0x20000
DWORD VideoAddress=VIDEO_ADDRESS_BASE;
BYTE VideoMode;
SWORD VICRaster=0;
union {
  BYTE b[4];
  DWORD d;
  } RTC;
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
BYTE KBDataI,KBDataO,KBControl,/*KBStatus,*/ KBRAM[32];   // https://wiki.osdev.org/%228042%22_PS/2_Controller
#define KBStatus KBRAM[0]   // pare...
BYTE Keyboard[1]={0};
volatile BYTE TIMIRQ,VIDIRQ,KBDIRQ,SERIRQ,RTCIRQ;

extern volatile BYTE keysFeedPtr;

BYTE DoReset=0,IPL=0,FC /*bus state*/=0,ActivateReset=0,DoStop=0,DoAddressExcep=0,DoTrace=0;
#define MAX_WATCHDOG 100      // x30mS v. sotto
WORD WDCnt=MAX_WATCHDOG;
BYTE ColdReset=1;
SWORD Pipe1;
union __attribute__((__packed__)) PIPE {
	DWORD d;
	struct __attribute__((__packed__)) {
		WORD dummy;
  	WORD w;     // per quando serve 1 word sola...
		};
	BYTE bd[8];
	WORD wd[4];
	DWORD dd[2];
	struct __attribute__((__packed__)) {
		WORD l;
		WORD h;
		} x;
	struct __attribute__((__packed__)) {
		BYTE u;
		BYTE u2;
		BYTE l;
		BYTE h;		 // oppure spostare la pipe quando ci sono le istruzioni lunghe 4+...
		} b;
	} Pipe2;


#define IO_BASE_ADDRESS 0x7c000
#define LCD_BOARD_ADDRESS 0x7c100
#define RTC_BASE_ADDRESS 0x18000
// emulo display LCD testo (4x20, 4x20 o altro) come su scheda Z80:
//  all'indirizzo+0 c'è la porta comandi (fili C/D, RW e E (E2)), a indirizzo+1 la porta dati (in/out)
#define IO_BOARD_ADDRESS 0x0

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
  
  else if(t==0x18063) {        //   video mode (dice 063 o anche 023...
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
  else if(t==0x18021) {        //   interrupt/IPC link status
//    B 	M 	R 	X 	F 	T 	 I  	G 	 B=Baud state, M=Microdrive inactive, R=Rtc state, X=eXternal Interrupt, F=Frame vsync, T=Transmit interrupt, I=IPC Interface interrupt G=Gap interrupt (microdrive)
    i=VIDIRQ ? 0b00001000 : 0; 
    }
  else if(t>=0x18000 && t<0x18004) {        //   RTC
    i=RTC.b[t-0x18000];
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
  
	return i;
	}

SWORD GetShortValue(DWORD t) {
	register SWORD i;

  if(t & 1) {
    DoAddressExcep=1;
    return 0;
    }
	if(t < ROM_SIZE) {			// 
		i=MAKEWORD(rom_seg[t+1],rom_seg[t]);
		}
#ifdef ROM_SIZE2
	else if(t < ROM_SIZE2) {			// cartridge
		t-=ROM_SIZE;
		i=MAKEWORD(rom_seg[t+1],rom_seg[t]);
		}
#endif
	else if(t >= RAM_START && t < (RAM_START+RAM_SIZE)) {
		t-=RAM_START;
		i=MAKEWORD(ram_seg[t+1],ram_seg[t]);
		}
#ifdef QL
  else if(t>=0x18000 && t<0x18004) {        //   RTC
    i=MAKEWORD(RTC.b[t-0x18001],RTC.b[t-0x18000]);
    }
#elif MICHELEFABBRI
#endif
	return i;
	}

DWORD GetIntValue(DWORD t) {
	register DWORD i;

  if(t & 1) {
    DoAddressExcep=1;
    return 0;
    }
	if(t < ROM_SIZE) {			// 
		i=MAKELONG(MAKEWORD(rom_seg[t+3],rom_seg[t+2]),MAKEWORD(rom_seg[t+1],rom_seg[t+0]));
		}
#ifdef ROM_SIZE2
	else if(t < ROM_SIZE2) {			// cartridge
		t-=ROM_SIZE;
		i=MAKELONG(MAKEWORD(rom_seg[t+3],rom_seg[t+2]),MAKEWORD(rom_seg[t+1],rom_seg[t+0]));
		}
#endif
	else if(t >= RAM_START && t < (RAM_START+RAM_SIZE)) {
		t-=RAM_START;
		i=MAKELONG(MAKEWORD(ram_seg[t+3],ram_seg[t+2]),MAKEWORD(ram_seg[t+1],ram_seg[t+0]));
		}
#ifdef QL
#elif MICHELEFABBRI
#endif
	return i;
	}

SWORD GetPipe(DWORD t) {
  SWORD *p;

  if(t & 1) {
    DoAddressExcep=1;
    return;
    }
	if(t < ROM_SIZE) {			// 
    if(!(t & 3))
      ;
#warning se indirizzo è long aligned, si potrebbero leggere 2 SWORD in un colpo solo VERIFICARE test e velocità
		Pipe1=MAKEWORD(rom_seg[t+1],rom_seg[t]);
    t+=2;
		Pipe2.x.h=MAKEWORD(rom_seg[t+1],rom_seg[t]);
    t+=2;
//		Pipe2.b.h=rom_seg[t++];
//		Pipe2.b.u=rom_seg[t];
		Pipe2.x.l=MAKEWORD(rom_seg[t+1],rom_seg[t]);
		}
	else if(t >= RAM_START && t < (RAM_START+RAM_SIZE)) {
    if(!(t & 3))
      ;
	  Pipe1=MAKEWORD(ram_seg[t+1],ram_seg[t]);
    t+=2;
		Pipe2.x.h=MAKEWORD(ram_seg[t+1],ram_seg[t]);
//		Pipe2.b.h=ram_seg[t++];
//		Pipe2.b.u=ram_seg[t];
    t+=2;
		Pipe2.x.l=MAKEWORD(ram_seg[t+1],ram_seg[t]);
		}
	return Pipe1;
	}

BYTE AdvPipe(DWORD t,BYTE n) {    // questa va usata quando il primo parm dell'istruzione è esplicito! NON per ils econdo (verificare)
  
  if(t & 1) {
    DoAddressExcep=1;
    return;
    }
  switch(n) {
    case 2:
  		Pipe2.wd[1]=Pipe2.wd[0];
      t+=4;
      if(t < ROM_SIZE) {			// 
        Pipe2.wd[0]=MAKEWORD(rom_seg[t+1],rom_seg[t]);
        }
      else if(t >= RAM_START && t < (RAM_START+RAM_SIZE)) {
        Pipe2.wd[0]=MAKEWORD(ram_seg[t+1],ram_seg[t]);
        }
      break;
    case 4:
      t+=4;
      if(t < ROM_SIZE) {			// 
        if(!(t & 3))
          ;
    #warning se indirizzo è long aligned, si potrebbero leggere 2 SWORD in un colpo solo VERIFICARE test e velocità
        Pipe2.wd[1]=MAKEWORD(rom_seg[t+1],rom_seg[t]);
        t+=2;
        Pipe2.wd[0]=MAKEWORD(rom_seg[t+1],rom_seg[t]);
        }
      else if(t >= RAM_START && t < (RAM_START+RAM_SIZE)) {
        if(!(t & 3))
          ;
        Pipe2.wd[1]=MAKEWORD(ram_seg[t+1],ram_seg[t]);
        t+=2;
        Pipe2.wd[0]=MAKEWORD(ram_seg[t+1],ram_seg[t]);
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
  
  else if(t==0x18063) {        //   video mode
    VideoMode=t1;
    if(VideoMode & 0b10000000)
      VideoAddress=0x28000;
    else
      VideoAddress=VIDEO_ADDRESS_BASE;
    }
  else if(t>=0x18000 && t<0x18004) {        //   RTC
    RTC.b[t-0x18000]=t1;
    }
  else if(t>=0x18022 && t<0x18023) {        //   microdrive

    }
  else if(t==0x18021) {        //   interrupt/IPC link status
//   R 	F 	M 	X 	F 	T 	 I  	G 	 R=tRansmit mask, F=interFace mask, M=gap Mask,X=reset eXternal Interrupt,F=reset Frame vsync, T=reset Transmit interrupt, I=reset IPC Interface interrupt G=reset Gap interrupt 
    if(t & 0b00001000)
      VIDIRQ=0; 

    }
  else if(t==0x18020) {        //   microdrive/RS232 link status

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
  else {      // boh??
// pare di no..    DoAddressExcep=1;
    }

	}

void PutShortValue(DWORD t,SWORD t1) {
	register SWORD i;

// printf("rom_seg: %04x, p: %04x\n",rom_seg,p);

  if(t & 1) {
    DoAddressExcep=1;
    return;
    }
	if(t >= RAM_START && t < (RAM_START+RAM_SIZE)) {		// ZX80,81
		t-=RAM_START;
	  ram_seg[t++]=HIBYTE(t1);
	  ram_seg[t]=LOBYTE(t1);
		}
#ifdef QL
  else if(t>=0x18000 && t<0x18004) {        //   RTC
    RTC.b[t-0x18000]=HIBYTE(t1);
    RTC.b[t-0x18001]=LOBYTE(t1);
    }
#elif MICHELEFABBRI
#endif
  else {      // boh??
// pare di no..    DoAddressExcep=1;
    }
	}

void PutIntValue(DWORD t,DWORD t1) {
	register SWORD i;

// printf("rom_seg: %04x, p: %04x\n",rom_seg,p);

  if(t & 1) {
    DoAddressExcep=1;
    }
	if(t >= RAM_START && t < (RAM_START+RAM_SIZE)) {		// ZX80,81
		t-=RAM_START;
	  ram_seg[t++]=HIBYTE(HIWORD(t1));
	  ram_seg[t++]=LOBYTE(HIWORD(t1));
	  ram_seg[t++]=HIBYTE(LOWORD(t1));
	  ram_seg[t]=LOBYTE(LOWORD(t1));
		}
#ifdef QL
#elif MICHELEFABBRI
#endif
  else {      // boh??
// pare di no..    DoAddressExcep=1;
    }
  
	}

#define MOVEM_R2M()  if(!LOBYTE(Pipe1) & 0x40) {\
    BYTE i; WORD j;\
    for(i=0; i<16; i++) {\
      if(res2.w & (1 << i)) {\
        PutShortValue(res3.x, ALL_REGS.r[i].w.l);\
        res3.x+=2;\
        }\
      }\
    }\
  else {\
    BYTE i; WORD j;\
    for(i=0; i<16; i++) {\
      if(res2.w & (1 << i)) {\
        PutIntValue(res3.x, ALL_REGS.r[i].x);\
        res3.x+=4;\
        }\
      }\
    }
#define MOVEM_R2M_PREDEC()  if(!LOBYTE(Pipe1) & 0x40) {\
    signed char i; WORD j;\
    for(i=15; i>=0; i--) {\
      if(res2.w & (1 << i)) {\
        res3.x-=2;\
        PutShortValue(res3.x, ALL_REGS.r[i].w.l);\
        }\
      }\
    }\
  else {\
    signed char i; WORD j;\
    for(i=15; i>=0; i--) {\
      if(res2.w & (1 << i)) {\
        res3.x-=4;\
        PutIntValue(res3.x, ALL_REGS.r[i].x);\
        }\
      }\
    }
#define MOVEM_M2R()  if(!LOBYTE(Pipe1) & 0x40) {\
    BYTE i; WORD j;\
    for(i=0; i<16; i++) {\
      if(res2.w & (1 << i)) {\
        ALL_REGS.r[i].x = (int32_t)GetShortValue(res3.x);\
        res3.x+=2;\
        }\
      }\
    }\
  else {\
    BYTE i; WORD j;\
    for(i=0; i<16; i++) {\
      if(res2.w & (1 << i)) {\
        ALL_REGS.r[i].x = GetIntValue(res3.x);\
        res3.x+=4;\
        }\
      }\
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
		BYTE  b[32];
	  union REG r[8];
		} regsD;
	union __attribute__((__packed__)) A_REGISTERS {   // questi SEGUONO i D (v. DISPLACEMENT_REG2 e anche MOVEM)
		BYTE  b[32];
	  union REG r[8];
		} regsA;
//	DWORD _sp=0;
#define _sp regsA.r[7].x    // A7 == SP, dice; gestisco 2 distinti a7, che swappo quando SR.Supervisor cambia
  DWORD a7S,a7U;
	DWORD _pc=0;
#define WORKING_REG_A (regsA.r[((LOBYTE(Pipe1)) & 7)])
#define WORKING_REG_D (regsD.r[((LOBYTE(Pipe1)) & 7)])
#define ALL_REGS regsD
#define DEST_REG_A (regsA.r[(((HIBYTE(Pipe1)) >> 1) & 7)])
#define DEST_REG_D (regsD.r[(((HIBYTE(Pipe1)) >> 1) & 7)])
#define ADDRESSING (LOBYTE(Pipe1) & 0b00111000)
#define ADDRESSING2 ((Pipe1 & 0b0000000111000000) >> 3)      // per MOVE/MOVEA...
enum ADDRESSING_MODES {
  DATAREGADD = 0b00000000,
  ADDRREGADD = 0b00001000,
  ADDRADD =    0b00010000,
  ADDRPINCADD =0b00011000,
  ADDRPDECADD =0b00100000,
  ADDRDISPADD =0b00101000,
  ADDRIDXADD = 0b00110000,
  PCDISPADD  = 0b00111000,  // bah lascio cmq i sinonimi...
  PCIDXADD   = 0b00111000,
  ABSOLUTEADD =0b00111000,
  IMMEDIATEADD=0b00111000,
  ABSSSUBADD  =0b000,    // è al posto di Register
  ABSLSUBADD  =0b001,    // 
  PCIDXSUBADD =0b010,    // 
  PCDISPSUBADD=0b011,    // 
  IMMSUBADD   =0b100 
  };
#define OPERAND_SIZE (LOBYTE(Pipe1) & 0b11000000)
#define	BYTE_SIZE 0x00
#define	WORD_SIZE 0x40
#define	DWORD_SIZE 0x80
#define	SIZE_ELSE 0xC0
#define DISPLACEMENT_REG (ALL_REGS.r[((Pipe2.b.h >> 4) & 15)])   // sia D che A (v.sopra)
#define DISPLACEMENT_SIZE (Pipe2.b.h & 0b00001000)
#define DISPLACEMENT_SCALE (1 << ((Pipe2.b.h & 0b00000110) >> 1))
#define ABSOLUTEADD_SIZE (Pipe1 & 0b0000000000000111)
#define ABSOLUTEADD_SIZE2 ((Pipe1 & 0b0000111000000000) >> 9)    // per MOVE/MOVEA
#define	BYTE_SIZE2 0x10     // coglioni ;)
#define	WORD_SIZE2 0x30
#define	DWORD_SIZE2 0x20

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
        unsigned int unused2: 2;    // su 68020 ecc qua c'è Master/Interrupt
        unsigned int Supervisor: 1;
        unsigned int unused3: 1;    // su 68020 ecc questo è Trace0 e il seg. Trace1
        unsigned int Trace: 1;
        };
      } SR;
		} _fIRQ;
	register union REGISTRO_F _f;
	union REGISTRO_F _fsup;
//	register SWORD i;
register union __attribute__((__packed__)) RESULT {
  struct __attribute__((__packed__)) {
    BYTE l;
    BYTE h;
    } b;
  WORD w;
  DWORD x;
  } res1,res2,res3;
int c=0;


	_pc=0;
  
  
 
  
	do {

		c++;
#ifdef QL
		if(!(c & 0x00003fff)) {
#else
		if(!(c & 0x0000ffff)) {
#endif
      ClrWdt();
// yield()
#ifndef USING_SIMULATOR
#ifdef QL
      UpdateScreen(VICRaster,VICRaster+8);
      VICRaster+=8;     // TROVARE registro HW!!
      if(VICRaster>=256) {
        VICRaster=0;
        VIDIRQ=1;
//      IPL=0b110;
//      ExtIRQNum=0x70;      // IRQ video??
        }
#else
			UpdateScreen(1);      // 50mS 17/11/22
#endif
#endif
      LED1^=1;    // 50mS~ con fabbri 17/11/22; 
      
      WDCnt--;
      if(!WDCnt) {
        WDCnt=MAX_WATCHDOG;
#ifdef QL
#elif MICHELEFABBRI
#else
        if(IOPortO & 0b00001000) {     // WDEn
          DoReset=1;
          }
#endif
        }
      
      }
		if(ColdReset) {
			ColdReset=0;
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
#ifdef QL
#elif MICHELEFABBRI
#else
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
      FC=0b110;
      _f.SR.w=0b0010011100000000;   // supervisor, IPL=7, area programma VERIFICARE
			_pc=GetIntValue(0x00000004);
			_sp=GetIntValue(0x00000000);   // bah per sicurezza li imposto entrambi :D NO!
      a7S=_sp;
			DoReset=0;
      ActivateReset=0;
			}
		if(IPL > _f.SR.IRQMask || IPL==7) {     // https://www.cs.mcgill.ca/~cs573/fall2002/notes/lec273/lecture21/21_2.htm
      // PERO' dice anche che 7 NON PUO' essere ignorato...
      FC=0b111;
      _fsup=_f;
      if(!_f.SR.Supervisor)
        a7U=regsA.r[7].x;
      else
        a7S=regsA.r[7].x;
      _f.SR.Supervisor=1;     // non mi è chiaro se sono nested o no...
      regsA.r[7].x=a7S;
      _f.SR.Trace=0;   // supervisor, area programma VERIFICARE solo >68020?
//      _f.SR.MasterInterrupt=1;   // supervisor, area programma VERIFICARE solo >68020?
      _f.SR.IRQMask=IPL;
      DoStop=0;
      _sp-=4;
      PutIntValue(_sp,_pc); // low word prima
      _sp-=2;
      PutShortValue(_sp,_fsup.w);
        
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
// si potrebbe anche    res3.b.l=0x18+IPL;  goto doTrap;
      FC=0b110;
			}

		if(DoStop)
			continue;		// esegue cmq IRQ FORSE qua

    if(ActivateReset) {
      // abbassare pin HW...
      ActivateReset--;
      if(!ActivateReset) {
        // e alzare
        }
      }
    if(DoAddressExcep) {
      DoAddressExcep=0;
      res3.b.l=3;
      goto doTrap;
      }
    if(DoTrace) {
      DoTrace=0;
      res3.b.l=9;
      goto doTrap;
      }

//printf("Pipe1: %04x, Pipe2w: %04x, Pipe2b1: %02x,%02x\n",Pipe1,Pipe2.word,Pipe2.bytes.byte1,Pipe2.bytes.byte2);
    
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
  
  
    if(_f.SR.Trace /* && !IRQ ... */)
      DoTrace=1;
      
		GetPipe(_pc); _pc+=2;
		switch(HIBYTE(Pipe1)) {
			case 0x00:   // ORI
        switch(ADDRESSING) {
          case ABSOLUTEADD:
            switch(LOBYTE(Pipe1)) {
              case 0x3c:
                _f.CCR.b |= Pipe2.b.l;
    						_pc+=2;
                goto noAggFlag;
                break;
              case 0x7c:
    						_pc+=2;
                if(_f.SR.Supervisor) {
                  res3.w = _f.SR.w | Pipe2.w;
                  
aggSR:
                  a7S=regsA.r[7].x;
                  _f.SR.w=res3.w;
                  if(!_f.SR.Supervisor)
                    regsA.r[7].x=a7U;
                  else
                    regsA.r[7].x=a7S;
	                goto noAggFlag;
                  }
                else {
                  res3.b.l=8;
                  goto doTrap;
                  }
	              break;
              default:
                switch(ABSOLUTEADD_SIZE) {
                  case ABSSSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res3.b.l = Pipe2.b.l;
                        _pc+=AdvPipe(_pc,2);
                        res3.b.l |= GetValue(Pipe2.w);
                        PutValue(Pipe2.w, res3.b.l);
                        _pc+=2;
                        break;
                      case WORD_SIZE:
                        res3.w = Pipe2.w;
                        _pc+=AdvPipe(_pc,2);
                        res3.w |= GetShortValue(Pipe2.w);
                        PutShortValue(Pipe2.w, res3.w);
                        _pc+=2;
                        break;
                      case DWORD_SIZE:
                        res3.x = Pipe2.d;
                        _pc+=AdvPipe(_pc,4);
                        res3.x |= GetIntValue(Pipe2.w);
                        PutIntValue(Pipe2.w, res3.x);
                        _pc+=2;
                        break;
                      }
                    break;
                  case ABSLSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res3.b.l = Pipe2.b.l;
                        _pc+=AdvPipe(_pc,2);
                        res3.b.l |= GetValue(Pipe2.d);
                        PutValue(Pipe2.d,res3.b.l);
                        _pc+=4;
                        break;
                      case WORD_SIZE:
                        res3.w = Pipe2.w;
                        _pc+=AdvPipe(_pc,2);
                        res3.w |= GetShortValue(Pipe2.d);
                        PutShortValue(Pipe2.d,res3.w);
                        _pc+=4;
                        break;
                      case DWORD_SIZE:
                        res3.x = Pipe2.d;
                        _pc+=AdvPipe(_pc,4);
                        res3.x |= GetIntValue(Pipe2.d); 
                        PutIntValue(Pipe2.d,res3.x);
                        _pc+=4;
                        break;
                      }
                    break;
                  case PCIDXSUBADD:   // qua no!
                  case PCDISPSUBADD:
                  case IMMSUBADD:
                    break;
                  }
                break;
              }
            break;
          case DATAREGADD:   // Dn
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = WORKING_REG_D.b.b0 |= Pipe2.b.l;
								_pc+=2;
                break;
              case WORD_SIZE:
                res3.w = WORKING_REG_D.w.l |= Pipe2.w;
								_pc+=2;
                break;
              case DWORD_SIZE:
                res3.x = WORKING_REG_D.x |= Pipe2.d;
								_pc+=4;
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
								_pc+=2;
                break;
              case WORD_SIZE:
                res3.w = GetShortValue(WORKING_REG_A.x) | Pipe2.w;
                PutShortValue(WORKING_REG_A.x, res3.w);
								_pc+=2;
                break;
              case DWORD_SIZE:
                res3.x = GetIntValue(WORKING_REG_A.x) | Pipe2.d;
                PutIntValue(WORKING_REG_A.x, res3.x);
								_pc+=4;
                break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GetValue(WORKING_REG_A.x) | Pipe2.b.l;
                PutValue(WORKING_REG_A.x, res3.b.l);
                WORKING_REG_A.x++;
								_pc+=2;
                break;
              case WORD_SIZE:
                res3.w = GetShortValue(WORKING_REG_A.x) | Pipe2.w;
                PutShortValue(WORKING_REG_A.x, res3.w);
                WORKING_REG_A.x+=2;
								_pc+=2;
                break;
              case DWORD_SIZE:
                res3.x = GetIntValue(WORKING_REG_A.x) | Pipe2.d;
                PutIntValue(WORKING_REG_A.x, res3.x);
                WORKING_REG_A.x+=4;
								_pc+=4;
                break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                WORKING_REG_A.x--;
                res3.b.l = GetValue(WORKING_REG_A.x) | Pipe2.b.l;
                PutValue(WORKING_REG_A.x, res3.b.l);
								_pc+=2;
                break;
              case WORD_SIZE:
                WORKING_REG_A.x-=2;
                res3.w = GetShortValue(WORKING_REG_A.x) | Pipe2.w;
                PutShortValue(WORKING_REG_A.x, res3.w);
								_pc+=2;
                break;
              case DWORD_SIZE:
                WORKING_REG_A.x-=4;
                res3.x = GetIntValue(WORKING_REG_A.x) | Pipe2.d;
                PutIntValue(WORKING_REG_A.x, res3.x);
								_pc+=4;
                break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = Pipe2.b.l;
                _pc+=AdvPipe(_pc,2);
                res3.b.l |= GetValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                PutValue(WORKING_REG_A.x + (signed short int)Pipe2.w, res3.b.l);
                break;
              case WORD_SIZE:
                res3.w = Pipe2.w;
                _pc+=AdvPipe(_pc,2);
                res3.w |= GetValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                PutShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w, res3.w);
                break;
              case DWORD_SIZE:
                res3.x = Pipe2.d;
                _pc+=AdvPipe(_pc,4);
                res3.x |= GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                PutIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w, res3.x);
                break;
              }
						_pc+=2;
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.b.l = Pipe2.b.l;
                  _pc+=AdvPipe(_pc,2);
                  res3.b.l |= GetValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  PutValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.b.l);
                  }
                else {
                  res3.b.l = Pipe2.b.l;
                  _pc+=AdvPipe(_pc,2);
                  res3.b.l |= GetValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  PutValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.b.l);
                  }
                break;
              case WORD_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.w = Pipe2.w;
                  _pc+=AdvPipe(_pc,2);
                  res3.w |= GetShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  PutShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.w);
                  }
                else {
                  res3.w = Pipe2.w;
                  _pc+=AdvPipe(_pc,2);
                  res3.w |= GetShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  PutShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.w);
                  }
                break;
              case DWORD_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.x = Pipe2.d;
                  _pc+=AdvPipe(_pc,4);
                  res3.x |= GetIntValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  PutIntValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.x);
                  }
                else {
                  res3.x = Pipe2.d;
                  _pc+=AdvPipe(_pc,4);
                  res3.x |= GetIntValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  PutIntValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.x);
                  }
                break;
              }
            _pc+=2;
            break;
          }

aggFlag:
        switch(OPERAND_SIZE) {
          case BYTE_SIZE:
aggFlag1:
            _f.CCR.Zero=!res3.b.l;
            _f.CCR.Sign=!!(res3.b.l & 0x80);
            break;
          case WORD_SIZE:
aggFlag2:
            _f.CCR.Zero=!res3.w;
            _f.CCR.Sign=!!(res3.w & 0x8000);
            break;
          case DWORD_SIZE:
aggFlag4:
            _f.CCR.Zero=!res3.x;
            _f.CCR.Sign=!!(res3.x & 0x80000000);
            break;
          }
        _f.CCR.Ovf=_f.CCR.Carry=0;
noAggFlag:
				break;

			case 0x08:   // BTST BCHG BCLR BSET
        res2.b.l=Pipe2.b.l;
        _pc+=AdvPipe(_pc,2);
        goto doBit;
        break;
        
			case 0x01:   // BTST BCHG BCLR BSET opp. MOVEP
			case 0x03:
			case 0x05:
			case 0x07:
			case 0x09:
			case 0x0b:
			case 0x0d:
			case 0x0f:
        if(ADDRESSING == ADDRREGADD) {      // MOVEP VERIFICARE ORDINE BYTE (v.pdf)
          if(LOBYTE(Pipe1) & 0b10000000) {    // Dx -> Mem
            if(LOBYTE(Pipe1) & 0b01000000) {    // 32bit
              DEST_REG_D.x = GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
              }
            else {                            // 16bit
              DEST_REG_D.w.l = GetShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
              }
            }
          else {                              // Mem -> Dx
            if(LOBYTE(Pipe1) & 0b01000000) {    // 32bit
              PutIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w,DEST_REG_D.x);
              }
            else {                            // 16bit
              PutShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w,DEST_REG_D.w.l);
              }
            }
          _pc+=2;
          }
        else {
          res2.b.l=DEST_REG_D.b.b0;
          
doBit:
          switch(ADDRESSING) {      // BTST ecc
            case ABSOLUTEADD:
              switch(ABSOLUTEADD_SIZE) {
                case ABSSSUBADD:
                  res1.b.l = GetValue(Pipe2.w);
                  _pc+=2;
                  break;
                case ABSLSUBADD:
                  res1.b.l = GetValue(Pipe2.d);
                  _pc+=4;
                  break;
                case PCIDXSUBADD:   // (D16,PC) SOLO BTST!!
                  // quindi filtrarli o ce ne sbattiamo??
                  res1.b.l = GetValue(_pc+(signed short int)Pipe2.w);
                  _pc+=2;
                  break;
                case PCDISPSUBADD:   // (D8,Xn,PC) SOLO BTST!!
                  if(!DISPLACEMENT_SIZE)
                    res1.b.l = GetValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  else
                    res1.b.l = GetValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  _pc+=2;
                  break;
                case IMMSUBADD:
                  res1.b.l = Pipe2.b.l;
                  _pc+=2;
                  break;
                }
              break;
            case DATAREGADD:   // Dn
              res1.x = WORKING_REG_D.x;
              break;
            case ADDRREGADD:   // qua no!
              break;
            case ADDRADD:   // (An)
              res1.b.l = GetValue(WORKING_REG_A.x);
              break;
            case ADDRPINCADD:   // (An)+
              res1.b.l = GetValue(WORKING_REG_A.x);
              break;
            case ADDRPDECADD:   // -(An)
              WORKING_REG_A.x--;
              res1.b.l = GetValue(WORKING_REG_A.x);
              break;
            case ADDRDISPADD:   // (D16,An)
              res1.b.l = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
              _pc+=2;
              break;
            case ADDRIDXADD:   // (D8,An,Xn)
              if(!DISPLACEMENT_SIZE) {
                res1.b.l = GetValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                }
              else {
                res1.b.l = GetValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                }
              _pc+=2;
              break;
            }
          switch(LOBYTE(Pipe1) & 0b11000000) {
            case 0x00:    // BTST
              if(!(((LOBYTE(Pipe1)) >> 3) & 7)) {    // 32bit, solo con registro Dn
                res3.x = res1.x & (1 << res2.b.l);
                _f.CCR.Zero = !res3.x;
                }
              else {                             // opp. 8bit
                res3.b.l = res1.b.l & (1 << res2.b.l);
                _f.CCR.Zero = !res3.b.l;
                }
              goto doBit2;
              break;
            case 0x40:    // BCHG
              if(!(((LOBYTE(Pipe1)) >> 3) & 7)) {    // 32bit, solo con registro Dn
                res3.x = res1.x & (1 << res2.b.l);
                _f.CCR.Zero = !res3.x;
                }
              else {                             // opp. 8bit
                res3.b.l = res1.b.l & (1 << res2.b.l);
                _f.CCR.Zero = !res3.b.l;
                }
              if(!(((LOBYTE(Pipe1)) >> 3) & 7)) {    // 32bit, solo con registro Dn
                res3.x = res1.x ^ (1 << res2.b.l);
                }
              else {                             // opp. 8bit
                res3.b.l = res1.b.l ^ (1 << res2.b.l);
                }
              break;
            case 0x80:    // BCLR
              if(!(((LOBYTE(Pipe1)) >> 3) & 7)) {    // 32bit, solo con registro Dn
                res3.x = res1.x & (1 << res2.b.l);
                _f.CCR.Zero = !res3.x;
                }
              else {                             // opp. 8bit
                res3.b.l = res1.b.l & (1 << res2.b.l);
                _f.CCR.Zero = !res3.b.l;
                }
              if(!(((LOBYTE(Pipe1)) >> 3) & 7)) {    // 32bit, solo con registro Dn
                res3.x = res1.x & ~(1 << res2.b.l);
                }
              else {                             // opp. 8bit
                res3.b.l = res1.b.l & ~(1 << res2.b.l);
                }
              break;
            case SIZE_ELSE:    // BSET
              if(!(((LOBYTE(Pipe1)) >> 3) & 7)) {    // 32bit, solo con registro Dn
                res3.x = res1.x & (1 << res2.b.l);
                _f.CCR.Zero = !res3.x;
                }
              else {                             // opp. 8bit
                res3.b.l = res1.b.l & (1 << res2.b.l);
                _f.CCR.Zero = !res3.b.l;
                }
              if(!(((LOBYTE(Pipe1)) >> 3) & 7)) {    // 32bit, solo con registro Dn
                res3.x = res1.x | (1 << res2.b.l);
                }
              else {                             // opp. 8bit
                res3.b.l = res1.b.l | (1 << res2.b.l);
                }
              break;
            }
          switch(ADDRESSING) {
            case ABSOLUTEADD:
                switch(ABSOLUTEADD_SIZE) {
                  case ABSSSUBADD:
                    PutValue(Pipe2.w, res3.b.l);
                    break;
                  case ABSLSUBADD:
                    PutValue(Pipe2.d, res3.b.l);
                    break;
                  case PCIDXSUBADD:   // (D16,PC) SOLO BTST!
                  case PCDISPSUBADD:   // (D8,Xn,PC)
                  case IMMSUBADD:
                    break;
                  }
              break;
            case DATAREGADD:   // Dn
              WORKING_REG_D.x = res3.x;
              break;
            case ADDRADD:   // (An)
              PutValue(WORKING_REG_A.x, res3.b.l);
              break;
            case ADDRPINCADD:   // (An)+
              PutValue(WORKING_REG_A.x, res3.b.l);
              break;
            case ADDRPDECADD:   // -(An)
              PutValue(WORKING_REG_A.x, res3.b.l);
              break;
            case ADDRDISPADD:   // (D16,An)
              PutValue(WORKING_REG_A.x + (signed short int)Pipe2.w, res3.b.l);
              break;
            case ADDRIDXADD:   // (D8,An,Xn)
              if(!DISPLACEMENT_SIZE) {
                PutValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.b.l);
                }
              else {
                PutValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.b.l);
                }
              break;
            }
          }
doBit2:   ;
				break;
        
			case 0x02:   // ANDI
        switch(ADDRESSING) {
          case ABSOLUTEADD:    // abs
            switch(LOBYTE(Pipe1)) {
              case 0x3c:
                _f.CCR.b &= Pipe2.b.l;
      					_pc+=2;
								goto noAggFlag;
                break;
              case 0x7c:
      					_pc+=2;
                if(_f.SR.Supervisor) {
                  res3.w = _f.SR.w & Pipe2.w;
			            goto aggSR;
                  }
                else {
                  res3.b.l=8;
                  goto doTrap;
                  }
                break;
              default:
                switch(ABSOLUTEADD_SIZE) {
                  case ABSSSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res3.b.l = Pipe2.b.l;
                        _pc+=AdvPipe(_pc,2);
                        res3.b.l &= GetValue(Pipe2.w);
                        PutValue(Pipe2.w, res3.b.l);
                        break;
                      case WORD_SIZE:
                        res3.w = Pipe2.w;
                        _pc+=AdvPipe(_pc,2);
                        res3.w &= GetShortValue(Pipe2.w);
                        PutShortValue(Pipe2.w, res3.w);
                        break;
                      case DWORD_SIZE:
                        res3.x = Pipe2.d;
                        _pc+=AdvPipe(_pc,4);
                        res3.x &= GetIntValue(Pipe2.w);
                        PutIntValue(LOWORD(Pipe2.dd[1]), res3.x);
                        break;
                      }
                    _pc+=2;
                    break;
                  case ABSLSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res3.b.l = Pipe2.b.l;
                        _pc+=AdvPipe(_pc,2);
                        res3.b.l &= GetValue(Pipe2.d);
                        PutValue(Pipe2.d,res3.b.l);
                        break;
                      case WORD_SIZE:
                        res3.w = Pipe2.w;
                        _pc+=AdvPipe(_pc,2);
                        res3.w &= GetShortValue(Pipe2.d);
                        PutShortValue(Pipe2.d,res3.w);
                        break;
                      case DWORD_SIZE:
                        res3.x = Pipe2.d;
                        _pc+=AdvPipe(_pc,4);
                        res3.x &= GetIntValue(Pipe2.d); 
                        PutIntValue(Pipe2.d,res3.x);
                        break;
                      }
                    _pc+=4;
                    break;
                  case PCIDXSUBADD:   // qua no!
                  case PCDISPSUBADD:
                  case IMMSUBADD:
                    break;
                  }
                break;
              }
            break;
          case DATAREGADD:   // Dn
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = WORKING_REG_D.b.b0 &= Pipe2.b.l;
								_pc+=2;
                break;
              case WORD_SIZE:
                res3.w = WORKING_REG_D.w.l &= Pipe2.w;
								_pc+=2;
                break;
              case DWORD_SIZE:
                res3.x = WORKING_REG_D.x &= Pipe2.d;
								_pc+=4;
                break;
              }
            break;
          case ADDRREGADD:   // An NO!
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GetValue(WORKING_REG_A.x) & Pipe2.b.l;
                PutValue(WORKING_REG_A.x, res3.b.l);
								_pc+=2;
                break;
              case WORD_SIZE:
                res3.w = GetShortValue(WORKING_REG_A.x) & Pipe2.w;
                PutShortValue(WORKING_REG_A.x, res3.w);
								_pc+=2;
                break;
              case DWORD_SIZE:
                res3.x = GetIntValue(WORKING_REG_A.x) & Pipe2.d;
                PutIntValue(WORKING_REG_A.x, res3.x);
								_pc+=4;
                break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GetValue(WORKING_REG_A.x) & Pipe2.b.l;
                PutValue(WORKING_REG_A.x, res3.b.l);
                WORKING_REG_A.x++;
								_pc+=2;
                break;
              case WORD_SIZE:
                res3.w = GetShortValue(WORKING_REG_A.x) & Pipe2.w;
                PutShortValue(WORKING_REG_A.x, res3.w);
                WORKING_REG_A.x+=2;
								_pc+=2;
                break;
              case DWORD_SIZE:
                res3.x = GetIntValue(WORKING_REG_A.x) & Pipe2.d;
                PutIntValue(WORKING_REG_A.x, res3.x);
                WORKING_REG_A.x+=4;
								_pc+=4;
                break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                WORKING_REG_A.x--;
                res3.b.l = GetValue(WORKING_REG_A.x) & Pipe2.b.l;
                PutValue(WORKING_REG_A.x, res3.b.l);
								_pc+=2;
                break;
              case WORD_SIZE:
                WORKING_REG_A.x-=2;
                res3.w = GetShortValue(WORKING_REG_A.x) & Pipe2.w;
                PutShortValue(WORKING_REG_A.x, res3.w);
								_pc+=2;
                break;
              case DWORD_SIZE:
                WORKING_REG_A.x-=4;
                res3.x = GetIntValue(WORKING_REG_A.x) & Pipe2.d;
                PutIntValue(WORKING_REG_A.x, res3.x);
								_pc+=4;
                break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = Pipe2.b.l;
                _pc+=AdvPipe(_pc,2);
                res3.b.l &= GetValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                PutValue(WORKING_REG_A.x + (signed short int)Pipe2.w, res3.b.l);
                break;
              case WORD_SIZE:
                res3.w = Pipe2.w;
                _pc+=AdvPipe(_pc,2);
                res3.w &= GetValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                PutShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w, res3.w);
                break;
              case DWORD_SIZE:
                res3.x = Pipe2.d;
                _pc+=AdvPipe(_pc,4);
                res3.x &= GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                PutIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w, res3.x);
                break;
              }
						_pc+=2;
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.b.l = Pipe2.b.l;
                  _pc+=AdvPipe(_pc,2);
                  res3.b.l &= GetValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  PutValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.b.l);
                  }
                else {
                  res3.b.l = Pipe2.b.l;
                  _pc+=AdvPipe(_pc,2);
                  res3.b.l &= GetValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  PutValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.b.l);
                  }
                break;
              case WORD_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.w = Pipe2.w;
                  _pc+=AdvPipe(_pc,2);
                  res3.w &= GetShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  PutShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.w);
                  }
                else {
                  res3.w = Pipe2.w;
                  _pc+=AdvPipe(_pc,2);
                  res3.w &= GetShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  PutShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.w);
                  }
                break;
              case DWORD_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.x = Pipe2.d;
                  _pc+=AdvPipe(_pc,4);
                  res3.x &= GetIntValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  PutIntValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.x);
                  }
                else {
                  res3.x = Pipe2.d;
                  _pc+=AdvPipe(_pc,4);
                  res3.x &= GetIntValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  PutIntValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.x);
                  }
                break;
              }
            _pc+=2;
            break;
          }
        goto aggFlag;
				break;

			case 0x04:   // SUBI
        switch(ADDRESSING) {
          case ABSOLUTEADD:
            switch(ABSOLUTEADD_SIZE) {
              case ABSSSUBADD:
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = Pipe2.b.l;
                    _pc+=AdvPipe(_pc,2);
                    res1.b.l = GetValue(Pipe2.w);
										res3.w = (WORD)res1.b.l - (WORD)res2.b.l;
                    PutValue(Pipe2.w, res3.b.l);
                    break;
                  case WORD_SIZE:
                    res2.w = Pipe2.w;
                    _pc+=AdvPipe(_pc,2);
                    res1.w = GetShortValue(Pipe2.w);
										res3.x = (DWORD)res1.w - (DWORD)res2.w;
                    PutShortValue(Pipe2.w, res3.w);
                    break;
                  case DWORD_SIZE:
                    res2.x = Pipe2.d;
                    _pc+=AdvPipe(_pc,4);
                    res1.x = GetIntValue(Pipe2.w);
										res3.x = res1.x - res2.x;
                    PutIntValue(Pipe2.w, res3.x);
                    break;
                  }
                _pc+=2;
                break;
              case ABSLSUBADD:
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = Pipe2.b.l;
                    _pc+=AdvPipe(_pc,2);
                    res1.b.l = GetValue(Pipe2.d);
										res3.w = (WORD)res1.b.l - (WORD)res2.b.l;
                    PutValue(Pipe2.d,res3.b.l);
                    break;
                  case WORD_SIZE:
                    res2.w = Pipe2.w;
                    _pc+=AdvPipe(_pc,2);
                    res1.w = GetShortValue(Pipe2.d);
										res3.x = (DWORD)res1.w - (DWORD)res2.w;
                    PutShortValue(Pipe2.d,res3.w);
                    break;
                  case DWORD_SIZE:
                    res2.x = Pipe2.d;
                    _pc+=AdvPipe(_pc,4);
                    res1.x = GetIntValue(Pipe2.d); 
										res3.x = res1.x - res2.x;
                    PutIntValue(Pipe2.d,res3.x);
                    break;
                  }
                _pc+=4;
                break;
              case PCIDXSUBADD:   // qua no!
              case PCDISPSUBADD:
              case IMMSUBADD:
                break;
              }
            break;
          case DATAREGADD:   // Dn
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res1.b.l = WORKING_REG_D.b.b0;
                res2.b.l = Pipe2.b.l;
                res3.w = (WORD)res1.b.l - (WORD)res2.b.l;
                WORKING_REG_D.b.b0 = res3.b.l;
		            _pc+=2;
                break;
              case WORD_SIZE:
                res1.w = WORKING_REG_D.w.l;
                res2.w = Pipe2.w;
                res3.x = (DWORD)res1.w - (DWORD)res2.w;
                WORKING_REG_D.w.l = res3.w;
		            _pc+=2;
                break;
              case DWORD_SIZE:
                res1.x = WORKING_REG_D.x;
                res2.x = Pipe2.d;
                res3.x = res1.x - res2.x;
                WORKING_REG_D.x = res3.x;
		            _pc+=4;
                break;
              }
            break;
          case ADDRREGADD:   // An
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res1.b.l = GetValue(WORKING_REG_A.x);
                res2.b.l = Pipe2.b.l;
                res3.w = (WORD)res1.b.l - (WORD)res2.b.l;
                PutValue(WORKING_REG_A.x, res3.b.l);
		            _pc+=2;
                break;
              case WORD_SIZE:
                res1.w = GetShortValue(WORKING_REG_A.x);
                res2.w = Pipe2.w;
                res3.x = (DWORD)res1.w - (DWORD)res2.w;
                PutShortValue(WORKING_REG_A.x, res3.w);
		            _pc+=2;
                break;
              case DWORD_SIZE:
                res1.x = GetIntValue(WORKING_REG_A.x);
                res2.x = Pipe2.d;
                res3.x = res1.x - res2.x;
                PutIntValue(WORKING_REG_A.x, res3.x);
		            _pc+=4;
                break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res1.b.l = GetValue(WORKING_REG_A.x);
                res2.b.l = Pipe2.b.l;
                res3.w = (WORD)res1.b.l - (WORD)res2.b.l;
                PutValue(WORKING_REG_A.x, res3.b.l);
                WORKING_REG_A.x++;
		            _pc+=2;
                break;
              case WORD_SIZE:
                res1.w = GetShortValue(WORKING_REG_A.x);
                res2.w = Pipe2.w;
                res3.x = (DWORD)res1.w - (DWORD)res2.w;
                PutShortValue(WORKING_REG_A.x, res3.w);
                WORKING_REG_A.x+=2;
		            _pc+=2;
                break;
              case DWORD_SIZE:
                res1.x = GetIntValue(WORKING_REG_A.x);
                res2.x = Pipe2.d;
                res3.x = res1.x - res2.x;
                PutIntValue(WORKING_REG_A.x, res3.x);
                WORKING_REG_A.x+=4;
		            _pc+=4;
                break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                WORKING_REG_A.x--;
                res1.b.l = GetValue(WORKING_REG_A.x);
                res2.b.l = Pipe2.b.l;
                res3.w = (WORD)res1.b.l - (WORD)res2.b.l;
                PutValue(WORKING_REG_A.x, res3.b.l);
		            _pc+=2;
                break;
              case WORD_SIZE:
                WORKING_REG_A.x-=2;
                res1.w = GetShortValue(WORKING_REG_A.x);
                res2.w = Pipe2.w;
                res3.x = (DWORD)res1.w - (DWORD)res2.w;
                PutShortValue(WORKING_REG_A.x, res3.w);
		            _pc+=2;
                break;
              case DWORD_SIZE:
                WORKING_REG_A.x-=4;
                res1.x = GetIntValue(WORKING_REG_A.x);
                res2.x = Pipe2.d;
                res3.x = res1.x - res2.x;
                PutIntValue(WORKING_REG_A.x, res3.x);
		            _pc+=4;
                break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res2.b.l = Pipe2.b.l;
                _pc+=AdvPipe(_pc,2);
                res1.b.l = GetValue(WORKING_REG_A.x - (signed short int)Pipe2.w);
                res3.w = (WORD)res1.b.l - (WORD)res2.b.l;
                PutValue(WORKING_REG_A.x + (signed short int)Pipe2.w, res3.b.l);
                break;
              case WORD_SIZE:
                res2.w = Pipe2.w;
                _pc+=AdvPipe(_pc,2);
                res1.w = GetValue(WORKING_REG_A.x - (signed short int)Pipe2.w);
                res3.x = (DWORD)res1.w - (DWORD)res2.w;
                PutShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w, res3.w);
                break;
              case DWORD_SIZE:
                res2.x = Pipe2.d;
                _pc+=AdvPipe(_pc,4);
                res1.x = GetIntValue(WORKING_REG_A.x - (signed short int)Pipe2.w);
                res3.x = res1.x - res2.x;
                PutIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w, res3.x);
                break;
              }
						_pc+=2;
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res2.b.l = Pipe2.b.l;
                  _pc+=AdvPipe(_pc,2);
                  res1.b.l = GetValue(WORKING_REG_A.x - (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
	                res3.w = (WORD)res1.b.l - (WORD)res2.b.l;
                  PutValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.b.l);
                  }
                else {
                  res2.b.l = Pipe2.b.l;
                  _pc+=AdvPipe(_pc,2);
                  res1.b.l = GetValue(WORKING_REG_A.x - (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
	                res3.w = (WORD)res1.b.l - (WORD)res2.b.l;
                  PutValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.b.l);
                  }
                break;
              case WORD_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res2.w = Pipe2.w;
                  _pc+=AdvPipe(_pc,2);
                  res1.w = GetShortValue(WORKING_REG_A.x - (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
	                res3.x = (DWORD)res1.w - (DWORD)res2.w;
                  PutShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.w);
                  }
                else {
                  res2.w = Pipe2.w;
                  _pc+=AdvPipe(_pc,2);
                  res1.w = GetShortValue(WORKING_REG_A.x - (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
	                res3.x = (DWORD)res1.w - (DWORD)res2.w;
                  PutShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.w);
                  }
                break;
              case DWORD_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res2.x = Pipe2.d;
                  _pc+=AdvPipe(_pc,4);
                  res1.x = GetIntValue(WORKING_REG_A.x - (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  res3.x = res1.x - res2.x;
	                PutIntValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.x);
                  }
                else {
                  res2.x = Pipe2.d;
                  _pc+=AdvPipe(_pc,4);
                  res1.x = GetIntValue(WORKING_REG_A.x - (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
	                res3.x = res1.x - res2.x;
                  PutIntValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.x);
                  }
                break;
              }
            _pc+=2;
            break;
          }
        
aggFlagS:
        switch(OPERAND_SIZE) {
          case BYTE_SIZE:
            _f.CCR.Zero=!res3.b.l;
            _f.CCR.Sign=!!(res3.b.l & 0x80);
            _f.CCR.Carry=!!(res3.b.h);
		        _f.CCR.Ovf = !!(((res1.b.l & 0x40) + (res2.b.l & 0x40)) & 0x80) != !!(((res1.w & 0x80) + (res2.w & 0x80)) & 0x100);
            break;
          case WORD_SIZE:
            _f.CCR.Zero=!res3.w;
            _f.CCR.Sign=!!(res3.w & 0x8000);
            _f.CCR.Carry=!!(HIWORD(res3.x));
            _f.CCR.Ovf = !!(((res1.b.h & 0x40) + (res2.b.h & 0x40)) & 0x80) != !!(((res1.x & 0x8000) + (res2.x & 0x8000)) & 0x10000);
            break;
          case DWORD_SIZE:
aggFlagS4:
            _f.CCR.Zero=!res3.x;
            _f.CCR.Sign=!!(res3.x & 0x80000000);
            _f.CCR.Carry=!!(((unsigned long long)res1.x - (unsigned long long)res2.x) & 0x8000000000000000ULL);
            _f.CCR.Ovf = !!((((res1.x & 0x40000000) + (res2.x & 0x40000000)) & 0x80000000) != 
                    !!((((res1.x >> 16) & 0x8000) + ((res2.x >> 16) & 0x8000)) & 0x10000));
            // credo sia meglio di usare QWORD longlong...
            break;
          }
        _f.CCR.Ext=_f.CCR.Carry;
				break;

			case 0x06:   // ADDI (RTM)
        switch(ADDRESSING) {
          case ABSOLUTEADD:
            switch(ABSOLUTEADD_SIZE) {
              case ABSSSUBADD:
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = Pipe2.b.l;
                    _pc+=AdvPipe(_pc,2);
                    res1.b.l = GetValue(Pipe2.w);
										res3.w = (WORD)res1.b.l + (WORD)res2.b.l;
                    PutValue(Pipe2.w, res3.b.l);
                    break;
                  case WORD_SIZE:
                    res2.w = Pipe2.w;
                    _pc+=AdvPipe(_pc,2);
                    res1.w = GetShortValue(Pipe2.w);
										res3.x = (DWORD)res1.w + (DWORD)res2.w;
                    PutShortValue(Pipe2.w, res3.w);
                    break;
                  case DWORD_SIZE:
                    res2.x = Pipe2.d;
                    _pc+=AdvPipe(_pc,4);
                    res1.x = GetIntValue(Pipe2.w);
										res3.x = res1.x + res2.x;
                    PutIntValue(Pipe2.w, res3.x);
                    break;
                  }
                _pc+=2;
                break;
              case ABSLSUBADD:
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = Pipe2.b.l;
                    _pc+=AdvPipe(_pc,2);
                    res1.b.l = GetValue(Pipe2.d);
										res3.w = (WORD)res1.b.l + (WORD)res2.b.l;;
                    PutValue(Pipe2.d,res3.b.l);
                    break;
                  case WORD_SIZE:
                    res2.w = Pipe2.w;
                    _pc+=AdvPipe(_pc,2);
                    res1.w = GetShortValue(Pipe2.d);
										res3.x = (DWORD)res1.w + (DWORD)res2.w;
                    PutShortValue(Pipe2.d,res3.w);
                    break;
                  case DWORD_SIZE:
                    res2.x = Pipe2.d;
                    _pc+=AdvPipe(_pc,4);
                    res1.x = GetIntValue(Pipe2.d); 
										res3.x = res1.x + res2.x;
                    PutIntValue(Pipe2.d,res3.x);
                    break;
                  }
                _pc+=4;
                break;
              case PCIDXSUBADD:   // qua no!
              case PCDISPSUBADD:
              case IMMSUBADD:
                break;
              }
            break;
          case DATAREGADD:   // Dn
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res1.b.l = WORKING_REG_D.b.b0;
                res2.b.l = Pipe2.b.l;
                res3.w = (WORD)res1.b.l + (WORD)res2.b.l;;
                WORKING_REG_D.b.b0 = res3.b.l;
		            _pc+=2;
                break;
              case WORD_SIZE:
                res1.w = WORKING_REG_D.w.l;
                res2.w = Pipe2.w;
                res3.x = (DWORD)res1.w + (DWORD)res2.w;
                WORKING_REG_D.w.l = res3.w;
		            _pc+=2;
                break;
              case DWORD_SIZE:
                res1.x = WORKING_REG_D.x;
                res2.x = Pipe2.d;
                res3.x = res1.x + res2.x;
                WORKING_REG_D.x = res3.x;
		            _pc+=4;
                break;
              }
            break;
          case ADDRREGADD:   // An
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res1.b.l = GetValue(WORKING_REG_A.x);
                res2.b.l = Pipe2.b.l;
                res3.w = (WORD)res1.b.l + (WORD)res2.b.l;;
                PutValue(WORKING_REG_A.x, res3.b.l);
		            _pc+=2;
                break;
              case WORD_SIZE:
                res1.w = GetShortValue(WORKING_REG_A.x);
                res2.w = Pipe2.w;
                res3.x = (DWORD)res1.w + (DWORD)res2.w;
                PutShortValue(WORKING_REG_A.x, res3.w);
		            _pc+=2;
                break;
              case DWORD_SIZE:
                res1.x = GetIntValue(WORKING_REG_A.x);
                res2.x = Pipe2.d;
                res3.x = res1.x + res2.x;
                PutIntValue(WORKING_REG_A.x, res3.x);
		            _pc+=4;
                break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res1.b.l = GetValue(WORKING_REG_A.x);
                res2.b.l = Pipe2.b.l;
                res3.w = (WORD)res1.b.l + (WORD)res2.b.l;;
                PutValue(WORKING_REG_A.x, res3.b.l);
                WORKING_REG_A.x++;
		            _pc+=2;
                break;
              case WORD_SIZE:
                res1.w = GetShortValue(WORKING_REG_A.x);
                res2.w = Pipe2.w;
                res3.x = (DWORD)res1.w + (DWORD)res2.w;
                PutShortValue(WORKING_REG_A.x, res3.w);
                WORKING_REG_A.x+=2;
		            _pc+=2;
                break;
              case DWORD_SIZE:
                res1.x = GetIntValue(WORKING_REG_A.x);
                res2.x = Pipe2.d;
                res3.x = res1.x + res2.x;
                PutIntValue(WORKING_REG_A.x, res3.x);
                WORKING_REG_A.x+=4;
		            _pc+=4;
                break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                WORKING_REG_A.x--;
                res1.b.l = GetValue(WORKING_REG_A.x);
                res2.b.l = Pipe2.b.l;
                res3.w = (WORD)res1.b.l + (WORD)res2.b.l;;
                PutValue(WORKING_REG_A.x, res3.b.l);
		            _pc+=2;
                break;
              case WORD_SIZE:
                WORKING_REG_A.x-=2;
                res1.w = GetShortValue(WORKING_REG_A.x);
                res2.w = Pipe2.w;
                res3.x = (DWORD)res1.w + (DWORD)res2.w;
                PutShortValue(WORKING_REG_A.x, res3.w);
		            _pc+=2;
                break;
              case DWORD_SIZE:
                WORKING_REG_A.x-=4;
                res1.x = GetIntValue(WORKING_REG_A.x);
                res2.x = Pipe2.d;
                res3.x = res1.x + res2.x;
                PutIntValue(WORKING_REG_A.x, res3.x);
		            _pc+=4;
                break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res2.b.l = Pipe2.b.l;
                _pc+=AdvPipe(_pc,2);
                res1.b.l = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                res3.w = (WORD)res1.b.l + (WORD)res2.b.l;;
                PutValue(WORKING_REG_A.x + (signed short int)Pipe2.w, res3.b.l);
                break;
              case WORD_SIZE:
                res2.w = Pipe2.w;
                _pc+=AdvPipe(_pc,2);
                res1.w = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                res3.x = (DWORD)res1.w + (DWORD)res2.w;
                PutShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w, res3.w);
                break;
              case DWORD_SIZE:
                res2.x = Pipe2.d;
                _pc+=AdvPipe(_pc,4);
                res1.x = GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                res3.x = res1.x + res2.x;
                PutIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w, res3.x);
                break;
              }
						_pc+=2;
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res2.b.l = Pipe2.b.l;
                  _pc+=AdvPipe(_pc,2);
                  res1.b.l = GetValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
	                res3.w = (WORD)res1.b.l + (WORD)res2.b.l;;
                  PutValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.b.l);
                  }
                else {
                  res2.b.l = Pipe2.b.l;
                  _pc+=AdvPipe(_pc,2);
                  res1.b.l = GetValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
	                res3.w = (WORD)res1.b.l + (WORD)res2.b.l;;
                  PutValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.b.l);
                  }
                break;
              case WORD_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res2.w = Pipe2.w;
                  _pc+=AdvPipe(_pc,2);
                  res1.w = GetShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
	                res3.x = (DWORD)res1.w + (DWORD)res2.w;
                  PutShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.w);
                  }
                else {
                  res2.w = Pipe2.w;
                  _pc+=AdvPipe(_pc,2);
                  res1.w = GetShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
	                res3.x = (DWORD)res1.w + (DWORD)res2.w;
                  PutShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.w);
                  }
                break;
              case DWORD_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res2.x = Pipe2.d;
                  _pc+=AdvPipe(_pc,4);
                  res1.x = GetIntValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  res3.x = res1.x + res2.x;
	                PutIntValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.x);
                  }
                else {
                  res2.x = Pipe2.d;
                  _pc+=AdvPipe(_pc,4);
                  res1.x = GetIntValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
	                res3.x = res1.x + res2.x;
                  PutIntValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.x);
                  }
                break;
              }
            _pc+=2;
            break;
          }
        
aggFlagA:
        switch(OPERAND_SIZE) {
          case BYTE_SIZE:
            _f.CCR.Zero=!res3.b.l;
            _f.CCR.Sign=!!(res3.b.l & 0x80);
            _f.CCR.Carry=!!(res3.b.h);
		        _f.CCR.Ovf = !!(((res1.b.l & 0x40) + (res2.b.l & 0x40)) & 0x80) != !!(((res1.w & 0x80) + (res2.w & 0x80)) & 0x100);
            break;
          case WORD_SIZE:
            _f.CCR.Zero=!res3.w;
            _f.CCR.Sign=!!(res3.w & 0x8000);
            _f.CCR.Carry=!!(HIWORD(res3.x));
            _f.CCR.Ovf = !!(((res1.b.h & 0x40) + (res2.b.h & 0x40)) & 0x80) != !!(((res1.x & 0x8000) + (res2.x & 0x8000)) & 0x10000);
            break;
          case DWORD_SIZE:
aggFlagA4:
            _f.CCR.Zero=!res3.x;
            _f.CCR.Sign=!!(res3.x & 0x80000000);
            _f.CCR.Carry=!!(((unsigned long long)res1.x + (unsigned long long)res2.x) & 0x8000000000000000ULL);
            _f.CCR.Ovf = !!((((res1.x & 0x40000000) + (res2.x & 0x40000000)) & 0x80000000) != 
                    !!((((res1.x >> 16) & 0x8000) + ((res2.x >> 16) & 0x8000)) & 0x10000));
            // credo sia meglio di usare QWORD longlong...
            break;
          }
        _f.CCR.Ext=_f.CCR.Carry;
				break;

			case 0x0a:   // EORI
        switch(ADDRESSING) {
          case ABSOLUTEADD:
            switch(LOBYTE(Pipe1)) {
              case 0x3c:
                _f.CCR.b ^= Pipe2.b.l;
      					_pc+=2;
                goto noAggFlag;
                break;
              case 0x7c:
      					_pc+=2;
                if(_f.SR.Supervisor) {
                  res3.w = _f.SR.w ^ Pipe2.w;
			            goto aggSR;
                  }
                else {
                  res3.b.l=8;
                  goto doTrap;
                  }
                break;
              default:
                switch(ABSOLUTEADD_SIZE) {
                  case ABSSSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res3.b.l = Pipe2.b.l;
                        _pc+=AdvPipe(_pc,2);
                        res3.b.l ^= GetValue(Pipe2.w);
                        PutValue(Pipe2.w, res3.b.l);
                        break;
                      case WORD_SIZE:
                        res3.w = Pipe2.w;
                        _pc+=AdvPipe(_pc,2);
                        res3.w ^= GetShortValue(Pipe2.w);
                        PutShortValue(Pipe2.w, res3.w);
                        break;
                      case DWORD_SIZE:
                        res3.x = Pipe2.d;
                        _pc+=AdvPipe(_pc,4);
                        res3.x ^= GetIntValue(Pipe2.w);
                        PutIntValue(LOWORD(Pipe2.dd[1]), res3.x);
                        break;
                      }
                    _pc+=2;
                    break;
                  case ABSLSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res3.b.l = Pipe2.b.l;
                        _pc+=AdvPipe(_pc,2);
                        res3.b.l ^= GetValue(Pipe2.d);
                        PutValue(Pipe2.d,res3.b.l);
                        break;
                      case WORD_SIZE:
                        res3.w = Pipe2.w;
                        _pc+=AdvPipe(_pc,2);
                        res3.w ^= GetShortValue(Pipe2.d);
                        PutShortValue(Pipe2.d,res3.w);
                        break;
                      case DWORD_SIZE:
                        res3.x = Pipe2.d;
                        _pc+=AdvPipe(_pc,4);
                        res3.x ^= GetIntValue(Pipe2.d); 
                        PutIntValue(Pipe2.d,res3.x);
                        break;
                      }
                    _pc+=4;
                    break;
                  case PCIDXSUBADD:   // qua no!
                  case PCDISPSUBADD:
                  case IMMSUBADD:
                    break;
                  }
                break;
              }
            break;
          case DATAREGADD:   // Dn
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = WORKING_REG_D.b.b0 ^= Pipe2.b.l;
								_pc+=2;
                break;
              case WORD_SIZE:
                res3.w = WORKING_REG_D.w.l ^= Pipe2.w;
								_pc+=2;
                break;
              case DWORD_SIZE:
                res3.x = WORKING_REG_D.x ^= Pipe2.d;
								_pc+=4;
                break;
              }
            break;
          case ADDRREGADD:   // An	NO
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GetValue(WORKING_REG_A.x) ^ Pipe2.b.l;
                PutValue(WORKING_REG_A.x, res3.b.l);
								_pc+=2;
                break;
              case WORD_SIZE:
                res3.w = GetShortValue(WORKING_REG_A.x) ^ Pipe2.w;
                PutShortValue(WORKING_REG_A.x, res3.w);
								_pc+=2;
                break;
              case DWORD_SIZE:
                res3.x = GetIntValue(WORKING_REG_A.x) ^ Pipe2.d;
                PutIntValue(WORKING_REG_A.x, res3.x);
								_pc+=4;
                break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GetValue(WORKING_REG_A.x) ^ Pipe2.b.l;
                PutValue(WORKING_REG_A.x, res3.b.l);
                WORKING_REG_A.x++;
								_pc+=2;
                break;
              case WORD_SIZE:
                res3.w = GetShortValue(WORKING_REG_A.x) ^ Pipe2.w;
                PutShortValue(WORKING_REG_A.x, res3.w);
								_pc+=2;
                break;
              case DWORD_SIZE:
                res3.x = GetIntValue(WORKING_REG_A.x) ^ Pipe2.d;
                PutIntValue(WORKING_REG_A.x, res3.x);
                WORKING_REG_A.x+=4;
								_pc+=4;
                break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                WORKING_REG_A.x--;
                res3.b.l = GetValue(WORKING_REG_A.x) ^ Pipe2.b.l;
                PutValue(WORKING_REG_A.x, res3.b.l);
								_pc+=2;
                break;
              case WORD_SIZE:
                WORKING_REG_A.x-=2;
                res3.w = GetShortValue(WORKING_REG_A.x) ^ Pipe2.w;
                PutShortValue(WORKING_REG_A.x, res3.w);
								_pc+=2;
                break;
              case DWORD_SIZE:
                WORKING_REG_A.x-=4;
                res3.x = GetIntValue(WORKING_REG_A.x) ^ Pipe2.d;
                PutIntValue(WORKING_REG_A.x, res3.x);
								_pc+=4;
                break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = Pipe2.b.l;
                _pc+=AdvPipe(_pc,2);
                res3.b.l ^= GetValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                PutValue(WORKING_REG_A.x + (signed short int)Pipe2.w, res3.b.l);
                break;
              case WORD_SIZE:
                res3.w = Pipe2.w;
                _pc+=AdvPipe(_pc,2);
                res3.w ^= GetValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                PutShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w, res3.w);
                break;
              case DWORD_SIZE:
                res3.x = Pipe2.d;
                _pc+=AdvPipe(_pc,4);
                res3.x ^= GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                PutIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w, res3.x);
                break;
              }
						_pc+=2;
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.b.l = Pipe2.b.l;
                  _pc+=AdvPipe(_pc,2);
                  res3.b.l ^= GetValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  PutValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.b.l);
                  }
                else {
                  res3.b.l = Pipe2.b.l;
                  _pc+=AdvPipe(_pc,2);
                  res3.b.l ^= GetValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  PutValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.b.l);
                  }
                break;
              case WORD_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.w = Pipe2.w;
                  _pc+=AdvPipe(_pc,2);
                  res3.w ^= GetShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  PutShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.w);
                  }
                else {
                  res3.w = Pipe2.w;
                  _pc+=AdvPipe(_pc,2);
                  res3.w ^= GetShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  PutShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.w);
                  }
                break;
              case DWORD_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.x = Pipe2.d;
                  _pc+=AdvPipe(_pc,4);
                  res3.x ^= GetIntValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  PutIntValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.x);
                  }
                else {
                  res3.x = Pipe2.d;
                  _pc+=AdvPipe(_pc,4);
                  res3.x ^= GetIntValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  PutIntValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.x);
                  }
                break;
              }
            _pc+=2;
            break;
          }
        goto aggFlag;
				break;

			case 0x0c:   // CMPI
        switch(ADDRESSING) {
          case ABSOLUTEADD:
            switch(ABSOLUTEADD_SIZE) {
              case ABSSSUBADD:
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = Pipe2.b.l;
                    _pc+=AdvPipe(_pc,2);
                    res1.b.l = GetValue(Pipe2.w);
                    break;
                  case WORD_SIZE:
                    res2.w = Pipe2.w;
                    _pc+=AdvPipe(_pc,2);
                    res1.w = GetShortValue(Pipe2.w);
                    break;
                  case DWORD_SIZE:
                    res2.x = Pipe2.d;
                    _pc+=AdvPipe(_pc,4);
                    res1.x = GetIntValue(Pipe2.w);
                    break;
                  }
                _pc+=2;
                break;
              case ABSLSUBADD:
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = Pipe2.b.l;
                    _pc+=AdvPipe(_pc,2);
                    res1.b.l = GetValue(Pipe2.d);
                    break;
                  case WORD_SIZE:
                    res2.w = Pipe2.w;
                    _pc+=AdvPipe(_pc,2);
                    res1.w = GetShortValue(Pipe2.d);
                    break;
                  case DWORD_SIZE:
                    res2.x = Pipe2.d;
                    _pc+=AdvPipe(_pc,4);
                    res1.x = GetIntValue(Pipe2.d); 
                    break;
                  }
                _pc+=4;
                break;
              case PCIDXSUBADD:     // qua no!
              case PCDISPSUBADD:
              case IMMSUBADD:
                break;
              }
            break;
          case DATAREGADD:   // Dn
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res1.b.l = WORKING_REG_D.b.b0;
                res2.b.l = Pipe2.b.l;
                _pc+=2;
                break;
              case WORD_SIZE:
                res1.w = WORKING_REG_D.w.l;
                res2.w = Pipe2.w;
                _pc+=2;
                break;
              case DWORD_SIZE:
                res1.x = WORKING_REG_D.x;
                res2.x = Pipe2.d;
                _pc+=4;
                break;
              }
            break;
          case ADDRREGADD:   // An	NO
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res1.b.l = GetValue(WORKING_REG_A.x);
                res2.b.l = Pipe2.b.l;
                _pc+=2;
                break;
              case WORD_SIZE:
                res1.w = GetShortValue(WORKING_REG_A.x);
                res2.w = Pipe2.w;
                _pc+=2;
                break;
              case DWORD_SIZE:
                res1.x = GetIntValue(WORKING_REG_A.x);
                res2.x = Pipe2.d;
								_pc+=4;
                break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res1.b.l = GetValue(WORKING_REG_A.x);
                res2.b.l = Pipe2.b.l;
                WORKING_REG_A.x++;
                _pc+=2;
                break;
              case WORD_SIZE:
                res1.w = GetShortValue(WORKING_REG_A.x);
                res2.w = Pipe2.w;
                WORKING_REG_A.x+=2;
                _pc+=2;
                break;
              case DWORD_SIZE:
                res1.x = GetIntValue(WORKING_REG_A.x);
                res2.x = Pipe2.d;
                WORKING_REG_A.x+=4;
		            _pc+=4;
                break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                WORKING_REG_A.x--;
                res1.b.l = GetValue(WORKING_REG_A.x);
                res2.b.l = Pipe2.b.l;
                _pc+=2;
                break;
              case WORD_SIZE:
                WORKING_REG_A.x-=2;
                res1.w = GetShortValue(WORKING_REG_A.x);
                res2.w = Pipe2.w;
                _pc+=2;
                break;
              case DWORD_SIZE:
                WORKING_REG_A.x-=4;
                res1.x = GetIntValue(WORKING_REG_A.x);
                res2.x = Pipe2.d;
								_pc+=4;
                break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res2.b.l = Pipe2.b.l;
                _pc+=AdvPipe(_pc,2);
                res1.b.l = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                break;
              case WORD_SIZE:
                res2.w = Pipe2.w;
                _pc+=AdvPipe(_pc,2);
                res1.w = GetShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                break;
              case DWORD_SIZE:
                res2.x = Pipe2.d;
                _pc+=AdvPipe(_pc,4);
                res1.x = GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                break;
              }
						_pc+=2;
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                if(!DISPLACEMENT_SIZE) {
									res2.b.l = Pipe2.b.l;
                  _pc+=AdvPipe(_pc,2);
									res1.b.l = GetValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
									}
                else {
									res2.b.l = Pipe2.b.l;
                  _pc+=AdvPipe(_pc,2);
									res1.b.l = GetValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  }
                break;
              case WORD_SIZE:
                if(!DISPLACEMENT_SIZE) {
									res2.w = Pipe2.w;
                  _pc+=AdvPipe(_pc,2);
									res1.w = GetValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
									}
                else {
									res2.w = Pipe2.w;
                  _pc+=AdvPipe(_pc,2);
									res1.w = GetValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  }
                break;
              case DWORD_SIZE:
                if(!DISPLACEMENT_SIZE) {
									res2.x = Pipe2.d;
                  _pc+=AdvPipe(_pc,4);
									res1.x = GetValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
									}
                else {
									res2.x = Pipe2.d;
                  _pc+=AdvPipe(_pc,4);
									res1.x = GetValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  }
                break;
              }
						_pc+=2;
            break;
          }
        switch(OPERAND_SIZE) {
          case BYTE_SIZE:
            res3.w=(WORD)res1.b.l-(WORD)res2.b.l;
            break;
          case WORD_SIZE:
            res3.x=(DWORD)res1.w-(DWORD)res2.w;
            break;
          case DWORD_SIZE:
            res3.x=res1.x-res2.x;
            break;
          }
        
aggFlagC:
        switch(OPERAND_SIZE) {
          case BYTE_SIZE:
            _f.CCR.Zero=!res3.b.l;
            _f.CCR.Sign=!!(res3.b.l & 0x80);
            _f.CCR.Carry=!!(res3.b.h);
		        _f.CCR.Ovf = !!(((res1.b.l & 0x40) + (res2.b.l & 0x40)) & 0x80) != !!(((res1.w & 0x80) + (res2.w & 0x80)) & 0x100);
            break;
          case WORD_SIZE:
            _f.CCR.Zero=!res3.w;
            _f.CCR.Sign=!!(res3.w & 0x8000);
            _f.CCR.Carry=!!(HIWORD(res3.x));
            _f.CCR.Ovf = !!(((res1.b.h & 0x40) + (res2.b.h & 0x40)) & 0x80) != !!(((res1.x & 0x8000) + (res2.x & 0x8000)) & 0x10000);
            break;
          case DWORD_SIZE:
aggFlagC4:
            _f.CCR.Zero=!res3.x;
            _f.CCR.Sign=!!(res3.x & 0x80000000);
            _f.CCR.Carry=!!(((unsigned long long)res1.x - (unsigned long long)res2.x) & 0x8000000000000000ULL);
            _f.CCR.Ovf = !!((((res1.x & 0x40000000) + (res2.x & 0x40000000)) & 0x80000000) != 
                    !!((((res1.x >> 16) & 0x8000) + ((res2.x >> 16) & 0x8000)) & 0x10000));
            // credo sia meglio di usare QWORD longlong...
            break;
          }
// qua X non toccato
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
          case ABSOLUTEADD:
            switch(ABSOLUTEADD_SIZE) {
              case ABSSSUBADD:
                res3.b.l = GetValue(Pipe2.w);
                _pc+=AdvPipe(_pc,2);
                break;
              case ABSLSUBADD:
                res3.b.l = GetValue(Pipe2.d);
                _pc+=AdvPipe(_pc,4);
                break;
              case PCIDXSUBADD:   // (D16,PC)
                res3.b.l = GetValue(_pc+(signed short int)Pipe2.w);
                _pc+=AdvPipe(_pc,2);
                break;
              case PCDISPSUBADD:   // (D8,Xn,PC)
                if(!DISPLACEMENT_SIZE)
                  res3.b.l = GetValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                else
                  res3.b.l = GetValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                _pc+=AdvPipe(_pc,2);
                break;
              case IMMSUBADD:
                res3.b.l = Pipe2.b.l;
                _pc+=AdvPipe(_pc,2);
                break;
              }
            break;
          case DATAREGADD:   // Dn
            res3.b.l = WORKING_REG_D.b.b0;
            break;
          case ADDRREGADD:   // An
            break;
          case ADDRADD:   // (An)
            res3.b.l = GetValue(WORKING_REG_A.x);
            break;
          case ADDRPINCADD:   // (An)+
            res3.b.l = GetValue(WORKING_REG_A.x);
            WORKING_REG_A.x++;
            break;
          case ADDRPDECADD:   // -(An)
            WORKING_REG_A.x--;
            res3.b.l = GetValue(WORKING_REG_A.x);
            break;
          case ADDRDISPADD:   // (D16,An)
            res3.b.l = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
            _pc+=AdvPipe(_pc,2);
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            if(!DISPLACEMENT_SIZE) {
              res3.b.l = GetValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
              }
            else {
              res3.b.l = GetValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
              }
            _pc+=AdvPipe(_pc,2);
            break;
          }
        switch(ADDRESSING2) {
          case ABSOLUTEADD:
            switch(ABSOLUTEADD_SIZE2) {
              case ABSSSUBADD:
                PutValue(Pipe2.w,res3.b.l);
                _pc+=2;
                break;
              case ABSLSUBADD:
                PutValue(Pipe2.d,res3.b.l);
                _pc+=4;
                break;
              case PCIDXSUBADD:   // (D16,PC) qua no!
              case PCDISPSUBADD:   // (D8,Xn,PC) qua no!
              case IMMSUBADD:
                break;
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
            PutValue(DEST_REG_A.x + (signed short int)Pipe2.w,res3.b.l);
            _pc+=2;
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            if(!DISPLACEMENT_SIZE)
              PutValue(DEST_REG_A.x + (signed char)DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l,res3.b.l);
            else
              PutValue(DEST_REG_A.x + (signed char)DISPLACEMENT_REG.x + (signed char)Pipe2.b.l,res3.b.l);
            _pc+=2;
            break;
          }
        goto aggFlag1;
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
          case ABSOLUTEADD:
            switch(ABSOLUTEADD_SIZE) {
              case ABSSSUBADD:
                res3.x = GetIntValue(Pipe2.w);
                _pc+=AdvPipe(_pc,2);
                break;
              case ABSLSUBADD:
                res3.x = GetIntValue(Pipe2.d); 
                _pc+=AdvPipe(_pc,4);
                break;
              case PCIDXSUBADD:   // (D16,PC)
                res3.x = GetIntValue(_pc+(signed short int)Pipe2.w);
                _pc+=AdvPipe(_pc,2);
	              break;
              case PCDISPSUBADD:   // (D8,Xn,PC)
                if(!DISPLACEMENT_SIZE)
                  res3.x = GetIntValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                else
                  res3.x = GetIntValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                _pc+=AdvPipe(_pc,2);
                break;
              case IMMSUBADD:
                res3.x = Pipe2.d; 
                _pc+=AdvPipe(_pc,4);
                break;
              }
            break;
          case DATAREGADD:   // Dn
            res3.x = WORKING_REG_D.x;
            break;
          case ADDRREGADD:   // An
            res3.x = WORKING_REG_A.x;
            break;
          case ADDRADD:   // (An)
            res3.x = GetIntValue(WORKING_REG_A.x);
            break;
          case ADDRPINCADD:   // (An)+
            res3.x = GetIntValue(WORKING_REG_A.x);
            WORKING_REG_A.x+=4;
            break;
          case ADDRPDECADD:   // -(An)
            WORKING_REG_A.x-=4;
            res3.x = GetIntValue(WORKING_REG_A.x);
            break;
          case ADDRDISPADD:   // (D16,An)
            res3.x = GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
            _pc+=AdvPipe(_pc,2);
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            if(!DISPLACEMENT_SIZE) {
              res3.x = GetIntValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
              }
            else {
              res3.x = GetIntValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
              }
            _pc+=AdvPipe(_pc,2);
            break;
          }
        switch(ADDRESSING2) {
          case ABSOLUTEADD:
            switch(ABSOLUTEADD_SIZE) {
              case ABSSSUBADD:
                PutIntValue(Pipe2.w,res3.x);
                _pc+=2;
                break;
              case ABSLSUBADD:
                PutIntValue(Pipe2.d,res3.x);
                _pc+=4;
                break;
              case PCIDXSUBADD:   // (D16,PC) qua no!
              case PCDISPSUBADD:   // (D8,Xn,PC) qua no!
              case IMMSUBADD:
                break;
              }
            break;
          case DATAREGADD:   // Dn
            DEST_REG_D.x = res3.x;
            break;
          case ADDRREGADD:   // An
						DEST_REG_A.x = res3.x;    // MOVEA
            goto noAggFlag;
            break;
          case ADDRADD:   // (An)
            PutIntValue(DEST_REG_A.x,res3.x);
            break;
          case ADDRPINCADD:   // (An)+
            PutIntValue(DEST_REG_A.x,res3.x);
            DEST_REG_A.x+=4;
            break;
          case ADDRPDECADD:   // -(An)
            DEST_REG_A.x-=4;
            PutIntValue(DEST_REG_A.x,res3.x);
            break;
          case ADDRDISPADD:   // (D16,An)
            PutIntValue(DEST_REG_A.x + (signed short int)Pipe2.w,res3.x);
            _pc+=2;
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            if(!DISPLACEMENT_SIZE)
              PutIntValue(DEST_REG_A.x + (signed char)DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l,res3.x);
            else
              PutIntValue(DEST_REG_A.x + (signed char)DISPLACEMENT_REG.x + (signed char)Pipe2.b.l,res3.x);
            _pc+=2;
            break;
          }
        goto aggFlag4;
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
          case ABSOLUTEADD:
            switch(ABSOLUTEADD_SIZE) {
              case ABSSSUBADD:
	              res3.w = GetShortValue(Pipe2.w);
                _pc+=AdvPipe(_pc,2);
                break;
              case ABSLSUBADD:
		            res3.w = GetShortValue(Pipe2.w);
                _pc+=AdvPipe(_pc,4);
                break;
              case PCIDXSUBADD:   // (D16,PC)
                res3.w = GetShortValue(_pc+(signed short int)Pipe2.w);
                _pc+=AdvPipe(_pc,2);
                break;
              case PCDISPSUBADD:   // (D8,Xn,PC)
                if(!DISPLACEMENT_SIZE)
                  res3.w = GetShortValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                else
                  res3.w = GetShortValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                _pc+=AdvPipe(_pc,2);
                break;
              case IMMSUBADD:
                res3.x = Pipe2.w; 
                _pc+=AdvPipe(_pc,2);
                break;
              }
            break;
          case DATAREGADD:   // Dn
            res3.w = WORKING_REG_D.w.l;
            break;
          case ADDRREGADD:   // An
						// qua no!
            break;
          case ADDRADD:   // (An)
            res3.w = GetShortValue(WORKING_REG_A.x);
            break;
          case ADDRPINCADD:   // (An)+
            res3.w = GetShortValue(WORKING_REG_A.x);
            WORKING_REG_A.x+=2;
            break;
          case ADDRPDECADD:   // -(An)
            WORKING_REG_A.x-=2;
            res3.w = GetShortValue(WORKING_REG_A.x);
            break;
          case ADDRDISPADD:   // (D16,An)
            res3.w = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
            _pc+=AdvPipe(_pc,2);
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            if(!DISPLACEMENT_SIZE) {
              res3.w = GetShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
              }
            else {
              res3.w = GetShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
              }
            _pc+=AdvPipe(_pc,2);
            break;
          }
        switch(ADDRESSING2) {
          case ABSOLUTEADD:
            switch(ABSOLUTEADD_SIZE2) {
              case ABSSSUBADD:
                PutShortValue(Pipe2.w,res3.w);
                _pc+=2;
                break;
              case ABSLSUBADD:
                PutShortValue(Pipe2.d,res3.w);
                _pc+=4;
                break;
              case PCIDXSUBADD:   // (D16,PC) qua no!
              case PCDISPSUBADD:   // (D8,Xn,PC) qua no!
              case IMMSUBADD:
                break;
              }
            break;
          case DATAREGADD:   // Dn
            DEST_REG_D.w.l = res3.w;
            break;
          case ADDRREGADD:   // An
            DEST_REG_A.x = (signed short int)res3.w;    // MOVEA
            goto noAggFlag;
            break;
          case ADDRADD:   // (An)
            PutShortValue(DEST_REG_A.x,res3.w);
            break;
          case ADDRPINCADD:   // (An)+
            PutShortValue(DEST_REG_A.x,res3.w);
            DEST_REG_A.x+=2;
            break;
          case ADDRPDECADD:   // -(An)
            DEST_REG_A.x-=2;
            PutShortValue(DEST_REG_A.x,res3.w);
            break;
          case ADDRDISPADD:   // (D16,An)
            PutShortValue(DEST_REG_A.x + (signed short int)Pipe2.w,res3.w);
            _pc+=2;
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            if(!DISPLACEMENT_SIZE)
              PutShortValue(DEST_REG_A.x + (signed char)DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l,res3.w);
            else
              PutShortValue(DEST_REG_A.x + (signed char)DISPLACEMENT_REG.x + (signed char)Pipe2.b.l,res3.w);
            _pc+=2;
            break;
          }
        goto aggFlag2;
				break;
        
			case 0x40:   // MOVE from SR, NEGX
        switch(ADDRESSING) {
          case ABSOLUTEADD:
            switch(ABSOLUTEADD_SIZE) {
              case ABSSSUBADD:
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = 0;
                    res2.b.l = GetValue(Pipe2.w) + _f.CCR.Ext;
                    res3.w = (WORD)res1.b.l - (WORD)res2.b.l;
                    PutValue(Pipe2.w, res3.b.l);
                    _pc+=2;
                    break;
                  case WORD_SIZE:
                    res1.w = 0;
                    res2.w = GetShortValue(Pipe2.w) + _f.CCR.Ext;
                    res3.x = (DWORD)res1.w - (DWORD)res2.w;
                    PutShortValue(Pipe2.w, res3.w);
                    _pc+=2;
                    break;
                  case DWORD_SIZE:
                    res1.x = 0;
                    res2.x = GetIntValue(Pipe2.w) + _f.CCR.Ext;
                    res3.x = res1.x - res2.x;
                    PutIntValue(Pipe2.w, res3.x);
                    _pc+=2;
                    break;
                  case SIZE_ELSE:
                    PutShortValue(Pipe2.w,_f.SR.w);
                    _pc+=2;
                    goto noAggFlag;
                    break;
                  }
                break;
              case ABSLSUBADD:
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = 0;
                    res2.b.l = GetValue(Pipe2.d) + _f.CCR.Ext;
                    res3.w = (WORD)res1.b.l - (WORD)res2.b.l;
                    PutValue(Pipe2.d,res3.b.l);
                    _pc+=4;
                    break;
                  case WORD_SIZE:
                    res1.w = 0;
                    res2.w = GetShortValue(Pipe2.d) + _f.CCR.Ext;
                    res3.x = (DWORD)res1.w - (DWORD)res2.w;
                    PutShortValue(Pipe2.d,res3.w);
                    _pc+=4;
                    break;
                  case DWORD_SIZE:
                    res1.x = 0;
                    res2.x = GetIntValue(Pipe2.d) + _f.CCR.Ext;
                    res3.x = res1.x - res2.x;
                    PutIntValue(Pipe2.d,res3.x);
                    _pc+=4;
                    break;
                  case SIZE_ELSE:
                    PutShortValue(Pipe2.d,_f.SR.w);
                    _pc+=4;
                    goto noAggFlag;
                    break;
                  }
                break;
              case PCIDXSUBADD:   // qua no!
              case PCDISPSUBADD:
              case IMMSUBADD:
                break;
              }
            break;
          case DATAREGADD:   // Dn
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res1.b.l = 0;
                res2.b.l = WORKING_REG_D.b.b0 + _f.CCR.Ext;
                res3.w = (WORD)res1.b.l - (WORD)res2.b.l;
                WORKING_REG_D.b.b0 = res3.b.l;
                break;
              case WORD_SIZE:
                res1.w = 0;
                res2.w = WORKING_REG_D.w.l + _f.CCR.Ext;
                res3.x = (DWORD)res1.w - (DWORD)res2.w;
                WORKING_REG_D.w.l = res3.w;
                break;
              case DWORD_SIZE:
                res1.x = 0;
                res2.x = WORKING_REG_D.x + _f.CCR.Ext;
                res3.x = res1.x - res2.x;
                WORKING_REG_D.x = res3.x;
                break;
		          case SIZE_ELSE:
                WORKING_REG_D.w.l = _f.SR.w;
								goto noAggFlag;
			          break;
              }
            break;
          case ADDRREGADD:   // An
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res1.b.l = 0;
                res2.b.l = GetValue(WORKING_REG_A.x) + _f.CCR.Ext;
                res3.w = (WORD)res1.b.l - (WORD)res2.b.l;
                PutValue(WORKING_REG_A.x, res3.b.l);
                break;
              case WORD_SIZE:
                res1.w = 0;
                res2.w = GetShortValue(WORKING_REG_A.x) + _f.CCR.Ext;
                res3.x = (DWORD)res1.w - (DWORD)res2.w;
                PutShortValue(WORKING_REG_A.x, res3.w);
                break;
              case DWORD_SIZE:
                res1.x = 0;
                res2.x = GetIntValue(WORKING_REG_A.x) + _f.CCR.Ext;
                res3.x = res1.x - res2.x;
                PutIntValue(WORKING_REG_A.x, res3.x);
                break;
		          case SIZE_ELSE:
                PutShortValue(WORKING_REG_A.x, _f.SR.w);
								goto noAggFlag;
			          break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res1.b.l = 0;
                res2.b.l = GetValue(WORKING_REG_A.x) + _f.CCR.Ext;
                res3.w = (WORD)res1.b.l - (WORD)res2.b.l;
                PutValue(WORKING_REG_A.x, res3.b.l);
                WORKING_REG_A.x++;
                break;
              case WORD_SIZE:
                res1.w = 0;
                res2.w = GetShortValue(WORKING_REG_A.x) + _f.CCR.Ext;
                res3.x = (DWORD)res1.w - (DWORD)res2.w;
                PutShortValue(WORKING_REG_A.x, res3.w);
                WORKING_REG_A.x+=2;
                break;
              case DWORD_SIZE:
                res1.x = 0;
                res2.x = GetIntValue(WORKING_REG_A.x) + _f.CCR.Ext;
                res3.x = res1.x - res2.x;
                PutIntValue(WORKING_REG_A.x, res3.x);
                WORKING_REG_A.x+=4;
                break;
		          case SIZE_ELSE:
                PutShortValue(WORKING_REG_A.x, _f.SR.w);
								goto noAggFlag;
			          break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                WORKING_REG_A.x--;
                res1.b.l = 0;
                res2.b.l = GetValue(WORKING_REG_A.x) + _f.CCR.Ext;
                res3.w = (WORD)res1.b.l - (WORD)res2.b.l;
                PutValue(WORKING_REG_A.x, res3.b.l);
                break;
              case WORD_SIZE:
                WORKING_REG_A.x-=2;
                res1.w = 0;
                res2.w = GetShortValue(WORKING_REG_A.x) + _f.CCR.Ext;
                res3.w = (DWORD)res1.w - (DWORD)res2.w;
                PutShortValue(WORKING_REG_A.x, res3.w);
                break;
              case DWORD_SIZE:
                WORKING_REG_A.x-=4;
                res1.x = 0;
                res2.x = GetIntValue(WORKING_REG_A.x) + _f.CCR.Ext;
                res3.x = res1.x - res2.x;
                PutIntValue(WORKING_REG_A.x, res3.x);
                break;
		          case SIZE_ELSE:
                PutShortValue(WORKING_REG_A.x, _f.SR.w);
								goto noAggFlag;
			          break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res1.b.l = 0;
                res2.b.l = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.w) + _f.CCR.Ext;
                res3.w = (WORD)res1.b.l - (WORD)res2.b.l;
                PutValue(WORKING_REG_A.x + (signed short int)Pipe2.w, res3.b.l);
								_pc+=2;
                break;
              case WORD_SIZE:
                res1.w = 0;
                res2.w = GetShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w) + _f.CCR.Ext;
                res3.x = (DWORD)res1.w - (DWORD)res2.w;
                PutShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w, res3.w);
								_pc+=2;
                break;
              case DWORD_SIZE:
                res1.x = 0;
                res2.x = GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w) + _f.CCR.Ext;
                res3.x = res1.x - res2.x;
                PutIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w, res3.x);
								_pc+=2;
                break;
		          case SIZE_ELSE:
                PutShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w, _f.SR.w);
								goto noAggFlag;
			          break;
              }
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                if(!DISPLACEMENT_SIZE) {
									res1.b.l = 0;
									res2.b.l = GetValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l) + _f.CCR.Ext;
									res3.w = (WORD)res1.b.l - (WORD)res2.b.l;
									PutValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.b.l);
									}
                else {
									res1.b.l = 0;
									res2.b.l = GetValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l) + _f.CCR.Ext;
									res3.w = (WORD)res1.b.l - (WORD)res2.b.l;
									PutValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.b.l);
                  }
								_pc+=2;
                break;
              case WORD_SIZE:
                res1.w = 0;
                if(!DISPLACEMENT_SIZE) {
                  res2.w = GetShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l) + _f.CCR.Ext;
                  res3.x = (DWORD)res1.w - (DWORD)res2.w;
                  PutShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.w);
									}
                else {
                  res2.w = GetShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l) + _f.CCR.Ext;
                  res3.x = (DWORD)res1.w - (DWORD)res2.w;
                  PutShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.w);
									}
								_pc+=2;
                break;
              case DWORD_SIZE:
                res1.x = 0;
                if(!DISPLACEMENT_SIZE) {
                  res2.x = GetIntValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l) + _f.CCR.Ext;
                  res3.x = res1.x - res2.x;
                  PutIntValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.x);
									}
                else {
                  res2.x = GetIntValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l) + _f.CCR.Ext;
                  res3.x = res1.x - res2.x;
                  PutIntValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.x);
									}
								_pc+=2;
                break;
		          case SIZE_ELSE:
                if(!DISPLACEMENT_SIZE)
                  PutShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, _f.SR.w);
                else
                  PutShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, _f.SR.w);
								goto noAggFlag;
			          break;
              }
            break;
          }
				goto aggFlagS;
				break;
        
			case 0x42:   // CLR
        switch(ADDRESSING) {
          case ABSOLUTEADD:
            switch(ABSOLUTEADD_SIZE) {
              case ABSSSUBADD:
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    PutValue(Pipe2.w, 0);
                    break;
                  case WORD_SIZE:
                    PutShortValue(Pipe2.w, 0);
                    break;
                  case DWORD_SIZE:
                    PutIntValue(Pipe2.w, 0);
                    break;
                  }
                _pc+=2;
                break;
              case ABSLSUBADD:
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    PutValue(Pipe2.d, 0);
                    break;
                  case WORD_SIZE:
                    PutShortValue(Pipe2.d, 0);
                    break;
                  case DWORD_SIZE:
                    PutIntValue(Pipe2.d, 0);
                    break;
                  }
                _pc+=4;
                break;
              case PCIDXSUBADD:   // qua no!
              case PCDISPSUBADD:
              case IMMSUBADD:
                break;
              }
            break;
          case DATAREGADD:   // Dn
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                WORKING_REG_D.b.b0 = 0;
                break;
              case WORD_SIZE:
                WORKING_REG_D.w.l = 0;
                break;
              case DWORD_SIZE:
                WORKING_REG_D.x = 0;
                break;
              }
            break;
          case ADDRREGADD:   // An Qua no!
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                PutValue(WORKING_REG_A.x, 0);
                break;
              case WORD_SIZE:
                PutShortValue(WORKING_REG_A.x, 0);
                break;
              case DWORD_SIZE:
                PutIntValue(WORKING_REG_A.x, 0);
                break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                PutValue(WORKING_REG_A.x, 0);
                break;
              case WORD_SIZE:
                PutShortValue(WORKING_REG_A.x, 0);
                WORKING_REG_A.x+=2;
                break;
              case DWORD_SIZE:
                PutIntValue(WORKING_REG_A.x, 0);
                WORKING_REG_A.x+=4;
                break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                WORKING_REG_A.x--;
                PutValue(WORKING_REG_A.x, 0);
                break;
              case WORD_SIZE:
                WORKING_REG_A.x-=2;
                PutShortValue(WORKING_REG_A.x, 0);
                break;
              case DWORD_SIZE:
                WORKING_REG_A.x-=4;
                PutIntValue(WORKING_REG_A.x, 0);
                break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                PutValue(WORKING_REG_A.x + (signed short int)Pipe2.w, 0);
                break;
              case WORD_SIZE:
                PutShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w, 0);
                break;
              case DWORD_SIZE:
                PutIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w, 0);
                break;
              }
						_pc+=2;
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  PutValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, 0);
		              }
                else {
                  PutValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, 0);
                  }
                break;
              case WORD_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  PutShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, 0);
                  }
                else {
                  PutShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, 0);
                  }
                break;
              case DWORD_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  PutIntValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, 0);
                  }
                else {
                  PutIntValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, 0);
                  }
                break;
              }
						_pc+=2;
            break;
          }
        _f.CCR.Zero=1;
        _f.CCR.Sign=0;
        _f.CCR.Ovf=_f.CCR.Carry=0;
				break;
        
			case 0x41:   // LEA CHK
			case 0x43:
			case 0x45:
			case 0x47:
			case 0x49:
			case 0x4b:
			case 0x4d:
			case 0x4f:
        if(Pipe1 & 0b0000000001000000) {    // LEA
					switch(ADDRESSING) {
						case ABSOLUTEADD:
							switch(ABSOLUTEADD_SIZE) {
								case ABSSSUBADD:
									res3.x = (signed short int)Pipe2.w;
									_pc+=2;
									break;
								case ABSLSUBADD:
									res3.x = Pipe2.d; 
									_pc+=4;
									break;
								case PCIDXSUBADD:   // (D16,PC)
									res3.x = _pc+(signed short int)Pipe2.w;
									_pc+=2;
									break;
								case PCDISPSUBADD:   // (D8,Xn,PC)
                  if(!DISPLACEMENT_SIZE)
    								res3.x = _pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l;
                  else
    								res3.x = _pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l;
									_pc+=2;
									break;
								case IMMSUBADD:		// qua no!
									break;
								}
							break;
						case DATAREGADD:   // Dn
						case ADDRREGADD:   // An
							break;
						case ADDRADD:   // (An)
							res3.x = WORKING_REG_A.x; //
							break;
						case ADDRPINCADD:   // (An)+
						case ADDRPDECADD:   // -(An)
							break;
						case ADDRDISPADD:   // (D16,An)
							res3.x = WORKING_REG_A.x + (signed short int)Pipe2.w;
							_pc+=2;
							break;
						case ADDRIDXADD:   // (D8,An,Xn)
							if(!DISPLACEMENT_SIZE) {
								res3.x = WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l;
								}
							else {
								res3.x = WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l;
								}
							_pc+=2;
							break;
						}
					DEST_REG_A.x = res3.x;
          }
        else {    // CHK
          if(Pipe1 & 0b0000000010000000) {    // SIZE (non è sicuro, dai doc...)
						switch(ADDRESSING) {
							case ABSOLUTEADD:
								switch(ABSOLUTEADD_SIZE) {
									case ABSSSUBADD:
										res3.x = GetIntValue((signed short int)Pipe2.w);
										_pc+=2;
										break;
									case ABSLSUBADD:
										res3.x = GetIntValue(Pipe2.d);
										_pc+=4;
										break;
									case PCIDXSUBADD:   // (D16,PC)
										res3.x = GetIntValue(_pc+(signed short int)Pipe2.w);
										_pc+=2;
										break;
									case PCDISPSUBADD:   // (D8,Xn,PC)
                    if(!DISPLACEMENT_SIZE)
    									res3.x = GetIntValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    else
      								res3.x = GetIntValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
										_pc+=2;
										break;
									case IMMSUBADD:
										res3.x = Pipe2.d; 
										_pc+=4;
										break;
									}
								break;
							case DATAREGADD:   // Dn
								res3.x = WORKING_REG_D.x;
								break;
							case ADDRREGADD:   // An
								break;
							case ADDRADD:   // (An)
								res3.x = GetIntValue(WORKING_REG_A.x); 
								break;
							case ADDRPINCADD:   // (An)+
								res3.x = GetIntValue(WORKING_REG_A.x);
								WORKING_REG_A.x+=4;
								break;
							case ADDRPDECADD:   // -(An)
								WORKING_REG_A.x-=4;
								res3.x = GetIntValue(WORKING_REG_A.x); 
								break;
							case ADDRDISPADD:   // (D16,An)
								res3.x = WORKING_REG_A.x + (signed short int)Pipe2.w;
								_pc+=2;
								break;
							case ADDRIDXADD:   // (D8,An,Xn)
								if(!DISPLACEMENT_SIZE) {
									res3.x = WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l;
									}
								else {
									res3.x = WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l;
									}
								_pc+=2;
								break;
							}
						_f.CCR.Sign=!!(DEST_REG_D.x & 0x80000000);
            if(((signed int)DEST_REG_D.x) < 0 || DEST_REG_D.x > ((signed int)res3.x)) {
              goto trap_chk_6;
            }
          else {
						switch(ADDRESSING) {
							case ABSOLUTEADD:
								switch(ABSOLUTEADD_SIZE) {
									case ABSSSUBADD:
										res3.w = GetShortValue((signed short int)Pipe2.w);
										_pc+=2;
										break;
									case ABSLSUBADD:
										res3.w = GetShortValue(Pipe2.d);
										_pc+=4;
										break;
									case PCIDXSUBADD:   // (D16,PC)
										res3.w = GetShortValue(_pc+(signed short int)Pipe2.w);
										_pc+=2;
										break;
									case PCDISPSUBADD:   // (D8,Xn,PC)
                    if(!DISPLACEMENT_SIZE)
  										res3.w = GetShortValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    else
    									res3.w = GetShortValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
										_pc+=2;
										break;
									case IMMSUBADD:
										res3.w = Pipe2.w;
										_pc+=2;
										break;
									}
								break;
							case DATAREGADD:   // Dn
								res3.w = WORKING_REG_D.w.l;
								break;
							case ADDRREGADD:   // An
								break;
							case ADDRADD:   // (An)
								res3.w = GetShortValue(WORKING_REG_A.x); // credo sbagliato, v. altri
								break;
							case ADDRPINCADD:   // (An)+
							case ADDRPDECADD:   // -(An)
								break;
							case ADDRDISPADD:   // (D16,An)
								res3.w = GetShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
								_pc+=2;
								break;
							case ADDRIDXADD:   // (D8,An,Xn)
								if(!DISPLACEMENT_SIZE) {
									res3.w = GetShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
									}
								else {
									res3.w = GetShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
									}
								_pc+=2;
								break;
							}
						_f.CCR.Sign=!!(DEST_REG_D.w.l & 0x8000);
						//N — Set if Dn < 0; cleared if Dn > effective address operand; undefined otherwise.
            if(((signed short int)DEST_REG_D.w.l) < 0 || DEST_REG_D.w.l > ((signed short int)res3.w))
              //trap #6
trap_chk_6:
              res3.b.l=6;
              goto doTrap;
              }
            }
          }
				break;
        
			case 0x44:   // NEG, MOVE to CCR
        switch(ADDRESSING) {
          case ABSOLUTEADD:
            switch(ABSOLUTEADD_SIZE) {
              case ABSSSUBADD:
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = 0;
                    res2.b.l = GetValue(Pipe2.w);
                    res3.w = (WORD)res1.b.l - (WORD)res2.b.l;
                    PutValue(Pipe2.w,res3.b.l);
                    _pc+=2;
                    break;
                  case WORD_SIZE:
                    res1.w = 0;
                    res2.w = GetShortValue(Pipe2.w);
                    res3.x = (DWORD)res1.w - (DWORD)res2.w;
                    PutShortValue(Pipe2.w,res3.w);
                    _pc+=2;
                    break;
                  case DWORD_SIZE:
                    res1.x = 0;
                    res2.x = GetIntValue(Pipe2.w);
                    res3.x = res1.x - res2.x;
                    PutIntValue(Pipe2.w,res3.x);
                    _pc+=2;
                    break;
                  case SIZE_ELSE:
                    _f.CCR.b =  LOBYTE(GetShortValue(Pipe2.w));
                    goto noAggFlag;
                    break;
                  }
                break;
              case ABSLSUBADD:
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = 0;
                    res2.b.l = GetValue(Pipe2.d);
                    res3.w = (WORD)res1.b.l - (WORD)res2.b.l;
                    PutValue(Pipe2.d,res3.b.l);
                    _pc+=4;
                    break;
                  case WORD_SIZE:
                    res1.w = 0;
                    res2.w = GetShortValue(Pipe2.d);
                    res3.x = (DWORD)res1.w - (DWORD)res2.w;
                    PutShortValue(Pipe2.d,res3.w);
                    _pc+=4;
                    break;
                  case DWORD_SIZE:
                    res1.x = 0;
                    res2.x = GetIntValue(Pipe2.d); 
                    res3.x = res1.x - res2.x;
                    PutIntValue(Pipe2.d,res3.x);
                    _pc+=4;
                    break;
                  case SIZE_ELSE:
                    _f.CCR.b =  LOBYTE(GetShortValue(Pipe2.w));
                    goto noAggFlag;
                    break;
                  }
                break;
              case PCIDXSUBADD:
                switch(OPERAND_SIZE) {
                  case SIZE_ELSE:
                    _f.CCR.b =  LOBYTE(GetShortValue(_pc+(signed short int)Pipe2.w));
										_pc+=2;
                    goto noAggFlag;
                    break;
                  }
                break;
              case PCDISPSUBADD:
                switch(OPERAND_SIZE) {
                  case SIZE_ELSE:
                    if(!DISPLACEMENT_SIZE)
                      _f.CCR.b =  LOBYTE(GetShortValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l));
                    else
                      _f.CCR.b =  LOBYTE(GetShortValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l));
										_pc+=2;
                    goto noAggFlag;
                    break;
                  }
                break;
              case IMMSUBADD:
                switch(OPERAND_SIZE) {
                  case SIZE_ELSE:
                    _f.CCR.b =  Pipe2.b.l;
										_pc+=2;
                    goto noAggFlag;
                    break;
                  }
                break;
              }
            break;
          case DATAREGADD:   // Dn
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res1.b.l = 0;
                res2.b.l = WORKING_REG_D.b.b0;
                res3.w = (WORD)res1.b.l - (WORD)res2.b.l;
                WORKING_REG_D.b.b0 = res3.b.l;
                break;
              case WORD_SIZE:
                res1.w = 0;
                res2.w = WORKING_REG_D.w.l;
                res3.x = (DWORD)res1.w - (DWORD)res2.w;
                WORKING_REG_D.w.l = res3.w;
                break;
              case DWORD_SIZE:
                res1.x = 0;
                res2.x = WORKING_REG_D.x;
                res3.x = res1.x - res2.x;
                WORKING_REG_D.x = res3.x;
                break;
		          case SIZE_ELSE:
                _f.CCR.b =  LOBYTE(WORKING_REG_D.w.l);
								goto noAggFlag;
			          break;
              }
            break;
          case ADDRREGADD:   // An
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res1.b.l = 0;
                res2.b.l = GetValue(WORKING_REG_A.x);
                res3.w = (WORD)res1.b.l - (WORD)res2.b.l;
                PutValue(WORKING_REG_A.x, res3.b.l);
                break;
              case WORD_SIZE:
                res1.w = 0;
                res2.w = GetShortValue(WORKING_REG_A.x);
                res3.x = (DWORD)res1.w - (DWORD)res2.w;
                PutShortValue(WORKING_REG_A.x, res3.w);
                break;
              case DWORD_SIZE:
                res1.x = 0;
                res2.x = GetIntValue(WORKING_REG_A.x);
                res3.x = res1.x - res2.x;
                PutIntValue(WORKING_REG_A.x, res3.x);
                break;
		          case SIZE_ELSE:
                _f.CCR.b = LOBYTE(GetShortValue(WORKING_REG_A.x));
								goto noAggFlag;
			          break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res1.b.l = 0;
                res2.b.l = GetValue(WORKING_REG_A.x);
                res3.w = (WORD)res1.b.l - (WORD)res2.b.l;
                PutValue(WORKING_REG_A.x, res3.b.l);
                WORKING_REG_A.x++;
                break;
              case WORD_SIZE:
                res1.w = 0;
                res2.w = GetShortValue(WORKING_REG_A.x);
                res3.x = (DWORD)res1.w - (DWORD)res2.w;
                PutShortValue(WORKING_REG_A.x, res3.w);
                WORKING_REG_A.x+=2;
                break;
              case DWORD_SIZE:
                res1.x = 0;
                res2.x = GetIntValue(WORKING_REG_A.x);
                res3.x = res1.x - res2.x;
                PutIntValue(WORKING_REG_A.x, res3.x);
                WORKING_REG_A.x+=4;
                break;
		          case SIZE_ELSE:
                _f.CCR.b = LOBYTE(GetShortValue(WORKING_REG_A.x));
								goto noAggFlag;
			          break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                WORKING_REG_A.x--;
                res1.b.l = 0;
                res2.b.l = GetValue(WORKING_REG_A.x);
                res3.w = (WORD)res1.b.l - (WORD)res2.b.l;
                PutValue(WORKING_REG_A.x, res3.b.l);
                break;
              case WORD_SIZE:
                WORKING_REG_A.x-=2;
                res1.w = 0;
                res2.w = GetShortValue(WORKING_REG_A.x);
                res3.x = (DWORD)res1.w - (DWORD)res2.w;
                PutShortValue(WORKING_REG_A.x, res3.w);
                break;
              case DWORD_SIZE:
                WORKING_REG_A.x-=4;
                res1.x = 0;
                res2.x = GetIntValue(WORKING_REG_A.x);
                res3.x = res1.x - res2.x;
                PutIntValue(WORKING_REG_A.x, res3.x);
                break;
		          case SIZE_ELSE:
                _f.CCR.b = LOBYTE(GetShortValue(WORKING_REG_A.x));
								goto noAggFlag;
			          break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res1.b.l = 0;
                res2.b.l = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                res3.w = (WORD)res1.b.l - (WORD)res2.b.l;
                PutValue(WORKING_REG_A.x + (signed short int)Pipe2.w, res3.b.l);
								_pc+=2;
                break;
              case WORD_SIZE:
                res1.w = 0;
                res2.w = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                res3.x = (DWORD)res1.w - (DWORD)res2.w;
                PutShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w, res3.w);
								_pc+=2;
                break;
              case DWORD_SIZE:
                res1.x = 0;
                res2.x = GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                res3.x = res1.x - res2.x;
                PutIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w, res3.x);
								_pc+=2;
                break;
		          case SIZE_ELSE:
                _f.CCR.b = LOBYTE(GetShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w));
								goto noAggFlag;
			          break;
              }
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res1.b.l = 0;
                if(!DISPLACEMENT_SIZE) {
                  res2.b.l = GetValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  res3.w = (WORD)res1.b.l - (WORD)res2.b.l;
                  PutValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.b.l);
                  }
                else {
                  res2.b.l = GetValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  res3.w = (WORD)res1.b.l - (WORD)res2.b.l;
                  PutValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.b.l);
                  }
								_pc+=2;
                break;
              case WORD_SIZE:
                res1.w = 0;
                if(!DISPLACEMENT_SIZE) {
                  res2.w = GetShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  res3.x = (DWORD)res1.w - (DWORD)res2.w;
                  PutShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.w);
                  }
                else {
                  res2.w = GetShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  res3.x = (DWORD)res1.w - (DWORD)res2.w;
                  PutShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.w);
                  }
								_pc+=2;
                break;
              case DWORD_SIZE:
                res1.x = 0;
                if(!DISPLACEMENT_SIZE) {
                  res2.x = GetIntValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  res3.x = res1.x - res2.x;
                  PutIntValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.x);
                  }
                else {
                  res2.x = GetIntValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  res3.x = res1.x - res2.x;
                  PutIntValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.x);
                  }
								_pc+=2;
                break;
		          case SIZE_ELSE:
                if(!DISPLACEMENT_SIZE) {
                  _f.CCR.b = LOBYTE(GetShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l));
                  }
                else {
	                _f.CCR.b = LOBYTE(GetShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l));
                  }
								goto noAggFlag;
			          break;
              }
            break;
          }
				goto aggFlagS;
				break;
        
			case 0x46:   // MOVE to SR, NOT
        switch(ADDRESSING) {
          case ABSOLUTEADD:
            switch(ABSOLUTEADD_SIZE) {
              case ABSSSUBADD:
       					_pc+=2;
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res3.b.l = ~GetValue(Pipe2.w);
                    PutValue(Pipe2.w,res3.b.l);
                    break;
                  case WORD_SIZE:
                    res3.w = ~GetShortValue(Pipe2.w);
                    PutShortValue(Pipe2.w,res3.w);
                    break;
                  case DWORD_SIZE:
                    res3.x = ~GetIntValue(Pipe2.w);
                    PutIntValue(Pipe2.w,res3.x);
                    break;
                  case SIZE_ELSE:
                    if(_f.SR.Supervisor) {
                      res3.w = GetShortValue(Pipe2.w);
                      goto aggSR;
                      }
                    else {
                      res3.b.l=8;
                      goto doTrap;
                      }
                    break;
                  }
                break;
              case ABSLSUBADD:
                _pc+=4;
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res3.b.l = ~GetValue(Pipe2.d);
                    PutValue(Pipe2.d,res3.b.l);
                    break;
                  case WORD_SIZE:
                    res3.w = ~GetShortValue(Pipe2.d);
                    PutShortValue(Pipe2.d,res3.w);
                    break;
                  case DWORD_SIZE:
                    res3.x = ~GetIntValue(Pipe2.d); 
                    PutIntValue(Pipe2.d,res3.x);
                    break;
                  case SIZE_ELSE:
                    if(_f.SR.Supervisor) {
                      res3.w = GetShortValue(Pipe2.d);
                      goto aggSR;
                      }
                    else {
                      res3.b.l=8;
                      goto doTrap;
                      }
                    break;
                  }
                break;
              case PCIDXSUBADD:
                switch(OPERAND_SIZE) {
                  case SIZE_ELSE:
          					_pc+=2;
                    if(_f.SR.Supervisor) {
                      res3.w = GetShortValue(_pc+(signed short int)Pipe2.w);
                      goto aggSR;
                      }
                    else {
                      res3.b.l=8;
                      goto doTrap;
                      }
                    break;
                  }
                break;
              case PCDISPSUBADD:
                switch(OPERAND_SIZE) {
                  case SIZE_ELSE:
          					_pc+=2;
                    if(_f.SR.Supervisor) {
                      if(!DISPLACEMENT_SIZE)
                        res3.w = GetShortValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                      else
                        res3.w = GetShortValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                      goto aggSR;
                      }
                    else {
                      res3.b.l=8;
                      goto doTrap;
                      }
                    break;
                  }
                break;
              case IMMSUBADD:
                switch(OPERAND_SIZE) {
                  case SIZE_ELSE:
                    _pc+=2;
                    if(_f.SR.Supervisor) {
                      res3.w = Pipe2.w;
                      goto aggSR;
                      }
                    else {
                      res3.b.l=8;
                      goto doTrap;
                      }
                    break;
                  }
                break;
              }
            break;
          case DATAREGADD:   // Dn
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = ~WORKING_REG_D.b.b0;
                WORKING_REG_D.b.b0 = res3.b.l;
                break;
              case WORD_SIZE:
                res3.w = ~WORKING_REG_D.w.l;
                WORKING_REG_D.w.l = res3.w;
                break;
              case DWORD_SIZE:
                res3.x = ~WORKING_REG_D.x;
                WORKING_REG_D.x = res3.x;
                break;
		          case SIZE_ELSE:
                if(_f.SR.Supervisor) {
                  res3.w = WORKING_REG_D.w.l;
                  goto aggSR;
                  }
                else {
                  res3.b.l=8;
                  goto doTrap;
                  }
			          break;
              }
            break;
          case ADDRREGADD:   // An
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = ~GetValue(WORKING_REG_A.x);
                PutValue(WORKING_REG_A.x, res3.b.l);
                break;
              case WORD_SIZE:
                res3.w = ~GetShortValue(WORKING_REG_A.x);
                PutShortValue(WORKING_REG_A.x, res3.w);
                break;
              case DWORD_SIZE:
                res3.x = ~GetIntValue(WORKING_REG_A.x);
                PutIntValue(WORKING_REG_A.x, res3.x);
                break;
		          case SIZE_ELSE:
                if(_f.SR.Supervisor) {
                  res3.w = GetShortValue(WORKING_REG_A.x);
    							goto aggSR;
                  }
                else {
                  res3.b.l=8;
                  goto doTrap;
                  }
			          break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = ~GetValue(WORKING_REG_A.x);
                PutValue(WORKING_REG_A.x, res3.b.l);
                WORKING_REG_A.x++;
                break;
              case WORD_SIZE:
                res3.w = ~GetShortValue(WORKING_REG_A.x);
                PutShortValue(WORKING_REG_A.x, res3.w);
                WORKING_REG_A.x+=2;
                break;
              case DWORD_SIZE:
                res3.x = ~GetIntValue(WORKING_REG_A.x);
                PutIntValue(WORKING_REG_A.x, res3.x);
                WORKING_REG_A.x+=4;
                break;
		          case SIZE_ELSE:
                if(_f.SR.Supervisor) {
                  res3.w = GetShortValue(WORKING_REG_A.x);
	                WORKING_REG_A.x+=2;
									goto aggSR;
                  }
                else {
                  res3.b.l=8;
                  goto doTrap;
                  }
			          break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                WORKING_REG_A.x--;
                res3.b.l = ~GetValue(WORKING_REG_A.x);
                PutValue(WORKING_REG_A.x, res3.b.l);
                break;
              case WORD_SIZE:
                WORKING_REG_A.x-=2;
                res3.w = ~GetShortValue(WORKING_REG_A.x);
                PutShortValue(WORKING_REG_A.x, res3.w);
                break;
              case DWORD_SIZE:
                WORKING_REG_A.x-=4;
                res3.x = ~GetIntValue(WORKING_REG_A.x);
                PutIntValue(WORKING_REG_A.x, res3.x);
                break;
		          case SIZE_ELSE:
                if(_f.SR.Supervisor) {
	                WORKING_REG_A.x-=2;
                  res3.w = GetShortValue(WORKING_REG_A.x);
									goto aggSR;
                  }
                else {
                  res3.b.l=8;
                  goto doTrap;
                  }
			          break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
						_pc+=2;
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = ~GetValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                PutValue(WORKING_REG_A.x + (signed short int)Pipe2.w, res3.b.l);
                break;
              case WORD_SIZE:
                res3.w = ~GetValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                PutShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w, res3.w);
                break;
              case DWORD_SIZE:
                res3.x = ~GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                PutIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w, res3.x);
                break;
		          case SIZE_ELSE:
                if(_f.SR.Supervisor) {
                  res3.w = GetShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
									goto aggSR;
                  }
                else {
                  res3.b.l=8;
                  goto doTrap;
                  }
			          break;
              }
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
						_pc+=2;
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.b.l = ~GetValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  PutValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.b.l);
                  }
                else {
                  res3.b.l = ~GetValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  PutValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.b.l);
                  }
                break;
              case WORD_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.w = ~GetShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  PutShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.w);
                  }
                else {
                  res3.w = ~GetShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  PutShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.w);
                  }
                break;
              case DWORD_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.x = ~GetIntValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  PutIntValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.x);
                  }
                else {
                  res3.x = ~GetIntValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  PutIntValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.x);
                  }
                break;
		          case SIZE_ELSE:
                if(!DISPLACEMENT_SIZE) {
                  if(_f.SR.Supervisor) {
                    res3.w =GetShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
										goto aggSR;
                    }
                  else {
                    res3.b.l=8;
                    goto doTrap;
                    }
                  }
                else {
                  if(_f.SR.Supervisor) {
  	                res3.w =GetShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
										goto aggSR;
                    }
                  else {
                    res3.b.l=8;
                    goto doTrap;
                    }
                  }
			          break;
              }
            break;
          }
				goto aggFlag;
				break;
        
			case 0x48:   // EXT NBCD SWAP PEA MOVEM
        switch((LOBYTE(Pipe1) & 0xf8)) {
          case 0x00:    // NBCD
            break;
          case 0x40:    // SWAP
            res3.x = MAKELONG(WORKING_REG_D.w.h,WORKING_REG_D.w.l);
            WORKING_REG_D.x = res3.x;
            goto aggFlag4;
            break;
          case 0x80:    // EXT.w
            res3.w = (signed short int)WORKING_REG_D.b.b0;
            WORKING_REG_D.w.l = res3.w;
            goto aggFlag2;
            break;
          case SIZE_ELSE:    // EXT.l
            res3.x = (signed short int)WORKING_REG_D.w.l;
            WORKING_REG_D.x = res3.x;
            goto aggFlag4;
            break;
          case 0x48:    // PEA
          case 0x50:
          case 0x58:
          case 0x60:
          case 0x68:
          case 0x70:
          case 0x78:
            switch(ADDRESSING) {    // v. LEA ..
              case ABSOLUTEADD:
                switch(ABSOLUTEADD_SIZE) {
                  case ABSSSUBADD:
                    res3.x = (signed short int)Pipe2.w;
                    _pc+=2;
                    break;
                  case ABSLSUBADD:
                    res3.x = Pipe2.d; 
                    _pc+=4;
                    break;
                  case PCIDXSUBADD:   // (D16,PC)
                    res3.x = _pc+(signed short int)Pipe2.w;
                    _pc+=2;
                    break;
                  case PCDISPSUBADD:   // (D8,Xn,PC)
                    if(!DISPLACEMENT_SIZE)
                      res3.x = _pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l;
                    else
                      res3.x = _pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l;
                    _pc+=2;
                    break;
                  case IMMSUBADD:				// qua no!
                    break;
                  }
                break;
              case DATAREGADD:   // Dn 
              case ADDRREGADD:   // An
                break;
              case ADDRADD:   // (An)
                res3.x = WORKING_REG_A.x; //
                break;
              case ADDRPINCADD:   // (An)+			// qua no!
              case ADDRPDECADD:   // -(An)
                break;
              case ADDRDISPADD:   // (D16,An)
                res3.x = WORKING_REG_A.x + (signed short int)Pipe2.w;
                _pc+=2;
                break;
              case ADDRIDXADD:   // (D8,An,Xn)
                if(!DISPLACEMENT_SIZE) {
                  res3.x = WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l;
                  }
                else {
                  res3.x = WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l;
                  }
                _pc+=2;
                break;
              }
            _sp-=4;
            PutIntValue(_sp,res3.x);
            break;
          default:      // MOVEM register -> memory
            res2.w = Pipe2.w;
            _pc+=AdvPipe(_pc,2);
            switch(ADDRESSING) {
              case ABSOLUTEADD:
                switch(ABSOLUTEADD_SIZE) {
                  case ABSSSUBADD:
                    res3.x = Pipe2.w;
                    MOVEM_R2M()
                    _pc+=2;
                    break;
                  case ABSLSUBADD:
                    res3.x = Pipe2.d;
                    MOVEM_R2M()
                    _pc+=4;
                    break;
                  case PCIDXSUBADD:   // (D16,PC)
                    // NON dovrebbe esserci!
                    res3.x = _pc+(signed short int)Pipe2.w;
                    MOVEM_R2M()
                    _pc+=2;
                    break;
                  case PCDISPSUBADD:   // (D8,Xn,PC)
                    // NON dovrebbe esserci!
                    if(!DISPLACEMENT_SIZE)
                      res3.x = _pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l;
                    else
                      res3.x = _pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l;
                    MOVEM_R2M()
                    _pc+=2;
                    break;
                  case IMMSUBADD:   // qua no!
                    break;
                  }
                break;
              case DATAREGADD:   // Dn NO!
              case ADDRREGADD:   // An NO!
                break;
              case ADDRADD:   // (An)
                res3.x = WORKING_REG_A.x;
                MOVEM_R2M()
                break;
              case ADDRPINCADD:   // (An)+ qua NO!
                break;
              case ADDRPDECADD:   // -(An)
                res3.x = WORKING_REG_A.x;
                MOVEM_R2M_PREDEC()
                WORKING_REG_A.x = res3.x;   // non è sicurissimo ma qua dovrebbe essere così... e 68020 è poi diverso
                break;
              case ADDRDISPADD:   // (D16,An)
                res3.x = WORKING_REG_A.x + (signed short int)Pipe2.w;
                MOVEM_R2M()
                _pc+=2;
                break;
              case ADDRIDXADD:   // (D8,An,Xn)
                if(!DISPLACEMENT_SIZE) {
                  res3.x = WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l;
                  }
                else {
                  res3.x = WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l;
                  }
                MOVEM_R2M()
                _pc+=2;
                break;
              }
            break;
          }
				break;
        
			case 0x4a:   // ILLEGAL TAS TST 
        switch(ADDRESSING) {
          case ABSOLUTEADD:
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                if(LOBYTE(Pipe1) & 1) {
                  res3.b.l = GetValue(Pipe2.d);
                  _pc+=4;
                  }
                else {
                  res3.b.l = GetValue(Pipe2.w);
                  _pc+=2;
                  }
                break;
              case WORD_SIZE:
                if(LOBYTE(Pipe1) & 1) {
                  res3.w = GetShortValue(Pipe2.d);
                  _pc+=4;
                  }
                else {
                  res3.w = GetShortValue(Pipe2.w);
                  _pc+=2;
                  }
                break;
              case DWORD_SIZE:
                if(LOBYTE(Pipe1) & 1) {
                  res3.x = GetIntValue(Pipe2.d);
                  _pc+=4;
                  }
                else {
                  res3.x = GetIntValue(Pipe2.w);
                  _pc+=2;
                  }
                break;
              case SIZE_ELSE:      // ILLEGAL TAS
                if(LOBYTE(Pipe1) != 0xfc) {   // TAS
                  if(LOBYTE(Pipe1) & 1) {
                    res3.b.l = GetValue(Pipe2.d);
                    PutValue(Pipe2.d,res3.b.l | 0x80);
                    _pc+=4;
                    }
                  else {
                    res3.b.l = GetValue(Pipe2.w);
                    PutValue(Pipe2.w,res3.b.l | 0x80);
                    _pc+=2;
                    }
                  }
                else {    // ILLEGAL
                  res3.b.l=4;
                  goto doTrap;
                  }
                break;
              }
            break;
          case DATAREGADD:   // Dn
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = WORKING_REG_D.b.b0;
                break;
              case WORD_SIZE:
                res3.w = WORKING_REG_D.w.l;
                break;
              case DWORD_SIZE:
                res3.x = WORKING_REG_D.x;
                break;
              case SIZE_ELSE:    // TAS
                res3.b.l = WORKING_REG_D.b.b0;
                WORKING_REG_D.b.b0 |= 0x80;
                break;
              }
            break;
          case ADDRREGADD:   // An QUA NO!
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GetValue(WORKING_REG_A.x);
                break;
              case WORD_SIZE:
                res3.w = GetShortValue(WORKING_REG_A.x);
                break;
              case DWORD_SIZE:
                res3.x = GetIntValue(WORKING_REG_A.x);
                break;
              case SIZE_ELSE:    // TAS
                res3.b.l = GetValue(WORKING_REG_A.x);
                PutValue(WORKING_REG_A.x, res3.b.l | 0x80);
                break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GetValue(WORKING_REG_A.x);
                WORKING_REG_A.x++;                
                break;
              case WORD_SIZE:
                res3.w = GetShortValue(WORKING_REG_A.x);
                WORKING_REG_A.x+=2;
                break;
              case DWORD_SIZE:
                res3.x = GetIntValue(WORKING_REG_A.x);
                WORKING_REG_A.x+=4;
                break;
              case SIZE_ELSE:    // TAS
                res3.b.l = GetValue(WORKING_REG_A.x);
                PutValue(WORKING_REG_A.x, res3.b.l | 0x80);
                WORKING_REG_A.x++;
                break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                WORKING_REG_A.x--;                
                res3.b.l = GetValue(WORKING_REG_A.x);
                break;
              case WORD_SIZE:
                WORKING_REG_A.x-=2;
                res3.w = GetShortValue(WORKING_REG_A.x);
                break;
              case DWORD_SIZE:
                WORKING_REG_A.x-=4;
                res3.x = GetIntValue(WORKING_REG_A.x);
                break;
              case SIZE_ELSE:    // TAS
                WORKING_REG_A.x--;
                res3.b.l = GetValue(WORKING_REG_A.x);
                PutValue(WORKING_REG_A.x, res3.b.l | 0x80);
                break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                break;
              case WORD_SIZE:
                res3.w = GetShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                break;
              case DWORD_SIZE:
                res3.x = GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                break;
              case SIZE_ELSE:    // TAS
                res3.b.l = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                PutValue(WORKING_REG_A.x + (signed short int)Pipe2.w, res3.b.l | 0x80);
                break;
              }
            _pc+=2;
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.b.l = GetValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  }
                else {
                  res3.b.l = GetValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  }
                break;
              case WORD_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.w = GetShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  }
                else {
                  res3.w = GetShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  }
                break;
              case DWORD_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.x = GetIntValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  }
                else {
                  res3.x = GetIntValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  }
                break;
              case SIZE_ELSE:    // TAS
                if(!DISPLACEMENT_SIZE) {
                  res3.b.l = GetValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  }
                else {
                  res3.b.l = GetValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  }
                if(!DISPLACEMENT_SIZE) {
                  PutValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.b.l | 0x80);
                  }
                else {
                  PutValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.b.l | 0x80);
                  }
                break;
              }
            _pc+=2;
            break;
          }
        switch(OPERAND_SIZE) {
          default:      // TST
            goto aggFlag;
            break;
          case SIZE_ELSE:    // TAS
            goto aggFlag1;
            break;
          }
				break;
        
			case 0x4c:   // MOVEM memory -> register
        res2.w = Pipe2.w;
        _pc+=AdvPipe(_pc,2);
        switch(ADDRESSING) {
          case ABSOLUTEADD:
            switch(ABSOLUTEADD_SIZE) {
              case ABSSSUBADD:
                res3.x = Pipe2.w;
                MOVEM_M2R()
                _pc+=2;
                break;
              case ABSLSUBADD:
                res3.x = Pipe2.d;
                MOVEM_M2R()
                _pc+=4;
                break;
              case PCIDXSUBADD:   // (D16,PC)
                res3.x = _pc+(signed short int)Pipe2.w;
                MOVEM_M2R()
                _pc+=2;
                break;
              case PCDISPSUBADD:   // (D8,Xn,PC)
                if(!DISPLACEMENT_SIZE)
                  res3.x = _pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l;
                else
                  res3.x = _pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l;
                MOVEM_M2R()
                _pc+=2;
                break;
              case IMMSUBADD:   // qua no!
                break;
              }
            break;
          case DATAREGADD:   // Dn NO!
          case ADDRREGADD:   // An NO!
            break;
          case ADDRADD:   // (An)
            res3.x = WORKING_REG_A.x;
            MOVEM_M2R()
            break;
          case ADDRPINCADD:   // (An)+
            res3.x = WORKING_REG_A.x;
            MOVEM_M2R()
            WORKING_REG_A.x = res3.x;   // si spera...
            break;
          case ADDRPDECADD:   // -(An) qua NO!
            break;
          case ADDRDISPADD:   // (D16,An)
            res3.x = WORKING_REG_A.x + (signed short int)Pipe2.w;
            MOVEM_M2R()
            _pc+=2;
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            if(!DISPLACEMENT_SIZE) {
              res3.x = WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l;
              }
            else {
              res3.x = WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l;
              }
            MOVEM_M2R()
            _pc+=2;
            break;
          }
				break;
        
			case 0x4e:   // TRAP LINK UNLK MOVEUSP RESET NOP STOP RTE RTS TRAPV RTR JSR JMP
        switch(LOBYTE(Pipe1)) {
          case 0x70:   // RESET
            if(_f.SR.Supervisor) {
              ActivateReset=124;
              }
            else {
              res3.b.l=8;
              goto doTrap;
              }
            break;
          case 0x71:   // NOP

            break;
          case 0x72:   // STOP
						_pc+=2;
            if(_f.SR.Supervisor) {
							// dalle prove pare che se il valore che carico NON contiene Supervisor diamo Trap Privilege!
	            if(!(Pipe2.w & 0x2000)) 
								;
              res3.w=Pipe2.w;
              DoStop=1;
              goto aggSR;
              }
            else {
              res3.b.l=8;
              goto doTrap;
              }

            break;
          case 0x73:   // RTE
            if(_f.SR.Supervisor) {
              res3.w=GetShortValue(_sp);
              _sp+=2;
              _pc=GetIntValue(_sp);
              _sp+=4;
              FC=0b010;
              goto aggSR;
              }
            else {
              res3.b.l=8;
              goto doTrap;
              }
            break;
#ifdef MC68010
          case 0x74:   // RTD
            _pc=GetIntValue(_sp);
            _sp+=4;  _sp+=(signed short int)Pipe2.w;
            break;
#endif
          case 0x75:   // RTS
            _pc=GetIntValue(_sp);
            _sp+=4;
            break;
          case 0x76:   // TRAPV
            if(_f.CCR.Ovf) {
              res3.b.l=7;
              goto doTrap;
              }
            break;
          case 0x77:   // RTR
            _f.CCR.b=LOBYTE(GetShortValue(_sp));
            _sp+=2;
            _pc=GetIntValue(_sp);
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
            res3.b.l=LOBYTE(Pipe1) & 0xf;
            
doTrap:
            FC=0b110;
            _fsup=_f;
            if(!_f.SR.Supervisor)
              a7U=regsA.r[7].x;
            else
              a7S=regsA.r[7].x;
            _f.SR.Supervisor=1;
            regsA.r[7].x=a7S;
            _sp-=4;
            PutIntValue(_sp,_pc); // low word prima
            _sp-=2;
            PutShortValue(_sp,_fsup.w);
            _pc=GetIntValue(0x00000080+(res3.b.l)*4);
            
            printf("68K exception %u\r\n",res3.b.l);
            break;

          case 0x50:   // LINK
          case 0x51:
          case 0x52:
          case 0x53:
          case 0x54:
          case 0x55:
          case 0x56:
          case 0x57:
            _sp-=4;
            PutIntValue(_sp,WORKING_REG_A.x);
            WORKING_REG_A.x=_sp;
            _sp+=(signed short int)Pipe2.w;
            _pc+=2;
            break;
          case 0x58:    //UNLK
          case 0x59:
          case 0x5a:
          case 0x5b:
          case 0x5c:
          case 0x5d:
          case 0x5e:
          case 0x5f:
            _sp=WORKING_REG_A.x;
            WORKING_REG_A.x=GetIntValue(_sp);
            _sp+=4;
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
						if(_f.SR.Supervisor) {
	            if(LOBYTE(Pipe1) & 0b1000) {   // direction
								WORKING_REG_A.x = a7U;
								}
							else {
								a7U = WORKING_REG_A.x;
								}
							}
						else {
              res3.b.l=8;
							goto doTrap;
							}
            break;
          default:
            if(!(LOBYTE(Pipe1) & 0b01000000)) {   // JSR
              _sp-=4;
              switch(ADDRESSING) {
                case ABSOLUTEADD:
                  switch(ABSOLUTEADD_SIZE) {
                    case ABSSSUBADD:
                      PutIntValue(_sp,_pc+2);  // low word prima
                      _pc=Pipe2.w;
                      break;
                    case ABSLSUBADD:
                      PutIntValue(_sp,_pc+4);  // 
                      _pc=Pipe2.d;
                      break;
                    case PCIDXSUBADD:   // (D16,PC)
                      PutIntValue(_sp,_pc+2);  // 
                      _pc = _pc+(signed short int)Pipe2.w;
                      break;
                    case PCDISPSUBADD:   // (D8,Xn,PC)
                      PutIntValue(_sp,_pc+2);  // 
                      if(!DISPLACEMENT_SIZE)
                        _pc = _pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l;
                      else
                        _pc = _pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l;
                      break;
                    case IMMSUBADD:   // qua no!
                      break;
                    }
                  break;
                case DATAREGADD:   // Dn
                case ADDRREGADD:   // An
                  break;
                case ADDRADD:   // (An)
                  PutIntValue(_sp,_pc);  // 
                  _pc=WORKING_REG_A.x;
                  break;
                case ADDRPINCADD:   // (An)+    qua no!
                case ADDRPDECADD:   // -(An)
                  break;
                case ADDRDISPADD:   // (D16,An)
                  PutIntValue(_sp,_pc+2);  // 
                  _pc=WORKING_REG_A.x + (signed short int)Pipe2.w;
                  break;
                case ADDRIDXADD:   // (D8,An,Xn)
                  PutIntValue(_sp,_pc+2);  // 
                  if(!DISPLACEMENT_SIZE)
                    _pc=WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l;
                  else
                    _pc=WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l;
                  break;
                }
              }
            else {    // JMP
              switch(ADDRESSING) {
                case ABSOLUTEADD:
                  switch(ABSOLUTEADD_SIZE) {
                    case ABSSSUBADD:
                      _pc=Pipe2.w;
                      break;
                    case ABSLSUBADD:
                      _pc=Pipe2.d;
                      break;
                    case PCIDXSUBADD:   // (D16,PC)
                      _pc = _pc+(signed short int)Pipe2.w;
                      break;
                    case PCDISPSUBADD:   // (D8,Xn,PC)
                      if(!DISPLACEMENT_SIZE)
                        _pc = _pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l;
                      else
                        _pc = _pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l;
                      break;
                    case IMMSUBADD:   // qua no!
                      break;
                    }
                  break;
                case DATAREGADD:   // Dn
                case ADDRREGADD:   // An
                  break;
                case ADDRADD:   // (An)
                  _pc=WORKING_REG_A.x;
                  break;
                case ADDRPINCADD:   // (An)+
                case ADDRPDECADD:   // -(An)
                  break;
                case ADDRDISPADD:   // (D16,An)
                  _pc=WORKING_REG_A.x + (signed short int)Pipe2.w;
                  break;
                case ADDRIDXADD:   // (D8,An,Xn)
                  if(!DISPLACEMENT_SIZE)
                    _pc=WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l;
                  else
                    _pc=WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l;
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
          case SIZE_ELSE:    // Scc DBcc
            switch(Pipe1 & 0xf) {
              case 0x0:
                res3.b.l=0xff;    // credo...!
                break;
              case 0x2:
                if(!_f.CCR.Carry && !_f.CCR.Zero)     // higher !C !Z
                  res3.b.l=0xff;
                else
                  res3.b.l=0x00;
                break;
              case 0x4:
                if(!_f.CCR.Carry)     // carry clear
                  res3.b.l=0xff;
                else
                  res3.b.l=0x00;
                break;
              case 0x6:
                if(!_f.CCR.Zero)      // not equal
                  res3.b.l=0xff;
                else
                  res3.b.l=0x00;
                break;
              case 0x8:
                if(!_f.CCR.Ovf)      // overflow clear
                  res3.b.l=0xff;
                else
                  res3.b.l=0x00;
                break;
              case 0xa:
                if(!_f.CCR.Sign)     // plus
                  res3.b.l=0xff;
                else
                  res3.b.l=0x00;
                break;
              case 0xc:
                if(!(_f.CCR.Sign ^ _f.CCR.Ovf))     // greater or equal
                  res3.b.l=0xff;
                else
                  res3.b.l=0x00;
                break;
              case 0xe:
                if(_f.CCR.Zero && !(_f.CCR.Sign ^ _f.CCR.Ovf))     // greater than
                  res3.b.l=0xff;
                else
                  res3.b.l=0x00;
                break;
              }
            
doDBccScc:
            if(ADDRESSING == ADDRREGADD) {    // DBcc
              if(!res3.b.l) {
                WORKING_REG_D.w.l--;
                if(((int16_t)WORKING_REG_D.w.l) != -1)
                  _pc+=(signed short int)Pipe2.w;
                else
                  _pc+=2;
                }
              else
                _pc+=2;
              }
            else {      // Scc
              switch(ADDRESSING) {
                case ABSOLUTEADD:    // abs
                  switch(ABSOLUTEADD_SIZE) {
                    case ABSSSUBADD:
                      PutValue(Pipe2.w, res3.b.l);
                      _pc+=2;
                      break;
                    case ABSLSUBADD:
                      PutValue(Pipe2.d, res3.b.l);
                      _pc+=4;
                      break;
                    case PCIDXSUBADD:   //qua no
                    case PCDISPSUBADD:
                    case IMMSUBADD:
                      break;
                    }
                  break;
                case DATAREGADD:   // Dn
                  WORKING_REG_D.b.b0= res3.b.l;
                  break;
                case ADDRADD:   // (An)
                  PutValue(WORKING_REG_A.x,res3.b.l);
                  break;
                case ADDRPINCADD:   // (An)+
                  PutValue(WORKING_REG_A.x,res3.b.l);
                  WORKING_REG_A.x++;
                  break;
                case ADDRPDECADD:   // -(An)
                  WORKING_REG_A.x--;
                  PutValue(WORKING_REG_A.x,res3.b.l);
                  break;
                case ADDRDISPADD:   // (D16,An)
                  PutValue(WORKING_REG_A.x + (signed short int)Pipe2.w,res3.b.l);
                  _pc+=2;
                  break;
                case ADDRIDXADD:   // (D8,An,Xn)
                  if(!DISPLACEMENT_SIZE) {
                    PutValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res3.b.l);
                    }
                  else {
                    PutValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.b.l);
                    }
                  _pc+=2;
                  break;
                }
              }
            break;
          default:      // ADDQ
            res2.b.l=(Pipe1 & 0b0000111000000000) >> 9;
            if(!res2.b.l)
              res2.b.l=8;
            switch(ADDRESSING) {
              case ABSOLUTEADD:    // abs
                switch(ABSOLUTEADD_SIZE) {
                  case ABSSSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res1.b.l = GetValue(Pipe2.w);
                        res3.w = (WORD)res1.b.l + (WORD)res2.b.l;
                        PutValue(Pipe2.w, res3.b.l);
                        break;
                      case WORD_SIZE:
                        res1.w = GetShortValue(Pipe2.w);
                        res3.x = (DWORD)res1.w + (DWORD)res2.b.l;
                        PutShortValue(Pipe2.w, res3.w);
                        break;
                      case DWORD_SIZE:
                        res1.w = GetShortValue(Pipe2.w);
                        res3.x = res1.x + (DWORD)res2.b.l;
                        PutIntValue(Pipe2.w, res3.x);
                        break;
                      }
                    _pc+=2;
                    break;
                  case ABSLSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res1.b.l = GetValue(Pipe2.d);
                        res3.w = (WORD)res1.b.l + (WORD)res2.b.l;
                        PutValue(Pipe2.d, res3.b.l);
                        break;
                      case WORD_SIZE:
                        res1.w = GetShortValue(Pipe2.d);
                        res3.x = (DWORD)res1.w + (DWORD)res2.b.l;
                        PutShortValue(Pipe2.d, res3.w);
                        break;
                      case DWORD_SIZE:
                        res1.w = GetIntValue(Pipe2.d);
                        res3.x = res1.x + (DWORD)res2.b.l;
                        PutIntValue(Pipe2.d,res3.x);
                        break;
                      }
                    _pc+=4;
                    break;
                  case PCIDXSUBADD:   // qua no
                  case PCDISPSUBADD:
                  case IMMSUBADD:
                    break;
                  }
                break;
              case DATAREGADD:   // Dn
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = WORKING_REG_D.b.b0;
                    res3.w = (WORD)res1.b.l + (WORD)res2.b.l;
                    WORKING_REG_D.b.b0 = (WORD)res1.b.l;
                    break;
                  case WORD_SIZE:
                    res1.w = WORKING_REG_D.w.l;
                    res3.x = (DWORD)res1.w + (DWORD)res2.b.l;
                    WORKING_REG_D.w.l = res3.w;
                    break;
                  case DWORD_SIZE:
                    res1.x = WORKING_REG_D.x;
                    res3.x = res1.x + (DWORD)res2.b.l;
                    WORKING_REG_D.x = res3.x;
                    break;
                  }
                break;
              case ADDRREGADD:   // An
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    break;
                  case WORD_SIZE:
                    res3.x = WORKING_REG_A.x += (DWORD)res2.b.l;		// tutto cmq.. dice..
                    break;
                  case DWORD_SIZE:
                    res3.x = WORKING_REG_A.x += (DWORD)res2.b.l;
                    break;
                  }
                goto noAggFlag;
                break;
              case ADDRADD:   // (An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GetValue(WORKING_REG_A.x);
                    res3.w = (WORD)res1.b.l + (WORD)res2.b.l;
                    PutValue(WORKING_REG_A.x,res3.b.l);
                    break;
                  case WORD_SIZE:
                    res1.w = GetShortValue(WORKING_REG_A.x);
                    res3.x = (DWORD)res1.w + (DWORD)res2.b.l;
                    PutShortValue(WORKING_REG_A.x,res3.w);
                    break;
                  case DWORD_SIZE:
                    res1.x = GetIntValue(WORKING_REG_A.x);
                    res3.x = res1.x + (DWORD)res2.b.l;
                    PutIntValue(WORKING_REG_A.x,res3.x);
                    break;
                  }
                break;
              case ADDRPINCADD:   // (An)+
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GetValue(WORKING_REG_A.x);
                    res3.w = (WORD)res1.b.l + (WORD)res2.b.l;
                    PutValue(WORKING_REG_A.x,res3.b.l);
                    WORKING_REG_A.x++;
                    break;
                  case WORD_SIZE:
                    res1.w = GetShortValue(WORKING_REG_A.x);
                    res3.x = (DWORD)res1.w + (DWORD)res2.b.l;
                    PutShortValue(WORKING_REG_A.x,res3.w);
                    WORKING_REG_A.x+=2;
                    break;
                  case DWORD_SIZE:
                    res1.x = GetIntValue(WORKING_REG_A.x);
                    res3.x = res1.x + (DWORD)res2.b.l;
                    PutIntValue(WORKING_REG_A.x,res3.x);
                    WORKING_REG_A.x+=4;
                    break;
                  }
                break;
              case ADDRPDECADD:   // -(An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    WORKING_REG_A.x--;
                    res1.b.l = GetValue(WORKING_REG_A.x);
                    res3.w = (WORD)res1.b.l + (WORD)res2.b.l;
                    break;
                  case WORD_SIZE:
                    WORKING_REG_A.x-=2;
                    res1.w = GetShortValue(WORKING_REG_A.x);
                    res3.x = (DWORD)res1.w + (DWORD)res2.b.l;
                    PutShortValue(WORKING_REG_A.x,res3.w);
                    break;
                  case DWORD_SIZE:
                    WORKING_REG_A.x-=4;
                    res1.x = GetIntValue(WORKING_REG_A.x);
                    res3.x = res1.x + (DWORD)res2.b.l;
                    PutIntValue(WORKING_REG_A.x,res3.x);
                    break;
                  }
                break;
              case ADDRDISPADD:   // (D16,An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                    res3.w = (WORD)res1.b.l + (WORD)res2.b.l;;
                    PutValue(WORKING_REG_A.x + (signed short int)Pipe2.w,res3.b.l);
                    break;
                  case WORD_SIZE:
                    res1.w = GetShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                    res3.x = (DWORD)res1.w + (DWORD)res2.b.l;
                    PutShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w,res3.w);
                    break;
                  case DWORD_SIZE:
                    res3.x = GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                    res3.x = res1.x + (DWORD)res2.b.l;
                    PutIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w, res3.x);
                    break;
                  }
                _pc+=2;
                break;
              case ADDRIDXADD:   // (D8,An,Xn)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    if(!DISPLACEMENT_SIZE) {
                      res1.b.l = GetValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                      res3.w = (WORD)res1.b.l + (WORD)res2.b.l;;
                      PutValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res3.b.l);
                      }
                    else {
                      res1.b.l = GetValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                      res3.w = (WORD)res1.b.l + (WORD)res2.b.l;;
                      PutValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.b.l);
                      }
                    break;
                  case WORD_SIZE:
                    if(!DISPLACEMENT_SIZE) {
                      res1.w = GetValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                      res3.x = (DWORD)res1.w + (DWORD)res2.b.l;
                      PutValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.w);
                      }
                    else {
                      res1.w = GetValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                      res3.x = (DWORD)res1.w + (DWORD)res2.b.l;
                      PutValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.w);
                      }
                    break;
                  case DWORD_SIZE:
                    if(!DISPLACEMENT_SIZE) {
                      res1.x = GetValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                      res3.x = res1.x + (DWORD)res2.b.l;
                      PutValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.x);
                      }
                    else {
                      res1.x = GetValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                      res3.x = res1.x + (DWORD)res2.b.l;
                      PutValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.x);
                      }
                    break;
                  }
                _pc+=2;
                break;
              }
            goto aggFlagA;
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
          case SIZE_ELSE:    // Scc DBcc
            switch(HIBYTE(Pipe1) & 0xf) {
              case 0x1:
                res3.b.l=0;    // credo...!
                break;
              case 0x3:
                if(_f.CCR.Zero || (_f.CCR.Sign ^ _f.CCR.Ovf))     // lower or equal
                  res3.b.l=0xff;
                else
                  res3.b.l=0x00;
                break;
              case 0x5:
                if(_f.CCR.Carry)      // carry set
                  res3.b.l=0xff;
                else
                  res3.b.l=0x00;
                break;
              case 0x7:
                if(_f.CCR.Zero)      // equal
                  res3.b.l=0xff;
                else
                  res3.b.l=0x00;
                break;
              case 0x9:
                if(_f.CCR.Ovf)       // overflow set
                  res3.b.l=0xff;
                else
                  res3.b.l=0x00;
                break;
              case 0xb:
                if(_f.CCR.Sign)      // minus
                  res3.b.l=0xff;
                else
                  res3.b.l=0x00;
                break;
              case 0xd:   // Bls
                if(_f.CCR.Sign ^ _f.CCR.Ovf)     // less than
                  res3.b.l=0xff;
                else
                  res3.b.l=0x00;
                break;
              case 0xf:
                if(_f.CCR.Carry || _f.CCR.Zero)     // less or equal
                  res3.b.l=0xff;
                else
                  res3.b.l=0x00;
                break;
              }
            goto doDBccScc;
            break;
          default:        // SUBQ
            res2.b.l=(Pipe1 & 0b0000111000000000) >> 9;
            if(!res2.b.l)
              res2.b.l=8;
            switch(ADDRESSING) {
              case ABSOLUTEADD:    // abs
                switch(ABSOLUTEADD_SIZE) {
                  case ABSSSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res1.b.l = GetValue(Pipe2.w);
                        res3.w = (WORD)res1.b.l - (WORD)res2.b.l;
                        PutValue(Pipe2.w,res3.b.l);
                        break;
                      case WORD_SIZE:
                        res1.w = GetShortValue(Pipe2.w);
                        res3.x = (DWORD)res1.w - (DWORD)res2.b.l;
                        PutShortValue(Pipe2.w,res3.w);
                        break;
                      case DWORD_SIZE:
                        res1.x = GetIntValue(Pipe2.w);
                        res3.x = res1.x - (DWORD)res2.b.l;
                        PutIntValue(Pipe2.w,res3.x); 
                        break;
                      }
                    _pc+=2;
                    break;
                  case ABSLSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res1.b.l = GetValue(Pipe2.d);
                        res3.w = (WORD)res1.b.l - (WORD)res2.b.l;
                        PutValue(Pipe2.d,res3.b.l);
                        break;
                      case WORD_SIZE:
                        res1.w = GetShortValue(Pipe2.d);
                        res3.x = (DWORD)res1.w - (DWORD)res2.b.l;
                        PutShortValue(Pipe2.d,res3.w);
                        break;
                      case DWORD_SIZE:
                        res1.x = GetIntValue(Pipe2.d); 
                        res3.x = res1.x - (DWORD)res2.b.l;
                        PutIntValue(Pipe2.d,res3.x); 
                        break;
                      }
                    _pc+=4;
                    break;
                  case PCIDXSUBADD:   // qua no
                  case PCDISPSUBADD:
                  case IMMSUBADD:
                    break;
                  }
                break;
              case DATAREGADD:   // Dn
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = WORKING_REG_D.b.b0;
                    res3.w = (WORD)res1.b.l - (WORD)res2.b.l;
                    WORKING_REG_D.b.b0 = res3.b.l;
                    break;
                  case WORD_SIZE:
                    res1.w = WORKING_REG_D.w.l;
                    res3.x = (DWORD)res1.w - (DWORD)res2.b.l;
                    WORKING_REG_D.w.l = res3.w;
                    break;
                  case DWORD_SIZE:
                    res1.x = WORKING_REG_D.x;
                    res3.x = res1.x - (DWORD)res2.b.l;
                    WORKING_REG_D.x = res3.x;
                    break;
                  }
                break;
              case ADDRREGADD:   // An
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    break;
                  case WORD_SIZE:
                    res3.x = (WORKING_REG_A.x -= (DWORD)res2.b.l);		// tutto cmq... dice..
                    break;
                  case DWORD_SIZE:
                    res3.x = (WORKING_REG_A.x -= (DWORD)res2.b.l);
                    break;
                  }
                goto noAggFlag;
                break;
              case ADDRADD:   // (An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GetValue(WORKING_REG_A.x);
                    res3.w = (WORD)res1.b.l - (WORD)res2.b.l;
                    PutValue(WORKING_REG_A.x,res3.b.l);
                    break;
                  case WORD_SIZE:
                    res1.w = GetShortValue(WORKING_REG_A.x);
                    res3.x = (DWORD)res1.w - (DWORD)res2.b.l;
                    PutShortValue(WORKING_REG_A.x,res3.w);
                    break;
                  case DWORD_SIZE:
                    res1.x = GetIntValue(WORKING_REG_A.x);
                    res3.x = res1.x - (DWORD)res2.b.l;
                    PutIntValue(WORKING_REG_A.x,res3.x);
                    break;
                  }
                break;
              case ADDRPINCADD:   // (An)+
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GetValue(WORKING_REG_A.x);
                    res3.w = (WORD)res1.b.l - (WORD)res2.b.l;
                    PutValue(WORKING_REG_A.x,res3.b.l);
                    WORKING_REG_A.x++;
                    break;
                  case WORD_SIZE:
                    res1.w = GetShortValue(WORKING_REG_A.x);
                    res3.x = (DWORD)res1.w - (DWORD)res2.b.l;
                    PutShortValue(WORKING_REG_A.x,res3.w);
                    WORKING_REG_A.x+=2;
                    break;
                  case DWORD_SIZE:
                    res1.x = GetIntValue(WORKING_REG_A.x);
                    res3.x = res1.x - (DWORD)res2.b.l;
                    PutIntValue(WORKING_REG_A.x,res3.x);
                    WORKING_REG_A.x+=4;
                    break;
                  }
                break;
              case ADDRPDECADD:   // -(An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    WORKING_REG_A.x--;
                    res1.b.l = GetValue(WORKING_REG_A.x);
                    res3.w = (WORD)res1.b.l - (WORD)res2.b.l;
                    PutValue(WORKING_REG_A.x,res3.b.l);
                    break;
                  case WORD_SIZE:
                    WORKING_REG_A.x-=2;
                    res1.w = GetShortValue(WORKING_REG_A.x);
                    res3.x = (DWORD)res1.w - (DWORD)res2.b.l;
                    PutShortValue(WORKING_REG_A.x,res3.w);
                    break;
                  case DWORD_SIZE:
                    WORKING_REG_A.x-=4;
                    res1.x = GetIntValue(WORKING_REG_A.x);
                    res3.x = res1.x - (DWORD)res2.b.l;
                    PutIntValue(WORKING_REG_A.x,res3.x);
                    break;
                  }
                break;
              case ADDRDISPADD:   // (D16,An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                    res3.w = (WORD)res1.b.l - (WORD)res2.b.l;
                    PutValue(WORKING_REG_A.x + (signed short int)Pipe2.w, res3.b.l);
                    break;
                  case WORD_SIZE:
                    res1.w = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                    res3.x = (DWORD)res1.w - (DWORD)res2.b.l;
                    PutValue(WORKING_REG_A.x + (signed short int)Pipe2.w, res3.w);
                    break;
                  case DWORD_SIZE:
                    res1.x = GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                    res3.x = res1.x - (DWORD)res2.b.l;
                    PutIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w, res3.x);
                    break;
                  }
                _pc+=2;
                break;
              case ADDRIDXADD:   // (D8,An,Xn)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    if(!DISPLACEMENT_SIZE) {
                      res1.b.l = GetValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                      res3.w = (WORD)res1.b.l - (WORD)res2.b.l;
                      PutValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.b.l);
                      }
                    else {
                      res1.b.l = GetValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                      res3.w = (WORD)res1.b.l - (WORD)res2.b.l;
                      PutValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.b.l);
                      }
                    break;
                  case WORD_SIZE:
                    if(!DISPLACEMENT_SIZE) {
                      res1.w = GetValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                      res3.x = (DWORD)res1.w - (DWORD)res2.b.l;
                      PutValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.w);
                      }
                    else {
                      res1.w = GetValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                      res3.x = (DWORD)res1.w - (DWORD)res2.b.l;
                      PutValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.w);
                      }
                    break;
                  case DWORD_SIZE:
                    if(!DISPLACEMENT_SIZE) {
                      res1.x = GetValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                      res3.x = res1.x - (DWORD)res2.b.l;
                      PutValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.x);
                      }
                    else {
                      res1.x = GetValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                      res3.x = res1.x - (DWORD)res2.b.l;
                      PutValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.x);
                      }
                    break;
                  }
                _pc+=2;
                break;
              }
            goto aggFlagS;
            break;
          }
				break;
        
			case 0x60:   // BRA
do_bra:
        if(LOBYTE(Pipe1))
          _pc+=(signed char)LOBYTE(Pipe1);
        else
          _pc+=(signed short int)Pipe2.w;
				break;
        
			case 0x61:   // BSR
        _sp-=4;
        if(LOBYTE(Pipe1)) {
          PutIntValue(_sp,_pc);  // low word prima
          _pc+=(signed char)LOBYTE(Pipe1);
          }
        else {
          PutIntValue(_sp,_pc+2);  // low word prima
          _pc+=(signed short int)Pipe2.w;
          }
				break;
        
			case 0x62:   // Bc Bhi
        if(!_f.CCR.Carry && !_f.CCR.Zero)     // higher !C !Z
          goto do_bra;
        else
          if(!LOBYTE(Pipe1))
            _pc+=2;
				break;
			case 0x63:
        if(_f.CCR.Zero || (_f.CCR.Sign ^ _f.CCR.Ovf))     // lower or equal
          goto do_bra;
        else
          if(!LOBYTE(Pipe1))
            _pc+=2;
				break;
			case 0x64:
        if(!_f.CCR.Carry)     // carry clear
          goto do_bra;
        else
          if(!LOBYTE(Pipe1))
            _pc+=2;
				break;
			case 0x65:
        if(_f.CCR.Carry)      // carry set
          goto do_bra;
        else
          if(!LOBYTE(Pipe1))
            _pc+=2;
				break;
			case 0x66:
        if(!_f.CCR.Zero)      // not equal
          goto do_bra;
        else
          if(!LOBYTE(Pipe1))
            _pc+=2;
				break;
			case 0x67:
        if(_f.CCR.Zero)      // equal
          goto do_bra;
        else
          if(!LOBYTE(Pipe1))
            _pc+=2;
				break;
			case 0x68:
        if(!_f.CCR.Ovf)      // overflow clear
          goto do_bra;
        else
          if(!LOBYTE(Pipe1))
            _pc+=2;
				break;
			case 0x69:
        if(_f.CCR.Ovf)       // overflow set
          goto do_bra;
        else
          if(!LOBYTE(Pipe1))
            _pc+=2;
				break;
			case 0x6a:
        if(!_f.CCR.Sign)     // plus
          goto do_bra;
        else
          if(!LOBYTE(Pipe1))
            _pc+=2;
				break;
			case 0x6b:
        if(_f.CCR.Sign)      // minus
          goto do_bra;
        else
          if(!LOBYTE(Pipe1))
            _pc+=2;
				break;
			case 0x6c:
        if(!(_f.CCR.Sign ^ _f.CCR.Ovf))     // greater or equal
          goto do_bra;
        else
          if(!LOBYTE(Pipe1))
            _pc+=2;
				break;
			case 0x6d:
        if(_f.CCR.Sign ^ _f.CCR.Ovf)     // less than
          goto do_bra;
        else
          if(!LOBYTE(Pipe1))
            _pc+=2;
				break;
			case 0x6e:
        if(_f.CCR.Zero && !(_f.CCR.Sign ^ _f.CCR.Ovf))     // greater than
          goto do_bra;
        else
          if(!LOBYTE(Pipe1))
            _pc+=2;
				break;
			case 0x6f:
        if(_f.CCR.Carry || _f.CCR.Zero)     // less or equal
          goto do_bra;
        else
          if(!LOBYTE(Pipe1))
            _pc+=2;
				break;
        
			case 0x70:   // MOVEQ
			case 0x72:
			case 0x74:
			case 0x76:
			case 0x78:
			case 0x7a:
			case 0x7c:
			case 0x7e:
        DEST_REG_D.x = (int8_t)LOBYTE(Pipe1);
        goto aggFlag4;
				break;
        
			case 0x80:   // DIVU OR
			case 0x82:
			case 0x84:
			case 0x86:
			case 0x88:
			case 0x8a:
			case 0x8c:
			case 0x8e:
			case 0x81:   // DIVS SBCD OR
			case 0x83:
			case 0x85:
			case 0x87:
			case 0x89:
			case 0x8b:
			case 0x8d:
			case 0x8f:
        if(OPERAND_SIZE == SIZE_ELSE) {    // DIVU DIVS
          res1.x=DEST_REG_D.x;
          switch(ADDRESSING) {
            case ABSOLUTEADD:
              switch(ABSOLUTEADD_SIZE) {
                case ABSSSUBADD:
                  res2.w = GetShortValue(Pipe2.w);
                  _pc+=2;
                  break;
                case ABSLSUBADD:
                  res2.w = GetShortValue(Pipe2.d); 
                  _pc+=4;
                  break;
                case PCIDXSUBADD:   // (D16,PC)
                  res2.w = GetShortValue(_pc+(signed short int)Pipe2.w);
                  _pc+=2;
                  break;
                case PCDISPSUBADD:   // (D8,Xn,PC)
                  if(!DISPLACEMENT_SIZE)
                    res2.w = GetShortValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  else
                    res2.w = GetShortValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  _pc+=2;
                  break;
                case IMMSUBADD:
                  res2.w = Pipe2.w; 
                  _pc+=2;
                  break;
                }
              break;
            case DATAREGADD:   // Dn
              res2.w = WORKING_REG_D.w.l;
              break;
            case ADDRREGADD:   // An qua NO!
              break;
            case ADDRADD:   // (An)
              res2.w = GetShortValue(WORKING_REG_A.x);
              break;
            case ADDRPINCADD:   // (An)+
              res2.w = GetShortValue(WORKING_REG_A.x);
              WORKING_REG_A.x+=2;
              break;
            case ADDRPDECADD:   // -(An)
              WORKING_REG_A.x-=2;
              res2.w = GetShortValue(WORKING_REG_A.x);
              break;
            case ADDRDISPADD:   // (D16,An)
              res2.w = GetShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
              _pc+=2;
              break;
            case ADDRIDXADD:   // (D8,An,Xn)
              if(!DISPLACEMENT_SIZE) {
                res2.w = GetShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                }
              else {
                res2.w = GetShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                }
              _pc+=2;
              break;
            }
          if(!res2.w) {
            res3.b.l=5;
            goto doTrap;
            }
          if(Pipe1 & 0b0000000100000000) {		// DIVS
  					res3.x = (signed int)res1.x / (signed short int)res2.w;
  					DEST_REG_D.x = MAKELONG(res3.w,
                    (signed int)res1.x % (signed short int)res2.w);
						}
					else {		// DIVU
  					res3.x = res1.x / res2.w;
  					DEST_REG_D.x = MAKELONG(res3.w, res1.x % res2.w);
            }
          _f.CCR.Zero=!res3.w;
          _f.CCR.Sign=!!(res3.w & 0x8000);
          _f.CCR.Carry=0;
          _f.CCR.Ovf=!!(HIWORD(res3.x));
					}
        else {      // SBCD OR 
          if(Pipe1 & 0b0000000100000000) { // direction... 
            switch(ADDRESSING) {
              case ABSOLUTEADD:    // 
                switch(ABSOLUTEADD_SIZE) {
                  case ABSSSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res1.b.l = GetValue(Pipe2.w);
                        break;
                      case WORD_SIZE:
                        res1.w = GetShortValue(Pipe2.w);
                        break;
                      case DWORD_SIZE:
                        res1.x = GetIntValue(Pipe2.w);
                        break;
                      }
                    break;
                  case ABSLSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res1.b.l = GetValue(Pipe2.d);
                        break;
                      case WORD_SIZE:
                        res1.w = GetShortValue(Pipe2.d);
                        break;
                      case DWORD_SIZE:
                        res1.x = GetIntValue(Pipe2.d);
                        break;
                      }
                    break;
                  case PCIDXSUBADD:   // (D16,PC)
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res1.b.l = GetValue(_pc+(signed short int)Pipe2.w);
                        break;
                      case WORD_SIZE:
                        res1.w = GetShortValue(_pc+(signed short int)Pipe2.w);
                        break;
                      case DWORD_SIZE:
                        res1.x = GetIntValue(_pc+(signed short int)Pipe2.w);
                        break;
                      }
                    break;
                  case PCDISPSUBADD:   // (D8,Xn,PC)
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        if(!DISPLACEMENT_SIZE)
                          res1.b.l = GetValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                        else
                          res1.b.l = GetValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                        break;
                      case WORD_SIZE:
                        if(!DISPLACEMENT_SIZE)
                          res1.w = GetShortValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                        else
                          res1.w = GetShortValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                        break;
                      case DWORD_SIZE:
                        if(!DISPLACEMENT_SIZE)
                          res1.x = GetIntValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                        else
                          res1.x = GetIntValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                        break;
                      }
                    break;
                  case IMMSUBADD:   // NON dovrebbe esistere qua perché confluisce in ORI, ma...
                    res1.x = Pipe2.d; 
                    _pc+=AdvPipe(_pc,4);
                    break;
                  }
                break;
              case DATAREGADD:   // Dn
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = WORKING_REG_D.b.b0;
                    break;
                  case WORD_SIZE:
                    res1.w = WORKING_REG_D.w.l;
                    break;
                  case DWORD_SIZE:
                    res1.w = WORKING_REG_D.x;
                    break;
                  }
                break;
              case ADDRREGADD:   // An qua SBCD dovrebbe andare anche su DATAREG v. anche ABCD
                if(OPERAND_SIZE == 0) {
                  res1.b.l = GetValue(--WORKING_REG_A.x);
                  res2.b.l = GetValue(--DEST_REG_A.x) + _f.CCR.Ext;
                  res3.w = (res1.b.l & 0xf) - (res2.b.l & 0xf);
                  res3.w = (((res1.b.l & 0xf0) >> 4) - ((res2.b.l & 0xf0) >> 4) - (res3.b.h ? 1 : 0)) | 
                            res3.b.l;
                  PutValue(DEST_REG_A.x, res3.b.l);
                  }
                _f.CCR.Zero=!res3.b.l;
                _f.CCR.Ext=_f.CCR.Carry=!!res3.b.h;
                goto noAggFlag;
                break;
              case ADDRADD:   // (An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GetValue(WORKING_REG_A.x);
                    break;
                  case WORD_SIZE:
                    res1.w = GetShortValue(WORKING_REG_A.x);
                    break;
                  case DWORD_SIZE:
                    res1.x = GetIntValue(WORKING_REG_A.x);
                    break;
                  }
                break;
              case ADDRPINCADD:   // (An)+
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GetValue(WORKING_REG_A.x);
                    break;
                  case WORD_SIZE:
                    res1.w = GetShortValue(WORKING_REG_A.x);
                    break;
                  case DWORD_SIZE:
                    res1.x = GetIntValue(WORKING_REG_A.x);
                    break;
                  }
                break;
              case ADDRPDECADD:   // -(An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    WORKING_REG_A.x--;
                    res1.b.l = GetValue(WORKING_REG_A.x);
                    break;
                  case WORD_SIZE:
                    WORKING_REG_A.x-=2;
                    res1.w = GetShortValue(WORKING_REG_A.x);
                    break;
                  case DWORD_SIZE:
                    WORKING_REG_A.x-=4;
                    res1.x = GetIntValue(WORKING_REG_A.x);
                    break;
                  }
                break;
              case ADDRDISPADD:   // (D16,An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                    break;
                  case WORD_SIZE:
                    res1.w = GetShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                    break;
                  case DWORD_SIZE:
                    res1.x = GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                    break;
                  }
                break;
              case ADDRIDXADD:   // (D8,An,Xn)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res1.b.l = GetValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    else
                      res1.b.l = GetValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    break;
                  case WORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res1.w = GetShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    else
                      res1.w = GetShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    break;
                  case DWORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res1.x = GetIntValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    else
                      res1.x = GetIntValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    break;
                  }
                break;
              }
            }
          else {
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res1.b.l = DEST_REG_D.b.b0;
                break;
              case WORD_SIZE:
                res1.w = DEST_REG_D.w.l;
                break;
              case DWORD_SIZE:
                res1.x = DEST_REG_D.x;
                break;
              }
            }

          if(Pipe1 & 0b0000000100000000) { // direction
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res2.b.l = DEST_REG_D.b.b0;
                break;
              case WORD_SIZE:
                res2.w = DEST_REG_D.w.l;
                break;
              case DWORD_SIZE:
                res2.x = DEST_REG_D.x;
                break;
              }
            }
          else {
            switch(ADDRESSING) {
              case ABSOLUTEADD:    // #imm
                switch(ABSOLUTEADD_SIZE) {
                  case ABSSSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res2.b.l = GetValue(Pipe2.w);
                        break;
                      case WORD_SIZE:
                        res2.w = GetShortValue(Pipe2.w);
                        break;
                      case DWORD_SIZE:
                        res2.x = GetIntValue(Pipe2.w);
                        break;
                      }
                    break;
                  case ABSLSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res2.b.l = GetValue(Pipe2.d);
                        break;
                      case WORD_SIZE:
                        res2.w = GetShortValue(Pipe2.d);
                        break;
                      case DWORD_SIZE:
                        res2.x = GetIntValue(Pipe2.d);
                        break;
                      }
                    break;
                  case PCIDXSUBADD:   // (D16,PC) qua NO!
                  case PCDISPSUBADD:   // (D8,Xn,PC)
                  case IMMSUBADD:
                    break;
                  }
                break;
              case DATAREGADD:   // Dn .. in un punto il Doc dice questo no qua...
                #warning MA VA IN CONFLITTO CON SBCD! v. anche sopra
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = WORKING_REG_D.b.b0;
                    break;
                  case WORD_SIZE:
                    res2.w = WORKING_REG_D.w.l;
                    break;
                  case DWORD_SIZE:
                    res2.x = WORKING_REG_D.x;
                    break;
                  }
                break;
              case ADDRREGADD:   // An
                break;
              case ADDRADD:   // (An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = GetValue(WORKING_REG_A.x);
                    break;
                  case WORD_SIZE:
                    res2.w = GetShortValue(WORKING_REG_A.x);
                    break;
                  case DWORD_SIZE:
                    res2.x = GetIntValue(WORKING_REG_A.x);
                    break;
                  }
                break;
              case ADDRPINCADD:   // (An)+
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = GetValue(WORKING_REG_A.x);
                    break;
                  case WORD_SIZE:
                    res2.w = GetShortValue(WORKING_REG_A.x);
                    break;
                  case DWORD_SIZE:
                    res2.x = GetIntValue(WORKING_REG_A.x);
                    break;
                  }
                break;
              case ADDRPDECADD:   // -(An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    WORKING_REG_A.x--;
                    res2.b.l = GetValue(WORKING_REG_A.x);
                    break;
                  case WORD_SIZE:
                    WORKING_REG_A.x-=2;
                    res2.w = GetShortValue(WORKING_REG_A.x);
                    break;
                  case DWORD_SIZE:
                    WORKING_REG_A.x-=4;
                    res2.x = GetIntValue(WORKING_REG_A.x);
                    break;
                  }
                break;
              case ADDRDISPADD:   // (D16,An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                    break;
                  case WORD_SIZE:
                    res2.w = GetShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                    break;
                  case DWORD_SIZE:
                    res2.x = GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                    break;
                  }
                break;
              case ADDRIDXADD:   // (D8,An,Xn)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res2.b.l = GetValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    else
                      res2.b.l = GetValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    break;
                  case WORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res2.w = GetShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    else
                      res2.w = GetShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    break;
                  case DWORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res2.x = GetIntValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    else
                      res2.x = GetIntValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    break;
                  }
                break;
              }
            }

          switch(OPERAND_SIZE) {
            case BYTE_SIZE:
              res3.b.l = res1.b.l | res2.b.l;
              break;
            case WORD_SIZE:
              res3.w = res1.w | res2.w;
              break;
            case DWORD_SIZE:
              res3.x = res1.x | res2.x;
              break;
            }

          if(Pipe1 & 0b0000000100000000) { // direction
            switch(ADDRESSING) {
              case ABSOLUTEADD:    // #imm
                switch(ABSOLUTEADD_SIZE) {
                  case ABSSSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        PutValue(Pipe2.w, res3.b.l);
                        break;
                      case WORD_SIZE:
                        PutShortValue(Pipe2.w, res3.w);
                        break;
                      case DWORD_SIZE:
                        PutIntValue(Pipe2.w, res3.x);
                        break;
                      }
                    _pc+=2;
                    break;
                  case ABSLSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        PutValue(Pipe2.d, res3.b.l);
                        break;
                      case WORD_SIZE:
                        PutShortValue(Pipe2.d, res3.w);
                        break;
                      case DWORD_SIZE:
                        PutIntValue(Pipe2.d, res3.x);
                        break;
                      }
                    _pc+=4;
                    break;
                  case PCIDXSUBADD:   // (D16,PC)
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        PutValue(_pc+(signed short int)Pipe2.w, res1.b.l);
                        break;
                      case WORD_SIZE:
                        PutShortValue(_pc+(signed short int)Pipe2.w,res1.w);
                        break;
                      case DWORD_SIZE:
                        PutIntValue(_pc+(signed short int)Pipe2.w,res1.x);
                        break;
                      }
                    _pc+=2;
                    break;
                  case PCDISPSUBADD:   // (D8,Xn,PC)
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        if(!DISPLACEMENT_SIZE)
                          PutValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res1.b.l);
                        else
                          PutValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res1.b.l);
                        break;
                      case WORD_SIZE:
                        if(!DISPLACEMENT_SIZE)
                          PutShortValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res1.w);
                        else
                          PutShortValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res1.w);
                        break;
                      case DWORD_SIZE:
                        if(!DISPLACEMENT_SIZE)
                          PutIntValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res1.x);
                        else
                          PutIntValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res1.x);
                        break;
                      }
                    _pc+=2;
                    break;
                  case IMMSUBADD:   // qua no!
                    break;
                  }
                break;
              case DATAREGADD:   // Dn .. in un punto il Doc dice questo no qua...
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    DEST_REG_D.b.b0 = res3.b.l;
                    break;
                  case WORD_SIZE:
                    DEST_REG_D.w.l = res3.w;
                    break;
                  case DWORD_SIZE:
                    DEST_REG_D.x = res3.x;
                    break;
                  }
                break;
              case ADDRREGADD:   // An qua NO!
                break;
              case ADDRADD:   // (An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    PutValue(WORKING_REG_A.x,res3.b.l);
                    break;
                  case WORD_SIZE:
                    PutShortValue(WORKING_REG_A.x,res3.w);
                    break;
                  case DWORD_SIZE:
                    PutIntValue(WORKING_REG_A.x,res3.x);
                    break;
                  }
                break;
              case ADDRPINCADD:   // (An)+
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    PutValue(WORKING_REG_A.x,res3.b.l);
                    WORKING_REG_A.x++;
                    break;
                  case WORD_SIZE:
                    PutShortValue(WORKING_REG_A.x,res3.w);
                    WORKING_REG_A.x+=2;
                    break;
                  case DWORD_SIZE:
                    PutIntValue(WORKING_REG_A.x,res3.w);
                    WORKING_REG_A.x+=4;
                    break;
                  }
                break;
              case ADDRPDECADD:   // -(An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    PutValue(WORKING_REG_A.x,res3.b.l);
                    break;
                  case WORD_SIZE:
                    PutShortValue(WORKING_REG_A.x,res3.w);
                    break;
                  case DWORD_SIZE:
                    PutIntValue(WORKING_REG_A.x,res3.x);
                    break;
                  }
                break;
              case ADDRDISPADD:   // (D16,An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    PutValue(WORKING_REG_A.x + (signed short int)Pipe2.w,res3.b.l);
                    break;
                  case WORD_SIZE:
                    PutShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w,res3.w);
                    break;
                  case DWORD_SIZE:
                    PutIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w,res3.x);
                    break;
                  }
                _pc+=2;
                break;
              case ADDRIDXADD:   // (D8,An,Xn)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      PutValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res3.b.l);
                    else
                      PutValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res3.b.l);
                    break;
                  case WORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      PutShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res3.w);
                    else
                      PutShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res3.w);
                    break;
                  case DWORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      PutIntValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res3.x);
                    else
                      PutIntValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res3.x);
                    break;
                  }
                _pc+=2;
                break;
              }
            }
          else {
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                DEST_REG_D.b.b0 = res3.b.l;
                break;
              case WORD_SIZE:
                DEST_REG_D.w.l = res3.w;
                break;
              case DWORD_SIZE:
                DEST_REG_D.x = res3.x;
                break;
              }
            }
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
        if(OPERAND_SIZE == SIZE_ELSE) { // SUBA
          switch(ADDRESSING) {
            case ABSOLUTEADD:    // 
              switch(ABSOLUTEADD_SIZE) {
                case ABSSSUBADD:
                  if(!(HIBYTE(Pipe1) & 0b00000001)) { // size
                    res1.w = GetShortValue(Pipe2.w);
                    _pc+=2;
                    }
                  else {
                    res1.x = GetIntValue(Pipe2.w);
                    _pc+=4;
                    }
                  break;
                case ABSLSUBADD:
                  if(!(HIBYTE(Pipe1) & 0b00000001)) { // 
                    res1.w = GetShortValue(Pipe2.d);
                    _pc+=2;
                    }
                  else {
                    res1.x = GetIntValue(Pipe2.d);
                    _pc+=4;
                    }
                  break;
                case PCIDXSUBADD:   // (D16,PC)
                  if(!(HIBYTE(Pipe1) & 0b00000001)) { // 
                    res1.w = GetShortValue(_pc+(signed short int)Pipe2.w);
                    }
                  else {
                    res1.x = GetIntValue(_pc+(signed short int)Pipe2.w);
                    }
                  _pc+=2;
                  break;
                case PCDISPSUBADD:   // (D8,Xn,PC)
                  if(!(HIBYTE(Pipe1) & 0b00000001)) {
                    if(!DISPLACEMENT_SIZE)
                      res1.w = GetShortValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    else
                      res1.w = GetShortValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    }
                  else {
                    if(!DISPLACEMENT_SIZE)
                      res1.x = GetIntValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    else
                      res1.x = GetIntValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    }
                  _pc+=2;
                  break;
                case IMMSUBADD:
                  res1.x = Pipe2.d;
                  _pc+=4;
                  break;
                }
              break;
            case DATAREGADD:   // Dn
              if(!(HIBYTE(Pipe1) & 0b00000001))  // size
                res1.w = WORKING_REG_D.w.l;
              else
                res1.x = WORKING_REG_D.x;
              break;
            case ADDRREGADD:   // An
              if(!(HIBYTE(Pipe1) & 0b00000001))  // size
                res1.w = WORKING_REG_A.w.l;
              else
                res1.x = WORKING_REG_A.x;
              break;
            case ADDRADD:   // (An)
              if(!(HIBYTE(Pipe1) & 0b00000001))  // 
                res1.w = GetShortValue(WORKING_REG_A.x);
              else
                res1.x = GetIntValue(WORKING_REG_A.x);
              break;
            case ADDRPINCADD:   // (An)+
              if(!(HIBYTE(Pipe1) & 0b00000001)) { // size
                res1.w = GetShortValue(WORKING_REG_A.x);
                WORKING_REG_A.x+=2;
                }
              else {
                res1.x = GetIntValue(WORKING_REG_A.x);
                WORKING_REG_A.x+=4;
                }
              break;
            case ADDRPDECADD:   // -(An)
              if(!(HIBYTE(Pipe1) & 0b00000001)) { // size
                WORKING_REG_A.x-=2;
                res1.w = GetShortValue(WORKING_REG_A.x);
                }
              else {
                WORKING_REG_A.x-=4;
                res1.x = GetIntValue(WORKING_REG_A.x);
                }
              break;
            case ADDRDISPADD:   // (D16,An)
              if(!(HIBYTE(Pipe1) & 0b00000001)) { // size
                res1.w = GetShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                }
              else {
                res1.x = GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                }
              _pc+=2;
              break;
            case ADDRIDXADD:   // (D8,An,Xn)
              if(!(HIBYTE(Pipe1) & 0b00000001)) { // size
                if(!DISPLACEMENT_SIZE)
                  res1.w = GetShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                else
                  res1.w = GetShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                }
              else {
                if(!DISPLACEMENT_SIZE)
                  res1.x = GetIntValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                else
                  res1.x = GetIntValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                }
              _pc+=2;
              break;
            }
          if(!(HIBYTE(Pipe1) & 0b00000001))  // size
            DEST_REG_A.x -= (signed short int)res1.w;
          else
            DEST_REG_A.x -= res1.x;
          // no flag qua
          }
        else {    // SUB SUBX
          if(Pipe1 & 0b0000000100000000) { // direction... da Dn a <ea>
            switch(ADDRESSING) {
              case ABSOLUTEADD:    // 
                switch(ABSOLUTEADD_SIZE) {
                  case ABSSSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res1.b.l = GetValue(Pipe2.w);
                        break;
                      case WORD_SIZE:
                        res1.w = GetShortValue(Pipe2.w);
                        break;
                      case DWORD_SIZE:
                        res1.x = GetIntValue(Pipe2.w);
                        break;
                      }
                    break;
                  case ABSLSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res1.b.l = GetValue(Pipe2.d);
                        break;
                      case WORD_SIZE:
                        res1.w = GetShortValue(Pipe2.d);
                        break;
                      case DWORD_SIZE:
                        res1.x = GetIntValue(Pipe2.d);
                        break;
                      }
                    break;
                  case PCIDXSUBADD:   // (D16,PC)
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res1.b.l = GetValue(_pc+(signed short int)Pipe2.w);
                        break;
                      case WORD_SIZE:
                        res1.w = GetShortValue(_pc+(signed short int)Pipe2.w);
                        break;
                      case DWORD_SIZE:
                        res1.x = GetIntValue(_pc+(signed short int)Pipe2.w);
                        break;
                      }
                    break;
                  case PCDISPSUBADD:   // (D8,Xn,PC)
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        if(!DISPLACEMENT_SIZE)
                          res1.b.l = GetValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                        else
                          res1.b.l = GetValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                        break;
                      case WORD_SIZE:
                        if(!DISPLACEMENT_SIZE)
                          res1.w = GetShortValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                        else
                          res1.w = GetShortValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                        break;
                      case DWORD_SIZE:
                        if(!DISPLACEMENT_SIZE)
                          res1.x = GetIntValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                        else
                          res1.x = GetIntValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                        break;
                      }
                    break;
                  case IMMSUBADD:
                    res1.x = Pipe2.d; 
                    break;
                  }
                break;
              case DATAREGADD:   // Dn NON DOVREBBE esserci qua
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = WORKING_REG_D.b.b0;
                    break;
                  case WORD_SIZE:
                    res1.w = WORKING_REG_D.w.l;
                    break;
                  case DWORD_SIZE:
                    res1.w = WORKING_REG_D.x;
                    break;
                  }
                break;
              case ADDRREGADD:   // An qua SUBX 
                if(LOBYTE(Pipe1) & 0b00001000) {
                  switch(OPERAND_SIZE) {
                    case BYTE_SIZE:
                      res1.b.l = GetValue(--WORKING_REG_A.x) + _f.CCR.Ext;
                      res1.b.l = GetValue(--DEST_REG_A.x);
                      res3.w = (WORD)res1.b.l - (WORD)res2.b.l;
                      PutValue(DEST_REG_A.x, res3.b.l);
                      break;
                    case WORD_SIZE:
                      WORKING_REG_A.x-=2; res2.w = GetShortValue(WORKING_REG_A.x) + _f.CCR.Ext;
                      DEST_REG_A.x-=2; res1.w = GetShortValue(DEST_REG_A.x);
                      res3.x = (DWORD)res1.w - (DWORD)res2.w;
                      PutShortValue(DEST_REG_A.x, res3.x);
                      break;
                    case DWORD_SIZE:
                      WORKING_REG_A.x-=4; res2.x = GetIntValue(WORKING_REG_A.x) + _f.CCR.Ext;
                      DEST_REG_A.x-=4; res1.x = GetIntValue(DEST_REG_A.x);
                      res3.x = res1.x - res2.x;
                      PutIntValue(DEST_REG_A.x, res3.x);
                      break;
                    }
                  }
                else {
                  switch(OPERAND_SIZE) {
                    case BYTE_SIZE:
                      res2.b.l = WORKING_REG_D.b.b0 + _f.CCR.Ext;
                      res1.b.l = DEST_REG_D.b.b0;
                      res3.w = (WORD)res1.b.l - (WORD)res2.b.l;
                      DEST_REG_D.b.b0=res3.b.l;
                      break;
                    case WORD_SIZE:
                      res2.w = WORKING_REG_D.w.l + _f.CCR.Ext;
                      res1.w = DEST_REG_D.w.l;
                      res3.x = (DWORD)res1.w - (DWORD)res2.w;
                      DEST_REG_D.w.l=res3.w;
                      break;
                    case DWORD_SIZE:
                      res2.x = WORKING_REG_D.x + _f.CCR.Ext;
                      res1.x = DEST_REG_D.x; 
                      res3.x = res1.x - res2.x;
                      DEST_REG_D.x=res3.x;
                      break;
                    }
                  }
                goto aggFlagS;
                break;
              case ADDRADD:   // (An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GetValue(WORKING_REG_A.x);
                    break;
                  case WORD_SIZE:
                    res1.w = GetShortValue(WORKING_REG_A.x);
                    break;
                  case DWORD_SIZE:
                    res1.x = GetIntValue(WORKING_REG_A.x);
                    break;
                  }
                break;
              case ADDRPINCADD:   // (An)+
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GetValue(WORKING_REG_A.x);
                    break;
                  case WORD_SIZE:
                    res1.w = GetShortValue(WORKING_REG_A.x);
                    break;
                  case DWORD_SIZE:
                    res1.x = GetIntValue(WORKING_REG_A.x);
                    break;
                  }
                break;
              case ADDRPDECADD:   // -(An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    WORKING_REG_A.x--;
                    res1.b.l = GetValue(WORKING_REG_A.x);
                    break;
                  case WORD_SIZE:
                    WORKING_REG_A.x-=2;
                    res1.w = GetShortValue(WORKING_REG_A.x);
                    break;
                  case DWORD_SIZE:
                    WORKING_REG_A.x-=4;
                    res1.x = GetIntValue(WORKING_REG_A.x);
                    break;
                  }
                break;
              case ADDRDISPADD:   // (D16,An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                    break;
                  case WORD_SIZE:
                    res1.w = GetShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                    break;
                  case DWORD_SIZE:
                    res1.x = GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                    break;
                  }
                break;
              case ADDRIDXADD:   // (D8,An,Xn)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res1.b.l = GetValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    else
                      res1.b.l = GetValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    break;
                  case WORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res1.w = GetShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    else
                      res1.w = GetShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    break;
                  case DWORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res1.x = GetIntValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    else
                      res1.x = GetIntValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    break;
                  }
                break;
              }
            }
          else {
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res1.b.l = DEST_REG_D.b.b0;
                break;
              case WORD_SIZE:
                res1.w = DEST_REG_D.w.l;
                break;
              case DWORD_SIZE:
                res1.x = DEST_REG_D.x;
                break;
              }
            }

          if(Pipe1 & 0b0000000100000000) { // direction
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res2.b.l = DEST_REG_D.b.b0;
                break;
              case WORD_SIZE:
                res2.w = DEST_REG_D.w.l;
                break;
              case DWORD_SIZE:
                res2.x = DEST_REG_D.x;
                break;
              }
            }
          else {
            switch(ADDRESSING) {
              case ABSOLUTEADD:    // #imm
                switch(ABSOLUTEADD_SIZE) {
                  case ABSSSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res2.b.l = GetValue(Pipe2.w);
                        break;
                      case WORD_SIZE:
                        res2.w = GetShortValue(Pipe2.w);
                        break;
                      case DWORD_SIZE:
                        res2.x = GetIntValue(Pipe2.w);
                        break;
                      }
                    break;
                  case ABSLSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res2.b.l = GetValue(Pipe2.d);
                        break;
                      case WORD_SIZE:
                        res2.w = GetShortValue(Pipe2.d);
                        break;
                      case DWORD_SIZE:
                        res2.x = GetIntValue(Pipe2.d);
                        break;
                      }
                    break;
                  case PCIDXSUBADD:   // (D16,PC)
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res2.b.l = GetValue(_pc+(signed short int)Pipe2.w);
                        break;
                      case WORD_SIZE:
                        res2.w = GetShortValue(_pc+(signed short int)Pipe2.w);
                        break;
                      case DWORD_SIZE:
                        res2.x = GetIntValue(_pc+(signed short int)Pipe2.w);
                        break;
                      }
                    break;
                  case PCDISPSUBADD:   // (D8,Xn,PC)
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        if(!DISPLACEMENT_SIZE)
                          res2.b.l = GetValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                        else
                          res2.b.l = GetValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                        break;
                      case WORD_SIZE:
                        if(!DISPLACEMENT_SIZE)
                          res2.w = GetShortValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                        else
                          res2.w = GetShortValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                        break;
                      case DWORD_SIZE:
                        if(!DISPLACEMENT_SIZE)
                          res2.x = GetIntValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                        else
                          res2.x = GetIntValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                        break;
                      }
                    break;
                  case IMMSUBADD:
                    res2.x = Pipe2.d; 
                    break;
                  }
                break;
              case DATAREGADD:   // Dn
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = WORKING_REG_D.b.b0;
                    break;
                  case WORD_SIZE:
                    res2.w = WORKING_REG_D.w.l;
                    break;
                  case DWORD_SIZE:
                    res2.x = WORKING_REG_D.x;
                    break;
                  }
                break;
              case ADDRREGADD:   // An
                switch(OPERAND_SIZE) {
                  case WORD_SIZE:
                    res2.w = WORKING_REG_A.w.l;
                    break;
                  case DWORD_SIZE:
                    res2.x = WORKING_REG_A.x;
                    break;
                  }
                break;
              case ADDRADD:   // (An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = GetValue(WORKING_REG_A.x);
                    break;
                  case WORD_SIZE:
                    res2.w = GetShortValue(WORKING_REG_A.x);
                    break;
                  case DWORD_SIZE:
                    res2.x = GetIntValue(WORKING_REG_A.x);
                    break;
                  }
                break;
              case ADDRPINCADD:   // (An)+
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = GetValue(WORKING_REG_A.x);
                    break;
                  case WORD_SIZE:
                    res2.w = GetShortValue(WORKING_REG_A.x);
                    break;
                  case DWORD_SIZE:
                    res2.x = GetIntValue(WORKING_REG_A.x);
                    break;
                  }
                break;
              case ADDRPDECADD:   // -(An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    WORKING_REG_A.x--;
                    res2.b.l = GetValue(WORKING_REG_A.x);
                    break;
                  case WORD_SIZE:
                    WORKING_REG_A.x-=2;
                    res2.w = GetShortValue(WORKING_REG_A.x);
                    break;
                  case DWORD_SIZE:
                    WORKING_REG_A.x-=4;
                    res2.x = GetIntValue(WORKING_REG_A.x);
                    break;
                  }
                break;
              case ADDRDISPADD:   // (D16,An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                    break;
                  case WORD_SIZE:
                    res2.w = GetShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                    break;
                  case DWORD_SIZE:
                    res2.x = GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                    break;
                  }
                break;
              case ADDRIDXADD:   // (D8,An,Xn)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res2.b.l = GetValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    else
                      res2.b.l = GetValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    break;
                  case WORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res2.w = GetShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    else
                      res2.w = GetShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    break;
                  case DWORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res2.x = GetIntValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    else
                      res2.x = GetIntValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    break;
                  }
                break;
              }
            }

          switch(OPERAND_SIZE) {
            case BYTE_SIZE:
              res3.w = (WORD)res1.b.l - (WORD)res2.b.l;
              break;
            case WORD_SIZE:
              res3.x = (DWORD)res1.w - (DWORD)res2.w;
              break;
            case DWORD_SIZE:
              res3.x = res1.x - res2.x;
              break;
            }

          if(Pipe1 & 0b0000000100000000) { // direction
            switch(ADDRESSING) {
              case ABSOLUTEADD:    // #imm
                switch(ABSOLUTEADD_SIZE) {
                  case ABSSSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        PutValue(Pipe2.w, res3.b.l);
                        break;
                      case WORD_SIZE:
                        PutShortValue(Pipe2.w, res3.w);
                        break;
                      case DWORD_SIZE:
                        PutIntValue(Pipe2.w, res3.x);
                        break;
                      }
                    _pc+=2;
                    break;
                  case ABSLSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        PutValue(Pipe2.d, res3.b.l);
                        break;
                      case WORD_SIZE:
                        PutShortValue(Pipe2.d, res3.w);
                        break;
                      case DWORD_SIZE:
                        PutIntValue(Pipe2.d, res3.x);
                        break;
                      }
                    _pc+=4;
                    break;
                  case PCIDXSUBADD:   // (D16,PC)
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        PutValue(_pc+(signed short int)Pipe2.w, res3.b.l);
                        break;
                      case WORD_SIZE:
                        PutShortValue(_pc+(signed short int)Pipe2.w, res3.w);
                        break;
                      case DWORD_SIZE:
                        PutIntValue(_pc+(signed short int)Pipe2.w, res3.x);
                        break;
                      }
                    _pc+=2;
                    break;
                  case PCDISPSUBADD:   // (D8,Xn,PC)
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        if(!DISPLACEMENT_SIZE)
                          PutValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.b.l);
                        else
                          PutValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.b.l);
                        break;
                      case WORD_SIZE:
                        if(!DISPLACEMENT_SIZE)
                          PutShortValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.w);
                        else
                          PutShortValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.w);
                        break;
                      case DWORD_SIZE:
                        if(!DISPLACEMENT_SIZE)
                          PutIntValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.x);
                        else
                          PutIntValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l, res3.x);
                        break;
                      }
                    _pc+=2;
                    break;
                  case IMMSUBADD:
//??                    res2.x = Pipe2.d; 
//                    _pc+=4;
                    break;
                  }
                break;
              case DATAREGADD:   // Dn qua NO!
                break;
              case ADDRREGADD:   // An CREDO DI NO idem
                switch(OPERAND_SIZE) {
                  case WORD_SIZE:
                    WORKING_REG_A.w.l = res3.w;
                    break;
                  case DWORD_SIZE:
                    WORKING_REG_A.x = res3.x;
                    break;
                  }
                break;
              case ADDRADD:   // (An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    PutValue(WORKING_REG_A.x,res3.b.l);
                    break;
                  case WORD_SIZE:
                    PutShortValue(WORKING_REG_A.x,res3.w);
                    break;
                  case DWORD_SIZE:
                    PutIntValue(WORKING_REG_A.x,res3.x);
                    break;
                  }
                break;
              case ADDRPINCADD:   // (An)+
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    PutValue(WORKING_REG_A.x,res3.b.l);
                    WORKING_REG_A.x++;
                    break;
                  case WORD_SIZE:
                    PutShortValue(WORKING_REG_A.x,res3.w);
                    WORKING_REG_A.x+=2;
                    break;
                  case DWORD_SIZE:
                    PutIntValue(WORKING_REG_A.x,res3.w);
                    WORKING_REG_A.x+=4;
                    break;
                  }
                break;
              case ADDRPDECADD:   // -(An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    PutValue(WORKING_REG_A.x,res3.b.l);
                    break;
                  case WORD_SIZE:
                    PutShortValue(WORKING_REG_A.x,res3.w);
                    break;
                  case DWORD_SIZE:
                    PutIntValue(WORKING_REG_A.x,res3.x);
                    break;
                  }
                break;
              case ADDRDISPADD:   // (D16,An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    PutValue(WORKING_REG_A.x + (signed short int)Pipe2.w,res3.b.l);
                    break;
                  case WORD_SIZE:
                    PutShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w,res3.w);
                    break;
                  case DWORD_SIZE:
                    PutIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w,res3.x);
                    break;
                  }
                _pc+=2;
                break;
              case ADDRIDXADD:   // (D8,An,Xn)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      PutValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res3.b.l);
                    else
                      PutValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res3.b.l);
                    break;
                  case WORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      PutShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res3.w);
                    else
                      PutShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res3.w);
                    break;
                  case DWORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      PutIntValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res3.x);
                    else
                      PutIntValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res3.x);
                    break;
                  }
                _pc+=2;
                break;
              }
            }
          else {
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                DEST_REG_D.b.b0 = res3.b.l;
                break;
              case WORD_SIZE:
                DEST_REG_D.w.l = res3.w;
                break;
              case DWORD_SIZE:
                DEST_REG_D.x = res3.x;
                break;
              }
            }
          goto aggFlagS;
          }
				break;
        
			case 0xb1:   // EOR CMPA CMPM
			case 0xb3:
			case 0xb5:
			case 0xb7:
			case 0xb9:
			case 0xbb:
			case 0xbd:
			case 0xbf:
        if(OPERAND_SIZE == SIZE_ELSE) {    // CMPA
          if(Pipe1 & 0b0000000100000000) {
            switch(ADDRESSING) {
              case ABSOLUTEADD:
                switch(ABSOLUTEADD_SIZE) {
                  case ABSSSUBADD:
                    res2.x = GetIntValue(Pipe2.w);
                    _pc+=2;
                    break;
                  case ABSLSUBADD:
                    res2.x = GetIntValue(Pipe2.d); 
                    _pc+=4;
                    break;
                  case PCIDXSUBADD:   // (D16,PC)
                    res2.x = GetIntValue(_pc+(signed short int)Pipe2.w);
                    _pc+=2;
                    break;
                  case PCDISPSUBADD:   // (D8,Xn,PC)
                    if(!DISPLACEMENT_SIZE)
                      res2.x = GetIntValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    else
                      res2.x = GetIntValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    _pc+=2;
                    break;
                  case IMMSUBADD:
                    res2.x = Pipe2.d; 
                    _pc+=4;
                    break;
                  }
                break;
              case DATAREGADD:   // Dn
                res2.x = WORKING_REG_D.x;
                break;
              case ADDRREGADD:   // An
                res2.x = WORKING_REG_A.x;
                break;
              case ADDRADD:   // (An)
                res2.x = GetIntValue(WORKING_REG_A.x);
                break;
              case ADDRPINCADD:   // (An)+
                res2.x = GetIntValue(WORKING_REG_A.x);
                WORKING_REG_A.x+=4;
                break;
              case ADDRPDECADD:   // -(An)
                WORKING_REG_A.x-=4;
                res2.x = GetIntValue(WORKING_REG_A.x);
                break;
              case ADDRDISPADD:   // (D16,An)
                res2.x = GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                _pc+=2;
                break;
              case ADDRIDXADD:   // (D8,An,Xn)
                if(!DISPLACEMENT_SIZE) {
                  res2.x = GetIntValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  }
                else {
                  res2.x = GetIntValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  }
                _pc+=4;
                break;
              }
            res1.x=DEST_REG_A.x;
            res3.x = res1.x - res2.x;
            }
          else {
            switch(ADDRESSING) {
              case ABSOLUTEADD:
                switch(ABSOLUTEADD_SIZE) {
                  case ABSSSUBADD:
                    res2.w = GetShortValue(Pipe2.w);
                    _pc+=2;
                    break;
                  case ABSLSUBADD:
                    res2.w = GetShortValue(Pipe2.d); 
                    _pc+=4;
                    break;
                  case PCIDXSUBADD:   // (D16,PC)
                    res2.w = GetShortValue(_pc+(signed short int)Pipe2.w);
                    _pc+=2;
                    break;
                  case PCDISPSUBADD:   // (D8,Xn,PC)
                    if(!DISPLACEMENT_SIZE)
                      res2.w = GetShortValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    else
                      res2.w = GetShortValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    _pc+=2;
                    break;
                  case IMMSUBADD:
                    res2.w = Pipe2.w; 
                    _pc+=2;
                    break;
                  }
                break;
              case DATAREGADD:   // Dn
                res2.w = WORKING_REG_D.w.l;
                break;
              case ADDRREGADD:   // An
                res2.w = WORKING_REG_A.w.l;
                break;
              case ADDRADD:   // (An)
                res2.w = GetShortValue(WORKING_REG_A.w.l);
                break;
              case ADDRPINCADD:   // (An)+
                res2.w = GetShortValue(WORKING_REG_A.x);
                WORKING_REG_A.x+=2;
                break;
              case ADDRPDECADD:   // -(An)
                WORKING_REG_A.x-=2;
                res2.w = GetShortValue(WORKING_REG_A.x);
                break;
              case ADDRDISPADD:   // (D16,An)
                res2.w = GetShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                _pc+=2;
                break;
              case ADDRIDXADD:   // (D8,An,Xn)
                if(!DISPLACEMENT_SIZE) {
                  res2.w = GetShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  }
                else {
                  res2.w = GetShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  }
                _pc+=2;
                break;
              }
            res1.x=DEST_REG_A.x;
            res3.x = res1.x - (signed short int)res2.w;
            }
          goto aggFlagC4;
          }
        else {    // EOR CMPM
          switch(ADDRESSING) {
            case ABSOLUTEADD:
              switch(ABSOLUTEADD_SIZE) {
                case ABSSSUBADD:
                  switch(OPERAND_SIZE) {
                    case BYTE_SIZE:
                      res3.b.l = DEST_REG_D.b.b0 ^ GetValue(Pipe2.w);
                      PutValue(Pipe2.w,res3.b.l);
                      break;
                    case WORD_SIZE:
                      res3.w = DEST_REG_D.w.l ^ GetShortValue(Pipe2.w);
                      PutShortValue(Pipe2.w,res3.w);
                      break;
                    case DWORD_SIZE:
                      res3.x = DEST_REG_D.x ^ GetIntValue(Pipe2.w);
                      PutIntValue(Pipe2.w,res3.x);
                      break;
                    }
                  _pc+=2;
                  break;
                case ABSLSUBADD:
                  switch(OPERAND_SIZE) {
                    case BYTE_SIZE:
                      res3.b.l = DEST_REG_D.b.b0 ^ GetValue(Pipe2.d);
                      PutValue(Pipe2.d,res3.b.l);
                      break;
                    case WORD_SIZE:
                      res3.w = DEST_REG_D.w.l ^ GetShortValue(Pipe2.d);
                      PutShortValue(Pipe2.d,res3.w);
                      break;
                    case DWORD_SIZE:
                      res3.x = DEST_REG_D.x ^ GetIntValue(Pipe2.d);
                      PutIntValue(Pipe2.d,res3.x);
                      break;
                    }
                  _pc+=4;
                  break;
                case PCIDXSUBADD:   // (D16,PC) qua NO!
                case PCDISPSUBADD:   // (D8,Xn,PC)
                case IMMSUBADD:
                  break;
                }
              break;
            case DATAREGADD:   // Dn
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  res3.b.l = DEST_REG_D.b.b0 ^= WORKING_REG_D.b.b0;
                  break;
                case WORD_SIZE:
                  res3.w = DEST_REG_D.w.l ^= WORKING_REG_D.w.l;
                  break;
                case DWORD_SIZE:
                  res3.x = DEST_REG_D.x ^= WORKING_REG_D.x;
                  break;
                }
              break;
            case ADDRREGADD:   // An; CMPM qua
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  res1.b.l = GetValue(WORKING_REG_A.x);
                  res2.b.l = GetValue(DEST_REG_A.x);
                  res3.w = (WORD)res1.b.l - (WORD)res2.b.l;
                  DEST_REG_A.x++; WORKING_REG_A.x++;
                  break;
                case WORD_SIZE:
                  res1.w = GetShortValue(WORKING_REG_A.x);
                  res2.w = GetShortValue(DEST_REG_A.x);
                  res3.x = (DWORD)res1.w - (DWORD)res2.w;
                  DEST_REG_A.x+=2; WORKING_REG_A.x+=2;
                  break;
                case DWORD_SIZE:
                  res1.x = GetIntValue(WORKING_REG_A.x);
                  res2.x = GetIntValue(DEST_REG_A.x);
                  res3.x = res1.x - res2.x;
                  DEST_REG_A.x+=4; WORKING_REG_A.x+=4;
                  break;
                }
              goto aggFlagC;
              break;
            case ADDRADD:   // (An)
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  res3.b.l = GetValue(WORKING_REG_A.x) ^ WORKING_REG_D.b.b0;
                  PutValue(WORKING_REG_A.x,res3.b.l);
                  break;
                case WORD_SIZE:
                  res3.w = GetShortValue(WORKING_REG_A.x) ^ WORKING_REG_D.w.l;
                  PutShortValue(WORKING_REG_A.x,res3.w);
                  break;
                case DWORD_SIZE:
                  res3.x = GetIntValue(WORKING_REG_A.x) ^ WORKING_REG_D.x;
                  PutIntValue(WORKING_REG_A.x,res3.x);
                  break;
                }
              break;
            case ADDRPINCADD:   // (An)+
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  res3.b.l = GetValue(WORKING_REG_A.x) ^ WORKING_REG_D.b.b0;
                  PutValue(WORKING_REG_A.x,res3.b.l);
                  WORKING_REG_A.x++;
                  break;
                case WORD_SIZE:
                  res3.w = GetShortValue(WORKING_REG_A.x) ^ WORKING_REG_D.w.l;
                  PutShortValue(WORKING_REG_A.x,res3.w);
                  WORKING_REG_A.x+=2;
                  break;
                case DWORD_SIZE:
                  res3.x = GetIntValue(WORKING_REG_A.x) ^ WORKING_REG_D.x;
                  PutIntValue(WORKING_REG_A.x,res3.x);
                  WORKING_REG_A.x+=4;
                  break;
                }
              break;
            case ADDRPDECADD:   // -(An)
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  WORKING_REG_A.x--;
                  res3.b.l = GetValue(WORKING_REG_A.x) ^ WORKING_REG_D.b.b0;
                  PutValue(WORKING_REG_A.x,res3.b.l);
                  break;
                case WORD_SIZE:
                  WORKING_REG_A.x-=2;
                  res3.w = GetShortValue(WORKING_REG_A.x) ^ WORKING_REG_D.w.l;
                  PutShortValue(WORKING_REG_A.x,res3.w);
                  break;
                case DWORD_SIZE:
                  WORKING_REG_A.x-=4;
                  res3.x = GetIntValue(WORKING_REG_A.x) ^ WORKING_REG_D.x;
                  PutIntValue(WORKING_REG_A.x,res3.x);
                  break;
                }
              break;
            case ADDRDISPADD:   // (D16,An)
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  res3.b.l = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.w) ^ WORKING_REG_D.b.b0;
                  PutValue(WORKING_REG_A.x + (signed short int)Pipe2.w,res3.b.l);
                  break;
                case WORD_SIZE:
                  res3.w = GetShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w) ^ WORKING_REG_D.w.l;
                  PutShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w,res3.w);
                  break;
                case DWORD_SIZE:
                  res3.x = GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w) ^ WORKING_REG_D.x;
                  PutIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w,res3.x);
                  break;
                }
              _pc+=2;
              break;
            case ADDRIDXADD:   // (D8,An,Xn)
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  if(!DISPLACEMENT_SIZE) {
                    res3.b.l = GetValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l) ^ WORKING_REG_D.b.b0;
                    PutValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res3.b.l);
                    }
                  else {
                    res3.b.l = GetValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l) ^ WORKING_REG_D.b.b0;
                    PutValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res3.b.l);
                    }
                  break;
                case WORD_SIZE:
                  if(!DISPLACEMENT_SIZE) {
                    res3.w = GetShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l) ^ WORKING_REG_D.w.l;
                    PutShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res3.w);
                    }
                  else {
                    res3.w = GetShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l) ^ WORKING_REG_D.w.l;
                    PutShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res3.w);
                    }
                  break;
                case DWORD_SIZE:
                  if(!DISPLACEMENT_SIZE) {
                    res3.x = GetIntValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l) ^ WORKING_REG_D.x;
                    PutIntValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res3.x);
                    }
                  else {
                    res3.x = GetIntValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l) ^ WORKING_REG_D.x;
                    PutIntValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res3.x);
                    }
                  break;
                }
              _pc+=2;
              break;
            }
          goto aggFlag;
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
          case ABSOLUTEADD:
            switch(ABSOLUTEADD_SIZE) {
              case ABSSSUBADD:
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = GetValue(Pipe2.w);
                    break;
                  case WORD_SIZE:
                    res2.w = GetShortValue(Pipe2.w);
                    break;
                  case DWORD_SIZE:
                    res2.x = GetIntValue(Pipe2.w);
                    break;
                  }
                _pc+=2;
                break;
              case ABSLSUBADD:
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = GetValue(Pipe2.d);
                    break;
                  case WORD_SIZE:
                    res2.w = GetShortValue(Pipe2.d);
                    break;
                  case DWORD_SIZE:
                    res2.x = GetIntValue(Pipe2.d);
                    break;
                  }
                _pc+=4;
                break;
              case PCIDXSUBADD:   // (D16,PC)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = GetValue(_pc+(signed short int)Pipe2.w);
                    break;
                  case WORD_SIZE:
                    res2.w = GetShortValue(_pc+(signed short int)Pipe2.w);
                    break;
                  case DWORD_SIZE:
                    res2.x = GetIntValue(_pc+(signed short int)Pipe2.w);
                    break;
                  }
                _pc+=2;
                break;
              case PCDISPSUBADD:   // (D8,Xn,PC)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res2.b.l = GetValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    else
                      res2.b.l = GetValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    break;
                  case WORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res2.w = GetShortValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    else
                      res2.w = GetShortValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    break;
                  case DWORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res2.x = GetIntValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    else
                      res2.x = GetIntValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    break;
                  }
                _pc+=2;
                break;
              case IMMSUBADD:
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = Pipe2.b.l;
                    _pc+=2;
                    break;
                  case WORD_SIZE:
                    res2.w = Pipe2.w;
                    _pc+=2;
                    break;
                  case DWORD_SIZE:
                    res2.x = Pipe2.d; 
                    _pc+=4;
                    break;
                  }
                break;
              }
            break;
          case DATAREGADD:   // Dn
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res2.b.l = WORKING_REG_D.b.b0;
                break;
              case WORD_SIZE:
                res2.w = WORKING_REG_D.w.l;
                break;
              case DWORD_SIZE:
                res2.x = WORKING_REG_D.x;
                break;
              case SIZE_ELSE:
                break;
              }
            break;
          case ADDRREGADD:   // An
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res2.b.l = WORKING_REG_A.b.b0;
                break;
              case WORD_SIZE:
                res2.w = WORKING_REG_A.w.l;
                break;
              case DWORD_SIZE:
                res2.x = WORKING_REG_A.x;
                break;
              case SIZE_ELSE:
                break;
              }
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res2.b.l = GetValue(WORKING_REG_A.x);
                break;
              case WORD_SIZE:
                res2.w = GetShortValue(WORKING_REG_A.x);
                break;
              case DWORD_SIZE:
                res2.x = GetIntValue(WORKING_REG_A.x);
                break;
              case SIZE_ELSE:
                break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res2.b.l = GetValue(WORKING_REG_A.x);
                WORKING_REG_A.x++;
                break;
              case WORD_SIZE:
                res2.w = GetShortValue(WORKING_REG_A.x);
                WORKING_REG_A.x+=2;
                break;
              case DWORD_SIZE:
                res2.x = GetIntValue(WORKING_REG_A.x);
                WORKING_REG_A.x+=4;
                break;
              case SIZE_ELSE:
                break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                WORKING_REG_A.x--;
                res2.b.l = GetValue(WORKING_REG_A.x);
                break;
              case WORD_SIZE:
                WORKING_REG_A.x-=2;
                res2.w = GetShortValue(WORKING_REG_A.x);
                break;
              case DWORD_SIZE:
                WORKING_REG_A.x-=4;
                res2.x = GetIntValue(WORKING_REG_A.x);
                break;
              case SIZE_ELSE:
                break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res2.b.l = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                _pc+=2;
                break;
              case WORD_SIZE:
                res2.w = GetShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                _pc+=2;
                break;
              case DWORD_SIZE:
                res2.x = GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                _pc+=2;
                break;
              case SIZE_ELSE:
                break;
              }
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res2.b.l = GetValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  }
                else {
                  res2.b.l = GetValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  }
                _pc+=2;
                break;
              case WORD_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res2.w = GetShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  }
                else {
                  res2.w = GetShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  }
                _pc+=2;
                break;
              case DWORD_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res2.x = GetIntValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  }
                else {
                  res2.x = GetIntValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  }
                _pc+=2;
                break;
              case SIZE_ELSE:
                break;
              }
            break;
          }
        switch(OPERAND_SIZE) {
          case BYTE_SIZE:
            res1.b.l = DEST_REG_D.b.b0;
            res3.w = (WORD)res1.b.l-(WORD)res2.b.l;
            break;
          case WORD_SIZE:
            res1.w = DEST_REG_D.w.l;
            res3.x = (DWORD)res1.w-(DWORD)res2.w;
            break;
          case DWORD_SIZE:
            res1.x = DEST_REG_D.x;
            res3.x = res1.x-res2.x;
            break;
          }
        goto aggFlagC;
				break;
        
			case 0xc0:   // MULU AND
			case 0xc2:
			case 0xc4:
			case 0xc6:
			case 0xc8:
			case 0xca:
			case 0xcc:
			case 0xce:
			case 0xc1:   // MULS ABCD EXG AND
			case 0xc3:
			case 0xc5:
			case 0xc7:
			case 0xc9:
			case 0xcb:
			case 0xcd:
			case 0xcf:
        if(OPERAND_SIZE == SIZE_ELSE) {    // MULU MULS
// "forse" in 68020 ecc c'è anche una versione 32x32bit... (per cui qua non c'è OVF ma là forse sì..
          res1.w=DEST_REG_D.w.l;
          switch(ADDRESSING) {
            case ABSOLUTEADD:
              switch(ABSOLUTEADD_SIZE) {
                case ABSSSUBADD:
                  res2.w = GetShortValue(Pipe2.w);
                  _pc+=2;
                  break;
                case ABSLSUBADD:
                  res2.w = GetShortValue(Pipe2.d); 
                  _pc+=4;
                  break;
                case PCIDXSUBADD:   // (D16,PC)
                  res2.w = GetShortValue(_pc+(signed short int)Pipe2.w);
                  _pc+=2;
                  break;
                case PCDISPSUBADD:   // (D8,Xn,PC)
                  if(!DISPLACEMENT_SIZE)
                    res2.w = GetShortValue(_pc+(((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  else
                    res2.w = GetShortValue(_pc+(((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  _pc+=2;
                  break;
                case IMMSUBADD:
                  res2.w = Pipe2.w; 
                  _pc+=2;
                  break;
                }
              break;
            case DATAREGADD:   // Dn
              res2.w = WORKING_REG_D.w.l;
              break;
            case ADDRREGADD:   // An qua NO!
              break;
            case ADDRADD:   // (An)
              res2.w = GetShortValue(WORKING_REG_A.x);
              break;
            case ADDRPINCADD:   // (An)+
              res2.w = GetShortValue(WORKING_REG_A.x);
              WORKING_REG_A.x+=2;
              break;
            case ADDRPDECADD:   // -(An)
              WORKING_REG_A.x-=2;
              res2.w = GetShortValue(WORKING_REG_A.x);
              break;
            case ADDRDISPADD:   // (D16,An)
              res2.w = GetShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
              _pc+=2;
              break;
            case ADDRIDXADD:   // (D8,An,Xn)
              if(!DISPLACEMENT_SIZE) {
                res2.w = GetShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                }
              else {
                res2.w = GetShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                }
              _pc+=2;
              break;
            }
          if(HIBYTE(Pipe1) & 0b00000001)      // MULS
            res3.x = (signed int)res1.w * (signed int)res2.w;
          else      // MULU
            res3.x = res1.w * res2.w;
          DEST_REG_D.x = res3.x;
          _f.CCR.Zero=!res3.x;
          _f.CCR.Sign=!!(res3.x & 0x80000000);
          _f.CCR.Ovf=0;     // ?? appunto, se long*long
          _f.CCR.Carry=0;
          }
        else {  // AND ABCD EXG
          if(Pipe1 & 0b0000000100000000) { // direction... 
            switch(ADDRESSING) {
              case ABSOLUTEADD:    // 
                switch(ABSOLUTEADD_SIZE) {
                  case ABSSSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res1.b.l = GetValue(Pipe2.w);
                        break;
                      case WORD_SIZE:
                        res1.w = GetShortValue(Pipe2.w);
                        break;
                      case DWORD_SIZE:
                        res1.x = GetIntValue(Pipe2.w);
                        break;
                      }
                    break;
                  case ABSLSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res1.b.l = GetValue(Pipe2.d);
                        break;
                      case WORD_SIZE:
                        res1.w = GetShortValue(Pipe2.d);
                        break;
                      case DWORD_SIZE:
                        res1.x = GetIntValue(Pipe2.d);
                        break;
                      }
                    break;
                  case PCIDXSUBADD:   // (D16,PC)
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res1.b.l = GetValue(_pc+(signed short int)Pipe2.w);
                        break;
                      case WORD_SIZE:
                        res1.w = GetShortValue(_pc+(signed short int)Pipe2.w);
                        break;
                      case DWORD_SIZE:
                        res1.x = GetIntValue(_pc+(signed short int)Pipe2.w);
                        break;
                      }
                    break;
                  case PCDISPSUBADD:   // (D8,Xn,PC)
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        if(!DISPLACEMENT_SIZE)
                          res1.b.l = GetValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                        else
                          res1.b.l = GetValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                        break;
                      case WORD_SIZE:
                        if(!DISPLACEMENT_SIZE)
                          res1.w = GetShortValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                        else
                          res1.w = GetShortValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                        break;
                      case DWORD_SIZE:
                        if(!DISPLACEMENT_SIZE)
                          res1.x = GetIntValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                        else
                          res1.x = GetIntValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                        break;
                      }
                    break;
                  case IMMSUBADD:
                    res1.x = Pipe2.d; 
                    break;
                  }
                break;
              case DATAREGADD:   // Dn
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = WORKING_REG_D.b.b0;
                    break;
                  case WORD_SIZE:
                    res1.w = WORKING_REG_D.w.l;
                    break;
                  case DWORD_SIZE:
                    res1.w = WORKING_REG_D.x;
                    break;
                  }
                break;
              case ADDRREGADD:   // An qua ABCD EXG
                switch(OPERAND_SIZE) {
                  case 0:     // ABCD v. anche SBCD DOVREBBE ANDARE ANCHE su DATAREG...
                    res1.b.l = GetValue(--WORKING_REG_A.x);
                    res2.b.l = GetValue(--DEST_REG_A.x);
                    res3.w = (res1.b.l & 0xf) + (res2.b.l & 0xf) + _f.CCR.Ext;
                    res3.w = (((res1.b.l & 0xf0) >> 4) + ((res2.b.l & 0xf0) >> 4) + (res3.b.h ? 1 : 0)) | 
                              res3.b.l;
                    PutValue(DEST_REG_A.x, res3.b.l);
                    _f.CCR.Zero=!res3.b.l;
                    _f.CCR.Ext=_f.CCR.Carry=!!res3.b.h;
                    goto noAggFlag;
                    break;
                  case 0x40:     //  EXG
										if(!(Pipe1 & 0b0000000000001000)) {
											res3.w=WORKING_REG_D.x;
											WORKING_REG_D.x=DEST_REG_D.x;
											DEST_REG_D.x=res3.w;
											}
										else {
											res3.w=WORKING_REG_A.x;
											WORKING_REG_A.x=DEST_REG_A.x;
											DEST_REG_A.x=res3.w;
											}
                    goto noAggFlag;
                    break;
                  case 0x80:     //  EXG
										if(!(Pipe1 & 0b0000000000001000))			// DEVE Essere così!
											;
                    res3.w=WORKING_REG_A.x;
                    WORKING_REG_A.x=DEST_REG_D.x;
                    DEST_REG_D.x=res3.w;
                    goto noAggFlag;
                    break;
                  }
                break;
              case ADDRADD:   // (An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GetValue(WORKING_REG_A.x);
                    break;
                  case WORD_SIZE:
                    res1.w = GetShortValue(WORKING_REG_A.x);
                    break;
                  case DWORD_SIZE:
                    res1.x = GetIntValue(WORKING_REG_A.x);
                    break;
                  }
                break;
              case ADDRPINCADD:   // (An)+
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GetValue(WORKING_REG_A.x);
                    break;
                  case WORD_SIZE:
                    res1.w = GetShortValue(WORKING_REG_A.x);
                    break;
                  case DWORD_SIZE:
                    res1.x = GetIntValue(WORKING_REG_A.x);
                    break;
                  }
                break;
              case ADDRPDECADD:   // -(An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    WORKING_REG_A.x--;
                    res1.b.l = GetValue(WORKING_REG_A.x);
                    break;
                  case WORD_SIZE:
                    WORKING_REG_A.x-=2;
                    res1.w = GetShortValue(WORKING_REG_A.x);
                    break;
                  case DWORD_SIZE:
                    WORKING_REG_A.x-=4;
                    res1.x = GetIntValue(WORKING_REG_A.x);
                    break;
                  }
                break;
              case ADDRDISPADD:   // (D16,An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                    break;
                  case WORD_SIZE:
                    res1.w = GetShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                    break;
                  case DWORD_SIZE:
                    res1.x = GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                    break;
                  }

                break;
              case ADDRIDXADD:   // (D8,An,Xn)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res1.b.l = GetValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    else
                      res1.b.l = GetValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    break;
                  case WORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res1.w = GetShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    else
                      res1.w = GetShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    break;
                  case DWORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res1.x = GetIntValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    else
                      res1.x = GetIntValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    break;
                  }

                break;
              }
            }
          else {
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res1.b.l = DEST_REG_D.b.b0;
                break;
              case WORD_SIZE:
                res1.w = DEST_REG_D.w.l;
                break;
              case DWORD_SIZE:
                res1.x = DEST_REG_D.x;
                break;
              }
            }

          if(Pipe1 & 0b0000000100000000) { // direction
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res2.b.l = DEST_REG_D.b.b0;
                break;
              case WORD_SIZE:
                res2.w = DEST_REG_D.w.l;
                break;
              case DWORD_SIZE:
                res2.x = DEST_REG_D.x;
                break;
              }
            }
          else {
            switch(ADDRESSING) {
              case ABSOLUTEADD:    // #imm
                switch(ABSOLUTEADD_SIZE) {
                  case ABSSSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res2.b.l = GetValue(Pipe2.w);
                        break;
                      case WORD_SIZE:
                        res2.w = GetShortValue(Pipe2.w);
                        break;
                      case DWORD_SIZE:
                        res2.x = GetIntValue(Pipe2.w);
                        break;
                      }
                    break;
                  case ABSLSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res2.b.l = GetValue(Pipe2.d);
                        break;
                      case WORD_SIZE:
                        res2.w = GetShortValue(Pipe2.d);
                        break;
                      case DWORD_SIZE:
                        res2.x = GetIntValue(Pipe2.d);
                        break;
                      }
                    break;
                  case PCIDXSUBADD:   // (D16,PC) qua NO!
                  case PCDISPSUBADD:   // (D8,Xn,PC)
                  case IMMSUBADD:
                    break;
                  }
                break;
              case DATAREGADD:   // Dn .. in un punto il Doc dice questo no qua...
                #warning MA VA IN CONFLITTO CON ABCD! v. anche sopra
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = DEST_REG_D.b.b0;
                    break;
                  case WORD_SIZE:
                    res2.w = DEST_REG_D.w.l;
                    break;
                  case DWORD_SIZE:
                    res2.x = DEST_REG_D.x;
                    break;
                  }
                break;
              case ADDRREGADD:   // An
                break;
              case ADDRADD:   // (An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = GetValue(WORKING_REG_A.x);
                    break;
                  case WORD_SIZE:
                    res2.w = GetShortValue(WORKING_REG_A.x);
                    break;
                  case DWORD_SIZE:
                    res2.x = GetIntValue(WORKING_REG_A.x);
                    break;
                  }
                break;
              case ADDRPINCADD:   // (An)+
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = GetValue(WORKING_REG_A.x);
                    break;
                  case WORD_SIZE:
                    res2.w = GetShortValue(WORKING_REG_A.x);
                    break;
                  case DWORD_SIZE:
                    res2.x = GetIntValue(WORKING_REG_A.x);
                    break;
                  }
                break;
              case ADDRPDECADD:   // -(An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    WORKING_REG_A.x--;
                    res2.b.l = GetValue(WORKING_REG_A.x);
                    break;
                  case WORD_SIZE:
                    WORKING_REG_A.x-=2;
                    res2.w = GetShortValue(WORKING_REG_A.x);
                    break;
                  case DWORD_SIZE:
                    WORKING_REG_A.x-=4;
                    res2.x = GetIntValue(WORKING_REG_A.x);
                    break;
                  }
                break;
              case ADDRDISPADD:   // (D16,An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                    break;
                  case WORD_SIZE:
                    res2.w = GetShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                    break;
                  case DWORD_SIZE:
                    res2.x = GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                    break;
                  }
                break;
              case ADDRIDXADD:   // (D8,An,Xn)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res2.b.l = GetValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    else
                      res2.b.l = GetValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    break;
                  case WORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res2.w = GetShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    else
                      res2.w = GetShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    break;
                  case DWORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res2.x = GetIntValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    else
                      res2.x = GetIntValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    break;
                  }
                break;
              }
            }

          switch(OPERAND_SIZE) {
            case BYTE_SIZE:
              res3.b.l = res1.b.l & res2.b.l;
              break;
            case WORD_SIZE:
              res3.w = res1.w & res2.w;
              break;
            case DWORD_SIZE:
              res3.x = res1.x & res2.x;
              break;
            }

          if(Pipe1 & 0b0000000100000000) { // direction
            switch(ADDRESSING) {
              case ABSOLUTEADD:    // #imm
                switch(ABSOLUTEADD_SIZE) {
                  case ABSSSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        PutValue(Pipe2.w, res3.b.l);
                        break;
                      case WORD_SIZE:
                        PutShortValue(Pipe2.w, res3.w);
                        break;
                      case DWORD_SIZE:
                        PutIntValue(Pipe2.w, res3.x);
                        break;
                      }
                    _pc+=2;
                    break;
                  case ABSLSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        PutValue(Pipe2.d, res3.b.l);
                        break;
                      case WORD_SIZE:
                        PutShortValue(Pipe2.d, res3.w);
                        break;
                      case DWORD_SIZE:
                        PutIntValue(Pipe2.d, res3.x);
                        break;
                      }
                    _pc+=4;
                    break;
                  case PCIDXSUBADD:   // (D16,PC) qua NO!
                  case PCDISPSUBADD:   // (D8,Xn,PC)
                  case IMMSUBADD:
                    break;
                  }
                break;
              case DATAREGADD:   // Dn .. in un punto il Doc dice questo no qua...
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    DEST_REG_D.b.b0 = res3.b.l;
                    break;
                  case WORD_SIZE:
                    DEST_REG_D.w.l = res3.w;
                    break;
                  case DWORD_SIZE:
                    DEST_REG_D.x = res3.x;
                    break;
                  }
                break;
              case ADDRREGADD:   // An qua NO!
                break;
              case ADDRADD:   // (An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    PutValue(WORKING_REG_A.x,res3.b.l);
                    break;
                  case WORD_SIZE:
                    PutShortValue(WORKING_REG_A.x,res3.w);
                    break;
                  case DWORD_SIZE:
                    PutIntValue(WORKING_REG_A.x,res3.x);
                    break;
                  }
                break;
              case ADDRPINCADD:   // (An)+
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    PutValue(WORKING_REG_A.x,res3.b.l);
                    WORKING_REG_A.x++;
                    break;
                  case WORD_SIZE:
                    PutShortValue(WORKING_REG_A.x,res3.w);
                    WORKING_REG_A.x+=2;
                    break;
                  case DWORD_SIZE:
                    PutIntValue(WORKING_REG_A.x,res3.x);
                    WORKING_REG_A.x+=4;
                    break;
                  }
                break;
              case ADDRPDECADD:   // -(An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    PutValue(WORKING_REG_A.x,res3.b.l);
                    break;
                  case WORD_SIZE:
                    PutShortValue(WORKING_REG_A.x,res3.w);
                    break;
                  case DWORD_SIZE:
                    PutIntValue(WORKING_REG_A.x,res3.x);
                    break;
                  }
                break;
              case ADDRDISPADD:   // (D16,An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    PutValue(WORKING_REG_A.x + (signed short int)Pipe2.w,res3.b.l);
                    break;
                  case WORD_SIZE:
                    PutShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w,res3.w);
                    break;
                  case DWORD_SIZE:
                    PutIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w,res3.x);
                    break;
                  }
                _pc+=2;
                break;
              case ADDRIDXADD:   // (D8,An,Xn)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      PutValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res3.b.l);
                    else
                      PutValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res3.b.l);
                    break;
                  case WORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      PutShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res3.w);
                    else
                      PutShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res3.w);
                    break;
                  case DWORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      PutIntValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res3.x);
                    else
                      PutIntValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res3.x);
                    break;
                  }
                _pc+=2;
                break;
              }
            }
          else {
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                DEST_REG_D.b.b0 = res3.b.l;
                break;
              case WORD_SIZE:
                DEST_REG_D.w.l = res3.w;
                break;
              case DWORD_SIZE:
                DEST_REG_D.x = res3.x;
                break;
              }
            }
          goto aggFlag;
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
        if(OPERAND_SIZE == SIZE_ELSE) { // ADDA
          switch(ADDRESSING) {
            case ABSOLUTEADD:    // 
              switch(ABSOLUTEADD_SIZE) {
                case ABSSSUBADD:
                  if(!(HIBYTE(Pipe1) & 0b00000001)) { // size
                    res1.w = Pipe2.w;
                    _pc+=2;
                    }
                  else {
                    res1.x = Pipe2.d;
                    _pc+=4;
                    }
                  break;
                case ABSLSUBADD:
                  if(!(HIBYTE(Pipe1) & 0b00000001)) { // 
                    res1.w = Pipe2.w;
                    _pc+=2;
                    }
                  else {
                    res1.x = Pipe2.d;
                    _pc+=4;
                    }
                  break;
                case PCIDXSUBADD:   // (D16,PC)
                  if(!(HIBYTE(Pipe1) & 0b00000001)) { // 
                    res1.w = GetShortValue(_pc+(signed short int)Pipe2.w);
                    }
                  else {
                    res1.x = GetIntValue(_pc+(signed short int)Pipe2.w);
                    }
                  _pc+=2;
                  break;
                case PCDISPSUBADD:   // (D8,Xn,PC)
                  if(!(HIBYTE(Pipe1) & 0b00000001)) { // 
                    if(!DISPLACEMENT_SIZE)
                      res1.w = GetShortValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    else
                      res1.w = GetShortValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    }
                  else {
                    if(!DISPLACEMENT_SIZE)
                      res1.x = GetIntValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    else
                      res1.x = GetIntValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    }
                  _pc+=2;
                  break;
                case IMMSUBADD:
                  res1.x = Pipe2.d; // boh
                  _pc+=4;
                  break;
                }
              break;
            case DATAREGADD:   // Dn
              if(!(HIBYTE(Pipe1) & 0b00000001))  // size
                res1.w = WORKING_REG_D.w.l;
              else
                res1.x = WORKING_REG_D.x;
              break;
            case ADDRREGADD:   // An
              if(!(HIBYTE(Pipe1) & 0b00000001))  // size
                res1.w = WORKING_REG_A.w.l;
              else
                res1.x = WORKING_REG_A.x;
              break;
            case ADDRADD:   // (An)
              if(!(HIBYTE(Pipe1) & 0b00000001))  // 
                res1.w = GetShortValue(WORKING_REG_A.x);
              else
                res1.x = GetIntValue(WORKING_REG_A.x);
              break;
            case ADDRPINCADD:   // (An)+
              if(!(HIBYTE(Pipe1) & 0b00000001)) { // size
                res1.w = GetShortValue(WORKING_REG_A.x);
                WORKING_REG_A.x+=2;
                }
              else {
                res1.x = GetIntValue(WORKING_REG_A.x);
                WORKING_REG_A.x+=4;
                }
              break;
            case ADDRPDECADD:   // -(An)
              if(!(HIBYTE(Pipe1) & 0b00000001)) { // size
                WORKING_REG_A.x-=2;
                res1.w = GetShortValue(WORKING_REG_A.x);
                }
              else {
                WORKING_REG_A.x-=4;
                res1.x = GetIntValue(WORKING_REG_A.x);
                }
              break;
            case ADDRDISPADD:   // (D16,An)
              if(!(HIBYTE(Pipe1) & 0b00000001)) { // size
                res1.w = GetShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                }
              else {
                res1.x = GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                }
              _pc+=2;
              break;
            case ADDRIDXADD:   // (D8,An,Xn)
              if(!(HIBYTE(Pipe1) & 0b00000001)) { // size
                if(!DISPLACEMENT_SIZE)
                  res1.w = GetShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                else
                  res1.w = GetShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                }
              else {
                if(!DISPLACEMENT_SIZE)
                  res1.x = GetIntValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                else
                  res1.x = GetIntValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                }
              _pc+=2;
              break;
            }
          if(!(HIBYTE(Pipe1) & 0b00000001))  // size
            DEST_REG_A.x += (signed short int)res1.w;
          else
            DEST_REG_A.x += res1.x;
          // no flag qua
          }
        else {    // ADD ADDX
          if(Pipe1 & 0b0000000100000000) { // direction... 
            switch(ADDRESSING) {
              case ABSOLUTEADD:    // 
                switch(ABSOLUTEADD_SIZE) {
                  case ABSSSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res1.b.l = GetValue(Pipe2.w);
                        break;
                      case WORD_SIZE:
                        res1.w = GetShortValue(Pipe2.w);
                        break;
                      case DWORD_SIZE:
                        res1.x = GetIntValue(Pipe2.w);
                        break;
                      }
                    break;
                  case ABSLSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res1.b.l = GetValue(Pipe2.d);
                        break;
                      case WORD_SIZE:
                        res1.w = GetShortValue(Pipe2.d);
                        break;
                      case DWORD_SIZE:
                        res1.x = GetIntValue(Pipe2.d);
                        break;
                      }
                    break;
                  case PCIDXSUBADD:   // (D16,PC)
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res1.b.l = GetValue(_pc+(signed short int)Pipe2.w);
                        break;
                      case WORD_SIZE:
                        res1.w = GetShortValue(_pc+(signed short int)Pipe2.w);
                        break;
                      case DWORD_SIZE:
                        res1.x = GetIntValue(_pc+(signed short int)Pipe2.w);
                        break;
                      }
                    break;
                  case PCDISPSUBADD:   // (D8,Xn,PC)
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        if(!DISPLACEMENT_SIZE)
                          res1.b.l = GetValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                        else
                          res1.b.l = GetValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                        break;
                      case WORD_SIZE:
                        if(!DISPLACEMENT_SIZE)
                          res1.w = GetShortValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                        else
                          res1.w = GetShortValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                        break;
                      case DWORD_SIZE:
                        if(!DISPLACEMENT_SIZE)
                          res1.x = GetIntValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                        else
                          res1.x = GetIntValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                        break;
                      }
                    break;
                  case IMMSUBADD:
                    res1.x = Pipe2.d; 
                    break;
                  }
                break;
              case DATAREGADD:   // Dn non dovrebbe esserci qua...
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = WORKING_REG_D.b.b0;
                    break;
                  case WORD_SIZE:
                    res1.w = WORKING_REG_D.w.l;
                    break;
                  case DWORD_SIZE:
                    res1.w = WORKING_REG_D.x;
                    break;
                  }
                break;
              case ADDRREGADD:   // An  qua ADDX PERO' VARREBBE ANCHE per DATAREG! (v. altri casi..)
                if(LOBYTE(Pipe1) & 0b00001000) {
                  switch(OPERAND_SIZE) {
                    case BYTE_SIZE:
                      res1.b.l = GetValue(--WORKING_REG_A.x);
                      res2.b.l = GetValue(--DEST_REG_A.x) + _f.CCR.Ext;
                      res3.w = (WORD)res1.b.l + (WORD)res2.b.l;
                      PutValue(DEST_REG_A.x, res3.b.l);
                      break;
                    case WORD_SIZE:
                      WORKING_REG_A.x-=2; res1.w = GetShortValue(WORKING_REG_A.x);
                      DEST_REG_A.x-=2; res2.w = GetShortValue(DEST_REG_A.x) + _f.CCR.Ext;
                      res3.x = (DWORD)res1.w + (DWORD)res2.w;
                      PutShortValue(DEST_REG_A.x, res3.x);
                      break;
                    case DWORD_SIZE:
                      WORKING_REG_A.x-=4; res1.x = GetIntValue(WORKING_REG_A.x);
                      DEST_REG_A.x-=4; res2.x = GetIntValue(DEST_REG_A.x) + _f.CCR.Ext;
                      res3.x = res1.x + res2.x;
                      PutIntValue(DEST_REG_A.x, res3.x);
                      break;
                    }
                  }
                else {
                  switch(OPERAND_SIZE) {
                    case BYTE_SIZE:
                      res1.b.l = WORKING_REG_D.b.b0;
                      res2.b.l = DEST_REG_D.b.b0 + _f.CCR.Ext;
                      res3.w = (WORD)res1.b.l + (WORD)res2.b.l;
                      DEST_REG_D.b.b0=res3.b.l;
                      break;
                    case WORD_SIZE:
                      res1.w = WORKING_REG_D.w.l; 
                      res2.w = DEST_REG_D.w.l + _f.CCR.Ext;
                      res3.x = (DWORD)res1.w + (DWORD)res2.w;
                      DEST_REG_D.w.l=res3.w;
                      break;
                    case DWORD_SIZE:
                      res1.x = WORKING_REG_D.x;
                      res2.x = DEST_REG_D.x + _f.CCR.Ext; 
                      res3.x = res1.x + res2.x;
                      DEST_REG_D.x=res3.x;
                      break;
                    }
                  }
                goto aggFlagA;
                break;
              case ADDRADD:   // (An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GetValue(WORKING_REG_A.x);
                    break;
                  case WORD_SIZE:
                    res1.w = GetShortValue(WORKING_REG_A.x);
                    break;
                  case DWORD_SIZE:
                    res1.x = GetIntValue(WORKING_REG_A.x);
                    break;
                  }
                break;
              case ADDRPINCADD:   // (An)+
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GetValue(WORKING_REG_A.x);
                    break;
                  case WORD_SIZE:
                    res1.w = GetShortValue(WORKING_REG_A.x);
                    break;
                  case DWORD_SIZE:
                    res1.x = GetIntValue(WORKING_REG_A.x);
                    break;
                  }
                break;
              case ADDRPDECADD:   // -(An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    WORKING_REG_A.x--;
                    res1.b.l = GetValue(WORKING_REG_A.x);
                    break;
                  case WORD_SIZE:
                    WORKING_REG_A.x-=2;
                    res1.w = GetShortValue(WORKING_REG_A.x);
                    break;
                  case DWORD_SIZE:
                    WORKING_REG_A.x-=4;
                    res1.x = GetIntValue(WORKING_REG_A.x);
                    break;
                  }
                break;
              case ADDRDISPADD:   // (D16,An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                    break;
                  case WORD_SIZE:
                    res1.w = GetShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                    break;
                  case DWORD_SIZE:
                    res1.x = GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                    break;
                  }
                break;
              case ADDRIDXADD:   // (D8,An,Xn)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res1.b.l = GetValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    else
                      res1.b.l = GetValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    break;
                  case WORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res1.w = GetShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    else
                      res1.w = GetShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    break;
                  case DWORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res1.x = GetIntValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    else
                      res1.x = GetIntValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    break;
                  }
                break;
              }
            }
          else {
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res1.b.l = DEST_REG_D.b.b0;
                break;
              case WORD_SIZE:
                res1.w = DEST_REG_D.w.l;
                break;
              case DWORD_SIZE:
                res1.x = DEST_REG_D.x;
                break;
              }
            }

          if(Pipe1 & 0b0000000100000000) { // direction
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res2.b.l = DEST_REG_D.b.b0;
                break;
              case WORD_SIZE:
                res2.w = DEST_REG_D.w.l;
                break;
              case DWORD_SIZE:
                res2.x = DEST_REG_D.x;
                break;
              }
            }
          else {
            switch(ADDRESSING) {
              case ABSOLUTEADD:    // #imm
                switch(ABSOLUTEADD_SIZE) {
                  case ABSSSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res2.b.l = GetValue(Pipe2.w);
                        break;
                      case WORD_SIZE:
                        res2.w = GetShortValue(Pipe2.w);
                        break;
                      case DWORD_SIZE:
                        res2.x = GetIntValue(Pipe2.w);
                        break;
                      }
                    break;
                  case ABSLSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res2.b.l = GetValue(Pipe2.d);
                        break;
                      case WORD_SIZE:
                        res2.w = GetShortValue(Pipe2.d);
                        break;
                      case DWORD_SIZE:
                        res2.x = GetIntValue(Pipe2.d);
                        break;
                      }
                    break;
                  case PCIDXSUBADD:   // (D16,PC) qua no!
                  case PCDISPSUBADD:   // (D8,Xn,PC)
                  case IMMSUBADD:
                    break;
                  }
                break;
              case DATAREGADD:   // Dn
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = WORKING_REG_D.b.b0;
                    break;
                  case WORD_SIZE:
                    res2.w = WORKING_REG_D.w.l;
                    break;
                  case DWORD_SIZE:
                    res2.x = WORKING_REG_D.x;
                    break;
                  }
                break;
              case ADDRREGADD:   // An
                switch(OPERAND_SIZE) {
                  case WORD_SIZE:
                    res2.w = WORKING_REG_A.w.l;
                    break;
                  case DWORD_SIZE:
                    res2.x = WORKING_REG_A.x;
                    break;
                  }
                break;
              case ADDRADD:   // (An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = GetValue(WORKING_REG_A.x);
                    break;
                  case WORD_SIZE:
                    res2.w = GetShortValue(WORKING_REG_A.x);
                    break;
                  case DWORD_SIZE:
                    res2.x = GetIntValue(WORKING_REG_A.x);
                    break;
                  }
                break;
              case ADDRPINCADD:   // (An)+
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = GetValue(WORKING_REG_A.x);
                    break;
                  case WORD_SIZE:
                    res2.w = GetShortValue(WORKING_REG_A.x);
                    break;
                  case DWORD_SIZE:
                    res2.x = GetIntValue(WORKING_REG_A.x);
                    break;
                  }
                break;
              case ADDRPDECADD:   // -(An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    WORKING_REG_A.x--;
                    res2.b.l = GetValue(WORKING_REG_A.x);
                    break;
                  case WORD_SIZE:
                    WORKING_REG_A.x-=2;
                    res2.w = GetShortValue(WORKING_REG_A.x);
                    break;
                  case DWORD_SIZE:
                    WORKING_REG_A.x-=4;
                    res2.x = GetIntValue(WORKING_REG_A.x);
                    break;
                  }
                break;
              case ADDRDISPADD:   // (D16,An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = GetValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                    break;
                  case WORD_SIZE:
                    res2.w = GetShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                    break;
                  case DWORD_SIZE:
                    res2.x = GetIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
                    break;
                  }
                break;
              case ADDRIDXADD:   // (D8,An,Xn)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res2.b.l = GetValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    else
                      res2.b.l = GetValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    break;
                  case WORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res2.w = GetShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    else
                      res2.w = GetShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    break;
                  case DWORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res2.x = GetIntValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    else
                      res2.x = GetIntValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                    break;
                  }
                break;
              }
            }

          switch(OPERAND_SIZE) {
            case BYTE_SIZE:
              res3.w = (WORD)res1.b.l + (WORD)res2.b.l;
              break;
            case WORD_SIZE:
              res3.x = (DWORD)res1.w + (DWORD)res2.w;
              break;
            case DWORD_SIZE:
              res3.x = res1.x + res2.x;
              break;
            }

          if(Pipe1 & 0b0000000100000000) { // direction
            switch(ADDRESSING) {
              case ABSOLUTEADD:    // #imm
                switch(ABSOLUTEADD_SIZE) {
                  case ABSSSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        PutValue(Pipe2.w, res3.b.l);
                        break;
                      case WORD_SIZE:
                        PutShortValue(Pipe2.w, res3.w);
                        break;
                      case DWORD_SIZE:
                        PutIntValue(Pipe2.w, res3.x);
                        break;
                      }
                    _pc+=2;
                    break;
                  case ABSLSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        PutValue(Pipe2.d, res3.b.l);
                        break;
                      case WORD_SIZE:
                        PutShortValue(Pipe2.d, res3.w);
                        break;
                      case DWORD_SIZE:
                        PutIntValue(Pipe2.d, res3.x);
                        break;
                      }
                    _pc+=4;
                    break;
                  case PCIDXSUBADD:   // (D16,PC) qua NO!
                  case PCDISPSUBADD:   // (D8,Xn,PC)
                  case IMMSUBADD:
                    break;
                  }
                break;
              case DATAREGADD:   // Dn .. in un punto il Doc dice questo no qua...
//              case ADDRREGADD:   // An CREDO DI NO idem
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    DEST_REG_D.b.b0 = res3.b.l;
                    break;
                  case WORD_SIZE:
                    DEST_REG_D.w.l = res3.w;
                    break;
                  case DWORD_SIZE:
                    DEST_REG_D.x = res3.x;
                    break;
                  }
                break;
              case ADDRREGADD:   // An
                switch(OPERAND_SIZE) {
                  case WORD_SIZE:
                    DEST_REG_A.w.l = res3.w;
                    break;
                  case DWORD_SIZE:
                    DEST_REG_A.x = res3.x;
                    break;
                  }
                break;
              case ADDRADD:   // (An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    PutValue(WORKING_REG_A.x,res3.b.l);
                    break;
                  case WORD_SIZE:
                    PutShortValue(WORKING_REG_A.x,res3.w);
                    break;
                  case DWORD_SIZE:
                    PutIntValue(WORKING_REG_A.x,res3.x);
                    break;
                  }
                break;
              case ADDRPINCADD:   // (An)+
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    PutValue(WORKING_REG_A.x,res3.b.l);
                    WORKING_REG_A.x++;
                    break;
                  case WORD_SIZE:
                    PutShortValue(WORKING_REG_A.x,res3.w);
                    WORKING_REG_A.x+=2;
                    break;
                  case DWORD_SIZE:
                    PutIntValue(WORKING_REG_A.x,res3.w);
                    WORKING_REG_A.x+=4;
                    break;
                  }
                break;
              case ADDRPDECADD:   // -(An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    PutValue(WORKING_REG_A.x,res3.b.l);
                    break;
                  case WORD_SIZE:
                    PutShortValue(WORKING_REG_A.x,res3.w);
                    break;
                  case DWORD_SIZE:
                    PutIntValue(WORKING_REG_A.x,res3.x);
                    break;
                  }
                break;
              case ADDRDISPADD:   // (D16,An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    PutValue(WORKING_REG_A.x + (signed short int)Pipe2.w,res3.b.l);
                    break;
                  case WORD_SIZE:
                    PutShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w,res3.w);
                    break;
                  case DWORD_SIZE:
                    PutIntValue(WORKING_REG_A.x + (signed short int)Pipe2.w,res3.x);
                    break;
                  }
                _pc+=2;
                break;
              case ADDRIDXADD:   // (D8,An,Xn)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      PutValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res3.b.l);
                    else
                      PutValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res3.b.l);
                    break;
                  case WORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      PutShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res3.w);
                    else
                      PutShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res3.w);
                    break;
                  case DWORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      PutIntValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res3.x);
                    else
                      PutIntValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res3.x);
                    break;
                  }
                _pc+=2;
                break;
              }
            }
          else {
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                DEST_REG_D.b.b0 = res3.b.l;
                break;
              case WORD_SIZE:
                DEST_REG_D.w.l = res3.w;
                break;
              case DWORD_SIZE:
                DEST_REG_D.x = res3.x;
                break;
              }
            }
          goto aggFlagA;
          }
				break;
        
			case 0xe0:   // ASd
			case 0xe1:
			case 0xe2:   // LSd
			case 0xe3:
			case 0xe4:   // ROXd
			case 0xe5:
			case 0xe6:   // ROd
			case 0xe7:
			case 0xe8:   // ASd LSd ROXd ROd
			case 0xe9:
			case 0xea:
			case 0xeb:
			case 0xec:
			case 0xed:
			case 0xee:
			case 0xef:
        if(OPERAND_SIZE == SIZE_ELSE) {
          res2.b.l=1;
          switch(ADDRESSING) {
            case ABSOLUTEADD:
              switch(ABSOLUTEADD_SIZE) {
                case ABSSSUBADD:
                  res1.w = GetShortValue(Pipe2.w);
                  _pc+=2;
                  break;
                case ABSLSUBADD:
                  res1.w = GetShortValue(Pipe2.d); 
                  _pc+=4;
                  break;
                case PCIDXSUBADD:   // (D16,PC)
                  res1.w = GetShortValue(_pc+(signed short int)Pipe2.w);
                  _pc+=2;
                  break;
                case PCDISPSUBADD:   // (D8,Xn,PC)
                  if(!DISPLACEMENT_SIZE)
                    res1.w = GetShortValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  else
                    res1.w = GetShortValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                  _pc+=2;
                  break;
                case IMMSUBADD:   // qua no direi
                  res1.w = Pipe2.d; // boh
                  _pc+=4;
                  break;
                }
              break;
            case DATAREGADD:   // Dn NO!
            case ADDRREGADD:   // An NO!
              break;
            case ADDRADD:   // (An)
              res1.w = GetShortValue(WORKING_REG_A.x);
              break;
            case ADDRPINCADD:   // (An)+
              res1.w = GetShortValue(WORKING_REG_A.x);
              break;
            case ADDRPDECADD:   // -(An)
              WORKING_REG_A.x-=2;
              res1.w = GetShortValue(WORKING_REG_A.x);
              break;
            case ADDRDISPADD:   // (D16,An)
              res1.w = GetShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w);
              _pc+=2;
              break;
            case ADDRIDXADD:   // (D8,An,Xn)
              if(!DISPLACEMENT_SIZE) {
                res1.w = GetShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l);
                }
              else {
                res1.w = GetShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l)  << Pipe2.d;
                }
              _pc+=2;
              break;
            }
          
          switch(HIBYTE(Pipe1) & 0b00000110) {
            case 0b000:   //ASd
              if(Pipe1 & 0b0000000100000000)  // rotate direction
                res3.w = res1.w << res2.b.l;
              else
                res3.w = (signed short int)res1.w >> res2.b.l;
              goto aggFlagRC;
              break;
            case 0b010:   //LSd
              if(Pipe1 & 0b0000000100000000)  // rotate direction
                res3.w = res1.w << res2.b.l;
              else
                res3.w = res1.w >> res2.b.l;
              goto aggFlagRC;
              break;
            case 0b100:   //ROXd
              if(Pipe1 & 0b0000000100000000) {  // rotate direction
                res3.w = res1.w << res2.b.l;
                if(_f.CCR.Ext)
                  res3.w |= 1;
                }
              else {
                res3.w = res1.w >> res2.b.l;
                if(_f.CCR.Ext)
                  res3.w |= 0x8000;
                }
              if(res2.b.l) {
                if(Pipe1 & 0b0000000100000000)  // 
                  _f.CCR.Ext=_f.CCR.Carry=res1.w & 0x8000 ? 1 : 0;
                else
                  _f.CCR.Ext=_f.CCR.Carry=res1.w & 0x0001;
                }
              else {
                _f.CCR.Carry=_f.CCR.Ext;
                }
              break;
            case 0b110:   //ROd
              if(Pipe1 & 0b0000000100000000) { // rotate direction
                res3.w = res1.w << res2.b.l;
                if(res1.w & 0x8000)
                  res3.w |= 1;
                }
              else {
                res3.w = res1.w >> res2.b.l;
                if(res1.w & 1)
                  res3.w |= 0x8000;
                }
              
aggFlagRC:
              if(res2.b.l) {
                if(Pipe1 & 0b0000000100000000)
                  _f.CCR.Carry=res1.w & 0x8000 ? 1 : 0;
                else
                  _f.CCR.Carry=res1.w & 0x0001;
                }
              else
                _f.CCR.Carry=0;
              break;
            }
        
          switch(ADDRESSING) {
            case ABSOLUTEADD:
              switch(ABSOLUTEADD_SIZE) {
                case ABSSSUBADD:
                  PutShortValue(Pipe2.w,res3.w);
                  break;
                case ABSLSUBADD:
                  PutShortValue(Pipe2.d,res3.w); 
                  break;
                case PCIDXSUBADD:   // (D16,PC)
                  PutShortValue(_pc+(signed short int)Pipe2.w,res3.w);
                  break;
                case PCDISPSUBADD:   // (D8,Xn,PC)
                  if(!DISPLACEMENT_SIZE)
                    PutShortValue(_pc + (((signed int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res3.w);
                  else
                    PutShortValue(_pc + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res3.w);
                  break;
                case IMMSUBADD:   // qua no!
                  break;
                }
              break;
            case DATAREGADD:   // Dn NO!
            case ADDRREGADD:   // An NO!
              break;
            case ADDRADD:   // (An)
              PutShortValue(WORKING_REG_A.x,res3.w);
              break;
            case ADDRPINCADD:   // (An)+
              PutShortValue(WORKING_REG_A.x,res3.w);
              WORKING_REG_A.x+=2;
              break;
            case ADDRPDECADD:   // -(An)
              PutShortValue(WORKING_REG_A.x,res3.w);
              break;
            case ADDRDISPADD:   // (D16,An)
              PutShortValue(WORKING_REG_A.x + (signed short int)Pipe2.w,res3.w);
              break;
            case ADDRIDXADD:   // (D8,An,Xn)
              if(!DISPLACEMENT_SIZE) {
                PutShortValue(WORKING_REG_A.x + (((signed short int)DISPLACEMENT_REG.w.l)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res3.w);
                }
              else {
                PutShortValue(WORKING_REG_A.x + (((signed int)DISPLACEMENT_REG.x)*DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l,res3.w);
                }
              break;
            }
          goto aggFlagR2;
          }
        else {
          if(!(Pipe1 & 0b0000000000010000)) { // i/r
            res2.b.l=(Pipe1 & 0b0000111000000000) >> 9;
            if(!res2.b.l)
              res2.b.l=8;
            }
          else {
            res2.b.l=DEST_REG_D.b.b0 & 63;
            }
          switch(OPERAND_SIZE) {
            case BYTE_SIZE:
              res1.b.l = WORKING_REG_D.b.b0;
              switch(Pipe1 & 0b0000000000011000) {
                case 0b00000:   //ASd
                  if(Pipe1 & 0b0000000100000000)  // rotate direction
                    res3.b.l = res1.b.l << res2.b.l;
                  else
                    res3.b.l = (signed char)res1.b.l >> res2.b.l;
                  goto aggFlagRC1;
                  break;
                case 0b01000:   //LSd
                  if(Pipe1 & 0b0000000100000000)  // rotate direction
                    res3.b.l = res1.b.l << res2.b.l;
                  else
                    res3.b.l = res1.b.l >> res2.b.l;
                  goto aggFlagRC1;
                  break;
                case 0b10000:   //ROXd
                  if(Pipe1 & 0b0000000100000000) {  // rotate direction
                    res3.b.l = res1.b.l << res2.b.l;
                    if(_f.CCR.Ext)
                      res3.b.l |= 1;
                    }
                  else {
                    res3.b.l = res1.b.l >> res2.b.l;
                    if(_f.CCR.Ext)
                      res3.b.l |= 0x80;
                    }
                  if(res2.b.l) {
                    if(Pipe1 & 0b0000000100000000)  // 
                      _f.CCR.Ext=_f.CCR.Carry=res1.w & 0x80 ? 1 : 0;
                    else
                      _f.CCR.Ext=_f.CCR.Carry=res1.w & 0x01;
                    }
                  else {
                    _f.CCR.Carry=_f.CCR.Ext;
                    }
                  break;
                case 0b11000:   //ROd
                  if(Pipe1 & 0b0000000100000000) { // rotate direction
                    res3.b.l = res1.b.l << res2.b.l;
                    if(_f.CCR.Carry)
                      res3.b.l |= 1;
                    }
                  else {
                    res3.b.l = res1.b.l >> res2.b.l;
                    if(_f.CCR.Carry)
                      res3.b.l |= 0x80;
                    }
                  
aggFlagRC1:
                  if(res2.b.l) {
                    if(Pipe1 & 0b0000000100000000)
                      _f.CCR.Carry=res1.b.l & 0x80 ? 1 : 0;
                    else
                      _f.CCR.Carry=res1.b.l & 0x01;
                    }
                  else
                    _f.CCR.Carry=0;
                  break;
                }
              WORKING_REG_D.b.b0 = res3.b.l;
              break;
            case WORD_SIZE:
              res1.w=WORKING_REG_D.w.l;
              switch(Pipe1 & 0b0000000000011000) {
                case 0b00000:   //ASd
                  if(Pipe1 & 0b0000000100000000)  // rotate direction
                    res3.w = res1.w << res2.b.l;
                  else
                    res3.w = (signed short int)res1.w >> res2.b.l;
                  goto aggFlagRC2;
                  break;
                case 0b01000:   //LSd
                  if(Pipe1 & 0b0000000100000000)  // rotate direction
                    res3.w = res1.w << res2.b.l;
                  else
                    res3.w = res1.w >> res2.b.l;
                  goto aggFlagRC2;
                  break;
                case 0b10000:   //ROXd
                  if(Pipe1 & 0b0000000100000000) {  // rotate direction
                    res3.w = res1.w << res2.b.l;
                    if(_f.CCR.Ext)
                      res3.w |= 1;
                    }
                  else {
                    res3.w = res1.w >> res2.b.l;
                    if(_f.CCR.Ext)
                      res3.w |= 0x8000;
                    }
                  if(res2.b.l) {
                    if(Pipe1 & 0b0000000100000000)  // 
                      _f.CCR.Ext=_f.CCR.Carry=res1.w & 0x8000 ? 1 : 0;
                    else
                      _f.CCR.Ext=_f.CCR.Carry=res1.w & 0x0001;
                    }
                  else {
                    _f.CCR.Carry=_f.CCR.Ext;
                    }
                  break;
                case 0b11000:   //ROd
                  if(Pipe1 & 0b0000000100000000) { // rotate direction
                    res3.w = res1.w << res2.b.l;
                    if(_f.CCR.Carry)
                      res3.w |= 1;
                    }
                  else {
                    res3.w = res1.b.l >> res2.b.l;
                    if(_f.CCR.Carry)
                      res3.w |= 0x8000;
                    }
                  
aggFlagRC2:
                  if(res2.b.l) {
                    if(Pipe1 & 0b0000000100000000)  // 
                      _f.CCR.Carry=res1.w & 0x8000 ? 1 : 0;
                    else
                      _f.CCR.Carry=res1.w & 0x0001;
                    }
                  else
                    _f.CCR.Carry=0;
                  break;
                }
              WORKING_REG_D.w.l = res3.w;
              break;
            case DWORD_SIZE:
              res1.x=WORKING_REG_D.x;
              if(Pipe1 & 0b0000000100000000)  // rotate direction
                _f.CCR.Carry=res1.x & 0x80000000 ? 1 : 0;
              else
                _f.CCR.Carry=res1.x & 0x00000001;
              switch(Pipe1 & 0b0000000000011000) {
                case 0b00000:   //ASd
                  if(Pipe1 & 0b0000000100000000)  // rotate direction
                    res3.x = res1.x << res2.b.l;
                  else
                    res3.x = (signed int)res1.x >> res2.b.l;
                  goto aggFlagRC4;
                  break;
                case 0b01000:   //LSd
                  if(Pipe1 & 0b0000000100000000)  // rotate direction
                    res3.x = res1.x << res2.b.l;
                  else
                    res3.x = res1.x >> res2.b.l;
                  goto aggFlagRC4;
                  break;
                case 0b10000:   //ROXd
                  if(Pipe1 & 0b0000000100000000) {  // rotate direction
                    res3.x = res1.x << res2.b.l;
                    if(_f.CCR.Ext)
                      res3.x |= 1;
                    }
                  else {
                    res3.x = res1.x >> res2.b.l;
                    if(_f.CCR.Ext)
                      res3.x |= 0x80000000;
                    }
                  if(res2.b.l) {
                    if(Pipe1 & 0b0000000100000000)  // 
                      _f.CCR.Ext=_f.CCR.Carry=res1.x & 0x80000000 ? 1 : 0;
                    else
                      _f.CCR.Ext=_f.CCR.Carry=res1.x & 0x00000001;
                    }
                  else {
                    _f.CCR.Carry=_f.CCR.Ext;
                    }
                  break;
                case 0b11000:   //ROd
                  if(Pipe1 & 0b0000000100000000) { // rotate direction
                    res3.x = res1.x << res2.b.l;
                    if(_f.CCR.Carry)
                      res3.x |= 1;
                    }
                  else {
                    res3.x = res1.x >> res2.b.l;
                    if(_f.CCR.Carry)
                      res3.x |= 0x80000000;
                    }
                  
aggFlagRC4:
                  if(res2.b.l) {
                    if(Pipe1 & 0b0000000100000000)  // 
                      _f.CCR.Carry=res1.x & 0x80000000 ? 1 : 0;
                    else
                      _f.CCR.Carry=res1.x & 0x00000001;
                    }
                  else
                    _f.CCR.Carry=0;
                  break;
                }
              WORKING_REG_D.x = res3.x;
              break;
            }
          }
        
aggFlagR:
        switch(OPERAND_SIZE) {
          case BYTE_SIZE:
            _f.CCR.Zero=!res3.b.l;
            _f.CCR.Sign=!!(res3.b.l & 0x80);
            break;
          case WORD_SIZE:
aggFlagR2:
            _f.CCR.Zero=!res3.w;
            _f.CCR.Sign=!!(res3.w & 0x8000);
            break;
          case DWORD_SIZE:
            _f.CCR.Zero=!res3.x;
            _f.CCR.Sign=!!(res3.x & 0x80000000);
            break;
          }
        _f.CCR.Ovf = 0;
                
				break;
        
      default:
		    res3.b.l=4;
		    goto doTrap;
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

