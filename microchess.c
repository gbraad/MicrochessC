//***********************************************************************
//
//  Kim-1 MicroChess (c) 1976-2005 Peter Jennings, www.benlo.com
//  6502 emulation   (c) 2005 Bill Forster
//
//  Runs an emulation of the Kim-1 Microchess on any standard C platform
//
//***********************************************************************

// All rights reserved.

// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 3. The name of the author may not be used to endorse or promote products
//    derived from this software without specific prior written permission.

// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ''AS IS'' AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
// OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
// NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES// LOSS OF USE,
// DATA, OR PROFITS// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
// THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

//**********************************************************************
//*
//*  Part 1
//*  ------
//*  Create virtual 6502 platform using standard C facilities.
//*  Goal is to run Microchess on any platform supporting C.
//*
//*       Part 1 added July 2005 by Bill Forster (www.triplehappy.com)
//**********************************************************************

// Standard library include files
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <math.h>

// Use <setjmp.h> macros and functions to emulate the "jump to reset
//  stack pointer then restart program" behaviour used by microchess
jmp_buf jmp_chess;
#define EXIT        -1
#define RESTART     -2
void RESTART_CHESS( void )  // start CHESS program with reset stack
{
    longjmp( jmp_chess, RESTART );
}
void EXIT_TO_SYSTEM( void ) // return to operating system
{
    longjmp( jmp_chess, EXIT );
}

// 6502 emulation memory
typedef unsigned char byte;
byte zeropage[256];
byte stack[256];
byte stack_cy[256];
byte stack_v[256];

// 6502 emulation registers
byte reg_a, reg_f, reg_x, reg_y, reg_s, reg_cy, reg_v, temp_cy;
unsigned int temp1, temp2;

// Debug stuff
#if 0
    #define DBG , register_dump()
#else
    #define DBG
#endif
void register_dump( void )
{
    printf( "A=%02x X=%02x Y=%02x S=%02x F=%02X CY=%d V=%d\n",
        reg_a, reg_x, reg_y, reg_s, reg_f, reg_cy, reg_v );
}

// 6502 emulation macros - register moves
#define T(src,dst)          reg_f = (dst) = (src)    DBG
#define A reg_a
#define S reg_s
#define X reg_x
#define Y reg_y
#define TYA                 T(Y,A)
#define TXS                 T(X,S)
#define TAX                 T(A,X)
#define TAY                 T(A,Y)
#define TSX                 T(S,X)
#define TXA                 T(X,A)

// 6502 emulation macros - branches
#define BEQ(label)          if( reg_f == 0 )     goto label
#define BNE(label)          if( reg_f != 0 )     goto label
#define BPL(label)          if( ! (reg_f&0x80) ) goto label
#define BMI(label)          if( reg_f & 0x80 )   goto label
#define BCC(label)          if( !reg_cy )        goto label
#define BCS(label)          if( reg_cy )         goto label
#define BVC(label)          if( !reg_v )         goto label
#define BVS(label)          if( reg_v )          goto label
#define BRA(label) /*extra*/ goto label

// 6502 emulation macros - call/return from functions
#define JSR(func)           func()
#define RTS                 return

// 6502 emulation macros - jump to functions, note that in
//  assembly language jumping to a function is a more efficient
//  way of calling the function then returning, so we emulate
//  that in the high level language by (actually) calling then
//  returning. There is no JEQ 6502 opcode, but it's useful to
//  us so we have made it up! (like BRA, SEV)
#define JMP(func)           if( 1 )          { func(); return; } \
                            else // else eats ';'
#define JEQ(func) /*extra*/ if( reg_f == 0 ) { func(); return; } \
                            else // else eats ';'

// 6502 emulation macros - load registers
//  Addressing conventions;
//   default addressing mode is zero page, else indicate with suffix;
//   i = immediate
//   x = indexed, zero page
//   f = indexed, not zero page (f for "far")
#define ZP(addr8)           (zeropage[ (byte) (addr8) ])
#define ZPX(addr8,idx)      (zeropage[ (byte) ((addr8)+(idx)) ])
#define LDAi(dat8)          reg_f = reg_a = dat8                  DBG
#define LDAx(addr8,idx)     reg_f = reg_a = ZPX(addr8,idx)        DBG
#define LDAf(addr16,idx)    reg_f = reg_a = (addr16)[idx]         DBG
#define LDA(addr8)          reg_f = reg_a = ZP(addr8)             DBG
#define LDXi(dat8)          reg_f = reg_x = dat8                  DBG
#define LDX(addr8)          reg_f = reg_x = ZP(addr8)             DBG
#define LDYi(dat8)          reg_f = reg_y = dat8                  DBG
#define LDY(addr8)          reg_f = reg_y = ZP(addr8)             DBG
#define LDYx(addr8,idx)     reg_f = reg_y = ZPX(addr8,idx)        DBG

// 6502 emulation macros - store registers
#define STA(addr8)          ZP(addr8)      = reg_a                DBG
#define STAx(addr8,idx)     ZPX(addr8,idx) = reg_a                DBG
#define STX(addr8)          ZP(addr8)      = reg_x                DBG
#define STY(addr8)          ZP(addr8)      = reg_y                DBG
#define STYx(addr8,idx)     ZPX(addr8,idx) = reg_y                DBG

// 6502 emulation macros - set/clear flags
#define CLD            // luckily CPU's BCD flag is cleared then never set
#define CLC                 reg_cy = 0                            DBG
#define SEC                 reg_cy = 1                            DBG
#define CLV                 reg_v  = 0                            DBG
#define SEV /*extra*/       reg_v  = 1  /*avoid problematic V emulation*/ DBG

// 6502 emulation macros - accumulator logical operations
#define ANDi(dat8)          reg_f = (reg_a &= dat8)               DBG
#define ORA(addr8)          reg_f = (reg_a |= ZP(addr8))          DBG

// 6502 emulation macros - shifts and rotates
#define ASL(addr8)          reg_cy = (ZP(addr8)&0x80) ? 1 : 0,  \
                            ZP(addr8) = ZP(addr8)<<1,           \
                            reg_f = ZP(addr8)                     DBG
#define ROL(addr8)          temp_cy = (ZP(addr8)&0x80) ? 1 : 0, \
                            ZP(addr8) = ZP(addr8)<<1,           \
                            ZP(addr8) |= reg_cy,                \
                            reg_cy = temp_cy,                   \
                            reg_f = ZP(addr8)                     DBG
#define LSR                 reg_cy = reg_a & 0x01,              \
                            reg_a  = reg_a>>1,                  \
                            reg_a  &= 0x7f,                     \
                            reg_f = reg_a                         DBG

// 6502 emulation macros - push and pull
#define PHA                 stack[reg_s--]  = reg_a               DBG
#define PLA                 reg_a           = stack[++reg_s]      DBG
#define PHY                 stack[reg_s--]  = reg_y               DBG
#define PLY                 reg_y           = stack[++reg_s]      DBG
#define PHP                 stack   [reg_s] = reg_f,       \
                            stack_cy[reg_s] = reg_cy,      \
                            stack_v [reg_s] = reg_v,       \
                            reg_s--                               DBG
#define PLP                 reg_s++,                       \
                            reg_f  = stack   [reg_s],      \
                            reg_cy = stack_cy[reg_s],      \
                            reg_v  = stack_v [reg_s]              DBG

// 6502 emulation macros - compare
#define cmp(reg,dat)        reg_f  = ((reg) - (dat)), \
                            reg_cy = ((reg) >= (dat) ? 1 : 0)  DBG
#define CMPi(dat8)          cmp( reg_a, dat8 )
#define CMP(addr8)          cmp( reg_a, ZP(addr8) )
#define CMPx(addr8,idx)     cmp( reg_a, ZPX(addr8,idx) )
#define CMPf(addr16,idx)    cmp( reg_a, (addr16)[idx] )
#define CPXi(dat8)          cmp( reg_x, dat8 )
#define CPXf(addr16,idx)    cmp( reg_x, (addr16)[idx] )
#define CPYi(dat8)          cmp( reg_y, dat8 )

// 6502 emulation macros - increment,decrement
#define DEX                 reg_f = --reg_x                       DBG
#define DEY                 reg_f = --reg_y                       DBG
#define DEC(addr8)          reg_f = --ZP(addr8)                   DBG
#define INX                 reg_f = ++reg_x                       DBG
#define INY                 reg_f = ++reg_y                       DBG
#define INC(addr8)          reg_f = ++ZP(addr8)                   DBG
#define INCx(addr8,idx)     reg_f = ++ZPX(addr8,idx)              DBG

// 6502 emulation macros - add
#define adc(dat)            temp1 = reg_a,                   \
                            temp2 = (dat),                   \
                            temp1 += (temp2+(reg_cy?1:0)),   \
                            reg_f = reg_a = (byte)temp1,     \
                            reg_cy = ((temp1&0xff00)?1:0)         DBG
#define ADCi(dat8)          adc( dat8 )
#define ADC(addr8)          adc( ZP(addr8) )
#define ADCx(addr8,idx)     adc( ZPX(addr8,idx) )
#define ADCf(addr16,idx)    adc( (addr16)[idx] )

// 6502 emulation macros - subtract
//   (note that both as an input and an output cy flag has opposite
//    sense to that used for adc(), seems unintuitive to me)
#define sbc(dat)            temp1 = reg_a,                   \
                            temp2 = (dat),                   \
                            temp1 -= (temp2+(reg_cy?0:1)),   \
                            reg_f = reg_a = (byte)temp1,     \
                            reg_cy = ((temp1&0xff00)?0:1)         DBG
#define SBC(addr8)          sbc( ZP(addr8) )
#define SBCx(addr8,idx)     sbc( ZPX(addr8,idx) )

// Test some of the trickier opcodes (hook this up as needed)
void test_function( void )
{
    byte hi, lo;
                LDAi    (0x33);     // 0x4444 - 0x3333 = 0x1111
                STA     (0);
                STA     (1);
                LDAi    (0x44);
                SEC;
                SBC     (0);
                lo      = reg_a;
                LDAi    (0x44);
                SBC     (1);
                hi      = reg_a;

                LDAi    (0x44);     // 0x3333 - 0x4444 = 0xeeef
                STA     (0);
                STA     (1);
                LDAi    (0x33);
                SEC;
                SBC     (0);
                lo      = reg_a;
                LDAi    (0x33);
                SBC     (1);
                hi      = reg_a;

                LDAi    (0x33);     // 0x3333 + 0x4444 = 0x7777
                STA     (0);
                STA     (1);
                LDAi    (0x44);
                CLC;
                ADC     (0);
                lo      = reg_a;
                LDAi    (0x44);
                ADC     (1);
                hi      = reg_a;
}


//**********************************************************************
//*
//*  Part 2
//*  ------
//*  Original microchess program by Peter Jennings, www.benlo.com
//*  In this form, 6502 assembly language has been minimally transformed
//*  to run with the virtual 6502 in C facilities created in part 1.
//*   (New comments by Bill Forster are identified with text (WRF))
//**********************************************************************

//
// page zero variables
//
const byte BOARD     = 0x50;
const byte BK        = 0x60;
const byte PIECE     = 0xB0;
const byte SQUARE    = 0xB1;
const byte SP2       = 0xB2;
const byte SP1       = 0xB3;
const byte INCHEK    = 0xB4;
const byte STATE     = 0xB5;
const byte MOVEN     = 0xB6;
const byte REV       = 0xB7;
const byte OMOVE     = 0xDC;
const byte WCAP0     = 0xDD;
const byte COUNT     = 0xDE;
const byte BCAP2     = 0xDE;
const byte WCAP2     = 0xDF;
const byte BCAP1     = 0xE0;
const byte WCAP1     = 0xE1;
const byte BCAP0     = 0xE2;
const byte MOB       = 0xE3;
const byte MAXC      = 0xE4;
const byte CC        = 0xE5;
const byte PCAP      = 0xE6;
const byte BMOB      = 0xE3;
const byte BMAXC     = 0xE4;
const byte BMCC      = 0xE5;     // was BCC (TASS doesn't like it as a label)
const byte BMAXP     = 0xE6;
const byte XMAXC     = 0xE8;
const byte WMOB      = 0xEB;
const byte WMAXC     = 0xEC;
const byte WCC       = 0xED;
const byte WMAXP     = 0xEE;
const byte PMOB      = 0xEF;
const byte PMAXC     = 0xF0;
const byte PCC       = 0xF1;
const byte PCP       = 0xF2;
const byte OLDKY     = 0xF3;
const byte BESTP     = 0xFB;
const byte BESTV     = 0xFA;
const byte BESTM     = 0xF9;
const byte DIS1      = 0xFB;
const byte DIS2      = 0xFA;
const byte DIS3      = 0xF9;
const byte temp      = 0xFC;

// (WRF) For C version, data definitions precede code references to data
byte SETW[]   = {       0x03, 0x04, 0x00, 0x07, 0x02, 0x05, 0x01, 0x06,
                        0x10, 0x17, 0x11, 0x16, 0x12, 0x15, 0x14, 0x13,
                        0x73, 0x74, 0x70, 0x77, 0x72, 0x75, 0x71, 0x76,
                        0x60, 0x67, 0x61, 0x66, 0x62, 0x65, 0x64, 0x63
                };

byte MOVEX[]  = {       0x00, 0xF0, 0xFF, 0x01, 0x10, 0x11, 0x0F, 0xEF, 0xF1,
                        0xDF, 0xE1, 0xEE, 0xF2, 0x12, 0x0E, 0x1F, 0x21
                };

byte POINTS[] = {       0x0B, 0x0A, 0x06, 0x06, 0x04, 0x04, 0x04, 0x04,
                        0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02
                };

byte OPNING[] = {       0x99, 0x25, 0x0B, 0x25, 0x01, 0x00, 0x33, 0x25,
                        0x07, 0x36, 0x34, 0x0D, 0x34, 0x34, 0x0E, 0x52,
                        0x25, 0x0D, 0x45, 0x35, 0x04, 0x55, 0x22, 0x06,
                        0x43, 0x33, 0x0F, 0xCC
                };

// (WRF) level information:
//       | Level       | addr=02F2 |  addr=018B
//       |             | (level1)  |  (level2)
//       +-------------+-----------+-------------
//       | SUPER BLITZ |    00     |    FF
//       | BLITZ       |    00     |    FB
//       | NORMAL      |    08     |    FB
static byte level1=8;
static byte level2=0xfb;

// (WRF) Forward declarations
void CHESS( void );
void JANUS( void );
void INPUT( void );
void DISP( void );
void GNMZ( void );
void GNMX( void );
void GNM( void );
void RUM( void );
void STRV( void );
void SNGMV( void );
void LINE( void );
void REVERSE( void );
void CMOVE( void );
void RESET( void );
void GENRM( void );
void UMOVE( void );
void MOVE( void );
void CKMATE( void );
void GO( void );
void DISMV( void );
void STRATGY( void );
void POUT( void );
void POUT5(void );
void POUT8(void );
void POUT9( void );
void POUT10( void );
void POUT12( void );
void POUT13( void );
void KIN( void );
void syskin( void );
void syschout( void );
void syshexout( void );
void PrintDig( void );

// WRF debug stuff
static void show_move_evaluation( int ivalue );
static void show_move_generation( byte src, byte dst );
static int bool_show_move_evaluation;
static int bool_show_move_generation;

// Start here; Was  *=$1000   ; load into RAM @ $1000-$15FF
int main( int argc, char* argv[] )
{
                LDAi    (0x00);             // REVERSE TOGGLE
                STA     (REV);
             // JSR     (Init_6551);
                if( EXIT != setjmp(jmp_chess) )
                    CHESS();    // after setjmp() and then any
                                //  subsequent RESTART_CHESS()
                return(0);  // after EXIT_TO_SYSTEM()
}

void CHESS( void )
{
CHESS_BEGIN:                                //
                CLD;                        // INITIALIZE
                LDXi    (0xFF);             // TWO STACKS
                TXS;
                LDXi    (0xC8);
                STX     (SP2);
//
//       ROUTINES TO LIGHT LED
//       DISPLAY AND GET KEY
//       FROM KEYBOARD
//
/*OUT:*/        JSR     (POUT);             // DISPLAY AND
                JSR     (KIN);              // GET INPUT   *** my routine waits for a keypress
//              CMP     (OLDKY);            // KEY IN ACC  *** no need to debounce
//              BEQ     (OUT);              // (DEBOUNCE)
//              STA     (OLDKY);
//
                CMPi    (0x43);             // [C]
                BNE     (NOSET);            // SET UP
                LDXi    (0x1F);             // BOARD
WHSET:          LDAf    (SETW,X);           // FROM
                STAx    (BOARD,X);          // SETW
                DEX;
                BPL     (WHSET);
                LDXi    (0x1B);             // *ADDED
                STX     (OMOVE);            // INITS TO 0xFF
                LDAi    (0xCC);             // Display CCC
                BNE     (CLDSP);
//
NOSET:          CMPi    (0x45);             // [E]
                BNE     (NOREV);            // REVERSE
                JSR     (REVERSE);          // BOARD IS
                SEC;
                LDAi    (0x01);
                SBC     (REV);
                STA     (REV);              // TOGGLE REV FLAG
                LDAi    (0xEE);             // IS
                BNE     (CLDSP);
//
NOREV:          CMPi    (0x40);             // [P]
                BNE     (NOGO);             // PLAY CHESS
                JSR     (GO);
CLDSP:          STA     (DIS1);             // DISPLAY
                STA     (DIS2);             // ACROSS
                STA     (DIS3);             // DISPLAY
                BNE     (CHESS_BEGIN);
//
NOGO:           CMPi    (0x0D);             // [Enter]
                BNE     (NOMV);             // MOVE MAN
                JSR     (MOVE);             // AS ENTERED
                JMP     (DISP);             //
NOMV:           CMPi    (0x41);             // [Q] ***Added to allow game exit***
                BEQ     (DONE);             // quit the game, exit back to system.
                JMP     (INPUT);            //
DONE:           JMP     (EXIT_TO_SYSTEM);   // JMP (0xFF00); // *** MUST set this to YOUR OS starting address
}

//
//       THE ROUTINE JANUS DIRECTS THE
//       ANALYSIS BY DETERMINING WHAT
//       SHOULD OCCUR AFTER EACH MOVE
//       GENERATED BY GNM
//
//
//
void JANUS( void )
{               LDX     (STATE);
                BMI     (NOCOUNT);
//
//       THIS ROUTINE COUNTS OCCURRENCES
//       IT DEPENDS UPON STATE TO INDEX
//       THE CORRECT COUNTERS
//
/*COUNTS:*/     LDA     (PIECE);
                BEQ     (OVER);             // IF STATE=8
                CPXi    (0x08);             // DO NOT COUNT
                BNE     (OVER);             // BLK MAX CAP
                CMP     (BMAXP);            // MOVES FOR
                BEQ     (XRT);              // WHITE
//
OVER:           INCx    (MOB,X);            // MOBILITY
                CMPi    (0x01);             //  + QUEEN
                BNE     (NOQ);              // FOR TWO
                INCx    (MOB,X);
//
NOQ:            BVC     (NOCAP);
                LDYi    (0x0F);             // CALCULATE
                LDA     (SQUARE);           // POINTS
ELOOP:          CMPx    (BK,Y);             // CAPTURED
                BEQ     (FOUN);             // BY THIS
                DEY;                        // MOVE
                BPL     (ELOOP);
FOUN:           LDAf    (POINTS,Y);
                CMPx    (MAXC,X);
                BCC     (LESS);             // SAVE IF
                STYx    (PCAP,X);           // BEST THIS
                STAx    (MAXC,X);           // STATE
//
LESS:           CLC;
                PHP;                        // ADD TO
                ADCx    (CC,X);             // CAPTURE
                STAx    (CC,X);             // COUNTS
                PLP;
//
NOCAP:          CPXi    (0x04);
                BEQ     (ON4);
                BMI     (TREE);             //(=00 ONLY)
XRT:            RTS;

//
//      GENERATE FURTHER MOVES FOR COUNT
//      AND ANALYSIS
//
ON4:            LDA     (XMAXC);            // SAVE ACTUAL
                STA     (WCAP0);            // CAPTURE
                LDAi    (0x00);             // STATE=0
                STA     (STATE);
                JSR     (MOVE);             // GENERATE
                JSR     (REVERSE);          // IMMEDIATE
                JSR     (GNMZ);             // REPLY MOVES
                JSR     (REVERSE);
//
                LDAi    (0x08);             // STATE=8
                STA     (STATE);            // GENERATE
                JSR     (GNM);              // CONTINUATION
                JSR     (UMOVE);            // MOVES
//
                JMP     (STRATGY);          //
NOCOUNT:        CPXi    (0xF9);
                BNE     (TREE);
//
//      DETERMINE IF THE KING CAN BE
//      TAKEN, USED BY CHKCHK
//
                LDA     (BK);               // IS KING
                CMP     (SQUARE);           // IN CHECK?
                BNE     (RETJ);             // SET INCHEK=0
                LDAi    (0x00);             // IF IT IS
                STA     (INCHEK);
RETJ:           RTS;

//
//      IF A PIECE HAS BEEN CAPTURED BY
//      A TRIAL MOVE, GENERATE REPLIES &
//      EVALUATE THE EXCHANGE GAIN/LOSS
//
TREE:           BVC     (RETJ);             // NO CAP
                LDYi    (0x07);             // (PIECES)
                LDA     (SQUARE);
LOOPX:          CMPx    (BK,Y);
                BEQ     (FOUNX);
                DEY;
                BEQ     (RETJ);             // (KING)
                BPL     (LOOPX);            // SAVE
FOUNX:          LDAf    (POINTS,Y);         // BEST CAP
                CMPx    (BCAP0,X);          // AT THIS
                BCC     (NOMAX);            // LEVEL
                STAx    (BCAP0,X);
NOMAX:          DEC     (STATE);
                LDAf    (&level2,0);        // IF STATE=FB  (WRF, was LDAi (0xFB);)
                CMP     (STATE);            // TIME TO TURN
                BEQ     (UPTREE);           // AROUND
                JSR     (GENRM);            // GENERATE FURTHER
UPTREE:         INC     (STATE);            // CAPTURES
                RTS;
}


//
//      THE PLAYER'S MOVE IS INPUT
//
void INPUT( void )
{
                CMPi    (0x08);             // NOT A LEGAL
                BCS     (ERROR);            // SQUARE #
                JSR     (DISMV);
                JMP     (DISP);             // fall through
ERROR:          JMP     (RESTART_CHESS);
}

void DISP( void )
{
                LDXi    (0x1F);
SEARCH:         LDAx    (BOARD,X);
                CMP     (DIS2);
                BEQ     (HERE);             // DISPLAY
                DEX;                        // PIECE AT
                BPL     (SEARCH);           // FROM
HERE:           STX     (DIS1);             // SQUARE
                STX     (PIECE);
                JMP     (RESTART_CHESS);
}

//
//      GENERATE ALL MOVES FOR ONE
//      SIDE, CALL JANUS AFTER EACH
//      ONE FOR NEXT STEP
//
//
void GNMZ( void )
{
                LDXi    (0x10);             // CLEAR
                JMP     (GNMX);             // fall through
}

void GNMX( void )
{
                LDAi    (0x00);             // COUNTERS
CLEAR:          STAx    (COUNT,X);
                DEX;
                BPL     (CLEAR);
                JMP     (GNM);              // fall though
}

void GNM( void )
{
                LDAi    (0x10);             // SET UP
                STA     (PIECE);            // PIECE
NEWP:           DEC     (PIECE);            // NEW PIECE
                BPL     (NEX);              // ALL DONE?
                RTS;                        //    -YES
//
NEX:            JSR     (RESET);            // READY
                LDY     (PIECE);            // GET PIECE
                LDXi    (0x08);
                STX     (MOVEN);            // COMMON START
                CPYi    (0x08);             // WHAT IS IT?
                BPL     (PAWN);             // PAWN
                CPYi    (0x06);
                BPL     (KNIGHT);           // KNIGHT
                CPYi    (0x04);
                BPL     (BISHOP);           // BISHOP
                CPYi    (0x01);
                BEQ     (QUEEN);            // QUEEN
                BPL     (ROOK);             // ROOK
//
KING:           JSR     (SNGMV);            // MUST BE KING!
                BNE     (KING);             // MOVES
                BEQ     (NEWP);             // 8 TO 1
QUEEN:          JSR     (LINE);
                BNE     (QUEEN);            // MOVES
                BEQ     (NEWP);             // 8 TO 1
//
ROOK:           LDXi    (0x04);
                STX     (MOVEN);            // MOVES
AGNR:           JSR     (LINE);             // 4 TO 1
                BNE     (AGNR);
                BEQ     (NEWP);
//
BISHOP:         JSR     (LINE);
                LDA     (MOVEN);            // MOVES
                CMPi    (0x04);             // 8 TO 5
                BNE     (BISHOP);
                BEQ     (NEWP);
//
KNIGHT:         LDXi    (0x10);
                STX     (MOVEN);            // MOVES
AGNN:           JSR     (SNGMV);            // 16 TO 9
                LDA     (MOVEN);
                CMPi    (0x08);
                BNE     (AGNN);
                BEQ     (NEWP);
//
PAWN:           LDXi    (0x06);
                STX     (MOVEN);
P1:             JSR     (CMOVE);            // RIGHT CAP?
                BVC     (P2);
                BMI     (P2);
                JSR     (JANUS);            // YES
P2:             JSR     (RESET);
                DEC     (MOVEN);            // LEFT CAP?
                LDA     (MOVEN);
                CMPi    (0x05);
                BEQ     (P1);
P3:             JSR     (CMOVE);            // AHEAD
                BVS     (NEWP);             // ILLEGAL
                BMI     (NEWP);
                JSR     (JANUS);
                LDA     (SQUARE);           // GETS TO
                ANDi    (0xF0);             // 3RD RANK?
                CMPi    (0x20);
                BEQ     (P3);               // DO DOUBLE
                BRA     (NEWP);             // JMP (NEWP);
}

//
//      CALCULATE SINGLE STEP MOVES
//      FOR K,N
//
void SNGMV( void )
{
                JSR     (CMOVE);            // CALC MOVE
                BMI     (ILL1);             // -IF LEGAL
                JSR     (JANUS);            // -EVALUATE
ILL1:           JSR     (RESET);
                DEC     (MOVEN);
                RTS;
}

//
//     CALCULATE ALL MOVES DOWN A
//     STRAIGHT LINE FOR Q,B,R
//
void LINE( void )
{
LINE:           JSR     (CMOVE);            // CALC MOVE
                BCC     (OVL);              // NO CHK
                BVC     (LINE);             // NOCAP
OVL:            BMI     (ILL);              // RETURN
                PHP;
                JSR     (JANUS);            // EVALUATE POSN
                PLP;
                BVC     (LINE);             // NOT A CAP
ILL:            JSR     (RESET);            // LINE STOPPED
                DEC     (MOVEN);            // NEXT DIR
                RTS;
}

//
//      EXCHANGE SIDES FOR REPLY
//      ANALYSIS
//
void REVERSE( void )
{
                LDXi    (0x0F);
ETC:            SEC;
                LDYx    (BK,X);             // SUBTRACT
                LDAi    (0x77);             // POSITION
                SBCx    (BOARD,X);          // FROM 77
                STAx    (BK,X);
                STYx    (BOARD,X);          // AND
                SEC;
                LDAi    (0x77);             // EXCHANGE
                SBCx    (BOARD,X);          // PIECES
                STAx    (BOARD,X);
                DEX;
                BPL     (ETC);
                RTS;
}
//
//        CMOVE CALCULATES THE TO SQUARE
//        USING SQUARE AND THE MOVE
//       TABLE  FLAGS SET AS FOLLOWS:
//       N - ILLEGAL MOVE
//       V - CAPTURE (LEGAL UNLESS IN CH)
//       C - ILLEGAL BECAUSE OF CHECK
//       [MY THANKS TO JIM BUTTERFIELD
//        WHO WROTE THIS MORE EFFICIENT
//        VERSION OF CMOVE]
//
void CMOVE( void )
{
    byte src;
                LDA     (SQUARE);           // GET SQUARE
                src     = reg_a;
                LDX     (MOVEN);            // MOVE POINTER
                CLC;
                ADCf    (MOVEX,X);          // MOVE LIST
                STA     (SQUARE);           // NEW POS'N
                ANDi    (0x88);
                BNE     (ILLEGAL);          // OFF BOARD
                LDA     (SQUARE);
                if( bool_show_move_generation )
                    show_move_generation( src, reg_a );

//
                LDXi    (0x20);
LOOP:           DEX;                        // IS TO
                BMI     (NO);               // SQUARE
                CMPx    (BOARD,X);          // OCCUPIED?
                BNE     (LOOP);
//
                CPXi    (0x10);             // BY SELF?
                BMI     (ILLEGAL);
//
             // LDAi    (0x7F);             // MUST BE CAP!
             // ADCi    (0x01);             // SET V FLAG
                SEV;    LDAi(0x80);         // Avoid problematic V emulation
                BVS     (SPX);              // (JMP)
//
NO:             CLV;                        // NO CAPTURE
//
SPX:            LDA     (STATE);            // SHOULD WE
                BMI     (RETL);             // DO THE
                CMPf    (&level1,0);        // CHECK CHECK? (WRF: was CMPi (0x08);)
                BPL     (RETL);
//
//        CHKCHK REVERSES SIDES
//       AND LOOKS FOR A KING
//       CAPTURE TO INDICATE
//       ILLEGAL MOVE BECAUSE OF
//       CHECK  SINCE THIS IS
//       TIME CONSUMING, IT IS NOT
//       ALWAYS DONE
//
/*CHKCHK:*/     PHA;                        // STATE
                PHP;
                LDAi    (0xF9);
                STA     (STATE);            // GENERATE
                STA     (INCHEK);           // ALL REPLY
                JSR     (MOVE);             // MOVES TO
                JSR     (REVERSE);          // SEE IF KING
                JSR     (GNM);              // IS IN
                JSR     (RUM);              // CHECK
                PLP;
                PLA;
                STA     (STATE);
                LDA     (INCHEK);
                BMI     (RETL);             // NO - SAFE
                SEC;                        // YES - IN CHK
                LDAi    (0xFF);
                RTS;
//
RETL:           CLC;                        // LEGAL
                LDAi    (0x00);             // RETURN
                RTS;
//
ILLEGAL:        LDAi    (0xFF);
                CLC;                        // ILLEGAL
                CLV;                        // RETURN
                RTS;
}

//
//       REPLACE PIECE ON CORRECT SQUARE
//
void RESET( void )
{
                LDX     (PIECE);            // GET LOGAT
                LDAx    (BOARD,X);          // FOR PIECE
                STA     (SQUARE);           // FROM BOARD
                RTS;
}


//
//
//
void GENRM( void )
{
                JSR     (MOVE);             // MAKE MOVE
/*GENR2:*/      JSR     (REVERSE);          // REVERSE BOARD
                JSR     (GNM);              // GENERATE MOVES
                JMP     (RUM);              // fall through
}

void RUM( void )
{
                JSR     (REVERSE);          // REVERSE BACK
                JMP     (UMOVE);            // fall through
}

//
//       ROUTINE TO UNMAKE A MOVE MADE BY
//         MOVE
//
void UMOVE( void )
{
                TSX;                        // UNMAKE MOVE
                STX     (SP1);
                LDX     (SP2);              // EXCHANGE
                TXS;                        // STACKS
                PLA;                        // MOVEN
                STA     (MOVEN);
                PLA;                        // CAPTURED
                STA     (PIECE);            // PIECE
                TAX;
                PLA;                        // FROM SQUARE
                STAx    (BOARD,X);
                PLA;                        // PIECE
                TAX;
                PLA;                        // TO SOUARE
                STA     (SQUARE);
                STAx    (BOARD,X);
                JMP     (STRV);
}

//
//       THIS ROUTINE MOVES PIECE
//       TO SQUARE, PARAMETERS
//       ARE SAVED IN A STACK TO UNMAKE
//       THE MOVE LATER
//
void MOVE( void )
{               TSX;
                STX     (SP1);              // SWITCH
                LDX     (SP2);              // STACKS
                TXS;
                LDA     (SQUARE);
                PHA;                        // TO SQUARE
                TAY;
                LDXi    (0x1F);
CHECK:          CMPx    (BOARD,X);          // CHECK FOR
                BEQ     (TAKE);             // CAPTURE
                DEX;
                BPL     (CHECK);
TAKE:           LDAi    (0xCC);
                STAx    (BOARD,X);
                TXA;                        // CAPTURED
                PHA;                        // PIECE
                LDX     (PIECE);
                LDAx    (BOARD,X);
                STYx    (BOARD,X);          // FROM
                PHA;                        // SQUARE
                TXA;
                PHA;                        // PIECE
                LDA     (MOVEN);
                PHA;                        // MOVEN
                JMP     (STRV);             // fall through
}

// (WRF) Fortunately when we swap stacks we jump here and swap back before
//  returning. So we aren't swapping stacks to do threading (if we were we
//  would need to enhance 6502 stack emulation to incorporate our
//  subroutine mechanism, instead we simply use the native C stack for
//  subroutine return addresses).
void STRV( void )
{
                TSX;
                STX     (SP2);              // SWITCH
                LDX     (SP1);              // STACKS
                TXS;                        // BACK
                RTS;
}

//
//       CONTINUATION OF SUB STRATGY
//       -CHECKS FOR CHECK OR CHECKMATE
//       AND ASSIGNS VALUE TO MOVE
//
void CKMATE( void )
{
                LDX     (BMAXC);            // CAN BLK CAP
                CPXf    (POINTS,0);         // MY KING?
                BNE     (NOCHEK);
                LDAi    (0x00);             // GULP!
                BEQ     (RETV);             // DUMB MOVE!
//
NOCHEK:         LDX     (BMOB);             // IS BLACK
                BNE     (RETV);             // UNABLE TO
                LDX     (WMAXP);            // MOVE AND
                BNE     (RETV);             // KING IN CH?
                LDAi    (0xFF);             // YES! MATE
//
RETV:           LDXi    (0x04);             // RESTORE
                STX     (STATE);            // STATE=4
//
//       THE VALUE OF THE MOVE (IN ACCU)
//       IS COMPARED TO THE BEST MOVE AND
//       REPLACES IT IF IT IS BETTER
//
                if( bool_show_move_evaluation )
                    show_move_evaluation( reg_a );
/*PUSH:*/       CMP     (BESTV);            // IS THIS BEST
                BCC     (RETP);             // MOVE SO FAR?
                BEQ     (RETP);
                if( bool_show_move_evaluation )
                    printf( "NEW BEST MOVE\n" );
                STA     (BESTV);            // YES!
                LDA     (PIECE);            // SAVE IT
                STA     (BESTP);
                LDA     (SQUARE);
                STA     (BESTM);            // FLASH DISPLAY
RETP:           LDAi    ('.');              // print ... instead of flashing disp
                JMP     (syschout);         // print . and return
}

//
//       MAIN PROGRAM TO PLAY CHESS
//       PLAY FROM OPENING OR THINK
//
void GO( void )
{
                LDX     (OMOVE);            // OPENING?
                BMI     (NOOPEN);           // -NO   *ADD CHANGE FROM BPL
                LDA     (DIS3);             // -YES WAS
                CMPf    (OPNING,X);         // OPPONENT'S
                BNE     (END);              // MOVE OK?
                DEX;
                LDAf    (OPNING,X);         // GET NEXT
                STA     (DIS1);             // CANNED
                DEX;                        // OPENING MOVE
                LDAf    (OPNING,X);
                STA     (DIS3);             // DISPLAY IT
                DEX;
                STX     (OMOVE);            // MOVE IT
                BNE     (MV2);              // (JMP)
//
END:            LDAi    (0xFF);             // *ADD - STOP CANNED MOVES
                STA     (OMOVE);            // FLAG OPENING
NOOPEN:         LDXi    (0x0C);             // FINISHED
                STX     (STATE);            // STATE=C
                STX     (BESTV);            // CLEAR BESTV
                LDXi    (0x14);             // GENERATE P
                JSR     (GNMX);             // MOVES
//
                LDXi    (0x04);             // STATE=4
                STX     (STATE);            // GENERATE AND
                JSR     (GNMZ);             // TEST AVAILABLE
//                                             MOVES
//
                LDX     (BESTV);            // GET BEST MOVE
                CPXi    (0x0F);             // IF NONE
                BCC     (MATE);             // OH OH!
//
MV2:            LDX     (BESTP);            // MOVE
                LDAx    (BOARD,X);          // THE
                STA     (BESTV);            // BEST
                STX     (PIECE);            // MOVE
                LDA     (BESTM);
                STA     (SQUARE);           // AND DISPLAY
                JSR     (MOVE);             // IT
                JMP     (RESTART_CHESS);
//
MATE:           LDAi    (0xFF);             // RESIGN
                RTS;                        // OR STALEMATE
}

//
//       SUBROUTINE TO ENTER THE
//       PLAYER'S MOVE
//
void DISMV( void )
{
                LDXi    (0x04);             // ROTATE
DROL:           ASL     (DIS3);             // KEY
                ROL     (DIS2);             // INTO
                DEX;                        // DISPLAY
                BNE     (DROL);             //
                ORA     (DIS3);
                STA     (DIS3);
                STA     (SQUARE);
                RTS;
}

//
//       THE FOLLOWING SUBROUTINE ASSIGNS
//       A VALUE TO THE MOVE UNDER
//       CONSIDERATION AND RETURNS IT IN
//       THE ACCUMULATOR
//
void STRATGY( void )
{
                CLC;
                LDAi    (0x80);
                ADC     (WMOB);             // PARAMETERS
                ADC     (WMAXC);            // WITH WEIGHT
                ADC     (WCC);              // OF O.25
                ADC     (WCAP1);
                ADC     (WCAP2);
                SEC;
                SBC     (PMAXC);
                SBC     (PCC);
                SBC     (BCAP0);
                SBC     (BCAP1);
                SBC     (BCAP2);
                SBC     (PMOB);
                SBC     (BMOB);
                BCS     (POS);              // UNDERFLOW
                LDAi    (0x00);             // PREVENTION
POS:            LSR;
                CLC;                        // **************
                ADCi    (0x40);
                ADC     (WMAXC);            // PARAMETERS
                ADC     (WCC);              // WITH WEIGHT
                SEC;                        // OF 0.5
                SBC     (BMAXC);
                LSR;                        // **************
                CLC;
                ADCi    (0x90);
                ADC     (WCAP0);            // PARAMETERS
                ADC     (WCAP0);            // WITH WEIGHT
                ADC     (WCAP0);            // OF 1.0
                ADC     (WCAP0);
                ADC     (WCAP1);
                SEC;                        // [UNDER OR OVER-
                SBC     (BMAXC);            // FLOW MAY OCCUR
                SBC     (BMAXC);            // FROM THIS
                SBC     (BMCC);             // SECTION]
                SBC     (BMCC);
                SBC     (BCAP1);
                LDX     (SQUARE);           // ***************
                CPXi    (0x33);
                BEQ     (POSN);             // POSITION
                CPXi    (0x34);             // BONUS FOR
                BEQ     (POSN);             // MOVE TO
                CPXi    (0x22);             // CENTRE
                BEQ     (POSN);             // OR
                CPXi    (0x25);             // OUT OF
                BEQ     (POSN);             // BACK RANK
                LDX     (PIECE);
                BEQ     (NOPOSN);
                LDYx    (BOARD,X);
                CPYi    (0x10);
                BPL     (NOPOSN);
POSN:           CLC;
                ADCi    (0x02);
NOPOSN:         JMP     (CKMATE);           // CONTINUE
}

//**********************************************************************
//*
//*  Part 3
//*  ------
//*  Text based interface over RS-232 port added later.
//*
//*       Part 3 added August 2002 by Daryl Richtor
//**********************************************************************

//-----------------------------------------------------------------
// The following routines were added to allow text-based board
// display over a standard RS-232 port.
//

// (WRF) For C version, data definitions precede code references to data
char banner[] = "MicroChess (c) 1976-2005 Peter Jennings, www.benlo.com\r\n";
char cpl[]    = "WWWWWWWWWWWWWWWWBBBBBBBBBBBBBBBBWWWWWWWWWWWWWWWW";
char cph[]    = "KQRRBBNNPPPPPPPPKQRRBBNNPPPPPPPP";

void POUT( void )
{               JSR     (POUT9);            // print CRLF
                JSR     (POUT13);           // print copyright
                JSR     (POUT10);           // print column labels
                LDYi    (0x00);             // init board location
                JSR     (POUT5);            // print board horz edge
POUT1:          LDAi    ('|');              // print vert edge
                JSR     (syschout);         // PRINT ONE ASCII CHR - SPACE
                LDXi    (0x1F);
POUT2:          TYA;                        // scan the pieces for a location match
                CMPx    (BOARD,X);          // match found?
                BEQ     (POUT4);            // yes, print the piece's color and type
                DEX;                        // no
                BPL     (POUT2);            // if not the last piece, try again
                TYA;                        // empty square
                ANDi    (0x01);             // odd or even column?
                STA     (temp);             // save it
                TYA;                        // is the row odd or even
                LSR;                        // shift column right 4 spaces
                LSR;                        //
                LSR;                        //
                LSR;                        //
                ANDi    (0x01);             // strip LSB
                CLC;                        //
                ADC     (temp);             // combine row & col to determine square color
                ANDi    (0x01);             // is board square white or blk?
                BEQ     (POUT25);           // white, print space
                LDAi    ('*');              // black, print *
                BRA     (POUT99);           // DB      0x2c             // used to skip over LDA #0x20
POUT25:         LDAi    (' ');              // ASCII space
POUT99:         JSR     (syschout);         // PRINT ONE ASCII CHR - SPACE
                JSR     (syschout);         // PRINT ONE ASCII CHR - SPACE
POUT3:          INY;                        //
                TYA;                        // get row number
                ANDi    (0x08);             // have we completed the row?
                BEQ     (POUT1);            // no, do next column
                LDAi    ('|');              // yes, put the right edge on
                JSR     (syschout);         // PRINT ONE ASCII CHR - |
                JSR     (POUT12);           // print row number
                JSR     (POUT9);            // print CRLF
                JSR     (POUT5);            // print bottom edge of board
                CLC;                        //
                TYA;                        //
                ADCi    (0x08);             // point y to beginning of next row
                TAY;                        //
                CPYi    (0x80);             // was that the last row?
                JEQ     (POUT8);            // (BEQ) yes, print the LED values
                BNE     (POUT1);            // no, do new row

POUT4:          LDA     (REV);              // print piece's color & type
                BEQ     (POUT41);           //
                LDAf    (cpl+16,X);         //
                BNE     (POUT42);           //
POUT41:         LDAf    (cpl,X);            //
POUT42:         JSR     (syschout);         //
                LDAf    (cph,X);            //
                JSR     (syschout);         //
                BNE     (POUT3);            // branch always
}

void POUT5( void )
{
                TXA;                        // print "-----...-----<crlf>"
                PHA;
                LDXi    (0x19);
                LDAi    ('-');
POUT6:          JSR     (syschout);         // PRINT ONE ASCII CHR - "-"
                DEX;
                BNE     (POUT6);
                PLA;
                TAX;
                JSR     (POUT9);
                RTS;
}

void POUT8( void )
{
                JSR     (POUT10);           //
                LDA     (0xFB);
                JSR     (syshexout);        // PRINT 1 BYTE AS 2 HEX CHRS
                LDAi    (0x20);
                JSR     (syschout);         // PRINT ONE ASCII CHR - SPACE
                LDA     (0xFA);
                JSR     (syshexout);        // PRINT 1 BYTE AS 2 HEX CHRS
                LDAi    (0x20);
                JSR     (syschout);         // PRINT ONE ASCII CHR - SPACE
                LDA     (0xF9);
                JSR     (syshexout);        // PRINT 1 BYTE AS 2 HEX CHRS
                JMP     (POUT9);            // fall through
}

void POUT9( void )
{
                LDAi    (0x0D);
                JSR     (syschout);         // PRINT ONE ASCII CHR - CR
                LDAi    (0x0A);
                JSR     (syschout);         // PRINT ONE ASCII CHR - LF
                RTS;
}

void POUT10( void )
{
                LDXi    (0x00);             // print the column labels
POUT11:         LDAi    (0x20);             // 00 01 02 03 ... 07 <CRLF>
                JSR     (syschout);
                TXA;
                JSR     (syshexout);
                INX;
                CPXi    (0x08);
                BNE     (POUT11);
                JEQ     (POUT9);
                JMP     (POUT12);           // fall through
}

void POUT12( void )
{
                TYA;
                ANDi    (0x70);
                JSR     (syshexout);
                RTS;
}

void POUT13( void )
{
                LDXi    (0x00);             // Print the copyright banner
POUT14:         LDAf    (banner,X);
                BEQ     (POUT15);
                JSR     (syschout);
                INX;
                BNE     (POUT14);
POUT15:         RTS;
}

void KIN( void )
{
                LDAi    ('?');
                JSR     (syschout);         // PRINT ONE ASCII CHR - ?
                JSR     (syskin);           // GET A KEYSTROKE FROM SYSTEM
                ANDi    (0x4F);             // MASK 0-7, AND ALPHA'S
                RTS;
}

//
// 6551 I/O Support Routines
//
//
/*
Init_6551      lda   #0x1F                  // 19.2K/8/1
               sta   ACIActl                // control reg
               lda   #0x0B                  // N parity/echo off/rx int off/ dtr active low
               sta   ACIAcmd                // command reg
               rts                          // done
*/

// (WRF) See enhanced syskin() routine in Part 4
// input chr from ACIA1 (waiting)
//
/*
syskin         lda   ACIASta                // Serial port status
               and   #0x08                  // is recvr full
               beq   syskin                 // no character to get
               Lda   ACIAdat                // get chr
               RTS                          //
*/

// (WRF) See enhanced syschout() routine in Part 4
// output to OutPut Port
//
/*
syschout       PHA                          // save registers
ACIA_Out1      lda   ACIASta                // serial port status
               and   #0x10                  // is tx buffer empty
               beq   ACIA_Out1              // no
               PLA                          // get chr
               sta   ACIAdat                // put character to Port
               RTS                          // done
*/

// Print as hex digits
void syshexout( void )
{
               PHA;                         //  prints AA hex digits
               LSR;                         //  MOVE UPPER NIBBLE TO LOWER
               LSR;                         //
               LSR;                         //
               LSR;                         //
               JSR   (PrintDig);            //
               PLA;                         //
               JMP   (PrintDig);            // fall through
}

void PrintDig( void )
{
               char Hexdigdata[] = "0123456789ABCDEF";
               ANDi  (0x0F);                //  prints A hex nibble (low 4 bits)
               PHY;                         //
               TAY;                         //
               LDAf  (Hexdigdata,Y);        //
               PLY;                         //
               JMP   (syschout);            //
}

//**********************************************************************
//*
//*  Part 4
//*  ------
//*  Enhance text interface by creating 'smart' syskin(), syschout()
//*  routines.
//*
//*       Part 4 added July 2005 by Bill Forster (www.triplehappy.com)
//**********************************************************************

// Misc prototypes
static char algebraic_file( byte square );
static char algebraic_rank( byte square );
static char octal_file( char file );
static char octal_rank( char rank );

// Here are the commands available in the enhanced interface
static char help[] =
    "Commands are;\n"
    " w      ;start game playing white against microchess playing black\n"
    " b      ;start game playing black against microchess playing white\n"
    " nnnn   ;(eg 6343 = P-K4) specify move with microchess numeric grid\n"
    " anan   ;(eg e7e5 = black P-K4, e2e4 = white P-K4) specify move with\n"
    "        ; algebraic notation\n"
    " oo     ;castle king side\n"
    " ooo    ;castle queen side\n"
    " f      ;play move specified\n"
    " p      ;make program play move\n"
    " a      ;toggle autoplay (autoplay inserts \'p\' and \'f\' commands)\n"
    " c      ;clear board\n"
    " e      ;exchange (reverse) board\n"
    " ln     ;set level, n=1 (weakest), 2 (medium), 3 (strongest)\n"
    " hh     ;piece editor, view piece location, eg 01, computer's queen\n"
    " hh=xx  ;piece editor, set piece location, eg 01=64 or 01=e2\n"
    " hh=    ;piece editor, clear piece, eg 01=, delete computer's queen\n"
    " m      ;debugging, toggle move generation information dump\n"
    " v      ;debugging, toggle move evaluation information dump\n"
    "        ;Note that the debugging features are very verbose and best\n"
    "        ; used with file redirection (especially move generation)\n"
    " q      ;quit\n"
    "?";

// Alternatively, allow a primitive interface for DOS/Windows systems
//  that closely simulates the RS-232 interface originally envisaged
//  for part 3
// #define PRIMITIVE_INTERFACE  // normally commented out
#ifdef PRIMITIVE_INTERFACE
#include <conio.h>      // these routines emulate dumb terminal in/out
#endif

// Forward declaration of smart in/out alternatives
void smart_out( char c );
char smart_in( void );

// Character in
void syskin( void )
{
    #ifdef PRIMITIVE_INTERFACE
    reg_a = (byte)getch();
    #else
    reg_a = (byte)smart_in();
    #endif
}

// Character out
void syschout( void )
{
    #ifdef PRIMITIVE_INTERFACE
    putch( (int)reg_a );
    #else
    smart_out( (char)reg_a );
    #endif
}

// Smart character out, supplements enhanced interface of smart_in()
static int discard = 1194;  // Discard until initial CLEAR and EXCHANGE
                            //  commands are complete
    // The discard mechanism optionally discards characters - this is
    //  useful because the smart_in() routine works by converting higher
    //  level commands into a a series of primitive commands. The discard
    //  mechanism allows us to skip over (discard) the board displays
    //  generated for all of the intermediate primitive commands.
    //  The values assigned to discard are in each case determined by
    //  simple trial and error
void smart_out( char c )
{
    static char first='?';  // replace first '?' with message below
    if( discard )
        discard--;
    else
    {
        if( first && c==first )
        {
            printf( " (type ? for help)\n?" );
            first = '\0';
        }
        else
        {
            if( c != '\r' )//printf converts "\n" to "\r\n" so don't need "\r"
                printf( "%c", c );
        }
    }
}

// Smart character in, provides a help screen + algebraic notation interface
//  + position editor + diagnostics commands etc.
char smart_in( void )
{
    static char error[] = "Illegal or unknown command, type ? for help\n?";
    static char buf[20] = " CE";  // start with a CLEAR then EXCHANGE command
    static int offset=1;
    static int bool_auto=1;
    char color, file, rank, file2, rank2, ch='\0';
    int i, len, bool_okay;
    byte piece, square;
    char *s;

    // Get orientation; is human player white or black ?
    byte bool_white = ZP(REV);

    // Emit buffered commands until '\0'
    if( offset )
        ch = buf[offset++];

    // Loop until command ready
    while( ch == '\0' )
    {

        // Reset grooming machinery
        offset  = 0;
        discard = 2;    // remove initial "\r\n"

        // Reset flag indicating entry of a legal command handled internally
        //  (i.e. within this function, without passing characters to
        //  underlying microchess implementation)
        bool_okay = 0;

        // Get edited command line
        if( NULL == fgets(buf,sizeof(buf)-1,stdin) )
            EXIT_TO_SYSTEM();
        else
        {

            // Convert to lower case, zap '\n'
            for( s=buf; *s; s++ )
            {
                if( isascii(*s) && isupper(*s) )
                    *s = tolower(*s);
                else if( *s == '\n' )
                    *s = '\0';
            }

            // Trim string from end
            s = strchr(buf,'\0') - 1;
            while( s>=buf && (*s==' '||*s=='\t') )
                *s-- = '\0';

            // Trim string from start
            s = buf;
            while( *s==' ' || *s=='\t' )
                s++;
            len = strlen(s);
            for( i=0; i<len+1; i++ )  // +1 gets '\0' at end
                buf[i] = *s++;  // if no leading space this does
                                //  nothing, but no harm either

            // Convert an algebraic move eg "e2e4" into an octal microchess
            //  move
            if( len == 4 &&
                'a'<=buf[0] && buf[0]<='h' &&
                '1'<=buf[1] && buf[1]<='8' &&
                'a'<=buf[2] && buf[2]<='h' &&
                '1'<=buf[3] && buf[3]<='8'
              )
            {
                file  = octal_file(buf[0]);
                rank  = octal_rank(buf[1]);
                file2 = octal_file(buf[2]);
                rank2 = octal_rank(buf[3]);
                buf[0] = rank;     // specify move microchess grid style
                buf[1] = file;     //
                buf[2] = rank2;    //
                buf[3] = file2;    //
            }

            // Is it a microchess octal numeric move eg "6364" ?
            if( len == 4 &&
                '0'<=buf[0] && buf[0]<='7' &&
                '0'<=buf[1] && buf[1]<='7' &&
                '0'<=buf[2] && buf[2]<='7' &&
                '0'<=buf[3] && buf[3]<='7'
              )
            {
                offset = 1;         // emit from here next
                if( bool_auto )
                {
                    buf[4] = '\r';   // play move
                    buf[5] = 'p';    // get response
                    buf[6] = '\0';   // done
                    discard = 2386;  // skip over intermediate board displays
                }
                else
                {
                    buf[4] = '\0';   // done
                    discard = 1790;  // skip over intermediate board displays
                }
            }

            // Is it a level command ?
            else if( len==2 &&
                (buf[0]=='l' && '1'<=buf[1] && buf[1]<='3' )
              )
            {
                bool_okay = 1;
                switch( buf[1] )
                {
                    case '1':   level1 = 0;
                                level2 = 0xff;
                                printf( "Level 1, super blitz\n" );
                                break;  // (on 6502: 3 seconds per move)
                    case '2':   level1 = 0;
                                level2 = 0xfb;
                                printf( "Level 2, blitz\n" );
                                break;  // (on 6502: 10 seconds per move)
                    case '3':   level1 = 8;
                                level2 = 0xfb;
                                printf( "Level 3, normal\n" );
                                break;  // (on 6502: 100 seconds per move)
                }
            }

            // Is it a single letter command ?
            else if( len == 1 )
            {
                switch( buf[0] )
                {

                    // Send single letter commands to underlying microchess
                    //  (step 3) interface
                    case 'c':   ch = 'C';   break;
                    case 'e':   ch = 'E';   break;
                    case 'p':   ch = 'P';   discard=0;  // no initial "\r\n"
                                            break;
                    case 'q':   ch = 'Q';   break;
                    case 'f':   ch = '\r';  break;

                    // Toggle various features
                    case 'a':
                    {
                        bool_okay = 1;
                        bool_auto = !bool_auto;
                        printf( "Auto play now %s\n",
                                            bool_auto ? "enabled"
                                                      : "disabled" );
                        break;
                    }
                    case 'm':
                    {
                        bool_okay = 1;
                        bool_show_move_generation = !bool_show_move_generation;
                        printf( "Show move generation now %s\n",
                                 bool_show_move_generation ? "enabled"
                                                           : "disabled" );
                        break;
                    }
                    case 'v':
                    {
                        bool_okay = 1;
                        bool_show_move_evaluation = !bool_show_move_evaluation;
                        printf( "Show move evaluation now %s\n",
                                 bool_show_move_evaluation ? "enabled"
                                                           : "disabled" );
                        break;
                    }

                    // Start a white game by emitting "clear" and "reverse"
                    //  commands. Make sure we set to "black" orientation
                    //  before clear command else we get a nasty mirror
                    //  image chess board
                    case 'w':
                    {
                        strcpy( buf, bool_white?"ece":"ce" );
                        discard = bool_white?1194:598;
                        offset = 1; // emit from here next
                        break;
                    }

                    // Start a black game by emitting ["reverse"], "clear",
                    //  and "play" commands. Make sure we set to "black"
                    //  orientation before clear command else we get a
                    //  nasty mirror image chess board
                    case 'b':
                    {
                        strcpy( buf, bool_white?"ecp":"cp" );
                        discard = bool_white?1194:598;
                        offset = 1; // emit from here next
                        break;
                    }
                }
            }

            // Algebraic castling - emit as two half moves
            else if( 0==strcmp(buf,"oo") || 0==strcmp(buf,"ooo") )
            {
                if( bool_auto )
                {
                    if( 0 == strcmp(buf,"oo") )
                        strcpy( buf, bool_white ? "7476\r7775\rp"
                                                : "7371\r7072\rp" );
                    else
                        strcpy( buf, bool_white ? "7472\r7073\rp"
                                                : "7375\r7774\rp" );
                    offset = 1;
                    discard = 5422; // skip intermediate boards
                }
                else
                {
                    printf( "Castling only available in auto play mode"
                            " (use \'a\' command)\n" );
                    bool_okay = 1;
                }
            }

            // Piece editor ?
            else if( len>=2 && '0'<=buf[0] && buf[0]<='1' &&
                               isascii(buf[1]) && isxdigit(buf[1]) )
            {
                bool_okay = 0; // assume syntax is bad unless proven otherwise
                if( len == 2 )
                    bool_okay = 1;   // view
                else if( len==3 && buf[2]=='=' )
                    bool_okay = 1;   // delete
                else if( len==5 && buf[2]=='=' &&
                        ( '0'<=buf[3] && buf[3]<='7' &&
                          '0'<=buf[4] && buf[4]<='7' )
                  )
                {
                    bool_okay = 1;   // octal edit
                    file = buf[4];
                    rank = buf[3];
                }
                else if( len==5 && buf[2]=='=' &&
                        ( 'a'<=buf[3] && buf[3]<='h' &&
                          '1'<=buf[4] && buf[4]<='8' )
                  )
                {
                    bool_okay = 1;   // algebraic edit
                    file = octal_file(buf[3]);
                    rank = octal_rank(buf[4]);
                }

                // If piece editor command with correct syntax
                if( bool_okay )
                {

                    // First two characters are hex 00-1f indicating one of
                    //  the 32 pieces
                    piece = buf[1]>='a' ? buf[1]-'a'+10 : buf[1]-'0';
                    if( buf[0] == '1' )
                        piece += 16;
                    square = ZP(BOARD+piece); // square our piece is occupying

                    // If edit place our piece on specified square, after
                    //  making sure no other piece is on that square
                    if( len == 5 )  // edit ?
                    {
                        square = (rank-'0')*16 + (file-'0');
                        for( i=0; i<32; i++ )
                        {
                            if( ZP(BOARD+i) == square ) //delete other piece?
                                ZP(BOARD+i)= 0xcc; // microchess convention
                        }
                        ZP(BOARD+piece) = square;
                    }

                    // If delete assign special illegal square value to piece
                    else if( len == 3 )
                        ZP(BOARD+piece) = 0xcc; // microchess convention

                    // Report on the color and type of piece ...
                    if( piece < 16 )
                        color = bool_white?'B':'W';
                    else
                        color = bool_white?'W':'B';
                    printf( "Piece %c%c is %s %c%c ", buf[0], buf[1],
                                        (piece&0x0f) < 2 ? "the" : "a",
                                        color,
                                        "KQRRBBNNPPPPPPPP"[piece&0x0f] );

                    // ... and the square it (now) occupies
                    if( square & 0x88 )
                        printf( "and is not on the board\n" );
                    else
                    {
                        printf( "%son square %02x",
                                           len==3?"previously ":"",
                                           square );
                        printf( " (algebraic %c%c)", algebraic_file(square),
                                                     algebraic_rank(square) );
                        if( len == 3 )
                            printf( " now deleted" );
                        printf("\n");
                    }
                    POUT();
                }
            }

            // Emit the first of a buffered series of commands ?
            if( offset )
                ch = buf[0];

            // If still no command available, illegal or unknown command
            if( ch == '\0' )
            {
                if( len==0 || bool_okay ) // if bool_okay internal command 
                    printf( "?" );
                else if( buf[0] == '?' )
                    printf( help );
                else
                    printf( error );
            }
        }
    }
    return( ch );
}


// Show internally generated move
void show_move_generation( byte src, byte dst )
{
    static byte lookup[64] =
    {
        0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
        0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
        0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,
        0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
        0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,
        0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,
        0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,
        0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77
    };
    static char spaces[]=
        "                                                           ";
    int i, indent;
    char ch;
    byte square, piece;

    // Indent according to state
    printf( "\n" );
    if( ZP(STATE) >= 0xf5 )
        indent = (ZP(STATE)-0xf5)*4;
    else
        indent = ZP(STATE);
    printf( strchr(spaces,'\0') - indent );

    // Print two characters for each square
    for( i=0; i<64; i++ )
    {
        square = lookup[i];
        ch = ' ';  // empty by default
        for( piece=0; piece<32; piece++ ) // unless we find a piece
        {
            if( ZP(BOARD+piece) == square )
            {
                ch = "KQRRBBNNPPPPPPPPkqrrbbnnpppppppp"[piece];
                break;
            }
        }
        printf( "%c", ch );
        if( square == src )
            printf( "*" );  // highlight src square like this
        else if( square == dst )
            printf( "@" );  // highlight dst square like this
        else
            printf( " " );  // normally no hightlight

        // Next row
        if( (i&7) == 7 )
        {
            printf( "\n" );
            printf( strchr(spaces,'\0') - indent );
        }
    }

    // Also show the most important debug variable information
    printf( "state=%02x ", ZP(STATE) );
}


// Show numeric move evaluation
static void show_move_evaluation( int ivalue )
{

    // Compare ivalue calculated by microchess with independently calculated
    //  float value, then float value scaled into same range as ivalue
    double value;
    int svalue;

    // Counters
    byte wcap0 = ZP(WCAP0);
    byte wcap1 = ZP(WCAP1);
    byte wmaxc = ZP(WMAXC);
    byte wcc   = ZP(WCC  );
    byte wmob  = ZP(WMOB );
    byte wcap2 = ZP(WCAP2);
    byte bmaxc = ZP(BMAXC);
    byte bcc   = ZP(BMCC );
    byte bcap1 = ZP(BCAP1);
    byte pmaxc = ZP(PMAXC);
    byte pcc   = ZP(PCC  );
    byte pmob  = ZP(PMOB );
    byte bcap0 = ZP(BCAP0);
    byte bcap2 = ZP(BCAP2);
    byte bmob  = ZP(BMOB );

    // Show move
    printf( "\nEvaluating move %c-%c%c\n",
                             "KQRRBBNNpppppppp"[ZP(PIECE)&0x0f],
                             algebraic_file(ZP(SQUARE)),
                             algebraic_rank(ZP(SQUARE)) );

    // Calculate weighted sum
    value =   4.00 * (wcap0)
            + 1.25 * (wcap1)
            + 0.75 * (wmaxc + wcc)
            + 0.25 * (wmob + wcap2)
            - 2.50 * (bmaxc)
            - 2.00 * (bcc)
            - 1.25 * (bcap1)
            - 0.25 * (pmaxc + pcc + pmob + bcap0 + bcap2 + bmob);
    printf( "(+4)    WCAP0=%u\n", wcap0 );
    printf( "(+1.25) WCAP1=%u\n", wcap1 );
    printf( "(+0.75) WMAXC=%u WCC=%u\n", wmaxc, wcc );
    printf( "(+0.25) WMOB =%u WCAP2=%u\n", wmob, wcap2  );
    printf( "(-2.50) BMAXC=%u\n", bmaxc );
    printf( "(-2.00) BCC  =%u\n", bcc   );
    printf( "(-1.25) BCAP1=%u\n", bcap1 );
    printf( "(-0.25) PMAXC=%u PCC=%u PMOB=%u BCAP0=%u BCAP2=%u BMOB=%u\n",
                     pmaxc, pcc, pmob, bcap0, bcap2, bmob  );
    printf( "Weighted sum        = %f\n", value  );

    // Calculate scaled weighted sum, corresponds to single byte value used
    //  internally by microchess
    svalue = (int)floor(208.0 + value);  // 208 = 0x90+0x40 from STRATGY();
    printf( "Scaled weighted sum = %d\n", svalue );

    // Comment on correspondence (or otherwise) of two values
    printf( "Move value = %d"  , ivalue );
    if( ivalue == 0 )
        printf( " (minimum, I'm in check?)\n", ivalue );
    else if( ivalue == 255 )
        printf( " (maximum, I'm delivering mate?)\n", ivalue );
    else if( ivalue == svalue )
        printf( " (=scaled weighted sum)\n" );
    else if( ivalue == svalue+2 )
        printf( " (=scaled weighted sum plus 2 bonus points)\n" );
    else
        printf( " (unexpected value, suspect overflow or underflow)\n" );
    printf( "best so far = %u\n", ZP(BESTV) );
}


// Get algebraic file 'a'-'h' from octal square
static char algebraic_file( byte square )
{
    char file = square & 0x0f;
    byte bool_white = ZP(REV);
    if( bool_white )
        file = 'a' +  file;     // eg 0->'a', 7->'h'
    else //if( black )
        file = 'a' + (7-file);  // eg 7->'a', 0->'h'
    return( file );
}


// Get algebraic rank '1'-'8' from octal square
static char algebraic_rank( byte square )
{
    char rank = (square>>4) & 0x0f;
    byte bool_white = ZP(REV);
    if( bool_white )
        rank = '1' + (7-rank);  // eg 7->'1', 0->'8'
    else //if( black )
        rank = '1' + rank;      // eg 0->'1', 7->'8'
    return( rank );
}


// Get microchess file '0'-'7' from algebraic file 'a'-'h'
static char octal_file( char file )
{
    byte bool_white = ZP(REV);
    if( bool_white )
        file = '0' + (file-'a');  // eg 'a'->'0', 'h'->'7'
    else //if( black )
        file = '7' - (file-'a');  // eg 'a'->'7', 'h'->'0'
    return( file );
}


// Get microchess rank '0'-'7' from algebraic rank '1'-'8'
static char octal_rank( char rank )
{
    byte bool_white = ZP(REV);
    if( bool_white )
        rank = '7' - (rank-'1');  // eg '1'->'7', '8'->'0'
    else //if( black )
        rank = '0' + (rank-'1');  // eg '1'->'0', '8'->'7'
    return( rank );
}
