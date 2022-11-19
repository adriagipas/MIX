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
 *  MIX.h - Simulador de 'MIX' escrit en C. Concretament simula
 *          una màquina amb bytes de 6 bits i amb unes prestacions
 *          normals (5MHz).
 *
 */

#ifndef __MIX_H__
#define __MIX_H__

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>


/*********/
/* TIPUS */
/*********/

#if (CHAR_BIT != 8) || (UINT_MAX != 4294967295U)
#error Arquitectura no suportada
#else
#if (ULONG_MAX == 18446744073709551615UL)
#define _MIX_64
#endif
#endif


/* TIPUS */

/* Tipus sencers. */
typedef int MIXs32;
typedef unsigned int MIXu32;
#ifdef _MIX_64
typedef long MIXs64;
#endif

/* Tipus booleà. */
typedef enum
  {
    MIX_FALSE,
    MIX_TRUE
  } MIX_Bool;

/* Identificador de dispostiu. */
typedef enum
  {
    MIX_TAPEUNIT1= 0,
    MIX_TAPEUNIT2,
    MIX_TAPEUNIT3,
    MIX_TAPEUNIT4,
    MIX_TAPEUNIT5,
    MIX_TAPEUNIT6,
    MIX_TAPEUNIT7,
    MIX_TAPEUNIT8,
    MIX_DISKORDRUMUNIT1,
    MIX_DISKORDRUMUNIT2,
    MIX_DISKORDRUMUNIT3,
    MIX_DISKORDRUMUNIT4,
    MIX_DISKORDRUMUNIT5,
    MIX_DISKORDRUMUNIT6,
    MIX_DISKORDRUMUNIT7,
    MIX_DISKORDRUMUNIT8,
    MIX_CARDREADER,
    MIX_CARDPUNCH,
    MIX_LINEPRINTER,
    MIX_TYPEWRITERTERMINAL,
    MIX_PAPERTAPE
  } MIX_Device;

/* Caràcters suportats per la máquina MIX. */
typedef enum
  {
    MIX_SPACE,
    MIX_A,
    MIX_B,
    MIX_C,
    MIX_D,
    MIX_E,
    MIX_F,
    MIX_G,
    MIX_H,
    MIX_I,
    MIX_DELTA,
    MIX_J,
    MIX_K,
    MIX_L,
    MIX_M,
    MIX_N,
    MIX_O,
    MIX_P,
    MIX_Q,
    MIX_R,
    MIX_SIGMA,
    MIX_PI,
    MIX_S,
    MIX_T,
    MIX_U,
    MIX_V,
    MIX_W,
    MIX_X,
    MIX_Y,
    MIX_Z,
    MIX_0,
    MIX_1,
    MIX_2,
    MIX_3,
    MIX_4,
    MIX_5,
    MIX_6,
    MIX_7,
    MIX_8,
    MIX_9,
    MIX_DOT,
    MIX_COMMA,
    MIX_OPARENTHESE,
    MIX_CPARENTHESE,
    MIX_PLUS,
    MIX_MINUS,
    MIX_ASTERISK,
    MIX_SLASH,
    MIX_EQUAL,
    MIX_DOLLAR,
    MIX_LESS,
    MIX_GREATER,
    MIX_AT,
    MIX_SEMICOLON,
    MIX_COLON,
    MIX_APOSTROPHE
  } MIX_Char;

/* Tipus paraula. És un sencer de 32 bits. El bit 0 és el bit 0 del
 * byte 5, el bit 29 és el bit 5 del byte 1, el bit 31 és el signe (1
 * negatiu, 0 positiu), el bit 30 no s'usa.
 */
typedef MIXu32 MIX_Word;

/* Tipus d'operacions i/o. */
typedef enum
  {
    MIX_IN,
    MIX_OUT
  } MIX_OPType;

/* Descriptor d'una operació d'entrada/eixida de caràcters. */
typedef struct
{
  
  size_t remain;     /* Caràcters que falten per escriure/llegir. */
  int    _addr;      /* Adreça on s'escriu/llig. */
  int    _pos;       /* Posició dins de la paraula actual. */
  MIXu32 _aux;       /* Variable auxiliar. */
  
} MIX_IOOPChar;

/* Descriptor d'una operació d'entrada/eixida de paraules. */
typedef struct
{
  
  size_t remain;    /* Paraules que falten per escriure/llegir. */
  int    _addr;     /* Adreça on s'escriu/llig. */
  
} MIX_IOOPWord;

/* Operacions de control dels dispositius d'entrada/eixida. */
typedef enum
  {
    MIX_LP_SKIPTOFOLLOWINGPAGE,    /* Ordena a l'impresora passar al
        			      principi de la següent
        			      pàgina. */
    MIX_MT_REWOUND,                /* Rebobina la cinta. */
    MIX_MT_SKIPBACKWARD,           /* Retrocedeix blocs. */
    MIX_MT_SKIPFORWARD             /* Avança blocs. */
  } MIX_IOControlOp;

/* Funció per a metre avísos. */
typedef void 
(MIX_Warning) (
               void       *udata,
               const char *format,
               ...
               );

/* Funció per a indicar que es vol començar una operació
 * d'escriptura/lectura en un dispositiu que treballa amb
 * caràcters. S'assegura que el dispositiu indicat sempre treballarà
 * amb caràcters, no s'assegura que el tipus d'operació siga suportada
 * pel dispositiu indicat.
 */
typedef void
(MIX_InitIOOPChar) (
        	    void         *udata,
        	    MIX_Device    dev,
        	    MIX_IOOPChar *op,
        	    MIX_OPType    type
        	    );

/* Funció per a indicar que es vol començar una operació
 * d'escriptura/lectura en un dispositiu que treballa amb
 * paraules. S'assegura que el dispositiu indicat sempre treballarà
 * amb paraules, no s'assegura que el tipus d'operació siga suportada
 * pel dispositiu indicat.
 */
typedef void
(MIX_InitIOOPWord) (
        	    void         *udata,
        	    MIX_Device    dev,
        	    MIX_IOOPWord *op,
        	    MIX_OPType    type
        	    );

/* Funció que torna fals si el dispositiu està preparat per a
 * realitzar una operació de i/o. Torna cert en cas contrari. No es
 * possible distingir entre dispositiu ocupat o dispositiu en estat de
 * fallada o desconnectat.
 */
typedef MIX_Bool
(MIX_DeviceBusy) (
        	  void       *udata,
        	  MIX_Device  dev
        	  );

/* Funció per a realitzar operacions de control dels dispositius
 * d'entrada/eixida.
 *
 * MIX_LP_SKIPTOFOLLOWINGPAGE
 *   - Imprimeix línies en blanc fins acabar la pàgina actual.
 * MIX_MT_RWOUND [0-7]
 *   - Rebobina la cinta indicada.
 * MIX_MT_SKIPBACKWARD [0-7] N
 *   - Retrocedeix la cinta indicada N paraules, o va a la primera paraula,
 *     el que ocorrega primer.
 * MIX_MT_SKIPFORWARD [0-7] N
 *   - Avança la cinta indicada N paraules, o va al final de la cinta,
 *     el que ocorrega primer.
 */
typedef void
(MIX_IOControl) (
        	 void            *udata,
        	 MIX_IOControlOp  op,
        	 ...
        	 );

/* Tipus de funció amb la que el frontend indica a la
 * llibreria si s'ha forçat una senyal de parada. A més esta funció
 * pot ser emprada per el frontend per a tractar els events pendents.
 */
typedef void (MIX_CheckSignals) (
        			 void     *udata,
        			 MIX_Bool *stop
        			 );

/* Tipus de funció amb la que es notifica que un dispositiu està en
 * mode espera (true) o ha deixa d'estar-ho. Aquesta funció és
 * purament informativa.
 */
typedef void (MIX_NotifyWaitingDevice) (
                                        void             *udata,
                                        const MIX_Device  dev,
                                        const bool        waiting
                                        );

/* Conté la informació necessària per a comunicar-se amb el
 * 'frontend'.
 */
typedef struct
{
  
  MIX_Warning      *warning;          /* Funció per a mostrar
        				 avisos. */
  MIX_CheckSignals *check;            // Tracta senyals i events. Pot
                                      // ser NULL.
  MIX_InitIOOPChar *init_ioopchar;    /* Inici d'operació i/o en
        				 dispositiu de caràcters. */
  MIX_InitIOOPWord *init_ioopword;    /* Inici d'operació i/o en
        				 dispositiu de paraules. */
  MIX_DeviceBusy   *device_busy;      /* Indica si un dispositiu està
        				 ocupat o no. */
  MIX_IOControl    *io_control;       /* Per a controlar els
        				 dispostius de i/o. */
  MIX_NotifyWaitingDevice *notify_waiting_device;
  
} MIX_Frontend;


/* FUNCIONS */

/* Llig la targeta del lector de targetes en la posició 0, fixa J=0 i
 * PC=0. És lo més paregut a un reset. S'ha de cridar amb la màquina
 * apagada.
 */
void
MIX_go (void);

// Inicialitza la llibreria. Per defecte la màquina està parada. Cal
// fer MIX_go per a executar-la.
void
MIX_init (
          const MIX_Frontend *frontend,
          void               *udata
          );

/* Executa cicles de MIX indicats. Aquesta funció executa almenys els
 * cicles indicats (probablement n'executarà uns pocs més). Si
 * CHECKSIGNALS en el fronted és no NULL aleshores al final de cada
 * execució crida a CHECKSIGNALS. Torna en halt true si la màquina
 * està parada. La parada es pot produir per una senyal externa que es
 * llig de CHECKSIGNALS o perquè la pròpia màquina s'ha parat. Torna
 * el número de cicles executats.
 *
 * NOTA!! La duració d'un cicle de MIX és desconegut, es pot assumir
 * el que es vullga. La màquina es comunica amb dispositius externs i
 * quan aquests estan ocupats es bloqueja.
 */
int
MIX_iter (
          const int  cc,
          MIX_Bool  *halt
          );

/* Llig caràcters de la memòria de la MIX. Emprat pels dispositius que
 * lligen caràcters. Torna el nombre de caràcters que falten per
 * llegir.
 */
size_t
MIX_read_chars (
        	MIX_Char     *to,
        	size_t        nmeb,
        	MIX_IOOPChar *op
        	);

/* Escriu caràcters en la memòria de la MIX. Emprat pels dispositius
 * que escriuen caràcters. Torna el nombre de caràcters que falten per
 * escriure.
 */
size_t
MIX_write_chars (
        	 const MIX_Char *from,
        	 size_t          nmeb,
        	 MIX_IOOPChar   *op
        	 );

/* Llig paraules de la memòria de la MIX. Emprat pels dispositius que
 * lligen paraules. Torna el nombre de paraules que falten per llegir.
 */
size_t
MIX_read_words (
        	MIX_Word     *to,
        	size_t        nmeb,
        	MIX_IOOPWord *op
        	);

/* Escriu paraules en la memòria de la MIX. Emprat pels dispositius
 * que escriuen paraules. Torna el nombre de paraules que falten per
 * escriure.
 */
size_t
MIX_write_words (
        	 const MIX_Word *from,
        	 size_t          nmeb,
        	 MIX_IOOPWord   *op
        	 );


#endif /* __MIX_H__ */
