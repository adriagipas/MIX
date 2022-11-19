#
# Copyright 2009-2022 Adrià Giménez Pastor.
#
# This file is part of adriagipas/MIX.
#
# adriagipas/MIX is free software: you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# adriagipas/MIX is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with adriagipas/MIX.  If not, see <https://www.gnu.org/licenses/>.
#
 
import sys
from array import array
from optparse import OptionParser
from functools import reduce




#############
# CONSTANTS #
#############

MASK= 0x3FFFFFFF




#########
# TIPUS #
#########

# Error.
class Error(Exception):
    
    def __init__ ( self, msg, line= None ):
        if line != None :
            self.msg= 'Línia %d: %s'%(line,msg)
        else:
            self.msg= msg
        
    def __str__ ( self ) :
        return self.msg


# Manté els símbols. És estàtica.
class Symbols:

    # Estat
    __defined_symbols= {}
    __num_local_symbols= [0]*10
    __future_references= set()

    # Comprova que un símbol és sintàcticament correcte.
    @staticmethod
    def check_sym ( sym ):
        length= len(sym)
        if length < 1 or length > 10 : err= True
        else:
            err= False
            count= 0
            for c in sym:
                if c >= 'A' and c <= 'Z' : count+= 1
                elif c < '0' or c > '9'  : err= True; break
            if count == 0 : err= True
        if err : raise Error ( "'%s' no és un símbol vàlid"%sym )
    
    # Afegeix un símbol sintàcticament correcte.
    @classmethod
    def add ( cls, sym, line, addr ):
        if cls.__defined_symbols.get ( sym ) != None :
            raise Error ( "redefinició del símbol '%s'"%sym, line )
        if len(sym) == 2 and sym[1] == 'H' and sym[0] >= '0' and sym[0] <= '9' :
            aux= int(sym[0])
            cls.__num_local_symbols[aux]+= 1
            sym= '%d@%d'%(aux,cls.__num_local_symbols[aux])
        cls.__defined_symbols[sym]= addr
        
    # Afegeix una futura referència sintàcticament correcta.
    @classmethod
    def add_fref ( cls, sym ):
        if len(sym) == 2 and sym[1] == 'F' and sym[0] >= '0' and sym[0] <= '9' :
            aux= int(sym[0])
            sym= '%d@%d'%(aux,cls.__num_local_symbols[aux]+1)
        cls.__future_references.add ( sym )
        return sym
    
    # Torna el valor numèric associat a un símbol definit o None si no
    # està definit.
    @classmethod
    def defsym2num ( cls, token ):
        if len(token) == 2 and token[0] >= '0' and \
           token[0] <= '9' and token[1] == 'B' :
            dig= int(token[0])
            aux= cls.__num_local_symbols[dig]
            if aux == 0 : return None
            return cls.__defined_symbols.get ( '%d@%d'%(dig,aux) )
        return cls.__defined_symbols.get ( token )

    # Torna una llista amb tots les referències futures que no són
    # símbols definits.
    @classmethod
    def get_frefs ( cls ):
        return [ x for x in cls.__future_references
                 if not x in cls.__defined_symbols ]
    
    # Torna cert si un símbol és una referència futura.
    @classmethod
    def is_fref ( cls, sym ):
        return sym in cls.__future_references

    # Torna el valor associat a un símbol definit.
    @classmethod
    def get_wvalue ( cls, sym ):
        return cls.__defined_symbols.get ( sym )
    
# Representa el valor d'una fila: LOC OP ADDRESS. Utilitza el valor
# None per indicar camp buit.
class Tuple:

    # Camps F i C associats a una instrucció. El camp 'mandatory' a
    # cert indica que el valor de F és l'únic possible per a eixa
    # instrucció.
    class CF:

        def __init__ ( self, C, F, mandatory= False ):
            self.C= C
            self.F= F
            self.mandatory= mandatory

        def __str__ ( self ):
            return '(C:%d,F:%d,%s)'%(self.C,self.F,self.mandatory)
    
    # Operacions especials que no tenen CF.
    EQU= 0
    ORIG= 1
    CON= 2
    ALF= 3
    END= 4
                 
    __opwords= { 'NOP'  : CF ( 0, 0 ),
                 'ADD'  : CF ( 1, 5 ),
                 'SUB'  : CF ( 2, 5 ),
                 'MUL'  : CF ( 3, 5 ),
                 'DIV'  : CF ( 4, 5 ),
                 'NUM'  : CF ( 5, 0, True ),
                 'CHAR' : CF ( 5, 1, True ),
                 'HLT'  : CF ( 5, 2, True ),
                 'SLA'  : CF ( 6, 0, True ),
                 'SRA'  : CF ( 6, 1, True ),
                 'SLAX' : CF ( 6, 2, True ),
                 'SRAX' : CF ( 6, 3, True ),
                 'SLC'  : CF ( 6, 4, True ),
                 'SRC'  : CF ( 6, 5, True ),
                 'MOVE' : CF ( 7, 1 ),
                 'LDA'  : CF ( 8, 5 ),
                 'LD1'  : CF ( 9, 5 ),
                 'LD2'  : CF ( 10, 5 ),
                 'LD3'  : CF ( 11, 5 ),
                 'LD4'  : CF ( 12, 5 ),
                 'LD5'  : CF ( 13, 5 ),
                 'LD6'  : CF ( 14, 5 ),
                 'LDX'  : CF ( 15, 5 ),
                 'LDAN' : CF ( 16, 5 ),
                 'LD1N' : CF ( 17, 5 ),
                 'LD2N' : CF ( 18, 5 ),
                 'LD3N' : CF ( 19, 5 ),
                 'LD4N' : CF ( 20, 5 ),
                 'LD5N' : CF ( 21, 5 ),
                 'LD6N' : CF ( 22, 5 ),
                 'LDXN' : CF ( 23, 5 ),
                 'STA'  : CF ( 24, 5 ),
                 'ST1'  : CF ( 25, 5 ),
                 'ST2'  : CF ( 26, 5 ),
                 'ST3'  : CF ( 27, 5 ),
                 'ST4'  : CF ( 28, 5 ),
                 'ST5'  : CF ( 29, 5 ),
                 'ST6'  : CF ( 30, 5 ),
                 'STX'  : CF ( 31, 5 ),
                 'STJ'  : CF ( 32, 2 ),
                 'STZ'  : CF ( 33, 5 ),
                 'JBUS' : CF ( 34, 0 ),
                 'IOC'  : CF ( 35, 0 ),
                 'IN'   : CF ( 36, 0 ),
                 'OUT'  : CF ( 37, 0 ),
                 'JRED' : CF ( 38, 0 ),
                 'JMP'  : CF ( 39, 0, True ),
                 'JSJ'  : CF ( 39, 1, True ),
                 'JOV'  : CF ( 39, 2, True ),
                 'JNOV' : CF ( 39, 3, True ),
                 'JL'   : CF ( 39, 4, True ),
                 'JE'   : CF ( 39, 5, True ),
                 'JG'   : CF ( 39, 6, True ),
                 'JGE'  : CF ( 39, 7, True ),
                 'JNE'  : CF ( 39, 8, True ),
                 'JLE'  : CF ( 39, 9, True ),
                 'JAN'  : CF ( 40, 0, True ),
                 'JAZ'  : CF ( 40, 1, True ),
                 'JAP'  : CF ( 40, 2, True ),
                 'JANN' : CF ( 40, 3, True ),
                 'JANZ' : CF ( 40, 4, True ),
                 'JANP' : CF ( 40, 5, True ),
                 'J1N'  : CF ( 41, 0, True ),
                 'J1Z'  : CF ( 41, 1, True ),
                 'J1P'  : CF ( 41, 2, True ),
                 'J1NN' : CF ( 41, 3, True ),
                 'J1NZ' : CF ( 41, 4, True ),
                 'J1NP' : CF ( 41, 5, True ),
                 'J2N'  : CF ( 42, 0, True ),
                 'J2Z'  : CF ( 42, 1, True ),
                 'J2P'  : CF ( 42, 2, True ),
                 'J2NN' : CF ( 42, 3, True ),
                 'J2NZ' : CF ( 42, 4, True ),
                 'J2NP' : CF ( 42, 5, True ),
                 'J3N'  : CF ( 43, 0, True ),
                 'J3Z'  : CF ( 43, 1, True ),
                 'J3P'  : CF ( 43, 2, True ),
                 'J3NN' : CF ( 43, 3, True ),
                 'J3NZ' : CF ( 43, 4, True ),
                 'J3NP' : CF ( 43, 5, True ),
                 'J4N'  : CF ( 44, 0, True ),
                 'J4Z'  : CF ( 44, 1, True ),
                 'J4P'  : CF ( 44, 2, True ),
                 'J4NN' : CF ( 44, 3, True ),
                 'J4NZ' : CF ( 44, 4, True ),
                 'J4NP' : CF ( 44, 5, True ),
                 'J5N'  : CF ( 45, 0, True ),
                 'J5Z'  : CF ( 45, 1, True ),
                 'J5P'  : CF ( 45, 2, True ),
                 'J5NN' : CF ( 45, 3, True ),
                 'J5NZ' : CF ( 45, 4, True ),
                 'J5NP' : CF ( 45, 5, True ),
                 'J6N'  : CF ( 46, 0, True ),
                 'J6Z'  : CF ( 46, 1, True ),
                 'J6P'  : CF ( 46, 2, True ),
                 'J6NN' : CF ( 46, 3, True ),
                 'J6NZ' : CF ( 46, 4, True ),
                 'J6NP' : CF ( 46, 5, True ),
                 'JXN'  : CF ( 47, 0, True ),
                 'JXZ'  : CF ( 47, 1, True ),
                 'JXP'  : CF ( 47, 2, True ),
                 'JXNN' : CF ( 47, 3, True ),
                 'JXNZ' : CF ( 47, 4, True ),
                 'JXNP' : CF ( 47, 5, True ),
                 'INCA' : CF ( 48, 0, True ),
                 'DECA' : CF ( 48, 1, True ),
                 'ENTA' : CF ( 48, 2, True ),
                 'ENNA' : CF ( 48, 3, True ),
                 'INC1' : CF ( 49, 0, True ),
                 'DEC1' : CF ( 49, 1, True ),
                 'ENT1' : CF ( 49, 2, True ),
                 'ENN1' : CF ( 49, 3, True ),
                 'INC2' : CF ( 50, 0, True ),
                 'DEC2' : CF ( 50, 1, True ),
                 'ENT2' : CF ( 50, 2, True ),
                 'ENN2' : CF ( 50, 3, True ),
                 'INC3' : CF ( 51, 0, True ),
                 'DEC3' : CF ( 51, 1, True ),
                 'ENT3' : CF ( 51, 2, True ),
                 'ENN3' : CF ( 51, 3, True ),
                 'INC4' : CF ( 52, 0, True ),
                 'DEC4' : CF ( 52, 1, True ),
                 'ENT4' : CF ( 52, 2, True ),
                 'ENN4' : CF ( 52, 3, True ),
                 'INC5' : CF ( 53, 0, True ),
                 'DEC5' : CF ( 53, 1, True ),
                 'ENT5' : CF ( 53, 2, True ),
                 'ENN5' : CF ( 53, 3, True ),
                 'INC6' : CF ( 54, 0, True ),
                 'DEC6' : CF ( 54, 1, True ),
                 'ENT6' : CF ( 54, 2, True ),
                 'ENN6' : CF ( 54, 3, True ),
                 'INCX' : CF ( 55, 0, True ),
                 'DECX' : CF ( 55, 1, True ),
                 'ENTX' : CF ( 55, 2, True ),
                 'ENNX' : CF ( 55, 3, True ),
                 'CMPA' : CF ( 56, 5  ),
                 'CMP1' : CF ( 57, 5 ),
                 'CMP2' : CF ( 58, 5 ),
                 'CMP3' : CF ( 59, 5 ),
                 'CMP4' : CF ( 60, 5 ),
                 'CMP5' : CF ( 61, 5 ),
                 'CMP6' : CF ( 62, 5 ),
                 'CMPX' : CF ( 63, 5 ),
                 'EQU'  : EQU,
                 'ORIG' : ORIG,
                 'CON'  : CON,
                 'ALF'  : ALF,
                 'END'  : END }

    # Comprova si loc és un símbol sintàcticament correcte.
    @staticmethod
    def __check_loc ( loc ):
        if loc == None : return
        Symbols.check_sym ( loc )
    
    # Crea una tupla a partir d'una seqüència de tokens. Comprova que
    # el valor de OP és vàlid, i que el de LOC és sintàcticament
    # correcte.
    @staticmethod
    def create_from_tokens ( tokens, line ):
        length= len(tokens)
        if length == 3 :
            loc,op,addr= tokens[0],tokens[1],tokens[2]
        elif length == 2 :
            if tokens[0] in iter(Tuple.__opwords.keys()) :
                loc,op,addr= None,tokens[0],tokens[1]
            else:
                loc,op,addr= tokens[0],tokens[1],None
        elif length == 1 :
            loc,op,addr= None,tokens[0],None
        else:
            raise Error ( 'massa columnes: %d'%length, line )
        if not op in iter(Tuple.__opwords.keys()) :
            raise Error ( 'operació desconeguda: %s'%op, line )
        Tuple.__check_loc ( loc )
        return Tuple ( loc, Tuple.__opwords[op], addr, line )

    # Crea una tupla a partir dels valors indicats. No fa ninguna
    # comprobació.
    def __init__ ( self, loc, op, addr, line ):
        self.loc= loc
        self.op= op
        self.addr= addr
        self.line= line

    def __repr__ ( self ):
        return '<LOC:%s OP:%s ADDR:%s LINE:%d>'%(self.loc,self.op,
                                                 self.addr,self.line)





# Memòria amb les dades compilades. És estàtica.
class Mem:

    # Estat
    __mem= []
    for i in range(0,4000):
        __mem.append ( array ( 'i', [0,0,0,0,0,0] ) )
    __min= 4000
    __max= -1
    __modified= [False]*4000
    __empty= True
    __start= 0
    __lines= [None]*4000
    
    # Modes
    ASCII= 0
    PUNCHCARD= 1
    DECK= 2

    # Fixa el valor de start.
    @classmethod
    def set_start ( cls, start, line= None ):
        if start >= 4000 :
            raise Error ( 'adreça inicial fora de rang: %d'%start, line )
        cls.__start= start
    
    # Fixa el contingut d'una paraula
    @classmethod
    def set ( cls, addr, value, line= None ):
        if addr < 0 or addr >= 4000 :
            raise Error ( 'adreça fora de rang: %d'%addr, line )
        cls.__lines[addr]= line
        if addr < cls.__min : cls.__min= addr
        if addr > cls.__max : cls.__max= addr
        cls.__modified[addr]= True
        cls.__empty= False
        aux= cls.__mem[addr]
        aux[0]= (value<0)
        value= abs(value)
        i= 5
        while i != 0:
            aux[i]= value&0x3F
            value>>= 6
            i-= 1
    
    # Imprimeix el contingut de la memòria per pantalla
    @classmethod
    def write ( cls, f, mode= ASCII ):
        if mode == cls.ASCII :
            if cls.__min > cls.__max : return
            f.write ( 'START: %d\n'%cls.__start )
            if not cls.__empty :
                f.write ( '     ------------------\n' )
            for i in range(0,4000):
                if cls.__modified[i] :
                    f.write (('%04d |%s|%02d|%02d|%02d|%02d|%02d|\n')%
                             (i,
                              '-' if cls.__mem[i][0] else '+',
                              cls.__mem[i][1],
                              cls.__mem[i][2],cls.__mem[i][3],
                              cls.__mem[i][4],cls.__mem[i][5]))
                    f.write ( '     ------------------\n' )
        elif mode == cls.PUNCHCARD :
            if cls.__min > cls.__max : return
            if cls.__start != 0 :
                raise Error ( "el codi no comença en l'adreça 0" )
            buffer= ''
            count= 0
            for i in range(cls.__min,cls.__max+1):
                for b in cls.__mem[i][1:]:
                    if b == 20 or b == 21 or b >= 48 :
                        raise Error ( "no es pot codificar el valor %d"%b )
                    buffer+= IChars[b]
                count+= 1
                if count == 16 :
                    f.write ( buffer.rstrip()+'\n' )
                    buffer= ''
                    count= 0
            if count > 0 : f.write ( buffer.rstrip()+'\n' )
        elif mode == Mem.DECK :
            overpunch= { '0':'&', '1':'J', '2':'K', '3':'L', '4':'M',
                         '5':'N', '6':'O', '7':'P', '8':'Q', '9':'R' }
            if cls.__min > cls.__max : return
            f.write ( ' O O6 Z O6    I C O4 0 EH A  F F CF 0  E   '+
                      'EU 0 IH G BB   EJ  CA. Z EU   EH E BA\n' )
            f.write ( '   EU 2A-H S BB  C U 1AEH 2AEN V  E  CLU  A'+
                      'BG Z EH E BB J B. A  9\n' )
            begin= cls.__min
            ind= 1
            while begin <= cls.__max :
                while not cls.__modified[begin]:
                    begin+= 1
                if begin <= 100 :
                    sys.stderr.write ( ('Avís: paraula amb adreça menor'+
                                        ' o igual a 100: %d\n')%begin )
                end= begin; count= 0
                while count < 7 and cls.__modified[end] and end < 4000:
                    count+= 1
                    end+= 1
                line= '%04d %d%04d'%(ind,count,begin)
                for i in range(begin,end):
                    aux= cls.__mem[i]
                    value= (((((((aux[1]<<6)|aux[2])<<6)|
                               aux[3])<<6)|aux[4])<<6)|aux[5]
                    value= '%010d'%value
                    if aux[0] : line+= value[:-1]+overpunch[value[-1]]
                    else      : line+= value
                f.write ( line+'\n' )
                ind+= 1
                begin= end
            f.write ( 'TRANS0%04d\n'%cls.__start )


# Caràcters '_' representa l'espai.
Chars= { '_' : 0,
         'A' : 1,
         'B' : 2,
         'C' : 3,
         'D' : 4,
         'E' : 5,
         'F' : 6,
         'G' : 7,
         'H' : 8,
         'I' : 9,
         'Δ' : 10,
         'J' : 11,
         'K' : 12,
         'L' : 13,
         'M' : 14,
         'N' : 15,
         'O' : 16,
         'P' : 17,
         'Q' : 18,
         'R' : 19,
         'Σ' : 20,
         'Π' : 21,
         'S' : 22,
         'T' : 23,
         'U' : 24,
         'V' : 25,
         'W' : 26,
         'X' : 27,
         'Y' : 28,
         'Z' : 29,
         '0' : 30,
         '1' : 31,
         '2' : 32,
         '3' : 33,
         '4' : 34,
         '5' : 35,
         '6' : 36,
         '7' : 37,
         '8' : 38,
         '9' : 39,
         '.' : 40,
         ',' : 41,
         '(' : 42,
         ')' : 43,
         '+' : 44,
         '-' : 45,
         '*' : 46,
         '/' : 47,
         '=' : 48,
         '$' : 49,
         '<' : 50,
         '>' : 51,
         '@' : 52,
         ';' : 53,
         ':' : 54,
         "'" : 55 }
IChars= {}
for k,v in Chars.items():
    IChars[v]= k
IChars[0]= ' '
IChars[10]= '&'




############
# FUNCIONS #
############

# Processa el fitxer carregant les entrades
def read_tuples ( input_fn ):
    f= sys.stdin if input_fn == "" else open ( input_fn )
    code= []
    num_line= 0
    l= f.readline()
    while l != "":
        line= l.split()
        l= f.readline()
        num_line+= 1
        if len(line) == 0 or line[0] == '*' : continue
        code.append ( Tuple.create_from_tokens ( line, num_line ) )
    if input_fn != "" : f.close()
    return code


# Tokeniza un camp adreça.
def tokenize_addr ( addr, line ):
    length= len(addr)
    res= []
    i= 0
    while i < length:
        c= addr[i]
        if c == '+' or c == '-' or c == '*' or c == ':' or \
           c == '(' or c == ')' or c == ',' or c == '=' :
            res.append ( c )
            i+= 1
        elif c == '/' :
            i+= 1
            c= addr[i]
            if i < length and c == '/' :
                res.append ( '//' )
                i+= 1
            else: res.append ( '/' )
        elif (c>='0' and c<='9') or (c>='A' and c<='Z'):
            token= c
            j= i+1
            while j < length and ((addr[j]>= '0' and addr[j]<='9') or \
                  (addr[j]>='A' and addr[j]<='Z')):
                token+= addr[j]
                j+= 1
            res.append ( token )
            i= j
        else: raise Error ( 'caràcter no vàlid: %c'%c, line )
    return res


# Comprova si un token és un número. Torna el número si ho és o None
# si no ho és.
def number ( token ):
    length= len(token)
    if length < 1 or length > 10 : return None
    if token.isdigit() : return int(token)&MASK
    else               : return None


# Comprova si un token és una expressió atòmica. Si ho és torna el
# número associat, None en cas contrari.
def atomic_expression ( token, ast ):
    if token == '*' : return ast&MASK
    num= number ( token )
    if num != None : return num
    return Symbols.defsym2num ( token )
    

# Comprova si una llista de tokens és una expressió. Si ho és torna el
# número associat, None en cas contrari
def expression ( tokens, ast ):
    length= len(tokens)
    if length == 1 :
        return atomic_expression ( tokens[0], ast )
    elif length == 2 :
        sign= tokens[0]
        if sign == '+' :
            return atomic_expression ( tokens[1], ast )
        elif sign == '-' :
            return -atomic_expression ( tokens[1], ast )
        else: return None
    else:
        lval= expression ( tokens[:-2], ast )
        if lval == None: return None
        rval= atomic_expression ( tokens[-1], ast )
        if rval == None: return None
        op= tokens[-2]
        if   op == '+'  : res= lval+rval
        elif op == '-'  : res= lval-rval
        elif op == '*'  : res= lval*rval
        elif op == '/'  : res= lval/rval
        elif op == '//' : res= (lval<<30)/rval
        elif op == ':'  : res= (lval<<3)+rval
        else: return None
        if res < 0 : return -((-res)&MASK)
        else       : return res&MASK


# Calcula el valor d'un w_value senzill a partir d'un valor ja
# inicial.
def w_value_base ( tokens, ast, base ):
    if tokens[-1] != ')' : return expression ( tokens, ast )
    length= len(tokens)
    i= 0
    while i < length and tokens[i] != '(': i+= 1
    if i == 0 or i == length : return None
    E= expression ( tokens[:i], ast )
    if E == None : return None
    F= expression ( tokens[i+1:-1], ast )
    if F == None or F < 0 : return None
    L= F>>3; R= F&0x7
    if L > 5 or R > 5 or L > R : return None
    isneg= (base<0); data= abs(base)
    if L == 0 :
        isneg= (E<0)
        L= 1
    value= abs(E)
    mask= (MASK>>(6*(L-1)))&(MASK<<(6*(5-R)))
    value<<= 6*(5-R)
    data= (data&(~mask)) | (value&mask)
    if isneg : data= -data
    return data


# Funció recursiva per a w_value.
def w_value_rec ( tokens, ast, base ):
    length= len(tokens)
    i= 0
    while i < length and tokens[i] != ',': i+= 1
    if i == 0 or i == length-1 : return None
    aux= w_value_base ( tokens[:i], ast, base )
    if i == length or aux == None : return aux
    return w_value_rec ( tokens[i+1:], ast, aux )


# Torna un valor d'una constat MIX que cap en una paraula, a partir
# d'una llista de tokens. Es supossa no buida. Torna None si no és una
# expressió vàlida.
def w_value ( tokens, ast ):
    return w_value_rec ( tokens, ast, 0 )


def a_value ( tokens, ast, code ):
    aux= expression ( tokens, ast )
    if aux != None : return aux
    length= len(tokens)
    if length == 1 :
        Symbols.check_sym ( tokens[0] )
        return Symbols.add_fref ( tokens[0] )
    elif length >= 3 and tokens[0] == '=' and tokens[-1] == '=' :
        aux= reduce ( lambda x,y:x+y, tokens[1:-1] )
        if len(aux) > 10 : return None
        val= w_value ( tokens[1:length-1], ast )
        sym= '=%d='%val
        if not Symbols.is_fref ( sym ) :
            code.append ( Tuple ( sym, Tuple.CON, str(val), -1 ) )
            Symbols.add_fref ( sym )
        return sym
    else: return None


# Torna el camp adreça, índex i F d'una instrucció MIX. Torna una
# tupla de tres elements, si la sintaxi no és correcta el primer camp
# és None, si és correcta però és una referència futura el primer camp
# és un string, en cas contrari el primer camp és un sencer
# representant l'adreça. El valor del camp F a None indica valor per
# defecte.
def aif_value ( tokens, ast, code ):
    length= len(tokens)
    i= 0
    while i < length and tokens[i] != ',' and tokens[i] != '(' : i+= 1
    if i == length-1: return None,None,None
    if i == 0 : aval= 0
    else      : aval= a_value ( tokens[:i], ast, code )
    if i < length and tokens[i] == ',' :
        i+= 1; j= i
        while j < length and tokens[j] != '(' : j+= 1
        if j == i or j == length-1: return None,None,None
        ival= expression ( tokens[i:j], ast )
    else:
        ival= 0
        j= i
    if j < length :
        if j == length-2 or tokens[-1] != ')' : return None,None,None
        fval= expression ( tokens[j+1:-1], ast )
    else: fval= None
    return aval,ival,fval
    

# Comprova que no hi han més de 2 END.
def check_end ( code ):
    count= 0
    for t in code:
        if t.op == Tuple.END :
            count+= 1
    if count > 1 :
        raise Error ( "hi han més de dos operacions END" )
    if count == 1 and code[-1].op != Tuple.END :
        raise Error ( "l'operació END no apareix al final del codi", 0 )


# Processa tot el codi excepte les línies amb referències futures. Al
# final d'aquest pas tots els símbols estan definits.
def step1 ( code ):
    check_end ( code )
    if code[-1].op == Tuple.END :
        last= code[-1]
        del code[-1]
    else: last= None
    end= len(code)
    _ast= 0
    i= 0
    while i < end:
        t= code[i]
        
        if t.op == Tuple.EQU :
            if t.addr == None :
                raise Error ( "operació EQU sense W-value", t.line )
            addr= w_value ( tokenize_addr ( t.addr, t.line ), _ast )
            if addr == None :
                raise Error ( "'%s' no és un W-value vàlid"%t.addr,
                              t.line )
            if t.loc != None :
                Symbols.add ( t.loc, t.line, addr )
            del code[i]; end-= 1
            
        elif t.op == Tuple.ORIG :
            if t.loc != None :
                Symbols.add ( t.loc, t.line, _ast )
            if t.addr == None :
                raise Error ( "operació ORIG sense W-value", t.line )
            _ast= w_value ( tokenize_addr ( t.addr, t.line ), _ast )
            if _ast == None :
                raise Error ( "'%s' no és un W-value vàlid"%t.addr,
                              t.line )
            del code[i]; end-= 1

        elif t.op == Tuple.CON :
            if t.loc != None :
                Symbols.add ( t.loc, t.line, _ast )
            if t.addr == None :
                raise Error ( "operació CON sense W-value", t.line )
            value= w_value ( tokenize_addr ( t.addr, t.line ), _ast )
            if value == None :
                raise Error ( "'%s' no és un W-value vàlid"%t.addr,
                              t.line )
            Mem.set ( _ast, value, t.line )
            _ast+= 1
            del code[i]; end-= 1

        elif t.op == Tuple.ALF :
            if t.loc != None :
                Symbols.add ( t.loc, t.line, _ast )
            if t.addr == None :
                raise Error ( "operació ALF sense W-value", t.line )
            t.addr= str ( t.addr )
            if len(t.addr) != 5 :
                raise Error ( ("el número de caràcters de l'adreça"+
                               " no és cinc: %s")%t.addr, t.line )
            value= 0
            for c in t.addr:
                aux= Chars.get ( c )
                if aux == None :
                    raise Error ( ("caràcter '%s' no soportat per"+
                                   " l'alfabet de la màquina MIX"%c),
                                  t.line )
                value<<= 6
                value|= aux
            Mem.set ( _ast, value, t.line )
            _ast+= 1
            del code[i]; end-= 1

        elif isinstance(t.op,Tuple.CF) :
            if t.loc != None :
                Symbols.add ( t.loc, t.line, _ast )
            if t.addr == None :
                aval,ival,fval= 0,0,None
            else:
                aval,ival,fval= aif_value ( tokenize_addr ( t.addr, t.line ),
                                            _ast, code )
                end= len(code)
            if aval == None or ival == None:
                raise Error ( "adreça '%s' no vàlida"%t.addr, t.line )
            if fval == None : fval= t.op.F
            elif (t.op.mandatory and t.op.F != fval) or fval >= 64 :
                raise Error ( ("l'operació %d no suporta"+
                               " el camp F=%d"%(t.op,t.op.F)),
                              t.line )
            if ival >= 64 :
                raise Error ( "l'índex de '%s' està fora de rang"%t.addr,
                              t.line )
            if type(aval) == str :
                t.aval,t.ival,t.fval= aval,ival,fval
                t.ast= _ast
                _ast+= 1
                i+= 1
                continue
            if aval <= -4096 or aval >= 4096 :
                raise Error ( "l'adreça de '%s' està fora de rang"%t.addr,
                              t.line )
            value= (((((abs(aval)<<6)|ival)<<6)|fval)<<6)|t.op.C
            if aval < 0 : value= -value
            Mem.set ( _ast, value, t.line )
            _ast+= 1
            del code[i]; end-= 1
            
        else: i+= 1

    # Crea els símbols no definits.
    frefs= Symbols.get_frefs()
    for s in frefs:
        if last == None or last.loc != s :
            Symbols.add ( s, None, _ast )
            Mem.set ( _ast, 0, None )
            _ast+= 1

    # Símbol END.
    if last != None :
        if last.loc != None :
            Symbols.add ( last.loc, last.line, _ast )
        if last.addr == None :
            raise Error ( "operació END sense W-value", last.line )
        aux= w_value ( tokenize_addr ( last.addr, last.line ), _ast )
        if aux == None :
            raise Error ( "'%s' no és un W-value vàlid"%last.addr,
                          last.line )
        Mem.set_start ( abs(aux)&0xFFF, last.line )
    
# end


# Processa les instruccions que tenien referències futures.
def step2 ( code ):
    for t in code:
        aval= Symbols.get_wvalue ( t.aval )
        if aval <= -4096 or aval >= 4096 :
                raise Error ( "l'adreça de '%s' està fora de rang"%t.addr,
                              t.line )
        value= (((((abs(aval)<<6)|t.ival)<<6)|t.fval)<<6)|t.op.C
        if aval < 0 : value= -value
        Mem.set ( t.ast, value, t.line )


########
# MAIN #
########

# Processa línia d'arguments
parser= OptionParser( usage= "mixala.py < <input> > <output>",
                      version= "mixala.py 1.0" )
parser.add_option ( "-i", "--input", action= "store",
                    type= "string", dest= "input",
                    default= "", metavar= "INPUT",
                    help= "Fitxer d'entrada" )
parser.add_option ( "-o", "--output", action= "store",
                    type= "string", dest= "output",
                    default= "", metavar= "OUTPUT",
                    help= "Fitxer d'eixida" )
parser.add_option ( "-m", "--mode", action= "store",
                    type= "choice", dest= "mode",
                    default= "DECK", choices= ["ASCII","PUNCHCARD","DECK"],
                    metavar= "MODE",
                    help=
                    "Especifica quin tipus d'eixida a de generar:"+
                    "                                            "+
                    "  ASCII: una representació en ASCII de les instruccions "+
                    " compilades en memòria."+
                    "                                                      "+
                    "  PUNCHCARD: en format PUNCHCARD preparat per a ser"+
                    " llegit per la màquina MIX quan s'engega la"+
                    " màquina amb el botó GO. Típicament per al loader"+
                    "                                                    "+
                    "  DECK: preparat per a ser executat per la màquina MIX"+
                    " l'única restricció és que les adreces siguen major que"+
                    " 100. Típicament per a codi que va després del loader" )
(opts, args)= parser.parse_args()
if opts.mode == 'ASCII' :
    mode= Mem.ASCII
elif opts.mode == 'PUNCHCARD' :
    mode= Mem.PUNCHCARD
elif opts.mode == 'DECK' :
    mode= Mem.DECK

# Cos
try:
    code= read_tuples ( opts.input )
    step1 ( code )
    step2 ( code )
    f= sys.stdout if opts.output == "" else open ( opts.output, 'w' )
    Mem.write ( f, mode )
    if f != sys.stdout : f.close()
except Exception as msg:
    sys.exit ( 'Error: %s.'%msg )
