//---------------------------------------------------------------------------
//
#ifndef _68000_PIC_INCLUDED
#define _68000_PIC_INCLUDED

//---------------------------------------------------------------------------


/* check if build is for a real debug tool */
#if defined(__DEBUG) && !defined(__MPLAB_ICD2_) && !defined(__MPLAB_ICD3_) && \
   !defined(__MPLAB_PICKIT2__) && !defined(__MPLAB_PICKIT3__) && \
   !defined(__MPLAB_REALICE__) && \
   !defined(__MPLAB_DEBUGGER_REAL_ICE) && \
   !defined(__MPLAB_DEBUGGER_ICD3) && \
   !defined(__MPLAB_DEBUGGER_PK3) && \
   !defined(__MPLAB_DEBUGGER_PICKIT2) && \
   !defined(__MPLAB_DEBUGGER_PIC32MXSK)
    #warning Debug with broken MPLAB simulator
    #define USING_SIMULATOR
#endif


//#define REAL_SIZE    


#define FCY 205000000ul    //Oscillator frequency; ricontrollato con baud rate, pare giusto così!

#define CPU_CLOCK_HZ             (FCY)    // CPU Clock Speed in Hz
#define CPU_CT_HZ            (CPU_CLOCK_HZ/2)    // CPU CoreTimer   in Hz
#define PERIPHERAL_CLOCK_HZ      (FCY/2 /*100000000UL*/)    // Peripheral Bus  in Hz
#define GetSystemClock()         (FCY)    // CPU Clock Speed in Hz
#define GetPeripheralClock()     (PERIPHERAL_CLOCK_HZ)    // Peripheral Bus  in Hz
#define FOSC 8000000ul

#define US_TO_CT_TICKS  (CPU_CT_HZ/1000000UL)    // uS to CoreTimer Ticks
    
#define VERNUML 5
#define VERNUMH 1



typedef char BOOL;
typedef unsigned char UINT8;
typedef unsigned char BYTE;
typedef signed char INT8;
typedef unsigned short int WORD;
typedef unsigned short int SWORD;       // v. C64: con int/32bit è più veloce!
typedef unsigned long UINT32;
typedef unsigned long DWORD;
typedef signed long INT32;
typedef unsigned short int UINT16;
typedef signed int INT16;

typedef DWORD COLORREF;

#define RGB(r,g,b)      ((COLORREF)(((BYTE)(r)|((WORD)((BYTE)(g))<<8))|(((DWORD)(BYTE)(b))<<16)))


#define TRUE 1
#define FALSE 0


#define _TFTWIDTH  		160     //the REAL W resolution of the TFT
#define _TFTHEIGHT 		128     //the REAL H resolution of the TFT

//#define SKYNET 1
//#define QL 1          v. progetto
#ifdef QL
#define HORIZ_SIZE 128
#define VERT_SIZE 256
#endif

typedef signed char GRAPH_COORD_T;
typedef unsigned char UGRAPH_COORD_T;

void mySYSTEMConfigPerformance(void);
void myINTEnableSystemMultiVectoredInt(void);

#define ReadCoreTimer()                  _CP0_GET_COUNT()           // Read the MIPS Core Timer

void ShortDelay(DWORD DelayCount);
#define __delay_ms(n) ShortDelay(n*100000UL)

#define ClrWdt() { WDTCONbits.WDTCLRKEY=0x5743; }

// PIC32 RTCC Structure
typedef union {
  struct {
    unsigned char   weekday;    // BCD codification for day of the week, 00-06
    unsigned char   mday;       // BCD codification for day of the month, 01-31
    unsigned char   mon;        // BCD codification for month, 01-12
    unsigned char   year;       // BCD codification for years, 00-99
  	};                              // field access	
  unsigned char       b[4];       // byte access
  unsigned short      w[2];       // 16 bits access
  unsigned long       l;          // 32 bits access
	} PIC32_RTCC_DATE;

// PIC32 RTCC Structure
typedef union {
  struct {
    unsigned char   reserved;   // reserved for future use. should be 0
    unsigned char   sec;        // BCD codification for seconds, 00-59
    unsigned char   min;        // BCD codification for minutes, 00-59
    unsigned char   hour;       // BCD codification for hours, 00-24
  	};                              // field access
  unsigned char       b[4];       // byte access
  unsigned short      w[2];       // 16 bits access
  unsigned long       l;          // 32 bits access
	} PIC32_RTCC_TIME;
extern volatile PIC32_RTCC_DATE currentDate;
extern volatile PIC32_RTCC_TIME currentTime;


union __attribute__((__packed__)) DWORD_BYTE {
  BYTE b[4];
  DWORD d;
  };
union __attribute__((__packed__)) WORD_BYTE {
  BYTE b[2];
  WORD w;
  };
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
		} ww;
	struct __attribute__((__packed__)) {
		BYTE u;
		BYTE u2;
		BYTE l;
		BYTE h;		 // oppure spostare la pipe quando ci sono le istruzioni lunghe 4+...
		} b;
	};
  union __attribute__((__packed__)) REG {
    DWORD d;
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
		};
	union __attribute__((__packed__)) A_REGISTERS {   // lascio 2 def separate ev. per gestire A7/SP
		BYTE  b[32];
	  union REG r[8];
		};
#define ID_CARRY 0x1
#define ID_OVF 0x2
#define ID_ZERO 0x4
#define ID_SIGN 0x8
#define ID_EXT 0x10
  union __attribute__((__packed__)) _CCR {
    BYTE b;
    struct __attribute__((__packed__)) {
      unsigned int Carry: 1;
      unsigned int Ovf: 1;
      unsigned int Zero: 1;
      unsigned int Sign: 1;
      unsigned int Ext: 1;
      unsigned int unused: 11;
      };
    };
  union __attribute__((__packed__)) _SR {
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
#ifdef MC68020
#endif
      unsigned int Supervisor: 1;
      unsigned int unused3: 1;    // su 68020 ecc questo è Trace0 e il seg. Trace1
#ifdef MC68020
#endif
      unsigned int Trace: 1;
      };
    };
	union __attribute__((__packed__)) REGISTRO_F {
    SWORD w;
    union _CCR CCR;
    union _SR SR;
		};
  union __attribute__((__packed__)) RESULT {
    struct __attribute__((__packed__)) {
      BYTE l;
      BYTE h;
      } b;
    struct __attribute__((__packed__)) {
      BYTE b1;
      BYTE b2;
      BYTE b3;
      BYTE b4;      // cmq non conviene usare b4 rispetto a int...
      } bb;
    WORD w;
    DWORD d;
    };
struct ADDRESS_EXCEPTION {
  DWORD addr;
  union {
    WORD w;
    struct {
      unsigned short int fc:3;
      unsigned short int in:1;
      unsigned short int rw:1;
      };
    } descr;
  BYTE active;
  };


void Timer_Init(void);
void PWM_Init(void);
void UART_Init(DWORD);
void putsUART1(unsigned int *buffer);

int decodeKBD(int, long, BOOL);
BYTE GetValue(DWORD);
SWORD GetShortValue(DWORD);
DWORD GetIntValue(DWORD);
void PutValue(DWORD, BYTE);
BYTE InValue(BYTE);
void OutValue(BYTE, BYTE);
int Emulate(int);

#ifdef QL
#define RAM_START 0x20000
#define RAM_SIZE 0x20000            // 128KB standard
#define ROM_START 0x00000
#define ROM_SIZE 0x0c000
//#define ROM_SIZE2 0x04000
int UpdateScreen(SWORD,SWORD);
#elif MICHELEFABBRI
#define RAM_START 0x10000
#define RAM_SIZE 0x10000            // 
#define ROM_START 0x00000
#define ROM_SIZE 0x01000
int UpdateScreen(WORD);
#else
#define RAM_START 0x10000
#define RAM_SIZE 0x10000
#define ROM_START 0x0000
#define ROM_SIZE 0xc000
//#define ROM_SIZE2 0x4000
int UpdateScreen(WORD);
#endif


#define LED1 LATEbits.LATE2
#define LED2 LATEbits.LATE3
#define LED3 LATEbits.LATE4
#define SW1  PORTDbits.RD2
#define SW2  PORTDbits.RD3


// pcb SDRradio 2019
#define	SPISDITris 0		// niente qua
#define	SPISDOTris TRISGbits.TRISG8				// SDO
#define	SPISCKTris TRISGbits.TRISG6				// SCK
#define	SPICSTris  TRISGbits.TRISG7				// CS
#define	LCDDCTris  TRISEbits.TRISE7				// DC che su questo LCD è "A0" per motivi ignoti
//#define	LCDRSTTris TRISBbits.TRISB7
	
#define	m_SPISCKBit LATGbits.LATG6		// pin 
#define	m_SPISDOBit LATGbits.LATG8		// pin 
#define	m_SPISDIBit 0
#define	m_SPICSBit  LATGbits.LATG7		// pin 
#define	m_LCDDCBit  LATEbits.LATE7 		// pin 
//#define	m_LCDRSTBit LATBbits.LATB7 //FARE
//#define	m_LCDBLBit  LATBbits.LATB12

#endif

