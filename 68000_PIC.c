// Local Header Files
#include <stdlib.h>
#include <string.h>
#include <xc.h>
#include "68000_PIC.h"

#include <sys/attribs.h>
#include <sys/kmem.h>

#if defined(ST7735)
#include "Adafruit_ST77xx.h"
#include "Adafruit_ST7735.h"
#endif
#if defined(ILI9341)
#include "Adafruit_ili9341.h"
#endif
#include "adafruit_gfx.h"



// PIC32MZ1024EFE064 Configuration Bit Settings

// 'C' source line config statements

// DEVCFG3
// USERID = No Setting
#pragma config FMIIEN = OFF             // Ethernet RMII/MII Enable (RMII Enabled)
#pragma config FETHIO = OFF             // Ethernet I/O Pin Select (Alternate Ethernet I/O)
#pragma config PGL1WAY = ON             // Permission Group Lock One Way Configuration (Allow only one reconfiguration)
#pragma config PMDL1WAY = ON            // Peripheral Module Disable Configuration (Allow only one reconfiguration)
#pragma config IOL1WAY = ON             // Peripheral Pin Select Configuration (Allow only one reconfiguration)
#pragma config FUSBIDIO = ON            // USB USBID Selection (Controlled by the USB Module)

// DEVCFG2
/* Default SYSCLK = 200 MHz (8MHz FRC / FPLLIDIV * FPLLMUL / FPLLODIV) */
//#pragma config FPLLIDIV = DIV_1, FPLLMULT = MUL_50, FPLLODIV = DIV_2
#pragma config FPLLIDIV = DIV_1         // System PLL Input Divider (1x Divider)
#pragma config FPLLRNG = RANGE_5_10_MHZ// System PLL Input Range (5-10 MHz Input)
#pragma config FPLLICLK = PLL_FRC       // System PLL Input Clock Selection (FRC is input to the System PLL)
#pragma config FPLLMULT = MUL_51       // System PLL Multiplier (PLL Multiply by 50)
#pragma config FPLLODIV = DIV_2        // System PLL Output Clock Divider (2x Divider)
#pragma config UPLLFSEL = FREQ_24MHZ    // USB PLL Input Frequency Selection (USB PLL input is 24 MHz)

// DEVCFG1
#pragma config FNOSC = FRCDIV           // Oscillator Selection Bits (Fast RC Osc w/Div-by-N (FRCDIV))
#pragma config DMTINTV = WIN_127_128    // DMT Count Window Interval (Window/Interval value is 127/128 counter value)
#pragma config FSOSCEN = ON             // Secondary Oscillator Enable (Enable SOSC)
#pragma config IESO = ON                // Internal/External Switch Over (Enabled)
#pragma config POSCMOD = OFF            // Primary Oscillator Configuration (Primary osc disabled)
#pragma config OSCIOFNC = OFF           // CLKO Output Signal Active on the OSCO Pin (Disabled)
#pragma config FCKSM = CSECME           // Clock Switching and Monitor Selection (Clock Switch Enabled, FSCM Enabled)
#pragma config WDTPS = PS16384          // Watchdog Timer Postscaler (1:16384)
  // circa 6-7 secondi, 24.7.19
#pragma config WDTSPGM = STOP           // Watchdog Timer Stop During Flash Programming (WDT stops during Flash programming)
#pragma config WINDIS = NORMAL          // Watchdog Timer Window Mode (Watchdog Timer is in non-Window mode)
#pragma config FWDTEN = ON             // Watchdog Timer Enable (WDT Enabled)
#pragma config FWDTWINSZ = WINSZ_25     // Watchdog Timer Window Size (Window size is 25%)
#pragma config DMTCNT = DMT31           // Deadman Timer Count Selection (2^31 (2147483648))
#pragma config FDMTEN = OFF             // Deadman Timer Enable (Deadman Timer is disabled)

// DEVCFG0
#pragma config DEBUG = OFF              // Background Debugger Enable (Debugger is disabled)
#pragma config JTAGEN = OFF             // JTAG Enable (JTAG Disabled)
#ifdef ILI9341      // pcb arduino forgetIvrea32
#pragma config ICESEL = ICS_PGx2        // ICE/ICD Comm Channel Select (Communicate on PGEC2/PGED2)
#else   // pcb radio 2019
#pragma config ICESEL = ICS_PGx1        // ICE/ICD Comm Channel Select (Communicate on PGEC1/PGED1)
// 1 su schedina PIC32 primissima, 2 su succ.
#endif
#pragma config TRCEN = OFF              // Trace Enable (Trace features in the CPU are disabled)
#pragma config BOOTISA = MIPS32         // Boot ISA Selection (Boot code and Exception code is MIPS32)
#pragma config FECCCON = OFF_UNLOCKED   // Dynamic Flash ECC Configuration (ECC and Dynamic ECC are disabled (ECCCON bits are writable))
#pragma config FSLEEP = OFF             // Flash Sleep Mode (Flash is powered down when the device is in Sleep mode)
#pragma config DBGPER = PG_ALL          // Debug Mode CPU Access Permission (Allow CPU access to all permission regions)
#pragma config SMCLR = MCLR_NORM        // Soft Master Clear Enable bit (MCLR pin generates a normal system Reset)
#pragma config SOSCGAIN = GAIN_2X       // Secondary Oscillator Gain Control bits (2x gain setting)
#pragma config SOSCBOOST = ON           // Secondary Oscillator Boost Kick Start Enable bit (Boost the kick start of the oscillator)
#pragma config POSCGAIN = GAIN_2X       // Primary Oscillator Gain Control bits (2x gain setting)
#pragma config POSCBOOST = ON           // Primary Oscillator Boost Kick Start Enable bit (Boost the kick start of the oscillator)
#pragma config EJTAGBEN = NORMAL        // EJTAG Boot (Normal EJTAG functionality)

// DEVCP0
#pragma config CP = OFF                 // Code Protect (Protection Disabled)

// SEQ3

// DEVADC0

// DEVADC1

// DEVADC2

// DEVADC3

// DEVADC4

// DEVADC7



const char CopyrightString[]= {'6','8','0','0','0',' ','E','m','u','l','a','t','o','r',' ','v',
	VERNUMH+'0','.',VERNUML/10+'0',(VERNUML % 10)+'0',' ','-',' ', '2','8','/','1','2','/','2','4', 0 };

const char Copyr1[]="(C) Dario's Automation 2022-2024 - G.Dar\xd\xa\x0";



// Global Variables:
extern BYTE fExit,debug;
extern BYTE ColdReset;
extern BYTE ram_seg[];
extern BYTE *rom_seg,*rom_seg2;
extern BYTE Keyboard[];
BYTE IRQenable;
#ifdef QL
extern volatile BYTE MDVIRQ;
extern DWORD VideoAddress;
extern BYTE VideoMode;
extern union {
  BYTE b[4];
  DWORD d;
  } RTC;
extern volatile BYTE TIMIRQ,VIDIRQ,KBDIRQ,SERIRQ,RTCIRQ;
#elif MACINTOSH
extern DWORD VideoAddress[2];
extern BYTE VideoMode;
extern union __attribute__((__packed__)) DWORD_BYTE RTC;
extern BYTE PRam[20];
extern BYTE VIA1RegR[16],VIA1RegW[16],VIA1ShCnt;
extern BYTE z8530RegR[16],z8530RegW[16],z8530RegRDA,z8530RegRDB,z8530RegWDA,z8530RegWDB;
extern BYTE mouseState;
uint8_t oldmouse=255;
extern volatile BYTE TIMIRQ,VIDIRQ,KBDIRQ,SCCIRQ,RTCIRQ;
#elif MICHELEFABBRI
extern BYTE LCDram[256 /* 4*40 per la geometria, vale così */],LCDfunction,LCDentry,LCDdisplay,LCDcursor;
extern signed char LCDptr;
extern BYTE IOExtPortI[4],LedPort[1];
extern BYTE i146818RegR[2],i146818RegW[2],i146818RAM[64];
extern const unsigned char fontLCD_eu[],fontLCD_jp[];
#else 
extern BYTE LCDram[256 /* 4*40 per la geometria, vale così */],LCDfunction,LCDentry,LCDdisplay,LCDcursor;
extern signed char LCDptr;
extern BYTE IOExtPortI[4],IOExtPortO[4];
extern BYTE IOPortI,IOPortO,ClIRQPort,ClWDPort;
extern BYTE i146818RegR[2],i146818RegW[2],i146818RAM[64];
extern const unsigned char fontLCD_eu[],fontLCD_jp[];
#endif
volatile PIC32_RTCC_DATE currentDate={1,1,0};
volatile PIC32_RTCC_TIME currentTime={0,0,0};
const BYTE dayOfMonth[12]={31,28,31,30,31,30,31,31,30,31,30,31};
volatile DWORD millis;


#ifdef MICHELEFABBRI
#elif QL
#else
WORD textColors[16]={BLACK,WHITE,RED,CYAN,MAGENTA,GREEN,BLUE,YELLOW,
	ORANGE,BROWN,BRIGHTRED,DARKGRAY,GRAY128,LIGHTGREEN,BRIGHTCYAN,LIGHTGRAY};
/*COLORREF Colori[16]={
	RGB(0,0,0),						 // nero
	RGB(0xff,0xff,0xff),	 // bianco
	RGB(0x80,0x00,0x00),	 // rosso
	RGB(0x00,0x80,0x80),	 // azzurro
	RGB(0x80,0x00,0x80),	 // porpora
	RGB(0x00,0x80,0x00),	 // verde
	RGB(0x00,0x00,0x80),	 // blu
	RGB(0x80,0x80,0x00),	 // giallo
	
	RGB(0xff,0x80,0x40),	 // arancio
	RGB(0x80,0x40,0x40),	 // marrone
	RGB(0xff,0x80,0x80),	 // rosso chiaro
	RGB(0x20,0x20,0x20),	 // grigio 1
	RGB(0x54,0x54,0x54),	 // grigio 2
	RGB(0xc0,0xc0,0xc0),	 // grigio chiaro
	RGB(0x80,0x80,0xff),	 // blu chiaro
	RGB(0xa8,0xa8,0xa8)		 // grigio 3
	};*/
WORD displayColor[3]={BLACK,BRIGHTRED,RED};

int PlotDisplay(WORD pos,BYTE ch,BYTE c) {
	register int i;
  int x,y;
  SWORD color;

#define DIGIT_X_SIZE 14
#define DIGIT_Y_SIZE 30
#define DIGIT_OBLIQ 2
  
  x=8;
  y=40;
  x+=(DIGIT_X_SIZE+4)*pos;
//	fillRect(x,y,DIGIT_X_SIZE+3,DIGIT_Y_SIZE+1,BLACK);
  
  if(c)
    color=BRIGHTRED;
  else
    color=RED;
  if(!(ch & 1)) {
    drawLine(x+1+DIGIT_OBLIQ,y+1,x+DIGIT_X_SIZE+DIGIT_OBLIQ,y+1,color);
    }
  else
    drawLine(x+1+DIGIT_OBLIQ,y+1,x+DIGIT_X_SIZE+DIGIT_OBLIQ,y+1,BLACK);
  if(!(ch & 2)) {
    drawLine(x+DIGIT_X_SIZE+DIGIT_OBLIQ,y+1,x+DIGIT_X_SIZE+DIGIT_OBLIQ/2,y+DIGIT_Y_SIZE/2,color);
    }
  else
    drawLine(x+DIGIT_X_SIZE+DIGIT_OBLIQ,y+1,x+DIGIT_X_SIZE+DIGIT_OBLIQ/2,y+DIGIT_Y_SIZE/2,BLACK);
  if(!(ch & 4)) {
    drawLine(x+DIGIT_X_SIZE+DIGIT_OBLIQ/2,y+DIGIT_Y_SIZE/2,x+DIGIT_X_SIZE,y+DIGIT_Y_SIZE,color);
    }
  else
    drawLine(x+DIGIT_X_SIZE+DIGIT_OBLIQ/2,y+DIGIT_Y_SIZE/2,x+DIGIT_X_SIZE,y+DIGIT_Y_SIZE,BLACK);
  if(!(ch & 8)) {
    drawLine(x+1,y+DIGIT_Y_SIZE,x+DIGIT_X_SIZE,y+DIGIT_Y_SIZE,color);
    }
  else
    drawLine(x+1,y+DIGIT_Y_SIZE,x+DIGIT_X_SIZE,y+DIGIT_Y_SIZE,BLACK);
  if(!(ch & 16)) {
    drawLine(x+1+DIGIT_OBLIQ/2,y+DIGIT_Y_SIZE/2,x+1,y+DIGIT_Y_SIZE,color);
    }
  else
    drawLine(x+1+DIGIT_OBLIQ/2,y+DIGIT_Y_SIZE/2,x+1,y+DIGIT_Y_SIZE,BLACK);
  if(!(ch & 32)) {
    drawLine(x+1+DIGIT_OBLIQ,y+1,x+1+DIGIT_OBLIQ/2,y+DIGIT_Y_SIZE/2,color);
    }
  else
    drawLine(x+1+DIGIT_OBLIQ,y+1,x+1+DIGIT_OBLIQ/2,y+DIGIT_Y_SIZE/2,BLACK);
  if(!(ch & 64)) {
    drawLine(x+1+DIGIT_OBLIQ/2,y+DIGIT_Y_SIZE/2,x+DIGIT_X_SIZE+DIGIT_OBLIQ/2,y+DIGIT_Y_SIZE/2,color);
    }
  else
    drawLine(x+1+DIGIT_OBLIQ/2,y+DIGIT_Y_SIZE/2,x+DIGIT_X_SIZE+DIGIT_OBLIQ/2,y+DIGIT_Y_SIZE/2,BLACK);
// più bello..?    drawLine(x+2+DIGIT_OBLIQ/2,y+DIGIT_Y_SIZE/2,x-1+DIGIT_X_SIZE+DIGIT_OBLIQ/2,y+DIGIT_Y_SIZE/2,BLACK);
  if(!(ch & 128)) {
    drawCircle(x+DIGIT_X_SIZE+2,y+DIGIT_Y_SIZE+1,1,color);    // 
    }
  else
    drawCircle(x+DIGIT_X_SIZE+2,y+DIGIT_Y_SIZE+1,1,BLACK);
  
	}
#endif


#ifdef QL
//#define REAL_SIZE 1

WORD graphColors256[8]={BLACK,BRIGHTGREEN,BRIGHTBLUE,BRIGHTCYAN,BRIGHTRED,BRIGHTYELLOW,BRIGHTMAGENTA,WHITE};    // G F R B -> RBG
WORD graphColors512[4]={BLACK,BRIGHTGREEN,BRIGHTRED,WHITE};    // G R
/* INK SuperBasic colors:
0 black   black
1 blue    black (should be avoided)
2 red     red
3 magenta red   (should be avoided)
4 green   green
5 cyan    green (should be avoided)
6 yellow  white (should be avoided)
7 white   white */

int UpdateScreen(SWORD rowIni, SWORD rowFin) {
	register int i,j;
	int k,y1,y2,x1,x2,row1,row2;
	register BYTE *p,*p1;
  BYTE chl,chh;

  // ci mette circa 1mS ogni passata... 4/1/23  @256*256 (erano 1.5 @512x256)
  
	// per SPI DMA https://www.microchip.com/forums/m1110777.aspx#1110777

  if(VideoMode & 0b00000010) {    // blanked
  //p1=VideoRAM+row1*(HORIZ_SIZE/2);
//    memset(p1,VICReg[0x20] | (VICReg[0x20] << 4),((HORIZ_SIZE/2)+(HORIZ_OFFSCREEN*2))*VERT_OFFSCREEN);
    }
  else if(rowIni>=0 && rowIni<=256) {
    
  LED3 = 1;
  
	UINT16 px,py;
  BYTE xtra;

#ifdef ST7735
  START_WRITE();
#ifdef REAL_SIZE
  setAddrWindow(0,rowIni,_width,rowFin-rowIni);
  if(VideoMode & 0b00001000) {    //512
    for(py=rowIni; py < min(128,rowFin); py++) {
      p1=((BYTE*)&ram_seg[VideoAddress-RAM_START]) + (py*HORIZ_SIZE);
      for(px=0; px<40; px++) {      // 40*4=160
        chl=*p1++;
        chh=*p1++;
//High byte (A0=0)              Low Byte (A0=1)           Mode
//D7 D6 D5 D4 D3 D2 D1 D0       D7 D6 D5 D4 D3 D2 D1 D0
//G7 G6 G5 G4 G3 G2 G1 G0       R7 R6 R5 R4 R3 R2 R1 R0   512-pixel
        writedata16x2(graphColors512[(chl & 0b10000000 ? 1 : 0) | (chh & 0b10000000 ? 2 : 0)],
                graphColors512[(chl & 0b00100000 ? 1 : 0) | (chh & 0b00100000 ? 2 : 0)]);
        writedata16x2(graphColors512[(chl & 0b00001000 ? 1 : 0) | (chh & 0b00001000 ? 2 : 0)],
                graphColors512[(chl & 0b0000010 ? 1 : 0) | (chh & 0b00000010 ? 2 : 0)]);
        // ogni 2 byte 8 pixel
        }
  #ifdef USA_SPI_HW
      ClrWdt();
  #endif
      }
    }
  else {
    for(py=rowIni; py < min(128,rowFin); py++) {
      p1=((BYTE*)&ram_seg[VideoAddress-RAM_START]) + (py*HORIZ_SIZE);
      for(px=0; px<40; px++) {
        chl=*p1++;
        chh=*p1++;
        
//High byte (A0=0)              Low Byte (A0=1)           Mode
//D7 D6 D5 D4 D3 D2 D1 D0       D7 D6 D5 D4 D3 D2 D1 D0
//G3 F3 G2 F2 G1 F1 G0 F0       R3 B3 R2 B2 R1 B1 R0 B0   256-pixel
        
        // inserire FLASH
        writedata16x2(graphColors256[((chl & 0b10000000) >> 7) | ((chh & 0b11000000) >> 5)],
                graphColors256[((chl & 0b00100000) >> 5) | ((chh & 0b00110000) >> 3)]);
        writedata16x2(graphColors256[((chl & 0b00001000) >> 3) | ((chh & 0b00001100) >> 1)],
                graphColors256[((chl & 0b00000010) >> 1) | ((chh & 0b00000011) << 1)]);
        // forse riarrangiare la tabella colori per facilitare shift...
        
        // ogni 2 byte 4 pixel
        xtra++;
        }
  #ifdef USA_SPI_HW
      ClrWdt();
  #endif
      }
    }
#else
  setAddrWindow(0,rowIni/2,_width,(rowFin-rowIni)/2);
  if(!(VideoMode & 0b00001000)) {    //512
    for(py=rowIni; py<rowFin; py+=2) {    // 256 linee diventa 128
      p1=((BYTE*)&ram_seg[VideoAddress-RAM_START]) + (py*HORIZ_SIZE);
      xtra=0;
      for(px=0; px<HORIZ_SIZE; px+=2) {    // 512 pixel diventano 128...
        chl=*p1++;
        chh=*p1++;

//High byte (A0=0)              Low Byte (A0=1)           Mode
//D7 D6 D5 D4 D3 D2 D1 D0       D7 D6 D5 D4 D3 D2 D1 D0
//G7 G6 G5 G4 G3 G2 G1 G0       R7 R6 R5 R4 R3 R2 R1 R0   512-pixel
        writedata16x2(graphColors512[(chl & 0b10000000 ? 1 : 0) | (chh & 0b10000000 ? 2 : 0)],
                graphColors512[(chl & 0b00001000 ? 1 : 0) | (chh & 0b00001000 ? 2 : 0)]);
        // ogni 2 byte 8 pixel
        xtra++;
        if(xtra>=2) {   // 128->160 USARE (px & 2) ??
          writedata16(graphColors512[(chl & 0b00000010 ? 1 : 0) | (chh & 0b00000010 ? 2 : 0)]);
          xtra=0;
          }
        }
  #ifdef USA_SPI_HW
      ClrWdt();
  #endif
      }
    }
  else {
    for(py=rowIni; py<rowFin; py+=2) {    // 256 linee diventa 128
      p1=((BYTE*)&ram_seg[VideoAddress-RAM_START]) + (py*HORIZ_SIZE);
      xtra=0;
      for(px=0; px<HORIZ_SIZE; px+=2) {    // 256 pixel diventano 128...
        chl=*p1++;
        chh=*p1++;
        
//High byte (A0=0)              Low Byte (A0=1)           Mode
//D7 D6 D5 D4 D3 D2 D1 D0       D7 D6 D5 D4 D3 D2 D1 D0
//G3 F3 G2 F2 G1 F1 G0 F0       R3 B3 R2 B2 R1 B1 R0 B0   256-pixel
        writedata16x2(graphColors256[((chl & 0b10000000) >> 7) | ((chh & 0b11000000) >> 5)],
                graphColors256[((chl & 0b00001000) >> 3) | ((chh & 0b00001100) >> 1)]);
        // ogni 2 byte 4 pixel
        xtra++;
        if(xtra>=2) {   // 128->160 USARE (px & 2) ??
          writedata16(graphColors256[((chl & 0b00000010) >> 1) | ((chh & 0b00000011) << 1)]);
          xtra=0;
          }
        }
  #ifdef USA_SPI_HW
      ClrWdt();
  #endif
      }
    }
#endif
  
  END_WRITE();
  //	writecommand(CMD_NOP);
  
#endif

#ifdef ILI9341
  START_WRITE();
#ifdef REAL_SIZE
  setAddrWindow(0,rowIni,_width,rowFin-rowIni);
  if(VideoMode & 0b00001000) {    //512
    for(py=rowIni; py < min(240,rowFin); py++) {
      p1=((BYTE*)&ram_seg[VideoAddress-RAM_START]) + (py*HORIZ_SIZE);
      for(px=0; px<40; px++) {      // 40*4=160
        chl=*p1++;
        chh=*p1++;
//High byte (A0=0)              Low Byte (A0=1)           Mode
//D7 D6 D5 D4 D3 D2 D1 D0       D7 D6 D5 D4 D3 D2 D1 D0
//G7 G6 G5 G4 G3 G2 G1 G0       R7 R6 R5 R4 R3 R2 R1 R0   512-pixel
        writedata16x2(graphColors512[(chl & 0b10000000 ? 1 : 0) | (chh & 0b10000000 ? 2 : 0)],
                graphColors512[(chl & 0b00100000 ? 1 : 0) | (chh & 0b00100000 ? 2 : 0)]);
        writedata16x2(graphColors512[(chl & 0b00001000 ? 1 : 0) | (chh & 0b00001000 ? 2 : 0)],
                graphColors512[(chl & 0b0000010 ? 1 : 0) | (chh & 0b00000010 ? 2 : 0)]);
        // ogni 2 byte 8 pixel
        }
      ClrWdt();
      }
    }
  else {
    for(py=rowIni; py < min(240,rowFin); py++) {
      p1=((BYTE*)&ram_seg[VideoAddress-RAM_START]) + (py*HORIZ_SIZE);
      for(px=0; px<40; px++) {
        chl=*p1++;
        chh=*p1++;
        
//High byte (A0=0)              Low Byte (A0=1)           Mode
//D7 D6 D5 D4 D3 D2 D1 D0       D7 D6 D5 D4 D3 D2 D1 D0
//G3 F3 G2 F2 G1 F1 G0 F0       R3 B3 R2 B2 R1 B1 R0 B0   256-pixel
        
        // inserire FLASH
        writedata16x2(graphColors256[((chl & 0b10000000) >> 7) | ((chh & 0b11000000) >> 5)],
                graphColors256[((chl & 0b00100000) >> 5) | ((chh & 0b00110000) >> 3)]);
        writedata16x2(graphColors256[((chl & 0b00001000) >> 3) | ((chh & 0b00001100) >> 1)],
                graphColors256[((chl & 0b00000010) >> 1) | ((chh & 0b00000011) << 1)]);
        
        // ogni 2 byte 4 pixel
        xtra++;
        }
      ClrWdt();
      }
    }
#else
  setAddrWindow(0,(rowIni*75)/80,_width,1+  (((rowFin-rowIni)*75)/80));
  if(!(VideoMode & 0b00001000)) {    //512
    for(py=rowIni; py<rowFin; py++) {    // 256 linee diventa 240
      p1=((BYTE*)&ram_seg[VideoAddress-RAM_START]) + (py*HORIZ_SIZE);
      xtra=0;
      for(px=0; px<HORIZ_SIZE; px+=2) {    // 512 pixel diventano 320...
        chl=*p1++;
        chh=*p1++;

//High byte (A0=0)              Low Byte (A0=1)           Mode
//D7 D6 D5 D4 D3 D2 D1 D0       D7 D6 D5 D4 D3 D2 D1 D0
//G7 G6 G5 G4 G3 G2 G1 G0       R7 R6 R5 R4 R3 R2 R1 R0   512-pixel
        writedata16x2(graphColors512[(chl & 0b10000000 ? 1 : 0) | (chh & 0b10000000 ? 2 : 0)],
                graphColors512[(chl & 0b00100000 ? 1 : 0) | (chh & 0b00100000 ? 2 : 0)]);
        writedata16x2(graphColors512[(chl & 0b00001000 ? 1 : 0) | (chh & 0b00001000 ? 2 : 0)],
                graphColors512[(chl & 0b00000010 ? 1 : 0) | (chh & 0b00000010 ? 2 : 0)]);
        writedata16(graphColors512[(chl & 0b00000001 ? 1 : 0) | (chh & 0b00000001 ? 2 : 0)]);
//        ogni 2 byte 5 pixel
        }
      ClrWdt();
      }
    }
  else {
    for(py=rowIni; py<rowFin; py++) {    // 256 linee diventa 240
      p1=((BYTE*)&ram_seg[VideoAddress-RAM_START]) + (py*HORIZ_SIZE);
      xtra=0;
      for(px=0; px<HORIZ_SIZE; px+=2) {    // 256 pixel diventano 320...
        chl=*p1++;
        chh=*p1++;
        
//High byte (A0=0)              Low Byte (A0=1)           Mode
//D7 D6 D5 D4 D3 D2 D1 D0       D7 D6 D5 D4 D3 D2 D1 D0
//G3 F3 G2 F2 G1 F1 G0 F0       R3 B3 R2 B2 R1 B1 R0 B0   256-pixel
        // gestire Flash
        writedata16x2(graphColors256[((chl & 0b10000000) >> 7) | ((chh & 0b11000000) >> 5)],
                graphColors256[((chl & 0b00100000) >> 5) | ((chh & 0b00110000) >> 3)]);
        writedata16x2(graphColors256[((chl & 0b00001000) >> 3) | ((chh & 0b00001100) >> 1)],
                graphColors256[((chl & 0b00000010) >> 1) | ((chh & 0b00000011) << 1)]);
        // ogni 2 byte 5 pixel
        writedata16(graphColors256[((chl & 0b00000010) >> 1) | ((chh & 0b00000011) << 1)]);
        }
      ClrWdt();
      }
    }
#endif
  END_WRITE();
#endif
   
    
    LED3 = 0;

    }
  }  

#elif MACINTOSH

#define REAL_SIZE 1

/* 16bit words, leftmost pixel = MSB
*/

int UpdateScreen(SWORD rowIni, SWORD rowFin) {
	register int i,j;
	int k,y1,y2,x1,x2,row1,row2;
	register BYTE /*WORD siamo big-endian e quindi...*/ *p,*p1;
  BYTE /*WORD*/ ch;   // ziofa :)

  // ci mette circa 1mS ogni passata... 4/1/23  @256*256 (erano 1.5 @512x256)
  
	// per SPI DMA https://www.microchip.com/forums/m1110777.aspx#1110777
extern BYTE floppyState[2];
  
  if(rowIni>=0 && rowIni<342) {
    
  LED3 = 1;
  
#ifdef ST7735
	UINT16 px,py;
  BYTE xtra;
  START_WRITE();
#ifdef REAL_SIZE
  setAddrWindow(0,rowIni,_width,rowFin-rowIni);
  for(py=rowIni; py < min(128,rowFin); py++) {
    p1=((BYTE*)&ram_seg[(VIA1RegW[1] & 0b01000000 ? VideoAddress[0] : VideoAddress[1]) & (RAM_SIZE-1) ]) + (py*HORIZ_SIZE/8);
    
#warning  centro realsize per prove, dischetto boot
    p1=((BYTE*)&ram_seg[(VIA1RegW[1] & 0b01000000 ? VideoAddress[0] : VideoAddress[1]) & (RAM_SIZE-1) ]) + ((py+100)*HORIZ_SIZE/8);
    p1+=22;
    
    for(px=0; px<20; px++) {
      ch=*p1++;
      for(i=0; i<4; i++) {
        writedata16x2(ch & 0b10000000 ? BLACK : WHITE,
                ch & 0b01000000 ? BLACK : WHITE);
        ch <<= 2;
        }
      }
#ifdef USA_SPI_HW
    ClrWdt();
#endif
    }
#else
  setAddrWindow(0,rowIni/3,_width,(rowFin-rowIni)/3);
  for(py=rowIni; py<rowFin; py+=3) {    // 342 linee diventa 128~
    p1=((BYTE*)&ram_seg[(VIA1RegW[1] & 0b01000000 ? VideoAddress[0] : VideoAddress[1]) & (RAM_SIZE-1) ]) + (py*HORIZ_SIZE/8);
    for(px=0; px<HORIZ_SIZE/8; px++) {    // 512 pixel diventano 160...
      ch=*p1++;
      
      if(py>=110 && py<111 && px==61) {
        GFX_COLOR led;
        if(floppyState[0] & 0x4)      // floppy motor 
          led=BRIGHTRED;
        else
          led=BLACK;

        writedata16x2(led,led);
        writedata16(led);
        }
      else {
        writedata16x2(ch & 0b01000000 ? BLACK : WHITE,
                ch & 0b00000100 ? BLACK : WHITE);
        if(px & 1)
          writedata16(ch & 0b00000001 ? BLACK : WHITE);
        }
      }
#ifdef USA_SPI_HW
    ClrWdt();
#endif
    }
#endif

  END_WRITE();
  //	writecommand(CMD_NOP);
#endif
  
#ifdef ILI9341
#undef  REAL_SIZE 
	UINT16 px,py;
  UINT8 cnt;
  START_WRITE();
#ifdef REAL_SIZE
  setAddrWindow(0,rowIni,_width,rowFin-rowIni);
  for(py=rowIni; py < min(240,rowFin); py++) {
9    p1=((BYTE*)&ram_seg[(VIA1RegW[1] & 0b01000000 ? VideoAddress[0] : VideoAddress[1]) & (RAM_SIZE-1) ]) + (py*HORIZ_SIZE/8);
    
    for(px=0; px<40; px++) {
      ch=*p1++;
      for(i=0; i<4; i++) {
        writedata16x2(ch & 0b10000000 ? BLACK : WHITE,
                ch & 0b01000000 ? BLACK : WHITE);
        ch <<= 2;
        }
      }
    ClrWdt();
    }
#else
  setAddrWindow(0,(rowIni*7)/10,_width,1   +(((rowFin-rowIni)*7)/10));
  cnt=0;
  for(py=(rowIni*7)/10; py<(rowFin*7)/10; py++) {    // 342 linee diventa 240~
    p1=((BYTE*)&ram_seg[(VIA1RegW[1] & 0b01000000 ? VideoAddress[0] : VideoAddress[1]) & (RAM_SIZE-1) ]) + (py*HORIZ_SIZE/8);
    for(px=0; px<HORIZ_SIZE/8; px++) {    // 512 pixel diventano 320...
      ch=*p1++;
      
      if(py>=234 && py<=236 && px==62) {
        GFX_COLOR led;
        if(floppyState[0] & 0x4)      // floppy motor 
          led=RED;
        else
          led=BLACK;

        writedata16x2(led,led);
        writedata16x2(led,led);
        writedata16(led);
        }
      else {
        writedata16x2(ch & 0b10000000 ? BLACK : WHITE,    // 5 ogni 8
              ch & 0b00100000 ? BLACK : WHITE);
        writedata16x2(ch & 0b00001000 ? BLACK : WHITE,
              ch & 0b00000010 ? BLACK : WHITE);
        writedata16(ch & 0b00000001 ? BLACK : WHITE);
        }

      }
/*    cnt++;
    if(cnt>2) {   // 342:240
#warning usare modulo e aggiungere 12 pixel 228->240
      cnt=0;
      py++;
      }*/
    ClrWdt();
    }
  
  
#endif
  END_WRITE();
#endif
   
    
    LED3 = 0;

    }
  }  

#elif MICHELEFABBRI
int UpdateScreen(WORD c) {
  int x,y,x1,y1;
  SWORD color;
	UINT8 i,j,lcdMax;
	BYTE *fontPtr,*lcdPtr;
  static BYTE cursorState=0,cursorDivider=0;

#define LCD_MAX_X 20
#define LCD_MAX_Y 4
#define DIGIT_X_SIZE 6
#define DIGIT_Y_SIZE 8
  
  
    LED3 = 1;

  y=(_TFTHEIGHT-(LCD_MAX_Y*DIGIT_Y_SIZE))/2 +20;
  
//	fillRect(x,y,DIGIT_X_SIZE+3,DIGIT_Y_SIZE+1,BLACK);
	gfx_drawRect((_TFTWIDTH-(LCD_MAX_X*DIGIT_X_SIZE))/2-1,(_TFTHEIGHT-(LCD_MAX_Y*DIGIT_Y_SIZE))/2 +16,
          DIGIT_X_SIZE*20+4,DIGIT_Y_SIZE*4+7,LIGHTGRAY);
  
  if(c)
    color=WHITE;
  else
    color=LIGHTGRAY;

  
//        LCDdisplay=7; //test cursore

  cursorDivider++;
  if(cursorDivider>=11) {
    cursorDivider=0;
    cursorState=!cursorState;
    }
          
        
  if(LCDdisplay & 4) {

    lcdMax=LCDfunction & 8 ? LCD_MAX_Y : LCD_MAX_Y/2;
    for(y1=0; y1<LCD_MAX_Y; y1++) {
      x=(_TFTWIDTH-(LCD_MAX_X*DIGIT_X_SIZE))/2;

  //    LCDram[0]='A';LCDram[1]='1';LCDram[2]=1;LCDram[3]=40;
  //    LCDram[21]='Z';LCDram[23]='8';LCDram[25]='0';LCDram[27]=64;LCDram[39]='.';
  //    LCDram[84+4]='C';LCDram[84+5]='4';


      switch(y1) {    // 4x20 
        case 0:
          lcdPtr=&LCDram[0];
          break;
        case 1:
          lcdPtr=&LCDram[0x40];
          break;
        case 2:
          lcdPtr=&LCDram[20];
          break;
        case 3:
          lcdPtr=&LCDram[0x40+20];
          break;
        }

      for(x1=0; x1<LCD_MAX_X; x1++) {
  //      UINT8 ch;

  //      ch=*lcdPtr;
  //      if(LCDdisplay & 2) {
  //	      if(!(LCDdisplay & 1) || cursorState) { questo era per avere il bloccone, fisso o lampeggiante, ma in effetti sui LCD veri è diverso!
        if((lcdPtr-&LCDram[0]) == LCDptr) {
          if(LCDdisplay & 2) {
            for(j=6; j>1; j--) {    //lineetta bassa E FONT TYPE QUA??
              drawPixel(x+x1*6+j, y+7, color);
              }
            }
          if((LCDdisplay & 1) && cursorState) {
            int k=LCDdisplay & 2 ? 7 : 8;

            for(i=0; i<k; i++) {    //

              if(LCDfunction & 4)   // font type...
                ;

              for(j=6; j>1; j--) {    //+ piccolo..
                drawPixel(x+x1*6+j, y+i, color);
                }
              }
            }
          goto skippa;
          }

        fontPtr=fontLCD_eu+((UINT16)*lcdPtr)*10;
        for(i=0; i<8; i++) {
          UINT8 line;

          line = pgm_read_byte(fontPtr+i);

          if(LCDfunction & 4)   // font type...
            ;

          for(j=6; j>0; j--, line >>= 1) {
            if(line & 0x1)
              drawPixel(x+x1*6+j, y+i, color);
            else
              drawPixel(x+x1*6+j, y+i, BLACK);
            }
          }

skippa:
        lcdPtr++;
        }

      y+=DIGIT_Y_SIZE;
      }
    }
  
  


  
	gfx_drawRect(4,41,_TFTWIDTH-8,13,ORANGE);
  for(i=0,j=1; i<8; i++,j<<=1) {
    if(LedPort[0] & j)
      fillCircle(10+i*9,47,3,RED);
    else
      fillCircle(10+i*9,47,3,DARKGRAY);
    }
          
    LED3 = 0;     // 2mS 17/11/22
	}
#else

int UpdateScreen(WORD c) {
  
  int x,y,x1,y1;
  SWORD color;
	UINT8 i,j,lcdMax;
	BYTE *fontPtr,*lcdPtr;
  static BYTE cursorState=0,cursorDivider=0;
  
#define LCD_MAX_X 20
#define LCD_MAX_Y 4
#define DIGIT_X_SIZE 6
#define DIGIT_Y_SIZE 8
  
  
//  i8255RegR[0] |= 0x80; mah... fare?? v. di là

  y=(_TFTHEIGHT-(LCD_MAX_Y*DIGIT_Y_SIZE))/2 +20;
  
//	fillRect(x,y,DIGIT_X_SIZE+3,DIGIT_Y_SIZE+1,BLACK);
	gfx_drawRect((_TFTWIDTH-(LCD_MAX_X*DIGIT_X_SIZE))/2-1,(_TFTHEIGHT-(LCD_MAX_Y*DIGIT_Y_SIZE))/2 +16,
          DIGIT_X_SIZE*20+4,DIGIT_Y_SIZE*4+7,LIGHTGRAY);
  
  if(c)
    color=WHITE;
  else
    color=LIGHTGRAY;

  
//        LCDdisplay=7; //test cursore

  cursorDivider++;
  if(cursorDivider>=11) {
    cursorDivider=0;
    cursorState=!cursorState;
    }
          
        
  if(LCDdisplay & 4) {

  lcdMax=LCDfunction & 8 ? LCD_MAX_Y : LCD_MAX_Y/2;
  for(y1=0; y1<LCD_MAX_Y; y1++) {
    x=(_TFTWIDTH-(LCD_MAX_X*DIGIT_X_SIZE))/2;
    
//    LCDram[0]='A';LCDram[1]='1';LCDram[2]=1;LCDram[3]=40;
//    LCDram[21]='Z';LCDram[23]='8';LCDram[25]='0';LCDram[27]=64;LCDram[39]='.';
//    LCDram[84+4]='C';LCDram[84+5]='4';
    
    
    switch(y1) {    // 4x20 
      case 0:
        lcdPtr=&LCDram[0];
        break;
      case 1:
        lcdPtr=&LCDram[0x40];
        break;
      case 2:
        lcdPtr=&LCDram[20];
        break;
      case 3:
        lcdPtr=&LCDram[0x40+20];
        break;
      }

    for(x1=0; x1<LCD_MAX_X; x1++) {
//      UINT8 ch;
  
//      ch=*lcdPtr;
//      if(LCDdisplay & 2) {
//	      if(!(LCDdisplay & 1) || cursorState) { questo era per avere il bloccone, fisso o lampeggiante, ma in effetti sui LCD veri è diverso!
      if((lcdPtr-&LCDram[0]) == LCDptr) {
        if(LCDdisplay & 2) {
          for(j=6; j>1; j--) {    //lineetta bassa E FONT TYPE QUA??
            drawPixel(x+x1*6+j, y+7, color);
            }
          }
        if((LCDdisplay & 1) && cursorState) {
          int k=LCDdisplay & 2 ? 7 : 8;

          for(i=0; i<k; i++) {    //

            if(LCDfunction & 4)   // font type...
              ;

            for(j=6; j>1; j--) {    //+ piccolo..
              drawPixel(x+x1*6+j, y+i, color);
              }
            }
          }
        goto skippa;
        }
      
      fontPtr=fontLCD_eu+((UINT16)*lcdPtr)*10;
      for(i=0; i<8; i++) {
        UINT8 line;

        line = pgm_read_byte(fontPtr+i);

        if(LCDfunction & 4)   // font type...
          ;
        
        for(j=6; j>0; j--, line >>= 1) {
          if(line & 0x1)
            drawPixel(x+x1*6+j, y+i, color);
          else
            drawPixel(x+x1*6+j, y+i, BLACK);
          }
        }
      
skippa:
      lcdPtr++;
      }
      
    y+=DIGIT_Y_SIZE;
    }
    }
  
//  i8255RegR[0] &= ~0x7f;
  
//  LuceLCD=i8255RegW[1] &= 0x80; fare??

  
	gfx_drawRect(4,41,_TFTWIDTH-9,13,ORANGE);
  for(i=0,j=1; i<8; i++,j<<=1) {
    if(IOExtPortO[0] & j)
      fillCircle(10+i*9,47,3,RED);
    else
      fillCircle(10+i*9,47,3,DARKGRAY);
    }
  for(i=8,j=1; i<16; i++,j<<=1) {
    if(IOExtPortO[1] & j)
      fillCircle(12+i*9,47,3,RED);
    else
      fillCircle(12+i*9,47,3,DARKGRAY);
    }
  for(i=0,j=1; i<8; i++,j<<=1) {
    if(IOPortO /*ram_seg[0] /*MemPort1 = 0x8000 */ & j)
      fillCircle(10+i*7,36,2,RED);
    else
      fillCircle(10+i*7,36,2,DARKGRAY);
    }
  
	}
#endif
  

int main(void) {


  // disable JTAG port
//  DDPCONbits.JTAGEN = 0;
  
  SYSKEY = 0x00000000;
  SYSKEY = 0xAA996655;    //qua non dovrebbe servire essendo 1° giro (v. IOLWAY
  SYSKEY = 0x556699AA;
  CFGCONbits.IOLOCK = 0;      // PPS Unlock
  SYSKEY = 0x00000000;

  
#ifdef ST7735
  RPB15Rbits.RPB15R = 4;        // Assign RPB15 as U6TX, pin 30
  U6RXRbits.U6RXR = 2;      // Assign RPB14 as U6RX, pin 29 
#ifdef USA_SPI_HW
  RPG8Rbits.RPG8R = 6;        // Assign RPG8 as SDO2, pin 6
//  SDI2Rbits.SDI2R = 1;        // Assign RPG7 as SDI2, pin 5
#endif
  RPD5Rbits.RPD5R = 12;        // Assign RPD5 as OC1, pin 53; anche vaga uscita audio :)
  RPD1Rbits.RPD1R = 12;        // Assign RPD1 as OC1, pin 49; buzzer
#endif
#ifdef ILI9341
  RPB1Rbits.RPB1R = 12;        // Assign RPB1 as OC7, pin 15; buzzer
#endif
  

//  PPSOutput(4,RPC4,OC1);   //buzzer 4KHz , qua rimappabile 

#ifdef DEBUG_TESTREFCLK
// test REFCLK
#ifdef ST7735
  PPSOutput(4,RPC4,REFCLKO2);   // RefClk su pin 1 (RG15, buzzer)
	REFOCONbits.ROSSLP=1;
	REFOCONbits.ROSEL=1;
	REFOCONbits.RODIV=0;
	REFOCONbits.ROON=1;
	TRISFbits.TRISF3=1;
#endif
#ifdef ILI9341
//  RPD9Rbits.RPD9R = 15;        // Assign RPD9 (SDA) as RefClk3, pin 43
  RPB8Rbits.RPB8R = 15;        // Assign RPB8 as RefClk3, pin 21 + comodo
	REFO3CONbits.SIDL=0;
	REFO3CONbits.RSLP=1;
	REFO3CONbits.ROSEL=1;   // PBclk
	REFO3CONbits.RODIV=1;   // :2
	REFO3CONbits.OE=1;
	REFO3CONbits.ON=1;
//	TRISDbits.TRISD9=1;
	TRISBbits.TRISB8=1;
#endif
#endif

  
  SYSKEY = 0x00000000;
  SYSKEY = 0xAA996655;
  SYSKEY = 0x556699AA;
  CFGCONbits.IOLOCK = 1;      // PPS Lock
  SYSKEY = 0x00000000;

   // Disable all Interrupts
  __builtin_disable_interrupts();
  
//  SPLLCONbits.PLLMULT=10;
  
  OSCTUN=0;
  OSCCONbits.FRCDIV=0;
  
  // Switch to FRCDIV, SYSCLK=8MHz
  SYSKEY=0xAA996655;
  SYSKEY=0x556699AA;
  OSCCONbits.NOSC=0x00; // FRC
  OSCCONbits.OSWEN=1;
  SYSKEY=0x33333333;
  while(OSCCONbits.OSWEN) {
    Nop();
    }
    // At this point, SYSCLK is ~8MHz derived directly from FRC
 //http://www.microchip.com/forums/m840347.aspx
  // Switch back to FRCPLL, SYSCLK=200MHz
  SYSKEY=0xAA996655;
  SYSKEY=0x556699AA;
  OSCCONbits.NOSC=0x01; // SPLL
  OSCCONbits.OSWEN=1;
  SYSKEY=0x33333333;
  while(OSCCONbits.OSWEN) {
    Nop();
    }
  // At this point, SYSCLK is ~200MHz derived from FRC+PLL
//***
  mySYSTEMConfigPerformance();
  //myINTEnableSystemMultiVectoredInt(();

    
#ifdef ST7735
	TRISB=0b0000000000110000;			// AN4,5 (rb4..5)
	TRISC=0b0000000000000000;
	TRISD=0b0000000000001100;			// 2 pulsanti
	TRISE=0b0000000000000000;			// 3 led
	TRISF=0b0000000000000000;			// 
	TRISG=0b0000000000000000;			// SPI2 (rg6..8)

  ANSELB=0;
  ANSELE=0;
  ANSELG=0;

  CNPUDbits.CNPUD2=1;   // switch/pulsanti
  CNPUDbits.CNPUD3=1;
  CNPUGbits.CNPUG6=1;   // I2C tanto per
  CNPUGbits.CNPUG8=1;  
#endif
#ifdef ILI9341

	TRISB=0b0000000000000001;			// pulsante; [ AN ?? ]
	TRISC=0b0000000000000000;
	TRISD=0b0000000000000000;			// 2led
	TRISE=0b0000000000000000;			// led
	TRISF=0b0000000000000001;			// pulsante
	TRISG=0b0000000000000000;			// SPI2 (rg6..8)

  ANSELB=0;
  ANSELE=0;
  ANSELG=0;

  CNPUFbits.CNPUF0=1;   // switch/pulsanti
  CNPUBbits.CNPUB0=1;
  CNPUDbits.CNPUD9=1;   // I2C tanto per
  CNPUDbits.CNPUD10=1;  
#endif
      
  
  Timer_Init();
  PWM_Init();
#ifdef ILI9341
  ADC_Init();   // per touch!
#endif
  UART_Init(/*230400L*/ 115200L);

  myINTEnableSystemMultiVectoredInt();
  ShortDelay(50000); 

  
//    	ColdReset=0;    Emulate(0);

#ifndef USING_SIMULATOR
//#ifndef __DEBUG
#ifdef ST7735
  Adafruit_ST7735_1(0,0,0,0,-1);
  Adafruit_ST7735_initR(INITR_BLACKTAB);
#endif
#ifdef ILI9341
  Adafruit_ILI9341_8(8, 9, 10, 11, 12, 13, 14);
	begin(0);
  __delay_ms(200);
#endif
  
//  displayInit(NULL);
  
#ifdef m_LCDBLBit
  m_LCDBLBit=1;
#endif
  
//	begin();
	clearScreen();

// init done
	setTextWrap(1);
//	setTextColor2(WHITE, BLACK);

	drawBG();
  
  __delay_ms(200);
  
	gfx_fillRect(3,_TFTHEIGHT-20,_TFTWIDTH-6,16,BLACK);
 	setTextColor(BLUE);
#ifdef ST7735
	LCDXY(1,14);
#elif ILI9341
	LCDXY(1,28);
#endif  
#ifdef QL
	gfx_print("(emulating 256*256)");
#elif MACINTOSH
	gfx_print("(emulating 512*342)");
#elif MICHELEFABBRI
	gfx_print("(emulating 8Led/4x20LCD)");
#else
	gfx_print("(emulating 8Led/4x20LCD)");
#endif
        
	LCDCls();


//#endif
#endif

  

#ifdef QL
// vabbe' :D il nome ... const unsigned short int QL_JM_BIN[]= {    // ql.jm 0000 ecc
extern const unsigned short int QL_JS_BIN[];
//  memcpy(rom_seg,QL_JS_BIN,0xc000);
  rom_seg=QL_JS_BIN;
#elif MICHELEFABBRI
extern const unsigned short int ROM_BIN[];
  rom_seg=ROM_BIN;
#elif MACINTOSH
extern const unsigned short int MACINTOSH_BIN[];
extern const unsigned short int MACINTOSHPLUS_BIN[];    // di questo c'è il disassemblato!
#if ROM_SIZE==0x10000
  rom_seg=MACINTOSH_BIN;
#else
  rom_seg=MACINTOSHPLUS_BIN;
#endif
#endif


#ifdef QL
#elif MACINTOSH
#elif MICHELEFABBRI
#else
  IOPortI=0b10111111;   // dip=1111; puls=1; Comx=1
#endif
        
  initHW();
	ColdReset=1;

  Emulate(0);

  }


void mySYSTEMConfigPerformance(void) {
  unsigned PLLIDIV;
  unsigned PLLMUL;
  unsigned PLLODIV;
  float CLK2USEC;
  unsigned int SYSCLK;
  char PLLODIVVAL[]={
    2,2,4,8,16,32,32,32
    };
  unsigned int cp0;

  PLLIDIV=SPLLCONbits.PLLIDIV+1;
  PLLMUL=SPLLCONbits.PLLMULT+1;
  PLLODIV=PLLODIVVAL[SPLLCONbits.PLLODIV];

  SYSCLK=(FOSC*PLLMUL)/(PLLIDIV*PLLODIV);
  CLK2USEC=SYSCLK/1000000.0f;

  SYSKEY = 0x0;
  SYSKEY = 0xAA996655;
  SYSKEY = 0x556699AA;

  if(SYSCLK<=60000000)
    PRECONbits.PFMWS=0;
  else if(SYSCLK<=120000000)
    PRECONbits.PFMWS=1;
  else if(SYSCLK<=210000000)		// per overclock :)
    PRECONbits.PFMWS=2;
  else if(SYSCLK<=252000000)
    PRECONbits.PFMWS=4;
  else
    PRECONbits.PFMWS=7;

  PRECONbits.PFMSECEN=0;    // non c'è nella versione "2019" ...
  PRECONbits.PREFEN=0x1;

  SYSKEY = 0x0;

	  // Set up caching
  cp0 = _mfc0(16, 0);
  cp0 &= ~0x07;
  cp0 |= 0b011; // K0 = Cacheable, non-coherent, write-back, write allocate
  _mtc0(16, 0, cp0);  

  }

void myINTEnableSystemMultiVectoredInt(void) {

  PRISS = 0x76543210;
  INTCONSET = _INTCON_MVEC_MASK /*0x1000*/;    //MVEC
  asm volatile ("ei");
  //__builtin_enable_interrupts();
  }

/* CP0.Count counts at half the CPU rate */
#define TICK_HZ (CPU_HZ / 2)

/* wait at least usec microseconds */
#if 0
void delay_usec(unsigned long usec) {
unsigned long start, stop;

  /* get start ticks */
  start = readCP0Count();

  /* calculate number of ticks for the given number of microseconds */
  stop = (usec * 1000000) / TICK_HZ;

  /* add start value */
  stop += start;

  /* wait till Count reaches the stop value */
  while (readCP0Count() < stop)
    ;
  }
#endif

void xdelay_us(uint32_t us) {
  
  if(us == 0) {
    return;
    }
  unsigned long start_count = ReadCoreTimer /*_CP0_GET_COUNT*/();
  unsigned long now_count;
  long cycles = ((GetSystemClock() + 1000000U) / 2000000U) * us;
  do {
    now_count = ReadCoreTimer /*_CP0_GET_COUNT*/();
    } while ((unsigned long)(now_count-start_count) < cycles);
  }

void __attribute__((used)) DelayUs(unsigned int usec) {
  unsigned int tWait, tStart;

  tWait=(GetSystemClock()/2000000)*usec;
  tStart=_mfc0(9,0);
  while((_mfc0(9,0)-tStart)<tWait)
    ClrWdt();        // wait for the time to pass
  }

void __attribute__((used)) DelayMs(unsigned int ms) {
  
  for(;ms;ms--)
    DelayUs(1000);
  }

// ===========================================================================
// ShortDelay - Delays (blocking) for a very short period (in CoreTimer Ticks)
// ---------------------------------------------------------------------------
// The DelayCount is specified in Core-Timer Ticks.
// This function uses the CoreTimer to determine the length of the delay.
// The CoreTimer runs at half the system clock. 100MHz
// If CPU_CLOCK_HZ is defined as 80000000UL, 80MHz/2 = 40MHz or 1LSB = 25nS).
// Use US_TO_CT_TICKS to convert from uS to CoreTimer Ticks.
// ---------------------------------------------------------------------------

void ShortDelay(                       // Short Delay
  DWORD DelayCount)                   // Delay Time (CoreTimer Ticks)
{
  DWORD StartTime;                    // Start Time
  StartTime = ReadCoreTimer();         // Get CoreTimer value for StartTime
  while ( (DWORD )(ReadCoreTimer() - StartTime) < DelayCount ) 
    ClrWdt();
  }
 

void Timer_Init(void) {

  T2CON=0;
  T2CONbits.TCS = 0;                  // clock from peripheral clock
  T2CONbits.TCKPS = 0b110;            // 1:64 prescaler (pwm clock=1KHz circa se buzzer viene impostato a 0x528=~1300)
  T2CONbits.T32 = 0;                  // 16bit
//  PR2 = 2000;                         // rollover every n clocks; 2000 = 50KHz
  PR2 = 65535;                         // per ora faccio solo onda quadra, v. SID
  T2CONbits.TON = 1;                  // start timer per PWM
  
  // TIMER 3 INITIALIZATION (TIMER IS USED AS A TRIGGER SOURCE FOR ALL CHANNELS).
  T3CON=0;
  T3CONbits.TCS = 0;                  // clock from peripheral clock
  T3CONbits.TCKPS = 0b101;            // 1:32 prescaler, mi servono 1600Hz per RTC/ToD ecc, e più precisione per Tach Macintosh
  PR3 = 1953;                         // rollover every n clocks; 
  T3CONbits.TON = 1;                  // start timer 

  IPC3bits.T3IP=4;            // set IPL 4, sub-priority 2??
  IPC3bits.T3IS=0;
  IEC0bits.T3IE=1;             // enable Timer 3 interrupt se si vuole

	}

void PWM_Init(void) {

  SYSKEY = 0x00000000;
  SYSKEY = 0xAA996655;
  SYSKEY = 0x556699AA;
  CFGCONbits.OCACLK=0;      // sceglie timer per PWM
  SYSKEY = 0x00000000;
  
#ifdef ST7735
  OC1CON = 0x0006;      // TimerX ossia Timer2; PWM mode no fault; Timer 16bit, TimerX
//  OC1R    = 500;		 // su PIC32 è read-only!
//  OC1RS   = 1000;   // 50%, relativo a PR2 del Timer2
  OC1R    = 32768;		 // su PIC32 è read-only!
  OC1RS   = 0;        // per ora faccio solo onda quadra, v. 
  OC1CONbits.ON = 1;   // on
#endif  
  
#ifdef ILI9341
  OC7CON = 0x0006;      // TimerX ossia Timer2; PWM mode no fault; Timer 16bit, TimerX
//  OC1R    = 500;		 // su PIC32 è read-only!
//  OC1RS   = 1000;   // 50%, relativo a PR2 del Timer2
  OC7R    = 32768;		 // su PIC32 è read-only!
  OC7RS   = 0;        // per ora faccio solo onda quadra, v. 
  OC7CONbits.ON = 1;   // on
#endif  

  }

void ADC_Init(void) {   // v. LCDcontroller e PC_PIC_audio

  
  // FINIRE !!
  
  ADCCON1=0;    // AICPMPEN=0, siamo sopra 2.5V
  CFGCONbits.IOANCPEN=0;    // idem; questo credo voglia SYSLOCK
  ADCCON2=0;
  ADCCON3=0;
  
  //Configure Analog Ports
  ADCCON3bits.VREFSEL = 0; //Set Vref to VREF+/-

  ADCCMPEN1=0x00000000;
  ADCCMPEN2=0x00000000;
  ADCCMPEN3=0x00000000;
  ADCCMPEN4=0x00000000;
  ADCCMPEN5=0x00000000;
  ADCCMPEN6=0x00000000;
  ADCFLTR1=0x00000000;
  ADCFLTR2=0x00000000;
  ADCFLTR3=0x00000000;
  ADCFLTR4=0x00000000;
  ADCFLTR5=0x00000000;
  ADCFLTR6=0x00000000;
  
  ADCFSTAT=0;
  
  ADCTRGMODE=0;
  // no SH ALT qua
  ADCTRGSNS=0;

  ADCTRG1=0;
  ADCTRG2=0;
  ADCTRG3=0;
  ADCTRG1bits.TRGSRC2 = 0b00011; // Set AN2 to trigger from scan trigger
  ADCTRG1bits.TRGSRC3 = 0b00011; // Set AN3 to trigger from scan trigger
  // 46 ecc usano SEMPRE scan implicito, mentre per AN8 vedere che fare
  
  // I PRIMI 12 POSSONO OVVERO DEVONO USARE gli ADC dedicati! e anche se si usano
  // poi gli SCAN, per quelli >12, bisogna usarli entrambi (e quindi TRGSRC passa a STRIG ossia "common")

  ADCIMCON1bits.DIFF0 = 0; // single ended, unsigned
  ADCIMCON1bits.SIGN0 = 0; // 
  ADCIMCON1bits.DIFF6 = 0; // 
  ADCIMCON1bits.SIGN6 = 0; // 
  ADCCON1bits.SELRES = 0b10; // ADC7 resolution is 10 bits
  ADCCON1bits.STRGSRC = 1; // Select scan trigger.

  // Initialize warm up time register
  ADCANCON = 0;
  ADCANCONbits.WKUPCLKCNT = 5; // Wakeup exponent = 32 * TADx

  ADCEIEN1 = 0;
    
  ADCCON2bits.ADCDIV = 64; // per SHARED: 2 TQ * (ADCDIV<6:0>) = 64 * TQ = TAD
  ADCCON2bits.SAMC = 5;
    
  ADCCON3bits.ADCSEL = 0;   //0=periph clock 3; 1=SYSCLK
  ADCCON3bits.CONCLKDIV = 4; // 25MHz, sotto è poi diviso 2 per il canale, = max 50MHz come da doc

  ADC2TIMEbits.SELRES=0b10;        // 10 bits
  ADC2TIMEbits.ADCDIV=4;       // 
  ADC2TIMEbits.SAMC=5;        // 
  ADC3TIMEbits.SELRES=0b10;        // 10 bits
  ADC3TIMEbits.ADCDIV=4;       // 
  ADC3TIMEbits.SAMC=5;        //   
  
  ADCCSS1 = 0; // Clear all bits
  ADCCSS2 = 0;
  ADCCSS1bits.CSS0 = 1; // AN0 (Class 1) set for scan
  ADCCSS1bits.CSS6 = 1; // AN6 (Class 2) set for scan

  ADC0CFG=DEVADC0;
  ADC1CFG=DEVADC1;
  ADC2CFG=DEVADC2;
  ADC3CFG=DEVADC3;
  ADC4CFG=DEVADC4;
  ADC7CFG=DEVADC7;

  ADCCON1bits.ON = 1;   //Enable AD
  ClrWdt();
  
  // Wait for voltage reference to be stable 
#ifndef USING_SIMULATOR
  while(!ADCCON2bits.BGVRRDY); // Wait until the reference voltage is ready
  //while(ADCCON2bits.REFFLT); // Wait if there is a fault with the reference voltage
#endif

  // Enable clock to the module.
  ADCANCONbits.ANEN7 = 1;
  ADCCON3bits.DIGEN7 = 1;
  ADCANCONbits.ANEN2 = 1;
  ADCCON3bits.DIGEN2 = 1;
  ADCANCONbits.ANEN3 = 1;
  ADCCON3bits.DIGEN3 = 1;
  
#ifndef USING_SIMULATOR
  while(!ADCANCONbits.WKRDY2); // Wait until ADC is ready
  while(!ADCANCONbits.WKRDY3); // 
  while(!ADCANCONbits.WKRDY7); // 
#endif
  
  // ADCGIRQEN1bits.AGIEN7=1;     // IRQ (anche ev. per DMA))

	}

BYTE ConvertADC(BYTE n) { // http://ww1.microchip.com/downloads/en/DeviceDoc/70005213f.pdf
  WORD retval;
  
  ANSELBbits.ANSB2 = 0;
  ANSELBbits.ANSB3 = 0;
  ANSELEbits.ANSE6 = 0;
  ANSELEbits.ANSE7 = 0;
  switch(n) {
    case 2:     //AD2
      ANSELBbits.ANSB3 = 1;
      break;
    case 3:     //AD3
      ANSELBbits.ANSB3 = 1;
      break;
    case 15:     //RE7
      ANSELEbits.ANSE7 = 1;
      break;
    case 16:     //RE6
      ANSELEbits.ANSE6 = 1;
      break;
    default:
      break;
    }

	__delay_us(30);
  
  ADCCON3bits.GSWTRG = 1; // Start software trigger

  switch(n) {
    case 2:
      ANSELBbits.ANSB2 = 1;
      while(!ADCDSTAT1bits.ARDY2)    // Wait for the conversion to complete
        ClrWdt();
      ADCDATA3;
      ADCDATA15;
      ADCDATA16;
      retval=ADCDATA0;
      break;
    case 3:
      ANSELBbits.ANSB3 = 1;
      while(!ADCDSTAT1bits.ARDY3)    // Wait for the conversion to complete
        ClrWdt();
      ADCDATA2;
      ADCDATA15;
      ADCDATA16;
      retval=ADCDATA3;
    // PARE che quando mandi il trigger, lui converte TUTTI i canali abilitati,
    // per cui se non pulisco "l'altro" mi becco un RDY e una lettura precedente...
    // forse COSI' funzionerebbe, PROVARE      while(ADCCON2bits.EOSRDY == 0) // Wait until the measurement run
      break;
      
    default:
      break;
    }
  
  ANSELBbits.ANSB2 = 0;
  ANSELBbits.ANSB3 = 0;
  ANSELEbits.ANSE6 = 0;
  ANSELEbits.ANSE7 = 0;
  
//    IFS0bits.AD1IF=0;
    
  return retval >> 0;   // 12 -> 0 bit
  }


void UART_Init(DWORD baudRate) {
  
  U6MODE=0b0000000000001000;    // BRGH=1
  U6STA= 0b0000010000000000;    // TXEN
  DWORD baudRateDivider = ((GetPeripheralClock()/(4*baudRate))-1);
  U6BRG=baudRateDivider;
  U6MODEbits.ON=1;
  
#if 0
  ANSELDCLR = 0xFFFF;
  CFGCONbits.IOLOCK = 0;      // PPS Unlock
  RPD11Rbits.RPD11R = 3;        // Assign RPD11 as U1TX
  U1RXRbits.U1RXR = 3;      // Assign RPD10 as U1RX
  CFGCONbits.IOLOCK = 1;      // PPS Lock

  // Baud related stuffs.
  U1MODEbits.BRGH = 1;      // Setup High baud rates.
  unsigned long int baudRateDivider = ((GetSystemClock()/(4*baudRate))-1);
  U1BRG = baudRateDivider;  // set BRG

  // UART Configuration
  U1MODEbits.ON = 1;    // UART1 module is Enabled
  U1STAbits.UTXEN = 1;  // TX is enabled
  U1STAbits.URXEN = 1;  // RX is enabled

  // UART Rx interrupt configuration.
  IFS1bits.U1RXIF = 0;  // Clear the interrupt flag
  IFS1bits.U1TXIF = 0;  // Clear the interrupt flag

  INTCONbits.MVEC = 1;  // Multi vector interrupts.

  IEC1bits.U1RXIE = 1;  // Rx interrupt enable
  IEC1bits.U1EIE = 1;
  IPC7bits.U1IP = 7;    // Rx Interrupt priority level
  IPC7bits.U1IS = 3;    // Rx Interrupt sub priority level
#endif
  }

char BusyUART1(void) {
  
  return(!U6STAbits.TRMT);
  }

void putsUART1(unsigned int *buffer) {
  char *temp_ptr = (char *)buffer;

    // transmit till NULL character is encountered 

  if(U6MODEbits.PDSEL == 3)        /* check if TX is 8bits or 9bits */
    {
        while(*buffer) {
            while(U6STAbits.UTXBF); /* wait if the buffer is full */
            U6TXREG = *buffer++;    /* transfer data word to TX reg */
        }
    }
  else {
        while(*temp_ptr) {
            while(U6STAbits.UTXBF);  /* wait if the buffer is full */
            U6TXREG = *temp_ptr++;   /* transfer data byte to TX reg */
        }
    }
  }

unsigned int ReadUART1(void) {
  
  if(U6MODEbits.PDSEL == 3)
    return (U6RXREG);
  else
    return (U6RXREG & 0xFF);
  }

void WriteUART1(unsigned int data) {
  
  if(U6MODEbits.PDSEL == 3)
    U6TXREG = data;
  else
    U6TXREG = data & 0xFF;
  }

void __ISR(_UART1_RX_VECTOR) UART1_ISR(void) {
  
  LATDbits.LATD4 ^= 1;    // LED to indicate the ISR.
  char curChar = U1RXREG;
  
#if QL
#elif MACINTOSH
  VIA1RegW[0xd] |= 0b10000000;
  if(VIA1RegW[0xe] & 0b10000000)
    SCCIRQ=1;
  z8530RegRDA=curChar;
  z8530RegRDB=curChar;    // boh :D
#endif
  
  IFS3bits.U1RXIF = 0;  // Clear the interrupt flag!
  }


int emulateKBD(BYTE ch) {
  int i;

/* QL: https://www.chibiakumas.com/68000/sinclairql.php
	  1     2     4     8    16    32    64    128      
7   Shift Ctrl 	Alt 	X     V     /     N     ,       // x=12 in tabella @B458 ossia B466, seguono di seguito gli altri
6 	8     2     6     Q     E     O     T     U
5 	9     W     I     Tab 	R     -     Y 	
4 	L     3     H     1     A     P     D     J
3 	I     Caps 	K     S     F     =     G     ;
2 	|     Z     .     C     B     pound 	M 	~
1 	Enter 	Left/J1-L 	Up/J1-U	Esc 	Right/J1-R 	\  Space/J1-F 	Down/J1-D     // Enter=@0xb493=45 dopo x
0 	F4/J2-U 	F1/J2-L 	5 	F2/J2-D 	F3/J2-R 	F5/J2-F 	4 	7
  
  a + 63 ci sono le maiuscole e gli shifted... ossia X=75, !=90
  
Sinclair QL keycode table       Till Harbaum            V1.0 - 08/04/2015
-------------------------------------------------------------------------

This table shows how the 64 keys of the Sinclair QL are wired and how their
codes on IPC cmd 8 and 9 are derived.

http://www.sinclairql.net/srv/keyboard_format.png

Example

E.g. pressing the Q key on a UK keyboard connects Pin 2 of J11 with Pin 4 of
J12. This is the key in row 1 and column 3. On IPC command 8 the 8049 will
respond with $0b for this key (8*1+3 = 11 = $0b). A Command 9 request for 
row 6 will return 8 (2^3 = 8).

+------------------- row (request to cmd 9 is 7-row)
|  +---------------- J11 (pin connected to J12 when pressed)
|  |  +------------- column (reply to cmd 9 is 2^column)
|  |  |  +---------- J12 (pin connected to J11 when pressed)
|  |  |  |  +------- keycode returned by IPC Command 8 (= 8*row+column)
|  |  |  |  |
|  |  |  |  |     +- key label on UK keyboard
|  |  |  |  |     |

   9     1        CTRL, reported in bit 1 of cmd 8 modifier bits
   9     2        SHIFT, reported in bit 2 of cmd 8 modifier bits
   9    11        ALT, reported in bit 0 of cmd 8 modifier bits

0  7  0  9        <unused> SHIFT is mapped here in cmd 9 reply
0  7  1  3        <unused> CTRL is mapped here in cmd 9 reply
0  7  2  7        <unused> ALT is mapped here in cmd 9 reply
0  7  3  4 03     x
0  7  4  5 04     v
0  7  5 10 05     /
0  7  6  6 06     n
0  7  7  8 07     ,

1  2  0  9 08     8
1  2  1  3 09     2
1  2  2  7 0A     6
1  2  3  4 0B     q
1  2  4  5 0C     e
1  2  5 10 0D     0
1  2  6  6 0E     t
1  2  7  8 0F     u

2  3  0  9 10     9
2  3  1  3 11     w
2  3  2  7 12     i
2  3  3  4 13     TAB
2  3  4  5 14     r
2  3  5 10 15     -
2  3  6  6 16     y
2  3  7  8 17     o

3  4  0  9 18     l
3  4  1  3 19     3
3  4  2  7 1A     h
3  4  3  4 1B     1
3  4  4  5 1C     a
3  4  5 10 1D     p
3  4  6  6 1E     d
3  4  7  8 1F     j

4  5  0  9 20     [
4  5  1  3 21     CAPS
4  5  2  7 22     k
4  5  3  4 23     s
4  5  4  5 24     f
4  5  5 10 25     =
4  5  6  6 26     g
4  5  7  8 27     ;

5  6  0  9 28     ]
5  6  1  3 29     z
5  6  2  7 2A     .
5  6  3  4 2B     c
5  6  4  5 2C     b
5  6  5 10 2D     Pound
5  6  6  6 2E     m
5  6  7  8 2F     '

6  8  0  9 30     RET
6  8  1  3 31     Left
6  8  2  7 32     Up
6  8  3  4 33     ESC
6  8  4  5 34     Right
6  8  5 10 35     \
6  8  6  6 36     SPACE
6  8  7  8 37     Down

7  1  0  9 38     F4
7  1  1  3 39     F1
7  1  2  7 3A     5
7  1  3  4 3B     F2
7  1  4  5 3C     F3
7  1  5 10 3D     F5
7  1  6  6 3E     4
7  1  7  8 3F     7
 
  */
  
#if QL
	switch(ch) {
    case 0:
      Keyboard[0] = Keyboard[1] = Keyboard[2] = 0x0;
      goto no_irq;
      break;
		case ' ':
      Keyboard[0] = 54;   // non c'è in tabella ma lo vedo nel codice...
			break;
		case 'A':
      Keyboard[0] = 28;
			break;
		case 'B':
      Keyboard[0] = 44;
			break;
		case 'C':
      Keyboard[0] = 43;
			break;
		case 'D':
      Keyboard[0] = 30;
			break;
		case 'E':
      Keyboard[0] = 12;
			break;
		case 'F':
      Keyboard[0] = 36;
			break;
		case 'G':
      Keyboard[0] = 38;
			break;
		case 'H':
      Keyboard[0] = 26;
			break;
		case 'I':
      Keyboard[0] = 18;
			break;
		case 'J':
      Keyboard[0] = 31;
			break;
		case 'K':
      Keyboard[0] = 34;
			break;
		case 'L':
      Keyboard[0] = 24;
			break;
		case 'M':
      Keyboard[0] = 46;
			break;
		case 'N':
      Keyboard[0] = 6;
			break;
		case 'O':
      Keyboard[0] = 23;
			break;
		case 'P':
      Keyboard[0] = 29;
			break;
		case 'Q':
      Keyboard[0] = 11;
			break;
		case 'R':
      Keyboard[0] = 20;
			break;
		case 'S':
      Keyboard[0] = 35;
			break;
		case 'T':
      Keyboard[0] = 14;
			break;
		case 'U':
      Keyboard[0] = 15;
			break;
		case 'V':
      Keyboard[0] = 4;
			break;
		case 'W':
      Keyboard[0] = 17;
			break;
		case 'X':
      Keyboard[0] = 3;
			break;
		case 'Y':
      Keyboard[0] = 22;
			break;
		case 'Z':
      Keyboard[0] = 41;
			break;
		case '0':
      Keyboard[0] = 13;
			break;
		case ')':
      Keyboard[0] = 13;
      Keyboard[1] |= 0x4;
			break;
		case '1':
      Keyboard[0] = 27;
			break;
		case '!':
      Keyboard[0] = 27;
      Keyboard[1] |= 0x4;
			break;
		case '2':
      Keyboard[0] = 9;
			break;
		case '@':
      Keyboard[0] = 9;
      Keyboard[1] |= 0x4;
			break;
		case '3':
      Keyboard[0] = 25;
			break;
		case '#':
      Keyboard[0] = 25;
      Keyboard[1] |= 0x4;
			break;
		case '4':
      Keyboard[0] = 62;
			break;
		case '$':
      Keyboard[0] = 62;
      Keyboard[1] |= 0x4;
			break;
		case '5':
      Keyboard[0] = 58;
			break;
		case '%':
      Keyboard[0] = 58;
      Keyboard[1] |= 0x4;
			break;
		case '6':
      Keyboard[0] = 10;
			break;
		case '^':
      Keyboard[0] = 10;
      Keyboard[1] |= 0x4;
			break;
		case '7':
      Keyboard[0] = 63;
			break;
		case '&':
      Keyboard[0] = 63;
      Keyboard[1] |= 0x4;
			break;
		case '8':
      Keyboard[0] = 8;
			break;
		case '*':
      Keyboard[0] = 8;
      Keyboard[1] |= 0x4;
			break;
		case '9':
      Keyboard[0] = 16;
			break;
		case '(':
      Keyboard[0] = 16;
      Keyboard[1] |= 0x4;
			break;
		case '.':
      Keyboard[0] = 19;
			break;
		case '[':
      Keyboard[0] = 32;
			break;
		case '{':
      Keyboard[0] = 32;
      Keyboard[1] |= 0x4;
			break;
		case ']':
      Keyboard[0] = 40;
			break;
		case '}':
      Keyboard[0] = 40;
      Keyboard[1] |= 0x4;
			break;
		case '=':
      Keyboard[0] = 37;
			break;
		case '+':
      Keyboard[0] = 37;
      Keyboard[1] |= 0x4;
			break;
		case ':':     // 
      Keyboard[0] = 39;
      Keyboard[1] |= 0x4;
			break;
		case ';':
      Keyboard[0] = 39;
			break;
		case ',':
      Keyboard[0] = 7;
			break;
//		case '.':
//      Keyboard[0] = 42;
//			break;
		case '\'':
      Keyboard[0] = 47;
			break;
		case '"':
      Keyboard[0] = 47;
      Keyboard[1] |= 0x4;
			break;
		case '`':   // in effetti esce £
//		case '£':
			break;
		case '~':
      Keyboard[0] = 45;
      Keyboard[1] |= 0x4;
			break;
		case '£':
      Keyboard[0] = 45;
			break;
		case '-':
      Keyboard[0] = 21;
			break;
		case '_':
      Keyboard[0] = 21;
      Keyboard[1] |= 0x4;
			break;
		case '?':
      Keyboard[0] = 2;     //
			break;
		case '/':
      Keyboard[0] = 5;     //
			break;
		case '\r':
      Keyboard[0] = 48;
			break;
		case '\n':
      Keyboard[0] = 48;
			break;
		case '\x1b':
      Keyboard[0] = 51;
			break;
      
		case 129:     // shift...
      Keyboard[1] |= 0x4;
			break;
		case 130:     // alt
      Keyboard[1] |= 0x1;
			break;
		case 131:     // ctrl
      Keyboard[1] |= 0x2;
			break;
      
		case 144:     // F1
      Keyboard[0] = 0x39;    //ossia sembra 232 dal codice a 4B02
			break;
		case 145:     // F2
      Keyboard[0] = 0x3b;		//ossia sembra 236 a 4B06
			break;
      
    default:
      goto no_irq;
      break;
      
/* We have to send a sequence of command bytes - with byte 6 as the row number - the trap will return a byte in D1 - with a bit high when the button is down.
    lea keycommand,a3
    move.b #$11,d0
    Trap #1

keycommand:
    dc.b $09    ;0
    dc.b $01    ;1
    dc.l 0        ;2345
    dc.b 1        ;6    - Row
    dc.b 2        ;7
  */

		}
  
  Keyboard[2] = 1;
 
#elif MACINTOSH
	switch(ch) {    // US keyboard...
    case 0:
      Keyboard[0] |= 0x80;    // rilascio
      Keyboard[1] |= 0x80;
//      goto no_irq;
      break;
		case ' ':
      Keyboard[0] = 0x63;   // 
			break;
		case 'A':
      Keyboard[0] = 0x01;
			break;
		case 'B':
      Keyboard[0] = 0x5b;
			break;
		case 'C':
      Keyboard[0] = 0x13;
			break;
		case 'D':
      Keyboard[0] = 0x05;
			break;
		case 'E':
      Keyboard[0] = 0x1d;
			break;
		case 'F':
      Keyboard[0] = 0x07;
			break;
		case 'G':
      Keyboard[0] = 0x0b;
			break;
		case 'H':
      Keyboard[0] = 0x09;
			break;
		case 'I':
      Keyboard[0] = 0x45;
			break;
		case 'J':
      Keyboard[0] = 0x4d;
			break;
		case 'K':
      Keyboard[0] = 0x51;
			break;
		case 'L':
      Keyboard[0] = 0x4b;
			break;
		case 'M':
      Keyboard[0] = 0x5d;
			break;
		case 'N':
      Keyboard[0] = 0x5b;
			break;
		case 'O':
      Keyboard[0] = 0x3f;
			break;
		case 'P':
      Keyboard[0] = 0x47;
			break;
		case 'Q':
      Keyboard[0] = 0x19;
			break;
		case 'R':
      Keyboard[0] = 0x1f;
			break;
		case 'S':
      Keyboard[0] = 0x03;
			break;
		case 'T':
      Keyboard[0] = 0x23;
			break;
		case 'U':
      Keyboard[0] = 0x41;
			break;
		case 'V':
      Keyboard[0] = 0x13;
			break;
		case 'W':
      Keyboard[0] = 0x1b;
			break;
		case 'X':
      Keyboard[0] = 0x0f;
			break;
		case 'Y':
      Keyboard[0] = 0x13;
			break;
		case 'Z':
      Keyboard[0] = 0x0d;
			break;
		case '0':
      Keyboard[0] = 0x3b;
			break;
		case ')':
      Keyboard[0] = 0x3b;
      // VERIFICARE modificatori!
      Keyboard[1] |= 0x4;
			break;
		case '1':
      Keyboard[0] = 0x25;
			break;
		case '!':
      Keyboard[0] = 0x25;
      Keyboard[1] |= 0x4;
			break;
		case '2':
      Keyboard[0] = 0x27;
			break;
		case '@':
      Keyboard[0] = 0x27;
      Keyboard[1] |= 0x4;
			break;
		case '3':
      Keyboard[0] = 0x29;
			break;
		case '#':
      Keyboard[0] = 0x29;
      Keyboard[1] |= 0x4;
			break;
		case '4':
      Keyboard[0] = 0x2b;
			break;
		case '$':
      Keyboard[0] = 0x2b;
      Keyboard[1] |= 0x4;
			break;
		case '5':
      Keyboard[0] = 0x2f;
			break;
		case '%':
      Keyboard[0] = 0x2f;
      Keyboard[1] |= 0x4;
			break;
		case '6':
      Keyboard[0] = 0x2d;
			break;
		case '^':
      Keyboard[0] = 0x2d;
      Keyboard[1] |= 0x4;
			break;
		case '7':
      Keyboard[0] = 0x35;
			break;
		case '&':
      Keyboard[0] = 0x35;
      Keyboard[1] |= 0x4;
			break;
		case '8':
      Keyboard[0] = 0x39;
			break;
		case '*':
      Keyboard[0] = 0x39;
      Keyboard[1] |= 0x4;
			break;
		case '9':
      Keyboard[0] = 0x33;
			break;
		case '(':
      Keyboard[0] = 0x33;
      Keyboard[1] |= 0x4;
			break;
		case '.':
      Keyboard[0] = 0x5f;
			break;
		case '[':
      Keyboard[0] = 0x43;
			break;
		case '{':
      Keyboard[0] = 0x43;
      Keyboard[1] |= 0x4;
			break;
		case ']':
      Keyboard[0] = 0x3d;
			break;
		case '}':
      Keyboard[0] = 0x3d;
      Keyboard[1] |= 0x4;
			break;
		case '=':
      Keyboard[0] = 0x31;
			break;
		case '+':
      Keyboard[0] = 0x31;
      Keyboard[1] |= 0x4;
			break;
		case ':':     // 
      Keyboard[0] = 0x53;
      Keyboard[1] |= 0x4;
			break;
		case ';':
      Keyboard[0] = 0x53;
			break;
		case ',':
      Keyboard[0] = 0x57;
			break;
//		case '.':
//      Keyboard[0] = 42;
//			break;
		case '\'':
      Keyboard[0] = 0x4f;
			break;
		case '"':
      Keyboard[0] = 0x4f;
      Keyboard[1] |= 0x4;
			break;
		case '`':
      Keyboard[0] = 0x65;
			break;
		case '~':
      Keyboard[0] = 0x65;
      Keyboard[1] |= 0x4;
			break;
		case '£':
			break;
		case '-':
      Keyboard[0] = 0x37;
			break;
		case '_':
      Keyboard[0] = 0x37;
      Keyboard[1] |= 0x4;
			break;
		case '?':
      Keyboard[0] = 0x59;     // boh
			break;
		case '/':
      Keyboard[0] = 0x59;     //
			break;
		case '\r':
      Keyboard[0] = 0x49;
			break;
		case '\n':
      Keyboard[0] = 0x49;
			break;
		case '\t':
      Keyboard[0] = 0x61;
			break;
		case '\x8':
      Keyboard[0] = 0x67;
			break;
		case '\x1b':
//      Keyboard[0] = 51; boh
			break;
      
		case 129:     // shift...
      Keyboard[0] |= 0x71;
			break;
		case 130:     // alt / mela / cmd
      Keyboard[0] |= 0x6f;
			break;
		case 131:     // ctrl / option
      Keyboard[0] |= 0x75;
			break;
      
		case 144:     // F1
      // non c'è
			break;
		case 145:     // F2
      // non c'è
			break;
     
    default:
      goto no_irq;
      break;
      
		}
    
#endif

//   
#if QL
  if(IRQenable & 0b01000000)        //1=attivi
    KBDIRQ=1;
#elif MACINTOSH
// v. di là  VIA1RegR[0xa] = Keyboard[0]; // (no, forse è solo su richiesta (v.
  VIA1ShCnt=1;
//#warning OCCHIO forse è meglio lasciare che il VIA lasci generare IRQ da shift register, qua...
//  VIA1RegW[0xd] |= 0b10000100;
//  if(VIA1RegW[0xe] & 0b00000100)
//    KBDIRQ=1;
  
  KBDIRQ=1;  // lo uso solo come semaforo qua!
  
#endif
  
  return;

  
no_irq:
#if QL
  Keyboard[2] = 0;
#elif MACINTOSH
  ;
#endif
//  if(IRQenable & 0b01000000)        //1=attivi
//    KBDIRQ=1;

  }

BYTE whichKeysFeed=0;
char keysFeed[32]={0};
volatile BYTE keysFeedPtr=255;
#ifdef QL
const char *keysFeed1="\x91";			// 0x91=145=F2 per andare in TV mode al boot; 0x90=F1=monitor
//const char *keysFeed2=" PRINT \"Ciao\"\n";
//const char *keysFeed3=" PRINT 666\n";
const char *keysFeed2="LIST \n";
const char *keysFeed3="PRINT 2\n";
//const char *keysFeed3="LIST\n";
const char *keysFeed4="PRINT 3\n";
//const char *keysFeed4="NEW\n";
//const char *keysFeed5="PRINT 4\n";
const char *keysFeed5="NEW\n";
//const char *keysFe1ed4="MODE 8\n";
//const char *keysFeed5="A$=\"dario\"\n";
#elif MACINTOSH
const char *keysFeed1="\x82\x83";			// CMD-option ridisegna desktop   http://www.jacsoft.co.nz/Tech_Notes/Mac_Keys.shtml
const char *keysFeed2="\n";
const char *keysFeed3="\t\n";
const char *keysFeed4="A\n";
const char *keysFeed5="B\n";
#endif

void __ISR(_TIMER_3_VECTOR,ipl4AUTO) TMR_ISR(void) {
// https://www.microchip.com/forums/m842396.aspx per IRQ priority ecc
  static BYTE divider,divider2;
  static WORD dividerTim;
  static WORD dividerEmulKbd;
  static BYTE keysFeedPhase=0;
  int i;

//  LED1 ^= 1;      // check timing: 1600Hz, 9/11/19 (fuck berlin day); 25/12/24
  
  divider++;
  if(divider>=32) {   // 50 Hz per TOD
    divider=0;
#ifdef QL
#elif MACINTOSH
    VIA1RegW[0xd] |= 0b01000000;
    if(VIA1RegW[0xe] & 0b01000000)
      TIMIRQ=1;
#endif
    }


  
  dividerTim++;
  millis++;
  if(dividerTim>=1600) {   // 1Hz RTC
    // vedere registro 0A, che ha i divisori...
    // i146818RAM[10] & 15
    dividerTim=0;
#ifdef QL
    RTC.d++;
//      RTCIRQ=1;
    
    if(IRQenable & 0b00100000)        //1=attivi
      MDVIRQ=1;     // proviamo...
    
#elif MACINTOSH
    RTC.d++;
    VIA1RegW[0xd] |= 0b00000001;
    if(VIA1RegW[0xe] & 0b00000001)
      RTCIRQ=1;
   
#elif MICHELEFABBRI
//      RTCIRQ=1;
#else
    if(!(i146818RAM[11] & 0x80)) {    // SET
      i146818RAM[10] |= 0x80;
      currentTime.sec++;
      if(currentTime.sec >= 60) {
        currentTime.sec=0;
        currentTime.min++;
        if(currentTime.min >= 60) {
          currentTime.min=0;
          currentTime.hour++;
          if( ((i146818RAM[11] & 2) && currentTime.hour >= 24) || 
            (!(i146818RAM[11] & 2) && currentTime.hour >= 12) ) {
            currentTime.hour=0;
            currentDate.mday++;
            i=dayOfMonth[currentDate.mon-1];
            if((i==28) && !(currentDate.year % 4))
              i++;
            if(currentDate.mday > i) {		// (rimangono i secoli...)
              currentDate.mday=0;
              currentDate.mon++;
              if(currentDate.mon > 12) {		// 
                currentDate.mon=1;
                currentDate.year++;
                }
              }
            }
          }
        } 
      i146818RAM[12] |= 0x90;
      i146818RAM[10] &= ~0x80;
      } 
    else
      i146818RAM[10] &= ~0x80;
    // inserire Alarm... :)
    i146818RAM[12] |= 0x40;     // in effetti dice che deve fare a 1024Hz! o forse è l'altro flag, bit3 ecc
    if(i146818RAM[12] & 0x40 && i146818RAM[11] & 0x40 ||
       i146818RAM[12] & 0x20 && i146818RAM[11] & 0x20 ||
       i146818RAM[12] & 0x10 && i146818RAM[11] & 0x10)     
      i146818RAM[12] |= 0x80;
    if(i146818RAM[12] & 0x80)     
      RTCIRQ=1;
#endif
		} 
  

  if(keysFeedPtr==255)      // EOL
    goto fine_kbd;
  if(keysFeedPtr==254) {    // NEW string
    keysFeedPtr=0;
    keysFeedPhase=0;
		switch(whichKeysFeed) {
			case 0:
				strcpy(keysFeed,keysFeed1);
				break;
			case 1:
				strcpy(keysFeed,keysFeed2);
				break;
			case 2:
				strcpy(keysFeed,keysFeed3);
				break;
			case 3:
				strcpy(keysFeed,keysFeed4);
				break;
			case 4:
				strcpy(keysFeed,keysFeed5);
				break;
			}
		whichKeysFeed++;
		if(whichKeysFeed>4)
			whichKeysFeed=0;
//    goto fine_kbd;
		}
  
  if(keysFeed[keysFeedPtr]) {
    dividerEmulKbd++;
    if(dividerEmulKbd>=300) {   // ~.2Hz per emulazione tastiera! (più veloce di tot non va...))
      dividerEmulKbd=0;
      if(!KBDIRQ) {     // semaforo...
        if(!keysFeedPhase) {
          keysFeedPhase=1;
          emulateKBD(keysFeed[keysFeedPtr]);
          }
        else {
          keysFeedPhase=0;
          emulateKBD(NULL);
          keysFeedPtr++;
wait_kbd: ;
          }
        }
      }
    }
  else
    keysFeedPtr=255;  
  
fine_kbd:
#if MACINTOSH
  if((mouseState ^ oldmouse) & 0b00001111) {
    oldmouse=mouseState;
    SCCIRQ=1;
    }
#endif
  
fine:
  IFS0CLR = _IFS0_T3IF_MASK;
  }


void readTouch() {
  uint16_t x,y;
  static uint16_t oldx,oldy;
  
//  TRISE &= ~0b0000000011000000;   già output
  TRISBbits.TRISB2=1;
  m_TouchX2=1;
  __delay_ms(25);
  x=ConvertADC(2);
  m_TouchX2=0;
  TRISBbits.TRISB2=0;
  
  TRISBbits.TRISB3=1;
  m_TouchY2=1;
  __delay_ms(25);
  y=ConvertADC(3);
  m_TouchY2=0;
  TRISBbits.TRISB3=0;
  
  if(x != oldx) {
    if(1)
      mouseState &= ~0b00000001;
    // GESTIRE quadratura!
    else
      mouseState &= ~0b00000010;
    
    oldx = x;
    }
  
  if(y != oldy) {
    if(y > oldy)
      mouseState &= ~0b00000100;
    // GESTIRE quadratura!
    else
      mouseState &= ~0b00001000;
    
    oldy = y;
    }
  }


/*******************************************************************************
  Exception Reason Data


  Remarks:
    These global static items are used instead of local variables in the
    _general_exception_handler function because the stack may not be available
    if an exception has occurred.
*/

// Code identifying the cause of the exception (CP0 Cause register).
static unsigned int _excep_code;

// Address of instruction that caused the exception.
static unsigned int _excep_addr;

// Pointer to the string describing the cause of the exception.
static char *_cause_str;

// Array identifying the cause (indexed by _exception_code).
static const char *cause[] = {
    "Interrupt",
    "Undefined",
    "Undefined",
    "Undefined",
    "Load/fetch address error",
    "Store address error",
    "Instruction bus error",
    "Data bus error",
    "Syscall",
    "Breakpoint",
    "Reserved instruction",
    "Coprocessor unusable",
    "Arithmetic overflow",
    "Trap",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved"
  };



// *****************************************************************************
// *****************************************************************************
// Section: Exception Handling
// *****************************************************************************
// *****************************************************************************

/*******************************************************************************
  Function:
    void _general_exception_handler ( void )

  Summary:
    Overrides the XC32 _weak_ _general_exception_handler.

  Description:
    This function overrides the XC32 default _weak_ _general_exception_handler.

  Remarks:
    Refer to the XC32 User's Guide for additional information.
 */
void _general_exception_handler(void) {
    /* Mask off Mask of the ExcCode Field from the Cause Register
    Refer to the MIPs Software User's manual */
  
    _excep_code = (_CP0_GET_CAUSE() & 0x0000007C) >> 2;
    _excep_addr = _CP0_GET_EPC();
    _cause_str  = (char *)cause[_excep_code];
//    SYS_DEBUG_PRINT(SYS_ERROR_FATAL, "\n\rGeneral Exception %s (cause=%d, addr=%x).\n\r",
//                    _cause_str, _excep_code, _excep_addr);


  	LCDCls();
  	setTextColorBG(BRIGHTRED,BLACK);
    LCDXY(0,0);
    gfx_print(_cause_str);    // e _excep_addr ?

    while(1)    {
//        SYS_DEBUG_BreakPoint();
        Nop();
        Nop();
        __delay_ms(2500);
      LED1^=1;
    }
  }



