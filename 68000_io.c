//http://www.dilwyn.me.uk/docs/ebooks/olqlug/QL%20Manual%20-%20Concepts.htm#memorymap
//https://www.atarimagazines.com/v5n3/ExceptionsInterrupts.php  per eccezioni/hw


#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <xc.h>

#include "68000_PIC.h"

#if defined(ST7735)
#include "Adafruit_ST77xx.h"
#include "Adafruit_ST7735.h"
#endif
#if defined(ILI9341)
#include "Adafruit_ili9341.h"
#endif
#include "adafruit_gfx.h"


#if MACINTOSH
#include "gcr.h"
#endif


extern struct ADDRESS_EXCEPTION AddressExcep,BusExcep;
extern BYTE CPUPins;
extern union WORD_BYTE Pipe1;
extern union __attribute__((__packed__)) PIPE Pipe2;

#ifdef QL
BYTE __attribute((aligned(65536))) ram_seg[RAM_SIZE];
#warning RAM_SEG ALIGNED x debug!
BYTE *rom_seg;
BYTE *rom_seg2;
#define VIDEO_ADDRESS_BASE 0x20000
DWORD VideoAddress=VIDEO_ADDRESS_BASE;
BYTE VideoMode=0b00000010;      // parto spento
SWORD VICRaster=0;
union __attribute__((__packed__)) DWORD_BYTE RTC;
BYTE IPCW=255,IPCR=0, IPCData=0,IPCCnt=4,IPCState=0;
BYTE MDVmotor=0;
extern volatile BYTE TIMIRQ,VIDIRQ,KBDIRQ,SERIRQ,RTCIRQ,MDVIRQ;
BYTE IRQenable=0;
#elif MACINTOSH
BYTE __attribute((aligned(65536))) ram_seg[RAM_SIZE];
BYTE *rom_seg;
BYTE *rom_seg2;
BYTE z8530RegR[16],z8530RegW[16],z8530RegRDA,z8530RegRDB,z8530RegWDA,z8530RegWDB;
BYTE IWMRegR[4]={0,0x0/*status*/,0b10111111/*handshake*/,0x0/*mode*/},    // 1f letto da disasm rom... pare, all'inizio
  IWMRegW[4]={0,0x0/*status*/,0b10111111/*handshake*/,0x0/*mode*/},
  IWMSelect=0,    		// https://www.applefritter.com/content/iwm-reverse-engineering
  floppyState[2]={0x80,0} /* b0=stepdir,b1=disk-switch,b2=motor,b3=head,b7=diskpresent*/,
  floppyTrack=0,floppySector=0;
WORD floppyTach=0;
static uint8_t sectorData[1024];
static const uint8_t sectorsPerTrack[]={
  12,12,12,12,12,12,12,12,12,12,
  12,12,12,12,12,12,11,11,11,11,
  11,11,11,11,11,11,11,11,11,11,
  11,11,10,10,10,10,10,10,10,10,
  10,10,10,10,10,10,10,10,9,9,
  9,9,9,9,9,9,9,9,9,9,
  9,9,9,9,9,8,8,8,8,8,
  8,8,8,8,8,8,8,8,8,8
  };
static const uint8_t interleave_table[5][12] = {
  {//speed group 1:
  0,6, 1,7, 2,8, 3,9, 4,10, 5,11,
  },
  {//speed group 2:
  0,6, 1,7, 2,8, 3,9, 4,10, 5,
  },
  {//speed group 3:
  0,5, 1,6, 2,7, 3,8, 4,9,
  },
  {//speed group 4:
  0,5, 1,6, 2,7, 3,8, 4,
  },
  {//speed group 5:
  0,4, 1,5, 2,6, 3,7
  },
  };

extern const unsigned char MACINTOSH_DISK[];
const unsigned char *macintoshDisk=MACINTOSH_DISK;
DWORD macintoshSectorPtr=0;
BYTE VIA1RegR[16]  = {
  0x8c,0x10,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
  },
  VIA1RegW[16] = {
  0x8c,0x10,0x00,0x00,0x00,0x00,0x00,0x00,    // Sound, Mouse button, RTCenable; OVERLAY=1, direi
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
  },VIA1ShCnt,VIA1IRQSet;
//#define VIDEO_ADDRESS_BASE 0x01a700   // occhio cmq, v.
DWORD VideoAddress[2]={0x01a700,0x012700};    // per non partire con 0!
SWORD VICRaster=0;
union __attribute__((__packed__)) DWORD_BYTE RTC;
BYTE PRam[20];
BYTE mouseState=255;
BYTE RTCdata,RTCcount,RTCselect,RTCwp=0,RTCstate=0;
extern volatile BYTE TIMIRQ,VIDIRQ,KBDIRQ,SCCIRQ,RTCIRQ,VIAIRQ;
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
extern BYTE Keyboard[];
extern volatile DWORD millis;




#ifdef QL
#define IO_BASE_ADDRESS 0x18000
#elif MACINTOSH
DWORD getMacSector(void);
uint8_t *encodeMacSector(void);
#elif MICHELEFABBRI
#else
#define IO_BASE_ADDRESS 0x7c000
#define LCD_BOARD_ADDRESS 0x7c100
// emulo display LCD testo (4x20, 4x20 o altro) come su scheda Z80:
//  all'indirizzo+0 c'è la porta comandi (fili C/D, RW e E (E2)), a indirizzo+1 la porta dati (in/out)
#define IO_BOARD_ADDRESS 0x0
#endif

BYTE GetValue(DWORD t) {
	register BYTE i;

#ifdef QL
  
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
  else {
    //Typically, you tried to access hardware directly when not in supervisor mode or tried to access non-existent hardware. 
    /*BusExcep.active=1;
    BusExcep.addr=t;
    BusExcep.descr.in=1;
    BusExcep.descr.rw=1;*/
    CPUPins |= BErr;
    // finire...
    }

  
#elif MACINTOSH
  //https://bitsavers.org/pdf/apple/mac/
  switch(LOBYTE(HIWORD(t)) >> 4) {

    case 0:
    case 1:
    case 2:
    case 3:     // 
      if(VIA1RegW[1] & 0b00010000) {    // OVERLAY, PA<4>
// superfluo      	if(t < ROM_SIZE) {			
        if(!(t & 0x100000))
          i=rom_seg[t];
//          }
        }
      else {
    		i=ram_seg[t & (RAM_SIZE-1)];
        }
      break;
      
/*    case 3:        //   IPC Rd   FINIRE QUA
      i=IPCW;     // mah dice qua solo WR..  scrive 0x0e, pare
      // (tastiera ecc va qua...)  scrive 4 o 8 bit, data 1=$0E or 0=$0C
//				IPCCnt++;
      break;*/
      // 0x0d è IPC per seriale_baud_rate, 0x01 è report input status, 6=read ser1, 7=read ser2, 8=read keyboard

    case 4:
      i=rom_seg[t & (ROM_SIZE-1)];
      break;
      
    case 5:     // nulla, dice
      break;
      
    case 6:
      if(VIA1RegW[1] & 0b00010000) {    // OVERLAY, PA<4>
// superfluo      	if(t < ROM_SIZE) {			
    		i=ram_seg[t & (RAM_SIZE-1)];
//          }
        }
      else {
        i=rom_seg2[t & (ROM_SIZE-1)];
        }
      break;
      
    case 7:     // nulla, dice
      break;
      
    case 8:     // nulla, dice (2 devices on bus
      break;
      
    case 0x9:      // SCC 8530 read
    case 0xa:      // DO NOT USE (2 devices on bus
//    case 0xb:      // SCC 8530 write
      if(t & 1)     // SCC reset
        ;
      t &= 7;
      switch(t) {    // NON SI CAPISCE se ci sono 2 set di control reg A e B, e dov e il registro 0...
        case 0x0:   //  read from control register channel B specified by control register 0
          z8530RegW[0]=0;   // v. $1a8c
    			switch(z8530RegW[0] & 0xf) {
            case 0:
              break;
            }
    			i=z8530RegR[z8530RegW[0] & 0xf];
          break;
        case 0x1:   //  write to control register channel B specified by control register 0
          break;
        case 0x2:   //  read from control register channel A specified by control register 0
              //qua ci sarebbero i 2 assi del mouse... X1 e Y1
    			switch(z8530RegW[0] & 0xf) {
            case 0:
              break;
//X1 and Y1, are connected to the SCC's DCDA and DCDB inputs, respectively, while the quadrature signals, X2 and Y2, go to inputs of the VIA's data register B
            case 2:
              if(mouseState & 0b00000001)
                z8530RegR[2] |= 0b00001000;      //X1 boh
              else
                z8530RegR[2] &= ~0b00001000;      //e verificare fronte/valore
              break;
            case 3:
              if(mouseState & 0b00000100)
                z8530RegR[3] |= 0b00001000;      //Y1 boh
              else
                z8530RegR[3] &= ~0b00001000;      //e verificare fronte/valore
              break;
            }
              
    			i=z8530RegR[z8530RegW[0] & 0xf];
          break;
        case 0x3:   //  write to control register channel A specified by control register 0
          break;
        case 0x4:   //  read from data register channel B
    			i=z8530RegRDB;
          break;
        case 0x5:   //  write to data register channel B
          break;
        case 0x6:   //  read from data register channel A; phase adjust (se word access
          // viene letto durante lettura da disco se ORA di VIA è 0x80...
          
    			i=z8530RegRDA;
          break;
        case 0x7:   //  write to data register channel A; reset SCC
          memset(z8530RegR,0,sizeof(z8530RegR));
          memset(z8530RegW,0,sizeof(z8530RegW));
          z8530RegRDA=z8530RegRDB=0;
          z8530RegWDA=z8530RegWDB=0;
          z8530RegW[4] =0b00000100;   // diverso da hw reset!
          z8530RegW[7] =0b00100000;
          z8530RegW[11]=0b00001000;
          z8530RegW[14]=0b00100000;
          z8530RegW[15]=0b11111000;
          z8530RegR[1] =0b00000110;
          break;
        }
      break;
        
    case 0xc:      // DO NOT USE (2 devices on bus
      break;
      
    case 0xd:      // IWM; v. https://github.com/evansm7/umac per paravirt emulation
/*      
Select bits:
	  7   6   5   4   3     2   1   0  
    Q7  Q6  SEL EN  LSTRB CA2 CA1 CA0

The bits of the mode register are laid out as follows:
	  7   6    5     4   3   2   1   0  
	+---+-----+-----+---+---+---+---+---+
	| R | Rst | Tst | S | C | M | H | L |
	+---+-----+-----+---+---+---+---+---+
With the various bit meanings described below:
	Bit	Function
	---	--------
	 R	Reserved
	 S	Clock speed:
			0 = 7MHz
			1 = 8MHz
		Should always be 0.
	 C	Bit cell time:
			0 = 4usec/bit (for 5.25 drives)
			1 = 2usec/bit (for 3.5 drives)
	 M	Motor-off timer:
			0 = leave drive on for 1 sec after program turns it off
			1 = no delay
		Should be 0 for 5.25 and 1 for 3.5.
	 H	Handshake protocol:
			0 = synchronous (software must supply proper timing for writing data)
			1 = asynchronous (IWM supplies timing)
		Should be 0 for 5.25 and 1 for 3.5.
	 L	Latch mode:
			0 = read-data stays valid for about 7usec
			1 = read-data stays valid for full byte time
		Should be 0 for 5.25 and 1 for 3.5.
 
The bits of the status register are laid out as follows:
	  7   6   5   4   3   2   1   0  
	+---+---+---+---+---+---+---+---+
	| I | R | E | S | C | M | H | L |
	+---+---+---+---+---+---+---+---+
	Bit	Function
	---	--------
	 I	Sense input.
			write-protect indicator (5.25-inch drive)
			general status line	(3.5-inch drive)
	 R	Reserved.
	 E	Drive enabled
			0 = no disk drive is on
			1 = a disk drive is on.
	 S	Same as S bit in the mode register.
	 C	Same as C bit in the mode register.
	 M	Same as M bit in the mode register.
	 H	Same as H bit in the mode register.
	 L	Same as L bit in the mode register.
      
The bits of the handshake register are laid out as follows:
	  7   6   5   4   3   2   1   0  
	+---+---+---+---+---+---+---+---+
	| B | U | R | R | R | R | R | R |
	+---+---+---+---+---+---+---+---+
	Bit	Function
	---	--------
	 B	Register Ready
			0 = IWM is busy
			1 = IWM is ready for data
	 U	Under-run
			0 = write under-run has occurred (the program took too long to write the next byte)
			1 = no under-run
	 R	Reserved.      
      */
      { uint8_t oldIwm=IWMSelect;
      i=(LOWORD(t) >> 10) & 0x7;
      if(t & 0x200)   // A9
        IWMSelect |= (1 << i);    // A10..A12; A9 è usato come set/reset...
      else
        IWMSelect &= ~(1 << i);
//      i=IWMSelect >> 6;
      if(IWMRegW[3] & 0b00100000)   // se test mode, operation unspecified a parte scrivere mode e leggere status <5>
        ;
      if((oldIwm & 0b00001000) && !(IWMSelect & 0b00001000)) {  // se LSTRB scende...
        // control function  Be sure that and that CA0 and CA1 are set high before changing SEL
        //  nb CA2 = 4 = valore parm!
        i=((VIA1RegW[1] & 0x20) >> 2) | (IWMSelect & 7);
        uint8_t qfloppy=(IWMSelect & 0b00100000) >> 5;
        switch(i) {   // solo i floppy 3.5, dice..
          case 0:   // DIRTN Set step direction inward (toward higher-numbered tracks.)
            floppyState[qfloppy] |= 0x1;
            break;
          case 1/*4*/:   // STEP Step one track in current direction (takes about 12 msec).
            if(floppyState[qfloppy] & 0x1) {
              if(floppyTrack<80-1)
                floppyTrack++;
              }
            else {
              if(floppyTrack>0)
                floppyTrack--;
              }
            floppySector=0;
            encodeMacSector();
            
            break;
          case 2/*8*/:   // MOTORON Turn spindle motor on.
            floppyState[qfloppy] |= 0x4;
            floppySector=0;
            encodeMacSector();// serve se parto con track=0...
            break;
          case 0x4/*1*/:   // Set step direction outward (toward lower-numbered tracks.
            floppyState[qfloppy] &= ~0x1;
            break;
          case 6/*9*/:   // Turn spindle motor off.
            floppyState[qfloppy] &= ~0x4;

            break;
          case 7/*0x0d*/:  // EJECT Eject the disk. This takes about 1/2 sec to complete. The drive may not recognize further control commands until this operation is complete.
            floppyState[qfloppy] &= ~0x80;
            break;
          case 0xc/*3*/:   // Reset disk-switched flag?  (The firmware uses this to clear disk-switched errors.)
            floppyState[qfloppy] &= ~0x2;
            break;
          default:
            break;
          }
        }
      }
      i=IWMSelect >> 6;
      switch(i) {
        case 0b00:    // data
          if(IWMSelect & 0b00010000) {          // data register       (if drive is on)
//            unsigned char c;
/*            gcr_encode(*macintoshDisk);
            macintoshDisk++;
            if(gcr_get_encoded(&c)) {
              IWMRegR[0]=c;    // 
              }*/
              i=((VIA1RegW[1] & 0x20) >> 2) | (IWMSelect & 7);
              uint8_t qfloppy=(IWMSelect & 0b00100000) >> 5;
              switch(i) {   // 
                case 4/*1*/:   // RDDATA0 Instantaneous data from lower head. Reading this bit configures the drive to do I/O with the lower head.
                  floppyState[qfloppy] &= ~0x8;
//                  do {
                    IWMRegR[0]=sectorData[macintoshSectorPtr++];    // skippo pad a 0 per praticità mia
//                    } while(!IWMRegR[0]); //inutile cmq
                  if(macintoshSectorPtr >= 710+17) { //(continuano a esserci letture ciucche all'inizio e quindi si sposta di settore durante la ricerca header a $18c4a
//                  
                    floppySector++;
                    if(floppySector>=sectorsPerTrack[floppyTrack])
                      floppySector=0;
                    encodeMacSector();
                    }
                  // arriva anche con 01 v. sotto
                  break;
                case 0xc/*3*/:   // RDDATA1 Instantaneous data from upper head. Reading this bit configures the drive to do I/O with the upper head.
                  floppyState[qfloppy] |= 0x8;
//                  do {
                    IWMRegR[0]=sectorData[macintoshSectorPtr++];    // skippo pad a 0 per praticità mia
//                    } while(!IWMRegR[0]);
                  if(macintoshSectorPtr >= 710+17) {
                    floppySector++;
                    if(floppySector>=sectorsPerTrack[floppyTrack])
                      floppySector=0;
                    encodeMacSector();
                    }
                  // arriva anche con 01 v. sotto
                  break;
                }
            // MSB=1 indica byte valido - implicito nella conversione che faccio sotto
       			i=IWMRegR[0];    // 
       			IWMRegR[0]=0;    // 
            }
          else {          // all ones (if drive is off)
       			i=0xff;    // 
            }
          break;
        case 0b01:    // status
          if(IWMSelect & 0b00010000) {       // ENABLE (Motor) (per accedere a Status e Mode, questo dev'essere off
            if(IWMSelect & 0b00100000) {   // drive-sel 2
              if(0)   // forzo enable inattivo
                IWMRegR[1] |= 0b00100000;
              else
                IWMRegR[1] &= ~0b00100000;
              }
            else {    // drive-sel 1
              if(1)   // forzo enable attivo
                IWMRegR[1] |= 0b00100000;
              else
                IWMRegR[1] &= ~0b00100000;
              }
            }
          else {
            IWMRegR[1] &= ~0b00100000;
            }
          i=((VIA1RegW[1] & 0x20) >> 2) | (IWMSelect & 7);
          uint8_t qfloppy=(IWMSelect & 0b00100000) >> 5;
          switch(i) {   // solo i floppy 3.5, dice..
            case 0:   // DIRTN Step direction. 0 = head set to step inward (toward higher-numbered tracks), 1 = head set to step outward (toward lower-numbered tracks)
              if(!(floppyState[qfloppy] & 0x1))
                IWMRegR[1] |= 0x80;
              else
                IWMRegR[1] &= ~0x80;
              break;
            case 1/*4*/:   // STEP Disk is stepping. 0 = head is stepping between tracks,	1 = head is not stepping.
              if(1 /* :) */)
                IWMRegR[1] |= 0x80;
              else
                IWMRegR[1] &= ~0x80;
              break;
            case 2/*8*/:   // MOTORON Motor on. 0 = spindle motor is spinning, 1 = motor is off
              if(!(floppyState[qfloppy] & 0x4) /*IWMSelect & 0b00100000*/)
                IWMRegR[1] |= 0x80;
              else
                IWMRegR[1] &= ~0x80;
              break;
            case 3/*C*/:  // Disk switched?	0 = user ejected disk by pressing the eject button, 1 = disk not ejected.
              if(floppyState[qfloppy] & 0x80 /*IWMSelect & 0b00100000*/)
                IWMRegR[1] |= 0x80;
              else
                IWMRegR[1] &= ~0x80;
              break;
            case 4/*1*/:   // RDDATA0 Instantaneous data from lower head. Reading this bit configures the drive to do I/O with the lower head.
              // v. anche VIA1RegW[1] ORA b5
              floppyState[qfloppy] &= ~0x8;
              if(0 /**/)
                IWMRegR[1] |= 0x80;
              else
                IWMRegR[1] &= ~0x80;
              floppySector=0;
              encodeMacSector();
              // arriva anche con 00 v. sopra
              break;
            case 6/*9*/:   // SIDES Number of sides.	0 = single-sided drive, 1 = double-sided drive
#ifndef MACINTOSHPLUS
              if(0 /* 400 su original/128, 800K disks su macintoshplus  V ANCHE ENCODE sotto*/) 
#else
              if(1 /* 400 su original/128, 800K disks su macintoshplus  V ANCHE ENCODE sotto*/) 
#endif
                IWMRegR[1] |= 0x80;
              else
                IWMRegR[1] &= ~0x80;
              break;
            case 0x7/*F*/:  // DRVIN Drive installed. 0 = drive is connected, 1 = no drive is connected
              if(!(floppyState[qfloppy] & 0x80) /* drive 0 connected, 1 no*/)
                IWMRegR[1] |= 0x80;
              else
                IWMRegR[1] &= ~0x80;
              break;
            case 8/*2*/:   // CSTIN Disk in place. 0 = disk in drive, 1 = drive is empty.
              if(!(floppyState[qfloppy] & 0x80) /*IWMSelect & 0b00100000*/)
                IWMRegR[1] |= 0x80;
              else
                IWMRegR[1] &= ~0x80;
              break;
            case 9/*6*/:   // WRTPRT Disk locked.	0 = disk is write protected, 1 = disk is write-enabled.
              if(0 /* Write protect */)
                IWMRegR[1] |= 0x80;
              else
                IWMRegR[1] &= ~0x80;
              break;
            case 0xa/*A*/:   // TK0 Track 0.	0 = head is at track 0, 1 = head is at some other track;  This bit becomes valid beginning 12 msec after the step that places the head at track 0.
              if(floppyTrack)
                IWMRegR[1] |= 0x80;
              else
                IWMRegR[1] &= ~0x80;
              break;
            case 0xb/*E*/:  // TACH Tachometer.  60 pulses per disk revolution
            {uint32_t n;
/*Track 00 to track 15 : 394 RPM         PWM $1FD00 sembra contenere valori tra 0x15 e 0x2b (spero, alternati con FF che dovrebbero essere audio...
  Track 16 to track 31 : 429 RPM
  Track 32 to track 47 : 472 RPM
  Track 48 to track 63 : 525 RPM
  Track 64 to track 79 : 590 RPM*/
              switch(sectorsPerTrack[floppyTrack]) {  // (TMR3 conta fino a 1950, ogni unità=320nS
                case 12:
                  n=787000L/394;    //~1.5-2.5uS per count, vogliamo 2.5mS ogni tach
                  break;
                case 11:
                  n=787000L/429;
                  break;
                case 10:
                  n=787000L/472;
                  break;
                case 9:
                  n=787000L/525;
                  break;
                case 8:
                  n=787000L/590; //~1.5-2.5uS per count, vogliamo 1.6mS ogni tach
                  break;
                }
              //serve 0x04000000 diviso questo
//              if(!(millis & 8) /* verificare, da ~6/sec a 10/sec dip. da track, ossia da 360 a 600 pulses, questo va a 1600Hz */)
              if((floppyState[qfloppy] & 0x84) == 0x84) {    // disco dentro e motore on
                if((floppyTach/*TMR3*/ % n) > ((n/2)+(rand() & 1)))
                  IWMRegR[1] |= 0x80;
                else
                  IWMRegR[1] &= ~0x80; 
                }
            }
              break;
            case 0xc/*3*/:   // RDDATA1 Instantaneous data from upper head. Reading this bit configures the drive to do I/O with the upper head.
              // v. anche VIA1RegW[1] ORA b5
              floppyState[qfloppy] |= 0x8;
              if(0 /**/)
                IWMRegR[1] |= 0x80;
              else
                IWMRegR[1] &= ~0x80;
              floppySector=0;
              encodeMacSector();
              // arriva anche con 00 v. sopra
              break;
            case 0xe/*B*/:  // Disk ready for reading?	0 = ready, 1 = not ready; I am not too sure about this bit. The firmware waits for this bit to go low before trying to read a sector address field.
              if(IWMSelect & 0b00100000)    // dico Ready sul disco 0 e no sull'1
                IWMRegR[1] |= 0x80;
              else
                IWMRegR[1] &= ~0x80;
              break; 
            case 0xf:      // REVISED, drive model (v. doc); secondo asm @sc è PRESENT/HD pd...
#ifndef MACINTOSHPLUS
              IWMRegR[1] &= ~0x80;    // dal codice pare che 0=400K e 1=800K
#else
              IWMRegR[1] |= 0x80;    // 
#endif
              break; 
            default:
              IWMRegR[1] |= 0x80;   // safety...
              break;
            }
     			i=IWMRegR[1];    // 
          macintoshSectorPtr =0;
          break;
        case 0b10:    // handshake
     			i=IWMRegR[2];    // 
          macintoshSectorPtr =0;
          break;
        case 0b11:    // mode (or data)
          if(IWMSelect & 0b00010000) {       //  data register       (if drive is on)
       			i=IWMRegW[0];    // 
            }
          else {          // Write mode register (if drive is off)
       			i=IWMRegR[3];    // 
            }
          macintoshSectorPtr =0;
          break;
        }
      break;
      
//    case 0xe0:      // DO NOT USE
    case 0xe:      // VIA, A0=0
      i=(LOWORD(t) >> 9) & 0xf;
      switch(i) {    // A9..A12
        case 0:   // ORB IRB (questo legge i latch!
/*    bit 7 = output: 0 to enable sound, 1 to disable
    bit 6 = input: 0 = video in display portion of scanline, 1 = hblank or vblank
    bit 5 = input: mouse Y2
    bit 4 = input: mouse X2
    bit 3 = input: mouse button (0 = pressed)
    bit 2 = output: 0 = enable RTC chip select
    bit 1 = output: RTC clock line
    bit 0 = output: RTC data line */
          if(!(VIA1RegW[0] & 0b00000100)) {   // RTC enable
            VIA1RegR[0] &= ~0b00000001;
            if(!(VIA1RegW[2] & 0b00000001))    // RTC r/w
              VIA1RegR[0] |= RTCdata & 0x80 ? 0b00000001 : 0;
            else
              VIA1RegR[0] |= VIA1RegW[0] & 0b00000001;
            }
          if(!(VIA1RegW[2] & 0b00001000)) {   // per OGNI bit andrebbe controllata direzione, 0=in 1=out
            if(mouseState & 0b00010000)
              VIA1RegR[0] |= 0b00001000;
            else
              VIA1RegR[0] &= ~0b00001000;
            }
          else
            VIA1RegR[0] |= VIA1RegW[0] & 0b00001000;
          
          //(qua ci sarebbero i 2 assi del mouse... X2 e Y2
//X1 and Y1, are connected to the SCC's DCDA and DCDB inputs, respectively, while the quadrature signals, X2 and Y2, go to inputs of the VIA's data register B
          VIA1RegR[0] &= ~0b00110000;
          if(mouseState & 0b00000010)
            VIA1RegR[0] |= 0b00010000;      //X2
          if(mouseState & 0b00001000)
            VIA1RegR[0] |= 0b00100000;      //Y2

          i=VIA1RegR[0];
          break;
        case 1:   // ORA/IRA (questo legge i pin!
        case 15:  // ORA (no handshake
    /*    bit 7 = SCC Wait/Request input
    bit 6 = Alternate screen buffer output
    bit 5 = Floppy disk head select output; SEL   
    bit 4 = output: overlay ROM at 0 when 1 (enabled at reset)
    bit 3 = Alternate sound buffer output
    bits 0-2 = Sound volume output */
//#warning          
          VIA1RegR[1] |= 0x80;    // FORZO SCC per disco... boh (credo sia per ev. byte riceviti da 232
          
          
          i=VIA1RegR[1];
          break;
        case 0x4:  // T1 low counter
          VIA1RegR[0xd] &= ~0x40;
        case 0x6:  // 
          i=VIA1RegR[4];
          break;
        case 0x5:  // T1 high counter
        case 0x7:  // 
          i=VIA1RegR[5];
          break;
        case 0x8:  // T2 low counter
          VIA1RegR[0xd] &= ~0x20;
          i=VIA1RegR[8];
          break;
        case 0xa:  // SR
          VIA1RegR[0xd] &= ~0x4;
          i=VIA1RegR[0xa];
          break;
        case 0xd:
          i=VIA1RegR[0xd];
//            VIA1RegR[0xd] = 0;
          break;
        case 0xe:
          i=VIA1RegR[0xe] | 0x80;
          break;
        default:
          i=VIA1RegR[i];
          break;
        }
      break;
      
    case 0xf:      // phase, auto-vector...
      switch(LOBYTE(HIWORD(t))) {
        case 0xf0:    // phase read
        case 0xf7:    // phase read
          i=0;
          break;
        case 0xf8:
          i=0;
          break;
        case 0xff:      // per IRQ/IPL
          switch((t & 0xf) >> 1) {
            case 0:
              break;
            case 1:
              KBDIRQ=0;
              VIDIRQ=0;
              VIAIRQ=0;
              TIMIRQ=0;
              RTCIRQ=0;
              break;
            case 2:
              SCCIRQ=0;
              break;
            case 3:
              break;
            case 4:
              break;
            case 5:
              break;
            case 6:
              break;
            case 7:
              break;
            }
          i=0;
          break;
        }
      break;
      
    default:
    //Typically, you tried to access hardware directly when not in supervisor mode or tried to access non-existent hardware. 
      /*BusExcep.active=1;
      BusExcep.addr=t;
      BusExcep.descr.in=1;
      BusExcep.descr.rw=1;*/
    CPUPins |= BErr;
      break;
      
    }

  
#elif MICHELEFABBRI
  
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
  else if(t==IO_BASE_ADDRESS) {        //   led
    i=LedPort[0];      // OCCHIO nel suo hw non c'è! io lo metto per test lettura/rotate
    }
  else if(t==LCD_BOARD_ADDRESS) {        //   lcd
    i=LCDPortR[0];
    }
  else if(t==LCD_BOARD_ADDRESS+1) {        //
    i=LCDPortR[1];
    }
  else {
    //Typically, you tried to access hardware directly when not in supervisor mode or tried to access non-existent hardware. 
    /*BusExcep.active=1;
    BusExcep.addr=t;
    BusExcep.descr.in=1;
    BusExcep.descr.rw=1;*/
    CPUPins |= BErr;
    // finire...
    }
  
#else
  
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
  else {
    //Typically, you tried to access hardware directly when not in supervisor mode or tried to access non-existent hardware. 
    /*BusExcep.active=1;
    BusExcep.addr=t;
    BusExcep.descr.in=1;
    BusExcep.descr.rw=1;*/
    CPUPins |= BErr;
    // finire...
    }
  
#endif
  
	return i;
	}

SWORD GetShortValue(DWORD t) {
	union WORD_BYTE pipe;

#if !defined(MC68020)
  if(t & 1) {
    AddressExcep.active=1;
    AddressExcep.addr=t;
    AddressExcep.descr.in=1;
    AddressExcep.descr.rw=1;
    return 0;
    }
#endif
#ifdef QL
	if(t < ROM_SIZE-1) {			// 
    pipe.b[0]=rom_seg[t+1];
    pipe.b[1]=rom_seg[t+0];
		}
#ifdef ROM_SIZE2
	else if(t < ROM_SIZE2-1) {			// cartridge
		t-=ROM_SIZE;
    pipe.b[0]=rom_seg[t+1];
    pipe.b[1]=rom_seg[t+0];
		}
#endif
	else if(t >= RAM_START && t < (RAM_START+RAM_SIZE-1)) {
		t-=RAM_START;
    pipe.b[0]=ram_seg[t+1];
    pipe.b[1]=ram_seg[t+0];
		}
  else if(t>=IO_BASE_ADDRESS && t<IO_BASE_ADDRESS+0x100) {        //   I/O
    }
  else {
    //Typically, you tried to access hardware directly when not in supervisor mode or tried to access non-existent hardware. 
    /*BusExcep.active=1;
    BusExcep.addr=t;
    BusExcep.descr.in=1;
    BusExcep.descr.rw=1;*/
    CPUPins |= BErr;
    // finire...
    }
#elif MACINTOSH
  switch(LOBYTE(HIWORD(t)) >> 4) {
    case 0:
    case 1:
    case 2:
    case 3:     // 
      if(VIA1RegW[1] & 0b00010000) {    // OVERLAY, PA<4>
// superfluo      	if(t < ROM_SIZE) {			
        if(!(t & 0x100000)) {
          pipe.b[1]=rom_seg[t];
          pipe.b[0]=rom_seg[t+1];
          }
//          }
        }
      else {
    		pipe.b[1]=ram_seg[t & (RAM_SIZE-1)];
    		pipe.b[0]=ram_seg[(t+1) & (RAM_SIZE-1)];
        }
      break;
      
/*    case 3:        //   IPC Rd   FINIRE QUA
//      i=IPCW;     // mah dice qua solo WR..  scrive 0x0e, pare
      // (tastiera ecc va qua...)  scrive 4 o 8 bit, data 1=$0E or 0=$0C
//				IPCCnt++;
      break;*/
      // 0x0d è IPC per seriale_baud_rate, 0x01 è report input status, 6=read ser1, 7=read ser2, 8=read keyboard

    case 4:
      pipe.b[1]=rom_seg[t & (ROM_SIZE-1)];
      pipe.b[0]=rom_seg[(t+1) & (ROM_SIZE-1)];
      break;
      
    case 5:     // nulla, dice
      break;
      
    case 6:
      if(VIA1RegW[1] & 0b00010000) {    // OVERLAY, PA<4>
// superfluo      	if(t < ROM_SIZE) {			
    		pipe.b[1]=ram_seg[t & (RAM_SIZE-1)];
    		pipe.b[0]=ram_seg[(t+1) & (RAM_SIZE-1)];
//          }
        }
      else {
        pipe.b[1]=rom_seg2[t & (ROM_SIZE-1)];
        pipe.b[0]=rom_seg2[(t+1) & (ROM_SIZE-1)];
        }
      break;
      
    case 7:     // nulla, dice
      break;
      
    case 8:     // nulla, dice (2 devices on bus
      break;
      
    case 0x9:      // SCC 8530 read
    case 0xa:      // DO NOT USE (2 devices on bus
//    case 0xb:      // SCC 8530 write
      //Word access to any SCC address shifts the phase of the high-frequency timing by 128 ns @sc
      break;
        
    case 0xc:      // DO NOT USE (2 devices on bus
      break;
      
    case 0xd:      // IWM
      break;
      
//    case 0xe0:      // DO NOT USE
    case 0xe:      // VIA, A0=0
      break;
      
    case 0xf:      // phase, auto-vector...
      switch(LOBYTE(HIWORD(t))) {
        case 0xf0:    // phase read
        case 0xf7:    // phase read
          pipe.b[1]=0;
          pipe.b[0]=0;
          break;
        case 0xf8:
          pipe.b[1]=0;
          pipe.b[0]=0;
          break;
        case 0xff:      // per IRQ/IPL
          pipe.b[1]=0;
          pipe.b[0]=0;
          break;
        }
      break;
      
    default:
    //Typically, you tried to access hardware directly when not in supervisor mode or tried to access non-existent hardware. 
      /*BusExcep.active=1;
      BusExcep.addr=t;
      BusExcep.descr.in=1;
      BusExcep.descr.rw=1;*/
      CPUPins |= BErr;
      break;
    }
#elif MICHELEFABBRI
	if(t < ROM_SIZE-1) {			// 
    pipe.b[0]=rom_seg[t+1];
    pipe.b[1]=rom_seg[t+0];
		}
#ifdef ROM_SIZE2
	else if(t < ROM_SIZE2-1) {			// cartridge
		t-=ROM_SIZE;
    pipe.b[0]=rom_seg[t+1];
    pipe.b[1]=rom_seg[t+0];
		}
#endif
	else if(t >= RAM_START && t < (RAM_START+RAM_SIZE-1)) {
		t-=RAM_START;
    pipe.b[0]=ram_seg[t+1];
    pipe.b[1]=ram_seg[t+0];
		}
  else {
    //Typically, you tried to access hardware directly when not in supervisor mode or tried to access non-existent hardware. 
    /*BusExcep.active=1;
    BusExcep.addr=t;
    BusExcep.descr.in=1;
    BusExcep.descr.rw=1;*/
    CPUPins |= BErr;
    // finire...
    }
#endif
  
	return pipe.w;
	}

DWORD GetIntValue(DWORD t) {
	union DWORD_BYTE pipe;

#if !defined(MC68020)
  if(t & 1) {
    AddressExcep.active=1;
    AddressExcep.addr=t;
    AddressExcep.descr.in=1;
    AddressExcep.descr.rw=1;
    return 0;
    }
#endif
#ifdef QL
	if(t < ROM_SIZE-3) {			// 
    pipe.b[0]=rom_seg[t+3];
    pipe.b[1]=rom_seg[t+2];
    pipe.b[2]=rom_seg[t+1];
    pipe.b[3]=rom_seg[t+0];
		}
#ifdef ROM_SIZE2
	else if(t < ROM_SIZE2-3) {			// cartridge
		t-=ROM_SIZE;
    pipe.b[0]=rom_seg[t+3];
    pipe.b[1]=rom_seg[t+2];
    pipe.b[2]=rom_seg[t+1];
    pipe.b[3]=rom_seg[t+0];
		}
#endif
  else if(t>=IO_BASE_ADDRESS && t<IO_BASE_ADDRESS+0x100) {       // I/O
    switch(t & 0x000ff) {
      case 0:
        pipe.b[0]=RTC.b[3];
        pipe.b[1]=RTC.b[2];
        pipe.b[2]=RTC.b[1];
        pipe.b[3]=RTC.b[0];
        break;
      }
    }
	else if(t >= RAM_START && t < (RAM_START+RAM_SIZE-3)) {
		t-=RAM_START;
    pipe.b[0]=ram_seg[t+3];
    pipe.b[1]=ram_seg[t+2];
    pipe.b[2]=ram_seg[t+1];
    pipe.b[3]=ram_seg[t+0];
    }
  else {
    //Typically, you tried to access hardware directly when not in supervisor mode or tried to access non-existent hardware. 
    /*BusExcep.active=1;
    BusExcep.addr=t;
    BusExcep.descr.in=0;
    BusExcep.descr.rw=1;*/
    CPUPins |= BErr;
    // finire...
    }
#elif MACINTOSH
  switch(LOBYTE(HIWORD(t)) >> 4) {
    case 0:
    case 1:
    case 2:
    case 3:     // 
      if(VIA1RegW[1] & 0b00010000) {    // OVERLAY, PA<4>
// superfluo      	if(t < ROM_SIZE) {			
        if(!(t & 0x100000)) {
          pipe.b[3]=rom_seg[t];
          pipe.b[2]=rom_seg[t+1];
          pipe.b[1]=rom_seg[t+2];
          pipe.b[0]=rom_seg[t+3];
          }
//          }
        }
      else {
    		pipe.b[3]=ram_seg[t & (RAM_SIZE-1)];
    		pipe.b[2]=ram_seg[(t+1) & (RAM_SIZE-1)];
    		pipe.b[1]=ram_seg[(t+2) & (RAM_SIZE-1)];
    		pipe.b[0]=ram_seg[(t+3) & (RAM_SIZE-1)];
        }
      break;
      
/*    case 3:        //   IPC Rd   FINIRE QUA
//      i=IPCW;     // mah dice qua solo WR..  scrive 0x0e, pare
      // (tastiera ecc va qua...)  scrive 4 o 8 bit, data 1=$0E or 0=$0C
//				IPCCnt++;
      break;*/
      // 0x0d è IPC per seriale_baud_rate, 0x01 è report input status, 6=read ser1, 7=read ser2, 8=read keyboard

    case 4:
      pipe.b[3]=rom_seg[t & (ROM_SIZE-1)];
      pipe.b[2]=rom_seg[(t+1) & (ROM_SIZE-1)];
      pipe.b[1]=rom_seg[(t+2) & (ROM_SIZE-1)];
      pipe.b[0]=rom_seg[(t+3) & (ROM_SIZE-1)];
      break;
      
    case 5:     // nulla, dice
      break;
      
    case 6:
      if(VIA1RegW[1] & 0b00010000) {    // OVERLAY, PA<4>
// superfluo      	if(t < ROM_SIZE) {			
    		pipe.b[3]=ram_seg[t & (RAM_SIZE-1)];
    		pipe.b[2]=ram_seg[(t+1) & (RAM_SIZE-1)];
    		pipe.b[1]=ram_seg[(t+2) & (RAM_SIZE-1)];
    		pipe.b[0]=ram_seg[(t+3) & (RAM_SIZE-1)];
//          }
        }
      else {
        pipe.b[3]=rom_seg2[t & (ROM_SIZE-1)];
        pipe.b[2]=rom_seg2[(t+1) & (ROM_SIZE-1)];
        pipe.b[1]=rom_seg2[(t+2) & (ROM_SIZE-1)];
        pipe.b[0]=rom_seg2[(t+3) & (ROM_SIZE-1)];
        }
      break;
      
    case 7:     // nulla, dice
      break;
      
    case 8:     // nulla, dice (2 devices on bus
      break;
      
    case 0x9:      // SCC 8530 read
    case 0xa:      // DO NOT USE (2 devices on bus
//    case 0xb:      // SCC 8530 write
      break;
        
    case 0xc:      // DO NOT USE (2 devices on bus
      break;
      
    case 0xd:      // IWM
      break;
      
//    case 0xe0:      // DO NOT USE
    case 0xe:      // VIA, A0=0
      break;
      
    case 0xf:      // phase, auto-vector...
      switch(LOBYTE(HIWORD(t))) {
        case 0xf0:    // phase read
        case 0xf7:    // phase read
          pipe.d=0;
          break;
        case 0xf8:
          if(LOWORD(t) == 0x0000) {  // signature per test software... 55AAAA55
            pipe.d=0; //0x55aaaa55;
            }
          else {
            pipe.d=0;
            }
          break;
        case 0xff:      // per IRQ/IPL
          switch((t & 0xf) >> 1) {
            case 0:
              break;
            case 1:
              KBDIRQ=0;
              VIDIRQ=0;
              VIAIRQ=0;
              TIMIRQ=0;
              RTCIRQ=0;
              break;
            case 2:
              SCCIRQ=0;
              break;
            case 3:
              break;
            case 4:
              break;
            case 5:
              break;
            case 6:
              break;
            case 7:
              break;
            }
          pipe.d=0;
          break;
        }
      break;
      
    default:
    //Typically, you tried to access hardware directly when not in supervisor mode or tried to access non-existent hardware. 
      /*BusExcep.active=1;
      BusExcep.addr=t;
      BusExcep.descr.in=1;
      BusExcep.descr.rw=1;*/
      CPUPins |= BErr;
      break;
    }
#elif MICHELEFABBRI
	else if(t >= RAM_START && t < (RAM_START+RAM_SIZE-3)) {
		t-=RAM_START;
    pipe.b[0]=ram_seg[t+3];
    pipe.b[1]=ram_seg[t+2];
    pipe.b[2]=ram_seg[t+1];
    pipe.b[3]=ram_seg[t+0];
		}
  else {
    //Typically, you tried to access hardware directly when not in supervisor mode or tried to access non-existent hardware. 
    /*BusExcep.active=1;
    BusExcep.addr=t;
    BusExcep.descr.in=1;
    BusExcep.descr.rw=1;*/
    CPUPins |= BErr;
    // finire...
    }
#endif
  
	return pipe.d;
	}

union WORD_BYTE GetPipe(DWORD t) {
  union WORD_BYTE p;

  if(t & 1) {     // qua anche 68020, dice
    AddressExcep.active=1;
    AddressExcep.addr=t;
    AddressExcep.descr.in=0;
    AddressExcep.descr.rw=1;
    return;
    }
#if QL
	if(t < ROM_SIZE-3) {			// 
    if(!(t & 3))
#warning se indirizzo è long aligned, si potrebbero leggere 2 SWORD in un colpo solo VERIFICARE test e velocità
      ;
		p.b[0]=rom_seg[t+1];
    p.b[1]=rom_seg[t];
		Pipe2.bd[0]=rom_seg[t+5];
    Pipe2.bd[1]=rom_seg[t+4];
		Pipe2.bd[2]=rom_seg[t+3];
    Pipe2.bd[3]=rom_seg[t+2];
//		Pipe2.ww.l=MAKEWORD(rom_seg[t+1],rom_seg[t]);
		}
	else if(t >= RAM_START && t < (RAM_START+RAM_SIZE-3)) {
    if(!(t & 3))
      ;
		t-=RAM_START;
		p.b[0]=ram_seg[t+1];
    p.b[1]=ram_seg[t];
		Pipe2.bd[0]=ram_seg[t+5];
    Pipe2.bd[1]=ram_seg[t+4];
		Pipe2.bd[2]=ram_seg[t+3];
    Pipe2.bd[3]=ram_seg[t+2];
		}
  else {
    //Typically, you tried to access hardware directly when not in supervisor mode or tried to access non-existent hardware. 
    /*BusExcep.active=1;
    BusExcep.addr=t;
    BusExcep.descr.in=0;
    BusExcep.descr.rw=1;*/
    CPUPins |= BErr;
    // finire...
    }
#elif MACINTOSH
  switch(LOBYTE(HIWORD(t)) >> 4) {
    case 0:
    case 1:
    case 2:
    case 3:     // 
      if(VIA1RegW[1] & 0b00010000) {    // OVERLAY, PA<4>
// superfluo      	if(t < ROM_SIZE) {			
        if(!(t & 0x100000)) {
          p.b[0]=rom_seg[t+1];
          p.b[1]=rom_seg[t];
          Pipe2.bd[0]=rom_seg[t+5];
          Pipe2.bd[1]=rom_seg[t+4];
          Pipe2.bd[2]=rom_seg[t+3];
          Pipe2.bd[3]=rom_seg[t+2];
          }
//          }
        }
      else {
        p.b[0]=ram_seg[(t+1) & (RAM_SIZE-1)];
        p.b[1]=ram_seg[t & (RAM_SIZE-1)];
        Pipe2.bd[0]=ram_seg[(t+5) & (RAM_SIZE-1)];
        Pipe2.bd[1]=ram_seg[(t+4) & (RAM_SIZE-1)];
        Pipe2.bd[2]=ram_seg[(t+3) & (RAM_SIZE-1)];
        Pipe2.bd[3]=ram_seg[(t+2) & (RAM_SIZE-1)];
        }
      break;
      
/*    case 3:        //   IPC Rd   FINIRE QUA
//      i=IPCW;     // mah dice qua solo WR..  scrive 0x0e, pare
      // (tastiera ecc va qua...)  scrive 4 o 8 bit, data 1=$0E or 0=$0C
//				IPCCnt++;
      break;*/
      // 0x0d è IPC per seriale_baud_rate, 0x01 è report input status, 6=read ser1, 7=read ser2, 8=read keyboard

    case 4:
      p.b[0]=rom_seg[(t+1) & (ROM_SIZE-1)];
      p.b[1]=rom_seg[t & (ROM_SIZE-1)];
      Pipe2.bd[0]=rom_seg[(t+5) & (ROM_SIZE-1)];
      Pipe2.bd[1]=rom_seg[(t+4) & (ROM_SIZE-1)];
      Pipe2.bd[2]=rom_seg[(t+3) & (ROM_SIZE-1)];
      Pipe2.bd[3]=rom_seg[(t+2) & (ROM_SIZE-1)];
      break;
      
    case 5:     // nulla, dice
      break;
      
    case 6:
      if(VIA1RegW[1] & 0b00010000) {    // OVERLAY, PA<4>
// superfluo      	if(t < ROM_SIZE) {			
        p.b[0]=ram_seg[(t+1) & (RAM_SIZE-1)];
        p.b[1]=ram_seg[t & (RAM_SIZE-1)];
        Pipe2.bd[0]=ram_seg[(t+5) & (RAM_SIZE-1)];
        Pipe2.bd[1]=ram_seg[(t+4) & (RAM_SIZE-1)];
        Pipe2.bd[2]=ram_seg[(t+3) & (RAM_SIZE-1)];
        Pipe2.bd[3]=ram_seg[(t+2) & (RAM_SIZE-1)];
//          }
        }
      else {
        p.b[0]=rom_seg[(t+1) & (ROM_SIZE-1)];
        p.b[1]=rom_seg[t & (ROM_SIZE-1)];
        Pipe2.bd[0]=rom_seg[(t+5) & (ROM_SIZE-1)];
        Pipe2.bd[1]=rom_seg[(t+4) & (ROM_SIZE-1)];
        Pipe2.bd[2]=rom_seg[(t+3) & (ROM_SIZE-1)];
        Pipe2.bd[3]=rom_seg[(t+2) & (ROM_SIZE-1)];
        }
      break;
      
    case 7:     // nulla, dice
      break;
      
    case 8:     // nulla, dice (2 devices on bus
      break;
      
    case 0x9:      // SCC 8530 read
    case 0xa:      // DO NOT USE (2 devices on bus
//    case 0xb:      // SCC 8530 write
      break;
        
    case 0xc:      // DO NOT USE (2 devices on bus
      break;
      
    case 0xd:      // IWM
      break;
      
//    case 0xe0:      // DO NOT USE
    case 0xe:      // VIA, A0=0
      break;
      
    case 0xf:      // phase, auto-vector...
      break;
      
    default:
    //Typically, you tried to access hardware directly when not in supervisor mode or tried to access non-existent hardware. 
      /*BusExcep.active=1;
      BusExcep.addr=t;
      BusExcep.descr.in=1;
      BusExcep.descr.rw=1;*/
      CPUPins |= BErr;
      break;
    }
#elif MICHELEFABBRI
	if(t < ROM_SIZE-3) {			// 
    if(!(t & 3))
#warning se indirizzo è long aligned, si potrebbero leggere 2 SWORD in un colpo solo VERIFICARE test e velocità
      ;
		p.b[0]=rom_seg[t+1];
    p.b[1]=rom_seg[t];
		Pipe2.bd[0]=rom_seg[t+5];
    Pipe2.bd[1]=rom_seg[t+4];
		Pipe2.bd[2]=rom_seg[t+3];
    Pipe2.bd[3]=rom_seg[t+2];
//		Pipe2.ww.l=MAKEWORD(rom_seg[t+1],rom_seg[t]);
		}
	else if(t >= RAM_START && t < (RAM_START+RAM_SIZE-3)) {
    if(!(t & 3))
      ;
		t-=RAM_START;
		p.b[0]=ram_seg[t+1];
    p.b[1]=ram_seg[t];
		Pipe2.bd[0]=ram_seg[t+5];
    Pipe2.bd[1]=ram_seg[t+4];
		Pipe2.bd[2]=ram_seg[t+3];
    Pipe2.bd[3]=ram_seg[t+2];
		}
  else {
    //Typically, you tried to access hardware directly when not in supervisor mode or tried to access non-existent hardware. 
    /*BusExcep.active=1;
    BusExcep.addr=t;
    BusExcep.descr.in=0;
    BusExcep.descr.rw=1;*/
    CPUPins |= BErr;
    // finire...
    }
#endif
  
	return p;
	}

BYTE AdvPipe(DWORD t,BYTE n) {    // questa va usata quando il primo parm dell'istruzione è esplicito! NON per ils econdo (verificare)
  
  if(t & 1) {   // qua anche 68020, dice
    AddressExcep.active=1;
    AddressExcep.addr=t;
    AddressExcep.descr.in=0;
    AddressExcep.descr.rw=1;
    return;
    }
#if QL
  switch(n) {
    case 2:
  		Pipe2.wd[1]=Pipe2.wd[0];
      if(t < ROM_SIZE-5) {			// 
        Pipe2.bd[0]=rom_seg[t+5];
        Pipe2.bd[1]=rom_seg[t+4];
        }
      else if(t >= RAM_START && t < (RAM_START+RAM_SIZE-5)) {
    		t-=RAM_START;
        Pipe2.bd[0]=ram_seg[t+5];
        Pipe2.bd[1]=ram_seg[t+4];
        }
      else {
    //Typically, you tried to access hardware directly when not in supervisor mode or tried to access non-existent hardware. 
        /*BusExcep.active=1;
        BusExcep.addr=t;
        BusExcep.descr.in=0;
        BusExcep.descr.rw=1;*/
    CPUPins |= BErr;
        // finire...
        }
      break;
    case 4:
      if(t < ROM_SIZE-7) {			// 
        if(!(t & 3))
    #warning se indirizzo è long aligned, si potrebbero leggere 2 SWORD in un colpo solo VERIFICARE test e velocità
          ;
        Pipe2.bd[0]=rom_seg[t+7];
        Pipe2.bd[1]=rom_seg[t+6];
        Pipe2.bd[2]=rom_seg[t+5];
        Pipe2.bd[3]=rom_seg[t+4];
        }
      else if(t >= RAM_START && t < (RAM_START+RAM_SIZE-7)) {
        if(!(t & 3))
          ;
    		t-=RAM_START;
        Pipe2.bd[0]=ram_seg[t+7];
        Pipe2.bd[1]=ram_seg[t+6];
        Pipe2.bd[2]=ram_seg[t+5];
        Pipe2.bd[3]=ram_seg[t+4];
        }
      else {
    //Typically, you tried to access hardware directly when not in supervisor mode or tried to access non-existent hardware. 
        /*BusExcep.active=1;
        BusExcep.addr=t;
        BusExcep.descr.in=0;
        BusExcep.descr.rw=1;*/
    CPUPins |= BErr;
        // finire...
        }
      break;
    }
#elif MACINTOSH
  switch(LOBYTE(HIWORD(t)) >> 4) {
    case 0:
    case 1:
    case 2:
    case 3:     // 
      if(VIA1RegW[1] & 0b00010000) {    // OVERLAY, PA<4>
        if(!(t & 0x100000)) {
          switch(n) {
            case 2:
              Pipe2.wd[1]=Pipe2.wd[0];
  //            if(t < ROM_SIZE-5) {			// 
                Pipe2.bd[0]=rom_seg[t+5];
                Pipe2.bd[1]=rom_seg[t+4];
  //              }
              break;
            case 4:
  //            if(t < ROM_SIZE-7) {			// 
                if(!(t & 3))
            #warning se indirizzo è long aligned, si potrebbero leggere 2 SWORD in un colpo solo VERIFICARE test e velocità
                  ;
                Pipe2.bd[0]=rom_seg[t+7];
                Pipe2.bd[1]=rom_seg[t+6];
                Pipe2.bd[2]=rom_seg[t+5];
                Pipe2.bd[3]=rom_seg[t+4];
              break;
            }
          }
        }
      else {
        switch(n) {
          case 2:
            Pipe2.wd[1]=Pipe2.wd[0];
//            if(t < ROM_SIZE-5) {			// 
              Pipe2.bd[0]=ram_seg[(t+5) & (RAM_SIZE-1)];
              Pipe2.bd[1]=ram_seg[(t+4) & (RAM_SIZE-1)];
//              }
            break;
          case 4:
//            if(t < ROM_SIZE-7) {			// 
              if(!(t & 3))
          #warning se indirizzo è long aligned, si potrebbero leggere 2 SWORD in un colpo solo VERIFICARE test e velocità
                ;
              Pipe2.bd[0]=ram_seg[(t+7) & (RAM_SIZE-1)];
              Pipe2.bd[1]=ram_seg[(t+6) & (RAM_SIZE-1)];
              Pipe2.bd[2]=ram_seg[(t+5) & (RAM_SIZE-1)];
              Pipe2.bd[3]=ram_seg[(t+4) & (RAM_SIZE-1)];
            break;
          }
        }
      break;
      
/*    case 3:        //   IPC Rd   FINIRE QUA
//      i=IPCW;     // mah dice qua solo WR..  scrive 0x0e, pare
      // (tastiera ecc va qua...)  scrive 4 o 8 bit, data 1=$0E or 0=$0C
//				IPCCnt++;
      break;*/
      // 0x0d è IPC per seriale_baud_rate, 0x01 è report input status, 6=read ser1, 7=read ser2, 8=read keyboard

    case 4:
        switch(n) {
          case 2:
            Pipe2.wd[1]=Pipe2.wd[0];
//            if(t < ROM_SIZE-5) {			// 
              Pipe2.bd[0]=rom_seg[(t+5) & (ROM_SIZE-1)];
              Pipe2.bd[1]=rom_seg[(t+4) & (ROM_SIZE-1)];
//              }
            break;
          case 4:
//            if(t < ROM_SIZE-7) {			// 
              if(!(t & 3))
          #warning se indirizzo è long aligned, si potrebbero leggere 2 SWORD in un colpo solo VERIFICARE test e velocità
                ;
              Pipe2.bd[0]=rom_seg[(t+7) & (ROM_SIZE-1)];
              Pipe2.bd[1]=rom_seg[(t+6) & (ROM_SIZE-1)];
              Pipe2.bd[2]=rom_seg[(t+5) & (ROM_SIZE-1)];
              Pipe2.bd[3]=rom_seg[(t+4) & (ROM_SIZE-1)];
            break;
          }
      break;
      
    case 5:     // nulla, dice
      break;
      
    case 6:
      if(VIA1RegW[1] & 0b00010000) {    // OVERLAY, PA<4>
        switch(n) {
          case 2:
            Pipe2.wd[1]=Pipe2.wd[0];
//            if(t < ROM_SIZE-5) {			// 
              Pipe2.bd[0]=ram_seg[(t+5) & (RAM_SIZE-1)];
              Pipe2.bd[1]=ram_seg[(t+4) & (RAM_SIZE-1)];
//              }
            break;
          case 4:
//            if(t < ROM_SIZE-7) {			// 
              if(!(t & 3))
          #warning se indirizzo è long aligned, si potrebbero leggere 2 SWORD in un colpo solo VERIFICARE test e velocità
                ;
              Pipe2.bd[0]=ram_seg[(t+7) & (RAM_SIZE-1)];
              Pipe2.bd[1]=ram_seg[(t+6) & (RAM_SIZE-1)];
              Pipe2.bd[2]=ram_seg[(t+5) & (RAM_SIZE-1)];
              Pipe2.bd[3]=ram_seg[(t+4) & (RAM_SIZE-1)];
            break;
          }
        }
      else {
        switch(n) {
          case 2:
            Pipe2.wd[1]=Pipe2.wd[0];
//            if(t < ROM_SIZE-5) {			// 
              Pipe2.bd[0]=rom_seg[(t+5) & (ROM_SIZE-1)];
              Pipe2.bd[1]=rom_seg[(t+4) & (ROM_SIZE-1)];
//              }
            break;
          case 4:
//            if(t < ROM_SIZE-7) {			// 
              if(!(t & 3))
          #warning se indirizzo è long aligned, si potrebbero leggere 2 SWORD in un colpo solo VERIFICARE test e velocità
                ;
              Pipe2.bd[0]=rom_seg[(t+7) & (ROM_SIZE-1)];
              Pipe2.bd[1]=rom_seg[(t+6) & (ROM_SIZE-1)];
              Pipe2.bd[2]=rom_seg[(t+5) & (ROM_SIZE-1)];
              Pipe2.bd[3]=rom_seg[(t+4) & (ROM_SIZE-1)];
            break;
          }
        }
      break;
      
    case 7:     // nulla, dice
      break;
      
    case 8:     // nulla, dice (2 devices on bus
      break;
      
    case 0x9:      // SCC 8530 read
    case 0xa:      // DO NOT USE (2 devices on bus
//    case 0xb:      // SCC 8530 write
      break;
        
    case 0xc:      // DO NOT USE (2 devices on bus
      break;
      
    case 0xd:      // IWM
      break;
      
//    case 0xe0:      // DO NOT USE
    case 0xe:      // VIA, A0=0
      break;
      
    case 0xf:      // phase, auto-vector...
      break;
      
    default:
    //Typically, you tried to access hardware directly when not in supervisor mode or tried to access non-existent hardware. 
      /*BusExcep.active=1;
      BusExcep.addr=t;
      BusExcep.descr.in=1;
      BusExcep.descr.rw=1;*/
    CPUPins |= BErr;
      break;
    }
#elif MICHELEFABBRI
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
    //Typically, you tried to access hardware directly when not in supervisor mode or tried to access non-existent hardware. 
        /*BusExcep.active=1;
        BusExcep.addr=t;
        BusExcep.descr.in=0;
        BusExcep.descr.rw=1;*/
    CPUPins |= BErr;
        // finire...
        }
      break;
    case 4:
      if(t < ROM_SIZE-7) {			// 
        if(!(t & 3))
    #warning se indirizzo è long aligned, si potrebbero leggere 2 SWORD in un colpo solo VERIFICARE test e velocità
          ;
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
    //Typically, you tried to access hardware directly when not in supervisor mode or tried to access non-existent hardware. 
        /*BusExcep.active=1;
        BusExcep.addr=t;
        BusExcep.descr.in=0;
        BusExcep.descr.rw=1;*/
    CPUPins |= BErr;
        // finire...
        }
      break;
    }
#endif
  
	return n;
	}

void PutValue(DWORD t,BYTE t1) {
	register SWORD i;

// printf("rom_seg: %04x, p: %04x\n",rom_seg,p);

#ifdef QL
  
	if(t >= RAM_START && t < (RAM_START+RAM_SIZE)) {		// 
	  ram_seg[t-RAM_START]=t1;
		}
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
        if(t1 & 0b00010000) 
          ;//EXTIRQ  ??
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
  else {
    //Typically, you tried to access hardware directly when not in supervisor mode or tried to access non-existent hardware. 
    /*BusExcep.active=1;
    BusExcep.addr=t;
    BusExcep.descr.in=1;
    BusExcep.descr.rw=0;*/
    CPUPins |= BErr;
    // finire...
    }
  
#elif MACINTOSH
  //https://bitsavers.org/pdf/apple/mac/
 
  switch(LOBYTE(HIWORD(t)) >> 4) {
    case 0:
    case 1:
    case 2:
    case 3:     // 
      if(VIA1RegW[1] & 0b00010000) {    // OVERLAY, PA<4>
        // exception!
        }
      else {
    		ram_seg[t & (RAM_SIZE-1)]=t1;
        }
      break;
      
    case 4:

      break;
      
    case 5:     // nulla, dice
      break;
      
    case 6:
      if(VIA1RegW[1] & 0b00010000) {    // OVERLAY, PA<4>
// superfluo      	if(t < ROM_SIZE) {			
    		ram_seg[t & (RAM_SIZE-1)]=t1;
//          }
        }
      else {
        // exception
        }
      break;
      
    case 7:     // nulla, dice
      break;
      
    case 8:     // nulla, dice (2 devices on bus
      break;
      
//    case 0x9:      // SCC 8530 read
    case 0xa:      // DO NOT USE
    case 0xb:      // SCC 8530 write
      t &= 7;
      switch(t) {    // NON SI CAPISCE se ci sono 2 set di control reg A e B, e dov e il registro 0...
        case 0x0:   //  read from control register channel B specified by control register 0
          break;
        case 0x1:   //  write to control register channel B specified by control register 0
//          z8530RegW[0]=0;   //seleziona registro pare v.$1A92
          
          //boh
//    			z8530RegW[z8530RegW[0] & 0xf]=t1;
    			z8530RegW[0]=t1;
          break;
        case 0x2:   //  read from control register channel A specified by control register 0
          break;
        case 0x3:   //  write to control register channel A specified by control register 0
    			z8530RegW[z8530RegW[0] & 0xf]=t1;
          break;
        case 0x4:   //  read from data register channel B
          break;
        case 0x5:   //  write to data register channel B
    			z8530RegWDB=t1;
          break;
        case 0x6:   //  read from data register channel A; (phase adjust se word-read
          break;
        case 0x7:   //  write to data register channel A; reset SCC
    			z8530RegWDA=t1;
          break;
        }
      break;
      
    case 0xc:      // DO NOT USE (2 devices on bus
      break;
      
    case 0xd:      // IWM; v. https://github.com/evansm7/umac per paravirt emulation
/*      
Select bits:
	  7   6   5   4   3     2   1   0  
    Q7  Q6  SEL EN  LSTRB CA2 CA1 CA0

The bits of the mode register are laid out as follows:
	  7   6    5     4   3   2   1   0  
	+---+-----+-----+---+---+---+---+---+
	| R | Rst | Tst | S | C | M | H | L |
	+---+-----+-----+---+---+---+---+---+
With the various bit meanings described below:
	Bit	Function
	---	--------
	 R	Reserved
	 S	Clock speed:
			0 = 7MHz
			1 = 8MHz
		Should always be 0.
	 C	Bit cell time:
			0 = 4usec/bit (for 5.25 drives)
			1 = 2usec/bit (for 3.5 drives)
	 M	Motor-off timer:
			0 = leave drive on for 1 sec after program turns it off
			1 = no delay
		Should be 0 for 5.25 and 1 for 3.5.
	 H	Handshake protocol:
			0 = synchronous (software must supply proper timing for writing data)
			1 = asynchronous (IWM supplies timing)
		Should be 0 for 5.25 and 1 for 3.5.
	 L	Latch mode:
			0 = read-data stays valid for about 7usec
			1 = read-data stays valid for full byte time
		Should be 0 for 5.25 and 1 for 3.5.
 
The bits of the status register are laid out as follows:
	  7   6   5   4   3   2   1   0  
	+---+---+---+---+---+---+---+---+
	| I | R | E | S | C | M | H | L |
	+---+---+---+---+---+---+---+---+
	Bit	Function
	---	--------
	 I	Sense input.
			write-protect indicator (5.25-inch drive)
			general status line	(3.5-inch drive)
	 R	Reserved.
	 E	Drive enabled
			0 = no disk drive is on
			1 = a disk drive is on.
	 S	Same as S bit in the mode register.
	 C	Same as C bit in the mode register.
	 M	Same as M bit in the mode register.
	 H	Same as H bit in the mode register.
	 L	Same as L bit in the mode register.
      
The bits of the handshake register are laid out as follows:
	  7   6   5   4   3   2   1   0  
	+---+---+---+---+---+---+---+---+
	| B | U | R | R | R | R | R | R |
	+---+---+---+---+---+---+---+---+
	Bit	Function
	---	--------
	 B	Register Ready
			0 = IWM is busy
			1 = IWM is ready for data
	 U	Under-run
			0 = write under-run has occurred (the program took too long to write the next byte)
			1 = no under-run
	 R	Reserved.      
      */
      { uint8_t oldIwm=IWMSelect;
      i=(LOWORD(t) >> 10) & 0x7;
      if(t & 0x200)   // A9
        IWMSelect |= (1 << i);    // A10..A12; A9 è usato come set/reset...
      else
        IWMSelect &= ~(1 << i);
      i=IWMSelect >> 6;
      if(IWMRegW[3] & 0b00100000)   // se test mode, operation unspecified a parte scrivere mode e leggere status <5>
        ;
      switch(i) {
        case 0b00:    // data
        case 0b10:    // handshake
     			IWMRegW[i]=t1;    // 
          break;
        case 0b01:    // status
          if(!(IWMSelect & 0b00010000))        //  dev'essere off per status
       			IWMRegW[i]=t1;    // 
          macintoshSectorPtr =0;
          break;
        case 0b11:    // mode (or data)
          if(IWMSelect & 0b00010000) {          // data register       (if drive is on)
            /*IWMRegR[0]=*/IWMRegW[0]=t1;
                           
            gcr_decodebyte(IWMRegW[0]);// e salvare
            
            }
          else {          // Write mode register (if drive is off)
       			IWMRegW[3]=t1;    // 
            
       			IWMRegR[1]= (IWMRegR[1] & 0b11100000) | (IWMRegW[3] & 0b00011111);    // sarebbe DOPO aver effettuato operazione richiesta :)
            }
          macintoshSectorPtr =0;
          break;
        }
// serve qua? sembra sempre solo in read, ossia con TST.B      
      if((oldIwm & 0b00001000) && !(IWMSelect & 0b00001000)) {  // se LSTRB scende...
        // control function  Be sure that and that CA0 and CA1 are set high before changing SEL
        //  nb CA2 = 4 = valore parm!
        i=((VIA1RegW[1] & 0x20) >> 2) | (IWMSelect & 7);
#if 0
        switch(i) {   // solo i floppy 3.5, dice..
          case 0:   // DIRTN Set step direction inward (toward higher-numbered tracks.)
            floppyState |= 0x1;
            break;
          case 0x4/*1*/:   // Set step direction outward (toward lower-numbered tracks.
            floppyState &= ~0x1;
            break;
          case 0xc/*3*/:   // Reset disk-switched flag?  (The firmware uses this to clear disk-switched errors.)
            floppyState &= ~0x2;
            break;
          case 1/*4*/:   // STEP Step one track in current direction (takes about 12 msec).
            if(floppyState & 0x1)
              floppyTrack++;
            else
              floppyTrack--;
            floppySector=0;
            encodeMacSector();
            macintoshSectorPtr=0;
            break;
          case 2/*8*/:   // MOTORON Turn spindle motor on.
            floppyState |= 0x4;
            gcr_init();   // verificare dove mettere
            break;
          case 6/*9*/:   // Turn spindle motor off.
            floppyState &= ~0x4;
            gcr_init();   // verificare dove mettere
            break;
          case 7/*0x0d*/:  // EJECT Eject the disk. This takes about 1/2 sec to complete. The drive may not recognize further control commands until this operation is complete.
            floppyState &= ~0x80;
            break;
          default:
            break;
          }
#endif
        }
      }
      break;
      
    case 0xe:      // VIA, A0=0
      i=(LOWORD(t) >> 9) & 0xf;
      switch(i) {    // A9..A12
        case 0:   // ORB (in lettura questo legge i latch!
/*  bit 7 = output: 0 to enable sound, 1 to disable
    bit 6 = input: 0 = video in display portion of scanline, 1 = hblank or vblank
    bit 5 = input: mouse Y2
    bit 4 = input: mouse X2
    bit 3 = input: mouse button (0 = pressed)
    bit 2 = output: 0 = enable RTC chip select
    bit 1 = output: RTC clock line
    bit 0 = output: RTC data line */
          if(!(VIA1RegW[0] & 0b00000100)) {   // RTC enable
            if(!(VIA1RegW[0] & 0b00000010) && (t1 & 0b00000010)) {   // quando clock sale...
              // il primo byte è RTCselect! fare
              RTCstate;
                
              if(!(VIA1RegW[2] & 0b00000001)) {   // RTC r/w
                RTCdata <<= 1;
                }
              else {
                RTCdata <<= 1;
                RTCdata |= VIA1RegW[0] & 1;
                }
              RTCcount++;
              if(RTCcount==8) {
                // salvare/aggiornare
                RTCstate;
                
                if(RTCselect & 0b10000000 /*!(VIA1RegW[2] & 0b00000001)*/) {   // RTC r/w
                  switch(RTCselect & 0x7f) {
                    case 0b0001:
                      RTCdata=RTC.b[0];
                      break;
                    case 0b0101:
                      RTCdata=RTC.b[1];
                      break;
                    case 0b1001:
                      RTCdata=RTC.b[2];
                      break;
                    case 0b1101:
                      RTCdata=RTC.b[3];
                      break;
                    case 0b0100001:
                    case 0b0100101:
                    case 0b0101001:
                    case 0b0101101:
                      RTCdata=PRam[0x10+((RTCselect & 0x0c) >> 2)];
                      break;
                    case 0b01000001:
                    case 0b01000101:
                    case 0b01001001:
                    case 0b01001101:
                    case 0b01010001:
                    case 0b01010101:
                    case 0b01011001:
                    case 0b01011101:
                    case 0b01100001:
                    case 0b01100101:
                    case 0b01101001:
                    case 0b01101101:
                    case 0b01110001:
                    case 0b01110101:
                    case 0b01111001:
                    case 0b01111101:
                      RTCdata=PRam[(RTCselect & 0x3c) >> 2];
                      break;
                    }
                  }
                else {
                  switch(RTCselect & 0x7f) {
                    case 0b0001:
                      RTC.b[0]=RTCdata;
                      break;
                    case 0b0101:
                      RTC.b[1]=RTCdata;
                      break;
                    case 0b1001:
                      RTC.b[2]=RTCdata;
                      break;
                    case 0b1101:
                      RTC.b[3]=RTCdata;
                      break;
                    case 0b00110001:    // test
                      break;
                    case 0b00110101:    // write-protect
                      RTCwp=RTCdata;
                      break;
                    case 0b0100001:
                    case 0b0100101:
                    case 0b0101001:
                    case 0b0101101:
                      PRam[0x10+((RTCselect & 0x0c) >> 2)]=RTCdata;
                      break;
                    case 0b01000001:
                    case 0b01000101:
                    case 0b01001001:
                    case 0b01001101:
                    case 0b01010001:
                    case 0b01010101:
                    case 0b01011001:
                    case 0b01011101:
                    case 0b01100001:
                    case 0b01100101:
                    case 0b01101001:
                    case 0b01101101:
                    case 0b01110001:
                    case 0b01110101:
                    case 0b01111001:
                    case 0b01111101:
                      PRam[(RTCselect & 0x3c) >> 2]=RTCdata;
                      break;
                    }
                  }
                
                RTCcount=0;
                }
              }
            }
          else {
            RTCselect=0;
            }
          if(t1 /*VIA1RegW[0]*/ & 0b10000000) {    // sound
#ifdef ST7735
            OC1CONbits.ON = 0;   // off
            PR2=400;
            OC1RS = PR2/2;		 // 
#endif            
#ifdef ILI9341
            OC7CONbits.ON = 0;   // off
            PR2=400;
            OC7RS = PR2/2;		 // 
#endif            
            }
          else {
#ifdef ST7735
            PR2=400;    // 3906Hz 13/12/24
            OC1RS = PR2/2;		 // 
            OC1CONbits.ON = 1;   // on
#endif            
#ifdef ILI9341
            PR2=400;    // 3906Hz 13/12/24
            OC7RS = PR2/2;		 // 
            OC7CONbits.ON = 1;   // on
#endif            
            }
    			VIA1RegR[0]=VIA1RegW[0]=t1;
          break;
        case 1:   // ORA (in lettura questo legge i pin!
        case 15:  // ORA (no handshake
    /*    bit 7 = SCC Wait/Request input
    bit 6 = Alternate screen buffer output
    bit 5 = Floppy disk head select output SEL Two purposes: selects the drive head to read/write data from either side of the disk (in double-sided drives) and plays part in drive register selection.
    bit 4 = output: overlay ROM at 0 when 1 (enabled at reset)
    bit 3 = Alternate sound buffer output
    bits 0-2 = Sound volume output */
    			VIA1RegR[1]= /*NON TOGLIERE cmq!*/   VIA1RegW[1]=t1;
          if(VIA1RegW[1] & 0b00010000) {    // OVERLAY, PA<4>
            VideoAddress[0]=0x61a700;
            VideoAddress[1]=0x612700;
            }
          else {
            VideoAddress[0]=0x01a700;
            VideoAddress[1]=0x012700;
            }
/*          if(VIA1RegW[1] & 0b00100000)     // disk head, PA<5>  usato per leggere status v.   Two purposes: selects the drive head to read/write data from either side of the disk (in double-sided drives) and plays part in drive register selection.
            floppyState |= 0x8;
          else
            floppyState &= ~0x8;*/
          break;
        case 0x04:  // T1 low latch
        case 0x06:  // idem
    			VIA1RegW[4]=t1;
          break;
        case 0x07:  // T1 high latch
          VIA1RegR[0xd] &= ~0x40;
          VIA1IRQSet |= 0x40;
    			VIA1RegR[4]=VIA1RegW[4];
        case 0x05:  // T1 high counter/latch
    			VIA1RegR[5]=VIA1RegW[5]=t1;
          break;
        case 0x08:  // T2 low latch
    			VIA1RegW[8]=t1;
          break;
        case 0x09:  // T2 high latch/counter
          VIA1RegR[0xd] &= ~0x20;
          VIA1IRQSet |= 0x20;
    			VIA1RegR[9]=VIA1RegW[9]=t1;
    			VIA1RegR[8]=VIA1RegW[8];
          break;
        case 0x0a:  // SR
          VIA1RegR[0xd] &= ~0x4;
    			VIA1RegW[0xa]=t1;
          switch(VIA1RegW[0xa]) {
            case 0x10:    // inquiry
              VIA1RegR[0xa]=Keyboard[0] ? Keyboard[0] : 0x7b;     // 
              break;
            case 0x14:    // instant
              VIA1RegR[0xa]=Keyboard[0] ? Keyboard[0] : 0x7b;     // 
              break;
            case 0x16:    // model number
              VIA1RegR[0xa]=0b00000011;      // 1; keyb model #; next dev. #; another dev
              break;
            case 0x36:    // test
              VIA1RegR[0xa]=0x7d;      // ACK; NACK=0x77
              break;
            }
          break;
        case 0xd:   // IFR
    			VIA1RegW[0xd] = t1;   // 
    			VIA1RegR[0xd] &= ~(t1 & 0x7f);   // 
          if(VIA1RegR[0xd] & 0x7f)
            VIA1RegR[0xd] |= 0x80;
          else
            VIA1RegR[0xd] &= ~0x80;
          break;
        case 0xe:   // IER
          if(t1 & 0x80)
            VIA1RegR[0xe]=VIA1RegW[0xe] |= t1 & 0x7f;   //
          else
            VIA1RegR[0xe]=VIA1RegW[0xe] &= ~(t1 & 0x7f);   //
          break;
        default:
          VIA1RegR[i]=VIA1RegW[i] = t1;
          break;
        }
      break;
      
    default:
    //Typically, you tried to access hardware directly when not in supervisor mode or tried to access non-existent hardware. 
      /*BusExcep.active=1;
      BusExcep.addr=t;
      BusExcep.descr.in=1;
      BusExcep.descr.rw=0;*/
    CPUPins |= BErr;
      // finire...
      break;
    }

#elif MICHELEFABBRI
	if(t >= RAM_START && t < (RAM_START+RAM_SIZE)) {		// 
	  ram_seg[t-RAM_START]=t1;
		}
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
  else {
    //Typically, you tried to access hardware directly when not in supervisor mode or tried to access non-existent hardware. 
    /*BusExcep.active=1;
    BusExcep.addr=t;
    BusExcep.descr.in=1;
    BusExcep.descr.rw=0;*/
    CPUPins |= BErr;
    // finire...
    }

#else
  
	if(t >= RAM_START && t < (RAM_START+RAM_SIZE)) {		// 
	  ram_seg[t-RAM_START]=t1;
		}
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
  else {
    //Typically, you tried to access hardware directly when not in supervisor mode or tried to access non-existent hardware. 
    /*BusExcep.active=1;
    BusExcep.addr=t;
    BusExcep.descr.in=1;
    BusExcep.descr.rw=0;*/
    CPUPins |= BErr;
    // finire...
    }
#endif

	}

void PutShortValue(DWORD t,SWORD t1) {
	register SWORD i;

// printf("rom_seg: %04x, p: %04x\n",rom_seg,p);

#if !defined(MC68020)
  if(t & 1) {
    AddressExcep.active=1;
    AddressExcep.addr=t;
    AddressExcep.descr.in=1;
    AddressExcep.descr.rw=0;
    return;
    }
#endif
#ifdef QL
	if(t >= RAM_START && t < (RAM_START+RAM_SIZE-1)) {		// 
		t-=RAM_START;
	  ram_seg[t++]=HIBYTE(t1);
	  ram_seg[t]=LOBYTE(t1);
		}
  else if(t>=IO_BASE_ADDRESS && t<IO_BASE_ADDRESS+0x100) {        //   
    }
  else {
    //Typically, you tried to access hardware directly when not in supervisor mode or tried to access non-existent hardware. 
    /*BusExcep.active=1;
    BusExcep.addr=t;
    BusExcep.descr.in=1;
    BusExcep.descr.rw=0;*/
    CPUPins |= BErr;
    // finire...
    }
#elif MACINTOSH
  switch(LOBYTE(HIWORD(t)) >> 4) {
    case 0:
    case 1:
    case 2:
    case 3:     // 
      if(VIA1RegW[1] & 0b00010000) {    // OVERLAY, PA<4>
        }
      else {
    	  ram_seg[t++ & (RAM_SIZE-1)]=HIBYTE(t1);
        ram_seg[t & (RAM_SIZE-1)]=LOBYTE(t1);
        }
      break;
      
/*    case 3:        //   IPC Rd   FINIRE QUA
//      i=IPCW;     // mah dice qua solo WR..  scrive 0x0e, pare
      // (tastiera ecc va qua...)  scrive 4 o 8 bit, data 1=$0E or 0=$0C
//				IPCCnt++;
      break;*/
      // 0x0d è IPC per seriale_baud_rate, 0x01 è report input status, 6=read ser1, 7=read ser2, 8=read keyboard

    case 4:
      break;
      
    case 5:     // nulla, dice
      break;
      
    case 6:
      if(VIA1RegW[1] & 0b00010000) {    // OVERLAY, PA<4>
    	  ram_seg[t++ & (RAM_SIZE-1)]=HIBYTE(t1);
        ram_seg[t & (RAM_SIZE-1)]=LOBYTE(t1);
        }
      break;
      
    case 7:     // nulla, dice
      break;
      
    case 8:     // nulla, dice (2 devices on bus
      break;
      
//    case 0x9:      // SCC 8530 read
    case 0xa:      // DO NOT USE (2 devices on bus
    case 0xb:      // SCC 8530 write
      //Word access to any SCC address shifts the phase of the high-frequency timing by 128 ns @sc
      break;
        
    case 0xc:      // DO NOT USE (2 devices on bus
      break;
      
    case 0xd:      // IWM
      break;
      
//    case 0xe0:      // DO NOT USE
    case 0xe:      // VIA, A0=0
      break;
      
    case 0xf:      // phase, auto-vector...
      break;
      
    default:
    //Typically, you tried to access hardware directly when not in supervisor mode or tried to access non-existent hardware. 
      /*BusExcep.active=1;
      BusExcep.addr=t;
      BusExcep.descr.in=1;
      BusExcep.descr.rw=1;*/
    CPUPins |= BErr;
      break;
    }
#elif MICHELEFABBRI
	if(t >= RAM_START && t < (RAM_START+RAM_SIZE-1)) {		// 
		t-=RAM_START;
	  ram_seg[t++]=HIBYTE(t1);
	  ram_seg[t]=LOBYTE(t1);
		}
  else {
    //Typically, you tried to access hardware directly when not in supervisor mode or tried to access non-existent hardware. 
    /*BusExcep.active=1;
    BusExcep.addr=t;
    BusExcep.descr.in=1;
    BusExcep.descr.rw=0;*/
    CPUPins |= BErr;
    // finire...
    }
#endif
	}

void PutIntValue(DWORD t,DWORD t1) {
	register SWORD i;

// printf("rom_seg: %04x, p: %04x\n",rom_seg,p);

#if !defined(MC68020)
  if(t & 1) {
    AddressExcep.active=1;
    AddressExcep.addr=t;
    AddressExcep.descr.in=1;
    AddressExcep.descr.rw=0;
    }
#endif
#ifdef QL
	if(t >= RAM_START && t < (RAM_START+RAM_SIZE-3)) {		// 
		t-=RAM_START;
	  ram_seg[t++]=HIBYTE(HIWORD(t1));
	  ram_seg[t++]=LOBYTE(HIWORD(t1));
	  ram_seg[t++]=HIBYTE(LOWORD(t1));
	  ram_seg[t]=LOBYTE(LOWORD(t1));
		}
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
  else {
    //Typically, you tried to access hardware directly when not in supervisor mode or tried to access non-existent hardware. 
    /*BusExcep.active=1;
    BusExcep.addr=t;
    BusExcep.descr.in=1;
    BusExcep.descr.rw=0;*/
    CPUPins |= BErr;
    // finire...
    }
#elif MACINTOSH
  switch(LOBYTE(HIWORD(t)) >> 4) {
    case 0:
    case 1:
    case 2:
    case 3:     // 
      if(VIA1RegW[1] & 0b00010000) {    // OVERLAY, PA<4>
        }
      else {
        ram_seg[t++ & (RAM_SIZE-1)]=HIBYTE(HIWORD(t1));
        ram_seg[t++ & (RAM_SIZE-1)]=LOBYTE(HIWORD(t1));
        ram_seg[t++ & (RAM_SIZE-1)]=HIBYTE(LOWORD(t1));
        ram_seg[t & (RAM_SIZE-1)]=LOBYTE(LOWORD(t1));
        }
      break;
      
/*    case 3:        //   IPC Rd   FINIRE QUA
//      i=IPCW;     // mah dice qua solo WR..  scrive 0x0e, pare
      // (tastiera ecc va qua...)  scrive 4 o 8 bit, data 1=$0E or 0=$0C
//				IPCCnt++;
      break;*/
      // 0x0d è IPC per seriale_baud_rate, 0x01 è report input status, 6=read ser1, 7=read ser2, 8=read keyboard

    case 4:
      break;
      
    case 5:     // nulla, dice
      break;
      
    case 6:
      if(VIA1RegW[1] & 0b00010000) {    // OVERLAY, PA<4>
        ram_seg[t++ & (RAM_SIZE-1)]=HIBYTE(HIWORD(t1));
        ram_seg[t++ & (RAM_SIZE-1)]=LOBYTE(HIWORD(t1));
        ram_seg[t++ & (RAM_SIZE-1)]=HIBYTE(LOWORD(t1));
        ram_seg[t & (RAM_SIZE-1)]=LOBYTE(LOWORD(t1));
        }
      break;
      
    case 7:     // nulla, dice
      break;
      
    case 8:     // nulla, dice (2 devices on bus
      break;
      
//    case 0x9:      // SCC 8530 read
    case 0xa:      // DO NOT USE (2 devices on bus
    case 0xb:      // SCC 8530 write
      break;
        
    case 0xc:      // DO NOT USE (2 devices on bus
      break;
      
    case 0xd:      // IWM
      break;
      
//    case 0xe0:      // DO NOT USE
    case 0xe:      // VIA, A0=0
      break;
      
    case 0xf:      // phase, auto-vector...
      break;
      
    default:
    //Typically, you tried to access hardware directly when not in supervisor mode or tried to access non-existent hardware. 
      /*BusExcep.active=1;
      BusExcep.addr=t;
      BusExcep.descr.in=1;
      BusExcep.descr.rw=1;*/
    CPUPins |= BErr;
      break;
    }
#elif MICHELEFABBRI
	if(t >= RAM_START && t < (RAM_START+RAM_SIZE-3)) {		// 
		t-=RAM_START;
	  ram_seg[t++]=HIBYTE(HIWORD(t1));
	  ram_seg[t++]=LOBYTE(HIWORD(t1));
	  ram_seg[t++]=HIBYTE(LOWORD(t1));
	  ram_seg[t]=LOBYTE(LOWORD(t1));
		}
  else {
    //Typically, you tried to access hardware directly when not in supervisor mode or tried to access non-existent hardware. 
/*    BusExcep.active=1;
    BusExcep.addr=t;
    BusExcep.descr.in=1;
    BusExcep.descr.rw=0;*/
    CPUPins |= BErr;
    // finire...
    }
#endif
  
	}

void initHW(void) {

#ifdef QL
  VideoAddress=VIDEO_ADDRESS_BASE;
  VideoMode=0b00000010;      // parto spento
  VICRaster=0;
  IPCW=255; IPCR=0; IPCData=0; IPCCnt=4; IPCState=0;
#elif MACINTOSH
  memset(VIA1RegR,0,sizeof(VIA1RegR));
  VIA1RegR[0]=0x8c;
  VIA1RegR[1]=0x10;
  memset(VIA1RegW,0,sizeof(VIA1RegW));
  VIA1RegW[0]=0x8c;
  VIA1RegW[1]=0x10;
  VIA1ShCnt=0;
  VIA1IRQSet=0;
  memset(IWMRegR,0,sizeof(IWMRegR));
  memset(IWMRegW,0,sizeof(IWMRegW));
  IWMRegR[1]=IWMRegW[1]=0b00000000; // (disco presente) viene impostato a 1F scrivendo a Mode, al boot
  IWMRegR[2]=IWMRegW[2]=0b10111111;
  IWMRegR[3]=IWMRegW[3]=0b00000000; // scritto a 1F al boot
  IWMSelect=0;
  floppyState[0]=0x80; floppyState[1]=0x00;   floppySector=0;
#ifndef USING_SIMULATOR
  if(!SW2) 
    floppyState[0]=0x00;
#warning REMEMBER disattivo floppy se parto con tasto premuto!
#endif

  floppyTrack=0;
  memset(z8530RegR,0,sizeof(z8530RegR));
  memset(z8530RegW,0,sizeof(z8530RegW));
  z8530RegRDA=z8530RegRDB=0;
  z8530RegWDA=z8530RegWDB=0;
  z8530RegW[4] =0b00000100;
  z8530RegW[7] =0b00100000;
  z8530RegW[9] =0b11000000;
  z8530RegW[11]=0b00001000;
  z8530RegW[14]=0b00110000;
  z8530RegW[15]=0b11111000;
  z8530RegR[1] =0b00000110;

  VideoAddress[0]=0x01a700;
  VideoAddress[1]=0x012700;
  VICRaster=0;
  //memset(PRam[20]; beh evidentemente no!

#ifdef ST7735
  OC1CONbits.ON = 0;   // spengo buzzer/audio
  PR2 = 65535;		 // 
  OC1RS = 65535;		 // 
#endif            
#ifdef ILI9341
  OC7CONbits.ON = 0;   // spengo buzzer/audio
  PR2 = 65535;		 // 
  OC7RS = 65535;		 // 
#endif            

#else
#endif      

  mouseState=0b11111111;    // b0-1 =X1/2; b2-3 =Y1/2; b4=button
  
#ifdef QL
  TIMIRQ=VIDIRQ=KBDIRQ=SERIRQ=RTCIRQ=MDVIRQ=0;
#elif MACINTOSH
  TIMIRQ=VIDIRQ=KBDIRQ=SCCIRQ=RTCIRQ=VIAIRQ=0;
#endif
  
  CPUPins=0 /*DoReset*/;  
	}


#if MACINTOSH

//https://thomasw.dev/post/mac-floppy-emu/
DWORD getMacSector(void) {
  uint8_t i;
  uint32_t n;
/*  Sectors per Track
Tracks 00-15 12
Tracks 16-31 11
Tracks 32-47 10
Tracks 48-63 9
Tracks 64-79 8*/

  n=0;
  for(i=0; i<floppyTrack; i++) {
#ifndef MACINTOSHPLUS
    n+=sectorsPerTrack[i];       // single-side
#else
    n+=sectorsPerTrack[i]*2;     // double-side
#endif
    }
  if(floppyState[0] & 0x8)             // head prima sotto poi sopra, 1=sopra (v.
    n+=sectorsPerTrack[i];
  
  n+=floppySector;
//TR0 0-1-2- mac sorridente -3-4-5-6-7-8-9-10-11-(0)  TR1 0-1-2-3-4??  TR2 0  TR3 0-1  TR4 0  TR5 0  TR6 0
//poi TR3 1-2-3  TR4 0    
/* no non sembra servire...
 *   switch(sectorsPerTrack[floppyTrack]) {
    case 12:
      n+=interleave_table[0][floppySector];
      break;
    case 11:
      n+=interleave_table[1][floppySector];
      break;
    case 10:
      n+=interleave_table[2][floppySector];
      break;
    case 9:
      n+=interleave_table[3][floppySector];
      break;
    case 8:
      n+=interleave_table[4][floppySector];
      break;
    }*/
  
  return n;  
	}
  

/*Header Fields (11 bytes):
The header field identifies the sector. The sub-fields are:
D5 AA 96  address marks: this identifies the fields as a header field
Track     encoded low 6 bits of track number
Sector    encoded sector number
Side      encoded high 2 bits of track number and side bit:
            decoded bit 5=0 for side 0, 1 for side 1
            decoded bit 0 is the high-order bit of the track number
            decoded bits 1-4 are reserved and should be 0
Format    encoded format specifications:
            decoded bit 5=0 for single-sided formats
            decoded bits 0-4 define format interleave:
              standard 2:1 interleave formats have 2 in the field
Checksum  checksum formed by exclusive 'or' in the track, sector side, and format fields
DE AA     bit slip marks: this identifies the end of the field
Off       pad byte where the write electronics were turned off*/
uint8_t *encodeMacSector(void) {
  int i;
  uint8_t c;
  uint16_t crc[3];
  uint8_t data[3],tag[12],*p;
  // in tutto abbiamo 3+11+3+(3+1+16+680+3+4+3/*710*/)
  
#define OFS_HDR (3+1)
#define OFS_BITSLIP (OFS_HDR+11)
#define OFS_DATA (OFS_BITSLIP+3)
  // DATA dovrebbero essere 710 codificati in tutto
  
/*Data Sync Field (6.25 bytes):
5 bit slip FF's (FF,3F,CF,F3,FC,FF)
The data sync field contains a pattern of ones and zeros that
  synchronizes the state machine with the data on the disk. This field
  is written whenever the data field is written.*/
  // BOH non è chiaro! da codice pare cerchi questi 3, qualsiasi
  sectorData[0]=0xff;        // bit slip
  sectorData[1]=0xff;
  sectorData[2]=0xff;
  sectorData[3]=0xff;   // aggiungo 1 perche' se ne perde 1 in lettura RDDATA0
/*  sectorData[0]=0xff;        // bit slip
  sectorData[1]=0x3f;
  sectorData[2]=0xcf;
  sectorData[3]=0xf3;
  sectorData[4]=0xfc;
  sectorData[5]=0xff;*/
  
  sectorData[OFS_HDR+0]=0xd5;
  sectorData[OFS_HDR+1]=0xaa;
  sectorData[OFS_HDR+2]=0x96;
  sectorData[OFS_HDR+3]=floppyTrack & 0x3f;    // GCR
  sectorData[OFS_HDR+7]=sectorData[OFS_HDR+3];
  sectorData[OFS_HDR+3]=gcr_encodebyte(sectorData[OFS_HDR+3]);
  sectorData[OFS_HDR+4]=floppySector;    // GCR
  sectorData[OFS_HDR+7] ^= sectorData[OFS_HDR+4];
  sectorData[OFS_HDR+4]=gcr_encodebyte(sectorData[OFS_HDR+4]);
  sectorData[OFS_HDR+5]=floppyTrack  >> 6 /*floppyTrack & 0x40 ? 1 : 0*/;   // nel codice pare i 5 bit alti, ma 1 basta.. (80 tracks)
  sectorData[OFS_HDR+5] |= floppyState[0] & 0x8 ? 0x20 : 0;    // GCR
  sectorData[OFS_HDR+7] ^= sectorData[OFS_HDR+5];
  sectorData[OFS_HDR+5]=gcr_encodebyte(sectorData[OFS_HDR+5]);
#ifndef MACINTOSHPLUS
  sectorData[OFS_HDR+6]=2 | 0x00 /*0x20*/;    // double side se macintoshplus V ANCHE SOPRA; GCR 
#else
  sectorData[OFS_HDR+6]=2 | 0x20;    // 
#endif
  sectorData[OFS_HDR+7] ^= sectorData[OFS_HDR+6];
  sectorData[OFS_HDR+6]=gcr_encodebyte(sectorData[OFS_HDR+6]);
  sectorData[OFS_HDR+7]=gcr_encodebyte(sectorData[OFS_HDR+7]);
  sectorData[OFS_HDR+8]=0xde;
  sectorData[OFS_HDR+9]=0xaa;
  sectorData[OFS_HDR+10]=0xff; /*0x0  boh  pad, l'asm non lo controlla, lo salta, e io potevo saltarlo sopra v. CMQ verrebbero ignorati da asm, dopo tag e prima di data*/;
//  fino a un max di 48, pare (v. $18cec
  
  sectorData[OFS_BITSLIP+0]=0xff;        // bit slip NON c'è nell'asm!!! 
  sectorData[OFS_BITSLIP+1]=0xff;
  sectorData[OFS_BITSLIP+2]=0xff;
/*  sectorData[OFS_BITSLIP+0]=0xff;        // bit slip
  sectorData[OFS_BITSLIP+1]=0x3f;
/*  sectorData[OFS_BITSLIP+2]=0xcf;
  sectorData[OFS_BITSLIP+3]=0xf3;
  sectorData[OFS_BITSLIP+4]=0xfc;
  sectorData[OFS_BITSLIP+5]=0xff;*/ //bah solo 2 o 3 pare... e non codificati o boh
  

  sectorData[OFS_DATA+0]=0xd5;
  sectorData[OFS_DATA+1]=0xaa;
  sectorData[OFS_DATA+2]=0xad;
  sectorData[OFS_DATA+3]=gcr_encodebyte(floppySector);
  // il pdf dice che c'è solo questo (1 byte=sector) ma nell'asm ce ne sono 10... codificati come gli altri sotto
  //  pare TAG per info o boh
  crc[0]=crc[1]=crc[2]=0;
  
  memset(tag,0,12);   // per ora 0, dicono anche altri...

strcpy(tag,"dario 0\x1\x2\x3\x4");

  p=tag;
  for(i=0; i<((12/6)*8); i+=4) {   // 13 (conta da 10 a 0 step 3..) nel codice asm, credo incluso quello sopra (sector
    data[0]=p[0];
    data[1]=p[1];
    data[2]=p[2];
    crc[2] <<= 1;               //1
    c=HIBYTE(crc[2]) ? 1 : 0;
    crc[2] |= c;
    crc[2] &= 0xff;
    crc[0]=crc[0]+data[0]+c;    //2
    c=HIBYTE(crc[0]) ? 1 : 0;
    crc[0] &= 0xff;
    data[0]=data[0]^crc[2];     //3
    crc[1]=crc[1]+data[1]+c;    //4
    c=HIBYTE(crc[1]) ? 1 : 0;
    crc[1] &= 0xff;
    data[1]=data[1]^crc[0];     //5
    crc[2]=crc[2]+data[2]+c;    //6
//    c=HIBYTE(crc[2]) ? 1 : 0;
    crc[2] &= 0xff;
    data[2]=data[2]^crc[1];     //7
    sectorData[OFS_DATA+4+i]=gcr_encodebyte(((data[0] & 0xc0) >> 2) | ((data[1] & 0xc0) >> 4) | ((data[2] & 0xc0) >> 6));   //8,9
    sectorData[OFS_DATA+4+1+i]=gcr_encodebyte(data[0] & 0x3f);
    sectorData[OFS_DATA+4+2+i]=gcr_encodebyte(data[1] & 0x3f);
    sectorData[OFS_DATA+4+3+i]=gcr_encodebyte(data[2] & 0x3f);
    p+=3;
    }
  
  macintoshDisk=((uint8_t*)MACINTOSH_DISK)+getMacSector()*512;
  for(i=0; i<((510/6)*8); i+=4) {   // 510+2 nel codice asm
    data[0]=macintoshDisk[0];
    data[1]=macintoshDisk[1];
    data[2]=macintoshDisk[2];
    crc[2] <<= 1;               //1
    c=HIBYTE(crc[2]) ? 1 : 0;
    crc[2] |= c;
    crc[2] &= 0xff;
    crc[0]=crc[0]+data[0]+c;    //2
    c=HIBYTE(crc[0]) ? 1 : 0;
    crc[0] &= 0xff;
    data[0]=data[0]^crc[2];     //3
    crc[1]=crc[1]+data[1]+c;    //4
    c=HIBYTE(crc[1]) ? 1 : 0;
    crc[1] &= 0xff;
    data[1]=data[1]^crc[0];     //5
    crc[2]=crc[2]+data[2]+c;    //6
//    c=HIBYTE(crc[2]) ? 1 : 0; non serve, pare
    crc[2] &= 0xff;
    data[2]=data[2]^crc[1];     //7
    sectorData[OFS_DATA+4+16+i]=gcr_encodebyte(((data[0] & 0xc0) >> 2) | ((data[1] & 0xc0) >> 4) | ((data[2] & 0xc0) >> 6));   //8,9
    sectorData[OFS_DATA+4+16+1+i]=gcr_encodebyte(data[0] & 0x3f);
    sectorData[OFS_DATA+4+16+2+i]=gcr_encodebyte(data[1] & 0x3f);
    sectorData[OFS_DATA+4+16+3+i]=gcr_encodebyte(data[2] & 0x3f);
    macintoshDisk+=3;
    }
  data[0]=macintoshDisk[0];   // ergo ultimi 2!
  data[1]=macintoshDisk[1];
  macintoshDisk+=2; /* esattamente 512 22/12*/
  crc[2] <<= 1;               //1
  c=HIBYTE(crc[2]) ? 1 : 0;
  crc[2] |= c;
  crc[2] &= 0xff;
  crc[0]=crc[0]+data[0]+c;    //
  c=HIBYTE(crc[0]) ? 1 : 0;
  crc[0] &= 0xff;
  data[0]=data[0]^crc[2];     //
  crc[1]=crc[1]+data[1]+c;    //
//  c=HIBYTE(crc[1]) ? 1 : 0;
  crc[1] &= 0xff;
  data[1]=data[1]^crc[0];     //
  sectorData[OFS_DATA+4+16+0+i]=gcr_encodebyte(((data[0] & 0xc0) >> 2) | ((data[1] & 0xc0) >> 4));
  sectorData[OFS_DATA+4+16+1+i]=gcr_encodebyte(data[0] & 0x3f);
  sectorData[OFS_DATA+4+16+2+i]=gcr_encodebyte(data[1] & 0x3f);
  i+=3;
  
  sectorData[OFS_DATA+4+16+i+0]=gcr_encodebyte(((crc[0] & 0xc0) >> 2) | ((crc[1] & 0xc0) >> 4) | ((crc[2] & 0xc0) >> 6));
  sectorData[OFS_DATA+4+16+i+1]=gcr_encodebyte(crc[0] & 0x3f);
  sectorData[OFS_DATA+4+16+i+2]=gcr_encodebyte(crc[1] & 0x3f);
  sectorData[OFS_DATA+4+16+i+3]=gcr_encodebyte(crc[2] & 0x3f);
  sectorData[OFS_DATA+4+16+i+4]=0xde;
  sectorData[OFS_DATA+4+16+i+5]=0xaa;
  sectorData[OFS_DATA+4+16+i+6]=0xff;
//
//  crc=52 27 44  per settore 0
  
  macintoshSectorPtr=0;  
  
  return sectorData;
  }


/*Data Field (710 bytes):
The data field contains the actual data in the sector. The sub-fields are:
D5 AA AD data marks: this identifies the field as a data field
Sector encoded sector number
Encoded Data 524 bytes encoded into 699 code bytes; the first 12 data
  bytes are typically used as a sector tag by the operating
  system, and the remaining 512 bytes are used for actual data.
Checksum  a 24-bit checksum encoded into 4 code byte
DE AA     bit slip marks: this identifies the end of the field
Off       pad byte where the write electronics were turned off*/
#endif      

