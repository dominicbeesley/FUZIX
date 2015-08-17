#ifndef __TERMCAP_H
#define __TERMCAP_H
#ifndef __TYPES_H
#include <types.h>
#endif

extern char PC;
extern char *UP;
extern char *BC;
extern int ospeed;

extern int tgetent __P((char *, char *));
extern int tgetflag __P((char *));
extern int tgetnum __P((char *));
extern char *tgetstr __P((char *, char **));

extern int tputs __P((char *, int, void (*outc)(int)));
extern char *tgoto __P((char *, int, int));
extern char * tparam(char *, char *, int, int, int, int, int);	/* VARARGS */

#endif /* _TERMCAP_H */
