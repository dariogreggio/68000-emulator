//https://www.google.com/url?sa=t&rct=j&q=&esrc=s&source=web&cd=&ved=2ahUKEwjc3b6Lwar2AhUPTcAKHRVnCmUQFnoECAMQAQ&url=http%3A%2F%2Fgoldencrystal.free.fr%2FM68kOpcodes-v2.3.pdf&usg=AOvVaw3a6qPsk5K_kpQd1MnlD07r
//https://www.google.com/url?sa=t&rct=j&q=&esrc=s&source=web&cd=&ved=2ahUKEwjc3b6Lwar2AhUPTcAKHRVnCmUQFnoECAUQAQ&url=https%3A%2F%2Fweb.njit.edu%2F~rosensta%2Fclasses%2Farchitecture%2F252software%2Fcode.pdf&usg=AOvVaw0awr9hRKXycE2-kghhbC3Y
//https://onlinedisassembler.com/odaweb/
//https://github.com/kstenerud/Musashi/tree/master

//#warning i fattori di AND EOR OR ADD SUB sono TUTTI invertiti! (corretto in ADD SUB)...
// in effetti credo che tutti questi siano da rivedere, le  IF DIRECTION... sono ridondanti 


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


//#pragma check_stack(off)
// #pragma check_pointer( off )
//#pragma intrinsic( _enable, _disable )

#undef MC68008
#undef MC68010
#undef MC68020

BYTE fExit=0;
BYTE debug=0;
//BYTE KBDataI,KBDataO,KBControl,/*KBStatus,*/ KBRAM[32];   // https://wiki.osdev.org/%228042%22_PS/2_Controller
//#define KBStatus KBRAM[0]   // pare...
#ifdef QL
BYTE Keyboard[3]={0,0,0};     // tasto, modifier, #tasti
#elif MACINTOSH
BYTE Keyboard[2]={0,0};     // tasto, modifier
#elif MICHELEFABBRI
#endif
volatile BYTE TIMIRQ=0,VIDIRQ=0,KBDIRQ=0,SERIRQ=0,RTCIRQ=0,MDVIRQ=0;
extern BYTE VIA1RegR[16],VIA1RegW[16],VIA1ShCnt;
extern BYTE IPCW,IPCR,IPCData,IPCCnt,IPCState;
extern SWORD VICRaster;

extern volatile BYTE keysFeedPtr;

BYTE DoReset=0,IPL=0,FC /*bus state*/=0,ActivateReset=0,DoStop=0,DoTrace=0,DoHalt=0;
struct ADDRESS_EXCEPTION AddressExcep,BusExcep /**/;
#define MAX_WATCHDOG 100      // x30mS v. sotto
WORD WDCnt=MAX_WATCHDOG;
BYTE ColdReset=1;
union PIPE Pipe2;


#define WORKING_REG_A (regsA[((Pipe1.b[0]) & 7)])
#define WORKING_REG_D (regsD[((Pipe1.b[0]) & 7)])
//#define ALL_REGS regsD
#define DEST_REG_A (regsA[((Pipe1.b[1] >> 1) & 7)])
#define DEST_REG_D (regsD[((Pipe1.b[1] >> 1) & 7)])
#define ADDRESSING (Pipe1.b[0] & 0b00111000)
#define ADDRESSING2 ((Pipe1.w & 0b0000000111000000) >> 3)      // per MOVE/MOVEA...
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
  PCIDXSUBADD =0b010,    // in 68020 ci sono dei modi aggiuntivi, con questi stessi "tipi" e non si capisce come si distinguano...
#ifdef MC68020
#endif
  PCDISPSUBADD=0b011,    // NB tutti i pc-indexed dovrebbero essere SOLO in lettura!
  IMMSUBADD   =0b100 
  };
#define OPERAND_SIZE (Pipe1.b[0] & 0b11000000)
#define	BYTE_SIZE 0x00
#define	WORD_SIZE 0x40
#define	DWORD_SIZE 0x80
#define	SIZE_ELSE 0xC0
#define DISPLACEMENT_REG (ALL_REGS[((Pipe2.b.h >> 4) /*& 15*/)])   // sia D che A (v.sopra)
#define DISPLACEMENT_SIZE (Pipe2.b.h & 0b00001000)
#ifdef MC68020
#define DISPLACEMENT_SCALE ((Pipe2.b.h & 0b00000110) >> 1)
#else
#define DISPLACEMENT_SCALE 0
#endif
#define ABSOLUTEADD_MODE (Pipe1.b[0] & 0b00000111)
#define ABSOLUTEADD_MODE2 ((Pipe1.b[1] & 0b00001110) >> 1)    // per MOVE/MOVEA
//#define OPERAND_SIZE2 (Pipe1.b[1] & 0b00110000)			// non usato, scelgo a monte
#define	BYTE_SIZE2 0x10     // coglioni ;)
#define	WORD_SIZE2 0x30
#define	DWORD_SIZE2 0x20
#define	DIRECTION (Pipe1.b[1] & 0b00000001)
#define ROTATE_DIRECTION (Pipe1.b[1] & 0b00000001)
#define OP_A_SIZE (Pipe1.b[1] & 0b00000001)
#define Q8BIT ((Pipe1.b[1] & 0b00001110) >> 1)
  
#define MOVEM_R2M()  if(!(Pipe1.b[0] & 0x40)) {\
    BYTE i; WORD j;\
    for(i=0,j=1; i<16; i++,j<<=1) {\
      if(res2.w & j) {\
        PutShortValue(res3.d, ALL_REGS[i].w.l);\
        res3.d+=2;\
        }\
      }\
    }\
  else {\
    BYTE i; WORD j;\
    for(i=0,j=1; i<16; i++,j<<=1) {\
      if(res2.w & j) {\
        PutIntValue(res3.d, ALL_REGS[i].d);\
        res3.d+=4;\
        }\
      }\
    }
#define MOVEM_R2M_PREDEC()  if(!(Pipe1.b[0] & 0x40)) {\
    signed char i; WORD j;\
    for(i=15,j=1; i>=0; i--,j<<=1) {\
      if(res2.w & j) {\
        res3.d-=2;\
        PutShortValue(res3.d, ALL_REGS[i].w.l);\
        }\
      }\
    }\
  else {\
    signed char i; WORD j;\
    for(i=15,j=1; i>=0; i--,j<<=1) {\
      if(res2.w & j) {\
        res3.d-=4;\
        PutIntValue(res3.d, ALL_REGS[i].d);\
        }\
      }\
    }
#define MOVEM_M2R()  if(!(Pipe1.b[0] & 0x40)) {\
    BYTE i; WORD j;\
    for(i=0,j=1; i<16; i++,j<<=1) {\
      if(res2.w & j) {\
        ALL_REGS[i].d = (int16_t)GetShortValue(res3.d);\
        res3.d+=2;\
        }\
      }\
    }\
  else {\
    BYTE i; WORD j;\
    for(i=0,j=1; i<16; i++,j<<=1) {\
      if(res2.w & j) {\
        ALL_REGS[i].d = GetIntValue(res3.d);\
        res3.d+=4;\
        }\
      }\
    }
// questo erano per potenziale bug in unsigned long long...
#define ADD64() {\
  _f.CCR.Carry=!!HIWORD((uint32_t)res1.w + (uint32_t)res2.w);\
  _f.CCR.Carry=!!HIWORD((res1.d>>16) + (res2.d>>16) + _f.CCR.Carry);\
  }
#define ADD64X() {\
  _f.CCR.Carry=!!HIWORD((uint32_t)res1.w + (uint32_t)res2.w + _f.CCR.Ext);\
  _f.CCR.Carry=!!HIWORD((res1.d>>16) + (res2.d>>16) + _f.CCR.Carry);\
  }
#define SUB64() {\
  _f.CCR.Carry=!!HIWORD((uint32_t)res1.w - (uint32_t)res2.w);\
  _f.CCR.Carry=!!HIWORD((res1.d>>16) - (res2.d>>16) - _f.CCR.Carry);\
  }
#define SUB64X() {\
  _f.CCR.Carry=!!HIWORD((uint32_t)res1.w - (uint32_t)res2.w - _f.CCR.Ext);\
  _f.CCR.Carry=!!HIWORD((res1.d>>16) - (res2.d>>16) - _f.CCR.Carry);\
  }
#define IS_A7() ((Pipe1.b[0] & 7) == 7)
#define GET8_PINC() GetValue(WORKING_REG_A.d);WORKING_REG_A.d += (IS_A7() ? 2 : 1);
  // qua non posso mettere le graffe... ma fare attenzione
#define GET16_PINC() GetShortValue(WORKING_REG_A.d);WORKING_REG_A.d+=2;
#define GET32_PINC() GetIntValue(WORKING_REG_A.d);WORKING_REG_A.d+=4;
#define PUT8_PINC() {PutValue(WORKING_REG_A.d,res3.b.l);WORKING_REG_A.d += (IS_A7() ? 2 : 1);}
#define PUT16_PINC() {PutShortValue(WORKING_REG_A.d,res3.w);WORKING_REG_A.d+=2;}
#define PUT32_PINC() {PutIntValue(WORKING_REG_A.d,res3.d);WORKING_REG_A.d+=4;}
#define GET8_PDEC() GetValue(WORKING_REG_A.d -= (IS_A7() ? 2 : 1))
#define GET16_PDEC() GetShortValue(WORKING_REG_A.d-=2)
#define GET32_PDEC() GetIntValue(WORKING_REG_A.d-=4)
#define PUT8_PDEC() PutValue(WORKING_REG_A.d -= (IS_A7() ? 2 : 1),res3.b.l)
#define PUT16_PDEC()  PutShortValue(WORKING_REG_A.d-=2,res3.w)
#define PUT32_PDEC()  PutIntValue(WORKING_REG_A.d-=4,res3.d)
#define DISP16() (WORKING_REG_A.d + (int16_t)Pipe2.w)
#define GET8_DISP16() GetValue(DISP16())
#define GET16_DISP16() GetShortValue(DISP16())
#define GET32_DISP16() GetIntValue(DISP16())
#define PUT8_DISP16() PutValue(DISP16(),res3.b.l)
#define PUT16_DISP16() PutShortValue(DISP16(),res3.w)
#define PUT32_DISP16() PutIntValue(DISP16(),res3.d)
#define DISPPC16() (_pc+(int16_t)Pipe2.w)
#define GET8_DISPPC16() GetValue(DISPPC16())
#define PUT8_DISPPC16() PutValue(DISPPC16(),res3.b.l)
#define GET16_DISPPC16() GetShortValue(DISPPC16())
#define PUT16_DISPPC16() PutShortValue(DISPPC16(),res3.w)
#define GET32_DISPPC16() GetIntValue(DISPPC16())
#define PUT32_DISPPC16() PutIntValue(DISPPC16(), res3.d)
#define DISP8S() (WORKING_REG_A.d + (((int16_t)DISPLACEMENT_REG.w.l)<<DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l)
#define DISP8L() (WORKING_REG_A.d + (((int32_t)DISPLACEMENT_REG.d)<<DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l)
#define GET8_DISP8S() GetValue(DISP8S())
#define GET8_DISP8L() GetValue(DISP8L())
#define GET16_DISP8S() GetShortValue(DISP8S())
#define GET16_DISP8L() GetShortValue(DISP8L())
#define GET32_DISP8S() GetIntValue(DISP8S())
#define GET32_DISP8L() GetIntValue(DISP8L())
#define PUT8_DISP8S() PutValue(DISP8S(),res3.b.l)
#define PUT8_DISP8L() PutValue(DISP8L(),res3.b.l)
#define PUT16_DISP8S() PutShortValue(DISP8S(),res3.w)
#define PUT16_DISP8L() PutShortValue(DISP8L(),res3.w)
#define PUT32_DISP8S() PutIntValue(DISP8S(),res3.d)
#define PUT32_DISP8L() PutIntValue(DISP8L(),res3.d)
#define DISPPC8S() (_pc + (((int16_t)DISPLACEMENT_REG.w.l)<<DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l)
#define DISPPC8L() (_pc + (((int32_t)DISPLACEMENT_REG.d)<<DISPLACEMENT_SCALE) + (signed char)Pipe2.b.l)
#define GET8_DISPPC8S() GetValue(DISPPC8S())
#define GET8_DISPPC8L() GetValue(DISPPC8L())
#define GET16_DISPPC8S() GetShortValue(DISPPC8S())
#define GET16_DISPPC8L() GetShortValue(DISPPC8L())
#define GET32_DISPPC8S() GetIntValue(DISPPC8S())
#define GET32_DISPPC8L() GetIntValue(DISPPC8L())
#define PUT8_DISPPC8S() PutValue(DISPPC8S(),res3.b.l)
#define PUT8_DISPPC8L() PutValue(DISPPC8L(),res3.b.l)
#define PUT16_DISPPC8S() PutShortValue(DISPPC8S(),res3.w)
#define PUT16_DISPPC8L() PutShortValue(DISPPC8L(),res3.w)
#define PUT32_DISPPC8S() PutIntValue(DISPPC8S(),res3.d)
#define PUT32_DISPPC8L() PutIntValue(DISPPC8L(),res3.d)

union WORD_BYTE GetPipe(DWORD);
int Emulate(int mode) {
  union WORD_BYTE Pipe1;      // usare variabili locali migliora codice... su PIC32
  // mettere qua anche pip2, spostare...
//	union A_REGISTERS regsA;   // questi SEGUONO i D (v. DISPLACEMENT_REG e anche MOVEM)
//  union D_REGISTERS regsD;
 	union REG ALL_REGS[16];
  union REG *regsD=&ALL_REGS[0];
	union REG *regsA=&ALL_REGS[8];   // con ottimizzazioni vengono invertiti... £$%@#
//	DWORD _sp=0;
#define _sp regsA[7].d    // A7 == SP, dice; gestisco 2 distinti a7, che swappo quando SR.Supervisor cambia
  DWORD a7S,a7U;
	DWORD _pc=0;
  
	union REGISTRO_F  _fIRQ;
	register union REGISTRO_F _f;
	union REGISTRO_F _fsup;
//	register SWORD i;
  register union RESULT res1,res2,res3;
  int c=0;


	_pc=0;
  
  
 
  
	do {

		c++;
#ifdef USING_SIMULATOR
#define VIDEO_DIVIDER 0x0000000f
#else
#define VIDEO_DIVIDER 0x00000fff
#endif
#if defined(QL) || defined(MACINTOSH)
		if(!(c & VIDEO_DIVIDER)) {     //~2uS (400 istruzioni) 2/12/24 Mac => 7mS
#else
		if(!(c & 0x0000ffff)) {
#endif
      ClrWdt();
// yield()
#ifdef QL
#ifndef USING_SIMULATOR
      UpdateScreen(VICRaster,VICRaster+8);      //15mS 4/1/23 e dura 0.8mS
#endif
      VICRaster+=8;     // 
        if(1 /*dov'è?? */)      // VELOCIZZO! altrimenti sballa i tempi (dovrebbe essere 50Hz/20mS)
          VIDIRQ=1;
      if(VICRaster>=256) {
        VICRaster=0;
        }
#elif MACINTOSH
#ifndef USING_SIMULATOR
      UpdateScreen(VICRaster,VICRaster+12);      //7mS 2/12/24 e dura ???mS
#endif
      VICRaster+=12;     // :3 in modalità non real_size, ok
      if(VICRaster>=VERT_SIZE-28 /* 28 dice */)
        VIA1RegR[0] |= 0b01000000;    // il VBlank dovrebbe durare 1.26mS, dice...
      if(VICRaster>=VERT_SIZE) {
        VIA1RegR[0xd] |= 0b00000010;
        if(VIA1RegW[0xe] & 0b00000010)
          VIDIRQ=1;
        VIA1RegR[0] &= ~0b01000000;    // VBlank 
        VICRaster=0;
        }

#else
#ifndef USING_SIMULATOR
			UpdateScreen(1);      // 50mS 17/11/22
#endif
#endif
      LED1^=1;    // 50mS~ con fabbri 17/11/22; 15mS QL 29/12/22; passo a 7mS Macintosh 2/12/24
      
      WDCnt--;
      if(!WDCnt) {
        WDCnt=MAX_WATCHDOG;
#ifdef QL
#elif MACINTOSH
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


    if(RTCIRQ) {      // bah non sembra esserci qua...
      if(0 /*bah non sembra esserci qua...*/)
        IPL=0b110;
#if !defined(QL) && !defined(MACINTOSH)
      LCDram[0x40+20+19]++;
#endif
      RTCIRQ=0;
      }
#if defined(QL)
    if(KBDIRQ || VIDIRQ || MDVIRQ /*|| TIMIRQ || SERIRQ || RTCIRQ*/) {
//    if(0) {
      IPL=0b010;    // 2, 
      }
    else {
      IPL=0;
      }
#elif defined(MACINTOSH)
    if(KBDIRQ || VIDIRQ || TIMIRQ || RTCIRQ) {
      IPL=0b001;    // tutti VIA=001
      }
    else if(SERIRQ) {
      IPL=0b010;    // SCC=010, 
      }
    // alcuni sembrano "sommati" e poi c'è "interrupt switch" che non so cosa sia.., v.
    else {
      IPL=0;
      }
#endif
#ifdef QL
#elif MACINTOSH
    if((VIA1RegR[0xb] & 0x40)) {   // 
      if(!MAKEWORD(VIA1RegR[4],VIA1RegR[5])) {
        if(VIA1RegW[0xe] & 0x40) {
          VIA1RegR[0xd] |= 0xc0;
          // o altro? TIMIRQ=1;
          }
        if(VIA1RegR[0xb] & 0x80) {
          // toggle PB7
          }
        VIA1RegR[4]=VIA1RegW[4];
        VIA1RegR[5]=VIA1RegW[5];
        }
      else {
        if(!VIA1RegR[4])
          VIA1RegR[5]--;
        VIA1RegR[4]--;
        }
      }
    if(!(VIA1RegR[0xb] & 0x20)) {   // finire, capire..
      if(!MAKEWORD(VIA1RegR[8],VIA1RegR[9])) {
        if(VIA1RegW[0xe] & 0x20) {
          VIA1RegR[0xd] |= 0xa0;
          // o altro? TIMIRQ=1;
          }
        VIA1RegR[8]=VIA1RegW[8];
        VIA1RegR[9]=VIA1RegW[9];
        }
      else {
        if(!VIA1RegR[8])
          VIA1RegR[9]--;
        VIA1RegR[8]--;
        }
      }
		switch(VIA1RegR[0xb] & 0b00011100) {
      case 0 << 2:   // disabled
        break;
      case 1 << 2:   // shift in, usa Timer2
    		VIA1RegR[0xa] <<= 1;
        goto updViaShReg;
        break;
      case 2 << 2:   // shift in, usa Clock
    		VIA1RegR[0xa] <<= 1;
        goto updViaShReg;
        break;
      case 3 << 2:   // shift in, usa Ext clock
    		VIA1RegR[0xa] <<= 1;
        goto updViaShReg;
        break;
      case 4 << 2:   // shift out, usa Timer2, free running
    		VIA1RegW[0xa] <<= 1;
        goto updViaShReg;
        break;
      case 5 << 2:   // shift out, usa Timer2
    		VIA1RegW[0xa] <<= 1;
        goto updViaShReg;
        break;
      case 6 << 2:   // shift out, usa Clock
    		VIA1RegW[0xa] <<= 1;
        goto updViaShReg;
        break;
      case 7 << 2:   // shift out, usa Ext clock
    		VIA1RegW[0xa] <<= 1;
        
updViaShReg:
        VIA1ShCnt++;
        if(VIA1ShCnt>=8) {
          if(VIA1RegW[0xe] & 0x04) {
            VIA1RegR[0xd] |= 0x84;
          //  VIAIRQ=1;
            }
          VIA1ShCnt=0;
          }
        break;
      }
#elif MICHELEFABBRI
#else
    if(!(IOPortI & 1)) {
//      DoNMI=1;
      }
#endif
    
    
		if(debug) {
//			printf("%04x    %02x\n",_pc,GetValue(_pc));
			}

		if(DoReset) {
      IPL=0b000;  // 
      FC=0b110; // supervisor mode, program
      _f.SR.w=0b0010011100000000;   // supervisor, IPL=7, area programma VERIFICARE
			_pc=GetIntValue(0x00000004);
			_sp=GetIntValue(0x00000000);   // bah per sicurezza li imposto entrambi :D NO!
      a7S=_sp;
			DoReset=DoStop=DoHalt=0;
      ActivateReset=128;    // così facciamo tutto :)
			}
		if(IPL > _f.SR.IRQMask || IPL==7) {     // https://www.cs.mcgill.ca/~cs573/fall2002/notes/lec273/lecture21/21_2.htm
      // PERO' dice anche che 7 NON PUO' essere ignorato...
      FC=0b111;   // ACK irq...
      _fIRQ=_f;
      if(!_f.SR.Supervisor)
        a7U=regsA[7].d;
      else
        a7S=regsA[7].d;
      _f.SR.Supervisor=1;     // non mi è chiaro se sono nested o no...
      regsA[7].d=a7S;
      _f.SR.Trace=0;   // supervisor, area programma VERIFICARE solo >68020?
#ifdef MC68020
//      _f.SR.MasterInterrupt=1;   // supervisor, area programma VERIFICARE solo >68020?
#endif
      _f.SR.IRQMask=IPL;
      DoStop=0;
      _sp-=4;
      PutIntValue(_sp,_pc); // low word prima
      _sp-=2;
      PutShortValue(_sp,_fIRQ.w);
        
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
#ifdef QL
      _pc=GetIntValue(/* 24*4 */ 0x00000060+IPL*4);   // ovviamente facciam b ;)
#elif MACINTOSH
      GetIntValue(0x00fffff0+IPL*2+1);   // serve solo a "gestire" (v.)
      _pc=GetIntValue(/* 24*4 */ 0x00000060+IPL*4);   // ovviamente facciam b ;)
      /* 64=VIA
       * 68 SCC
       * 6C VIA+SCC (temp
       * 70 Interrupt switch
       * 74 Interrupt switch + VIA
       * 78 Interrupt switch + SCC
       * 7C Interrupt switch + VIA+SCC
       * */
#endif
// si potrebbe anche    res3.b.l=0x18+IPL;  goto doTrap;
      FC=0b110; // supervisor mode, program
			}

		if(DoStop)
			continue;		// esegue cmq IRQ FORSE qua
//		if(DoHalt)
//			continue;		// boh 

    if(ActivateReset) {
      // abbassare pin HW...
      ActivateReset--;
      if(!ActivateReset) {
        
        TIMIRQ=VIDIRQ=KBDIRQ=SERIRQ=RTCIRQ=MDVIRQ=0;
#ifdef QL
        IPCW=255; IPCR=0; IPCData=IPCState=0; IPCCnt=4;
#elif MACINTOSH
        
#endif
        Keyboard[0] = Keyboard[1] = Keyboard[2] = 0x0;

        // e alzare
        }
      }
    if(AddressExcep.active) {    // 
isExcep:
      FC=0b110;   // supervisor mode, program
      _fsup=_f;
      if(!_f.SR.Supervisor)
        a7U=regsA[7].d;
      else
        a7S=regsA[7].d;
      _f.SR.Supervisor=1;
      regsA[7].d=a7S;
      _sp-=4;
      PutIntValue(_sp,_pc); // low word prima
      _sp-=2;
      PutShortValue(_sp,_fsup.w);
      _sp-=2;
      PutIntValue(_sp,Pipe1);     // EXCEPTION STACK FRAME B.2
      _sp-=4;
      PutIntValue(_sp,AddressExcep.addr); // 
      _sp-=2;
      PutIntValue(_sp,AddressExcep.descr.w);
      // nello stack vanno messi (sempre dal basso verso l'alto); altri excep sono diverse (e 68020 dipiù)
      //15  5  4    3    2  0
//            R/W  I/N  FUNCTION CODE 
      // ACCESS ADDRESS HIGH 
      // ACCESS ADDRESS LOW
      // e poi status/PC come solito

      AddressExcep.active=0;
      printf("68K memory exception %X\r\n",_pc);
      
      _pc=GetIntValue(3*4);
      }
    if(BusExcep.active) {    // mettere
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
    
    
      if(!SW1) {        // test tastiera, v.semaforo di là
       // continue;
        __delay_ms(100); ClrWdt();
        DoReset=1;
        }
      if(!SW2) {        // test tastiera
        if(keysFeedPtr==255)      // debounce...
          keysFeedPtr=254;
        }

      LED2^=1;    // ~1.5uS 4/1/23 O0 (oscilla tra 1 e 2); con O3 (thx RKarnik) tra 500 e 1us 7/1/23

    
/*      if(_pc >= 0x4000) {
        ClrWdt();
        }*/
  
  
    if(_f.SR.Trace /* && !IRQ ... */)
      DoTrace=1;
      
		Pipe1=GetPipe(_pc); _pc+=2;
    if(AddressExcep.active)     // bah serve davvero?? (Macintosh 2024
      goto isExcep;
		switch(Pipe1.b[1]) {
			case 0x00:   // ORI
        switch(ADDRESSING) {
          case ABSOLUTEADD:
            switch(Pipe1.b[0]) {
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
                  a7S=regsA[7].d;
                  _f.SR.w=res3.w;
                  if(!_f.SR.Supervisor)
                    regsA[7].d=a7U;
                  else
                    regsA[7].d=a7S;
	                goto noAggFlag;
                  }
                else {
doPrivilegeTrap:
                  res3.b.l=8;
                  goto doTrap;
                  }
	              break;
              default:
                switch(ABSOLUTEADD_MODE) {
                  case ABSSSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res3.b.l = Pipe2.b.l;
                        _pc+=AdvPipe(_pc,2);
                        res3.b.l |= GetValue((int16_t)Pipe2.w);
                        PutValue((int16_t)Pipe2.w, res3.b.l);
                        break;
                      case WORD_SIZE:
                        res3.w = Pipe2.w;
                        _pc+=AdvPipe(_pc,2);
                        res3.w |= GetShortValue((int16_t)Pipe2.w);
                        PutShortValue((int16_t)Pipe2.w, res3.w);
                        break;
                      case DWORD_SIZE:
                        res3.d = Pipe2.d;
                        _pc+=AdvPipe(_pc,4);
                        res3.d |= GetIntValue((int16_t)Pipe2.w);
                        PutIntValue((int16_t)Pipe2.w, res3.d);
                        break;
                      }
                    _pc+=2;
                    break;
                  case ABSLSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res3.b.l = Pipe2.b.l;
                        _pc+=AdvPipe(_pc,2);
                        res3.b.l |= GetValue(Pipe2.d);
                        PutValue(Pipe2.d,res3.b.l);
                        break;
                      case WORD_SIZE:
                        res3.w = Pipe2.w;
                        _pc+=AdvPipe(_pc,2);
                        res3.w |= GetShortValue(Pipe2.d);
                        PutShortValue(Pipe2.d,res3.w);
                        break;
                      case DWORD_SIZE:
                        res3.d = Pipe2.d;
                        _pc+=AdvPipe(_pc,4);
                        res3.d |= GetIntValue(Pipe2.d); 
                        PutIntValue(Pipe2.d,res3.d);
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
                res3.b.l = WORKING_REG_D.b.b0 |= Pipe2.b.l;
								_pc+=2;
                break;
              case WORD_SIZE:
                res3.w = WORKING_REG_D.w.l |= Pipe2.w;
								_pc+=2;
                break;
              case DWORD_SIZE:
                res3.d = WORKING_REG_D.d |= Pipe2.d;
								_pc+=4;
                break;
              }
            break;
          case ADDRREGADD:   // An NO!
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GetValue(WORKING_REG_A.d) | Pipe2.b.l;
                PutValue(WORKING_REG_A.d, res3.b.l);
								_pc+=2;
                break;
              case WORD_SIZE:
                res3.w = GetShortValue(WORKING_REG_A.d) | Pipe2.w;
                PutShortValue(WORKING_REG_A.d, res3.w);
								_pc+=2;
                break;
              case DWORD_SIZE:
                res3.d = GetIntValue(WORKING_REG_A.d) | Pipe2.d;
                PutIntValue(WORKING_REG_A.d, res3.d);
								_pc+=4;
                break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GetValue(WORKING_REG_A.d) | Pipe2.b.l;
                PUT8_PINC();
								_pc+=2;
                break;
              case WORD_SIZE:
                res3.w = GetShortValue(WORKING_REG_A.d) | Pipe2.w;
                PUT16_PINC();
								_pc+=2;
                break;
              case DWORD_SIZE:
                res3.d = GetIntValue(WORKING_REG_A.d) | Pipe2.d;
                PUT32_PINC();
								_pc+=4;
                break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GET8_PDEC() | Pipe2.b.l;
                PutValue(WORKING_REG_A.d, res3.b.l);
								_pc+=2;
                break;
              case WORD_SIZE:
                res3.w = GET16_PDEC() | Pipe2.w;
                PutShortValue(WORKING_REG_A.d, res3.w);
								_pc+=2;
                break;
              case DWORD_SIZE:
                res3.d = GET32_PDEC() | Pipe2.d;
                PutIntValue(WORKING_REG_A.d, res3.d);
								_pc+=4;
                break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = Pipe2.b.l;
                _pc+=AdvPipe(_pc,2);
                res3.b.l |= GET8_DISP16();
                PUT8_DISP16();
                break;
              case WORD_SIZE:
                res3.w = Pipe2.w;
                _pc+=AdvPipe(_pc,2);
                res3.w |= GET16_DISP16();
                PUT16_DISP16();
                break;
              case DWORD_SIZE:
                res3.d = Pipe2.d;
                _pc+=AdvPipe(_pc,4);
                res3.d |= GET32_DISP16();
                PUT32_DISP16();
                break;
              }
						_pc+=2;
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = Pipe2.b.l;
                _pc+=AdvPipe(_pc,2);
                if(!DISPLACEMENT_SIZE) {
                  res3.b.l |= GET8_DISP8S();
									PUT8_DISP8S();
                  }
                else {
                  res3.b.l |= GET8_DISP8L();
                  PUT8_DISP8L();
                  }
                break;
              case WORD_SIZE:
                res3.w = Pipe2.w;
                _pc+=AdvPipe(_pc,2);
                if(!DISPLACEMENT_SIZE) {
                  res3.w |= GET16_DISP8S();
                  PUT16_DISP8S();
                  }
                else {
                  res3.w |= GET16_DISP8L();
                  PUT16_DISP8L();
                  }
                break;
              case DWORD_SIZE:
                res3.d = Pipe2.d;
                _pc+=AdvPipe(_pc,4);
                if(!DISPLACEMENT_SIZE) {
                  res3.d |= GET32_DISP8S();
                  PUT32_DISP8S();
                  }
                else {
                  res3.d |= GET32_DISP8L();
                  PUT32_DISP8L();
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
            _f.CCR.Sign=!!(res3.w & 0x8000);    // non cambia rispetto a accedere al byte..
            break;
          case DWORD_SIZE:
aggFlag4:
            _f.CCR.Zero=!res3.d;
            _f.CCR.Sign=!!(res3.d & 0x80000000);
            break;
          }
        _f.CCR.Ovf=_f.CCR.Carry=0;
noAggFlag:
				break;

			case 0x08:   // BTST BCHG BCLR BSET
        res2.b.l=Pipe2.b.l & 31;  // max 7 opp 31 (v. doBit
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
        if(ADDRESSING == ADDRREGADD) {      // MOVEP 
          if(Pipe1.b[0] & 0b10000000) {    // Dx -> Mem
            if(Pipe1.b[0] & 0b01000000) {    // 32bit
              PutValue(DISP16(),DEST_REG_D.b.b3);
              PutValue(DISP16()+2,DEST_REG_D.b.b2);
              PutValue(DISP16()+4,DEST_REG_D.b.b1);
              PutValue(DISP16()+6,DEST_REG_D.b.b0);
              }
            else {                            // 16bit
              PutValue(DISP16(),DEST_REG_D.b.b1);
              PutValue(DISP16()+2,DEST_REG_D.b.b0);
              }
            }
          else {                              // Mem -> Dx
            if(Pipe1.b[0] & 0b01000000) {    // 32bit
              DEST_REG_D.b.b3 = GetValue(DISP16()+0);
              DEST_REG_D.b.b2 = GetValue(DISP16()+2);
              DEST_REG_D.b.b1 = GetValue(DISP16()+4);
              DEST_REG_D.b.b0 = GetValue(DISP16()+6);
              }
            else {                            // 16bit
              DEST_REG_D.b.b1 = GET8_DISP16();
              DEST_REG_D.b.b0 = GetValue(DISP16()+2);
              }
            }
          _pc+=2;
          }
        else {
          res2.b.l=DEST_REG_D.b.b0 & 0x1f;       // max 32bit, solo con registri Dn
          
doBit:
          switch(ADDRESSING) {      // BTST ecc
            case ABSOLUTEADD:
              switch(ABSOLUTEADD_MODE) {
                case ABSSSUBADD:
                  res1.b.l = GetValue((int16_t)Pipe2.w);
                  _pc+=2;
                  break;
                case ABSLSUBADD:
                  res1.b.l = GetValue(Pipe2.d);
                  _pc+=4;
                  break;
                case PCIDXSUBADD:   // (D16,PC) SOLO BTST!!
                  // quindi filtrarli o ce ne sbattiamo??
                  res1.b.l = GET8_DISPPC16();
                  _pc+=2;
                  break;
                case PCDISPSUBADD:   // (D8,Xn,PC) SOLO BTST!!
                  if(!DISPLACEMENT_SIZE)
                    res1.b.l = GET8_DISPPC8S();
                  else
                    res1.b.l = GET8_DISPPC8L();
                  _pc+=2;
                  break;
                case IMMSUBADD:			// mah.. pare di sì... ma solo con numero bit statico
                  res1.b.l = Pipe2.b.l;
                  _pc+=2;
                  break;
                }
              break;
            case DATAREGADD:   // Dn
              res1.d = WORKING_REG_D.d;			// se Dn allora 32bit!
              break;
            case ADDRREGADD:   // qua no!
              break;
            case ADDRADD:   // (An)
              res1.b.l = GetValue(WORKING_REG_A.d);
              break;
            case ADDRPINCADD:   // (An)+
              res1.b.l = GET8_PINC();   // OCCHIO se NON BTST!! v. sotto
              break;
            case ADDRPDECADD:   // -(An)
              res1.b.l = GET8_PDEC();
              break;
            case ADDRDISPADD:   // (D16,An)
              res1.b.l = GET8_DISP16();
              _pc+=2;
              break;
            case ADDRIDXADD:   // (D8,An,Xn)
              if(!DISPLACEMENT_SIZE)
                res1.b.l = GET8_DISP8S();
              else
                res1.b.l = GET8_DISP8L();
              _pc+=2;
              break;
            }
          if(ADDRESSING == DATAREGADD /*Pipe1.b[1] != 0x08*/) {    // 32bit, solo con registro Dn
            res3.d = res1.d & (1 << res2.b.l/*& 31*/);
            _f.CCR.Zero = !res3.d;
            }
          else {                         // opp. 8bit
            res3.b.l = res1.b.l & (1 << res2.b.l/*& 7*/);
            _f.CCR.Zero = !res3.b.l;
            }
          switch(Pipe1.b[0] & 0b11000000) {
            case 0x00:    // BTST
              goto doBit2;
              break;
            case 0x40:    // BCHG
              if(ADDRESSING == DATAREGADD)     // 32bit, solo con registro Dn
                res3.d = res1.d ^ (1 << res2.b.l);
              else                              // opp. 8bit
                res3.b.l = res1.b.l ^ (1 << res2.b.l);
              break;
            case 0x80:    // BCLR
              if(ADDRESSING == DATAREGADD)     // 32bit, solo con registro Dn
                res3.d = res1.d & ~(1 << res2.b.l);
              else                              // opp. 8bit
                res3.b.l = res1.b.l & ~(1 << res2.b.l);
              break;
            case 0xc0:    // BSET
              if(ADDRESSING == DATAREGADD)     // 32bit, solo con registro Dn
                res3.d = res1.d | (1 << res2.b.l);
              else                              // opp. 8bit
                res3.b.l = res1.b.l | (1 << res2.b.l);
              break;
            }
          switch(ADDRESSING) {
            case ABSOLUTEADD:
              switch(ABSOLUTEADD_MODE) {
                case ABSSSUBADD:
                  PutValue((int16_t)Pipe2.w, res3.b.l);
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
            case DATAREGADD:   // Dn 32bit 
              WORKING_REG_D.d = res3.d;
              break;
            case ADDRADD:   // (An)
              PutValue(WORKING_REG_A.d, res3.b.l);
              break;
            case ADDRPINCADD:   // (An)+
              PutValue(WORKING_REG_A.d - (IS_A7() ? 2 : 1), res3.b.l);      // OCCHIO! v. sopra
              break;
            case ADDRPDECADD:   // -(An)
              PutValue(WORKING_REG_A.d, res3.b.l);
              break;
            case ADDRDISPADD:   // (D16,An)
              PUT8_DISP16();
              break;
            case ADDRIDXADD:   // (D8,An,Xn)
              if(!DISPLACEMENT_SIZE)
                PUT8_DISP8S();
              else
                PUT8_DISP8L();
              break;
            }
          }
doBit2:   ;
				break;
        
			case 0x02:   // ANDI
        switch(ADDRESSING) {
          case ABSOLUTEADD:    // 
            switch(Pipe1.b[0]) {
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
                  goto doPrivilegeTrap;
                  }
                break;
              default:
                switch(ABSOLUTEADD_MODE) {
                  case ABSSSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res3.b.l = Pipe2.b.l;
                        _pc+=AdvPipe(_pc,2);
                        res3.b.l &= GetValue((int16_t)Pipe2.w);
                        PutValue((int16_t)Pipe2.w, res3.b.l);
                        break;
                      case WORD_SIZE:
                        res3.w = Pipe2.w;
                        _pc+=AdvPipe(_pc,2);
                        res3.w &= GetShortValue((int16_t)Pipe2.w);
                        PutShortValue((int16_t)Pipe2.w, res3.w);
                        break;
                      case DWORD_SIZE:
                        res3.d = Pipe2.d;
                        _pc+=AdvPipe(_pc,4);
                        res3.d &= GetIntValue((int16_t)Pipe2.w);
                        PutIntValue((int16_t)Pipe2.w, res3.d);
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
                        res3.d = Pipe2.d;
                        _pc+=AdvPipe(_pc,4);
                        res3.d &= GetIntValue(Pipe2.d); 
                        PutIntValue(Pipe2.d,res3.d);
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
                res3.d = WORKING_REG_D.d &= Pipe2.d;
								_pc+=4;
                break;
              }
            break;
          case ADDRREGADD:   // An NO!
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GetValue(WORKING_REG_A.d) & Pipe2.b.l;
                PutValue(WORKING_REG_A.d, res3.b.l);
								_pc+=2;
                break;
              case WORD_SIZE:
                res3.w = GetShortValue(WORKING_REG_A.d) & Pipe2.w;
                PutShortValue(WORKING_REG_A.d, res3.w);
								_pc+=2;
                break;
              case DWORD_SIZE:
                res3.d = GetIntValue(WORKING_REG_A.d) & Pipe2.d;
                PutIntValue(WORKING_REG_A.d, res3.d);
								_pc+=4;
                break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GetValue(WORKING_REG_A.d) & Pipe2.b.l;
                PUT8_PINC();
								_pc+=2;
                break;
              case WORD_SIZE:
                res3.w = GetShortValue(WORKING_REG_A.d) & Pipe2.w;
                PUT16_PINC();
								_pc+=2;
                break;
              case DWORD_SIZE:
                res3.d = GetIntValue(WORKING_REG_A.d) & Pipe2.d;
                PUT32_PINC();
								_pc+=4;
                break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GET8_PDEC() & Pipe2.b.l;
                PutValue(WORKING_REG_A.d, res3.b.l);
								_pc+=2;
                break;
              case WORD_SIZE:
                res3.w = GET16_PDEC() & Pipe2.w;
                PutShortValue(WORKING_REG_A.d, res3.w);
								_pc+=2;
                break;
              case DWORD_SIZE:
                res3.d = GET32_PDEC() & Pipe2.d;
                PutIntValue(WORKING_REG_A.d, res3.d);
								_pc+=4;
                break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = Pipe2.b.l;
                _pc+=AdvPipe(_pc,2);
                res3.b.l &= GET8_DISP16();
                PUT8_DISP16();
                break;
              case WORD_SIZE:
                res3.w = Pipe2.w;
                _pc+=AdvPipe(_pc,2);
                res3.w &= GET16_DISP16();
                PUT16_DISP16();
                break;
              case DWORD_SIZE:
                res3.d = Pipe2.d;
                _pc+=AdvPipe(_pc,4);
                res3.d &= GET32_DISP16();
                PUT32_DISP16();
                break;
              }
						_pc+=2;
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = Pipe2.b.l;
                _pc+=AdvPipe(_pc,2);
                if(!DISPLACEMENT_SIZE) {
                  res3.b.l &= GET8_DISP8S();
                  PUT8_DISP8S();
                  }
                else {
                  res3.b.l &= GET8_DISP8L();
                  PUT8_DISP8L();
                  }
                break;
              case WORD_SIZE:
                res3.w = Pipe2.w;
                _pc+=AdvPipe(_pc,2);
                if(!DISPLACEMENT_SIZE) {
                  res3.w &= GET16_DISP8S();
                  PUT16_DISP8S();
                  }
                else {
                  res3.w &= GET16_DISP8L();
                  PUT16_DISP8L();
                  }
                break;
              case DWORD_SIZE:
                res3.d = Pipe2.d;
                _pc+=AdvPipe(_pc,4);
                if(!DISPLACEMENT_SIZE) {
                  res3.d &= GET32_DISP8S();
                  PUT32_DISP8S();
                  }
                else {
                  res3.d &= GET32_DISP8L();
                  PUT32_DISP8L();
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
            switch(ABSOLUTEADD_MODE) {
              case ABSSSUBADD:
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = Pipe2.b.l;
                    _pc+=AdvPipe(_pc,2);
                    res1.b.l = GetValue((int16_t)Pipe2.w);
										res3.b.l = res1.b.l - res2.b.l;
                    PutValue((int16_t)Pipe2.w, res3.b.l);
                    break;
                  case WORD_SIZE:
                    res2.w = Pipe2.w;
                    _pc+=AdvPipe(_pc,2);
                    res1.w = GetShortValue((int16_t)Pipe2.w);
										res3.w = res1.w - res2.w;
                    PutShortValue((int16_t)Pipe2.w, res3.w);
                    break;
                  case DWORD_SIZE:
                    res2.d = Pipe2.d;
                    _pc+=AdvPipe(_pc,4);
                    res1.d = GetIntValue((int16_t)Pipe2.w);
										res3.d = res1.d - res2.d;
                    PutIntValue((int16_t)Pipe2.w, res3.d);
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
										res3.b.l = res1.b.l - res2.b.l;
                    PutValue(Pipe2.d,res3.b.l);
                    break;
                  case WORD_SIZE:
                    res2.w = Pipe2.w;
                    _pc+=AdvPipe(_pc,2);
                    res1.w = GetShortValue(Pipe2.d);
										res3.w = res1.w - res2.w;
                    PutShortValue(Pipe2.d,res3.w);
                    break;
                  case DWORD_SIZE:
                    res2.d = Pipe2.d;
                    _pc+=AdvPipe(_pc,4);
                    res1.d = GetIntValue(Pipe2.d); 
										res3.d = res1.d - res2.d;
                    PutIntValue(Pipe2.d,res3.d);
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
                res3.b.l = WORKING_REG_D.b.b0 = res1.b.l - res2.b.l;
		            _pc+=2;
                break;
              case WORD_SIZE:
                res1.w = WORKING_REG_D.w.l;
                res2.w = Pipe2.w;
                res3.w = WORKING_REG_D.w.l = res1.w - res2.w;
		            _pc+=2;
                break;
              case DWORD_SIZE:
                res1.d = WORKING_REG_D.d;
                res2.d = Pipe2.d;
                res3.d = WORKING_REG_D.d = res1.d - res2.d;
		            _pc+=4;
                break;
              }
            break;
          case ADDRREGADD:   // An
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res1.b.l = GetValue(WORKING_REG_A.d);
                res2.b.l = Pipe2.b.l;
                res3.b.l = res1.b.l - res2.b.l;
                PutValue(WORKING_REG_A.d, res3.b.l);
		            _pc+=2;
                break;
              case WORD_SIZE:
                res1.w = GetShortValue(WORKING_REG_A.d);
                res2.w = Pipe2.w;
                res3.w = res1.w - res2.w;
                PutShortValue(WORKING_REG_A.d, res3.w);
		            _pc+=2;
                break;
              case DWORD_SIZE:
                res1.d = GetIntValue(WORKING_REG_A.d);
                res2.d = Pipe2.d;
                res3.d = res1.d - res2.d;
                PutIntValue(WORKING_REG_A.d, res3.d);
		            _pc+=4;
                break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res1.b.l = GetValue(WORKING_REG_A.d);
                res2.b.l = Pipe2.b.l;
                res3.b.l = res1.b.l - res2.b.l;
                PUT8_PINC();
		            _pc+=2;
                break;
              case WORD_SIZE:
                res1.w = GetShortValue(WORKING_REG_A.d);
                res2.w = Pipe2.w;
                res3.w = res1.w - res2.w;
                PUT16_PINC();
		            _pc+=2;
                break;
              case DWORD_SIZE:
                res1.d = GetIntValue(WORKING_REG_A.d);
                res2.d = Pipe2.d;
                res3.d = res1.d - res2.d;
                PUT32_PINC();
		            _pc+=4;
                break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res1.b.l = GET8_PDEC();
                res2.b.l = Pipe2.b.l;
                res3.b.l = res1.b.l - res2.b.l;
                PutValue(WORKING_REG_A.d, res3.b.l);
		            _pc+=2;
                break;
              case WORD_SIZE:
                res1.w = GET16_PDEC();
                res2.w = Pipe2.w;
                res3.w = res1.w - res2.w;
                PutShortValue(WORKING_REG_A.d, res3.w);
		            _pc+=2;
                break;
              case DWORD_SIZE:
                res1.d = GET32_PDEC();
                res2.d = Pipe2.d;
                res3.d = res1.d - res2.d;
                PutIntValue(WORKING_REG_A.d, res3.d);
		            _pc+=4;
                break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res2.b.l = Pipe2.b.l;
                _pc+=AdvPipe(_pc,2);
                res1.b.l = GET8_DISP16();
                res3.b.l = res1.b.l - res2.b.l;
                PUT8_DISP16();
                break;
              case WORD_SIZE:
                res2.w = Pipe2.w;
                _pc+=AdvPipe(_pc,2);
                res1.w = GET16_DISP16();
                res3.w = res1.w - res2.w;
                PUT16_DISP16();
                break;
              case DWORD_SIZE:
                res2.d = Pipe2.d;
                _pc+=AdvPipe(_pc,4);
                res1.d = GET32_DISP16();
                res3.d = res1.d - res2.d;
                PUT32_DISP16();
                break;
              }
						_pc+=2;
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res2.b.l = Pipe2.b.l;
                _pc+=AdvPipe(_pc,2);
                if(!DISPLACEMENT_SIZE) {
                  res1.b.l = GET8_DISP8S();
	                res3.b.l = res1.b.l - res2.b.l;
                  PUT8_DISP8S();
                  }
                else {
                  res1.b.l = GET8_DISP8L();
	                res3.b.l = res1.b.l - res2.b.l;
                  PUT8_DISP8L();
                  }
                break;
              case WORD_SIZE:
                res2.w = Pipe2.w;
                _pc+=AdvPipe(_pc,2);
                if(!DISPLACEMENT_SIZE) {
                  res1.w = GET16_DISP8S();
	                res3.w = res1.w - res2.w;
                  PUT16_DISP8S();
                  }
                else {
                  res1.w = PUT16_DISP8S();
	                res3.w = res1.w - res2.w;
                  PUT16_DISP8L();
                  }
                break;
              case DWORD_SIZE:
                res2.d = Pipe2.d;
                _pc+=AdvPipe(_pc,4);
                if(!DISPLACEMENT_SIZE) {
                  res1.d = GET32_DISP8S();
                  res3.d = res1.d - res2.d;
	                PUT32_DISP8S();
                  }
                else {
                  res1.d = GET32_DISP8L();
	                res3.d = res1.d - res2.d;
                  PUT32_DISP8L();
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
    				_f.CCR.Carry= !!(((res2.b.l & 0x80) & (~res1.b.l & 0x80)) | ((res3.b.l & 0x80) &
        			(~res1.b.l & 0x80)) | ((res2.b.l & 0x80) & (res3.b.l & 0x80)));
    				_f.CCR.Ovf= !!(((~res2.b.l & 0x80) & (res1.b.l & 0x80) & (~res3.b.l & 0x80)) |
        			((res2.b.l & 0x80) & (~res1.b.l & 0x80) & (res3.b.l & 0x80)));
            break;
            // unire i fattori e poi fare AND non migliora a 32bit e peggiora a 8/16...
          case WORD_SIZE:
            _f.CCR.Zero=!res3.w;
            _f.CCR.Sign=!!(res3.w & 0x8000);
    				_f.CCR.Carry= !!(((res2.w & 0x8000) & (~res1.w & 0x8000)) | ((res3.w & 0x8000) &
        			(~res1.w & 0x8000)) | ((res2.w & 0x8000) & (res3.w & 0x8000)));
    				_f.CCR.Ovf= !!(((~res2.w & 0x8000) & (res1.w & 0x8000) & (~res3.w & 0x8000)) |
        			((res2.w & 0x8000) & (~res1.w & 0x8000) & (res3.w & 0x8000)));
            break;
          case DWORD_SIZE:
            _f.CCR.Zero=!res3.d;
            _f.CCR.Sign=!!(res3.d & 0x80000000);
    				_f.CCR.Carry= !!(((res2.d & 0x80000000) & (~res1.d & 0x80000000)) | ((res3.d & 0x80000000) &
        			(~res1.d & 0x80000000)) | ((res2.d & 0x80000000) & (res3.d & 0x80000000)));
    				_f.CCR.Ovf= !!(((~res2.d & 0x80000000) & (res1.d & 0x80000000) & (~res3.d & 0x80000000)) |
        			((res2.d & 0x80000000) & (~res1.d & 0x80000000) & (res3.d & 0x80000000)));
            break;
          }
        _f.CCR.Ext=_f.CCR.Carry;
				break;

			case 0x06:   // ADDI (RTM)
        switch(ADDRESSING) {
          case ABSOLUTEADD:
            switch(ABSOLUTEADD_MODE) {
              case ABSSSUBADD:
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = Pipe2.b.l;
                    _pc+=AdvPipe(_pc,2);
                    res1.b.l = GetValue((int16_t)Pipe2.w);
										res3.b.l = res1.b.l + res2.b.l;
                    PutValue((int16_t)Pipe2.w,res3.b.l);
                    break;
                  case WORD_SIZE:
                    res2.w = Pipe2.w;
                    _pc+=AdvPipe(_pc,2);
                    res1.w = GetShortValue((int16_t)Pipe2.w);
										res3.w = res1.w + res2.w;
                    PutShortValue((int16_t)Pipe2.w,res3.w);
                    break;
                  case DWORD_SIZE:
                    res2.d = Pipe2.d;
                    _pc+=AdvPipe(_pc,4);
                    res1.d = GetIntValue((int16_t)Pipe2.w);
										res3.d = res1.d + res2.d;
                    PutIntValue((int16_t)Pipe2.w,res3.d);
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
										res3.b.l = res1.b.l + res2.b.l;
                    PutValue(Pipe2.d,res3.b.l);
                    break;
                  case WORD_SIZE:
                    res2.w = Pipe2.w;
                    _pc+=AdvPipe(_pc,2);
                    res1.w = GetShortValue(Pipe2.d);
										res3.w = res1.w + res2.w;
                    PutShortValue(Pipe2.d,res3.w);
                    break;
                  case DWORD_SIZE:
                    res2.d = Pipe2.d;
                    _pc+=AdvPipe(_pc,4);
                    res1.d = GetIntValue(Pipe2.d); 
										res3.d = res1.d + res2.d;
                    PutIntValue(Pipe2.d,res3.d);
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
                res3.b.l = WORKING_REG_D.b.b0 = res1.b.l + res2.b.l;
		            _pc+=2;
                break;
              case WORD_SIZE:
                res1.w = WORKING_REG_D.w.l;
                res2.w = Pipe2.w;
                res3.w = WORKING_REG_D.w.l = res1.w + res2.w;
		            _pc+=2;
                break;
              case DWORD_SIZE:
                res1.d = WORKING_REG_D.d;
                res2.d = Pipe2.d;
                res3.d =WORKING_REG_D.d =  res1.d + res2.d;
		            _pc+=4;
                break;
              }
            break;
          case ADDRREGADD:   // An
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res1.b.l = GetValue(WORKING_REG_A.d);
                res2.b.l = Pipe2.b.l;
                res3.b.l = res1.b.l + res2.b.l;
                PutValue(WORKING_REG_A.d, res3.b.l);
		            _pc+=2;
                break;
              case WORD_SIZE:
                res1.w = GetShortValue(WORKING_REG_A.d);
                res2.w = Pipe2.w;
                res3.w = res1.w + res2.w;
                PutShortValue(WORKING_REG_A.d, res3.w);
		            _pc+=2;
                break;
              case DWORD_SIZE:
                res1.d = GetIntValue(WORKING_REG_A.d);
                res2.d = Pipe2.d;
                res3.d = res1.d + res2.d;
                PutIntValue(WORKING_REG_A.d, res3.d);
		            _pc+=4;
                break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res1.b.l = GetValue(WORKING_REG_A.d);
                res2.b.l = Pipe2.b.l;
                res3.b.l = res1.b.l + res2.b.l;
                PUT8_PINC();
		            _pc+=2;
                break;
              case WORD_SIZE:
                res1.w = GetShortValue(WORKING_REG_A.d);
                res2.w = Pipe2.w;
                res3.w = res1.w + res2.w;
                PUT16_PINC();
		            _pc+=2;
                break;
              case DWORD_SIZE:
                res1.d = GetIntValue(WORKING_REG_A.d);
                res2.d = Pipe2.d;
                res3.d = res1.d + res2.d;
                PUT32_PINC();
		            _pc+=4;
                break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res1.b.l = GET8_PDEC();
                res2.b.l = Pipe2.b.l;
                res3.b.l = res1.b.l + res2.b.l;
                PutValue(WORKING_REG_A.d, res3.b.l);
		            _pc+=2;
                break;
              case WORD_SIZE:
                res1.w = GET16_PDEC();
                res2.w = Pipe2.w;
                res3.w = res1.w + res2.w;
                PutShortValue(WORKING_REG_A.d, res3.w);
		            _pc+=2;
                break;
              case DWORD_SIZE:
                res1.d = GET32_PDEC();
                res2.d = Pipe2.d;
                res3.d = res1.d + res2.d;
                PutIntValue(WORKING_REG_A.d, res3.d);
		            _pc+=4;
                break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res2.b.l = Pipe2.b.l;
                _pc+=AdvPipe(_pc,2);
                res1.b.l = GET8_DISP16();
                res3.b.l = res1.b.l + res2.b.l;
                PUT8_DISP16();
                break;
              case WORD_SIZE:
                res2.w = Pipe2.w;
                _pc+=AdvPipe(_pc,2);
                res1.w = GET16_DISP16();
                res3.w = res1.w + res2.w;
                PUT16_DISP16();
                break;
              case DWORD_SIZE:
                res2.d = Pipe2.d;
                _pc+=AdvPipe(_pc,4);
                res1.d = GET32_DISP16();
                res3.d = res1.d + res2.d;
                PUT32_DISP16();
                break;
              }
						_pc+=2;
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res2.b.l = Pipe2.b.l;
                _pc+=AdvPipe(_pc,2);
                if(!DISPLACEMENT_SIZE) {
                  res1.b.l = GET8_DISP8S();
	                res3.b.l = res1.b.l + res2.b.l;
                  PUT8_DISP8S();
                  }
                else {
                  res1.b.l = GET8_DISP8L();
	                res3.b.l = res1.b.l + res2.b.l;
                  PUT8_DISP8L();
                  }
                break;
              case WORD_SIZE:
                res2.w = Pipe2.w;
                _pc+=AdvPipe(_pc,2);
                if(!DISPLACEMENT_SIZE) {
                  res1.w = GET16_DISP8S();
	                res3.w = res1.w + res2.w;
                  PUT16_DISP8S();
                  }
                else {
                  res1.w = GET16_DISP8L();
	                res3.w = res1.w + res2.w;
                  PUT16_DISP8L();
                  }
                break;
              case DWORD_SIZE:
                res2.d = Pipe2.d;
                _pc+=AdvPipe(_pc,4);
                if(!DISPLACEMENT_SIZE) {
                  res1.d = GET32_DISP8S();
                  res3.d = res1.d + res2.d;
	                PUT32_DISP8S();
                  }
                else {
                  res1.d = GET32_DISP8L();
	                res3.d = res1.d + res2.d;
                  PUT32_DISP8L();
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
    				_f.CCR.Carry= !!(((res2.b.l & 0x80) & (res1.b.l & 0x80)) | ((~res3.b.l & 0x80) &
        			(res1.b.l & 0x80)) | ((res2.b.l & 0x80) & (~res3.b.l & 0x80)));
    				_f.CCR.Ovf= !!(((res2.b.l & 0x80) & (res1.b.l & 0x80) & (~res3.b.l & 0x80)) |
        			((~res2.b.l & 0x80) & (~res1.b.l & 0x80) & (res3.b.l & 0x80)));
            break;
          case WORD_SIZE:
            _f.CCR.Zero=!res3.w;
            _f.CCR.Sign=!!(res3.w & 0x8000);
    				_f.CCR.Carry= !!(((res2.w & 0x8000) & (res1.w & 0x8000)) | ((~res3.w & 0x8000) &
        			(res1.w & 0x8000)) | ((res2.w & 0x8000) & (~res3.w & 0x8000)));
    				_f.CCR.Ovf= !!(((res2.w & 0x8000) & (res1.w & 0x8000) & (~res3.w & 0x8000)) |
        			((~res2.w & 0x8000) & (~res1.w & 0x8000) & (res3.w & 0x8000)));
            break;
          case DWORD_SIZE:
            _f.CCR.Zero=!res3.d;
            _f.CCR.Sign=!!(res3.d & 0x80000000);
    				_f.CCR.Carry= !!(((res2.d & 0x80000000) & (res1.d & 0x80000000)) | ((~res3.d & 0x80000000) &
        			(res1.d & 0x80000000)) | ((res2.d & 0x80000000) & (~res3.d & 0x80000000)));
    				_f.CCR.Ovf= !!(((res2.d & 0x80000000) & (res1.d & 0x80000000) & (~res3.d & 0x80000000)) |
        			((~res2.d & 0x80000000) & (~res1.d & 0x80000000) & (res3.d & 0x80000000)));
            break;
          }
        _f.CCR.Ext=_f.CCR.Carry;
				break;

			case 0x0a:   // EORI
        switch(ADDRESSING) {
          case ABSOLUTEADD:
            switch(Pipe1.b[0]) {
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
                  goto doPrivilegeTrap;
                  }
                break;
              default:
                switch(ABSOLUTEADD_MODE) {
                  case ABSSSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res3.b.l = Pipe2.b.l;
                        _pc+=AdvPipe(_pc,2);
                        res3.b.l ^= GetValue((int16_t)Pipe2.w);
                        PutValue((int16_t)Pipe2.w, res3.b.l);
                        break;
                      case WORD_SIZE:
                        res3.w = Pipe2.w;
                        _pc+=AdvPipe(_pc,2);
                        res3.w ^= GetShortValue((int16_t)Pipe2.w);
                        PutShortValue((int16_t)Pipe2.w, res3.w);
                        break;
                      case DWORD_SIZE:
                        res3.d = Pipe2.d;
                        _pc+=AdvPipe(_pc,4);
                        res3.d ^= GetIntValue((int16_t)Pipe2.w);
                        PutIntValue((int16_t)Pipe2.w, res3.d);
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
                        res3.d = Pipe2.d;
                        _pc+=AdvPipe(_pc,4);
                        res3.d ^= GetIntValue(Pipe2.d); 
                        PutIntValue(Pipe2.d,res3.d);
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
                res3.d = WORKING_REG_D.d ^= Pipe2.d;
								_pc+=4;
                break;
              }
            break;
          case ADDRREGADD:   // An	NO
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GetValue(WORKING_REG_A.d) ^ Pipe2.b.l;
                PutValue(WORKING_REG_A.d, res3.b.l);
								_pc+=2;
                break;
              case WORD_SIZE:
                res3.w = GetShortValue(WORKING_REG_A.d) ^ Pipe2.w;
                PutShortValue(WORKING_REG_A.d, res3.w);
								_pc+=2;
                break;
              case DWORD_SIZE:
                res3.d = GetIntValue(WORKING_REG_A.d) ^ Pipe2.d;
                PutIntValue(WORKING_REG_A.d, res3.d);
								_pc+=4;
                break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GetValue(WORKING_REG_A.d) ^ Pipe2.b.l;
                PUT8_PINC();
								_pc+=2;
                break;
              case WORD_SIZE:
                res3.w = GetShortValue(WORKING_REG_A.d) ^ Pipe2.w;
                PUT16_PINC();
								_pc+=2;
                break;
              case DWORD_SIZE:
                res3.d = GetIntValue(WORKING_REG_A.d) ^ Pipe2.d;
                PUT32_PINC();
								_pc+=4;
                break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GET8_PDEC() ^ Pipe2.b.l;
                PutValue(WORKING_REG_A.d, res3.b.l);
								_pc+=2;
                break;
              case WORD_SIZE:
                res3.w = GET16_PDEC() ^ Pipe2.w;
                PutShortValue(WORKING_REG_A.d, res3.w);
								_pc+=2;
                break;
              case DWORD_SIZE:
                res3.d = GET32_PDEC() ^ Pipe2.d;
                PutIntValue(WORKING_REG_A.d, res3.d);
								_pc+=4;
                break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = Pipe2.b.l;
                _pc+=AdvPipe(_pc,2);
                res3.b.l ^= GET8_DISP16();
                PUT8_DISP16();
                break;
              case WORD_SIZE:
                res3.w = Pipe2.w;
                _pc+=AdvPipe(_pc,2);
                res3.w ^= GET16_DISP16();
                PUT16_DISP16();
                break;
              case DWORD_SIZE:
                res3.d = Pipe2.d;
                _pc+=AdvPipe(_pc,4);
                res3.d ^= GET32_DISP16();
                PUT32_DISP16();
                break;
              }
						_pc+=2;
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = Pipe2.b.l;
                _pc+=AdvPipe(_pc,2);
                if(!DISPLACEMENT_SIZE) {
                  res3.b.l ^= GET8_DISP8S();
                  PUT8_DISP8S();
                  }
                else {
                  res3.b.l ^= GET8_DISP8L();
                  PUT8_DISP8L();
                  }
                break;
              case WORD_SIZE:
                res3.w = Pipe2.w;
                _pc+=AdvPipe(_pc,2);
                if(!DISPLACEMENT_SIZE) {
                  res3.w ^= GET16_DISP8S();
                  PUT16_DISP8S();
                  }
                else {
                  res3.w ^= GET16_DISP8L();
                  PUT16_DISP8L();
                  }
                break;
              case DWORD_SIZE:
                res3.d = Pipe2.d;
                _pc+=AdvPipe(_pc,4);
                if(!DISPLACEMENT_SIZE) {
                  res3.d ^= GET32_DISP8S();
                  PUT32_DISP8S();
                  }
                else {
                  res3.d ^= GET32_DISP8L();
                  PUT32_DISP8L();
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
            switch(ABSOLUTEADD_MODE) {
              case ABSSSUBADD:
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = Pipe2.b.l;
                    _pc+=AdvPipe(_pc,2);
                    res1.b.l = GetValue((int16_t)Pipe2.w);
                    break;
                  case WORD_SIZE:
                    res2.w = Pipe2.w;
                    _pc+=AdvPipe(_pc,2);
                    res1.w = GetShortValue((int16_t)Pipe2.w);
                    break;
                  case DWORD_SIZE:
                    res2.d = Pipe2.d;
                    _pc+=AdvPipe(_pc,4);
                    res1.d = GetIntValue((int16_t)Pipe2.w);
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
                    res2.d = Pipe2.d;
                    _pc+=AdvPipe(_pc,4);
                    res1.d = GetIntValue(Pipe2.d); 
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
                res1.d = WORKING_REG_D.d;
                res2.d = Pipe2.d;
                _pc+=4;
                break;
              }
            break;
          case ADDRREGADD:   // An	NO
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res1.b.l = GetValue(WORKING_REG_A.d);
                res2.b.l = Pipe2.b.l;
                _pc+=2;
                break;
              case WORD_SIZE:
                res1.w = GetShortValue(WORKING_REG_A.d);
                res2.w = Pipe2.w;
                _pc+=2;
                break;
              case DWORD_SIZE:
                res1.d = GetIntValue(WORKING_REG_A.d);
                res2.d = Pipe2.d;
								_pc+=4;
                break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res1.b.l = GET8_PINC();
                res2.b.l = Pipe2.b.l;
                _pc+=2;
                break;
              case WORD_SIZE:
                res1.w = GET16_PINC();
                res2.w = Pipe2.w;
                _pc+=2;
                break;
              case DWORD_SIZE:
                res1.d = GET32_PINC();
                res2.d = Pipe2.d;
		            _pc+=4;
                break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res1.b.l = GET8_PDEC();
                res2.b.l = Pipe2.b.l;
                _pc+=2;
                break;
              case WORD_SIZE:
                res1.w = GET16_PDEC();
                res2.w = Pipe2.w;
                _pc+=2;
                break;
              case DWORD_SIZE:
                res1.d = GET32_PDEC();
                res2.d = Pipe2.d;
								_pc+=4;
                break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res2.b.l = Pipe2.b.l;
                _pc+=AdvPipe(_pc,2);
                res1.b.l = GET8_DISP16();
                break;
              case WORD_SIZE:
                res2.w = Pipe2.w;
                _pc+=AdvPipe(_pc,2);
                res1.w = GET16_DISP16();
                break;
              case DWORD_SIZE:
                res2.d = Pipe2.d;
                _pc+=AdvPipe(_pc,4);
                res1.d = GET32_DISP16();
                break;
              }
						_pc+=2;
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
  							res2.b.l = Pipe2.b.l;
                _pc+=AdvPipe(_pc,2);
                if(!DISPLACEMENT_SIZE)
									res1.b.l = GET8_DISP8S();
                else 
									res1.b.l = GET8_DISP8L();
                break;
              case WORD_SIZE:
  							res2.w = Pipe2.w;
                _pc+=AdvPipe(_pc,2);
                if(!DISPLACEMENT_SIZE)
									res1.w = GET16_DISP8S();
                else
									res1.w = GET16_DISP8L();
                break;
              case DWORD_SIZE:
								res2.d = Pipe2.d;
                 _pc+=AdvPipe(_pc,4);
                if(!DISPLACEMENT_SIZE) 
									res1.d = GET32_DISP8S();
                else 
									res1.d = GET32_DISP8L();
                break;
              }
						_pc+=2;
            break;
          }

        switch(OPERAND_SIZE) {
          case BYTE_SIZE:
            res3.b.l=res1.b.l-res2.b.l;
            break;
          case WORD_SIZE:
            res3.w=res1.w-res2.w;
            break;
          case DWORD_SIZE:
            res3.d=res1.d-res2.d;
            break;
          }
        
aggFlagC:
        switch(OPERAND_SIZE) {
          case BYTE_SIZE:
            _f.CCR.Zero=!res3.b.l;
            _f.CCR.Sign=!!(res3.b.l & 0x80);
    				_f.CCR.Carry= !!(((res2.b.l & 0x80) & (~res1.b.l & 0x80)) | ((res3.b.l & 0x80) &
        			(~res1.b.l & 0x80)) | ((res2.b.l & 0x80) & (res3.b.l & 0x80)));
    				_f.CCR.Ovf= !!(((~res2.b.l & 0x80) & (res1.b.l & 0x80) & (~res3.b.l & 0x80)) |
        			((res2.b.l & 0x80) & (~res1.b.l & 0x80) & (res3.b.l & 0x80)));
            break;
          case WORD_SIZE:
            _f.CCR.Zero=!res3.w;
            _f.CCR.Sign=!!(res3.w & 0x8000);
    				_f.CCR.Carry= !!(((res2.w & 0x8000) & (~res1.w & 0x8000)) | ((res3.w & 0x8000) &
        			(~res1.w & 0x8000)) | ((res2.w & 0x8000) & (res3.w & 0x8000)));
    				_f.CCR.Ovf= !!(((~res2.w & 0x8000) & (res1.w & 0x8000) & (~res3.w & 0x8000)) |
        			((res2.w & 0x8000) & (~res1.w & 0x8000) & (res3.w & 0x8000)));
            break;
          case DWORD_SIZE:
aggFlagC4:
            _f.CCR.Zero=!res3.d;
            _f.CCR.Sign=!!(res3.d & 0x80000000);
    				_f.CCR.Carry= !!(((res2.d & 0x80000000) & (~res1.d & 0x80000000)) | ((res3.d & 0x80000000) &
        			(~res1.d & 0x80000000)) | ((res2.d & 0x80000000) & (res3.d & 0x80000000)));
    				_f.CCR.Ovf= !!(((~res2.d & 0x80000000) & (res1.d & 0x80000000) & (~res3.d & 0x80000000)) |
        			((res2.d & 0x80000000) & (~res1.d & 0x80000000) & (res3.d & 0x80000000)));
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
            switch(ABSOLUTEADD_MODE) {
              case ABSSSUBADD:
                res3.b.l = GetValue((int16_t)Pipe2.w);
                _pc+=AdvPipe(_pc,2);
                break;
              case ABSLSUBADD:
                res3.b.l = GetValue(Pipe2.d);
                _pc+=AdvPipe(_pc,4);
                break;
              case PCIDXSUBADD:   // (D16,PC)
                res3.b.l = GET8_DISPPC16();
                _pc+=AdvPipe(_pc,2);
                break;
              case PCDISPSUBADD:   // (D8,Xn,PC)
                if(!DISPLACEMENT_SIZE)
                  res3.b.l = GET8_DISPPC8S();
                else
                  res3.b.l = GET8_DISPPC8L();
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
            res3.b.l = GetValue(WORKING_REG_A.d);
            break;
          case ADDRPINCADD:   // (An)+
            res3.b.l = GET8_PINC();
            break;
          case ADDRPDECADD:   // -(An)
            res3.b.l = GET8_PDEC();
            break;
          case ADDRDISPADD:   // (D16,An)
            res3.b.l = GET8_DISP16();
            _pc+=AdvPipe(_pc,2);
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            if(!DISPLACEMENT_SIZE)
              res3.b.l = GET8_DISP8S();
            else
              res3.b.l = GET8_DISP8L();
            _pc+=AdvPipe(_pc,2);
            break;
          }
        switch(ADDRESSING2) {
          case ABSOLUTEADD:
            switch(ABSOLUTEADD_MODE2) {
              case ABSSSUBADD:
                PutValue((int16_t)Pipe2.w,res3.b.l);
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
            PutValue(DEST_REG_A.d,res3.b.l);
            break;
          case ADDRPINCADD:   // (An)+
            PutValue(DEST_REG_A.d,res3.b.l);
            DEST_REG_A.d += ((Pipe1.b[1] & 0xe) == 0xe) ? 2 : 1;
            break;
          case ADDRPDECADD:   // -(An)
            DEST_REG_A.d -= ((Pipe1.b[1] & 0xe) == 0xe) ? 2 : 1;
            PutValue(DEST_REG_A.d,res3.b.l);
            break;
          case ADDRDISPADD:   // (D16,An)
            PutValue(DEST_REG_A.d + (int16_t)Pipe2.w,res3.b.l);
            _pc+=2;
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            if(!DISPLACEMENT_SIZE)
              PutValue(DEST_REG_A.d + (int16_t)DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l,res3.b.l);
            else
              PutValue(DEST_REG_A.d + (int32_t)DISPLACEMENT_REG.d + (signed char)Pipe2.b.l,res3.b.l);
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
            switch(ABSOLUTEADD_MODE) {
              case ABSSSUBADD:
                res3.d = GetIntValue((int16_t)Pipe2.w);
                _pc+=AdvPipe(_pc,2);
                break;
              case ABSLSUBADD:
                res3.d = GetIntValue(Pipe2.d); 
                _pc+=AdvPipe(_pc,4);
                break;
              case PCIDXSUBADD:   // (D16,PC)
                res3.d = GET32_DISPPC16();
                _pc+=AdvPipe(_pc,2);
	              break;
              case PCDISPSUBADD:   // (D8,Xn,PC)
                if(!DISPLACEMENT_SIZE)
                  res3.d = GET32_DISPPC8S();
                else
                  res3.d = GET32_DISPPC8L();
                _pc+=AdvPipe(_pc,2);
                break;
              case IMMSUBADD:
                res3.d = Pipe2.d; 
                _pc+=AdvPipe(_pc,4);
                break;
              }
            break;
          case DATAREGADD:   // Dn
            res3.d = WORKING_REG_D.d;
            break;
          case ADDRREGADD:   // An
            res3.d = WORKING_REG_A.d;
            break;
          case ADDRADD:   // (An)
            res3.d = GetIntValue(WORKING_REG_A.d);
            break;
          case ADDRPINCADD:   // (An)+
            res3.d = GET32_PINC();
            break;
          case ADDRPDECADD:   // -(An)
            res3.d = GET32_PDEC();
            break;
          case ADDRDISPADD:   // (D16,An)
            res3.d = GET32_DISP16();
            _pc+=AdvPipe(_pc,2);
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            if(!DISPLACEMENT_SIZE)
              res3.d = GET32_DISP8S();
            else 
              res3.d = GET32_DISP8L();
            _pc+=AdvPipe(_pc,2);
            break;
          }
        switch(ADDRESSING2) {
          case ABSOLUTEADD:
            switch(ABSOLUTEADD_MODE2) {
              case ABSSSUBADD:
                PutIntValue((int16_t)Pipe2.w,res3.d);
                _pc+=2;
                break;
              case ABSLSUBADD:
                PutIntValue(Pipe2.d,res3.d);
                _pc+=4;
                break;
              case PCIDXSUBADD:   // (D16,PC) qua no!
              case PCDISPSUBADD:   // (D8,Xn,PC) qua no!
              case IMMSUBADD:
                break;
              }
            break;
          case DATAREGADD:   // Dn
            DEST_REG_D.d = res3.d;
            break;
          case ADDRREGADD:   // An
						DEST_REG_A.d = res3.d;    // MOVEA
            goto noAggFlag;
            break;
          case ADDRADD:   // (An)
            PutIntValue(DEST_REG_A.d,res3.d);
            break;
          case ADDRPINCADD:   // (An)+
            PutIntValue(DEST_REG_A.d,res3.d);
            DEST_REG_A.d+=4;
            break;
          case ADDRPDECADD:   // -(An)
            DEST_REG_A.d-=4;
            PutIntValue(DEST_REG_A.d,res3.d);
            break;
          case ADDRDISPADD:   // (D16,An)
            PutIntValue(DEST_REG_A.d + (int16_t)Pipe2.w,res3.d);
            _pc+=2;
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            if(!DISPLACEMENT_SIZE)
              PutIntValue(DEST_REG_A.d + (int16_t)DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l,res3.d);
            else
              PutIntValue(DEST_REG_A.d + (int32_t)DISPLACEMENT_REG.d + (signed char)Pipe2.b.l,res3.d);
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
            switch(ABSOLUTEADD_MODE) {
              case ABSSSUBADD:
	              res3.w = GetShortValue((int16_t)Pipe2.w);
                _pc+=AdvPipe(_pc,2);
                break;
              case ABSLSUBADD:
		            res3.w = GetShortValue(Pipe2.d);
                _pc+=AdvPipe(_pc,4);
                break;
              case PCIDXSUBADD:   // (D16,PC)
                res3.w = GET16_DISPPC16();
                _pc+=AdvPipe(_pc,2);
                break;
              case PCDISPSUBADD:   // (D8,Xn,PC)
                if(!DISPLACEMENT_SIZE)
                  res3.w = GET16_DISPPC8S();
                else
                  res3.w = GET16_DISPPC8L();
                _pc+=AdvPipe(_pc,2);
                break;
              case IMMSUBADD:
                res3.w = Pipe2.w; 
                _pc+=AdvPipe(_pc,2);
                break;
              }
            break;
          case DATAREGADD:   // Dn
            res3.w = WORKING_REG_D.w.l;
            break;
          case ADDRREGADD:   // An
            res3.w = WORKING_REG_A.w.l;
            break;
          case ADDRADD:   // (An)
            res3.w = GetShortValue(WORKING_REG_A.d);
            break;
          case ADDRPINCADD:   // (An)+
            res3.w = GET16_PINC();
            break;
          case ADDRPDECADD:   // -(An)
            res3.w = GET16_PDEC();
            break;
          case ADDRDISPADD:   // (D16,An)
            res3.w = GET16_DISP16();
            _pc+=AdvPipe(_pc,2);
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            if(!DISPLACEMENT_SIZE)
              res3.w = GET16_DISP8S();
            else
              res3.w = GET16_DISP8L();
            _pc+=AdvPipe(_pc,2);
            break;
          }
        switch(ADDRESSING2) {
          case ABSOLUTEADD:
            switch(ABSOLUTEADD_MODE2) {
              case ABSSSUBADD:
                PutShortValue((int16_t)Pipe2.w,res3.w);
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
            DEST_REG_A.d = (int16_t)res3.w;    // MOVEA
            goto noAggFlag;
            break;
          case ADDRADD:   // (An)
            PutShortValue(DEST_REG_A.d,res3.w);
            break;
          case ADDRPINCADD:   // (An)+
            PutShortValue(DEST_REG_A.d,res3.w);
            DEST_REG_A.d+=2;
            break;
          case ADDRPDECADD:   // -(An)
            DEST_REG_A.d-=2;
            PutShortValue(DEST_REG_A.d,res3.w);
            break;
          case ADDRDISPADD:   // (D16,An)
            PutShortValue(DEST_REG_A.d + (int16_t)Pipe2.w,res3.w);
            _pc+=2;
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            if(!DISPLACEMENT_SIZE)
              PutShortValue(DEST_REG_A.d + (int16_t)DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l,res3.w);
            else
              PutShortValue(DEST_REG_A.d + (int32_t)DISPLACEMENT_REG.d + (signed char)Pipe2.b.l,res3.w);
            _pc+=2;
            break;
          }
        goto aggFlag2;
				break;
        
			case 0x40:   // MOVE from SR, NEGX
        switch(ADDRESSING) {
          case ABSOLUTEADD:
            switch(ABSOLUTEADD_MODE) {
              case ABSSSUBADD:
                _pc+=2;
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = 0;
                    res2.b.l = GetValue((int16_t)Pipe2.w) + _f.CCR.Ext;
                    res3.b.l = res1.b.l - res2.b.l;
                    PutValue((int16_t)Pipe2.w, res3.b.l);
                    break;
                  case WORD_SIZE:
                    res1.w = 0;
                    res2.w = GetShortValue((int16_t)Pipe2.w) + _f.CCR.Ext;
                    res3.w = res1.w - res2.w;
                    PutShortValue((int16_t)Pipe2.w, res3.w);
                    break;
                  case DWORD_SIZE:
                    res1.d = 0;
                    res2.d = GetIntValue((int16_t)Pipe2.w) + _f.CCR.Ext;
                    res3.d = res1.d - res2.d;
                    PutIntValue((int16_t)Pipe2.w, res3.d);
                    break;
                  case SIZE_ELSE:
#ifdef MC68010
                    if(!_f.SR.Supervisor) {
                      goto doPrivilegeTrap;
                      }
#endif
                    PutShortValue((int16_t)Pipe2.w,_f.SR.w);
                    goto noAggFlag;
                    break;
                  }
                break;
              case ABSLSUBADD:
                _pc+=4;
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = 0;
                    res2.b.l = GetValue(Pipe2.d) + _f.CCR.Ext;
                    res3.b.l = res1.b.l - res2.b.l;
                    PutValue(Pipe2.d,res3.b.l);
                    break;
                  case WORD_SIZE:
                    res1.w = 0;
                    res2.w = GetShortValue(Pipe2.d) + _f.CCR.Ext;
                    res3.w = res1.w - res2.w;
                    PutShortValue(Pipe2.d,res3.w);
                    break;
                  case DWORD_SIZE:
                    res1.d = 0;
                    res2.d = GetIntValue(Pipe2.d) + _f.CCR.Ext;
                    res3.d = res1.d - res2.d;
                    PutIntValue(Pipe2.d,res3.d);
                    break;
                  case SIZE_ELSE:
#ifdef MC68010
                    if(!_f.SR.Supervisor) {
                      goto doPrivilegeTrap;
                      }
#endif
                    PutShortValue(Pipe2.d,_f.SR.w);
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
                res3.b.l = WORKING_REG_D.b.b0 = res1.b.l - res2.b.l;
                break;
              case WORD_SIZE:
                res1.w = 0;
                res2.w = WORKING_REG_D.w.l + _f.CCR.Ext;
                res3.w = WORKING_REG_D.w.l = res1.w - res2.w;
                break;
              case DWORD_SIZE:
                res1.d = 0;
                res2.d = WORKING_REG_D.d + _f.CCR.Ext;
                res3.d = WORKING_REG_D.d = res1.d - res2.d;
                break;
		          case SIZE_ELSE:
#ifdef MC68010
                    if(!_f.SR.Supervisor) {
                      goto doPrivilegeTrap;
                      }
#endif
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
                res2.b.l = GetValue(WORKING_REG_A.d) + _f.CCR.Ext;
                res3.b.l = res1.b.l - res2.b.l;
                PutValue(WORKING_REG_A.d, res3.b.l);
                break;
              case WORD_SIZE:
                res1.w = 0;
                res2.w = GetShortValue(WORKING_REG_A.d) + _f.CCR.Ext;
                res3.w = res1.w - res2.w;
                PutShortValue(WORKING_REG_A.d, res3.w);
                break;
              case DWORD_SIZE:
                res1.d = 0;
                res2.d = GetIntValue(WORKING_REG_A.d) + _f.CCR.Ext;
                res3.d = res1.d - res2.d;
                PutIntValue(WORKING_REG_A.d, res3.d);
                break;
		          case SIZE_ELSE:
#ifdef MC68010
                    if(!_f.SR.Supervisor) {
                      goto doPrivilegeTrap;
                      }
#endif
                PutShortValue(WORKING_REG_A.d, _f.SR.w);
								goto noAggFlag;
			          break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res1.b.l = 0;
                res2.b.l = GetValue(WORKING_REG_A.d) + _f.CCR.Ext;
                res3.b.l = res1.b.l - res2.b.l;
                PUT8_PINC();
                break;
              case WORD_SIZE:
                res1.w = 0;
                res2.w = GetShortValue(WORKING_REG_A.d) + _f.CCR.Ext;
                res3.w = res1.w - res2.w;
                PUT16_PINC();
                break;
              case DWORD_SIZE:
                res1.d = 0;
                res2.d = GetIntValue(WORKING_REG_A.d) + _f.CCR.Ext;
                res3.d = res1.d - res2.d;
                PUT32_PINC();
                break;
		          case SIZE_ELSE:
#ifdef MC68010
                    if(!_f.SR.Supervisor) {
                      goto doPrivilegeTrap;
                      }
#endif
                PutShortValue(WORKING_REG_A.d, _f.SR.w);
                WORKING_REG_A.d+=2;
								goto noAggFlag;
			          break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res1.b.l = 0;
                res2.b.l = GET8_PDEC() + _f.CCR.Ext;
                res3.b.l = res1.b.l - res2.b.l;
                PutValue(WORKING_REG_A.d, res3.b.l);
                break;
              case WORD_SIZE:
                res1.w = 0;
                res2.w = GET16_PDEC() + _f.CCR.Ext;
                res3.w = res1.w - res2.w;
                PutShortValue(WORKING_REG_A.d, res3.w);
                break;
              case DWORD_SIZE:
                res1.d = 0;
                res2.d = GET32_PDEC() + _f.CCR.Ext;
                res3.d = res1.d - res2.d;
                PutIntValue(WORKING_REG_A.d, res3.d);
                break;
		          case SIZE_ELSE:
#ifdef MC68010
                    if(!_f.SR.Supervisor) {
                      goto doPrivilegeTrap;
                      }
#endif
                WORKING_REG_A.d-=2;
                PutShortValue(WORKING_REG_A.d, _f.SR.w);
								goto noAggFlag;
			          break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            _pc+=2;
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res1.b.l = 0;
                res2.b.l = GET8_DISP16() + _f.CCR.Ext;
                res3.b.l = res1.b.l - res2.b.l;
                PUT8_DISP16();
                break;
              case WORD_SIZE:
                res1.w = 0;
                res2.w = GET16_DISP16() + _f.CCR.Ext;
                res3.w = res1.w - res2.w;
                PUT16_DISP16();
                break;
              case DWORD_SIZE:
                res1.d = 0;
                res2.d = GET32_DISP16() + _f.CCR.Ext;
                res3.d = res1.d - res2.d;
                PUT32_DISP16();
                break;
		          case SIZE_ELSE:
#ifdef MC68010
                    if(!_f.SR.Supervisor) {
                      goto doPrivilegeTrap;
                      }
#endif
                PutShortValue(DISP16(),_f.SR.w);
								goto noAggFlag;
			          break;
              }
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            _pc+=2;
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res1.b.l = 0;
                if(!DISPLACEMENT_SIZE) {
									res2.b.l = GET8_DISP8S() + _f.CCR.Ext;
									res3.b.l = res1.b.l - res2.b.l;
									PUT8_DISP8S();
									}
                else {
									res2.b.l = GET8_DISP8L() + _f.CCR.Ext;
									res3.b.l = res1.b.l - res2.b.l;
									PUT8_DISP8L();
                  }
                break;
              case WORD_SIZE:
                res1.w = 0;
                if(!DISPLACEMENT_SIZE) {
                  res2.w = GET16_DISP8S() + _f.CCR.Ext;
                  res3.w = res1.w - res2.w;
                  PUT16_DISP8S();
									}
                else {
                  res2.w = GET16_DISP8L() + _f.CCR.Ext;
                  res3.w = res1.w - res2.w;
                  PUT16_DISP8L();
									}
                break;
              case DWORD_SIZE:
                res1.d = 0;
                if(!DISPLACEMENT_SIZE) {
                  res2.d = GET32_DISP8S() + _f.CCR.Ext;
                  res3.d = res1.d - res2.d;
                  PUT32_DISP8S();
									}
                else {
                  res2.d = GET32_DISP8L() + _f.CCR.Ext;
                  res3.d = res1.d - res2.d;
                  PUT32_DISP8L();
									}
                break;
		          case SIZE_ELSE:
#ifdef MC68010
                    if(!_f.SR.Supervisor) {
                      goto doPrivilegeTrap;
                      }
#endif
                if(!DISPLACEMENT_SIZE)
                  PutShortValue(DISP8S(), _f.SR.w);
                else
                  PutShortValue(DISP8L(), _f.SR.w);
								goto noAggFlag;
			          break;
              }
            break;
          }
				break;
        
			case 0x42:   // CLR
				res3.d=0;		// più pratico!
        switch(ADDRESSING) {
          case ABSOLUTEADD:
            switch(ABSOLUTEADD_MODE) {
              case ABSSSUBADD:
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    PutValue((int16_t)Pipe2.w,res3.b.l);
                    break;
                  case WORD_SIZE:
                    PutShortValue((int16_t)Pipe2.w,res3.w);
                    break;
                  case DWORD_SIZE:
                    PutIntValue((int16_t)Pipe2.w,res3.d);
                    break;
                  }
                _pc+=2;
                break;
              case ABSLSUBADD:
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    PutValue(Pipe2.d,res3.b.l);
                    break;
                  case WORD_SIZE:
                    PutShortValue(Pipe2.d,res3.w);
                    break;
                  case DWORD_SIZE:
                    PutIntValue(Pipe2.d,res3.d);
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
                WORKING_REG_D.b.b0 = res3.b.l;
                break;
              case WORD_SIZE:
                WORKING_REG_D.w.l = res3.w;
                break;
              case DWORD_SIZE:
                WORKING_REG_D.d = res3.d;
                break;
              }
            break;
          case ADDRREGADD:   // An Qua no!
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                PutValue(WORKING_REG_A.d,res3.b.l);
                break;
              case WORD_SIZE:
                PutShortValue(WORKING_REG_A.d,res3.w);
                break;
              case DWORD_SIZE:
                PutIntValue(WORKING_REG_A.d,res3.d);
                break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                PUT8_PINC();
                break;
              case WORD_SIZE:
                PUT16_PINC();
                break;
              case DWORD_SIZE:
                PUT32_PINC();
                break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                PUT8_PDEC();
                break;
              case WORD_SIZE:
                PUT16_PDEC();
                break;
              case DWORD_SIZE:
                PUT32_PDEC();
                break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                PUT8_DISP16();
                break;
              case WORD_SIZE:
                PUT16_DISP16();
                break;
              case DWORD_SIZE:
                PUT32_DISP16();
                break;
              }
						_pc+=2;
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                if(!DISPLACEMENT_SIZE)
                  PUT8_DISP8S();
                else
                  PUT8_DISP8L();
                break;
              case WORD_SIZE:
                if(!DISPLACEMENT_SIZE)
                  PUT16_DISP8S();
                else
                  PUT16_DISP8L();
                break;
              case DWORD_SIZE:
                if(!DISPLACEMENT_SIZE)
                  PUT32_DISP8S();
                else
                  PUT32_DISP8L();
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
        if(Pipe1.b[0] & 0b01000000) {    // LEA
					switch(ADDRESSING) {
						case ABSOLUTEADD:
							switch(ABSOLUTEADD_MODE) {
								case ABSSSUBADD:
									res3.d = (int16_t)Pipe2.w;
									_pc+=2;
									break;
								case ABSLSUBADD:
									res3.d = Pipe2.d; 
									_pc+=4;
									break;
								case PCIDXSUBADD:   // (D16,PC)
									res3.d = DISPPC16();
									_pc+=2;
									break;
								case PCDISPSUBADD:   // (D8,Xn,PC)
                  if(!DISPLACEMENT_SIZE)
    								res3.d = DISPPC8S();
                  else
    								res3.d = DISPPC8L();
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
							res3.d = WORKING_REG_A.d; //
							break;
						case ADDRPINCADD:   // (An)+
						case ADDRPDECADD:   // -(An)
							break;
						case ADDRDISPADD:   // (D16,An)
							res3.d = DISP16();
							_pc+=2;
							break;
						case ADDRIDXADD:   // (D8,An,Xn)
							if(!DISPLACEMENT_SIZE)
								res3.d = DISP8S();
							else
								res3.d = DISP8L();
							_pc+=2;
							break;
						}
					DEST_REG_A.d = res3.d;
          }
        else {    // CHK
          if(Pipe1.b[0] & 0b10000000) {    // SIZE 
						switch(ADDRESSING) {
							case ABSOLUTEADD:
								switch(ABSOLUTEADD_MODE) {
									case ABSSSUBADD:
										res3.d = GetIntValue((int16_t)Pipe2.w);
										_pc+=2;
										break;
									case ABSLSUBADD:
										res3.d = GetIntValue(Pipe2.d);
										_pc+=4;
										break;
									case PCIDXSUBADD:   // (D16,PC)
										res3.d = GET32_DISPPC16();
										_pc+=2;
										break;
									case PCDISPSUBADD:   // (D8,Xn,PC)
                    if(!DISPLACEMENT_SIZE)
    									res3.d = GET32_DISPPC8S();
                    else
      								res3.d = GET32_DISPPC8L();
										_pc+=2;
										break;
									case IMMSUBADD:
										res3.d = Pipe2.d; 
										_pc+=4;
										break;
									}
								break;
							case DATAREGADD:   // Dn
								res3.d = WORKING_REG_D.d;
								break;
							case ADDRREGADD:   // An
								break;
							case ADDRADD:   // (An)
								res3.d = GetIntValue(WORKING_REG_A.d); 
								break;
							case ADDRPINCADD:   // (An)+
								res3.d = GET32_PINC();
								break;
							case ADDRPDECADD:   // -(An)
								res3.d = GET32_PDEC();
								break;
							case ADDRDISPADD:   // (D16,An)
								res3.d = DISP16();
								_pc+=2;
								break;
							case ADDRIDXADD:   // (D8,An,Xn)
								if(!DISPLACEMENT_SIZE)
									res3.d = DISP8S();
								else
									res3.d = DISP8L();
								_pc+=2;
								break;
							}
						_f.CCR.Sign=!!(DEST_REG_D.d & 0x80000000);
            if(((int32_t)DEST_REG_D.d) < 0 || DEST_REG_D.d > ((int32_t)res3.d)) {
              goto trap_chk_6;
            }
          else {
						switch(ADDRESSING) {
							case ABSOLUTEADD:
								switch(ABSOLUTEADD_MODE) {
									case ABSSSUBADD:
										res3.w = GetShortValue((int16_t)Pipe2.w);
										_pc+=2;
										break;
									case ABSLSUBADD:
										res3.w = GetShortValue(Pipe2.d);
										_pc+=4;
										break;
									case PCIDXSUBADD:   // (D16,PC)
										res3.w = GET16_DISPPC16();
										_pc+=2;
										break;
									case PCDISPSUBADD:   // (D8,Xn,PC)
                    if(!DISPLACEMENT_SIZE)
  										res3.w = GET16_DISPPC8S();
                    else
    									res3.w = GET16_DISPPC8L();
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
								res3.w = GetShortValue(WORKING_REG_A.d); // credo sbagliato, v. altri
								break;
							case ADDRPINCADD:   // (An)+
							case ADDRPDECADD:   // -(An)
								break;
							case ADDRDISPADD:   // (D16,An)
								res3.w = GET16_DISP16();
								_pc+=2;
								break;
							case ADDRIDXADD:   // (D8,An,Xn)
								if(!DISPLACEMENT_SIZE)
									res3.w = GET16_DISP8S();
								else
									res3.w = GET16_DISP8L();
								_pc+=2;
								break;
							}
						_f.CCR.Sign=!!(DEST_REG_D.w.l & 0x8000);
						//N — Set if Dn < 0; cleared if Dn > effective address operand; undefined otherwise.
            if(((int16_t)DEST_REG_D.w.l) < 0 || DEST_REG_D.w.l > ((int16_t)res3.w))
              //trap #6
trap_chk_6:// ???pc-2
              res3.b.l=6;
              goto doTrap;
              }
            }
          }
				break;
        
			case 0x44:   // NEG, MOVE to CCR
        switch(ADDRESSING) {
          case ABSOLUTEADD:
            switch(ABSOLUTEADD_MODE) {
              case ABSSSUBADD:
                _pc+=2;
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = 0;
                    res2.b.l = GetValue((int16_t)Pipe2.w);
                    res3.b.l = res1.b.l - res2.b.l;
                    PutValue((int16_t)Pipe2.w,res3.b.l);
                    break;
                  case WORD_SIZE:
                    res1.w = 0;
                    res2.w = GetShortValue((int16_t)Pipe2.w);
                    res3.w = res1.w - res2.w;
                    PutShortValue((int16_t)Pipe2.w,res3.w);
                    break;
                  case DWORD_SIZE:
                    res1.d = 0;
                    res2.d = GetIntValue((int16_t)Pipe2.w);
                    res3.d = res1.d - res2.d;
                    PutIntValue((int16_t)Pipe2.w,res3.d);
                    break;
                  case SIZE_ELSE:
                    _f.CCR.b =  GetValue((int16_t)Pipe2.w);
                    goto noAggFlag;
                    break;
                  }
                break;
              case ABSLSUBADD:
                _pc+=4;
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = 0;
                    res2.b.l = GetValue(Pipe2.d);
                    res3.b.l = res1.b.l - res2.b.l;
                    PutValue(Pipe2.d,res3.b.l);
                    break;
                  case WORD_SIZE:
                    res1.w = 0;
                    res2.w = GetShortValue(Pipe2.d);
                    res3.w = res1.w - res2.w;
                    PutShortValue(Pipe2.d,res3.w);
                    break;
                  case DWORD_SIZE:
                    res1.d = 0;
                    res2.d = GetIntValue(Pipe2.d); 
                    res3.d = res1.d - res2.d;
                    PutIntValue(Pipe2.d,res3.d);
                    break;
                  case SIZE_ELSE:
                    _f.CCR.b = GetValue(Pipe2.d);
                    goto noAggFlag;
                    break;
                  }
                break;
              case PCIDXSUBADD:
                switch(OPERAND_SIZE) {
                  case SIZE_ELSE:
                    _f.CCR.b = GET8_DISPPC16();
										_pc+=2;
                    goto noAggFlag;
                    break;
                  }
                break;
              case PCDISPSUBADD:
                switch(OPERAND_SIZE) {
                  case SIZE_ELSE:
                    if(!DISPLACEMENT_SIZE)
                      _f.CCR.b = GET8_DISPPC8S();
                    else
                      _f.CCR.b = GET8_DISPPC8L();
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
                res3.b.l = WORKING_REG_D.b.b0 = res1.b.l - res2.b.l;
                break;
              case WORD_SIZE:
                res1.w = 0;
                res2.w = WORKING_REG_D.w.l;
                res3.w = WORKING_REG_D.w.l = res1.w - res2.w;
                break;
              case DWORD_SIZE:
                res1.d = 0;
                res2.d = WORKING_REG_D.d;
                res3.d = WORKING_REG_D.d = res1.d - res2.d;
                break;
		          case SIZE_ELSE:
                _f.CCR.b = WORKING_REG_D.b.b0;
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
                res2.b.l = GetValue(WORKING_REG_A.d);
                res3.b.l = res1.b.l - res2.b.l;
                PutValue(WORKING_REG_A.d, res3.b.l);
                break;
              case WORD_SIZE:
                res1.w = 0;
                res2.w = GetShortValue(WORKING_REG_A.d);
                res3.w = res1.w - res2.w;
                PutShortValue(WORKING_REG_A.d, res3.w);
                break;
              case DWORD_SIZE:
                res1.d = 0;
                res2.d = GetIntValue(WORKING_REG_A.d);
                res3.d = res1.d - res2.d;
                PutIntValue(WORKING_REG_A.d, res3.d);
                break;
		          case SIZE_ELSE:
                _f.CCR.b = GetValue(WORKING_REG_A.d);
								goto noAggFlag;
			          break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res1.b.l = 0;
                res2.b.l = GetValue(WORKING_REG_A.d);
                res3.b.l = res1.b.l - res2.b.l;
                PUT8_PINC();
                break;
              case WORD_SIZE:
                res1.w = 0;
                res2.w = GetShortValue(WORKING_REG_A.d);
                res3.w = res1.w - res2.w;
                PUT16_PINC();
                break;
              case DWORD_SIZE:
                res1.d = 0;
                res2.d = GetIntValue(WORKING_REG_A.d);
                res3.d = res1.d - res2.d;
                PUT32_PINC();
                break;
		          case SIZE_ELSE:
                _f.CCR.b = GetValue(WORKING_REG_A.d);
                WORKING_REG_A.d+=2;
								goto noAggFlag;
			          break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res1.b.l = 0;
                res2.b.l = GET8_PDEC();
                res3.b.l = res1.b.l - res2.b.l;
                PutValue(WORKING_REG_A.d, res3.b.l);
                break;
              case WORD_SIZE:
                res1.w = 0;
                res2.w = GET16_PDEC();
                res3.w = res1.w - res2.w;
                PutShortValue(WORKING_REG_A.d, res3.w);
                break;
              case DWORD_SIZE:
                res1.d = 0;
                res2.d = GET32_PDEC();
                res3.d = res1.d - res2.d;
                PutIntValue(WORKING_REG_A.d, res3.d);
                break;
		          case SIZE_ELSE:
                WORKING_REG_A.d-=2;
                _f.CCR.b = GetValue(WORKING_REG_A.d);
								goto noAggFlag;
			          break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            _pc+=2;
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res1.b.l = 0;
                res2.b.l = GET8_DISP16();
                res3.b.l = res1.b.l - res2.b.l;
                PUT8_DISP16();
                break;
              case WORD_SIZE:
                res1.w = 0;
                res2.w = GET16_DISP16();
                res3.w = res1.w - res2.w;
                PUT16_DISP16();
                break;
              case DWORD_SIZE:
                res1.d = 0;
                res2.d = GET32_DISP16();
                res3.d = res1.d - res2.d;
                PUT32_DISP16();
                break;
		          case SIZE_ELSE:
                _f.CCR.b = GET8_DISP16();
								goto noAggFlag;
			          break;
              }
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            _pc+=2;
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res1.b.l = 0;
                if(!DISPLACEMENT_SIZE) {
                  res2.b.l = GET8_DISP8S();
                  res3.b.l = res1.b.l - res2.b.l;
                  PUT8_DISP8S();
                  }
                else {
                  res2.b.l = GET8_DISP8L();
                  res3.b.l = res1.b.l - res2.b.l;
                  PUT8_DISP8L();
                  }
                break;
              case WORD_SIZE:
                res1.w = 0;
                if(!DISPLACEMENT_SIZE) {
                  res2.w = GET16_DISP8S();
                  res3.w = res1.w - res2.w;
                  PUT16_DISP8S();
                  }
                else {
                  res2.w = GET16_DISP8L();
                  res3.w = res1.w - res2.w;
                  PUT16_DISP8L();
                  }
                break;
              case DWORD_SIZE:
                res1.d = 0;
                if(!DISPLACEMENT_SIZE) {
                  res2.d = GET32_DISP8S();
                  res3.d = res1.d - res2.d;
                  PUT32_DISP8S();
                  }
                else {
                  res2.d = GET32_DISP8L();
                  res3.d = res1.d - res2.d;
                  PUT32_DISP8L();
                  }
                break;
		          case SIZE_ELSE:
                if(!DISPLACEMENT_SIZE)
                  _f.CCR.b = GET8_DISP8S();
                else
	                _f.CCR.b = GET8_DISP8L();
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
            switch(ABSOLUTEADD_MODE) {
              case ABSSSUBADD:
       					_pc+=2;
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res3.b.l = ~GetValue((int16_t)Pipe2.w);
                    PutValue((int16_t)Pipe2.w,res3.b.l);
                    break;
                  case WORD_SIZE:
                    res3.w = ~GetShortValue((int16_t)Pipe2.w);
                    PutShortValue((int16_t)Pipe2.w,res3.w);
                    break;
                  case DWORD_SIZE:
                    res3.d = ~GetIntValue((int16_t)Pipe2.w);
                    PutIntValue((int16_t)Pipe2.w,res3.d);
                    break;
                  case SIZE_ELSE:
                    if(_f.SR.Supervisor) {
                      res3.w = GetShortValue((int16_t)Pipe2.w);
                      goto aggSR;
                      }
                    else {
                      goto doPrivilegeTrap;
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
                    res3.d = ~GetIntValue(Pipe2.d); 
                    PutIntValue(Pipe2.d,res3.d);
                    break;
                  case SIZE_ELSE:
                    if(_f.SR.Supervisor) {
                      res3.w = GetShortValue(Pipe2.d);
                      goto aggSR;
                      }
                    else {
                      goto doPrivilegeTrap;
                      }
                    break;
                  }
                break;
              case PCIDXSUBADD:
                switch(OPERAND_SIZE) {
                  case SIZE_ELSE:
          					_pc+=2;
                    if(_f.SR.Supervisor) {
                      res3.w = GET16_DISPPC16();
                      goto aggSR;
                      }
                    else {
                      goto doPrivilegeTrap;
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
                        res3.w = GET16_DISPPC8S();
                      else
                        res3.w = GET16_DISPPC8L();
                      goto aggSR;
                      }
                    else {
                      goto doPrivilegeTrap;
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
                      goto doPrivilegeTrap;
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
                res3.d = ~WORKING_REG_D.d;
                WORKING_REG_D.d = res3.d;
                break;
		          case SIZE_ELSE:
                if(_f.SR.Supervisor) {
                  res3.w = WORKING_REG_D.w.l;
                  goto aggSR;
                  }
                else {
                  goto doPrivilegeTrap;
                  }
			          break;
              }
            break;
          case ADDRREGADD:   // An
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = ~GetValue(WORKING_REG_A.d);
                PutValue(WORKING_REG_A.d, res3.b.l);
                break;
              case WORD_SIZE:
                res3.w = ~GetShortValue(WORKING_REG_A.d);
                PutShortValue(WORKING_REG_A.d, res3.w);
                break;
              case DWORD_SIZE:
                res3.d = ~GetIntValue(WORKING_REG_A.d);
                PutIntValue(WORKING_REG_A.d, res3.d);
                break;
		          case SIZE_ELSE:
                if(_f.SR.Supervisor) {
                  res3.w = GetShortValue(WORKING_REG_A.d);
    							goto aggSR;
                  }
                else {
                  goto doPrivilegeTrap;
                  }
			          break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = ~GetValue(WORKING_REG_A.d);
                PUT8_PINC();
                break;
              case WORD_SIZE:
                res3.w = ~GetShortValue(WORKING_REG_A.d);
                PUT16_PINC();
                break;
              case DWORD_SIZE:
                res3.d = ~GetIntValue(WORKING_REG_A.d);
                PUT32_PINC();
                break;
		          case SIZE_ELSE:
                if(_f.SR.Supervisor) {
                  res3.w = GetShortValue(WORKING_REG_A.d);
	                WORKING_REG_A.d+=2;
									goto aggSR;
                  }
                else {
                  goto doPrivilegeTrap;
                  }
			          break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = ~GET8_PDEC();
                PutValue(WORKING_REG_A.d, res3.b.l);
                break;
              case WORD_SIZE:
                res3.w = ~GET16_PDEC();
                PutShortValue(WORKING_REG_A.d, res3.w);
                break;
              case DWORD_SIZE:
                res3.d = ~GET32_PDEC();
                PutIntValue(WORKING_REG_A.d, res3.d);
                break;
		          case SIZE_ELSE:
                if(_f.SR.Supervisor) {
	                WORKING_REG_A.d-=2;
                  res3.w = GetShortValue(WORKING_REG_A.d);
									goto aggSR;
                  }
                else {
                  goto doPrivilegeTrap;
                  }
			          break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
						_pc+=2;
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = ~GET8_DISP16();
                PUT8_DISP16();
                break;
              case WORD_SIZE:
                res3.w = ~GET16_DISP16();
                PUT16_DISP16();
                break;
              case DWORD_SIZE:
                res3.d = ~GET32_DISP16();
                PUT32_DISP16();
                break;
		          case SIZE_ELSE:
                if(_f.SR.Supervisor) {
                  res3.w = GET16_DISP16();
									goto aggSR;
                  }
                else {
                  goto doPrivilegeTrap;
                  }
			          break;
              }
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
						_pc+=2;
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.b.l = ~GET8_DISP8S();
                  PUT8_DISP8S();
                  }
                else {
                  res3.b.l = ~GET8_DISP8L();
                  PUT8_DISP8L();
                  }
                break;
              case WORD_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.w = ~GET16_DISP8S();
                  PUT16_DISP8S();
                  }
                else {
                  res3.w = ~GET16_DISP8L();
                  PUT16_DISP8L();
                  }
                break;
              case DWORD_SIZE:
                if(!DISPLACEMENT_SIZE) {
                  res3.d = ~GET32_DISP8S();
                  PUT32_DISP8S();
                  }
                else {
                  res3.d = ~GET32_DISP8L();
                  PUT32_DISP8L();
                  }
                break;
		          case SIZE_ELSE:
                if(_f.SR.Supervisor) {
		              if(!DISPLACEMENT_SIZE)
                    res3.w = GET16_DISP8S();
									else
  	                res3.w = GET16_DISP8L();
									goto aggSR;
                  }
                else {
                  goto doPrivilegeTrap;
                  }
			          break;
              }
            break;
          }
				goto aggFlag;
				break;
        
			case 0x48:   // EXT NBCD SWAP PEA MOVEM (bkpt)
        switch((Pipe1.b[0] & 0xf8)) {
          case 0x00:    // NBCD   VERIFICARE!!! finire
          case 0x08:
          case 0x10:
          case 0x18:
          case 0x20:
          case 0x28:
          case 0x30:
          case 0x38:
            switch(ADDRESSING) {
              case ABSOLUTEADD:
                switch(ABSOLUTEADD_MODE) {
                  case ABSSSUBADD:
                    res3.b.l = GetValue((int16_t)Pipe2.w);
                    _pc+=AdvPipe(_pc,2);
                    break;
                  case ABSLSUBADD:
                    res3.b.l = GetValue(Pipe2.d);
                    _pc+=AdvPipe(_pc,4);
                    break;
                  case PCIDXSUBADD:   // qua no!
                  case PCDISPSUBADD:   
                  case IMMSUBADD:
                    break;
                  }
                break;
              case DATAREGADD:   // Dn
                res3.b.l = WORKING_REG_D.b.b0;
                break;
              case ADDRREGADD:   // An qua  no!
                break;
              case ADDRADD:   // (An)
                res3.b.l = GetValue(WORKING_REG_A.d);
                break;
              case ADDRPINCADD:   // (An)+
                res3.b.l = GET8_PINC();
                break;
              case ADDRPDECADD:   // -(An)
                res3.b.l = GET8_PDEC();
                break;
              case ADDRDISPADD:   // (D16,An)
                res3.b.l = GET8_DISP16();
                _pc+=AdvPipe(_pc,2);
                break;
              case ADDRIDXADD:   // (D8,An,Xn)
                if(!DISPLACEMENT_SIZE)
                  res3.b.l = GET8_DISP8S();
                else
                  res3.b.l = GET8_DISP8L();
                _pc+=AdvPipe(_pc,2);
                break;
              }
            res2.w = + _f.CCR.Ext;        // FINIRE!!! v.SBCD
            res3.w = (WORD)(res1.b.l & 0xf) - (WORD)(res2.b.l & 0xf);
            res3.w = (((res1.b.l & 0xf0) >> 4) - ((res2.b.l & 0xf0) >> 4) - (res3.b.h ? 1 : 0)) | 
                      res3.b.l;
            switch(ADDRESSING) {
              case ABSOLUTEADD:
                switch(ABSOLUTEADD_MODE) {
                  case ABSSSUBADD:
                    PutValue((int16_t)Pipe2.w,res3.b.l);
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
              case ADDRREGADD:   // An qua  no!
                break;
              case ADDRADD:   // (An)
                PutValue(DEST_REG_A.d,res3.b.l);
                break;
              case ADDRPINCADD:   // (An)+
                PutValue(DEST_REG_A.d,res3.b.l);
                DEST_REG_A.d += ((Pipe1.b[1] & 0xe) == 0xe) ? 2 : 1;
                break;
              case ADDRPDECADD:   // -(An)
                DEST_REG_A.d -= ((Pipe1.b[1] & 0xe) == 0xe) ? 2 : 1;
                PutValue(DEST_REG_A.d,res3.b.l);
                break;
              case ADDRDISPADD:   // (D16,An)
                PutValue(DEST_REG_A.d + (int16_t)Pipe2.w,res3.b.l);
                _pc+=2;
                break;
              case ADDRIDXADD:   // (D8,An,Xn)
                if(!DISPLACEMENT_SIZE)
                  PutValue(DEST_REG_A.d + (int16_t)DISPLACEMENT_REG.w.l + (signed char)Pipe2.b.l,res3.b.l);
                else
                  PutValue(DEST_REG_A.d + (int32_t)DISPLACEMENT_REG.d + (signed char)Pipe2.b.l,res3.b.l);
                _pc+=2;
                break;
              }
            if(res3.b.l) _f.CCR.Zero=0;
            _f.CCR.Ext=_f.CCR.Carry=!!res3.b.h;		// v. metodo usato per ADD e SUB senza byte alto...
            goto noAggFlag;
            break;
          case 0x40:    // SWAP
            res3.d = MAKELONG(WORKING_REG_D.w.h,WORKING_REG_D.w.l);
            WORKING_REG_D.d = res3.d;
            goto aggFlag4;
            break;
          case 0x80:    // EXT.w
            res3.w = (signed char)WORKING_REG_D.b.b0;
            WORKING_REG_D.w.l = res3.w;
            goto aggFlag2;
            break;
          case 0xc0:    // EXT.l
            res3.d = (int16_t)WORKING_REG_D.w.l;
            WORKING_REG_D.d = res3.d;
            goto aggFlag4;
#ifdef MC68020
            // fare..  v. b0 di HIBYTE di Pipe1
            res3.d = (signed char)WORKING_REG_D.b.b0;
            WORKING_REG_D.d = res3.d;
            goto aggFlag4;
#endif
            break;
// questo no          case 0x48:    // PEA
          case 0x50:
//          case 0x58:
//          case 0x60:
          case 0x68:
          case 0x70:
          case 0x78:
            switch(ADDRESSING) {    // v. LEA ..
              case ABSOLUTEADD:
                switch(ABSOLUTEADD_MODE) {
                  case ABSSSUBADD:
                    res3.d = (int16_t)Pipe2.w;
                    _pc+=2;
                    break;
                  case ABSLSUBADD:
                    res3.d = Pipe2.d; 
                    _pc+=4;
                    break;
                  case PCIDXSUBADD:   // (D16,PC)
                    res3.d = DISPPC16();
                    _pc+=2;
                    break;
                  case PCDISPSUBADD:   // (D8,Xn,PC)
                    if(!DISPLACEMENT_SIZE)
                      res3.d = DISPPC8S();
                    else
                      res3.d = DISPPC8L();
                    _pc+=2;
                    break;
                  case IMMSUBADD:				// qua no!
                    break;
                  }
                break;
              case DATAREGADD:   // Dn 
                break;
              case ADDRREGADD:   // An        // qua no! opp BKPT
#ifdef MC68010
												    // BKPT ANCHE altri finire
#endif
                break;
              case ADDRADD:   // (An)
                res3.d = WORKING_REG_A.d; //
                break;
              case ADDRPINCADD:   // (An)+			// qua no!
              case ADDRPDECADD:   // -(An)
                break;
              case ADDRDISPADD:   // (D16,An)
                res3.d = DISP16();
                _pc+=2;
                break;
              case ADDRIDXADD:   // (D8,An,Xn)
                if(!DISPLACEMENT_SIZE)
                  res3.d = DISP8S();
                else
                  res3.d = DISP8L();
                _pc+=2;
                break;
              }
            _sp-=4;
            PutIntValue(_sp,res3.d);
            break;
          default:      // MOVEM register -> memory
						if(Pipe1.b[0] & 0b10000000) {
							res2.w = Pipe2.w;
							_pc+=AdvPipe(_pc,2);
							switch(ADDRESSING) {
								case ABSOLUTEADD:
									switch(ABSOLUTEADD_MODE) {
										case ABSSSUBADD:
											res3.d = (int16_t)Pipe2.w;
											MOVEM_R2M()
											_pc+=2;
											break;
										case ABSLSUBADD:
											res3.d = Pipe2.d;
											MOVEM_R2M()
											_pc+=4;
											break;
										case PCIDXSUBADD:   // (D16,PC)
										case PCDISPSUBADD:   // (D8,Xn,PC)
										case IMMSUBADD:   // qua no!
											break;
										}
									break;
								case DATAREGADD:   // Dn NO!
								case ADDRREGADD:   // An NO!
									break;
								case ADDRADD:   // (An)
									res3.d = WORKING_REG_A.d;
									MOVEM_R2M()
									break;
								case ADDRPINCADD:   // (An)+ qua NO!
									break;
								case ADDRPDECADD:   // -(An)
									res3.d = WORKING_REG_A.d;
									MOVEM_R2M_PREDEC()
									WORKING_REG_A.d = res3.d;   //
									break;
								case ADDRDISPADD:   // (D16,An)
									res3.d = DISP16();
									MOVEM_R2M()
									_pc+=2;
									break;
								case ADDRIDXADD:   // (D8,An,Xn)
									if(!DISPLACEMENT_SIZE)
										res3.d = DISP8S();
									else
										res3.d = DISP8L();
									MOVEM_R2M()
									_pc+=2;
									break;
								}
							}
            break;
          }
				break;
        
			case 0x4a:   // ILLEGAL TAS TST 
        switch(ADDRESSING) {
	        case ABSOLUTEADD:
						switch(ABSOLUTEADD_MODE) {
							case ABSSSUBADD:
								switch(OPERAND_SIZE) {
									case BYTE_SIZE:
										res3.b.l = GetValue((int16_t)Pipe2.w);
										break;
									case WORD_SIZE:
										res3.w = GetShortValue((int16_t)Pipe2.w);
										break;
									case DWORD_SIZE:
										res3.d = GetIntValue((int16_t)Pipe2.w);
										break;
									case SIZE_ELSE:      // ILLEGAL TAS
										if(Pipe1.b[0] != 0xfc) {   // TAS
											res3.b.l = GetValue((int16_t)Pipe2.w);
											PutValue((int16_t)Pipe2.w,res3.b.l | 0x80);
											}
										else {    // ILLEGAL
illegal_trap:
											res3.b.l=4;
											goto doTrap;
											}
										break;
									}
								_pc+=2;
								break;
							case ABSLSUBADD:
								switch(OPERAND_SIZE) {
									case BYTE_SIZE:
										res3.b.l = GetValue(Pipe2.d);
										break;
									case WORD_SIZE:
										res3.w = GetShortValue(Pipe2.d);
										break;
									case DWORD_SIZE:
										res3.d = GetIntValue(Pipe2.d);
										break;
									case SIZE_ELSE:      // ILLEGAL TAS
										if(Pipe1.b[0] != 0xfc) {   // TAS
											res3.b.l = GetValue(Pipe2.d);
											PutValue(Pipe2.d,res3.b.l | 0x80);
											}
										else {    // ILLEGAL
        							goto illegal_trap;
											}
										break;
									}
								_pc+=4;
								break;
              case PCIDXSUBADD:   // (D16,PC)
#ifdef MC68020
								switch(OPERAND_SIZE) {
									case BYTE_SIZE:
		                res3.b.l = GET8_DISPPC16();
										break;
									case WORD_SIZE:
		                res3.w = GET16_DISPPC16();
										break;
									case DWORD_SIZE:
		                res3.d = GET32_DISPPC16();
										break;
									case SIZE_ELSE:      // ILLEGAL 
      							goto illegal_trap;
										break;
									}
                _pc+=2;
#endif
                break;
              case PCDISPSUBADD:   // (D8,Xn,PC)
#ifdef MC68020
								switch(OPERAND_SIZE) {
									case BYTE_SIZE:
										if(!DISPLACEMENT_SIZE)
											res3.b.l = GET8_DISPPC8S();
										else
											res3.b.l = GET8_DISPPC8L();
										break;
									case WORD_SIZE:
										if(!DISPLACEMENT_SIZE)
											res3.w = GET16_DISPPC8S();
										else
											res3.w = GET16_DISPPC8L();
										break;
									case DWORD_SIZE:
										if(!DISPLACEMENT_SIZE)
											res3.d = GET32_DISPPC8S();
										else
											res3.d = GET32_DISPPC8L();
										break;
									case SIZE_ELSE:      // ILLEGAL 
       							goto illegal_trap;
										break;
									}
                _pc+=2;
#endif
                break;
              case IMMSUBADD:
#ifdef MC68020
								switch(OPERAND_SIZE) {
									case BYTE_SIZE:
		                res3.b.l = Pipe2.b.l;
										break;
									case WORD_SIZE:
		                res3.w = Pipe2.w; 
										break;
									case DWORD_SIZE:
		                res3.d = Pipe2.d;
										break;
									case SIZE_ELSE:      // ILLEGAL 
       							goto illegal_trap;
										break;
									}
                _pc+=2;
#endif
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
                res3.d = WORKING_REG_D.d;
                break;
              case SIZE_ELSE:    // TAS
                res3.b.l = WORKING_REG_D.b.b0;
                WORKING_REG_D.b.b0 |= 0x80;
                break;
              }
            break;
          case ADDRREGADD:   // An solo 68020 e solo TST e solo word, long
#ifdef MC68020
            switch(OPERAND_SIZE) {
              case WORD_SIZE:
                res3.w = WORKING_REG_A.w.l;
                break;
              case DWORD_SIZE:
                res3.d = WORKING_REG_A.d;
                break;
              }
#endif
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GetValue(WORKING_REG_A.d);
                break;
              case WORD_SIZE:
                res3.w = GetShortValue(WORKING_REG_A.d);
                break;
              case DWORD_SIZE:
                res3.d = GetIntValue(WORKING_REG_A.d);
                break;
              case SIZE_ELSE:    // TAS
                res3.b.l = GetValue(WORKING_REG_A.d);
                PutValue(WORKING_REG_A.d, res3.b.l | 0x80);
                break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GET8_PINC();
                break;
              case WORD_SIZE:
                res3.w = GET16_PINC();
                break;
              case DWORD_SIZE:
                res3.d = GET32_PINC();
                break;
              case SIZE_ELSE:    // TAS
                res3.b.l = GetValue(WORKING_REG_A.d);
                PutValue(WORKING_REG_A.d, res3.b.l | 0x80);
                WORKING_REG_A.d += IS_A7() ? 2 : 1;
                break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GET8_PDEC();
                break;
              case WORD_SIZE:
                res3.w = GET16_PDEC();
                break;
              case DWORD_SIZE:
                res3.d = GET32_PDEC();
                break;
              case SIZE_ELSE:    // TAS
                WORKING_REG_A.d -= IS_A7() ? 2 : 1;
                res3.b.l = GetValue(WORKING_REG_A.d);
                PutValue(WORKING_REG_A.d, res3.b.l | 0x80);
                break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res3.b.l = GET8_DISP16();
                break;
              case WORD_SIZE:
                res3.w = GET16_DISP16();
                break;
              case DWORD_SIZE:
                res3.d = GET32_DISP16();
                break;
              case SIZE_ELSE:    // TAS
                res3.b.l = GET8_DISP16();
                PutValue(DISP16(), res3.b.l | 0x80);
                break;
              }
            _pc+=2;
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                if(!DISPLACEMENT_SIZE)
                  res3.b.l = GET8_DISP8S();
                else
                  res3.b.l = GET8_DISP8L();
                break;
              case WORD_SIZE:
                if(!DISPLACEMENT_SIZE)
                  res3.w = GET16_DISP8S();
                else
                  res3.w = GET16_DISP8L();
                break;
              case DWORD_SIZE:
                if(!DISPLACEMENT_SIZE)
                  res3.d = GET32_DISP8S();
                else
                  res3.d = GET32_DISP8L();
                break;
              case SIZE_ELSE:    // TAS
                if(!DISPLACEMENT_SIZE)
                  res3.b.l = GET8_DISP8S();
                else
                  res3.b.l = GET8_DISP8L();
                if(!DISPLACEMENT_SIZE)
                  PutValue(DISP8S(), res3.b.l | 0x80);
                else
                  PutValue(DISP8L(), res3.b.l | 0x80);
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
				if(Pipe1.b[0] & 0b10000000) {
					res2.w = Pipe2.w;
					_pc+=AdvPipe(_pc,2);
					switch(ADDRESSING) {
						case ABSOLUTEADD:
							switch(ABSOLUTEADD_MODE) {
								case ABSSSUBADD:
									res3.d = (int16_t)Pipe2.w;
									MOVEM_M2R()
									_pc+=2;
									break;
								case ABSLSUBADD:
									res3.d = Pipe2.d;
									MOVEM_M2R()
									_pc+=4;
									break;
								case PCIDXSUBADD:   // (D16,PC)
									res3.d = DISPPC16();
									MOVEM_M2R()
									_pc+=2;
									break;
								case PCDISPSUBADD:   // (D8,Xn,PC)
									if(!DISPLACEMENT_SIZE)
										res3.d = DISPPC8S();
									else
										res3.d = DISPPC8L();
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
							res3.d = WORKING_REG_A.d;
							MOVEM_M2R()
							break;
						case ADDRPINCADD:   // (An)+
							res3.d = WORKING_REG_A.d;
							MOVEM_M2R()
							WORKING_REG_A.d = res3.d;   // 
							break;
						case ADDRPDECADD:   // -(An) qua NO!
							break;
						case ADDRDISPADD:   // (D16,An)
							res3.d = DISP16();
							MOVEM_M2R()
							_pc+=2;
							break;
						case ADDRIDXADD:   // (D8,An,Xn)
							if(!DISPLACEMENT_SIZE)
								res3.d = DISP8S();
							else
								res3.d = DISP8L();
							MOVEM_M2R()
							_pc+=2;
							break;
						}
					}
				break;
        
			case 0x4e:   // TRAP LINK UNLK MOVEUSP RESET NOP STOP RTE RTS TRAPV RTR JSR JMP
        switch(Pipe1.b[0]) {
          case 0x70:   // RESET
            if(_f.SR.Supervisor) {
              ActivateReset=124;
              }
            else {
              goto doPrivilegeTrap;
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
              goto doPrivilegeTrap;
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
              goto doPrivilegeTrap;
              }
            break;
#ifdef MC68010
          case 0x74:   // RTD
            _pc=GetIntValue(_sp);
            _sp+=4;  _sp+=(int16_t)Pipe2.w;
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
            _f.CCR.b=GetValue(_sp);
            _sp+=2;
            _pc=GetIntValue(_sp);
            _sp+=4;
            break;
#ifdef MC68010
          case 0x7a:   // MOVEC   http://oldwww.nvg.ntnu.no/amiga/MC680x0_Sections/movec.HTML
          case 0x7b:   // 
            if(_f.SR.Supervisor) {
              switch(Pipe2.w & 0x0fff) {
                case 0x000:
                  //SFC
                  break;
                case 0x001:
                  // DSP
                  break;
                case 0x002:
                  // CACR
                  break;
#ifdef MC68020
                case 0x800:
     							a7U = WORKING_REG_A.d;

                  USP;
                  break;
                case 0x801:
                  // VBR
                  break;
                case 0x802:
                  // CAAR
                  break;
                case 0x803:
                  // MSP
                  break;
                case 0x804:
                  // ISP
                  break;
#endif
                }
              
              if(Pipe1 & 1 /*DIRECTION*/) {   // Rc -> Rn
                }
              else {            // Rn -> Rc
                }
              }
            else {
              goto doPrivilegeTrap;
              }
            break;
#endif
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
            res3.b.l=Pipe1.b[0] & 0xf;
            res3.b.l+=32;      // trap utente DOPO le trap/IRQ
            
doTrap:
            FC=0b110;     // supervisor mode, program 
            _fsup=_f;
            if(!_f.SR.Supervisor)
              a7U=regsA[7].d;
            else
              a7S=regsA[7].d;
            _f.SR.Supervisor=1;
            regsA[7].d=a7S;
            _sp-=4;
            PutIntValue(_sp,_pc); // low word prima
            _sp-=2;
            PutShortValue(_sp,_fsup.w);
            _pc=GetIntValue(res3.b.l*4);
            
//            printf("68K trap %u\r\n",res3.b.l);
            break;

          case 0x50:   // LINK
          case 0x51:
          case 0x52:
          case 0x53:
          case 0x54:
          case 0x55:
          case 0x56:
          case 0x57:
#ifdef MC68020
						if((Pipe1.b[0] & 0xf8) == 0xf8) {
						TRAPcc
							switch(Pipe1.b[0] & 7) {
								case 2:
									break;
								case 3:
									break;
								case 4:
									break;
								}
							}
						else {
#endif
            _sp-=4;
            PutIntValue(_sp,WORKING_REG_A.d);
            WORKING_REG_A.d=_sp;
            _sp+=(int16_t)Pipe2.w;
            _pc+=2;
#ifdef MC68020
						}
#endif
            break;
          case 0x58:    //UNLK
          case 0x59:
          case 0x5a:
          case 0x5b:
          case 0x5c:
          case 0x5d:
          case 0x5e:
          case 0x5f:
#ifdef MC68020
						if((Pipe1.b[0] & 0xf8) == 0xf8) {
						TRAPcc
							switch(Pipe1.b[0] & 7) {
								case 2:
									break;
								case 3:
									break;
								case 4:
									break;
								}
							}
						else {
#endif
            _sp=WORKING_REG_A.d;
            WORKING_REG_A.d=GetIntValue(_sp);
            _sp+=4;
#ifdef MC68020
						}
#endif
            break;
          case 0x60:   // MOVE USP
          case 0x61:
          case 0x62:
          case 0x63:
          case 0x64:
          case 0x65:
          case 0x66:
          case 0x67:
						if(_f.SR.Supervisor) {
							a7U = WORKING_REG_A.d;
							}
						else {
							goto doPrivilegeTrap;
							}
            break;
          case 0x68:		// MOVE USP
          case 0x69:
          case 0x6a:
          case 0x6b:
          case 0x6c:
          case 0x6d:
          case 0x6e:
          case 0x6f:
						if(_f.SR.Supervisor) {
							WORKING_REG_A.d = a7U;
							}
						else {
							goto doPrivilegeTrap;
							}
            break;
          default:
#if !defined(MC68010) && !defined(MC68020)
            if(!(Pipe1.b[0] & 0b10000000)) {   // v.sopra
							goto illegal_trap;
              }
#endif
            if(!(Pipe1.b[0] & 0b01000000)) {   // JSR
              _sp-=4;
              switch(ADDRESSING) {
                case ABSOLUTEADD:
                  switch(ABSOLUTEADD_MODE) {
                    case ABSSSUBADD:
                      PutIntValue(_sp,_pc+2);  // low word prima
                      _pc=(int16_t)Pipe2.w;
                      break;
                    case ABSLSUBADD:
                      PutIntValue(_sp,_pc+4);  // 
                      _pc=Pipe2.d;
                      break;
                    case PCIDXSUBADD:   // (D16,PC)
                      PutIntValue(_sp,_pc+2);  // 
                      _pc = DISPPC16();
                      break;
                    case PCDISPSUBADD:   // (D8,Xn,PC)
                      PutIntValue(_sp,_pc+2);  // 
                      if(!DISPLACEMENT_SIZE)
                        _pc = DISPPC8S();
                      else
                        _pc = DISPPC8L();
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
                  _pc=WORKING_REG_A.d;
                  break;
                case ADDRPINCADD:   // (An)+    qua no!
                case ADDRPDECADD:   // -(An)
                  break;
                case ADDRDISPADD:   // (D16,An)
                  PutIntValue(_sp,_pc+2);  // 
                  _pc=DISP16();
                  break;
                case ADDRIDXADD:   // (D8,An,Xn)
                  PutIntValue(_sp,_pc+2);  // 
                  if(!DISPLACEMENT_SIZE)
                    _pc=DISP8S();
                  else
                    _pc=DISP8L();
                  break;
                }
              }
            else {    // JMP
              switch(ADDRESSING) {
                case ABSOLUTEADD:
                  switch(ABSOLUTEADD_MODE) {
                    case ABSSSUBADD:
                      _pc=(int16_t)Pipe2.w;
                      break;
                    case ABSLSUBADD:
                      _pc=Pipe2.d;
                      break;
                    case PCIDXSUBADD:   // (D16,PC)
                      _pc = DISPPC16();
                      break;
                    case PCDISPSUBADD:   // (D8,Xn,PC)
                      if(!DISPLACEMENT_SIZE) //if 68020 MOVEC il Macintosh usa questo per vedere tipo CPU!
                        _pc = DISPPC8S();
                      else
                        _pc = DISPPC8L();
                      break;
                    case IMMSUBADD:   // qua no!
                      break;
                    }
                  break;
                case DATAREGADD:   // Dn
                case ADDRREGADD:   // An
                  break;
                case ADDRADD:   // (An)
                  _pc=WORKING_REG_A.d;
                  break;
                case ADDRPINCADD:   // (An)+
                case ADDRPDECADD:   // -(An)
                  break;
                case ADDRDISPADD:   // (D16,An)
                  _pc=DISP16();
                  break;
                case ADDRIDXADD:   // (D8,An,Xn)
                  if(!DISPLACEMENT_SIZE)
                    _pc=DISP8S();
                  else
                    _pc=DISP8L();
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
            switch(Pipe1.b[1] & 0xf) {
              case 0x0:
                res3.b.l=0xff;    // 
                break;
              case 0x2:						// Shi    //https://mrjester.hapisan.com/04_MC68/Sect07Part04/Index.html
                if(!_f.CCR.Carry && !_f.CCR.Zero)     // higher !C !Z
                  res3.b.l=0xff;
                else
                  res3.b.l=0x00;
                break;
              case 0x4:						// Scc
                if(!_f.CCR.Carry)     // carry clear
                  res3.b.l=0xff;
                else
                  res3.b.l=0x00;
                break;
              case 0x6:						// Sne
                if(!_f.CCR.Zero)      // not equal
                  res3.b.l=0xff;
                else
                  res3.b.l=0x00;
                break;
              case 0x8:						// Svc
                if(!_f.CCR.Ovf)      // overflow clear
                  res3.b.l=0xff;
                else
                  res3.b.l=0x00;
                break;
              case 0xa:						// Spl
                if(!_f.CCR.Sign)     // plus
                  res3.b.l=0xff;
                else
                  res3.b.l=0x00;
                break;
              case 0xc:						// Sge 
                if(_f.CCR.Sign == _f.CCR.Ovf)     // greater or equal
                  res3.b.l=0xff;
                else
                  res3.b.l=0x00;
                break;
              case 0xe:						// Sgt 
                if(1 & (~_f.CCR.Zero & ~(_f.CCR.Sign ^ _f.CCR.Ovf)))     // greater than
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
                  _pc+=(int16_t)Pipe2.w;
                else
                  _pc+=2;
                }
              else
                _pc+=2;
              }
            else {      // Scc
              switch(ADDRESSING) {
                case ABSOLUTEADD:    // abs
                  switch(ABSOLUTEADD_MODE) {
                    case ABSSSUBADD:
                      PutValue((int16_t)Pipe2.w, res3.b.l);
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
                  PutValue(WORKING_REG_A.d,res3.b.l);
                  break;
                case ADDRPINCADD:   // (An)+
                  PUT8_PINC();
                  break;
                case ADDRPDECADD:   // -(An)
                  PUT8_PDEC();
                  break;
                case ADDRDISPADD:   // (D16,An)
                  PUT8_DISP16();
                  _pc+=2;
                  break;
                case ADDRIDXADD:   // (D8,An,Xn)
                  if(!DISPLACEMENT_SIZE)
                    PUT8_DISP8S();
                  else
                    PUT8_DISP8L();
                  _pc+=2;
                  break;
                }
              }
            break;
          default:      // ADDQ
            res2.b.l=Q8BIT;
            if(!res2.b.l)
              res2.b.l=8;
            switch(ADDRESSING) {
              case ABSOLUTEADD:    // abs
                switch(ABSOLUTEADD_MODE) {
                  case ABSSSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res1.b.l = GetValue((int16_t)Pipe2.w);
                        res3.b.l = res1.b.l + res2.b.l;
                        PutValue((int16_t)Pipe2.w, res3.b.l);
                        break;
                      case WORD_SIZE:
                        res1.w = GetShortValue((int16_t)Pipe2.w);
                        res3.w = res1.w + res2.b.l;
                        PutShortValue((int16_t)Pipe2.w, res3.w);
                        break;
                      case DWORD_SIZE:
                        res1.d = GetIntValue((int16_t)Pipe2.w);
                        res3.d = res1.d + res2.b.l;
                        PutIntValue((int16_t)Pipe2.w, res3.d);
                        break;
                      }
                    _pc+=2;
                    break;
                  case ABSLSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res1.b.l = GetValue(Pipe2.d);
                        res3.b.l = res1.b.l + res2.b.l;
                        PutValue(Pipe2.d,res3.b.l);
                        break;
                      case WORD_SIZE:
                        res1.w = GetShortValue(Pipe2.d);
                        res3.w = res1.w + res2.b.l;
                        PutShortValue(Pipe2.d,res3.w);
                        break;
                      case DWORD_SIZE:
                        res1.d = GetIntValue(Pipe2.d);
                        res3.d = res1.d + res2.b.l;
                        PutIntValue(Pipe2.d,res3.d);
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
                    WORKING_REG_D.b.b0 = res3.b.l = res1.b.l + res2.b.l;
                    break;
                  case WORD_SIZE:
                    res1.w = WORKING_REG_D.w.l;
                    WORKING_REG_D.w.l = res3.w = res1.w + res2.b.l;
                    break;
                  case DWORD_SIZE:
                    res1.d = WORKING_REG_D.d;
                    WORKING_REG_D.d = res3.d = res1.d + res2.b.l;
                    break;
                  }
                break;
              case ADDRREGADD:   // An
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    break;
                  case WORD_SIZE:
                  case DWORD_SIZE:
                    res3.d = WORKING_REG_A.d += res2.b.l;		// tutto cmq.. dice..
                    break;
                  }
                goto noAggFlag;
                break;
              case ADDRADD:   // (An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GetValue(WORKING_REG_A.d);
                    res3.b.l = res1.b.l + res2.b.l;
                    PutValue(WORKING_REG_A.d,res3.b.l);
                    break;
                  case WORD_SIZE:
                    res1.w = GetShortValue(WORKING_REG_A.d);
                    res3.w = res1.w + res2.b.l;
                    PutShortValue(WORKING_REG_A.d,res3.w);
                    break;
                  case DWORD_SIZE:
                    res1.d = GetIntValue(WORKING_REG_A.d);
                    res3.d = res1.d + res2.b.l;
                    PutIntValue(WORKING_REG_A.d,res3.d);
                    break;
                  }
                break;
              case ADDRPINCADD:   // (An)+
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GetValue(WORKING_REG_A.d);
                    res3.b.l = res1.b.l + res2.b.l;
                    PUT8_PINC();
                    break;
                  case WORD_SIZE:
                    res1.w = GetShortValue(WORKING_REG_A.d);
                    res3.w = res1.w + res2.b.l;
                    PUT16_PINC();
                    break;
                  case DWORD_SIZE:
                    res1.d = GetIntValue(WORKING_REG_A.d);
                    res3.d = res1.d + res2.b.l;
                    PUT32_PINC();
                    break;
                  }
                break;
              case ADDRPDECADD:   // -(An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GET8_PDEC();
                    res3.b.l = res1.b.l + res2.b.l;
                    PutValue(WORKING_REG_A.d,res3.b.l);
                    break;
                  case WORD_SIZE:
                    res1.w = GET16_PDEC();
                    res3.w = res1.w + res2.b.l;
                    PutShortValue(WORKING_REG_A.d,res3.w);
                    break;
                  case DWORD_SIZE:
                    res1.d = GET32_PDEC();
                    res3.d = res1.d + res2.b.l;
                    PutIntValue(WORKING_REG_A.d,res3.d);
                    break;
                  }
                break;
              case ADDRDISPADD:   // (D16,An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GET8_DISP16();
                    res3.b.l = res1.b.l + res2.b.l;
                    PUT8_DISP16();
                    break;
                  case WORD_SIZE:
                    res1.w = GET16_DISP16();
                    res3.w = res1.w + res2.b.l;
                    PUT16_DISP16();
                    break;
                  case DWORD_SIZE:
                    res3.d = GET32_DISP16();
                    res3.d = res1.d + res2.b.l;
                    PUT32_DISP16();
                    break;
                  }
                _pc+=2;
                break;
              case ADDRIDXADD:   // (D8,An,Xn)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    if(!DISPLACEMENT_SIZE) {
                      res1.b.l = GET8_DISP8S();
                      res3.b.l = res1.b.l + res2.b.l;
                      PUT8_DISP8S();
                      }
                    else {
                      res1.b.l = GET8_DISP8L();
                      res3.b.l = res1.b.l + res2.b.l;
                      PUT8_DISP8L();
                      }
                    break;
                  case WORD_SIZE:
                    if(!DISPLACEMENT_SIZE) {
                      res1.w = GET16_DISP8S();
                      res3.w = res1.w + res2.b.l;
                      PUT16_DISP8S();
                      }
                    else {
                      res1.w = GET16_DISP8L();
                      res3.w = res1.w + res2.b.l;
                      PUT16_DISP8L();
                      }
                    break;
                  case DWORD_SIZE:
                    if(!DISPLACEMENT_SIZE) {
                      res1.d = GET32_DISP8S();
                      res3.d = res1.d + res2.b.l;
                      PUT32_DISP8S();
                      }
                    else {
                      res1.d = GET32_DISP8L();
                      res3.d = res1.d + res2.b.l;
                      PUT32_DISP8L();
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
            switch(Pipe1.b[1] & 0xf) {
              case 0x1:
                res3.b.l=0x00;    // 
                break;
              case 0x3:			// Sls
                if(_f.CCR.Carry || _f.CCR.Zero)     // lower or equal
                  res3.b.l=0xff;
                else
                  res3.b.l=0x00;
                break;
              case 0x5:			// Scs
                if(_f.CCR.Carry)      // carry set
                  res3.b.l=0xff;
                else
                  res3.b.l=0x00;
                break;
              case 0x7:			// Seq
                if(_f.CCR.Zero)      // equal
                  res3.b.l=0xff;
                else
                  res3.b.l=0x00;
                break;
              case 0x9:			// Svs
                if(_f.CCR.Ovf)       // overflow set
                  res3.b.l=0xff;
                else
                  res3.b.l=0x00;
                break;
              case 0xb:			// Smi
                if(_f.CCR.Sign)      // minus
                  res3.b.l=0xff;
                else
                  res3.b.l=0x00;
                break;
              case 0xd:			// Slt
                if(_f.CCR.Sign != _f.CCR.Ovf)     // less than
                  res3.b.l=0xff;
                else
                  res3.b.l=0x00;
                break;
              case 0xf:			// Sle
                if(_f.CCR.Zero | (_f.CCR.Sign ^ _f.CCR.Ovf))     // less or equal
                  res3.b.l=0xff;
                else
                  res3.b.l=0x00;
                break;
              }
            goto doDBccScc;
            break;
          default:        // SUBQ
            res2.b.l=Q8BIT;
            if(!res2.b.l)
              res2.b.l=8;
            switch(ADDRESSING) {
              case ABSOLUTEADD:    // abs
                switch(ABSOLUTEADD_MODE) {
                  case ABSSSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res1.b.l = GetValue((int16_t)Pipe2.w);
                        res3.b.l = res1.b.l - res2.b.l;
                        PutValue((int16_t)Pipe2.w,res3.b.l);
                        break;
                      case WORD_SIZE:
                        res1.w = GetShortValue((int16_t)Pipe2.w);
                        res3.w = res1.w - res2.b.l;
                        PutShortValue((int16_t)Pipe2.w,res3.w);
                        break;
                      case DWORD_SIZE:
                        res1.d = GetIntValue((int16_t)Pipe2.w);
                        res3.d = res1.d - res2.b.l;
                        PutIntValue((int16_t)Pipe2.w,res3.d); 
                        break;
                      }
                    _pc+=2;
                    break;
                  case ABSLSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res1.b.l = GetValue(Pipe2.d);
                        res3.b.l = res1.b.l - res2.b.l;
                        PutValue(Pipe2.d,res3.b.l);
                        break;
                      case WORD_SIZE:
                        res1.w = GetShortValue(Pipe2.d);
                        res3.w = res1.w - res2.b.l;
                        PutShortValue(Pipe2.d,res3.w);
                        break;
                      case DWORD_SIZE:
                        res1.d = GetIntValue(Pipe2.d); 
                        res3.d = res1.d - res2.b.l;
                        PutIntValue(Pipe2.d,res3.d); 
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
                    res3.b.l = WORKING_REG_D.b.b0 = res1.b.l - res2.b.l;
                    break;
                  case WORD_SIZE:
                    res1.w = WORKING_REG_D.w.l;
                    res3.w = WORKING_REG_D.w.l = res1.w - res2.b.l;
                    break;
                  case DWORD_SIZE:
                    res1.d = WORKING_REG_D.d;
                    res3.d = WORKING_REG_D.d = res1.d - res2.b.l;
                    break;
                  }
                break;
              case ADDRREGADD:   // An
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    break;
                  case WORD_SIZE:
                  case DWORD_SIZE:
                    res3.d = WORKING_REG_A.d -= res2.b.l;		// tutto cmq... dice..
                    break;
                  }
                goto noAggFlag;
                break;
              case ADDRADD:   // (An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GetValue(WORKING_REG_A.d);
                    res3.b.l = res1.b.l - res2.b.l;
                    PutValue(WORKING_REG_A.d,res3.b.l);
                    break;
                  case WORD_SIZE:
                    res1.w = GetShortValue(WORKING_REG_A.d);
                    res3.w = res1.w - res2.b.l;
                    PutShortValue(WORKING_REG_A.d,res3.w);
                    break;
                  case DWORD_SIZE:
                    res1.d = GetIntValue(WORKING_REG_A.d);
                    res3.d = res1.d - res2.b.l;
                    PutIntValue(WORKING_REG_A.d,res3.d);
                    break;
                  }
                break;
              case ADDRPINCADD:   // (An)+
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GetValue(WORKING_REG_A.d);
                    res3.b.l = res1.b.l - res2.b.l;
                    PUT8_PINC();
                    break;
                  case WORD_SIZE:
                    res1.w = GetShortValue(WORKING_REG_A.d);
                    res3.w = res1.w - res2.b.l;
                    PUT16_PINC();
                    break;
                  case DWORD_SIZE:
                    res1.d = GetIntValue(WORKING_REG_A.d);
                    res3.d = res1.d - res2.b.l;
                    PUT32_PINC();
                    break;
                  }
                break;
              case ADDRPDECADD:   // -(An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GET8_PDEC();
                    res3.b.l = res1.b.l - res2.b.l;
                    PutValue(WORKING_REG_A.d,res3.b.l);
                    break;
                  case WORD_SIZE:
                    res1.w = GET16_PDEC();
                    res3.w = res1.w - res2.b.l;
                    PutShortValue(WORKING_REG_A.d,res3.w);
                    break;
                  case DWORD_SIZE:
                    res1.d = GET32_PDEC();
                    res3.d = res1.d - res2.b.l;
                    PutIntValue(WORKING_REG_A.d,res3.d);
                    break;
                  }
                break;
              case ADDRDISPADD:   // (D16,An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GET8_DISP16();
                    res3.b.l = res1.b.l - res2.b.l;
                    PUT8_DISP16();
                    break;
                  case WORD_SIZE:
                    res1.w = GET16_DISP16();
                    res3.w = res1.w - res2.b.l;
                    PUT16_DISP16();
                    break;
                  case DWORD_SIZE:
                    res1.d = GET32_DISP16();
                    res3.d = res1.d - res2.b.l;
                    PUT32_DISP16();
                    break;
                  }
                _pc+=2;
                break;
              case ADDRIDXADD:   // (D8,An,Xn)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    if(!DISPLACEMENT_SIZE) {
                      res1.b.l = GET8_DISP8S();
                      res3.b.l = res1.b.l - res2.b.l;
                      PUT8_DISP8S();
                      }
                    else {
                      res1.b.l = GET8_DISP8L();
                      res3.b.l = res1.b.l - res2.b.l;
                      PUT8_DISP8L();
                      }
                    break;
                  case WORD_SIZE:
                    if(!DISPLACEMENT_SIZE) {
                      res1.w = GET16_DISP8S();
                      res3.w = res1.w - res2.b.l;
                      PUT16_DISP8S();
                      }
                    else {
                      res1.w = GET16_DISP8L();
                      res3.w = res1.w - res2.b.l;
                      PUT16_DISP8L();
                      }
                    break;
                  case DWORD_SIZE:
                    if(!DISPLACEMENT_SIZE) {
                      res1.d = GET32_DISP8S();
                      res3.d = res1.d - res2.b.l;
                      PUT32_DISP8S();
                      }
                    else {
                      res1.d = GET32_DISP8L();
                      res3.d = res1.d - res2.b.l;
                      PUT32_DISP8L();
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
        if(Pipe1.b[0]) {
#ifdef MC68020
        if(Pipe1.b[0] == 0xff) {
//					If the 8-bit displacement field in the instruction word is all ones ($FF), the 32-bit displacement
// (long word immediately following the instruction) is used.
          _pc+=4;???
          _pc+=(int32_t)Pipe2.d;
					}
				else {
#endif
          _pc+=(signed char)Pipe1.b[0];
#ifdef MC68020
					}
#endif
          }
        else {   // the 00 after it indicates .w
//          _pc+=2; no, strano..
          _pc+=(int16_t)Pipe2.w;
          }
				break;
        
			case 0x61:   // BSR
        _sp-=4;
        if(Pipe1.b[0]) {
#ifdef MC68020
        if(Pipe1.b[0] == 0xff) {
//					If the 8-bit displacement field in the instruction word is all ones ($FF), the 32-bit displacement
// (long word immediately following the instruction) is used.

          PutIntValue(_sp,_pc+4);  // 
          _pc+=(int32_t)Pipe2.d  _pc+4; no, strano..;
					}
				else {
#endif
          PutIntValue(_sp,_pc);  // low word prima
          _pc+=(signed char)Pipe1.b[0];
#ifdef MC68020
					}
#endif
          }
        else {    // the 00 after it indicates .w
          PutIntValue(_sp,_pc+2);  // low word prima
          _pc+=(int16_t)Pipe2.w /*_pc+2; no, strano..*/;
          }
				break;
        
			case 0x62:   // Bhi   //https://mrjester.hapisan.com/04_MC68/Sect07Part04/Index.html https://reversing.pl/literatura/68k/Programming_The_68000.pdf
        if(!_f.CCR.Carry && !_f.CCR.Zero)     // higher !C !Z
          goto do_bra;
        else
          goto no_bra;
				break;
			case 0x63:  // Bls
        if(_f.CCR.Carry || _f.CCR.Zero)     // lower or equal
          goto do_bra;
        else
          goto no_bra;
				break;
			case 0x64:			// Bcc
        if(!_f.CCR.Carry)     // carry clear
          goto do_bra;
        else
          goto no_bra;
				break;
			case 0x65:		  // Bcs		
        if(_f.CCR.Carry)      // carry set
          goto do_bra;
        else
          goto no_bra;
				break;
			case 0x66:    // Bne
        if(!_f.CCR.Zero)      // not equal
          goto do_bra;
        else
          goto no_bra;
				break;
			case 0x67:    // Beq
        if(_f.CCR.Zero)      // equal
          goto do_bra;
        else
          goto no_bra;
				break;
			case 0x68:		// Bvc
        if(!_f.CCR.Ovf)      // overflow clear
          goto do_bra;
        else
          goto no_bra;
				break;
			case 0x69:		// Bvs
        if(_f.CCR.Ovf)       // overflow set
          goto do_bra;
        else
          goto no_bra;
				break;
			case 0x6a:		// Bpl
        if(!_f.CCR.Sign)     // plus
          goto do_bra;
        else
          goto no_bra;
				break;
			case 0x6b:		// Bmi
        if(_f.CCR.Sign)      // minus
          goto do_bra;
        else
          goto no_bra;
				break;
			case 0x6c:    // Bge
        if(_f.CCR.Sign == _f.CCR.Ovf)     // greater or equal
          goto do_bra;
        else
          goto no_bra;
				break;
			case 0x6d:		// Blt
        if(_f.CCR.Sign != _f.CCR.Ovf)     // less than
          goto do_bra;
        else
          goto no_bra;
				break;
			case 0x6e:		// Bgt
        if(1 & (~_f.CCR.Zero & ~(_f.CCR.Sign ^ _f.CCR.Ovf)))     // greater than
          goto do_bra;
        else
          goto no_bra;
				break;
			case 0x6f:    // Ble
        if(_f.CCR.Zero | (_f.CCR.Sign ^ _f.CCR.Ovf))     // less or equal
          goto do_bra;
        else
no_bra:
          if(!Pipe1.b[0])
            _pc+=2;
#ifdef MC68020
          else if(Pipe1.b[0] == 0xff) {
            _pc+=4;
            }
#endif
				break;
        
			case 0x70:   // MOVEQ
			case 0x72:
			case 0x74:
			case 0x76:
			case 0x78:
			case 0x7a:
			case 0x7c:
			case 0x7e:
        res3.d = DEST_REG_D.d = (int8_t)Pipe1.b[0];
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
          res1.d=DEST_REG_D.d;
          switch(ADDRESSING) {
            case ABSOLUTEADD:
              switch(ABSOLUTEADD_MODE) {
                case ABSSSUBADD:
                  res2.w = GetShortValue((int16_t)Pipe2.w);
                  _pc+=2;
                  break;
                case ABSLSUBADD:
                  res2.w = GetShortValue(Pipe2.d); 
                  _pc+=4;
                  break;
                case PCIDXSUBADD:   // (D16,PC)
                  res2.w = GET16_DISPPC16();
                  _pc+=2;
                  break;
                case PCDISPSUBADD:   // (D8,Xn,PC)
                  if(!DISPLACEMENT_SIZE)
                    res2.w = GET16_DISPPC8S();
                  else
                    res2.w = GET16_DISPPC8L();
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
              res2.w = GetShortValue(WORKING_REG_A.d);
              break;
            case ADDRPINCADD:   // (An)+
              res2.w = GET16_PINC();
              break;
            case ADDRPDECADD:   // -(An)
              res2.w = GET16_PDEC();
              break;
            case ADDRDISPADD:   // (D16,An)
              res2.w = GET16_DISP16();
              _pc+=2;
              break;
            case ADDRIDXADD:   // (D8,An,Xn)
              if(!DISPLACEMENT_SIZE)
                res2.w = GET16_DISP8S();
              else
                res2.w = GET16_DISP8L();
              _pc+=2;
              break;
            }
          if(!res2.w) {
            res3.b.l=5;
            goto doTrap;
            }
          if(Pipe1.b[1] & 0b00000001) {		// DIVS
  					res3.d = (int32_t)res1.d / (int16_t)res2.w;
		        _f.CCR.Ovf=!!(HIWORD(res3.d));
						// If overflow is detected during division, the operands are unaffected  (in which case the Z- and N-bits are undefined)
						if(!_f.CCR.Ovf)
  						DEST_REG_D.d = MAKELONG(res3.w, (int32_t)res1.d % (int16_t)res2.w);
						}
					else {		// DIVU
  					res3.d = res1.d / res2.w;
						if(!_f.CCR.Ovf)
	  					DEST_REG_D.d = MAKELONG(res3.w, res1.d % res2.w);
            }
          _f.CCR.Zero=!res3.w;
          _f.CCR.Sign=!!(res3.w & 0x8000);
          _f.CCR.Carry=0;
					}
        else {      // SBCD OR 
          if(DIRECTION) { // 
            switch(ADDRESSING) {
              case ABSOLUTEADD:    // 
                switch(ABSOLUTEADD_MODE) {
                  case ABSSSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res1.b.l = GetValue((int16_t)Pipe2.w);
                        break;
                      case WORD_SIZE:
                        res1.w = GetShortValue((int16_t)Pipe2.w);
                        break;
                      case DWORD_SIZE:
                        res1.d = GetIntValue((int16_t)Pipe2.w);
                        break;
                      }
                    _pc+=2;
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
                        res1.d = GetIntValue(Pipe2.d);
                        break;
                      }
                    _pc+=4;
                    break;
                  case PCIDXSUBADD:   // (D16,PC)
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res1.b.l = GET8_DISPPC16();
                        break;
                      case WORD_SIZE:
                        res1.w = GET16_DISPPC16();
                        break;
                      case DWORD_SIZE:
                        res1.d = GET32_DISPPC16();
                        break;
                      }
                    _pc+=2;
                    break;
                  case PCDISPSUBADD:   // (D8,Xn,PC)
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        if(!DISPLACEMENT_SIZE)
                          res1.b.l = GET8_DISPPC8S();
                        else
                          res1.b.l = GET8_DISPPC8L();
                        break;
                      case WORD_SIZE:
                        if(!DISPLACEMENT_SIZE)
                          res1.w = GET16_DISPPC8S();
                        else
                          res1.w = GET16_DISPPC8L();
                        break;
                      case DWORD_SIZE:
                        if(!DISPLACEMENT_SIZE)
                          res1.d = GET32_DISPPC8S();
                        else
                          res1.d = GET32_DISPPC8L();
                        break;
                      }
                    _pc+=2;
                    break;
                  case IMMSUBADD:   // NON dovrebbe esistere qua perché confluisce in ORI, ma...
                    res1.d = Pipe2.d; 
                    _pc+=4;
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
                    res1.d = WORKING_REG_D.d;
                    break;
                  }
                break;
              case ADDRREGADD:   // An qua SBCD dovrebbe andare anche su DATAREG v. anche ABCD
                if(OPERAND_SIZE == 0) {
                  res1.b.l = GetValue(--WORKING_REG_A.d);
                  res2.b.l = GetValue(--DEST_REG_A.d) + _f.CCR.Ext;
                  res3.w = (WORD)(res1.b.l & 0xf) - (WORD)(res2.b.l & 0xf);
                  res3.w = (((res1.b.l & 0xf0) >> 4) - ((res2.b.l & 0xf0) >> 4) - (res3.b.h ? 1 : 0)) | 
                            res3.b.l;
                  PutValue(DEST_REG_A.d, res3.b.l);
                  }
                if(res3.b.l) _f.CCR.Zero=0;
                _f.CCR.Ext=_f.CCR.Carry=!!res3.b.h;
                goto noAggFlag;
                break;
              case ADDRADD:   // (An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GetValue(WORKING_REG_A.d);
                    break;
                  case WORD_SIZE:
                    res1.w = GetShortValue(WORKING_REG_A.d);
                    break;
                  case DWORD_SIZE:
                    res1.d = GetIntValue(WORKING_REG_A.d);
                    break;
                  }
                break;
              case ADDRPINCADD:   // (An)+
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GetValue(WORKING_REG_A.d);
                    // postinc DOPO!
                    break;
                  case WORD_SIZE:
                    res1.w = GetShortValue(WORKING_REG_A.d);
                    // postinc DOPO!
                    break;
                  case DWORD_SIZE:
                    res1.d = GetIntValue(WORKING_REG_A.d);
                    // postinc DOPO!
                    break;
                  }
                break;
              case ADDRPDECADD:   // -(An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GET8_PDEC();
                    break;
                  case WORD_SIZE:
                    res1.w = GET16_PDEC();
                    break;
                  case DWORD_SIZE:
                    res1.d = GET32_PDEC();
                    break;
                  }
                break;
              case ADDRDISPADD:   // (D16,An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GET8_DISP16();
                    break;
                  case WORD_SIZE:
                    res1.w = GET16_DISP16();
                    break;
                  case DWORD_SIZE:
                    res1.d = GET32_DISP16();
                    break;
                  }
                _pc+=2;
                break;
              case ADDRIDXADD:   // (D8,An,Xn)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res1.b.l = GET8_DISP8S();
                    else
                      res1.b.l = GET8_DISP8L();
                    break;
                  case WORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res1.w = GET16_DISP8S();
                    else
                      res1.w = GET16_DISP8L();
                    break;
                  case DWORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res1.d = GET32_DISP8S();
                    else
                      res1.d = GET32_DISP8L();
                    break;
                  }
                _pc+=2;
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
                res1.d = DEST_REG_D.d;
                break;
              }
            }

          if(DIRECTION) { // direction
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res2.b.l = DEST_REG_D.b.b0;
                break;
              case WORD_SIZE:
                res2.w = DEST_REG_D.w.l;
                break;
              case DWORD_SIZE:
                res2.d = DEST_REG_D.d;
                break;
              }
            }
          else {
            switch(ADDRESSING) {
              case ABSOLUTEADD:    // #imm
                switch(ABSOLUTEADD_MODE) {
                  case ABSSSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res2.b.l = GetValue((int16_t)Pipe2.w);
                        break;
                      case WORD_SIZE:
                        res2.w = GetShortValue((int16_t)Pipe2.w);
                        break;
                      case DWORD_SIZE:
                        res2.d = GetIntValue((int16_t)Pipe2.w);
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
                        res2.d = GetIntValue(Pipe2.d);
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
                // bah cmq è ok .. 2/1/23  #warning MA VA IN CONFLITTO CON SBCD! v. anche sopra
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = WORKING_REG_D.b.b0;
                    break;
                  case WORD_SIZE:
                    res2.w = WORKING_REG_D.w.l;
                    break;
                  case DWORD_SIZE:
                    res2.d = WORKING_REG_D.d;
                    break;
                  }
                break;
              case ADDRREGADD:   // An
                break;
              case ADDRADD:   // (An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = GetValue(WORKING_REG_A.d);
                    break;
                  case WORD_SIZE:
                    res2.w = GetShortValue(WORKING_REG_A.d);
                    break;
                  case DWORD_SIZE:
                    res2.d = GetIntValue(WORKING_REG_A.d);
                    break;
                  }
                break;
              case ADDRPINCADD:   // (An)+
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = GET8_PINC();
                    break;
                  case WORD_SIZE:
                    res2.w = GET16_PINC();
                    break;
                  case DWORD_SIZE:
                    res2.d = GET32_PINC();
                    break;
                  }
                break;
              case ADDRPDECADD:   // -(An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = GET8_PDEC();
                    break;
                  case WORD_SIZE:
                    res2.w = GET16_PDEC();
                    break;
                  case DWORD_SIZE:
                    res2.d = GET32_PDEC();
                    break;
                  }
                break;
              case ADDRDISPADD:   // (D16,An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = GET8_DISP16();
                    break;
                  case WORD_SIZE:
                    res2.w = GET16_DISP16();
                    break;
                  case DWORD_SIZE:
                    res2.d = GET32_DISP16();
                    break;
                  }
                _pc+=2;
                break;
              case ADDRIDXADD:   // (D8,An,Xn)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res2.b.l = GET8_DISP8S();
                    else
                      res2.b.l = GET8_DISP8L();
                    break;
                  case WORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res2.w = GET16_DISP8S();
                    else
                      res2.w = GET16_DISP8L();
                    break;
                  case DWORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res2.d = GET32_DISP8S();
                    else
                      res2.d = GET32_DISP8L();
                    break;
                  }
                _pc+=2;
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
              res3.d = res1.d | res2.d;
              break;
            }

          if(DIRECTION) { // direction
            switch(ADDRESSING) {
              case ABSOLUTEADD:    // #imm
                switch(ABSOLUTEADD_MODE) {
                  case ABSSSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        PutValue((int16_t)Pipe2.w, res3.b.l);
                        break;
                      case WORD_SIZE:
                        PutShortValue((int16_t)Pipe2.w, res3.w);
                        break;
                      case DWORD_SIZE:
                        PutIntValue((int16_t)Pipe2.w, res3.d);
                        break;
                      }
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
                        PutIntValue(Pipe2.d, res3.d);
                        break;
                      }
                    break;
                  case PCIDXSUBADD:   // (D16,PC) NON dovrebbero esserci..
                  case PCDISPSUBADD:   // (D8,Xn,PC) NON dovrebbero esserci..
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
                    DEST_REG_D.d = res3.d;
                    break;
                  }
                break;
              case ADDRREGADD:   // An qua NO!
                break;
              case ADDRADD:   // (An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    PutValue(WORKING_REG_A.d,res3.b.l);
                    break;
                  case WORD_SIZE:
                    PutShortValue(WORKING_REG_A.d,res3.w);
                    break;
                  case DWORD_SIZE:
                    PutIntValue(WORKING_REG_A.d,res3.d);
                    break;
                  }
                break;
              case ADDRPINCADD:   // (An)+
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    PUT8_PINC();
                    break;
                  case WORD_SIZE:
                    PUT16_PINC();
                    break;
                  case DWORD_SIZE:
                    PUT32_PINC();
                    break;
                  }
                break;
              case ADDRPDECADD:   // -(An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    PutValue(WORKING_REG_A.d,res3.b.l);
                    break;
                  case WORD_SIZE:
                    PutShortValue(WORKING_REG_A.d,res3.w);
                    break;
                  case DWORD_SIZE:
                    PutIntValue(WORKING_REG_A.d,res3.d);
                    break;
                  }
                break;
              case ADDRDISPADD:   // (D16,An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    PUT8_DISP16();
                    break;
                  case WORD_SIZE:
                    PUT16_DISP16();
                    break;
                  case DWORD_SIZE:
                    PUT32_DISP16();
                    break;
                  }
                break;
              case ADDRIDXADD:   // (D8,An,Xn)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      PUT8_DISP8S();
                    else
                      PUT8_DISP8L();
                    break;
                  case WORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      PUT16_DISP8S();
                    else
                      PUT16_DISP8L();
                    break;
                  case DWORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      PUT32_DISP8S();
                    else
                      PUT32_DISP8L();
                    break;
                  }
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
                DEST_REG_D.d = res3.d;
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
              switch(ABSOLUTEADD_MODE) {
                case ABSSSUBADD:
                  if(!OP_A_SIZE) { // size
                    res1.w = GetShortValue((int16_t)Pipe2.w);
                    }
                  else {
                    res1.d = GetIntValue((int16_t)Pipe2.w);
                    }
                  _pc+=2;
                  break;
                case ABSLSUBADD:
                  if(!OP_A_SIZE) { // 
                    res1.w = GetShortValue(Pipe2.d);
                    }
                  else {
                    res1.d = GetIntValue(Pipe2.d);
                    }
                  _pc+=4;
                  break;
                case PCIDXSUBADD:   // (D16,PC)
                  if(!OP_A_SIZE) { // 
                    res1.w = GET16_DISPPC16();
                    }
                  else {
                    res1.d = GET32_DISPPC16();
                    }
                  _pc+=2;
                  break;
                case PCDISPSUBADD:   // (D8,Xn,PC)
                  if(!OP_A_SIZE) {
                    if(!DISPLACEMENT_SIZE)
                      res1.w = GET16_DISPPC8S();
                    else
                      res1.w = GET16_DISPPC8L();
                    }
                  else {
                    if(!DISPLACEMENT_SIZE)
                      res1.d = GET32_DISPPC8S();
                    else
                      res1.d = GET32_DISPPC8L();
                    }
                  _pc+=2;
                  break;
                case IMMSUBADD:
                  if(!OP_A_SIZE) {
                    res1.w = Pipe2.w;
                    _pc+=2;
                    }
                  else {
                    res1.d = Pipe2.d;
                    _pc+=4;
                    }
                  break;
                }
              break;
            case DATAREGADD:   // Dn
              if(!OP_A_SIZE)  // size
                res1.w = WORKING_REG_D.w.l;
              else
                res1.d = WORKING_REG_D.d;
              break;
            case ADDRREGADD:   // An
              if(!OP_A_SIZE)  // size
                res1.w = WORKING_REG_A.w.l;
              else
                res1.d = WORKING_REG_A.d;
              break;
            case ADDRADD:   // (An)
              if(!OP_A_SIZE)  // 
                res1.w = GetShortValue(WORKING_REG_A.d);
              else
                res1.d = GetIntValue(WORKING_REG_A.d);
              break;
            case ADDRPINCADD:   // (An)+
              if(!OP_A_SIZE) { // size
                res1.w = GET16_PINC();
                }
              else {
                res1.d = GET32_PINC();
                }
              break;
            case ADDRPDECADD:   // -(An)
              if(!OP_A_SIZE) { // size
                res1.w = GET16_PDEC();
                }
              else {
                res1.d = GET32_PDEC();
                }
              break;
            case ADDRDISPADD:   // (D16,An)
              if(!OP_A_SIZE) { // size
                res1.w = GET16_DISP16();
                }
              else {
                res1.d = GET32_DISP16();
                }
              _pc+=2;
              break;
            case ADDRIDXADD:   // (D8,An,Xn)
              if(!OP_A_SIZE) { // size
                if(!DISPLACEMENT_SIZE)
                  res1.w = GET16_DISP8S();
                else
                  res1.w = GET16_DISP8L();
                }
              else {
                if(!DISPLACEMENT_SIZE)
                  res1.d = GET32_DISP8S();
                else
                  res1.d = GET32_DISP8L();
                }
              _pc+=2;
              break;
            }
          if(!OP_A_SIZE)  // size
            DEST_REG_A.d -= (int16_t)res1.w;
          else
            DEST_REG_A.d -= res1.d;
          // no flag qua
          }
        else {    // SUB SUBX
          if(DIRECTION) {		// <ea> - Dn -> <ea>
            switch(ADDRESSING) {
              case ABSOLUTEADD:    // 
                switch(ABSOLUTEADD_MODE) {
                  case ABSSSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res1.b.l = GetValue((int16_t)Pipe2.w);
                        break;
                      case WORD_SIZE:
                        res1.w = GetShortValue((int16_t)Pipe2.w);
                        break;
                      case DWORD_SIZE:
                        res1.d = GetIntValue((int16_t)Pipe2.w);
                        break;
                      }
                    _pc+=2;
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
                        res1.d = GetIntValue(Pipe2.d);
                        break;
                      }
                    _pc+=4;
                    break;
                  case PCIDXSUBADD:   // (D16,PC)
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res1.b.l = GET8_DISPPC16();
                        break;
                      case WORD_SIZE:
                        res1.w = GET16_DISPPC16();
                        break;
                      case DWORD_SIZE:
                        res1.d = GET32_DISPPC16();
                        break;
                      }
                    _pc+=2;
                    break;
                  case PCDISPSUBADD:   // (D8,Xn,PC)
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        if(!DISPLACEMENT_SIZE)
                          res1.b.l = GET8_DISPPC8S();
                        else
                          res1.b.l = GET8_DISPPC8L();
                        break;
                      case WORD_SIZE:
                        if(!DISPLACEMENT_SIZE)
                          res1.w = GET16_DISPPC8S();
                        else
                          res1.w = GET16_DISPPC8L();
                        break;
                      case DWORD_SIZE:
                        if(!DISPLACEMENT_SIZE)
                          res1.d = GET32_DISPPC8S();
                        else
                          res1.d = GET32_DISPPC8L();
                        break;
                      }
                    _pc+=2;
                    break;
                  case IMMSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
		                    res1.b.l = Pipe2.b.l;
		                    _pc+=2;
                        break;
                      case WORD_SIZE:
		                    res1.w = Pipe2.w;
		                    _pc+=2;
                        break;
                      case DWORD_SIZE:
		                    res1.d = Pipe2.d;
		                    _pc+=4;
                        break;
                      }
                    break;
                  }
                break;
              case DATAREGADD:   // Dn qua SUBX Dn
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = WORKING_REG_D.b.b0 + _f.CCR.Ext;
                    res1.b.l = DEST_REG_D.b.b0;
                    res3.b.l = DEST_REG_D.b.b0 = res1.b.l - res2.b.l;
                    break;
                  case WORD_SIZE:
                    res2.w = WORKING_REG_D.w.l + _f.CCR.Ext;
                    res1.w = DEST_REG_D.w.l;
                    res3.w = DEST_REG_D.w.l = res1.w - res2.w;
                    break;
                  case DWORD_SIZE:
                    res2.d = WORKING_REG_D.d + _f.CCR.Ext;
                    res1.d = DEST_REG_D.d; 
                    res3.d = DEST_REG_D.d = res1.d - res2.d;
                    break;
                  }
                goto aggFlagSX;
                break;
              case ADDRREGADD:   // An qua SUBX -(An) 
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    --WORKING_REG_A.d; res2.b.l = GetValue(WORKING_REG_A.d) + _f.CCR.Ext;
                    // secondo il simulatore non raddoppia A7 qua..
                    --DEST_REG_A.d; res1.b.l = GetValue(DEST_REG_A.d);
                    res3.b.l = res1.b.l - res2.b.l;
                    PutValue(DEST_REG_A.d, res3.b.l);
                    break;
                  case WORD_SIZE:
                    WORKING_REG_A.d-=2; res2.w = GetShortValue(WORKING_REG_A.d) + _f.CCR.Ext;
                    DEST_REG_A.d-=2; res1.w = GetShortValue(DEST_REG_A.d);
                    res3.w = res1.w - res2.w;
                    PutShortValue(DEST_REG_A.d, res3.w);
                    break;
                  case DWORD_SIZE:
                    WORKING_REG_A.d-=4; res2.d = GetIntValue(WORKING_REG_A.d) + _f.CCR.Ext;
                    DEST_REG_A.d-=4; res1.d = GetIntValue(DEST_REG_A.d);
                    res3.d = res1.d - res2.d;
                    PutIntValue(DEST_REG_A.d, res3.d);
                    break;
                  }
aggFlagSX:
#warning verificare cmq se le varie opzioni qua vanno a posto con i flag e il comportamento speciale di addx
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    if(res3.b.l) _f.CCR.Zero=0;  // cleared or unchanged
                    _f.CCR.Sign=!!(res3.b.l & 0x80);
                    _f.CCR.Carry= !!(((res2.b.l & 0x80) & (~res1.b.l & 0x80)) | ((res3.b.l & 0x80) &
                      (~res1.b.l & 0x80)) | ((res2.b.l & 0x80) & (res3.b.l & 0x80)));
                    _f.CCR.Ovf= !!(((~res2.b.l & 0x80) & (res1.b.l & 0x80) & (~res3.b.l & 0x80)) |
                      ((res2.b.l & 0x80) & (~res1.b.l & 0x80) & (res3.b.l & 0x80)));
                    break;
                  case WORD_SIZE:
                    if(res3.w) _f.CCR.Zero=0;
                    _f.CCR.Sign=!!(res3.w & 0x8000);
                    _f.CCR.Carry= !!(((res2.w & 0x8000) & (~res1.w & 0x8000)) | ((res3.w & 0x8000) &
                      (~res1.w & 0x8000)) | ((res2.w & 0x8000) & (res3.w & 0x8000)));
                    _f.CCR.Ovf= !!(((~res2.w & 0x8000) & (res1.w & 0x8000) & (~res3.w & 0x8000)) |
                      ((res2.w & 0x8000) & (~res1.w & 0x8000) & (res3.w & 0x8000)));
                    break;
                  case DWORD_SIZE:
                    if(res3.d) _f.CCR.Zero=0;
                    _f.CCR.Sign=!!(res3.d & 0x80000000);
                    _f.CCR.Carry= !!(((res2.d & 0x80000000) & (~res1.d & 0x80000000)) | ((res3.d & 0x80000000) &
                      (~res1.d & 0x80000000)) | ((res2.d & 0x80000000) & (res3.d & 0x80000000)));
                    _f.CCR.Ovf= !!(((~res2.d & 0x80000000) & (res1.d & 0x80000000) & (~res3.d & 0x80000000)) |
                      ((res2.d & 0x80000000) & (~res1.d & 0x80000000) & (res3.d & 0x80000000)));
                    break;
                  }
                _f.CCR.Ext=_f.CCR.Carry;
                goto noAggFlag;
                break;
              case ADDRADD:   // (An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GetValue(WORKING_REG_A.d);
                    break;
                  case WORD_SIZE:
                    res1.w = GetShortValue(WORKING_REG_A.d);
                    break;
                  case DWORD_SIZE:
                    res1.d = GetIntValue(WORKING_REG_A.d);
                    break;
                  }
                break;
              case ADDRPINCADD:   // (An)+
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GetValue(WORKING_REG_A.d);
                    // postinc DOPO!
                    break;
                  case WORD_SIZE:
                    res1.w = GetShortValue(WORKING_REG_A.d);
                    break;
                  case DWORD_SIZE:
                    res1.d = GetIntValue(WORKING_REG_A.d);
                    break;
                  }
                break;
              case ADDRPDECADD:   // -(An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GET8_PDEC();
                    break;
                  case WORD_SIZE:
                    res1.w = GET16_PDEC();
                    break;
                  case DWORD_SIZE:
                    res1.d = GET32_PDEC();
                    break;
                  }
                break;
              case ADDRDISPADD:   // (D16,An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GET8_DISP16();
                    break;
                  case WORD_SIZE:
                    res1.w = GET16_DISP16();
                    break;
                  case DWORD_SIZE:
                    res1.d = GET32_DISP16();
                    break;
                  }
                _pc+=2;
                break;
              case ADDRIDXADD:   // (D8,An,Xn)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res1.b.l = GET8_DISP8S();
                    else
                      res1.b.l = GET8_DISP8L();
                    break;
                  case WORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res1.w = GET16_DISP8S();
                    else
                      res1.w = GET16_DISP8L();
                    break;
                  case DWORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res1.d = GET32_DISP8S();
                    else
                      res1.d = GET32_DISP8L();
                    break;
                  }
                _pc+=2;
                break;
              }
            }
          else {			// Dn - <ea> -> Dn
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res1.b.l = DEST_REG_D.b.b0;
                break;
              case WORD_SIZE:
                res1.w = DEST_REG_D.w.l;
                break;
              case DWORD_SIZE:
                res1.d = DEST_REG_D.d;
                break;
              }
            }

          if(DIRECTION) { // direction
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res2.b.l = DEST_REG_D.b.b0;
                break;
              case WORD_SIZE:
                res2.w = DEST_REG_D.w.l;
                break;
              case DWORD_SIZE:
                res2.d = DEST_REG_D.d;
                break;
              }
            }
          else {
            switch(ADDRESSING) {
              case ABSOLUTEADD:    // #imm
                switch(ABSOLUTEADD_MODE) {
                  case ABSSSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res2.b.l = GetValue((int16_t)Pipe2.w);
                        break;
                      case WORD_SIZE:
                        res2.w = GetShortValue((int16_t)Pipe2.w);
                        break;
                      case DWORD_SIZE:
                        res2.d = GetIntValue((int16_t)Pipe2.w);
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
                        res2.d = GetIntValue(Pipe2.d);
                        break;
                      }
                    _pc+=4;
                    break;
                  case PCIDXSUBADD:   // (D16,PC)
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res2.b.l = GET8_DISPPC16();
                        break;
                      case WORD_SIZE:
                        res2.w = GET16_DISPPC16();
                        break;
                      case DWORD_SIZE:
                        res2.d = GET32_DISPPC16();
                        break;
                      }
                    _pc+=2;
                    break;
                  case PCDISPSUBADD:   // (D8,Xn,PC)
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        if(!DISPLACEMENT_SIZE)
                          res2.b.l = GET8_DISPPC8S();
                        else
                          res2.b.l = GET8_DISPPC8L();
                        break;
                      case WORD_SIZE:
                        if(!DISPLACEMENT_SIZE)
                          res2.w = GET16_DISPPC8S();
                        else
                          res2.w = GET16_DISPPC8L();
                        break;
                      case DWORD_SIZE:
                        if(!DISPLACEMENT_SIZE)
                          res2.d = GET32_DISPPC8S();
                        else
                          res2.d = GET32_DISPPC8L();
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
		                    res2.d = Pipe2.d;
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
                    res2.d = WORKING_REG_D.d;
                    break;
                  }
                break;
              case ADDRREGADD:   // An
                switch(OPERAND_SIZE) {
                  case WORD_SIZE:
                    res2.w = WORKING_REG_A.w.l;
                    break;
                  case DWORD_SIZE:
                    res2.d = WORKING_REG_A.d;
                    break;
                  }
                break;
              case ADDRADD:   // (An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = GetValue(WORKING_REG_A.d);
                    break;
                  case WORD_SIZE:
                    res2.w = GetShortValue(WORKING_REG_A.d);
                    break;
                  case DWORD_SIZE:
                    res2.d = GetIntValue(WORKING_REG_A.d);
                    break;
                  }
                break;
              case ADDRPINCADD:   // (An)+
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = GET8_PINC();
                    break;
                  case WORD_SIZE:
                    res2.w = GET16_PINC();
                    break;
                  case DWORD_SIZE:
                    res2.d = GET32_PINC();
                    break;
                  }
                break;
              case ADDRPDECADD:   // -(An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = GET8_PDEC();
                    break;
                  case WORD_SIZE:
                    res2.w = GET16_PDEC();
                    break;
                  case DWORD_SIZE:
                    res2.d = GET32_PDEC();
                    break;
                  }
                break;
              case ADDRDISPADD:   // (D16,An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = GET8_DISP16();
                    break;
                  case WORD_SIZE:
                    res2.w = GET16_DISP16();
                    break;
                  case DWORD_SIZE:
                    res2.d = GET32_DISP16();
                    break;
                  }
                _pc+=2;
                break;
              case ADDRIDXADD:   // (D8,An,Xn)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res2.b.l = GET8_DISP8S();
                    else
                      res2.b.l = GET8_DISP8L();
                    break;
                  case WORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res2.w = GET16_DISP8S();
                    else
                      res2.w = GET16_DISP8L();
                    break;
                  case DWORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res2.d = GET32_DISP8S();
                    else
                      res2.d = GET32_DISP8L();
                    break;
                  }
                _pc+=2;
                break;
              }
            }

          switch(OPERAND_SIZE) {
            case BYTE_SIZE:
              res3.b.l = res1.b.l - res2.b.l;
              break;
            case WORD_SIZE:
              res3.w = res1.w - res2.w;
              break;
            case DWORD_SIZE:
              res3.d = res1.d - res2.d;
              break;
            }

          if(DIRECTION) { // direction
            switch(ADDRESSING) {
              case ABSOLUTEADD:    // #imm
                switch(ABSOLUTEADD_MODE) {
                  case ABSSSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        PutValue((int16_t)Pipe2.w, res3.b.l);
                        break;
                      case WORD_SIZE:
                        PutShortValue((int16_t)Pipe2.w, res3.w);
                        break;
                      case DWORD_SIZE:
                        PutIntValue((int16_t)Pipe2.w, res3.d);
                        break;
                      }
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
                        PutIntValue(Pipe2.d, res3.d);
                        break;
                      }
                    break;
                  case PCIDXSUBADD:   // (D16,PC) NON dovrebbero esserci..
                  case PCDISPSUBADD:   // (D8,Xn,PC) NON dovrebbero esserci..
                    break;
                  case IMMSUBADD:		// boh
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
                    WORKING_REG_A.d = res3.d;
                    break;
                  }
                break;
              case ADDRADD:   // (An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    PutValue(WORKING_REG_A.d,res3.b.l);
                    break;
                  case WORD_SIZE:
                    PutShortValue(WORKING_REG_A.d,res3.w);
                    break;
                  case DWORD_SIZE:
                    PutIntValue(WORKING_REG_A.d,res3.d);
                    break;
                  }
                break;
              case ADDRPINCADD:   // (An)+
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    PUT8_PINC();
                    break;
                  case WORD_SIZE:
                    PUT16_PINC();
                    break;
                  case DWORD_SIZE:
                    PUT32_PINC();
                    break;
                  }
                break;
              case ADDRPDECADD:   // -(An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    PutValue(WORKING_REG_A.d,res3.b.l);
                    break;
                  case WORD_SIZE:
                    PutShortValue(WORKING_REG_A.d,res3.w);
                    break;
                  case DWORD_SIZE:
                    PutIntValue(WORKING_REG_A.d,res3.d);
                    break;
                  }
                break;
              case ADDRDISPADD:   // (D16,An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    PUT8_DISP16();
                    break;
                  case WORD_SIZE:
                    PUT16_DISP16();
                    break;
                  case DWORD_SIZE:
                    PUT32_DISP16();
                    break;
                  }
                break;
              case ADDRIDXADD:   // (D8,An,Xn)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      PUT8_DISP8S();
                    else
                      PUT8_DISP8L();
                    break;
                  case WORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      PUT16_DISP8S();
                    else
                      PUT16_DISP8L();
                    break;
                  case DWORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      PUT32_DISP8S();
                    else
                      PUT32_DISP8L();
                    break;
                  }
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
                DEST_REG_D.d = res3.d;
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
          if(OP_A_SIZE) { // size
            switch(ADDRESSING) {
              case ABSOLUTEADD:
                switch(ABSOLUTEADD_MODE) {
                  case ABSSSUBADD:
                    res2.d = GetIntValue((int16_t)Pipe2.w);
                    _pc+=2;
                    break;
                  case ABSLSUBADD:
                    res2.d = GetIntValue(Pipe2.d); 
                    _pc+=4;
                    break;
                  case PCIDXSUBADD:   // (D16,PC)
                    res2.d = GET32_DISPPC16();
                    _pc+=2;
                    break;
                  case PCDISPSUBADD:   // (D8,Xn,PC)
                    if(!DISPLACEMENT_SIZE)
                      res2.d = GET32_DISPPC8S();
                    else
                      res2.d = GET32_DISPPC8L();
                    _pc+=2;
                    break;
                  case IMMSUBADD:
                    res2.d = Pipe2.d; 
                    _pc+=4;
                    break;
                  }
                break;
              case DATAREGADD:   // Dn
                res2.d = WORKING_REG_D.d;
                break;
              case ADDRREGADD:   // An
                res2.d = WORKING_REG_A.d;
                break;
              case ADDRADD:   // (An)
                res2.d = GetIntValue(WORKING_REG_A.d);
                break;
              case ADDRPINCADD:   // (An)+
                res2.d = GET32_PINC();
                break;
              case ADDRPDECADD:   // -(An)
                res2.d = GET32_PDEC();
                break;
              case ADDRDISPADD:   // (D16,An)
                res2.d = GET32_DISP16();
                _pc+=2;
                break;
              case ADDRIDXADD:   // (D8,An,Xn)
                if(!DISPLACEMENT_SIZE)
                  res2.d = GET32_DISP8S();
                else
                  res2.d = GET32_DISP8L();
                _pc+=2;
                break;
              }
            res1.d=DEST_REG_A.d;
            res3.d = res1.d - res2.d;
            }
          else {
            switch(ADDRESSING) {
              case ABSOLUTEADD:
                switch(ABSOLUTEADD_MODE) {
                  case ABSSSUBADD:
                    res2.w = GetShortValue((int16_t)Pipe2.w);
                    _pc+=2;
                    break;
                  case ABSLSUBADD:
                    res2.w = GetShortValue(Pipe2.d); 
                    _pc+=4;
                    break;
                  case PCIDXSUBADD:   // (D16,PC)
                    res2.w = GET16_DISPPC16();
                    _pc+=2;
                    break;
                  case PCDISPSUBADD:   // (D8,Xn,PC)
                    if(!DISPLACEMENT_SIZE)
                      res2.w = GET16_DISPPC8S();
                    else
                      res2.w = GET16_DISPPC8L();
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
                res2.w = GET16_PINC();
                break;
              case ADDRPDECADD:   // -(An)
                res2.w = GET16_PDEC();
                break;
              case ADDRDISPADD:   // (D16,An)
                res2.w = GET16_DISP16();
                _pc+=2;
                break;
              case ADDRIDXADD:   // (D8,An,Xn)
                if(!DISPLACEMENT_SIZE)
                  res2.w = GET16_DISP8S();
                else
                  res2.w = GET16_DISP8L();
                _pc+=2;
                break;
              }
            res1.d=DEST_REG_A.d;
            res3.d = res1.d - (int16_t)res2.w;
            }
          goto aggFlagC4;
          }
        else {    // EOR CMPM
          switch(ADDRESSING) {
            case ABSOLUTEADD:
              switch(ABSOLUTEADD_MODE) {
                case ABSSSUBADD:
                  switch(OPERAND_SIZE) {
                    case BYTE_SIZE:
                      res3.b.l = DEST_REG_D.b.b0 ^ GetValue((int16_t)Pipe2.w);
                      PutValue((int16_t)Pipe2.w,res3.b.l);
                      break;
                    case WORD_SIZE:
                      res3.w = DEST_REG_D.w.l ^ GetShortValue((int16_t)Pipe2.w);
                      PutShortValue((int16_t)Pipe2.w,res3.w);
                      break;
                    case DWORD_SIZE:
                      res3.d = DEST_REG_D.d ^ GetIntValue((int16_t)Pipe2.w);
                      PutIntValue((int16_t)Pipe2.w,res3.d);
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
                      res3.d = DEST_REG_D.d ^ GetIntValue(Pipe2.d);
                      PutIntValue(Pipe2.d,res3.d);
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
                  res3.b.l = WORKING_REG_D.b.b0 ^= DEST_REG_D.b.b0;
                  break;
                case WORD_SIZE:
                  res3.w = WORKING_REG_D.w.l ^= DEST_REG_D.w.l;
                  break;
                case DWORD_SIZE:
                  res3.d = WORKING_REG_D.d ^= DEST_REG_D.d;
                  break;
                }
              break;
            case ADDRREGADD:   // An; CMPM qua
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  res2.b.l = GetValue(WORKING_REG_A.d);
                  res1.b.l = GetValue(DEST_REG_A.d);
                  res3.w = res1.b.l - res2.b.l;
                  DEST_REG_A.d++; WORKING_REG_A.d++;			// qua pare NON c'è il caso particolare A7!
                  break;
                case WORD_SIZE:
                  res2.w = GetShortValue(WORKING_REG_A.d);
                  res1.w = GetShortValue(DEST_REG_A.d);
                  res3.w = res1.w - res2.w;
                  DEST_REG_A.d+=2; WORKING_REG_A.d+=2;
                  break;
                case DWORD_SIZE:
                  res2.d = GetIntValue(WORKING_REG_A.d);
                  res1.d = GetIntValue(DEST_REG_A.d);
                  res3.d = res1.d - res2.d;
                  DEST_REG_A.d+=4; WORKING_REG_A.d+=4;
                  break;
                }
              goto aggFlagC;
              break;
            case ADDRADD:   // (An)
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  res3.b.l = GetValue(WORKING_REG_A.d) ^ DEST_REG_D.b.b0;
                  PutValue(WORKING_REG_A.d,res3.b.l);
                  break;
                case WORD_SIZE:
                  res3.w = GetShortValue(WORKING_REG_A.d) ^ DEST_REG_D.w.l;
                  PutShortValue(WORKING_REG_A.d,res3.w);
                  break;
                case DWORD_SIZE:
                  res3.d = GetIntValue(WORKING_REG_A.d) ^ DEST_REG_D.d;
                  PutIntValue(WORKING_REG_A.d,res3.d);
                  break;
                }
              break;
            case ADDRPINCADD:   // (An)+
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  res3.b.l = GetValue(WORKING_REG_A.d) ^ DEST_REG_D.b.b0;
                  PUT8_PINC();
                  break;
                case WORD_SIZE:
                  res3.w = GetShortValue(WORKING_REG_A.d) ^ DEST_REG_D.w.l;
                  PUT16_PINC();
                  break;
                case DWORD_SIZE:
                  res3.d = GetIntValue(WORKING_REG_A.d) ^ DEST_REG_D.d;
                  PUT32_PINC();
                  break;
                }
              break;
            case ADDRPDECADD:   // -(An)
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  res3.b.l = GET8_PDEC() ^ DEST_REG_D.b.b0;
                  PutValue(WORKING_REG_A.d,res3.b.l);
                  break;
                case WORD_SIZE:
                  res3.w = GET16_PDEC() ^ DEST_REG_D.w.l;
                  PutShortValue(WORKING_REG_A.d,res3.w);
                  break;
                case DWORD_SIZE:
                  res3.d = GET32_PDEC() ^ DEST_REG_D.d;
                  PutIntValue(WORKING_REG_A.d,res3.d);
                  break;
                }
              break;
            case ADDRDISPADD:   // (D16,An)
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  res3.b.l = GET8_DISP16() ^ DEST_REG_D.b.b0;
                  PUT8_DISP16();
                  break;
                case WORD_SIZE:
                  res3.w = GET16_DISP16() ^ DEST_REG_D.w.l;
                  PUT16_DISP16();
                  break;
                case DWORD_SIZE:
                  res3.d = GET32_DISP16() ^ DEST_REG_D.d;
                  PUT32_DISP16();
                  break;
                }
              _pc+=2;
              break;
            case ADDRIDXADD:   // (D8,An,Xn)
              switch(OPERAND_SIZE) {
                case BYTE_SIZE:
                  if(!DISPLACEMENT_SIZE) {
                    res3.b.l = GET8_DISP8S() ^ DEST_REG_D.b.b0;
                    PUT8_DISP8S();
                    }
                  else {
                    res3.b.l = GET8_DISP8L() ^ DEST_REG_D.b.b0;
                    PUT8_DISP8L();
                    }
                  break;
                case WORD_SIZE:
                  if(!DISPLACEMENT_SIZE) {
                    res3.w = GET16_DISP8S() ^ DEST_REG_D.w.l;
                    PUT16_DISP8S();
                    }
                  else {
                    res3.w = GET16_DISP8L() ^ DEST_REG_D.w.l;
                    PUT16_DISP8L();
                    }
                  break;
                case DWORD_SIZE:
                  if(!DISPLACEMENT_SIZE) {
                    res3.d = GET32_DISP8S() ^ DEST_REG_D.d;
                    PUT32_DISP8S();
                    }
                  else {
                    res3.d = GET32_DISP8L() ^ DEST_REG_D.d;
                    PUT32_DISP8L();
                    }
                  break;
                }
              _pc+=2;
              break;
            }
          goto aggFlag;
          }
				break;
        
 			case 0xa0:   // exception 1010
 			case 0xa1:
 			case 0xa2:
 			case 0xa3:
 			case 0xa4:
 			case 0xa5:
 			case 0xa6:
 			case 0xa7:
 			case 0xa8:
 			case 0xa9:
 			case 0xaa:
 			case 0xab:
 			case 0xac:
 			case 0xad:
 			case 0xae:
 			case 0xaf:
		    res3.b.l=10;
		    goto doTrap;
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
            switch(ABSOLUTEADD_MODE) {
              case ABSSSUBADD:
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = GetValue((int16_t)Pipe2.w);
                    break;
                  case WORD_SIZE:
                    res2.w = GetShortValue((int16_t)Pipe2.w);
                    break;
                  case DWORD_SIZE:
                    res2.d = GetIntValue((int16_t)Pipe2.w);
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
                    res2.d = GetIntValue(Pipe2.d);
                    break;
                  }
                _pc+=4;
                break;
              case PCIDXSUBADD:   // (D16,PC)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = GET8_DISPPC16();
                    break;
                  case WORD_SIZE:
                    res2.w = GET16_DISPPC16();
                    break;
                  case DWORD_SIZE:
                    res2.d = GET32_DISPPC16();
                    break;
                  }
                _pc+=2;
                break;
              case PCDISPSUBADD:   // (D8,Xn,PC)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res2.b.l = GET8_DISPPC8S();
                    else
                      res2.b.l = GET8_DISPPC8L();
                    break;
                  case WORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res2.w = GET16_DISPPC8S();
                    else
                      res2.w = GET16_DISPPC8L();
                    break;
                  case DWORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res2.d = GET32_DISPPC8S();
                    else
                      res2.d = GET32_DISPPC8L();
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
                    res2.d = Pipe2.d; 
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
                res2.d = WORKING_REG_D.d;
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
                res2.d = WORKING_REG_A.d;
                break;
              }
            break;
          case ADDRADD:   // (An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res2.b.l = GetValue(WORKING_REG_A.d);
                break;
              case WORD_SIZE:
                res2.w = GetShortValue(WORKING_REG_A.d);
                break;
              case DWORD_SIZE:
                res2.d = GetIntValue(WORKING_REG_A.d);
                break;
              }
            break;
          case ADDRPINCADD:   // (An)+
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res2.b.l = GET8_PINC();
                break;
              case WORD_SIZE:
                res2.w = GET16_PINC();
                break;
              case DWORD_SIZE:
                res2.d = GET32_PINC();
                break;
              }
            break;
          case ADDRPDECADD:   // -(An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res2.b.l = GET8_PDEC();
                break;
              case WORD_SIZE:
                res2.w = GET16_PDEC();
                break;
              case DWORD_SIZE:
                res2.d = GET32_PDEC();
                break;
              }
            break;
          case ADDRDISPADD:   // (D16,An)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res2.b.l = GET8_DISP16();
                break;
              case WORD_SIZE:
                res2.w = GET16_DISP16();
                break;
              case DWORD_SIZE:
                res2.d = GET32_DISP16();
                break;
              }
            _pc+=2;
            break;
          case ADDRIDXADD:   // (D8,An,Xn)
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                if(!DISPLACEMENT_SIZE)
                  res2.b.l = GET8_DISP8S();
                else
                  res2.b.l = GET8_DISP8L();
                break;
              case WORD_SIZE:
                if(!DISPLACEMENT_SIZE)
                  res2.w = GET16_DISP8S();
                else
                  res2.w = GET16_DISP8L();
                break;
              case DWORD_SIZE:
                if(!DISPLACEMENT_SIZE)
                  res2.d = GET32_DISP8S();
                else
                  res2.d = GET32_DISP8L();
                break;
              }
            _pc+=2;
            break;
          }
        switch(OPERAND_SIZE) {
          case BYTE_SIZE:
            res1.b.l = DEST_REG_D.b.b0;
            res3.b.l = res1.b.l-res2.b.l;
            break;
          case WORD_SIZE:
            res1.w = DEST_REG_D.w.l;
            res3.w = res1.w-res2.w;
            break;
          case DWORD_SIZE:
            res1.d = DEST_REG_D.d;
            res3.d = res1.d-res2.d;
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
#ifdef MC68020
// "forse" in 68020 ecc c'è anche una versione 32x32bit... (per cui qua non c'è OVF ma là forse sì..
#endif
          res1.w=DEST_REG_D.w.l;
          switch(ADDRESSING) {
            case ABSOLUTEADD:
              switch(ABSOLUTEADD_MODE) {
                case ABSSSUBADD:
                  res2.w = GetShortValue((int16_t)Pipe2.w);
                  _pc+=2;
                  break;
                case ABSLSUBADD:
                  res2.w = GetShortValue(Pipe2.d); 
                  _pc+=4;
                  break;
                case PCIDXSUBADD:   // (D16,PC)
                  res2.w = GET16_DISPPC16();
                  _pc+=2;
                  break;
                case PCDISPSUBADD:   // (D8,Xn,PC)
                  if(!DISPLACEMENT_SIZE)
                    res2.w = GET16_DISPPC8S();
                  else
                    res2.w = GET16_DISPPC8L();
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
              res2.w = GetShortValue(WORKING_REG_A.d);
              break;
            case ADDRPINCADD:   // (An)+
              res2.w = GET16_PINC();
              break;
            case ADDRPDECADD:   // -(An)
              res2.w = GET16_PDEC();
              break;
            case ADDRDISPADD:   // (D16,An)
              res2.w = GET16_DISP16();
              _pc+=2;
              break;
            case ADDRIDXADD:   // (D8,An,Xn)
              if(!DISPLACEMENT_SIZE)
                res2.w = GET16_DISP8S();
              else
                res2.w = GET16_DISP8L();
              _pc+=2;
              break;
            }
          if(Pipe1.b[1] & 0b00000001)      // MULS
            res3.d = (int32_t)((int16_t)res1.w) * (int32_t)((int16_t)res2.w);
          else      // MULU
            res3.d = (uint32_t)res1.w * (uint32_t)res2.w;
          DEST_REG_D.d = res3.d;
          _f.CCR.Zero=!res3.d;
          _f.CCR.Sign=!!(res3.d & 0x80000000);
          _f.CCR.Ovf=0;     // ?? appunto, se long*long
          _f.CCR.Carry=0;
          }
        else {  // AND ABCD EXG
          if(DIRECTION) { // 
            switch(ADDRESSING) {
              case ABSOLUTEADD:    // 
                switch(ABSOLUTEADD_MODE) {
                  case ABSSSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res1.b.l = GetValue((int16_t)Pipe2.w);
                        break;
                      case WORD_SIZE:
                        res1.w = GetShortValue((int16_t)Pipe2.w);
                        break;
                      case DWORD_SIZE:
                        res1.d = GetIntValue((int16_t)Pipe2.w);
                        break;
                      }
                    _pc+=2;
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
                        res1.d = GetIntValue(Pipe2.d);
                        break;
                      }
                    _pc+=4;
                    break;
                  case PCIDXSUBADD:   // (D16,PC)
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res1.b.l = GET8_DISPPC16();
                        break;
                      case WORD_SIZE:
                        res1.w = GET16_DISPPC16();
                        break;
                      case DWORD_SIZE:
                        res1.d = GET32_DISPPC16();
                        break;
                      }
                    _pc+=2;
                    break;
                  case PCDISPSUBADD:   // (D8,Xn,PC)
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        if(!DISPLACEMENT_SIZE)
                          res1.b.l = GET8_DISPPC8S();
                        else
                          res1.b.l = GET8_DISPPC8L();
                        break;
                      case WORD_SIZE:
                        if(!DISPLACEMENT_SIZE)
                          res1.w = GET16_DISPPC8S();
                        else
                          res1.w = GET16_DISPPC8L();
                        break;
                      case DWORD_SIZE:
                        if(!DISPLACEMENT_SIZE)
                          res1.d = GET32_DISPPC8S();
                        else
                          res1.d = GET32_DISPPC8L();
                        break;
                      }
                    _pc+=2;
                    break;
                  case IMMSUBADD:
										switch(OPERAND_SIZE) {
											case BYTE_SIZE:
												res1.b.l = Pipe2.b.l;
												_pc+=2;
												break;
											case WORD_SIZE:
												res1.w = Pipe2.w;
												_pc+=2;
												break;
											case DWORD_SIZE:
												res1.d = Pipe2.d; 
												_pc+=4;
												break;
											}
                    break;
                  }
                break;
              case DATAREGADD:   // Dn  QUA EXG Dx Dy...
								res3.d=WORKING_REG_D.d;
                WORKING_REG_D.d=DEST_REG_D.d;
                DEST_REG_D.d=res3.d;
                goto noAggFlag;
/*                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = WORKING_REG_D.b.b0;
                    break;
                  case WORD_SIZE:
                    res1.w = WORKING_REG_D.w.l;
                    break;
                  case DWORD_SIZE:
                    res1.w = WORKING_REG_D.d;
                    break;
                  }*/
                break;
              case ADDRREGADD:   // An qua ABCD EXG
                switch(OPERAND_SIZE) {
                  case 0x00:     // ABCD v. anche SBCD DOVREBBE ANDARE ANCHE su DATAREG...
                    --WORKING_REG_A.d; res1.b.l = GetValue(WORKING_REG_A.d);
                    --DEST_REG_A.d; res2.b.l = GetValue(DEST_REG_A.d);
                    res3.w = (WORD)(res1.b.l & 0xf) + (WORD)(res2.b.l & 0xf) + _f.CCR.Ext;
                    res3.w = (((res1.b.l & 0xf0) >> 4) + ((res2.b.l & 0xf0) >> 4) + (res3.b.h ? 1 : 0)) | 
                              res3.b.l;
                    PutValue(DEST_REG_A.d, res3.b.l);
                    if(res3.b.l) _f.CCR.Zero=0;
                    _f.CCR.Ext=_f.CCR.Carry=!!res3.b.h;
                    goto noAggFlag;
                    break;
                  case 0x40:     //  EXG
                    res3.d=WORKING_REG_A.d;
                    WORKING_REG_A.d=DEST_REG_A.d;
                    DEST_REG_A.d=res3.d;
                    goto noAggFlag;
                    break;
                  case 0x80:     //  EXG
                    res3.d=WORKING_REG_A.d;
                    WORKING_REG_A.d=DEST_REG_D.d;
                    DEST_REG_D.d=res3.d;
                    goto noAggFlag;
                    break;
                  }
                break;
              case ADDRADD:   // (An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GetValue(WORKING_REG_A.d);
                    break;
                  case WORD_SIZE:
                    res1.w = GetShortValue(WORKING_REG_A.d);
                    break;
                  case DWORD_SIZE:
                    res1.d = GetIntValue(WORKING_REG_A.d);
                    break;
                  }
                break;
              case ADDRPINCADD:   // (An)+
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GetValue(WORKING_REG_A.d);
                    // postinc DOPO!
                    break;
                  case WORD_SIZE:
                    res1.w = GetShortValue(WORKING_REG_A.d);
                    break;
                  case DWORD_SIZE:
                    res1.d = GetIntValue(WORKING_REG_A.d);
                    break;
                  }
                break;
              case ADDRPDECADD:   // -(An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GET8_PDEC();
                    break;
                  case WORD_SIZE:
                    res1.w = GET16_PDEC();
                    break;
                  case DWORD_SIZE:
                    res1.d = GET32_PDEC();
                    break;
                  }
                break;
              case ADDRDISPADD:   // (D16,An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GET8_DISP16();
                    break;
                  case WORD_SIZE:
                    res1.w = GET16_DISP16();
                    break;
                  case DWORD_SIZE:
                    res1.d = GET32_DISP16();
                    break;
                  }
                _pc+=2;
                break;
              case ADDRIDXADD:   // (D8,An,Xn)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res1.b.l = GET8_DISP8S();
                    else
                      res1.b.l = GET8_DISP8L();
                    break;
                  case WORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res1.w = GET16_DISP8S();
                    else
                      res1.w = GET16_DISP8L();
                    break;
                  case DWORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res1.d = GET32_DISP8S();
                    else
                      res1.d = GET32_DISP8L();
                    break;
                  }
                _pc+=2;
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
                res1.d = DEST_REG_D.d;
                break;
              }
            }

          if(DIRECTION) { // direction
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res2.b.l = DEST_REG_D.b.b0;
                break;
              case WORD_SIZE:
                res2.w = DEST_REG_D.w.l;
                break;
              case DWORD_SIZE:
                res2.d = DEST_REG_D.d;
                break;
              }
            }
          else {
            switch(ADDRESSING) {
              case ABSOLUTEADD:    // #imm
                switch(ABSOLUTEADD_MODE) {
                  case ABSSSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res2.b.l = GetValue((int16_t)Pipe2.w);
                        break;
                      case WORD_SIZE:
                        res2.w = GetShortValue((int16_t)Pipe2.w);
                        break;
                      case DWORD_SIZE:
                        res2.d = GetIntValue((int16_t)Pipe2.w);
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
                        res2.d = GetIntValue(Pipe2.d);
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
              // bah cmq è ok come AND.. 2/1/23  #warning MA VA IN CONFLITTO CON ABCD! v. anche sopra
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = WORKING_REG_D.b.b0;
                    break;
                  case WORD_SIZE:
                    res2.w = WORKING_REG_D.w.l;
                    break;
                  case DWORD_SIZE:
                    res2.d = WORKING_REG_D.d;
                    break;
                  }
                break;
              case ADDRREGADD:   // An
                break;
              case ADDRADD:   // (An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = GetValue(WORKING_REG_A.d);
                    break;
                  case WORD_SIZE:
                    res2.w = GetShortValue(WORKING_REG_A.d);
                    break;
                  case DWORD_SIZE:
                    res2.d = GetIntValue(WORKING_REG_A.d);
                    break;
                  }
                break;
              case ADDRPINCADD:   // (An)+
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = GET8_PINC();
                    break;
                  case WORD_SIZE:
                    res2.w = GET16_PINC();
                    break;
                  case DWORD_SIZE:
                    res2.d = GET32_PINC();
                    break;
                  }
                break;
              case ADDRPDECADD:   // -(An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = GET8_PDEC();
                    break;
                  case WORD_SIZE:
                    res2.w = GET16_PDEC();
                    break;
                  case DWORD_SIZE:
                    res2.d = GET32_PDEC();
                    break;
                  }
                break;
              case ADDRDISPADD:   // (D16,An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = GET8_DISP16();
                    break;
                  case WORD_SIZE:
                    res2.w = GET16_DISP16();
                    break;
                  case DWORD_SIZE:
                    res2.d = GET32_DISP16();
                    break;
                  }
                _pc+=2;
                break;
              case ADDRIDXADD:   // (D8,An,Xn)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res2.b.l = GET8_DISP8S();
                    else
                      res2.b.l = GET8_DISP8L();
                    break;
                  case WORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res2.w = GET16_DISP8S();
                    else
                      res2.w = GET16_DISP8L();
                    break;
                  case DWORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res2.d = GET32_DISP8S();
                    else
                      res2.d = GET32_DISP8L();
                    break;
                  }
                _pc+=2;
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
              res3.d = res1.d & res2.d;
              break;
            }

          if(DIRECTION) { // direction
            switch(ADDRESSING) {
              case ABSOLUTEADD:    // #imm
                switch(ABSOLUTEADD_MODE) {
                  case ABSSSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        PutValue((int16_t)Pipe2.w, res3.b.l);
                        break;
                      case WORD_SIZE:
                        PutShortValue((int16_t)Pipe2.w, res3.w);
                        break;
                      case DWORD_SIZE:
                        PutIntValue((int16_t)Pipe2.w, res3.d);
                        break;
                      }
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
                        PutIntValue(Pipe2.d, res3.d);
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
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    DEST_REG_D.b.b0 = res3.b.l;
                    break;
                  case WORD_SIZE:
                    DEST_REG_D.w.l = res3.w;
                    break;
                  case DWORD_SIZE:
                    DEST_REG_D.d = res3.d;
                    break;
                  }
                break;
              case ADDRREGADD:   // An qua NO!
                break;
              case ADDRADD:   // (An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    PutValue(WORKING_REG_A.d,res3.b.l);
                    break;
                  case WORD_SIZE:
                    PutShortValue(WORKING_REG_A.d,res3.w);
                    break;
                  case DWORD_SIZE:
                    PutIntValue(WORKING_REG_A.d,res3.d);
                    break;
                  }
                break;
              case ADDRPINCADD:   // (An)+
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    PUT8_PINC();
                    break;
                  case WORD_SIZE:
                    PUT16_PINC();
                    break;
                  case DWORD_SIZE:
                    PUT32_PINC();
                    break;
                  }
                break;
              case ADDRPDECADD:   // -(An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    PutValue(WORKING_REG_A.d,res3.b.l);
                    break;
                  case WORD_SIZE:
                    PutShortValue(WORKING_REG_A.d,res3.w);
                    break;
                  case DWORD_SIZE:
                    PutIntValue(WORKING_REG_A.d,res3.d);
                    break;
                  }
                break;
              case ADDRDISPADD:   // (D16,An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    PUT8_DISP16();
                    break;
                  case WORD_SIZE:
                    PUT16_DISP16();
                    break;
                  case DWORD_SIZE:
                    PUT32_DISP16();
                    break;
                  }
                break;
              case ADDRIDXADD:   // (D8,An,Xn)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      PUT8_DISP8S();
                    else
                      PUT8_DISP8L();
                    break;
                  case WORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      PUT16_DISP8S();
                    else
                      PUT16_DISP8L();
                    break;
                  case DWORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      PUT32_DISP8S();
                    else
                      PUT32_DISP8L();
                    break;
                  }
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
                DEST_REG_D.d = res3.d;
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
              switch(ABSOLUTEADD_MODE) {
                case ABSSSUBADD:
                  if(!OP_A_SIZE) { // size
                    res1.w = GetShortValue((int16_t)Pipe2.w);
                    _pc+=2;
                    }
                  else {
                    res1.d = GetIntValue((int16_t)Pipe2.w);
                    _pc+=4;
                    }
                  break;
                case ABSLSUBADD:
                  if(!OP_A_SIZE) { // 
                    res1.w = GetShortValue(Pipe2.d);
                    _pc+=2;
                    }
                  else {
                    res1.d = GetIntValue(Pipe2.d);
                    _pc+=4;
                    }
                  break;
                case PCIDXSUBADD:   // (D16,PC)
                  if(!OP_A_SIZE) { // 
                    res1.w = GET16_DISPPC16();
                    }
                  else {
                    res1.d = GET32_DISPPC16();
                    }
                  _pc+=2;
                  break;
                case PCDISPSUBADD:   // (D8,Xn,PC)
                  if(!OP_A_SIZE) { // 
                    if(!DISPLACEMENT_SIZE)
                      res1.w = GET16_DISPPC8S();
                    else
                      res1.w = GET16_DISPPC8L();
                    }
                  else {
                    if(!DISPLACEMENT_SIZE)
                      res1.d = GET32_DISPPC8S();
                    else
                      res1.d = GET32_DISPPC8L();
                    }
                  _pc+=2;
                  break;
                case IMMSUBADD:
                  if(!OP_A_SIZE) { // 
                    res1.w = Pipe2.w;
                    _pc+=2;
                    }
                  else {
                    res1.d = Pipe2.d;
                    _pc+=4;
                    }
                  break;
                }
              break;
            case DATAREGADD:   // Dn
              if(!OP_A_SIZE)  // size
                res1.w = WORKING_REG_D.w.l;
              else
                res1.d = WORKING_REG_D.d;
              break;
            case ADDRREGADD:   // An
              if(!OP_A_SIZE)  // size
                res1.w = WORKING_REG_A.w.l;
              else
                res1.d = WORKING_REG_A.d;
              break;
            case ADDRADD:   // (An)
              if(!OP_A_SIZE)  // 
                res1.w = GetShortValue(WORKING_REG_A.d);
              else
                res1.d = GetIntValue(WORKING_REG_A.d);
              break;
            case ADDRPINCADD:   // (An)+
              if(!OP_A_SIZE) { // size
                res1.w = GET16_PINC();
                }
              else {
                res1.d = GET32_PINC();
                }
              break;
            case ADDRPDECADD:   // -(An)
              if(!OP_A_SIZE) { // size
                res1.w = GET16_PDEC();
                }
              else {
                res1.d = GET32_PDEC();
                }
              break;
            case ADDRDISPADD:   // (D16,An)
              if(!OP_A_SIZE) { // size
                res1.w = GET16_DISP16();
                }
              else {
                res1.d = GET32_DISP16();
                }
              _pc+=2;
              break;
            case ADDRIDXADD:   // (D8,An,Xn)
              if(!OP_A_SIZE) { // size
                if(!DISPLACEMENT_SIZE)
                  res1.w = GET16_DISP8S();
                else
                  res1.w = GET16_DISP8L();
                }
              else {
                if(!DISPLACEMENT_SIZE)
                  res1.d = GET32_DISP8S();
                else
                  res1.d = GET32_DISP8L();
                }
              _pc+=2;
              break;
            }
          if(!OP_A_SIZE)  // size
            DEST_REG_A.d += (int16_t)res1.w;
          else
            DEST_REG_A.d += res1.d;
          // no flag qua
          }
        else {    // ADD ADDX
          if(DIRECTION) { // 
            switch(ADDRESSING) {
              case ABSOLUTEADD:    // 
                switch(ABSOLUTEADD_MODE) {
                  case ABSSSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res1.b.l = GetValue((int16_t)Pipe2.w);
                        break;
                      case WORD_SIZE:
                        res1.w = GetShortValue((int16_t)Pipe2.w);
                        break;
                      case DWORD_SIZE:
                        res1.d = GetIntValue((int16_t)Pipe2.w);
                        break;
                      }
                    _pc+=2;
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
                        res1.d = GetIntValue(Pipe2.d);
                        break;
                      }
                    _pc+=4;
                    break;
                  case PCIDXSUBADD:   // (D16,PC)
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res1.b.l = GET8_DISPPC16();
                        break;
                      case WORD_SIZE:
                        res1.w = GET16_DISPPC16();
                        break;
                      case DWORD_SIZE:
                        res1.d = GET32_DISPPC16();
                        break;
                      }
                    _pc+=2;
                    break;
                  case PCDISPSUBADD:   // (D8,Xn,PC)
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        if(!DISPLACEMENT_SIZE)
                          res1.b.l = GET8_DISPPC8S();
                        else
                          res1.b.l = GET8_DISPPC8L();
                        break;
                      case WORD_SIZE:
                        if(!DISPLACEMENT_SIZE)
                          res1.w = GET16_DISPPC8S();
                        else
                          res1.w = GET16_DISPPC8L();
                        break;
                      case DWORD_SIZE:
                        if(!DISPLACEMENT_SIZE)
                          res1.d = GET32_DISPPC8S();
                        else
                          res1.d = GET32_DISPPC8L();
                        break;
                      }
                    _pc+=2;
                    break;
                  case IMMSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
		                    res1.b.l = Pipe2.b.l;
		                    _pc+=2;
                        break;
                      case WORD_SIZE:
		                    res1.w = Pipe2.w;
		                    _pc+=2;
                        break;
                      case DWORD_SIZE:
		                    res1.d = Pipe2.d;
		                    _pc+=4;
                        break;
                      }
                    break;
                  }
                break;
              case DATAREGADD:   // qua ADDX Dn
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = WORKING_REG_D.b.b0 + _f.CCR.Ext;
                    res1.b.l = DEST_REG_D.b.b0;
                    res3.b.l = DEST_REG_D.b.b0 = res1.b.l + res2.b.l;
                    break;
                  case WORD_SIZE:
                    res2.w = WORKING_REG_D.w.l + _f.CCR.Ext;
                    res1.w = DEST_REG_D.w.l;
                    res3.w = DEST_REG_D.w.l = res1.w + res2.w;
                    break;
                  case DWORD_SIZE:
                    res2.d = WORKING_REG_D.d + _f.CCR.Ext;
                    res1.d = DEST_REG_D.d; 
                    res3.d = DEST_REG_D.d = res1.d + res2.d;
                    break;
                  }
                goto aggFlagAX;
                break;
              case ADDRREGADD:   // An  qua ADDX (-An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
										--WORKING_REG_A.d; res2.b.l = GetValue(WORKING_REG_A.d) + _f.CCR.Ext;
                    // secondo il simulatore non raddoppia A7 qua..
                    --DEST_REG_A.d; res1.b.l = GetValue(DEST_REG_A.d);
                    res3.b.l = res1.b.l + res2.b.l;
                    PutValue(DEST_REG_A.d, res3.b.l);
                    break;
                  case WORD_SIZE:
                    WORKING_REG_A.d-=2; res2.w = GetShortValue(WORKING_REG_A.d) + _f.CCR.Ext;
                    DEST_REG_A.d-=2; res1.w = GetShortValue(DEST_REG_A.d);
                    res3.w = res1.w + res2.w;
                    PutShortValue(DEST_REG_A.d, res3.w);
                    break;
                  case DWORD_SIZE:
                    WORKING_REG_A.d-=4; res2.d = GetIntValue(WORKING_REG_A.d) + _f.CCR.Ext;
                    DEST_REG_A.d-=4; res1.d = GetIntValue(DEST_REG_A.d);
                    res3.d = res1.d + res2.d;
                    PutIntValue(DEST_REG_A.d, res3.d);
                    break;
                  }

aggFlagAX:
#warning verificare cmq se le varie opzioni qua vanno a posto con i flag e il comportamento speciale di addx
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    if(res3.b.l) _f.CCR.Zero=0;  // cleared or unchanged
                    _f.CCR.Sign=!!(res3.b.l & 0x80);
                    _f.CCR.Carry= !!(((res2.b.l & 0x80) & (res1.b.l & 0x80)) | ((~res3.b.l & 0x80) &
                      (res1.b.l & 0x80)) | ((res2.b.l & 0x80) & (~res3.b.l & 0x80)));
                    _f.CCR.Ovf= !!(((res2.b.l & 0x80) & (res1.b.l & 0x80) & (~res3.b.l & 0x80)) |
                      ((~res2.b.l & 0x80) & (~res1.b.l & 0x80) & (res3.b.l & 0x80)));
										break;
									case WORD_SIZE:
                    if(res3.w) _f.CCR.Zero=0;
										_f.CCR.Sign=!!(res3.w & 0x8000);
                    _f.CCR.Carry= !!(((res2.w & 0x8000) & (res1.w & 0x8000)) | ((~res3.w & 0x8000) &
                      (res1.w & 0x8000)) | ((res2.w & 0x8000) & (~res3.w & 0x8000)));
                    _f.CCR.Ovf= !!(((res2.w & 0x8000) & (res1.w & 0x8000) & (~res3.w & 0x8000)) |
                      ((~res2.w & 0x8000) & (~res1.w & 0x8000) & (res3.w & 0x8000)));
                    break;
                  case DWORD_SIZE:
                    if(res3.d) _f.CCR.Zero=0;
                    _f.CCR.Sign=!!(res3.d & 0x80000000);
                    _f.CCR.Carry= !!(((res2.d & 0x80000000) & (res1.d & 0x80000000)) | 
											((~res3.d & 0x80000000) & (res1.d & 0x80000000)) | 
											((res2.d & 0x80000000) & (~res3.d & 0x80000000)));
	                  _f.CCR.Ovf= !!(((res2.d & 0x80000000) & (res1.d & 0x80000000) & (~res3.d & 0x80000000)) |
                     ((~res2.d & 0x80000000) & (~res1.d & 0x80000000) & (res3.d & 0x80000000)));
                    break;
                  }
                _f.CCR.Ext=_f.CCR.Carry;
                goto noAggFlag;
                break;
              case ADDRADD:   // (An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GetValue(WORKING_REG_A.d);
                    break;
                  case WORD_SIZE:
                    res1.w = GetShortValue(WORKING_REG_A.d);
                    break;
                  case DWORD_SIZE:
                    res1.d = GetIntValue(WORKING_REG_A.d);
                    break;
                  }
                break;
              case ADDRPINCADD:   // (An)+
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GetValue(WORKING_REG_A.d);
                    // postinc DOPO!
                    break;
                  case WORD_SIZE:
                    res1.w = GetShortValue(WORKING_REG_A.d);
                    break;
                  case DWORD_SIZE:
                    res1.d = GetIntValue(WORKING_REG_A.d);
                    break;
                  }
                break;
              case ADDRPDECADD:   // -(An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GET8_PDEC();
                    break;
                  case WORD_SIZE:
                    res1.w = GET16_PDEC();
                    break;
                  case DWORD_SIZE:
                    res1.d = GET32_PDEC();
                    break;
                  }
                break;
              case ADDRDISPADD:   // (D16,An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res1.b.l = GET8_DISP16();
                    break;
                  case WORD_SIZE:
                    res1.w = GET16_DISP16();
                    break;
                  case DWORD_SIZE:
                    res1.d = GET32_DISP16();
                    break;
                  }
                _pc+=2;
                break;
              case ADDRIDXADD:   // (D8,An,Xn)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res1.b.l = GET8_DISP8S();
                    else
                      res1.b.l = GET8_DISP8L();
                    break;
                  case WORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res1.w = GET16_DISP8S();
                    else
                      res1.w = GET16_DISP8L();
                    break;
                  case DWORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res1.d = GET32_DISP8S();
                    else
                      res1.d = GET32_DISP8L();
                    break;
                  }
                _pc+=2;
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
                res1.d = DEST_REG_D.d;
                break;
              }
            }

          if(DIRECTION) { // direction
            switch(OPERAND_SIZE) {
              case BYTE_SIZE:
                res2.b.l = DEST_REG_D.b.b0;
                break;
              case WORD_SIZE:
                res2.w = DEST_REG_D.w.l;
                break;
              case DWORD_SIZE:
                res2.d = DEST_REG_D.d;
                break;
              }
            }
          else {
            switch(ADDRESSING) {
              case ABSOLUTEADD:    // #imm
                switch(ABSOLUTEADD_MODE) {
                  case ABSSSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        res2.b.l = GetValue((int16_t)Pipe2.w);
                        break;
                      case WORD_SIZE:
                        res2.w = GetShortValue((int16_t)Pipe2.w);
                        break;
                      case DWORD_SIZE:
                        res2.d = GetIntValue((int16_t)Pipe2.w);
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
                        res2.d = GetIntValue(Pipe2.d);
                        break;
                      }
                    _pc+=4;
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
                    res2.d = WORKING_REG_D.d;
                    break;
                  }
                break;
              case ADDRREGADD:   // An
                switch(OPERAND_SIZE) {
                  case WORD_SIZE:
                    res2.w = WORKING_REG_A.w.l;
                    break;
                  case DWORD_SIZE:
                    res2.d = WORKING_REG_A.d;
                    break;
                  }
                break;
              case ADDRADD:   // (An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = GetValue(WORKING_REG_A.d);
                    break;
                  case WORD_SIZE:
                    res2.w = GetShortValue(WORKING_REG_A.d);
                    break;
                  case DWORD_SIZE:
                    res2.d = GetIntValue(WORKING_REG_A.d);
                    break;
                  }
                break;
              case ADDRPINCADD:   // (An)+
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = GET8_PINC();
                    break;
                  case WORD_SIZE:
                    res2.w = GET16_PINC();
                    break;
                  case DWORD_SIZE:
                    res2.d = GET32_PINC();
                    break;
                  }
                break;
              case ADDRPDECADD:   // -(An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = GET8_PDEC();
                    break;
                  case WORD_SIZE:
                    res2.w = GET16_PDEC();
                    break;
                  case DWORD_SIZE:
                    res2.d = GET32_PDEC();
                    break;
                  }
                break;
              case ADDRDISPADD:   // (D16,An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    res2.b.l = GET8_DISP16();
                    break;
                  case WORD_SIZE:
                    res2.w = GET16_DISP16();
                    break;
                  case DWORD_SIZE:
                    res2.d = GET32_DISP16();
                    break;
                  }
                _pc+=2;
                break;
              case ADDRIDXADD:   // (D8,An,Xn)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res2.b.l = GET8_DISP8S();
                    else
                      res2.b.l = GET8_DISP8L();
                    break;
                  case WORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res2.w = GET16_DISP8S();
                    else
                      res2.w = GET16_DISP8L();
                    break;
                  case DWORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      res2.d = GET32_DISP8S();
                    else
                      res2.d = GET32_DISP8L();
                    break;
                  }
                _pc+=2;
                break;
              }
            }

          switch(OPERAND_SIZE) {
            case BYTE_SIZE:
              res3.b.l = res1.b.l + res2.b.l;
              break;
            case WORD_SIZE:
              res3.w = res1.w + res2.w;
              break;
            case DWORD_SIZE:
              res3.d = res1.d + res2.d;
              break;
            }

          if(DIRECTION) { // direction
            switch(ADDRESSING) {
              case ABSOLUTEADD:    // #imm
                switch(ABSOLUTEADD_MODE) {
                  case ABSSSUBADD:
                    switch(OPERAND_SIZE) {
                      case BYTE_SIZE:
                        PutValue((int16_t)Pipe2.w, res3.b.l);
                        break;
                      case WORD_SIZE:
                        PutShortValue((int16_t)Pipe2.w, res3.w);
                        break;
                      case DWORD_SIZE:
                        PutIntValue((int16_t)Pipe2.w, res3.d);
                        break;
                      }
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
                        PutIntValue(Pipe2.d, res3.d);
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
//              case ADDRREGADD:   // An CREDO DI NO idem
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    DEST_REG_D.b.b0 = res3.b.l;
                    break;
                  case WORD_SIZE:
                    DEST_REG_D.w.l = res3.w;
                    break;
                  case DWORD_SIZE:
                    DEST_REG_D.d = res3.d;
                    break;
                  }
                break;
              case ADDRREGADD:   // An
                switch(OPERAND_SIZE) {
                  case WORD_SIZE:
                    DEST_REG_A.w.l = res3.w;
                    break;
                  case DWORD_SIZE:
                    DEST_REG_A.d = res3.d;
                    break;
                  }
                break;
              case ADDRADD:   // (An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    PutValue(WORKING_REG_A.d,res3.b.l);
                    break;
                  case WORD_SIZE:
                    PutShortValue(WORKING_REG_A.d,res3.w);
                    break;
                  case DWORD_SIZE:
                    PutIntValue(WORKING_REG_A.d,res3.d);
                    break;
                  }
                break;
              case ADDRPINCADD:   // (An)+
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    PUT8_PINC();
                    break;
                  case WORD_SIZE:
                    PUT16_PINC();
                    break;
                  case DWORD_SIZE:
                    PUT32_PINC();
                    break;
                  }
                break;
              case ADDRPDECADD:   // -(An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    PutValue(WORKING_REG_A.d,res3.b.l);
                    break;
                  case WORD_SIZE:
                    PutShortValue(WORKING_REG_A.d,res3.w);
                    break;
                  case DWORD_SIZE:
                    PutIntValue(WORKING_REG_A.d,res3.d);
                    break;
                  }
                break;
              case ADDRDISPADD:   // (D16,An)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    PUT8_DISP16();
                    break;
                  case WORD_SIZE:
                    PUT16_DISP16();
                    break;
                  case DWORD_SIZE:
                    PUT32_DISP16();
                    break;
                  }
                break;
              case ADDRIDXADD:   // (D8,An,Xn)
                switch(OPERAND_SIZE) {
                  case BYTE_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      PUT8_DISP8S();
                    else
                      PUT8_DISP8L();
                    break;
                  case WORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      PUT16_DISP8S();
                    else
                      PUT16_DISP8L();
                    break;
                  case DWORD_SIZE:
                    if(!DISPLACEMENT_SIZE)
                      PUT32_DISP8S();
                    else
                      PUT32_DISP8L();
                    break;
                  }
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
                DEST_REG_D.d = res3.d;
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
        	union _CCR f2;
          
          res2.b.l=1;
          switch(ADDRESSING) {
            case ABSOLUTEADD:
              switch(ABSOLUTEADD_MODE) {
                case ABSSSUBADD:
                  res1.w = GetShortValue((int16_t)Pipe2.w);
                  _pc+=2;
                  break;
                case ABSLSUBADD:
                  res1.w = GetShortValue(Pipe2.d); 
                  _pc+=4;
                  break;
                case PCIDXSUBADD:   // (D16,PC)			 pare di no...
                  res1.w = GET16_DISPPC16();
                  _pc+=2;
                  break;
                case PCDISPSUBADD:   // (D8,Xn,PC)			pare di no..
                  if(!DISPLACEMENT_SIZE)
                    res1.w = GET16_DISPPC8S();
                  else
                    res1.w = GET16_DISPPC8L();
                  _pc+=2;
                  break;
                case IMMSUBADD:   // qua no direi
                  res1.w = Pipe2.w; // boh
                  _pc+=2;
                  break;
                }
              break;
            case DATAREGADD:   // Dn NO!
            case ADDRREGADD:   // An NO!
              break;
            case ADDRADD:   // (An)
              res1.w = GetShortValue(WORKING_REG_A.d);
              break;
            case ADDRPINCADD:   // (An)+
              res1.w = GetShortValue(WORKING_REG_A.d);
							// postinc dopo
              break;
            case ADDRPDECADD:   // -(An)
              res1.w = GET16_PDEC();
              break;
            case ADDRDISPADD:   // (D16,An)
              res1.w = GET16_DISP16();
              _pc+=2;
              break;
            case ADDRIDXADD:   // (D8,An,Xn)
              if(!DISPLACEMENT_SIZE)
                res1.w = GET16_DISP8S();
              else
                res1.w = GET16_DISP8L();
              _pc+=2;
              break;
            }
          
          res3.w = res1.w;
          switch(Pipe1.b[1] & 0b00000110) {
            case 0b000:   //ASd
              _f.CCR.Ovf=0;
              while(res2.b.l) {
                if(ROTATE_DIRECTION) {
                  _f.CCR.Ext=_f.CCR.Carry=!!(res3.w & 0x8000);
                  res3.w <<= 1;
                  if(_f.CCR.Carry && !(res3.w & 0x8000))
                    _f.CCR.Ovf=1;   // SOLO ASL è diversa! il doc breve non lo menziona @#£$% ma il simulatore pare confermare così..
                  //ASL, Arithmetic shift left, sets the V flag if the MSB changes sign at any time during the shift.
                  }
                else {
                  _f.CCR.Ext=_f.CCR.Carry=res3.b.l & 0x01;
                  res3.w = (int16_t)res3.w >> 1;
                  }
                res2.b.l--;
                }
              goto aggFlagRZ;
              break;
            case 0b010:   //LSd
              _f.CCR.Carry=0;
              while(res2.b.l) {
                if(ROTATE_DIRECTION) {
                  _f.CCR.Ext=_f.CCR.Carry=!!(res3.w & 0x8000);
                  res3.w <<= 1;
                  }
                else {
                  _f.CCR.Ext=_f.CCR.Carry=res3.b.l & 0x01;
                  res3.w >>= 1;
                  }
                res2.b.l--;
                }
              goto aggFlagRZO;
              break;
            case 0b100:   //ROXd
              while(res2.b.l) {
                if(ROTATE_DIRECTION) {  // rotate direction
                  f2.Ext=!!(res3.w & 0x8000);
                  res3.w <<= 1;
                  if(_f.CCR.Ext)
                    res3.w |= 1;
                  _f.CCR.Ext=f2.Ext;
                  }
                else {
                  f2.Ext=res3.b.l & 0x01;
                  res3.w >>= 1;
                  if(_f.CCR.Ext)
                    res3.w |= 0x8000;
                  _f.CCR.Ext=f2.Ext;
                  }
                res2.b.l--;
                }
              _f.CCR.Carry=_f.CCR.Ext;
              goto aggFlagRZO;
              break;
            case 0b110:   //ROd
              _f.CCR.Carry=0;
              while(res2.b.l) {
                if(ROTATE_DIRECTION) { // rotate direction
                  _f.CCR.Carry=!!(res3.w & 0x8000);
                  res3.w <<= 1;
                  if(res3.w & 0x8000)
                    res3.b.l |= 1;
                  }
                else {
                  _f.CCR.Carry=res3.b.l & 0x01;
                  res3.w >>= 1;
                  if(res3.b.l & 1)
                    res3.w |= 0x8000;
                  }
                }
              
aggFlagRZO:
              _f.CCR.Ovf=0;

aggFlagRZ:
              _f.CCR.Zero=!res3.w;
              _f.CCR.Sign=!!(res3.w & 0x8000);
              break;
            }
        
          switch(ADDRESSING) {
            case ABSOLUTEADD:
              switch(ABSOLUTEADD_MODE) {
                case ABSSSUBADD:
                  PutShortValue((int16_t)Pipe2.w,res3.w);
                  break;
                case ABSLSUBADD:
                  PutShortValue(Pipe2.d,res3.w); 
                  break;
                case PCIDXSUBADD:   // (D16,PC) NON dovrebbero esserci..
                case PCDISPSUBADD:   // (D8,Xn,PC) NON dovrebbero esserci..
                  break;
                case IMMSUBADD:   // qua no!
                  break;
                }
              break;
            case DATAREGADD:   // Dn NO!
            case ADDRREGADD:   // An NO!
              break;
            case ADDRADD:   // (An)
              PutShortValue(WORKING_REG_A.d,res3.w);
              break;
            case ADDRPINCADD:   // (An)+
              PUT16_PINC();
              break;
            case ADDRPDECADD:   // -(An)
              PutShortValue(WORKING_REG_A.d,res3.w);
              break;
            case ADDRDISPADD:   // (D16,An)
              PUT16_DISP16();
              break;
            case ADDRIDXADD:   // (D8,An,Xn)
              if(!DISPLACEMENT_SIZE)
                PUT16_DISP8S();
              else
                PUT16_DISP8L();
              break;
            }
          }
        else {
        	union _CCR f2;
          if(!(Pipe1.b[0] & 0b00100000)) { // i/r
            res2.b.l=Q8BIT;
            if(!res2.b.l)
              res2.b.l=8;
            }
          else {
            res2.b.l=DEST_REG_D.b.b0 & 63;
            }
          switch(OPERAND_SIZE) {
            case BYTE_SIZE:
              res3.b.l = res1.b.l = WORKING_REG_D.b.b0;
              switch(Pipe1.b[0] & 0b00011000) {
                case 0b00000:   //ASd
                  _f.CCR.Ovf=0;
                  while(res2.b.l) {
                    if(ROTATE_DIRECTION) {
                      res3.b.l <<= 1;
                      _f.CCR.Ext=_f.CCR.Carry=!!(res3.b.l & 0x80);
                      if(_f.CCR.Carry && !(res3.b.l & 0x80))
                        _f.CCR.Ovf=1;   // SOLO ASL è diversa! il doc breve non lo menziona @#£$% ma il simulatore pare confermare così..
                      //ASL, Arithmetic shift left, sets the V flag if the MSB changes sign at any time during the shift.
                      }
                    else {
                      _f.CCR.Ext=_f.CCR.Carry=res3.b.l & 0x01;
                      res3.b.l = (int8_t)res3.b.l >> 1;
                      }
                    res2.b.l--;
                    }
                  goto aggFlagRZ1;
                  break;
                case 0b01000:   //LSd
                  _f.CCR.Carry=0;
                  while(res2.b.l) {
                    if(ROTATE_DIRECTION) {
                      _f.CCR.Ext=_f.CCR.Carry=!!(res3.b.l & 0x80);
                      res3.b.l <<= 1;
                      }
                    else {
                      _f.CCR.Ext=_f.CCR.Carry=res3.b.l & 0x01;
                      res3.b.l >>= 1;
                      }
                    res2.b.l--;
                    }
                  goto aggFlagRZO1;
                  break;
                case 0b10000:   //ROXd
                  while(res2.b.l) {
                    if(ROTATE_DIRECTION) {  // rotate direction
                      f2.Ext=!!(res3.b.l & 0x80);
                      res3.b.l <<= 1;
                      if(_f.CCR.Ext)
                        res3.b.l |= 1;
                      _f.CCR.Ext=f2.Ext;
                      }
                    else {
                      f2.Ext=res3.b.l & 0x01;
                      res3.b.l >>= 1;
                      if(_f.CCR.Ext)
                        res3.b.l |= 0x80;
                      _f.CCR.Ext=f2.Ext;
                      }
                    res2.b.l--;
                    }
                  _f.CCR.Carry=_f.CCR.Ext;
                  goto aggFlagRZO1;
                  break;
                case 0b11000:   //ROd
                  _f.CCR.Carry=0;
                  while(res2.b.l) {
                    if(ROTATE_DIRECTION) { // rotate direction
                      _f.CCR.Carry=!!(res3.b.l & 0x80);
                      res3.b.l <<= 1;
                      if(_f.CCR.Carry)
                        res3.b.l |= 1;
                      }
                    else {
                      _f.CCR.Carry=res3.b.l & 0x01;
                      res3.b.l >>= 1;
                      if(_f.CCR.Carry)
                        res3.b.l |= 0x80;
                      }
                    res2.b.l--;
                    }
                  
aggFlagRZO1:
                  _f.CCR.Ovf = 0;

aggFlagRZ1:
                  _f.CCR.Zero=!res3.b.l;
                  _f.CCR.Sign=!!(res3.b.l & 0x80);
                  WORKING_REG_D.b.b0 = res3.b.l;
                  break;
                }
              break;
            case WORD_SIZE:
              res3.w = res1.w = WORKING_REG_D.w.l;
              switch(Pipe1.b[0] & 0b00011000) {
                case 0b00000:   //ASd
                  _f.CCR.Ovf=0;
                  while(res2.b.l) {
                    if(ROTATE_DIRECTION) {
                      _f.CCR.Ext=_f.CCR.Carry=!!(res3.w & 0x8000);
                      res3.w <<= 1;
                      if(_f.CCR.Carry && !(res3.w & 0x8000))
                        _f.CCR.Ovf=1;   // SOLO ASL è diversa! il doc breve non lo menziona @#£$% ma il simulatore pare confermare così..
                      //ASL, Arithmetic shift left, sets the V flag if the MSB changes sign at any time during the shift.
                      }
                    else {
                      _f.CCR.Ext=_f.CCR.Carry=res3.b.l & 0x01;
                      res3.w = (int16_t)res3.w >> 1;
                      }
                    res2.b.l--;
                    }
                  goto aggFlagRZ2;
                  break;
                case 0b01000:   //LSd
                  _f.CCR.Carry=0;
                  while(res2.b.l) {
                    if(ROTATE_DIRECTION) {
                      _f.CCR.Ext=_f.CCR.Carry=!!(res3.w & 0x8000);
                      res3.w <<= 1;
                      }
                    else {
                      _f.CCR.Ext=_f.CCR.Carry=res3.b.l & 0x01;
                      res3.w >>= 1;
                      }
                    res2.b.l--;
                    }
                  goto aggFlagRZO2;
                  break;
                case 0b10000:   //ROXd
                  while(res2.b.l) {
                    if(ROTATE_DIRECTION) {  // rotate direction
                      f2.Ext=!!(res3.w & 0x8000);
                      res3.w <<= 1;
                      if(_f.CCR.Ext)
                        res3.b.l |= 1;
                      _f.CCR.Ext=f2.Ext;
                      }
                    else {
                      f2.Ext=res3.b.l & 0x01;
                      res3.w >>= 1;
                      if(_f.CCR.Ext)
                        res3.w |= 0x8000;
                      _f.CCR.Ext=f2.Ext;
                      }
                    res2.b.l--;
                    }
                  _f.CCR.Carry=_f.CCR.Ext;
                  goto aggFlagRZO2;
                  break;
                case 0b11000:   //ROd
                  _f.CCR.Carry=0;
                  while(res2.b.l) {
                    if(ROTATE_DIRECTION) { // rotate direction
                      _f.CCR.Carry=!!(res3.w & 0x8000);
                      res3.w <<= 1;
                      if(_f.CCR.Carry)
                        res3.b.l |= 1;
                      }
                    else {
                      _f.CCR.Carry=res3.b.l & 0x01;
                      res3.w >>= 1;
                      if(_f.CCR.Carry)
                        res3.w |= 0x8000;
                      }
                    res2.b.l--;
                    }
                  
aggFlagRZO2:
                  _f.CCR.Ovf = 0;

aggFlagRZ2:
                  _f.CCR.Zero=!res3.w;
                  _f.CCR.Sign=!!(res3.w & 0x8000);
                  WORKING_REG_D.w.l = res3.w;
                  break;
                }
              break;
            case DWORD_SIZE:
              res3.d = res1.d = WORKING_REG_D.d;
              switch(Pipe1.b[0] & 0b00011000) {
                case 0b00000:   //ASd
                  _f.CCR.Ovf=0;
                  while(res2.b.l) {
                    if(ROTATE_DIRECTION) {
                      _f.CCR.Ext=_f.CCR.Carry=!!(res3.d & 0x80000000);
                      res3.d <<= 1;
                      if(_f.CCR.Carry && !(res3.d & 0x80000000))
                        _f.CCR.Ovf=1;   // SOLO ASL è diversa! il doc breve non lo menziona @#£$% ma il simulatore pare confermare così..
                      //ASL, Arithmetic shift left, sets the V flag if the MSB changes sign at any time during the shift.
                      }
                    else {
                      _f.CCR.Ext=_f.CCR.Carry=res3.b.l & 0x01;
                      res3.d = (int32_t)res3.d >> 1;
                      }
                    res2.b.l--;
                    }
                  goto aggFlagRZ4;
                  break;
                case 0b01000:   //LSd
                  _f.CCR.Carry=0;
                  while(res2.b.l) {
                    if(ROTATE_DIRECTION) {
                      _f.CCR.Ext=_f.CCR.Carry=!!(res3.d & 0x80000000);
                      res3.d <<= 1;
                      }
                    else {
                      _f.CCR.Ext=_f.CCR.Carry=res3.b.l & 0x01;
                      res3.d >>= 1;
                      }
                    res2.b.l--;
                    }
                  goto aggFlagRZO4;
                  break;
                case 0b10000:   //ROXd
                  while(res2.b.l) {
                    if(ROTATE_DIRECTION) {  // rotate direction
                      f2.Ext=!!(res3.d & 0x80000000);
                      res3.d <<= 1;
                      if(_f.CCR.Ext)
                        res3.d |= 1;
                      _f.CCR.Ext=f2.Ext;
                      }
                    else {
                      f2.Ext=res3.b.l & 0x1;
                      res3.d >>= 1;
                      if(_f.CCR.Ext)
                        res3.d |= 0x80000000;
                      _f.CCR.Ext=f2.Ext;
                      }
                    res2.b.l--;
                    }
                  _f.CCR.Carry=_f.CCR.Ext;
                  goto aggFlagRZO4;
                  break;
                case 0b11000:   //ROd
                  _f.CCR.Carry=0;
                  while(res2.b.l) {
                    if(ROTATE_DIRECTION) { // rotate direction
                      _f.CCR.Carry=!!(res3.d & 0x80000000);
                      res3.d <<= 1;
                      if(_f.CCR.Carry)
                        res3.b.l |= 1;
                      }
                    else {
                      _f.CCR.Carry=res3.b.l & 0x1;
                      res3.d >>= 1;
                      if(_f.CCR.Carry)
                        res3.d |= 0x80000000;
                      }
                    res2.b.l--;
                    }
                  
aggFlagRZO4:
                  _f.CCR.Ovf = 0;

aggFlagRZ4:
                  _f.CCR.Zero=!res3.d;
                  _f.CCR.Sign=!!(res3.d & 0x80000000);
                  WORKING_REG_D.d = res3.d;
                  break;
                }
              break;
            }
          }
				break;
        
 			case 0xf0:   // exception 1111 (coprocessor volendo, solo 68020?)
 			case 0xf1:
 			case 0xf2:
 			case 0xf3:
 			case 0xf4:
 			case 0xf5:
 			case 0xf6:
 			case 0xf7:
 			case 0xf8:
 			case 0xf9:
 			case 0xfa:
 			case 0xfb:
 			case 0xfc:
 			case 0xfd:
 			case 0xfe:
 			case 0xff:
		    res3.b.l=11;
		    goto doTrap;
				break;
        
      default:
		    res3.b.l=4;
		    goto doTrap;
				break;

			}
    
		} while(!fExit);
	}

