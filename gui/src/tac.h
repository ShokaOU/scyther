#ifndef TAC_H
#define TAC_H

#include "symbol.h"

/*
 * TAC instructions
 */

enum tactypes
{
  TAC_UNDEF,
  TAC_SYM,
  TAC_TUPLE,
  TAC_ENCRYPT,
  TAC_VAR,
  TAC_CONST,
  TAC_READ,
  TAC_SEND,
  TAC_CLAIM,
  TAC_FUNC,
  TAC_STRING,
  TAC_ROLE,
  TAC_PROTOCOL,
  TAC_KNOWS,
  TAC_RUN,
  TAC_ROLEREF,
  TAC_SECRET,
  TAC_INVERSEKEYS,
  TAC_UNTRUSTED,
  TAC_COMPROMISED,
  TAC_USERTYPE
};

//! Structure to hold the compilation tree nodes
struct tacnode
{
  struct tacnode *next;		//!< pointer to previous node
  struct tacnode *prev;		//!< pointer to next node
  struct tacnode *allnext;
  int op;			//!< operator for this node
  int lineno;			//!< line number of parser location in the input file
  union
  {
    Symbol sym;
    struct tacnode *tac;
    char *str;
    int value;
  } t1;
  union
  {
    Symbol sym;
    struct tacnode *tac;
    char *str;
    int value;
  } t2;
  union
  {
    Symbol sym;
    struct tacnode *tac;
    char *str;
    int value;
  } t3;
};

typedef struct tacnode *Tac;

void tacInit (void);
void tacDone (void);
Tac tacCreate (int op);
Tac tacSymb (char *s);
Tac tacJoin (int op, Tac t1, Tac t2, Tac t3);
Tac tacTuple (Tac taclist);
Tac tacCat (Tac t1, Tac t2);
void tacPrint (Tac t);

#endif