/* bits.c -- manage creation and output of bit sets used by the parser.
 *
 * SOFTWARE RIGHTS
 *
 * We reserve no LEGAL rights to the Purdue Compiler Construction Tool
 * Set (PCCTS) -- PCCTS is in the public domain.  An individual or
 * company may do whatever they wish with source code distributed with
 * PCCTS or the code generated by PCCTS, including the incorporation of
 * PCCTS, or its output, into commerical software.
 *
 * We encourage users to develop software with PCCTS.  However, we do ask
 * that credit is given to us for developing PCCTS.  By "credit",
 * we mean that if you incorporate our source code into one of your
 * programs (commercial product, research project, or otherwise) that you
 * acknowledge this fact somewhere in the documentation, research report,
 * etc...  If you like PCCTS and have developed a nice tool with the
 * output, please mention that you developed it using PCCTS.  In
 * addition, we ask that this header remain intact in our source code.
 * As long as these guidelines are kept, we expect to continue enhancing
 * this system and expect to make other tools available as they are
 * completed.
 *
 * ANTLR 1.33
 * Terence Parr
 * Parr Research Corporation
 * with Purdue University and AHPCRC, University of Minnesota
 * 1989-2001
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include "pcctscfg.h"
#include "set.h"
#include "syn.h"
#include "hash.h"
#include "generic.h"
#include "dlgdef.h"

/* char is only thing that is pretty much always known == 8 bits
 * This allows output of antlr (set stuff, anyway) to be androgynous (portable)
 */
typedef unsigned char SetWordType;
#define BitsPerByte		8
#define BitsPerWord		BitsPerByte*sizeof(SetWordType)

static SetWordType *setwd = NULL;
int setnum = -1;
int wordnum = 0;

int esetnum = 0;

/* Used to convert native wordsize, which ANTLR uses (via set.c) to manipulate sets,
   to bytes that are most portable size-wise.
   */
void
#ifdef __USE_PROTOS
DumpIntAsChars( FILE *f, char *format, unsigned wd )
#else
DumpIntAsChars( f, format, wd )
FILE *f;
char *format;
unsigned wd;
#endif
{
	int i;
	/* uses max of 32 bit unsigned integer for the moment */
	static unsigned long byte_mask[sizeof(unsigned long)] =
				{ 0xFF, 0xFF00UL, 0xFF0000UL, 0xFF000000UL };  /* MR20 G. Hobbelt */
/*				  0xFF00000000, 0xFF0000000000, 0xFF000000000000, 0xFF00000000000000 };*/

	/* for each byte in the word */
	assert(sizeof(unsigned) <= 4); /* M20 G. Hobbelt Sanity check */
	for (i=0; i<sizeof(unsigned); i++)
	{
		/* mask out the ith byte and shift down to the first 8 bits */
		fprintf(f, format, (wd&byte_mask[i])>>(i*BitsPerByte));
		if ( i<sizeof(unsigned)-1) fprintf(f, ",");
	}
}

/* Create a new setwd (ignoring [Ep] token on end) */
void
#ifdef __USE_PROTOS
NewSetWd( void )
#else
NewSetWd( )
#endif
{
	SetWordType *p;

	if ( setwd == NULL )
	{
		setwd = (SetWordType *) calloc(TokenNum, sizeof(SetWordType));
		require(setwd!=NULL, "NewSetWd: cannot alloc set wd\n");
	}
	for (p = setwd; p<&(setwd[TokenNum]); p++)  {*p=0;}
	wordnum++;
}

void
#ifdef __USE_PROTOS
DumpSetWd( void )
#else
DumpSetWd( )
#endif
{
	if ( GenCC ) DumpSetWdForCC();
	else DumpSetWdForC();
}

/* Dump the current setwd to ErrFile. 0..MaxTokenVal */
void
#ifdef __USE_PROTOS
DumpSetWdForC( void )
#else
DumpSetWdForC( )
#endif
{
	int i,c=1;

	if ( setwd==NULL ) return;
	fprintf(DefFile, "extern SetWordType setwd%d[];\n", wordnum);
	fprintf(ErrFile,
			"SetWordType setwd%d[%d] = {", wordnum, TokenNum-1);
	for (i=0; i<TokenNum-1; i++)
	{
		DAWDLE;
		if ( i!=0 ) fprintf(ErrFile, ",");
		if ( c == 8 ) {fprintf(ErrFile, "\n\t"); c=1;} else c++;
		fprintf(ErrFile, "0x%x", setwd[i]);
	}
	fprintf(ErrFile, "};\n");
}

/* Dump the current setwd to Parser.C file. 0..MaxTokenVal;
 * Only used if -CC on.
 */
void
#ifdef __USE_PROTOS
DumpSetWdForCC( void )
#else
DumpSetWdForCC( )
#endif
{
	int i,c=1;

	if ( setwd==NULL ) return;
	fprintf(Parser_h, "\tstatic SetWordType setwd%d[%d];\n", wordnum, TokenNum-1);
	fprintf(Parser_c,
			"SetWordType %s::setwd%d[%d] = {", CurrentClassName, wordnum,
			TokenNum-1);
	for (i=0; i<TokenNum-1; i++)
	{
		DAWDLE;
		if ( i!=0 ) fprintf(Parser_c, ",");
		if ( c == 8 ) {fprintf(Parser_c, "\n\t"); c=1;} else c++;
		fprintf(Parser_c, "0x%x", setwd[i]);
	}
	fprintf(Parser_c, "};\n");
}

/* Make a new set.  Dump old setwd and create new setwd if current setwd is full */
void
#ifdef __USE_PROTOS
NewSet( void )
#else
NewSet( )
#endif
{
	setnum++;
	if ( setnum==BitsPerWord )		/* is current setwd full? */
	{
		DumpSetWd(); NewSetWd(); setnum = 0;
	}
}

/* s is a set of tokens.  Turn on bit at each token position in set 'setnum' */
void
#ifdef __USE_PROTOS
FillSet( set s )
#else
FillSet( s )
set s;
#endif
{
	SetWordType mask=(((unsigned)1)<<setnum);
	unsigned int e;

	while ( !set_nil(s) )
	{
		e = set_int(s);
		set_rm(e, s);
		setwd[e] |= mask;
	}
}

					/* E r r o r  C l a s s  S t u f f */

/* compute the FIRST of a rule for the error class stuff */
static set
#ifdef __USE_PROTOS
Efirst( char *rule, ECnode *eclass )
#else
Efirst( rule, eclass )
char *rule;
ECnode *eclass;
#endif
{
	set rk, a;
	Junction *r;
	RuleEntry *q = (RuleEntry *) hash_get(Rname, rule);

	if ( q == NULL )
	{
		warnNoFL(eMsg2("undefined rule '%s' referenced in errclass '%s'; ignored",
						rule, TokenString(eclass->tok)));
		return empty;
	}
	r = RulePtr[q->rulenum];
	r->end->halt = TRUE;		/* don't let reach fall off end of rule here */
	rk = empty;
	REACH(r, 1, &rk, a);
	r->end->halt = FALSE;
	return a;
}

/*
 * scan the list of tokens/eclasses/nonterminals filling the new eclass
 * with the set described by the list.  Note that an eclass can be
 * quoted to allow spaces etc... However, an eclass must not conflict
 * with a reg expr found elsewhere.  The reg expr will be taken over
 * the eclass name.
 */
static void
#ifdef __USE_PROTOS
doEclass( char *eclass )
#else
doEclass( eclass )
char *eclass;
#endif
{
	TermEntry *q;
	ECnode *p;
	TCnode *tcnode;
	ListNode *e;
	unsigned int t;
	unsigned deg=0;
	set a;
	require(eclass!=NULL, "doEclass: NULL eset");

	p = (ECnode *) eclass;
	lexmode(p->lexclass);	/* switch to lexclass where errclass is defined */
	p->eset = empty;
	for (e = (p->elist)->next; e!=NULL; e=e->next)
	{
		q = NULL;								/* MR23 */

		if ( islower( *((char *)e->elem) ) )	/* is it a rule ref? (alias FIRST request) */
		{
			a = Efirst((char *)e->elem, p);
			set_orin(&p->eset, a);
			deg += set_deg(a);
			set_free( a );
			continue;
		}
		else if ( *((char *)e->elem)=='"' )
		{
			t = 0;
			q = (TermEntry *) hash_get(Texpr, (char *) e->elem);
			if ( q == NULL )
			{
				/* if quoted and not an expr look for eclass name */
				q = (TermEntry *) hash_get(Tname, *((char **)&(e->elem))=StripQuotes((char *)e->elem));
				if ( q != NULL ) t = q->token;
			}
			else t = q->token;
		}
		else	/* labelled token/eclass/tokclass */
		{
			q = (TermEntry *) hash_get(Tname, (char *)e->elem);
			if ( q != NULL )
			{
				if ( strcmp((char *)e->elem, TokenString(p->tok))==0 )
				{
					warnNoFL(eMsg1("self-referential error class '%s'; ignored",
								   (char *)e->elem));
					continue;
				}
				else
					t = q->token;
			}
			else t=0;
		}
		if ( t!=0 )
		{
			if (isTermEntryTokClass(q))  {			/* MR23 */
			    tcnode = q->tclass;					/* MR23 */
				set_orin(&p->eset, tcnode->tset);	/* MR23 */
				deg = set_deg(p->eset);				/* MR23 */
			}										/* MR23 */
			else {
				set_orel(t, &p->eset);
				deg++;
			}
		}
		else warnNoFL(eMsg2("undefined token '%s' referenced in errclass '%s'; ignored",
							(char *)e->elem, TokenString(p->tok)));
	}
	p->setdeg = deg;
}

void
#ifdef __USE_PROTOS
ComputeErrorSets( void )
#else
ComputeErrorSets( )
#endif
{
#ifdef __cplusplus
    list_apply(eclasses, (void (*)(void *)) doEclass);
#else
#ifdef __USE_PROTOS
    list_apply(eclasses, (void (*)(void *)) doEclass);
#else
    list_apply(eclasses, doEclass);
#endif
#endif
}

void
#ifdef __USE_PROTOS
ComputeTokSets( void )
#else
ComputeTokSets( )
#endif
{
	ListNode *t, *e = NULL, *e1, *e2;
	int something_changed;
    int i;
	TCnode *p;
	TermEntry *q, *q1, *q2;

	if ( tclasses == NULL ) return;

	/* turn lists of token/tokclass references into sets */
	for (t = tclasses->next; t!=NULL; t=t->next)
	{
		p = (TCnode *) t->elem;

		/* if wild card, then won't have entries in tclass, assume all_tokens */
		if ( p->tok == WildCardToken )
		{
			p->tset = set_dup(all_tokens);
			continue;
		}

		lexmode(p->lexclass);	/* switch to lexclass where tokclass is defined */
		p->tset = empty;

		/* instantiate all tokens/token_classes into the tset */
		for (e = (p->tlist)->next; e!=NULL; e=e->next)
		{
			char *tokstr;
			tokstr = (char *)e->elem;
			if ( *tokstr == '"' ) {
                q = (TermEntry *) hash_get(Texpr, tokstr);
    			require(q!=NULL, "ComputeTokSets: no token def");
    			set_orel(q->token, &p->tset);
			} else if (tokstr[0] == '.') {
                e1=e->next;
                e2=e1->next;
                e=e2;
                q1= (TermEntry *) hash_get(Tname, (char *)e1->elem);
    			require(q1!=NULL, "ComputeTokSets: no token def");
                q2= (TermEntry *) hash_get(Tname, (char *)e2->elem);
    			require(q2!=NULL, "ComputeTokSets: no token def");

                if (set_el(q1->token,imag_tokens)) {
errNoFL(eMsg2("can't define #tokclass %s using #tokclass or #errclass %s",
                        TokenString(p->tok),(char *)e1->elem) );
                }
                if (set_el(q2->token,imag_tokens)) {
errNoFL(eMsg2("can't define #tokclass %s using #tokclass or #errclass %s",
                        TokenString(p->tok),(char *)e2->elem) );
                }
                if (q1->token > q2->token) {
errNoFL(eMsg3("for #tokclass %s %s..%s - first token number > second token number",
                        TokenString(p->tok),(char *)e1->elem,(char *)e2->elem) );
                  for (i=q2->token; i<=q1->token; i++) { set_orel(i, &p->tset); }
                } else {
                  for (i=q1->token; i<=q2->token; i++) { set_orel(i, &p->tset); }
                }
            } else {
                q = (TermEntry *) hash_get(Tname, tokstr);
    			require(q!=NULL, "ComputeTokSets: no token def");
    			set_orel(q->token, &p->tset);
            }
		}
	}

	/* Go thru list of tokclasses again looking for tokclasses in sets */
again:
	something_changed = 0;
	for (t = tclasses->next; t!=NULL; t=t->next)
	{
		set tcl;
		p = (TCnode *) t->elem;
		tcl = set_and(p->tset, tokclasses);
		if ( !set_nil(tcl) )
		{
			int tk;
			/* replace refs to tokclasses with the associated set of tokens */
			something_changed = 1;
			while ( !set_nil(tcl) )
			{
				tk = set_int(tcl);		/* grab one of the tok class refs */
				set_rm(tk, tcl);
				if ( p->tok != tk )		/* tokclass ref to yourself? */
				{
					q = (TermEntry *) hash_get(Tname, TokenString(tk));
					require(q!=NULL, "#tokclass not in hash table");
					set_orin(&p->tset, q->tclass->tset);
				}
				set_rm(tk, p->tset);	/* remove ref that we replaced */
			}
		}
		set_free(tcl);
	}
	if ( something_changed ) goto again;
}

void
#ifdef __USE_PROTOS
DumpRemainingTokSets(void)
#else
DumpRemainingTokSets()
#endif
{
	TCnode *p;
	ListNode *t;

	/* Go thru tclasses (for the last time) and dump the sets not dumped
	 * during code gen; yes, this is a bogus way to do this, but ComputeTokSets()
	 * can't dump the defs as the error file and tok file has not been created
	 * yet etc...
	 */
	if ( tclasses==NULL ) return;
	for (t = tclasses->next; t!=NULL; t=t->next)
	{
		unsigned e;
		p = (TCnode *) t->elem;
		if ( p->dumped ) continue;
		e = DefErrSet(&(p->tset), 0, TokenString(p->tok));
		p->dumped = 1;
		p->setnum = e;
	}
}


/* replace a subset of an error set with an error class name if a subset is found
 * repeat process until no replacements made
 */
void
#ifdef __USE_PROTOS
SubstErrorClass( set *f )
#else
SubstErrorClass( f )
set *f;
#endif
{
	int max, done = 0;
	ListNode *p;
	ECnode *ec, *maxclass = NULL;
	set a;
	require(f!=NULL, "SubstErrorClass: NULL eset");

	if ( eclasses == NULL ) return;
	while ( !done )
	{
		max = 0;
		maxclass = NULL;
		for (p=eclasses->next; p!=NULL; p=p->next)	/* chk all error classes */
		{
			ec = (ECnode *) p->elem;
			if ( ec->setdeg > max )
			{
				if ( set_sub(ec->eset, *f) || set_equ(ec->eset, *f) )
					{maxclass = ec; max=ec->setdeg;}
			}
		}
		if ( maxclass != NULL )	/* if subset found, replace with token */
		{
			a = set_dif(*f, maxclass->eset);
			set_orel((unsigned)maxclass->tok, &a);
			set_free(*f);
			*f = a;
		}
		else done = 1;
	}
}

int
#ifdef __USE_PROTOS
DefErrSet1(int nilOK, set *f, int subst, char *name )
#else
DefErrSet1(nilOK, f, subst, name )
int nilOK;
set *f;
int subst;			/* should be substitute error classes? */
char *name;
#endif
{
	if ( GenCC ) return DefErrSetForCC1(nilOK, f, subst, name, "_set");
	else return DefErrSetForC1(nilOK, f, subst, name, "_set");
}

int
#ifdef __USE_PROTOS
DefErrSet( set *f, int subst, char *name )
#else
DefErrSet( f, subst, name )
set *f;
int subst;			/* should be substitute error classes? */
char *name;
#endif
{
    return DefErrSet1(0,f,subst,name);
}

int
#ifdef __USE_PROTOS
DefErrSetWithSuffix(int nilOK, set *f, int subst, char *name, const char* suffix)
#else
DefErrSetWithSuffix(nilOK, f, subst, name, suffix )
int nilOK;
set *f;
int subst;			/* should be substitute error classes? */
char *name;
char *suffix;
#endif
{
	if ( GenCC ) return DefErrSetForCC1(nilOK, f, subst, name, suffix );
	else return DefErrSetForC1(nilOK, f, subst, name, suffix);
}

/* Define a new error set.  WARNING...set-implementation dependent.
 */
int
#ifdef __USE_PROTOS
DefErrSetForC1(int nilOK, set *f, int subst, char * name, const char * suffix)
#else
DefErrSetForC1(nilOK, f, subst, name, suffix)
int nilOK;          /* MR13 */
set *f;
int subst;			/* should be substitute error classes? */
char *name;
const char *suffix;
#endif
{
	unsigned *p, *endp;
	int e=1;

    if (!nilOK)	require(!set_nil(*f), "DefErrSetForC1: nil set to dump?");

	if ( subst ) SubstErrorClass(f);
	p = f->setword;
	endp = &(f->setword[f->n]);
	esetnum++;
	if ( name!=NULL )
		fprintf(DefFile, "extern SetWordType %s%s[];\n", name, suffix);
	else
		fprintf(DefFile, "extern SetWordType zzerr%d[];\n", esetnum);
	if ( name!=NULL ) {
		fprintf(ErrFile, "SetWordType %s%s[%d] = {",
				name,
                suffix,
				NumWords(TokenNum-1)*sizeof(unsigned));
	}
	else {
		fprintf(ErrFile, "SetWordType zzerr%d[%d] = {",
				esetnum,
				NumWords(TokenNum-1)*sizeof(unsigned));
	}
	while ( p < endp )
	{
		if ( e > 1 ) fprintf(ErrFile, ", ");
		DumpIntAsChars(ErrFile, "0x%x", *p++);
		if ( e == 3 )
		{
			DAWDLE;
			if ( p < endp ) fprintf(ErrFile, ",");
			fprintf(ErrFile, "\n\t");
			e=1;
		}
		else e++;
	}
	fprintf(ErrFile, "};\n");

	return esetnum;
}

int
#ifdef __USE_PROTOS
DefErrSetForC( set *f, int subst, char *name )
#else
DefErrSetForC( f, subst, name )
set *f;
int subst;			/* should be substitute error classes? */
char *name;
#endif
{
  return DefErrSetForC1(0,f,subst,name, "_set");
}

/* Define a new error set.  WARNING...set-implementation dependent;
 * Only used when -CC on.
 */

int
#ifdef __USE_PROTOS
DefErrSetForCC1(int nilOK, set *f, int subst, char *name, const char *suffix )
#else
DefErrSetForCC1(nilOK, f, subst, name, suffix )
int nilOK;          /* MR13 */
set *f;
int subst;			/* should be substitute error classes? */
char *name;
const char *suffix;
#endif
{
	unsigned *p, *endp;
	int e=1;

    if (!nilOK)	require(!set_nil(*f), "DefErrSetForCC1: nil set to dump?");

	if ( subst ) SubstErrorClass(f);
	p = f->setword;
	endp = &(f->setword[f->n]);
	esetnum++;

	if ( name!=NULL ) {
		fprintf(Parser_h, "\tstatic SetWordType %s%s[%d];\n", name, suffix,
				NumWords(TokenNum-1)*sizeof(unsigned));
		fprintf(Parser_c, "SetWordType %s::%s%s[%d] = {",
				CurrentClassName,
				name,
				suffix,
				NumWords(TokenNum-1)*sizeof(unsigned));
	}
	else {
		fprintf(Parser_c, "SetWordType %s::err%d[%d] = {",
				CurrentClassName,
				esetnum,
				NumWords(TokenNum-1)*sizeof(unsigned));
		fprintf(Parser_h, "\tstatic SetWordType err%d[%d];\n", esetnum,
				NumWords(TokenNum-1)*sizeof(unsigned));
	}

	while ( p < endp )
	{
		if ( e > 1 ) fprintf(Parser_c, ", ");
		DumpIntAsChars(Parser_c, "0x%x", *p++);
		if ( e == 3 )
		{
			if ( p < endp ) fprintf(Parser_c, ",");
			fprintf(Parser_c, "\n\t");
			e=1;
		}
		else e++;
	}
	fprintf(Parser_c, "};\n");

	return esetnum;
}

int
#ifdef __USE_PROTOS
DefErrSetForCC( set *f, int subst, char *name )
#else
DefErrSetForCC( f, subst, name )
set *f;
int subst;			/* should be substitute error classes? */
char *name;
#endif
{
  return DefErrSetForCC1(0,f,subst,name, "_set");
}

void
#ifdef __USE_PROTOS
GenParser_c_Hdr(void)
#else
GenParser_c_Hdr()
#endif
{
	int i,j;
    TermEntry   *te;
    char * hasAkaName = NULL;									/* MR23 */

	hasAkaName = (char *) malloc(TokenNum+1);					/* MR23 */
	require(hasAkaName!=NULL, "Cannot alloc hasAkaName\n");		/* MR23 */
	for (i = 0; i < TokenNum; i++) hasAkaName[i]='0';			/* MR23 */
	hasAkaName[TokenNum] = 0;                                   /* MR23 */

	fprintf(Parser_c, "/*\n");
	fprintf(Parser_c, " * %s: P a r s e r  S u p p o r t\n", CurrentClassName);
	fprintf(Parser_c, " *\n");
	fprintf(Parser_c, " * Generated from:");
	for (i=0; i<NumFiles; i++) fprintf(Parser_c, " %s", FileStr[i]);
	fprintf(Parser_c, "\n");
	fprintf(Parser_c, " *\n");
	fprintf(Parser_c, " * Terence Parr, Russell Quong, Will Cohen, and Hank Dietz: 1989-2001\n");
	fprintf(Parser_c, " * Parr Research Corporation\n");
	fprintf(Parser_c, " * with Purdue University Electrical Engineering\n");
	fprintf(Parser_c, " * with AHPCRC, University of Minnesota\n");
	fprintf(Parser_c, " * ANTLR Version %s\n", Version);
	fprintf(Parser_c, " */\n\n");

  if ( FirstAction != NULL ) dumpAction(FirstAction,Parser_c, 0, -1, 0, 1);    /* MR11 MR15b */

	fprintf(Parser_c, "#define ANTLR_VERSION	%s\n", VersionDef);

	fprintf(Parser_c, "#include \"pcctscfg.h\"\n");
	fprintf(Parser_c, "#include \"pccts_stdio.h\"\n");
	fprintf(Parser_c, "#define ANTLR_SUPPORT_CODE\n");
	if ( UserTokenDefsFile != NULL )
	   fprintf(Parser_c, "#include %s\n", UserTokenDefsFile);
	else
	   fprintf(Parser_c, "#include \"%s\"\n", DefFileName);

	fprintf(Parser_c, "#include \"%s.h\"\n\n", CurrentClassName);

	fprintf(Parser_c, "const ANTLRChar *%s::tokenName(int tok) ",   /* MR1 */
					CurrentClassName);                  	        /* MR1 */
	fprintf(Parser_c, "  { return _token_tbl[tok]; }\n");	        /* MR1 */ /* MR10 */
	/* Dump a Parser::tokens for each automaton */
	fprintf(Parser_c, "\nconst ANTLRChar *%s::_token_tbl[]={\n",
                                                 CurrentClassName); /* MR20 */
	fprintf(Parser_c, "\t/* 00 */\t\"Invalid\"");

	for (i=1; i<TokenNum-1; i++)
	{
		DAWDLE;
		if ( i == EpToken ) continue;
		/* remapped to invalid token? */
		if ( TokenInd!=NULL && TokenInd[i]>=LastTokenCounted )
		{
			fprintf(Parser_c, ",\n\t/* %02d */\t\"invalid\"", i);
			continue;
		}
		if ( TokenString(i) != NULL ) {
           te=(TermEntry *) hash_get(Tname,TokenString(i));                     /* MR11 */
            if (te == NULL || te->akaString == NULL) {                          /* MR11 */
  	   	      fprintf(Parser_c, ",\n\t/* %02d */\t\"%s\"", i, TokenString(i));
            } else {
			  hasAkaName[i] = '1';											    /* MR23 */
  	   	      fprintf(Parser_c, ",\n\t/* %02d */\t\"%s\"", i, te->akaString);   /* MR11 */
            }
        }
		else
		{
			/* look in all lexclasses for the reg expr */
			for (j=0; j<NumLexClasses; j++)
			{
				lexmode(j);
				if ( ExprString(i) != NULL )
				{
					fprintf(Parser_c, ",\n\t/* %02d */\t", i);
					dumpExpr(Parser_c, ExprString(i));
					break;
				}
			}
			if ( j>=NumLexClasses )
			{
				if ( UserDefdTokens )
				{
					fprintf(Parser_c, ",\n\t/* %02d */\t\"\"", i);
				}
				else
					fatal_internal(eMsgd("No label or expr for token %d",i));
			}
		}
	}
	fprintf(Parser_c, "\n};\n");

	/* Build constructors */
	fprintf(Parser_c, "\n%s::", CurrentClassName);
	fprintf(Parser_c,	"%s(ANTLRTokenBuffer *input) : %s(input,%d,%d,%d,%d)\n",
						CurrentClassName,
						(BaseClassName == NULL ? "ANTLRParser" : BaseClassName),
						OutputLL_k,
						FoundGuessBlk,
						DemandLookahead,
						NumWords(TokenNum-1)*sizeof(unsigned));
	fprintf(Parser_c, "{\n");
	fprintf(Parser_c, "\ttoken_tbl = _token_tbl;\n");
    if (TraceGen) {
      fprintf(Parser_c, "\ttraceOptionValueDefault=1;\t\t// MR10 turn trace ON\n");
    } else {
      fprintf(Parser_c, "\ttraceOptionValueDefault=0;\t\t// MR10 turn trace OFF\n");
    };
	fprintf(Parser_c, "}\n\n");
	free ( (void *) hasAkaName);
}

void
#ifdef __USE_PROTOS
GenParser_h_Hdr(void)
#else
GenParser_h_Hdr()
#endif
{
	int i;

	fprintf(Parser_h, "/*\n");
	fprintf(Parser_h, " * %s: P a r s e r  H e a d e r \n", CurrentClassName);
	fprintf(Parser_h, " *\n");
	fprintf(Parser_h, " * Generated from:");
	for (i=0; i<NumFiles; i++) fprintf(Parser_h, " %s", FileStr[i]);
	fprintf(Parser_h, "\n");
	fprintf(Parser_h, " *\n");
	fprintf(Parser_h, " * Terence Parr, Russell Quong, Will Cohen, and Hank Dietz: 1989-2001\n");
	fprintf(Parser_h, " * Parr Research Corporation\n");
	fprintf(Parser_h, " * with Purdue University Electrical Engineering\n");
	fprintf(Parser_h, " * with AHPCRC, University of Minnesota\n");
	fprintf(Parser_h, " * ANTLR Version %s\n", Version);
	fprintf(Parser_h, " */\n\n");

  if ( FirstAction != NULL ) dumpAction( FirstAction, Parser_h, 0, -1, 0, 1);         /* MR11 MR15b */

	fprintf(Parser_h, "#ifndef %s_h\n", CurrentClassName);
	fprintf(Parser_h, "#define %s_h\n\n", CurrentClassName);

    fprintf(Parser_h, "#ifndef ANTLR_VERSION\n");
    fprintf(Parser_h, "#define ANTLR_VERSION %s\n",VersionDef);
    fprintf(Parser_h, "#endif\n\n");

	if ( GenAST ) fprintf(Parser_h, "class ASTBase;\n");
    if (TraceGen) {
      fprintf(Parser_h,"#ifndef zzTRACE_RULES\n");  /* MR20 */
      fprintf(Parser_h,"#define zzTRACE_RULES\n");  /* MR20 */
      fprintf(Parser_h,"#endif\n");                 /* MR22 */
    };
	fprintf(Parser_h, "#include \"%s\"\n\n", APARSER_H);

	if ( HdrAction != NULL ) dumpAction( HdrAction, Parser_h, 0, -1, 0, 1);

/* MR10 */    if (ClassDeclStuff == NULL) {
/* MR10 */  	fprintf(Parser_h, "class %s : public ANTLRParser {\n", CurrentClassName);
/* MR10 */    } else {
/* MR10 */      fprintf(Parser_h, "class %s %s {\n",CurrentClassName,ClassDeclStuff);
/* MR10 */    };

	fprintf(Parser_h, "public:\n");					          /* MR1 */
	fprintf(Parser_h, "\tstatic  const ANTLRChar *tokenName(int tk);\n");/* MR1 */
    fprintf(Parser_h, "\tenum { SET_SIZE = %i };\n",TokenNum-1);         /* MR21 */
	fprintf(Parser_h, "protected:\n");
	fprintf(Parser_h, "\tstatic const ANTLRChar *_token_tbl[];\n");     /* MR20 */
	fprintf(Parser_h, "private:\n");
}

/* Currently, this is only used in !GenCC mode */
void
#ifdef __USE_PROTOS
GenErrHdr( void )
#else
GenErrHdr( )
#endif
{
	int i, j;
    TermEntry   *te;

	fprintf(ErrFile, "/*\n");
	fprintf(ErrFile, " * A n t l r  S e t s / E r r o r  F i l e  H e a d e r\n");
	fprintf(ErrFile, " *\n");
	fprintf(ErrFile, " * Generated from:");
	for (i=0; i<NumFiles; i++) fprintf(ErrFile, " %s", FileStr[i]);
	fprintf(ErrFile, "\n");
	fprintf(ErrFile, " *\n");
	fprintf(ErrFile, " * Terence Parr, Russell Quong, Will Cohen, and Hank Dietz: 1989-2001\n");
	fprintf(ErrFile, " * Parr Research Corporation\n");
	fprintf(ErrFile, " * with Purdue University Electrical Engineering\n");
	fprintf(ErrFile, " * With AHPCRC, University of Minnesota\n");
	fprintf(ErrFile, " * ANTLR Version %s\n", Version);
	fprintf(ErrFile, " */\n\n");

  if ( FirstAction != NULL ) dumpAction( FirstAction, ErrFile, 0, -1, 0, 1);         /* MR11 MR15b */

  fprintf(ErrFile, "#define ANTLR_VERSION	%s\n", VersionDef);

  fprintf(ErrFile, "#include \"pcctscfg.h\"\n");
	fprintf(ErrFile, "#include \"pccts_stdio.h\"\n");
	if ( strcmp(ParserName, DefaultParserName)!=0 )
		fprintf(ErrFile, "#define %s %s\n", DefaultParserName, ParserName);
	if ( strcmp(ParserName, DefaultParserName)!=0 )
		fprintf(ErrFile, "#include \"%s\"\n", RemapFileName);
	if ( HdrAction != NULL ) dumpAction( HdrAction, ErrFile, 0, -1, 0, 1 );
	if ( FoundGuessBlk )
	{
		fprintf(ErrFile, "#define ZZCAN_GUESS\n");
		fprintf(ErrFile, "#include \"pccts_setjmp.h\"\n");
	}
    if (TraceGen) {
      fprintf(ErrFile,"#ifndef zzTRACE_RULES\n");  /* MR20 */
      fprintf(ErrFile,"#define zzTRACE_RULES\n");  /* MR20 */
      fprintf(ErrFile,"#endif\n");                 /* MR22 */
    };

	if ( OutputLL_k > 1 ) fprintf(ErrFile, "#define LL_K %d\n", OutputLL_k);
#ifdef DUM
	if ( LexGen ) fprintf(ErrFile, "#define zzEOF_TOKEN %d\n", (TokenInd!=NULL?TokenInd[EofToken]:EofToken));
#endif
	fprintf(ErrFile, "#define zzSET_SIZE %d\n", NumWords(TokenNum-1)*sizeof(unsigned));
	if ( DemandLookahead ) fprintf(ErrFile, "#define DEMAND_LOOK\n");
	fprintf(ErrFile, "#include \"antlr.h\"\n");
	if ( GenAST ) fprintf(ErrFile, "#include \"ast.h\"\n");

    if ( UserDefdTokens ) fprintf(ErrFile, "#include %s\n", UserTokenDefsFile);
	/* still need this one as it has the func prototypes */
	fprintf(ErrFile, "#include \"%s\"\n", DefFileName);
	fprintf(ErrFile, "#include \"dlgdef.h\"\n");
	fprintf(ErrFile, "#include \"err.h\"\n\n");

	/* Dump a zztokens for each automaton */
	if ( strcmp(ParserName, DefaultParserName)!=0 )
	{
		fprintf(ErrFile, "ANTLRChar *%s_zztokens[%d]={\n", ParserName, TokenNum-1);
	}
	else
	{
		fprintf(ErrFile, "ANTLRChar *zztokens[%d]={\n", TokenNum-1);
	}
	fprintf(ErrFile, "\t/* 00 */\t\"Invalid\"");
	for (i=1; i<TokenNum-1; i++)
	{
		DAWDLE;
		if ( i == EpToken ) continue;
		/* remapped to invalid token? */
		if ( TokenInd!=NULL && TokenInd[i]>=LastTokenCounted )
		{
			fprintf(ErrFile, ",\n\t/* %02d */\t\"invalid\"", i);
			continue;
		}
		if ( TokenString(i) != NULL ) {
            te=(TermEntry *) hash_get(Tname,TokenString(i));                     /* MR11 */
            if (te == NULL || te->akaString == NULL) {                          /* MR11 */
  			  fprintf(ErrFile, ",\n\t/* %02d */\t\"%s\"", i, TokenString(i));
            } else {
  			  fprintf(ErrFile, ",\n\t/* %02d */\t\"%s\"", i, te->akaString);    /* MR11 */
            }
        }
		else
		{
			/* look in all lexclasses for the reg expr */
			for (j=0; j<NumLexClasses; j++)
			{
				lexmode(j);
				if ( ExprString(i) != NULL )
				{
					fprintf(ErrFile, ",\n\t/* %02d */\t", i);
					dumpExpr(ErrFile, ExprString(i));
					break;
				}
			}
			if ( j>=NumLexClasses )
			{
				if ( UserDefdTokens )
				{
					fprintf(ErrFile, ",\n\t/* %02d */\t\"\"", i);
				}
				else
					fatal_internal(eMsgd("No label or expr for token %d",i));
			}
		}
	}
	fprintf(ErrFile, "\n};\n");
}

void
#ifdef __USE_PROTOS
dumpExpr( FILE *f, char *e )
#else
dumpExpr( f, e )
FILE *f;
char *e;
#endif
{
	while ( *e!='\0' )
	{
		if ( *e=='\\' && *(e+1)=='\\' )
			{putc('\\', f); putc('\\', f); e+=2;}
		else if ( *e=='\\' && *(e+1)=='"' )
			{putc('\\', f); putc('"', f); e+=2;}
		else if ( *e=='\\' ) {putc('\\', f); putc('\\', f); e++;}
		else {putc(*e, f); e++;}
	}
}

int
#ifdef __USE_PROTOS
isTermEntryTokClass(TermEntry *te)
#else
isTermEntryTokClass(te)
TermEntry *te;
#endif
{
	ListNode *t;
	TCnode *p;
	TermEntry *q;
	char *tokstr;

	if (tclasses == NULL) return 0;

	for (t = tclasses->next; t!=NULL; t=t->next)
	{
		p = (TCnode *) t->elem;
		tokstr = TokenString(p->tok);
		lexmode(p->lexclass);	/* switch to lexclass where tokclass is defined */
        q = (TermEntry *) hash_get(Tname, tokstr);
		if (q == te) return 1;
	}
	return 0;
}
