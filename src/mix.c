/*
 * Copyright 2009-2022 Adrià Giménez Pastor.
 *
 * This file is part of adriagipas/MIX.
 *
 * adriagipas/MIX is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * adriagipas/MIX is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with adriagipas/MIX.  If not, see <https://www.gnu.org/licenses/>.
 */
/*
 *  mix.c - Implementació de 'MIX.h'.
 *
 */


#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MIX.h"




/**********/
/* MACROS */
/**********/

/* Auxiliars. */

#define NMASK 0x80000000

#define INMASK 0x3FFFFFFF

#define IMASK 0x80000FFF

#define READ_F ((_vars.inst>>6)&0x3F)

#define READ_I ((_vars.inst>>12)&0x3F)

#define IS_NEG(DATA) ((DATA)&NMASK)

#define CALC_ADDR(DATA,VAR)               \
  (VAR)= ((DATA)>>18)&0xFFF;           \
  if ( IS_NEG ( DATA ) ) (VAR)= -(VAR)

#define GET_DATA _mem[_vars.M]

#define LDI (ld()&IMASK)

#define LDN (ld()^NMASK)

#define LDIN (LDN&IMASK)

#define WORDTOS32(WORD)          \
  if ( IS_NEG ( WORD ) )         \
    (WORD)= -((WORD)&INMASK)

#define CALC_OP2(OP2)        \
  calc_LR ();        	\
  calc_M ();                \
  (OP2)= (MIXs32) ld ()

#define CALC_OP1_OP2(OP1,OP2) \
  CALC_OP2 ( OP2 );              \
  WORDTOS32 ( OP2 );              \
  (OP1)= (MIXs32) _regs.A;    \
  WORDTOS32 ( (OP1) )

#define ADD_(OP,OP1,OP2)              \
  CALC_OP1_OP2 ( OP1, OP2 );              \
  (OP1) OP (OP2);        	      \
  _regs.A= add_aux ( _regs.A, (OP1) )

#define ABS(S32) ((S32)&INMASK)

#define MOPI(REG) (mop ( (REG) )&IMASK)


#define CHECK_DEV_BASE(DEV,BASE)        	     \
  if ( (DEV) > 20 )                                  \
    {                                                \
      _warning ( _udata,                             \
        	 "número de dispositiu invàlid: %d", \
        	 (DEV) );			     \
      BASE;                                          \
    }


#define CHECK_DEV(DEV) CHECK_DEV_BASE ( DEV, return )




/*********/
/* ESTAT */
/*********/

/* Registres. El bit més alt s'empra per al signe, els 30 bits amb
   menys pes per als bytes. El byte 5 es correspon amb els 6 bts de
   menys pes. */
static struct
{
  
  MIXu32 A;
  MIXu32 X;
  MIXu32 I[6];
  MIXu32 J;
  int  PC;
  int  old_PC;
  
} _regs;


/* Variables auxiliars. */
static struct
{
  
  MIXu32 inst;
  int    L;
  int    R;
  int    M;

} _vars;


/* Memòria. */
static MIXu32 _mem[4000];


/* Inidicadors d'estat. */
static enum {
  OFF= 0,
  ON
} _overflow;
static enum {
  EQUAL= 0,
  LESS,
  GREATER
} _cmp;


/* Avísos. */
static MIX_Warning *_warning;


/* I/O. */
static MIX_InitIOOPChar *_init_ioopchar;
static MIX_InitIOOPWord *_init_ioopword;
static MIX_DeviceBusy *_device_busy;
static MIX_IOControl *_io_control;
static MIX_NotifyWaitingDevice *_notify_waiting_device;


/* Dades d'usuari. */
static void *_udata;


/* Tractament de senyals. */
static MIX_CheckSignals *_check;


// Controla l'estat del simulador.
static struct
{
  enum
    {
     RUNNING,
     HALT,
     WAIT_DEVICE,
     RUNNING_GO_STEP0,
     RUNNING_GO_STEP1
    } v;
  int dev; // Utilitzat amb WAIT_DEVICE
  bool notify_cr;
} _run_state;




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static void
calc_LR ()
{
  
  int F;
  
  
  F= READ_F;
  _vars.L= F>>3;
  _vars.R= F&0x7;
  if ( _vars.L > 5 || _vars.R > 5 || _vars.L > _vars.R )
    {
      _warning ( _udata,
        	 "valor de F invàlid: 8*L[%d]+R[%d] = F[%d]",
        	 _vars.L, _vars.R, F );
      _vars.L= 0; _vars.R= 5;
    }
  
} /* end calc_LR */


static void
calc_M_val ()
{
  
  int I, aux;
  MIXu32 value;
  
  
  I= READ_I;
  if ( I > 6 )
    {
      _warning ( _udata,
        	 "valor de I invàlid: %d",
        	 I );
      I= 6;
    }
  CALC_ADDR ( _vars.inst, _vars.M );
  if ( I != 0 )
    {
      value= _regs.I[I-1];
      aux= value&0xFFF;
      if ( IS_NEG ( value ) )
        aux= -aux;
      _vars.M+= aux;
    }
  
} /* end calc_M_val */


static void
calc_M ()
{
  
  calc_M_val ();
  if ( _vars.M < 0 || _vars.M > 3999 )
    {
      _warning ( _udata, "valor de M invàlid: %d", _vars.M );
      _vars.M= 3999;
    }
  
} /* end calc_M */


static MIXu32
ld ()
{
  
  MIXu32 ret, data, mask;
  
  
  calc_LR ();
  calc_M ();
  data= GET_DATA;
  ret= 0;
  if ( _vars.L == 0 )
    {
      ret|= data&NMASK;
      _vars.L= 1;
    }
  if ( _vars.R != 0 )
    {
      data>>= 6*(5-_vars.R);
      mask= 0xFFFFFFFF<<(6*(_vars.R-_vars.L+1));
      ret|= ((~mask)&data);
    }
  
  return ret;
  
} /* end ld */


static void
st (
    MIXu32 value
    )
{
  
  MIXu32 data, mask;
  
  
  calc_LR ();
  calc_M ();
  data= GET_DATA;
  if ( _vars.L == 0 )
    {
      data&= INMASK;
      data|= value&NMASK;
      _vars.L= 1;
    }
  if ( _vars.R != 0 )
    {
      mask=
        (INMASK>>(6*(_vars.L-1))) &
        (INMASK<<(6*(5-_vars.R)));
      value<<= 6*(5-_vars.R);
      data= (data&(~mask)) | (value&mask);
    }
  _mem[_vars.M]= data;
  
} /* end st */


static MIXu32
add_aux (
         MIXu32 reg,
         MIXs32 val
         )
{
  
  if ( val == 0 )
    reg&= NMASK;
  else
    {
      if ( val < 0 )
        {
          reg= NMASK;
          val= -val;
        }
      else reg= 0;
      if ( val&0xC0000000 ) _overflow= ON;
      reg|= ((MIXu32) val)&INMASK;
    }
  
  return reg;
  
} /* end add_aux */


static MIXu32
mop (
     MIXu32 reg
     )
{
  
  int F;
  MIXs32 op;
  
  
  F= READ_F;
  calc_M_val ();
  
  /* IN i DE. */
  if ( F < 2 )
    {
      op= (MIXs32) reg;
      WORDTOS32 ( op );
      reg= add_aux ( reg, op + (MIXs32) (F?-_vars.M:_vars.M) );
    }
  
  /* EN i ENN. */
  else if ( F < 4 )
    {
      if ( _vars.M == 0 )
        {
          reg= _vars.inst&NMASK;
          if ( F == 3 ) reg^= NMASK;
        }
      else
        {
          if ( _vars.M < 0 )
            {
              _vars.M= -_vars.M;
              reg= NMASK;
            }
          else reg= 0;
          if ( F == 3 ) reg^= NMASK;
          reg|= (MIXu32) _vars.M;
        }
    }
  
  else
    {
      reg= 0;
      _warning ( _udata, "operació C=%d F=%d no vàlida",
        	 _vars.inst&0x3F, F );
    }
  
  return reg;
  
} /* end mop */


static void
cmp (
     MIXu32 value
     )
{
  
  MIXu32 data, mask;
  MIXs32 op1, op2;
  MIX_Bool sign;
  int desp;
  
  
  calc_LR ();
  calc_M ();
  data= GET_DATA;
  if ( _vars.L == 0 )
    {
      sign= MIX_TRUE;
      _vars.L= 1;
    }
  else sign= MIX_FALSE;
  if ( _vars.R != 0 )
    {
      mask= ~(0xFFFFFFFF<<(6*(_vars.R-_vars.L+1)));
      desp= 6*(5-_vars.R);
      op1= (value>>desp)&mask;
      op2= (data>>desp)&mask;
      if ( sign )
        {
          if ( value&NMASK ) op1= -op1;
          if ( data&NMASK ) op2= -op2;
        }
      if ( op1 == op2 ) _cmp= EQUAL;
      else if ( op1 < op2 ) _cmp= LESS;
      else _cmp= GREATER;
    }
  else _cmp= EQUAL;
  
} /* end cmp */


static void
jreg (
      MIXu32 reg
      )
{
  
  MIXs32 op;
  MIX_Bool jump;
  int F;
  
  
  op= (MIXs32) reg;
  WORDTOS32 ( op );
  F= READ_F;
  switch ( F )
    {
      
      /* J_N */
    case 0: jump= (op < 0); break;
      
      /* J_Z */
    case 1: jump= (op == 0); break;
      
      /* J_P */
    case 2: jump= (op > 0); break;
      
      /* J_NN */
    case 3: jump= (op >= 0); break;
      
      /* J_NZ */
    case 4: jump= (op != 0); break;
      
      /* J_NP */
    case 5: jump= (op <= 0); break;
      
    default:
      jump= MIX_FALSE;
      _warning ( _udata, "operació C=%d F=%d no vàlida",
                 _vars.inst&0x3F, F );
      
    }
  
  if ( jump )
    {
      _regs.J= _regs.PC;
      calc_M ();
      _regs.PC= _vars.M;
    }
  
} /* end jreg */


static void
inout (
       MIX_OPType op
       )
{
  
  int dev;
  MIX_IOOPChar *ioop;
  MIX_IOOPWord *ioopw;
  
  static MIX_IOOPChar ioopchars[21];
  static const size_t remain_chars[21]=
    {
      0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0,
      80, 80, 120, 70, 70
    };
  static MIX_IOOPWord ioopwords[21];
  
  
  dev= READ_F;
  CHECK_DEV ( dev );
  if ( _device_busy ( _udata, dev ) )
    {
      _regs.PC= _regs.old_PC;
      _run_state.v= WAIT_DEVICE;
      _run_state.dev= dev;
      _notify_waiting_device ( _udata, dev, true );
      return;
    }
  calc_M ();
  if ( dev < MIX_CARDREADER )
    {
      ioopw= &(ioopwords[dev]);
      ioopw->remain= 100;
      ioopw->_addr= _vars.M;
      _init_ioopword ( _udata, dev, ioopw, op );
    }
  else
    {
      ioop= &(ioopchars[dev]);
      ioop->remain= remain_chars[dev];
      ioop->_pos= 0;
      ioop->_addr= _vars.M;
      if ( op == MIX_IN )
        ioop->_aux= 0;
      else
        {
          ioop->_aux= _mem[ioop->_addr];
          if ( ++(ioop->_addr) == 4000 ) ioop->_addr= 0;
        }
      _init_ioopchar ( _udata, dev, ioop, op );
    }
  
} /* end inout */


static void
jbusy (
       MIX_Bool jump
       )
{
  
  int dev;
  
  
  dev= READ_F;
  CHECK_DEV ( dev )
  if ( _device_busy ( _udata, dev ) == jump )
    {
      _regs.J= _regs.PC;
      calc_M ();
      _regs.PC= _vars.M;
    }
  
} /* end jbusy */


static void
num ()
{
  
  /* NOTA: El desbordament té una senzilla solució, simplement el
   * mòdul de 2^30 es pot calcular ignorant tots els bits majors.
   */
  int i;
  MIXu32 res;
  
  
  res= 0;
  for ( i= 24; i != -6; i-= 6 )
    {
      res*= 10;
      res+= ((_regs.A>>i)&0x3F)%10;
    }
  for ( i= 24; i != -6; i-= 6 )
    {
      res*= 10;
      res+= ((_regs.X>>i)&0x3F)%10;
    }
  _regs.A&= NMASK;
  _regs.A|= res&INMASK;
  
} /* end num */


static void
char_op ()
{
  
  int i;
  MIXu32 aux, aux2;
  
  
  aux= _regs.A&INMASK;
  aux2= 0;
  for ( i= 0; i < 5; ++i )
    {
      aux2>>= 6;
      aux2|= ((aux%10)+30)<<24;
      aux/= 10;
    }
  _regs.X&= NMASK;
  _regs.X|= aux2;
  aux2= 0;
  for ( i= 0; i < 5; ++i )
    {
      aux2>>= 6;
      aux2|= ((aux%10)+30)<<24;
      aux/= 10;
    }
  _regs.A&= NMASK;
  _regs.A|= aux2;
  
} /* end char_op */




/****************/
/* INSTRUCCIONS */
/****************/

static unsigned int
LDA ()
{
  
  _regs.A= ld ();
  return 2;
  
} /* end LDA */


static unsigned int
LDX ()
{
  
  _regs.X= ld ();
  return 2;
  
} /* end LDX */


static unsigned int
LD1 ()
{
  
  _regs.I[0]= LDI;
  return 2;
  
} /* end LD1 */


static unsigned int
LD2 ()
{
  
  _regs.I[1]= LDI;
  return 2;
  
} /* end LD2 */


static unsigned int
LD3 ()
{
  
  _regs.I[2]= LDI;
  return 2;
  
} /* end LD3 */


static unsigned int
LD4 ()
{
  
  _regs.I[3]= LDI;
  return 2;
  
} /* end LD4 */


static unsigned int
LD5 ()
{
  
  _regs.I[4]= LDI;
  return 2;
  
} /* end LD5 */


static unsigned int
LD6 ()
{
  
  _regs.I[5]= LDI;
  return 2;
  
} /* end LD6 */


static unsigned int
LDAN ()
{
  
  _regs.A= LDN;
  return 2;
  
} /* end LDAN */


static unsigned int
LDXN ()
{
  
  _regs.X= LDN;
  return 2;
  
} /* end LDXN */


static unsigned int
LD1N ()
{
  
  _regs.I[0]= LDIN;
  return 2;
  
} /* end LD1N */


static unsigned int
LD2N ()
{
  
  _regs.I[1]= LDIN;
  return 2;
  
} /* end LD2N */


static unsigned int
LD3N ()
{
  
  _regs.I[2]= LDIN;
  return 2;
  
} /* end LD3N */


static unsigned int
LD4N ()
{
  
  _regs.I[3]= LDIN;
  return 2;
  
} /* end LD4N */


static unsigned int
LD5N ()
{
  
  _regs.I[4]= LDIN;
  return 2;
  
} /* end LD5N */


static unsigned int
LD6N ()
{
  
  _regs.I[5]= LDIN;
  return 2;
  
} /* end LD6N */


static unsigned int
STA ()
{
  
  st ( _regs.A );
  return 2;
  
} /* end STA */


static unsigned int
STX ()
{
  
  st ( _regs.X );
  return 2;
  
} /* end STX */


static unsigned int
ST1 ()
{
  
  st ( _regs.I[0] );
  return 2;
  
} /* end ST1 */


static unsigned int
ST2 ()
{
  
  st ( _regs.I[1] );
  return 2;
  
} /* end ST2 */


static unsigned int
ST3 ()
{
  
  st ( _regs.I[2] );
  return 2;
  
} /* end ST3 */


static unsigned int
ST4 ()
{
  
  st ( _regs.I[3] );
  return 2;
  
} /* end ST4 */


static unsigned int
ST5 ()
{
  
  st ( _regs.I[4] );
  return 2;
  
} /* end ST5 */


static unsigned int
ST6 ()
{
  
  st ( _regs.I[5] );
  return 2;
  
} /* end ST6 */


static unsigned int
STJ ()
{
  
  st ( _regs.J );
  return 2;
  
} /* end STJ */


static unsigned int
STZ ()
{
  
  st ( 0 );
  return 2;
  
} /* end STZ */


static unsigned int
ADD ()
{
  
  MIXs32 op1, op2;
  
  
  ADD_ ( +=, op1, op2 );
  
  return 2;
  
} /* end ADD */


static unsigned int
SUB ()
{
  
  MIXs32 op1, op2;
  
  
  ADD_ ( -=, op1, op2 );
  
  return 2;
  
} /* end SUB */


static unsigned int
MUL ()
{

#ifdef _MIX_64
  MIXs32 op1, op2;
  MIXs64 res;
  
  
  CALC_OP1_OP2 ( op1, op2 );
  res= (MIXs64) op1 * (MIXs64) op2;
  if ( res < 0 )
    {
      _regs.A= _regs.X= NMASK;
      res= -res;
    }
  else _regs.A= _regs.X= 0;
  _regs.X|= (MIXu32) (res&INMASK);
  _regs.A|= (MIXu32) ((res>>30)&INMASK);
#else
  _warning ( _udata, "l'operació MUL no està implementada" );
#endif
  
  return 10;
  
} /* end MUL */


static unsigned int
DIV ()
{
  
#ifdef _MIX_64
  MIXs32 op1, op2, aop1, aop2;
  MIXs64 op;
  
  
  op1= (MIXs32) _regs.A; aop1= ABS ( op1 );
  CALC_OP2 ( op2 ); aop2= ABS ( op2 );
  if ( aop1 >= aop2 || aop2 == 0 )
    _overflow= ON;
  /* Es supossa que en aquest cas tant el resultat com la resta caben
     en 30 bits cadascuna. */
  else
    {
      op= ((MIXs32) aop1)<<30;
      op|= (MIXs32) (_regs.X&INMASK);
      _regs.A= (MIXu32) (op/aop2);
      _regs.X= (MIXu32) (op%aop2);
      if ( (op1&NMASK) != (op2&NMASK) )
        {
          _regs.A|= NMASK;
          _regs.X|= NMASK;
        }
    }
#else
  _warning ( _udata, "la operació DIV no està implementada" );
#endif

  return 12;
  
} /* end DIV */


static unsigned int
MOPA ()
{
  
  _regs.A= mop ( _regs.A );
  return 1;
  
} /* end MOPA */


static unsigned int
MOPX ()
{
  
  _regs.X= mop ( _regs.X );
  return 1;
  
} /* end MOPX */


static unsigned int
MOP1 ()
{
  
  _regs.I[0]= MOPI ( _regs.I[0] );
  return 1;
  
} /* end MOP1 */


static unsigned int
MOP2 ()
{
  
  _regs.I[1]= MOPI ( _regs.I[1] );
  return 1;
  
} /* end MOP2 */


static unsigned int
MOP3 ()
{
  
  _regs.I[2]= MOPI ( _regs.I[2] );
  return 1;
  
} /* end MOP3 */


static unsigned int
MOP4 ()
{
  
  _regs.I[3]= MOPI ( _regs.I[3] );
  return 1;
  
} /* end MOP4 */


static unsigned int
MOP5 ()
{
  
  _regs.I[4]= MOPI ( _regs.I[4] );
  return 1;
  
} /* end MOP5 */


static unsigned int
MOP6 ()
{
  
  _regs.I[5]= MOPI ( _regs.I[5] );
  return 1;
  
} /* end MOP6 */


static unsigned int
CMPA ()
{
  
  cmp ( _regs.A );
  return 2;
  
} /* end CMPA */


static unsigned int
CMPX ()
{
  
  cmp ( _regs.X );
  return 2;
  
} /* end CMPX */


static unsigned int
CMP1 ()
{
  
  cmp ( _regs.I[0] );
  return 2;
  
} /* end CMP1 */


static unsigned int
CMP2 ()
{
  
  cmp ( _regs.I[1] );
  return 2;
  
} /* end CMP2 */


static unsigned int
CMP3 ()
{
  
  cmp ( _regs.I[2] );
  return 2;
  
} /* end CMP3 */


static unsigned int
CMP4 ()
{
  
  cmp ( _regs.I[3] );
  return 2;
  
} /* end CMP4 */


static unsigned int
CMP5 ()
{
  
  cmp ( _regs.I[4] );
  return 2;
  
} /* end CMP5 */


static unsigned int
CMP6 ()
{
  
  cmp ( _regs.I[5] );
  return 2;
  
} /* end CMP6 */


/* NOTA: És precís que PC s'incremente abans de cridar a les
   instruccions. */
static unsigned int
JOP ()
{
  
  int F;
  MIX_Bool jump, save;
  
  
  F= READ_F;
  jump= save= MIX_FALSE;
  switch ( F )
    {
      
      /* JMP */
    case 0:
      jump= MIX_TRUE;
      break;
      
      /* JSJ */
    case 1:
      jump= save= MIX_TRUE;
      break;
      
      /* JOV */
    case 2:
      if ( _overflow == ON )
        {
          jump= MIX_TRUE;
          _overflow= MIX_FALSE;
        }
      break;
      
      /* JNOV */
    case 3:
      if ( _overflow == OFF )
        jump= MIX_TRUE;
      else _overflow= OFF;
      break;
      
      /* JL */
    case 4:
      if ( _cmp == LESS ) jump= MIX_TRUE;
      break;
      
      /* JE */
    case 5:
      if ( _cmp == EQUAL ) jump= MIX_TRUE;
      break;
      
      /* JG */
    case 6:
      if ( _cmp == GREATER ) jump= MIX_TRUE;
      break;
      
      /* JGE */
    case 7:
      if ( _cmp == GREATER || _cmp == EQUAL )
        jump= MIX_TRUE;
      break;
      
      /* JNE */
    case 8:
      if ( _cmp != EQUAL ) jump= MIX_TRUE;
      break;
      
      /* JLE */
    case 9:
      if ( _cmp == LESS || _cmp == EQUAL )
        jump= MIX_TRUE;
      break;
      
    default:
      _warning ( _udata, "operació C=39 F=%d no vàlida", F );
      
    }
  
  if ( jump )
    {
      if ( !save ) _regs.J= (MIXu32) _regs.PC;
      calc_M ();
      _regs.PC= _vars.M;
    }
  
  return 1;
  
} /* end JOP */


static unsigned int
JA ()
{
  
  jreg ( _regs.A );
  return 1;
  
} /* end JA */


static unsigned int
JX ()
{
  
  jreg ( _regs.X );
  return 1;
  
} /* end JX */


static unsigned int
J1 ()
{
  
  jreg ( _regs.I[0] );
  return 1;
  
} /* end J1 */


static unsigned int
J2 ()
{
  
  jreg ( _regs.I[1] );
  return 1;
  
} /* end J2 */


static unsigned int
J3 ()
{
  
  jreg ( _regs.I[2] );
  return 1;
  
} /* end J3 */


static unsigned int
J4 ()
{
  
  jreg ( _regs.I[3] );
  return 1;
  
} /* end J4 */


static unsigned int
J5 ()
{
  
  jreg ( _regs.I[4] );
  return 1;
  
} /* end J5 */


static unsigned int
J6 ()
{
  
  jreg ( _regs.I[5] );
  return 1;
  
} /* end J6 */


static unsigned int
SHIFT ()
{
  
  int F;
  MIXu32 signA, signX, aux;
  
  
  F= READ_F;
  calc_M_val ();
  if ( _vars.M < 0 )
    {
      _warning ( _udata, "el valor de M és engatiu"
        	 " en una operació  'shift'"  );
      goto ret;
    }
  else if ( F < 4 )
    {
      if ( _vars.M > 10 ) _vars.M= 10;
      _vars.M*= 6;
    }
  else _vars.M= (_vars.M%10)*6;
  switch ( F )
    {
      
      /* SLA */
    case 0:
      signA= _regs.A&NMASK;
      _regs.A= ((_regs.A<<_vars.M)&INMASK)|signA;
      break;
      
      /* SRA */
    case 1:
      signA= _regs.A&NMASK;
      _regs.A= ((_regs.A&INMASK)>>_vars.M)|signA;
      break;
      
      /* SLAX */
    case 2:
      signA= _regs.A&NMASK;
      signX= _regs.X&NMASK;
      if ( _vars.M < 30 )
        {
          _regs.A= (((_regs.A<<_vars.M)|
        	     ((_regs.X&INMASK)>>(30-_vars.M)))
        	    &INMASK)|signA;
          _regs.X= ((_regs.X<<_vars.M)&INMASK)|signX;
        }
      else
        {
          _regs.A= ((_regs.X<<(_vars.M-30))&INMASK)|signA;
          _regs.X= signX;
        }
      break;
      
      /* SRAX */
    case 3:
      signA= _regs.A&NMASK;
      signX= _regs.X&NMASK;
      if ( _vars.M < 30 )
        {
          _regs.X= ((((_regs.X&INMASK)>>_vars.M)|
        	     (_regs.A<<(30-_vars.M)))
        	    &INMASK)|signX;
          _regs.A= ((_regs.A&INMASK)>>_vars.M)|signA;
        }
      else
        {
          _regs.X= ((_regs.A&INMASK)>>(_vars.M-30))|signX;
          _regs.A= signA;
        }
      break;
      
      /* SLC */
    case 4:
      aux= _regs.A;
      signA= _regs.A&NMASK;
      signX= _regs.X&NMASK;
      if ( _vars.M < 30 )
        {
          _regs.A= (((_regs.A<<_vars.M)|
        	     ((_regs.X&INMASK)>>(30-_vars.M)))
        	    &INMASK)|signA;
          _regs.X= (((_regs.X<<_vars.M)|
        	     ((aux&INMASK)>>(30-_vars.M)))
        	    &INMASK)|signX;
        }
      else
        {
          _vars.M-= 30;
          _regs.A= (((_regs.X<<_vars.M)|
        	     ((_regs.A&INMASK)>>(30-_vars.M)))
        	    &INMASK)|signA;
          _regs.X= (((aux<<_vars.M)|
        	     ((_regs.X&INMASK)>>(30-_vars.M)))
        	    &INMASK)|signX;
        }
      break;
      
      /* SRC */
    case 5:
      aux= _regs.X;
      signA= _regs.A&NMASK;
      signX= _regs.X&NMASK;
      if ( _vars.M < 30 )
        {
          _regs.X= ((((_regs.X&INMASK)>>_vars.M)|
        	     (_regs.A<<(30-_vars.M)))
        	    &INMASK)|signX;
          _regs.A= ((((_regs.A&INMASK)>>_vars.M)|
        	     (aux<<(30-_vars.M)))
        	    &INMASK)|signA;
        }
      else
        {
          _vars.M-= 30;
          _regs.X= ((((_regs.A&INMASK)>>_vars.M)|
        	     (_regs.X<<(30-_vars.M)))
        	    &INMASK)|signX;
          _regs.A= ((((aux&INMASK)>>_vars.M)|
        	     (_regs.A<<(30-_vars.M)))
        	    &INMASK)|signA;
        }
      break;
      
    default:
      _warning ( _udata, "operació C=6 F=%d no vàlida", F );
      
    }
  
 ret:
  
  return 2;
  
} /* end SHIFT */


/* Esta instrucció es tan costosa que podria executar-se de manera no
   atòmica a al vegada que es fan operacions d'entrada eixida. Però
   com manipula memòria interna (s'enten que anirà molt ràpid), el
   màxim són 63 paraules i a més normlament sol ser 1, aleshores, ho
   faré de manera atòmica. */
static unsigned int
MOVE ()
{
  
  int F, I1, f;
  
  
  F= READ_F;
  if ( F == 0 ) return 1;
  calc_M ();
  I1= _regs.I[0];
  if ( IS_NEG ( I1 ) )
    {
      _warning ( _udata, "el valor de I1 és negatiu,"
        	 " s'interpretarà com positiu per a"
        	 " executar MOVE" );
      I1&= INMASK;
    }
  if ( I1 > 3999 )
    {
      _warning ( _udata, "valor de I1=%d no vàlid per a MOVE", I1 );
      I1= 3999;
    }
  for ( f= 0; f < F; ++f )
    {
      _mem[I1]= _mem[_vars.M];
      if ( ++I1 == 4000 ) I1= 0;
      if ( ++_vars.M == 4000 ) _vars.M= 0;
    }
  _regs.I[0]= (MIXu32) I1;
  
  return (unsigned int) ((F<<1)|0x1);
  
} /* end MOVE */


static unsigned int
NOP ()
{
  return 1;
} /* end NOP */


static unsigned int
IN ()
{
  inout ( MIX_IN );
  return 1;
} /* end IN */


static unsigned int
OUT ()
{
  inout ( MIX_OUT );
  return 1;
} /* end OUT */


static unsigned int
JRED ()
{
  jbusy ( MIX_FALSE );
  return 1;
} /* end JRED */


static unsigned int
JBUS ()
{
  jbusy ( MIX_TRUE );
  return 1;
} /* end JBUS */


static unsigned int
IOC ()
{
  
  int dev;
  
  
  dev= READ_F;
  CHECK_DEV_BASE ( dev, return 0 );
    if ( _device_busy ( _udata, dev ) )
    {
      _regs.PC= _regs.old_PC;
      _run_state.v= WAIT_DEVICE;
      _run_state.dev= dev;
      _notify_waiting_device ( _udata, dev, true );
      return 0;
    }
  calc_M_val ();
  switch ( dev )
    {
    case MIX_TAPEUNIT1:
    case MIX_TAPEUNIT2:
    case MIX_TAPEUNIT3:
    case MIX_TAPEUNIT4:
    case MIX_TAPEUNIT5:
    case MIX_TAPEUNIT6:
    case MIX_TAPEUNIT7:
    case MIX_TAPEUNIT8:
      if ( _vars.M == 0 )
        _io_control ( _udata, MIX_MT_REWOUND, dev );
      else if ( _vars.M < 0 )
        _io_control ( _udata, MIX_MT_SKIPBACKWARD, dev, -_vars.M*100 );
      else
        _io_control ( _udata, MIX_MT_SKIPFORWARD, dev, _vars.M*100 );
      break;
    case MIX_LINEPRINTER:
      if ( _vars.M != 0 )
        _warning ( _udata, "operació de control (M:%d) no"
        	   " suportada per l'impresora", _vars.M );
      else _io_control ( _udata, MIX_LP_SKIPTOFOLLOWINGPAGE );
      break;
    default: _warning ( _udata, "el dispositiu %d no suporta"
        		" operacions de control", dev );
    }
  
  return 1;
  
} /* end IOC */


static unsigned int
SPECIAL ()
{
  
  int F;
  
  
  F= READ_F;
  switch ( F )
    {
      
    case 0: /* NUM */
      num ();
      break;
      
    case 1: /* CHAR */
      char_op ();
      break;
      
    case 2:
      _run_state.v= HALT;
      break;
      
    default:
      _warning ( _udata,
        	 "operació C=%d F=%d no vàlida",
        	 _vars.inst&0x3F, F );
      
    }
  
  return 10;
  
} /* end SPECIAL */


static unsigned int (*_insts[64]) (void)=
{
  NOP,
  ADD,
  SUB,
  MUL,
  DIV,
  SPECIAL,
  SHIFT,
  MOVE,
  LDA,
  LD1,
  LD2,
  LD3,
  LD4,
  LD5,
  LD6,
  LDX,
  LDAN,
  LD1N,
  LD2N,
  LD3N,
  LD4N,
  LD5N,
  LD6N,
  LDXN,
  STA,
  ST1,
  ST2,
  ST3,
  ST4,
  ST5,
  ST6,
  STX,
  STJ,
  STZ,
  JBUS,
  IOC,
  IN,
  OUT,
  JRED,
  JOP,
  JA,
  J1,
  J2,
  J3,
  J4,
  J5,
  J6,
  JX,
  MOPA,
  MOP1,
  MOP2,
  MOP3,
  MOP4,
  MOP5,
  MOP6,
  MOPX,
  CMPA,
  CMP1,
  CMP2,
  CMP3,
  CMP4,
  CMP5,
  CMP6,
  CMPX
};




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
MIX_go (void)
{
  _run_state.v= RUNNING_GO_STEP0;
} // end MIX_go


void
MIX_init (
          const MIX_Frontend *frontend,
          void               *udata
          )
{
  
  int i;
  
  
  _regs.A= _regs.X= 0;
  for ( i= 0; i < 6; ++i )
    _regs.I[i]= 0;
  _regs.J= 0;
  _regs.PC= 0;
  
  memset ( &(_mem[0]), 0, 16000 /* 4000 * 4 */ );
  
  _vars.inst= 0;
  _vars.L= 0;
  _vars.R= 5;
  _vars.M= 0;
  
  _overflow= OFF;
  _cmp= EQUAL;
  
  _init_ioopchar= frontend->init_ioopchar;
  _init_ioopword= frontend->init_ioopword;
  _device_busy= frontend->device_busy;
  _io_control= frontend->io_control;
  _notify_waiting_device= frontend->notify_waiting_device;
  
  _warning= frontend->warning;
  _udata= udata;
  
  _check= frontend->check;
  
  _run_state.v= HALT;
  _run_state.notify_cr= false;
  
} // end MIX_init


int
MIX_iter (
          const int  cc,
          MIX_Bool  *halt
          )
{

  static MIX_IOOPChar ioop; // Operació inicial
  
  int cc_remain,cc_total,tmp;
  MIX_Bool stop;

  

  cc_remain= cc;
  cc_total= 0;
  *halt= MIX_FALSE;
  while ( cc_remain > 0 )
    switch ( _run_state.v )
      {
        
      case RUNNING: // Executa següent instrucció.
        _regs.old_PC= _regs.PC;
        _vars.inst= _mem[_regs.PC];
        if ( ++_regs.PC == 4000 ) _regs.PC= 0;
        tmp= _insts[_vars.inst&0x3F] ();
        cc_remain-= tmp;
        cc_total+= tmp;
        break;

      case HALT:
        *halt= MIX_TRUE;
        cc_total+= cc_remain;
        cc_remain= 0;
        break;

      case WAIT_DEVICE:
        if ( _device_busy ( _udata, _run_state.dev ) )
          {
            cc_total+= cc_remain;
            cc_remain= 0;
          }
        else
          {
            _run_state.v= RUNNING;
            _notify_waiting_device ( _udata, _run_state.dev, false );
          }
        break;
        
      case RUNNING_GO_STEP0:
        if ( _device_busy ( _udata, MIX_CARDREADER ) )
          {
            cc_total+= cc_remain;
            cc_remain= 0;
            if ( !_run_state.notify_cr )
              {
                _run_state.notify_cr= true;
                _notify_waiting_device ( _udata, MIX_CARDREADER, true );
              }
          }
        else
          {
            if ( _run_state.notify_cr )
              {
                _notify_waiting_device ( _udata, MIX_CARDREADER, false );
                _run_state.notify_cr= false;
              }
            // LLig una targeta en 0.
            ioop.remain= 80; // Vore inout.
            ioop._pos= 0;
            ioop._addr= 0;
            ioop._aux= 0;
            _init_ioopchar ( _udata, MIX_CARDREADER, &ioop, MIX_IN );
            _run_state.v= RUNNING_GO_STEP1;
          }
        break;
        
      case RUNNING_GO_STEP1:
        if ( _device_busy ( _udata, MIX_CARDREADER ) )
          {
            cc_total+= cc_remain;
            cc_remain= 0;
            if ( !_run_state.notify_cr )
              {
                _run_state.notify_cr= true;
                _notify_waiting_device ( _udata, MIX_CARDREADER, true );
              }
          }
        else
          {
            if ( _run_state.notify_cr )
              {
                _notify_waiting_device ( _udata, MIX_CARDREADER, false );
                _run_state.notify_cr= false;
              }
            _regs.PC= 0;
            _regs.J= 0;
            _run_state.v= RUNNING;
          }
        break;
        
      }
  
  if ( _check != NULL )
    {
      _check ( _udata, &stop );
      if ( stop )
        {
          *halt= MIX_TRUE;
          if ( _run_state.v == WAIT_DEVICE )
            _notify_waiting_device ( _udata, _run_state.dev, false );
          else if ( _run_state.notify_cr )
            {
              _notify_waiting_device ( _udata, MIX_CARDREADER, false );
              _run_state.notify_cr= false;
            }
          _run_state.v= HALT;
        }
    }

  return cc_total;
  
} // end MIX_iter


size_t
MIX_read_chars (
        	MIX_Char     *to,
        	size_t        nmeb,
        	MIX_IOOPChar *op
        	)
{
  
  MIX_IOOPChar ioop;
  size_t i;
  
  
  ioop= *op;
  for ( i= 0; i < nmeb && ioop.remain > 0;
        ++i, --ioop.remain )
    {
      to[i]= (ioop._aux&0x3F000000)>>24;
      ioop._aux<<= 6;
      if ( ++ioop._pos == 5 )
        {
          ioop._aux= _mem[ioop._addr];
          if ( ++ioop._addr == 4000 ) ioop._addr= 0;
          ioop._pos= 0;
        }
    }
  *op= ioop;
  
  return ioop.remain;
  
} /* end MIX_read_chars */


size_t
MIX_write_chars (
        	 const MIX_Char *from,
        	 size_t          nmeb,
        	 MIX_IOOPChar   *op
        	 )
{
  
  MIX_IOOPChar ioop;
  size_t i;
  

  ioop= *op;
  for ( i= 0; i < nmeb && ioop.remain > 0;
        ++i, --ioop.remain )
    {
      ioop._aux<<= 6;
      ioop._aux|= from[i]&0x3F;
      if ( ++ioop._pos == 5 )
        {
          _mem[ioop._addr]= ioop._aux;
          if ( ++ioop._addr == 4000 ) ioop._addr= 0;
          ioop._pos= 0;
          ioop._aux= 0;
        }
    }
  *op= ioop;
  
  return ioop.remain;
  
} /* end MIX_write_chars */


size_t
MIX_read_words (
        	MIX_Word     *to,
        	size_t        nmeb,
        	MIX_IOOPWord *op
        	)
{
  
  MIX_IOOPWord ioop;
  size_t i;
  
  
  ioop= *op;
  for ( i= 0; i < nmeb && ioop.remain > 0;
        ++i, --ioop.remain )
    {
      to[i]= _mem[ioop._addr];
      if ( ++ioop._addr == 4000 ) ioop._addr= 0;
    }
  *op= ioop;
  
  return ioop.remain;
  
} /* end MIX_read_words */


size_t
MIX_write_words (
        	 const MIX_Word *from,
        	 size_t          nmeb,
        	 MIX_IOOPWord   *op
        	 )
{
  
  MIX_IOOPWord ioop;
  size_t i;
  
  
  ioop= *op;
  for ( i= 0; i < nmeb && ioop.remain > 0;
        ++i, --ioop.remain )
    {
      _mem[ioop._addr]= from[i]&(NMASK|INMASK);
      if ( ++ioop._addr == 4000 ) ioop._addr= 0;
    }
  *op= ioop;
  
  return ioop.remain;
  
} /* end MIX_write_words */
