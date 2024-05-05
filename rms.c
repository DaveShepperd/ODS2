/* check for cr - return terminator - update file length */
/* RMS.c v1.3  RMS components */

/*
		This is part of ODS2 written by Paul Nankervis,
		email address:  Paulnank@au1.ibm.com

		ODS2 is distributed freely for all members of the
		VMS community to use. However all derived works
		must maintain comments in their source to acknowledge
		the contibution of the original author.
*/

/*  Boy some cleanups are needed here - especially for
	error handling and deallocation of memory after errors..
	For now we have to settle for loosing memory left/right/center...

	This module implements file name parsing, file searches,
	file opens, gets, etc...       */


#define DEBUGx x

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <memory.h>
#include <ctype.h>
#include <string.h>

#define NO_DOLLAR
#define RMS$INITIALIZE          /* used by rms.h to create templates */
#include "descrip.h"
#include "ssdef.h"
#include "fibdef.h"
#include "rms.h"
#include "access.h"
#include "direct.h"


/* Table of file name component delimeters... */

char char_delim[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0 };


/* Routine to find size of file name components */

unsigned name_delim(char *str, int len, int size[5])
{
	register unsigned ch;
	register char *curptr = str;
	register char *endptr = curptr + len;
	register char *begptr = curptr;
	while ( curptr < endptr )
	{
		ch = (*curptr++ & 127);
		if ( char_delim[ch] )
			break;
	}
	if ( curptr > begptr && ch == ':' )
	{
		size[0] = (curptr - begptr);
		begptr = curptr;
		while ( curptr < endptr )
		{
			ch = (*curptr++ & 127);
			if ( char_delim[ch] )
				break;
		}
	}
	else
	{
		size[0] = 0;
	}
	if ( ch == '[' || ch == '<' )
	{
		if ( curptr != begptr + 1 )
			return RMS$_FNM;
		while ( curptr < endptr )
		{
			ch = (*curptr++ & 127);
			if ( char_delim[ch] )
				if ( ch != '.' )
					break;
		}
		if ( curptr < begptr + 2 || (ch != ']' && ch != '>') )
			return RMS$_FNM;
		size[1] = (curptr - begptr);
		begptr = curptr;
		while ( curptr < endptr )
		{
			ch = (*curptr++ & 127);
			if ( char_delim[ch] )
				break;
		}
	}
	else
	{
		size[1] = 0;
	}
	if ( curptr > begptr && char_delim[ch] )
	{
		size[2] = (curptr - begptr) - 1;
		begptr = curptr - 1;
	}
	else
	{
		size[2] = (curptr - begptr);
		begptr = curptr;
	}
	if ( curptr > begptr && ch == '.' )
	{
		while ( curptr < endptr )
		{
			ch = (*curptr++ & 127);
			if ( char_delim[ch] )
				break;
		}
		if ( curptr > begptr && char_delim[ch] )
		{
			size[3] = (curptr - begptr) - 1;
			begptr = curptr - 1;
		}
		else
		{
			size[3] = (curptr - begptr);
			begptr = curptr;
		}
	}
	else
	{
		size[3] = 0;
	}
	if ( curptr > begptr && (ch == ';' || ch == '.') )
	{
		while ( curptr < endptr )
		{
			ch = (*curptr++ & 127);
			if ( char_delim[ch] )
				break;
		}
		size[4] = (curptr - begptr);
	}
	else
	{
		size[4] = 0;
	}
#ifdef DEBUG
	printf("Name delim %d %d %d %d %d\n", size[0], size[1], size[2], size[3], size[4]);
#endif
	if ( curptr >= endptr )
	{
		return 1;
	}
	else
	{
		return RMS$_FNM;
	}
}


/* Function to compare directory cache names - directory cache NOT IN USE */

int dircmp(unsigned keylen, void *key, void *node)
{
	register struct DIRCACHE *dirnode = (struct DIRCACHE *)node;
	register int cmp = keylen - dirnode->dirlen;
	if ( cmp == 0 )
	{
		register int len = keylen;
		register char *keynam = (char *)key;
		register char *dirnam = dirnode->dirnam;
		while ( len-- > 0 )
		{
			cmp = toupper(*keynam++) - toupper(*dirnam++);
			if ( cmp != 0 )
				break;
		}
	}
	return cmp;
}


/* Routine to find directory name in cache... NOT CURRENTLY IN USE!!! */

unsigned dircache(struct VCB *vcb, char *dirnam, int dirlen, struct fiddef *dirid)
{
	register struct DIRCACHE *dir;
	if ( dirlen < 1 )
	{
		dirid->fid$w_num = 4;
		dirid->fid$w_seq = 4;
		dirid->fid$b_rvn = 0;
		dirid->fid$b_nmx = 0;
		return 1;
	}
	else
	{
		unsigned sts;
		dir = cache_find((void *)&vcb->dircache, dirlen, dirnam, &sts, dircmp, NULL);
		if ( dir != NULL )
		{
			memcpy(dirid, &dir->dirid, sizeof(struct fiddef));
			return 1;
		}
		return 0;
	}
}


/*      For file context info we use WCCDIR and WCCFILE structures...
		Each WCCFILE structure contains one WCCDIR structure for file
		context. Each level of directory has an additional WCCDIR record.
		For example DKA200:[PNANKERVIS.F11]RMS.C is loosley stored as:-
						next         next
				WCCFILE  -->  WCCDIR  -->  WCCDIR
				RMS.C;       F11.DIR;1    PNANKERVIS.DIR;1

		WCCFILE is pointed to by fab->fab$l_nam->nam$l_wcc and if a
		file is open also by ifi_table[fab->fab$w_ifi]  (so that close
		can easily locate it).

		Most importantly WCCFILE contains a resulting filename field
		which stores the resulting file spec. Each WCCDIR has a prelen
		length to indicate how many characters in the specification
		exist before the bit contributed by this WCCDIR. (ie to store
		the device name and any previous directory name entries.)  */


#define STATUS_INIT 1
#define STATUS_TMPDIR  2
#define STATUS_WILDCARD 4

struct WCCDIR
{
	struct WCCDIR *wcd_next;
	struct WCCDIR *wcd_prev;
	int wcd_status;
	int wcd_wcc;
	int wcd_prelen;
	unsigned short wcd_reslen;
	struct dsc_descriptor wcd_serdsc;
	struct fiddef wcd_dirid;
	char wcd_sernam[1];         /* Must be last in structure */
};                              /* Directory context */


#define STATUS_RECURSE 8
#define STATUS_TMPWCC  16
#define MAX_FILELEN 1024

struct WCCFILE
{
	struct FAB *wcf_fab;
	struct VCB *wcf_vcb;
	struct FCB *wcf_fcb;
	int wcf_status;
	struct fiddef wcf_fid;
	char wcf_result[MAX_FILELEN];
	struct WCCDIR wcf_wcd;      /* Must be last..... (dynamic length). */
};                              /* File context */


/* Function to remove WCCFILE and WCCDIR structures when not required */

void cleanup_wcf(struct WCCFILE *wccfile)
{
	if ( wccfile != NULL )
	{
		struct WCCDIR *wcc = wccfile->wcf_wcd.wcd_next;
		wccfile->wcf_wcd.wcd_next = NULL;
		wccfile->wcf_wcd.wcd_prev = NULL;
		/* should deaccess volume */
		free(wccfile);
		while ( wcc != NULL )
		{
			struct WCCDIR *next = wcc->wcd_next;
			wcc->wcd_next = NULL;
			wcc->wcd_prev = NULL;
			free(wcc);
			wcc = next;
		}
	}
}


/* Function to perform an RMS search... */

unsigned do_search(struct FAB *fab, struct WCCFILE *wccfile)
{
	int sts;
	struct fibdef fibblk;
	struct WCCDIR *wcc;
	struct dsc_descriptor fibdsc,resdsc;
	struct NAM *nam = fab->fab$l_nam;
	wcc = &wccfile->wcf_wcd;
	if ( fab->fab$w_ifi != 0 )
		return RMS$_IFI;

	/* if first time through position at top directory... WCCDIR */

	while ( (wcc->wcd_status & STATUS_INIT) == 0 && wcc->wcd_next != NULL )
	{
		wcc = wcc->wcd_next;
	}
	fibdsc.dsc_w_length = sizeof(struct fibdef);
	fibdsc.dsc_a_pointer = (char *)&fibblk;
	while ( 1 )
	{
		if ( (wcc->wcd_status & STATUS_INIT) == 0 || wcc->wcd_wcc != 0 )
		{
			wcc->wcd_status |= STATUS_INIT;
			resdsc.dsc_w_length = 256 - wcc->wcd_prelen;
			resdsc.dsc_a_pointer = wccfile->wcf_result + wcc->wcd_prelen;
			memcpy(&fibblk.fib$w_did_num, &wcc->wcd_dirid, sizeof(struct fiddef));
			fibblk.fib$w_nmctl = 0;     /* FIB_M_WILD; */
			fibblk.fib$l_acctl = 0;
			fibblk.fib$w_fid_num = 0;
			fibblk.fib$w_fid_seq = 0;
			fibblk.fib$b_fid_rvn = 0;
			fibblk.fib$b_fid_nmx = 0;
			fibblk.fib$l_wcc = wcc->wcd_wcc;
#ifdef DEBUG
			wcc->wcd_sernam[wcc->wcd_serdsc.dsc_w_length] = '\0';
			wccfile->wcf_result[wcc->wcd_prelen + wcc->wcd_reslen] = '\0';
			printf("Ser: '%s' (%d,%d,%d) WCC: %d Prelen: %d '%s'\n", wcc->wcd_sernam,
				   fibblk.fib$w_did_num | (fibblk.fib$b_did_nmx << 16),
				   fibblk.fib$w_did_seq, fibblk.fib$b_did_rvn,
				   wcc->wcd_wcc, wcc->wcd_prelen, wccfile->wcf_result + wcc->wcd_prelen);
#endif
			sts = direct(wccfile->wcf_vcb, &fibdsc, &wcc->wcd_serdsc, &wcc->wcd_reslen, &resdsc, 0);
		}
		else
		{
			sts = SS$_NOMOREFILES;
		}
		if ( sts & 1 )
		{
#ifdef DEBUG
			wccfile->wcf_result[wcc->wcd_prelen + wcc->wcd_reslen] = '\0';
			printf("Fnd: '%s'  (%d,%d,%d) WCC: %d\n", wccfile->wcf_result + wcc->wcd_prelen,
				   fibblk.fib$w_fid_num | (fibblk.fib$b_fid_nmx << 16),
				   fibblk.fib$w_fid_seq, fibblk.fib$b_fid_rvn,
				   wcc->wcd_wcc);
#endif
			wcc->wcd_wcc = fibblk.fib$l_wcc;
			if ( wcc->wcd_prev ) /* go down directory */
			{
				if ( wcc->wcd_prev->wcd_next != wcc )
					printf("wcd_PREV corruption\n");
				if ( fibblk.fib$w_fid_num != 4 || fibblk.fib$b_fid_nmx != 0 ||
					 wcc == &wccfile->wcf_wcd ||
					 memcmp(wcc->wcd_sernam, "000000.", 7) == 0 )
				{
					memcpy(&wcc->wcd_prev->wcd_dirid, &fibblk.fib$w_fid_num, sizeof(struct fiddef));
					if ( wcc->wcd_next )
						wccfile->wcf_result[wcc->wcd_prelen - 1] = '.';
					wcc->wcd_prev->wcd_prelen = wcc->wcd_prelen + wcc->wcd_reslen - 5;
					wcc = wcc->wcd_prev;        /* go down one level */
					if ( wcc->wcd_prev == NULL )
						wccfile->wcf_result[wcc->wcd_prelen - 1] = ']';
				}
			}
			else
			{
				if ( nam != NULL )
				{
					int fna_size[5];
					memcpy(&nam->nam$w_fid_num, &fibblk.fib$w_fid_num, sizeof(struct fiddef));
					nam->nam$b_rsl = wcc->wcd_prelen + wcc->wcd_reslen;
					name_delim(wccfile->wcf_result, nam->nam$b_rsl, fna_size);
					nam->nam$l_dev = nam->nam$l_rsa;
					nam->nam$b_dev = fna_size[0];
					nam->nam$l_dir = nam->nam$l_dev + fna_size[0];
					nam->nam$b_dir = fna_size[1];
					nam->nam$l_name = nam->nam$l_dir + fna_size[1];
					nam->nam$b_name = fna_size[2];
					nam->nam$l_type = nam->nam$l_name + fna_size[2];
					nam->nam$b_type = fna_size[3];
					nam->nam$l_ver = nam->nam$l_type + fna_size[3];
					nam->nam$b_ver = fna_size[4];
					if ( nam->nam$b_rsl <= nam->nam$b_rss )
					{
						memcpy(nam->nam$l_rsa, wccfile->wcf_result, nam->nam$b_rsl);
					}
					else
					{
						return RMS$_RSS;
					}
				}
				memcpy(&wccfile->wcf_fid, &fibblk.fib$w_fid_num, sizeof(struct fiddef));

				return 1;
			}
		}
		else
		{
#ifdef DEBUG
			printf("Err: %d\n", sts);
#endif
			if ( sts == SS$_BADIRECTORY )
			{
				if ( wcc->wcd_next != NULL )
				{
					if ( wcc->wcd_next->wcd_status & STATUS_INIT )
						sts = SS$_NOMOREFILES;
				}
			}
			if ( sts == SS$_NOMOREFILES )
			{
				wcc->wcd_status &= ~STATUS_INIT;
				wcc->wcd_wcc = 0;
				wcc->wcd_reslen = 0;
				if ( wcc->wcd_status & STATUS_TMPDIR )
				{
					struct WCCDIR *savwcc = wcc;
					if ( wcc->wcd_next != NULL )
						wcc->wcd_next->wcd_prev = wcc->wcd_prev;
					if ( wcc->wcd_prev != NULL )
						wcc->wcd_prev->wcd_next = wcc->wcd_next;
					wcc = wcc->wcd_next;
					memcpy(wccfile->wcf_result + wcc->wcd_prelen + wcc->wcd_reslen - 6, ".DIR;1", 6);
					free(savwcc);
				}
				else
				{
					if ( (wccfile->wcf_status & STATUS_RECURSE) && wcc->wcd_prev == NULL )
					{
						struct WCCDIR *newwcc;
						newwcc = (struct WCCDIR *)malloc(sizeof(struct WCCDIR) + 8);
						newwcc->wcd_next = wcc->wcd_next;
						newwcc->wcd_prev = wcc;
						newwcc->wcd_wcc = 0;
						newwcc->wcd_status = STATUS_TMPDIR;
						newwcc->wcd_reslen = 0;
						if ( wcc->wcd_next != NULL )
						{
							wcc->wcd_next->wcd_prev = newwcc;
						}
						wcc->wcd_next = newwcc;
						memcpy(&newwcc->wcd_dirid, &wcc->wcd_dirid, sizeof(struct fiddef));
						newwcc->wcd_serdsc.dsc_w_length = 7;
						newwcc->wcd_serdsc.dsc_a_pointer = newwcc->wcd_sernam;
						memcpy(newwcc->wcd_sernam, "*.DIR;1", 7);
						newwcc->wcd_prelen = wcc->wcd_prelen;
						wcc = newwcc;

					}
					else
					{
						if ( wcc->wcd_next != NULL )
						{
#ifdef DEBUG
							if ( wcc->wcd_next->wcd_prev != wcc )
								printf("wcd_NEXT corruption\n");
#endif
							wcc = wcc->wcd_next;        /* backup one level */
							memcpy(wccfile->wcf_result + wcc->wcd_prelen + wcc->wcd_reslen - 6, ".DIR;1", 6);
						}
						else
						{
							sts = RMS$_NMF;
							break;      /* giveup */
						}
					}
				}
			}
			else
			{
				if ( sts == SS$_NOSUCHFILE )
				{
					if ( wcc->wcd_prev )
					{
						sts = RMS$_DNF;
					}
					else
					{
						sts = RMS$_FNF;
					}
				}
				break;          /* error - abort! */
			}
		}
	}
	cleanup_wcf(wccfile);
	if ( nam != NULL )
		nam->nam$l_wcc = 0;
	fab->fab$w_ifi = 0;         /* must dealloc memory blocks! */
	return sts;
}


/* External entry for search function... */

unsigned sys_search(struct FAB *fab)
{
	struct NAM *nam = fab->fab$l_nam;
	struct WCCFILE *wccfile;
	if ( nam == NULL )
		return RMS$_NAM;
	wccfile = (struct WCCFILE *)nam->nam$l_wcc;
	if ( wccfile == NULL )
		return RMS$_WCC;
	return do_search(fab, wccfile);
}



#define DEFAULT_SIZE 120
static char default_buffer[DEFAULT_SIZE];
static char DefaultName[] = "DKA200:[000000].;";
static char *default_name = DefaultName;
static int default_size[] = { 7, 8, 0, 1, 1 };


/* Function to perform RMS parse.... */

unsigned do_parse(struct FAB *fab, struct WCCFILE **wccret)
{
	struct WCCFILE *wccfile;
	char *fna = fab->fab$l_fna;
	char *dna = fab->fab$l_dna;
	struct NAM *nam = fab->fab$l_nam;
	char topDir[] = "[000000]";
	int sts;
	int fna_size[5] = { 0, 0, 0, 0, 0 }, dna_size[5] = { 0, 0, 0, 0, 0 };
	if ( fab->fab$w_ifi != 0 )
		return RMS$_IFI;
	if ( nam != NULL )
		if ( nam->nam$l_wcc == 0 )
		{
			cleanup_wcf((struct WCCFILE *)nam->nam$l_wcc);
			nam->nam$l_wcc = 0;
		}
	/* Break up file specifications... */

	sts = name_delim(fna, fab->fab$b_fns, fna_size);
	if ( (sts & 1) == 0 )
		return sts;
	if ( dna )
	{
		sts = name_delim(dna, fab->fab$b_dns, dna_size);
		if ( (sts & 1) == 0 )
			return sts;
	}
	/* Make WCCFILE entry for rest of processing */

	{
		wccfile = (struct WCCFILE *)malloc(sizeof(struct WCCFILE) + 256);
		if ( wccfile == NULL )
			return SS$_INSFMEM;
		memset(wccfile, 0, sizeof(struct WCCFILE) + 256);
		wccfile->wcf_fab = fab;
		wccfile->wcf_vcb = NULL;
		wccfile->wcf_fcb = NULL;
		wccfile->wcf_status = 0;
		wccfile->wcf_wcd.wcd_status = 0;
	}

	/* Combine file specifications */

	{
		int field, ess = MAX_FILELEN;
		char *esa, *def = default_name;
		esa = wccfile->wcf_result;
		for ( field = 0; field < 5; field++ )
		{
			char *src;
			int len = fna_size[field];
			if ( len > 0 )
			{
				src = fna;
			}
			else
			{
				len = dna_size[field];
				if ( len > 0 )
				{
					src = dna;
				}
				else
				{
					len = default_size[field];
					src = def;
				}
			}
			fna += fna_size[field];
			if ( field == 1 )
			{
				int dirlen = len;
				if ( len < 3 )
				{
					dirlen = len = default_size[field];
					src = def;
				}
				else
				{
					char ch1 = *(src + 1);
					char ch2 = *(src + 2);
					if ( ch1 == '.' || (ch1 == '-' &&
										(ch2 == '-' || ch2 == '.' || ch2 == ']')) )
					{
						char *dir = def;
						int count = default_size[1] - 1;
						len--;
						src++;
						while ( len >= 2 && *src == '-' )
						{
							len--;
							src++;
							if ( count < 2 || (count == 7 &&
											   memcmp(dir, topDir, 7) == 0) )
								return RMS$_DIR;
							while ( count > 1 )
							{
								if ( dir[--count] == '.' )
									break;
							}
						}
						if ( count < 2 && len < 2 )
						{
							src = topDir;
							dirlen = len = 8;
						}
						else
						{
							if ( *src != '.' && *src != ']' )
								return RMS$_DIR;
							if ( *src == '.' && count < 2 )
							{
								src++;
								len--;
							}
							dirlen = len + count;
							if ( (ess -= count) < 0 )
								return RMS$_ESS;
							memcpy(esa, dir, count);
							esa += count;
						}
					}
				}
				fna_size[field] = dirlen;
			}
			else
			{
				fna_size[field] = len;
			}
			dna += dna_size[field];
			def += default_size[field];
			if ( (ess -= len) < 0 )
				return RMS$_ESS;
			while ( len-- > 0 )
			{
				register char ch;
				*esa++ = ch = *src++;
				if ( ch == '*' || ch == '%' )
					wccfile->wcf_status |= STATUS_WILDCARD;
			}
		}
		/* Pass back results... */
		if ( nam )
		{
			nam->nam$l_dev = nam->nam$l_esa;
			nam->nam$b_dev = fna_size[0];
			nam->nam$l_dir = nam->nam$l_dev + fna_size[0];
			nam->nam$b_dir = fna_size[1];
			nam->nam$l_name = nam->nam$l_dir + fna_size[1];
			nam->nam$b_name = fna_size[2];
			nam->nam$l_type = nam->nam$l_name + fna_size[2];
			nam->nam$b_type = fna_size[3];
			nam->nam$l_ver = nam->nam$l_type + fna_size[3];
			nam->nam$b_ver = fna_size[4];
			nam->nam$b_esl = esa - wccfile->wcf_result;
			nam->nam$l_fnb = 0;
			if ( wccfile->wcf_status & STATUS_WILDCARD )
				nam->nam$l_fnb = NAM$M_WILDCARD;
			if ( nam->nam$b_esl <= nam->nam$b_ess )
			{
				memcpy(nam->nam$l_esa, wccfile->wcf_result, nam->nam$b_esl);
			}
			else
			{
				return RMS$_ESS;
			}
		}
	}
	sts = 1;
	if ( nam != NULL )
		if ( nam->nam$b_nop & NAM$M_SYNCHK )
			sts = 0;

	/* Now build up WCC structures as required */

	if ( sts )
	{
		int dirlen, dirsiz;
		char *dirnam;
		struct WCCDIR *wcc;
		struct DEV *dev;
		sts = device_lookup(fna_size[0], wccfile->wcf_result, 0, &dev);
		if ( (sts & 1) == 0 )
			return sts;
		if ( (wccfile->wcf_vcb = dev->vcb) == NULL )
			return SS$_DEVNOTMOUNT;
		wcc = &wccfile->wcf_wcd;
		wcc->wcd_prev = NULL;
		wcc->wcd_next = NULL;



		/* find directory name - chop off ... if found */

		dirnam = wccfile->wcf_result + fna_size[0] + 1;
		dirlen = fna_size[1] - 2;       /* Don't include [] */
		if ( dirlen >= 3 )
		{
			if ( memcmp(dirnam + dirlen - 3, "...", 3) == 0 )
			{
				wccfile->wcf_status |= STATUS_RECURSE;
				dirlen -= 3;
				wccfile->wcf_status |= STATUS_WILDCARD;
			}
		}
		/* see if we can find directory in cache... */

		dirsiz = dirlen;
		do
		{
			char *dirend = dirnam + dirsiz;
			if ( dircache(wccfile->wcf_vcb, dirnam, dirsiz, &wcc->wcd_dirid) )
				break;
			while ( dirsiz > 0 )
			{
				dirsiz--;
				if ( char_delim[*--dirend & 127] )
					break;
			}
		} while ( 1 );


		/* Create directory wcc blocks for what's left ... */

		while ( dirsiz < dirlen )
		{
			int seglen = 0;
			char *dirptr = dirnam + dirsiz;
			struct WCCDIR *wcd;
			do
			{
				if ( char_delim[*dirptr++ & 127] )
					break;
				seglen++;
			} while ( dirsiz + seglen < dirlen );
			wcd = (struct WCCDIR *)malloc(sizeof(struct WCCDIR) + seglen + 8);
			wcd->wcd_wcc = 0;
			wcd->wcd_status = 0;
			wcd->wcd_prelen = 0;
			wcd->wcd_reslen = 0;
			memcpy(wcd->wcd_sernam, dirnam + dirsiz, seglen);
			memcpy(wcd->wcd_sernam + seglen, ".DIR;1", 7);
			wcd->wcd_serdsc.dsc_w_length = seglen + 6;
			wcd->wcd_serdsc.dsc_a_pointer = wcd->wcd_sernam;
			wcd->wcd_prev = wcc;
			wcd->wcd_next = wcc->wcd_next;
			if ( wcc->wcd_next != NULL )
				wcc->wcd_next->wcd_prev = wcd;
			wcc->wcd_next = wcd;
			wcd->wcd_prelen = fna_size[0] + dirsiz + 1; /* Include [. */
			memcpy(&wcd->wcd_dirid, &wccfile->wcf_wcd.wcd_dirid, sizeof(struct fiddef));
			dirsiz += seglen + 1;
		}
		wcc->wcd_wcc = 0;
		wcc->wcd_status = 0;
		wcc->wcd_reslen = 0;
		wcc->wcd_serdsc.dsc_w_length = fna_size[2] + fna_size[3] + fna_size[4];
		wcc->wcd_serdsc.dsc_a_pointer = wcc->wcd_sernam;
		memcpy(wcc->wcd_sernam, wccfile->wcf_result + fna_size[0] + fna_size[1],
			   wcc->wcd_serdsc.dsc_w_length);
#ifdef DEBUG
		wcc->wcd_sernam[wcc->wcd_serdsc.dsc_w_length] = '\0';
		printf("Parse spec is %s\n", wccfile->wcf_wcd.wcd_sernam);
		for ( dirsiz = 0; dirsiz < 5; dirsiz++ )
			printf("  %d", fna_size[dirsiz]);
		printf("\n");
#endif
	}
	if ( wccret != NULL )
		*wccret = wccfile;
	if ( nam != NULL )
		nam->nam$l_wcc =  wccfile;
	return SS$_NORMAL;
}


/* External entry for parse function... */

unsigned sys_parse(struct FAB *fab)
{
	struct NAM *nam = fab->fab$l_nam;
	if ( nam == NULL )
		return RMS$_NAM;
	return do_parse(fab, NULL);
}


/* Function to set default directory (heck we can sneak in the device...)  */

unsigned sys_setddir(struct dsc_descriptor *newdir, unsigned short *oldlen,
					 struct dsc_descriptor *olddir)
{
	unsigned sts = 1;
	if ( oldlen != NULL )
	{
		int retlen = default_size[0] + default_size[1];
		if ( retlen > olddir->dsc_w_length )
			retlen = olddir->dsc_w_length;
		*oldlen = retlen;
		memcpy(olddir->dsc_a_pointer, default_name, retlen);
	}
	if ( newdir != NULL )
	{
		struct FAB fab = cc$rms_fab;
		struct NAM nam = cc$rms_nam;
		fab.fab$l_nam = &nam;
		nam.nam$b_nop |= NAM$M_SYNCHK;
		nam.nam$b_ess = DEFAULT_SIZE;
		nam.nam$l_esa = default_buffer;
		fab.fab$b_fns = newdir->dsc_w_length;
		fab.fab$l_fna = newdir->dsc_a_pointer;
		sts = sys_parse(&fab);
		if ( sts & 1 )
		{
			if ( nam.nam$b_name + nam.nam$b_type + nam.nam$b_ver > 2 )
				return RMS$_DIR;
			if ( nam.nam$l_fnb & NAM$M_WILDCARD )
				return RMS$_WLD;
			default_name = default_buffer;
			default_size[0] = nam.nam$b_dev;
			default_size[1] = nam.nam$b_dir;
			memcpy(default_name + nam.nam$b_dev + nam.nam$b_dir, ".;", 3);
		}
	}
	return sts;
}


/* This version of connect only resets record pointer */

unsigned sys_connect(struct RAB *rab)
{
	rab->rab$w_rfa[0] = 0;
	rab->rab$w_rfa[1] = 0;
	rab->rab$w_rfa[2] = 0;
	rab->rab$w_rsz = 0;
	if ( rab->rab$l_fab->fab$b_org == FAB$C_SEQ )
	{
		return 1;
	}
	else
	{
		return SS$_NOTINSTALL;
	}
}


/* Disconnect is even more boring */

unsigned sys_disconnect(struct RAB *rab)
{
	return 1;
}



#define IFI_MAX 64
struct WCCFILE *ifi_table[] = {
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

char *sys_getFileName(const struct FAB *fab, char *dest, int destLen)
{
	const struct NAM *nam;
	int dLen = 0;
	if ( fab && (nam = fab->fab$l_nam) )
	{
		dLen += nam->nam$b_dev;
		dLen += nam->nam$b_dir;
		dLen += nam->nam$b_name;
		dLen += nam->nam$b_ver;
		if ( dLen && !dest )
		{
			dest = (char *)malloc(dLen+1);
			destLen = dLen;
		}
		if ( dest )
		{
			if ( dLen && destLen )
			{
				if ( dLen > destLen - 1 )
					dLen = destLen - 1;
				memcpy(dest, nam->nam$l_dev, dLen);
			}
			dest[dLen] = 0;
		}
	}
	return dest;
}

/* get for sequential files */
typedef struct
{
	uint64_t nextByte;      // Basically the total bytes read from file so far
	uint64_t eofByte;       // Index into file of the last byte
	struct FAB *fab;        // Pointer to FAB
	struct RAB *rab;        // Pointer to RAB
	struct FCB *fcb;        // Pointer to FCB
	struct VIOC *vioc;      // Pointer to VIOC
	char *fBuff;            // Pointer to chunk of file's data
	unsigned int stBlock;   // Starting block of chunk
	unsigned int blocks;    // Number of blocks in fBuff
	unsigned int segLen;    // Number of bytes available in fBuff
	unsigned int fbIndex;   // Index into fBuff to get the next read data
	int recordCounter;      // Number of records found
} FileData_t;

static int getFileData(const char *title, FileData_t *filePtr, int fresh)
{
	int sts;

	if ( filePtr->vioc )
		deaccesschunk(filePtr->vioc, 0, 0, 1);
	filePtr->vioc = NULL;
	sts = accesschunk(filePtr->fcb, filePtr->stBlock, &filePtr->vioc, &filePtr->fBuff, &filePtr->blocks, 0);
#if DEBUG
	printf("sys_get(): %s - Asked accesschunk() for block 0x%X to get record's byte count. Returned a total of %d blocks. sts=%d. fbIndex=0x%X, eofByte=0x%lX, nextByte=0x%lX\n",
		   title ? title : "", filePtr->stBlock, filePtr->blocks, sts, filePtr->fbIndex, filePtr->eofByte, filePtr->nextByte);
#endif
	if ( (sts & 1) == 0 )
	{
		if ( sts == SS$_ENDOFFILE )
		{
			/* Keep compiler happy about using ll in format string on 32 bit systems */
			unsigned long long eof, nxt;
			eof = filePtr->eofByte;
			nxt = filePtr->nextByte;
			printf("getFileData(): %s - Unexpected EOF detected. block=0x%X, fbIndex=0x%X, eofByte=0x%llX, nextByte=0x%llX\n",
				   title ? title : "", filePtr->stBlock, filePtr->fbIndex, eof, nxt);
			sts = RMS$_EOF;
		}
		return sts;
	}
	filePtr->segLen = filePtr->blocks * 512;
	if ( fresh )
		filePtr->fbIndex = 0;
	return 1;
}

static int handleEndOfRecord(FileData_t *fileData)
{
	struct RAB *rab;        // Pointer to RAB
	unsigned char *recPtr;
	unsigned short rcdLen;

	rab = fileData->rab;
	rcdLen = rab->rab$w_rsz;
#ifdef RAB$M_RABMRS
	if ( (rab->rab$w_flg & RAB$M_VARMRS) )
	{
		if ( rcdLen > rab->rab$w_tmpmrs )
			rab->rab$w_tmpmrs = rcdLen;
		return 1;
	}
#endif
	if ( rab->rab$b_vfcs && rab->rab$l_rhb )
	{
		unsigned char vfc0, vfc1;

		vfc0 = rab->rab$l_rhb[0];
		vfc1 = rab->rab$l_rhb[1];
		rab->rab$l_rhb[0] = 0;
		rab->rab$l_rhb[1] = 0;
		recPtr = rab->rab$l_rhb;
		/* Here's an attempt at handling fortran carriage control */
		/* vfc0 spec. Char has:
		 *  0  - (as in nul) no carriage control
		 * ' ' - (space) Normal: \n followed by text followed by \r
		 * '$' - Prompt: \n followed by text, no \r at end of line
		 * '+' - Overstrike: text followed by \r
		 * '0' - Double space: \n \n followed by text followed by \r
		 * '1' - Formfeed: \f followed by text followed by \r
		 * any - any other is same as Normal above 
		 * Despite the comments above about the end-of-record
		 * char, it is determined by vfc1 and handled separately below.
		 */
		switch (vfc0)
		{
		case 0:             /* No carriage control at all on this record */
			break;
		default:
		case ' ':           /* normal. \n text \cr */
			if ( (rab->rab$w_flg & RAB$M_BUF_SHARED) )
				++recPtr;
			*recPtr = '\n';
			break;
		case '$':           /* Prompt: \n - buffer */
			if ( (rab->rab$w_flg & RAB$M_BUF_SHARED) )
				++recPtr;
			*recPtr = '\n';
			break;
		case '+':           /* Overstrike: buffer - \r */
			break;
		case '0':           /* Double space: \n\n text \r */
			recPtr[0] = '\n';
			recPtr[1] = '\n';
			break;
		case '1':
			if ( (rab->rab$w_flg & RAB$M_BUF_SHARED) )
				++recPtr;
			*recPtr = '\f';
			break;
		}
		/* vfc1 spec. Bits:
		 * 7 6 5 4 3 2 1 0
		 * 0 0 0 0 0 0 0 0 - no trailing character
		 * 0 x x x x x x x - bits 6-0 indicate how many nl's to output followed by a cr
		 * 1 0 0 x x x x x - bits 4-0 describe the end-of-record char (normally a 0x0D
		 * 1 0 1 x x x x x - all other conditions = just one cr
		 * 1 1 0 0 x x x x - bits 3-0 describe bits to send to VFU. If no VFU, just one cr 
		 * 1 1 0 1 x x x x - all other conditions = just one cr
		 * 1 1 1 x x x x x - all other conditions = just one cr
		 */
		if ( vfc1 )
		{
			int code = (vfc1 >> 5) & 7; /* get top 3 bits of vfc1 */
			recPtr = rab->rab$l_ubf;
			switch (code)
			{
			case 0:
			case 1:
			case 2:
			case 3:
				if ( rcdLen + vfc1 > rab->rab$w_usz - 1 )
					vfc1 = rcdLen + vfc1 - (rab->rab$w_usz - 1);
				memset(recPtr + rcdLen, '\n', vfc1);   /*fill end of record with nl's */
				rcdLen += vfc1;         /* Advance record length */
				recPtr[rcdLen] = '\r';  /* follow all that with a cr */
				++rcdLen;               /* advance count */
				break;
			case 4:
				if ( rcdLen + 1 <= rab->rab$w_usz - 1 )
				{
					recPtr[rcdLen] = vfc1 & 0x1F; /* Follow record with this control code */
					++rcdLen;                   /* advance count */
				}
				break;
			case 5: /* Not used*/
			case 6: /* special VFU stuff (not used) */
			case 7: /* Not used */
				if ( rcdLen + 1 <= rab->rab$w_usz - 1 )
				{
					recPtr[rcdLen] = '\r';      /* Follow record with cr */
					++rcdLen;                   /* advance count */
				}
				break;
			}
		}
	}
	else
	{
		/* Not VFC */
		recPtr = rab->rab$l_ubf;
		if ( (rab->rab$w_flg & RAB$M_CRLF) )
		{
			if ( rcdLen + 1 <= rab->rab$w_usz - 1 )
			{
				recPtr[rcdLen] = '\r';      /* Follow record with cr */
				++rcdLen;                   /* advance count */
			}
		}
		if ( rcdLen + 1 <= rab->rab$w_usz - 1 )
		{
			recPtr[rcdLen] = '\n';      /* Follow record with nl */
			++rcdLen;                   /* advance count */
		}
	}
	rab->rab$w_rsz = rcdLen;
	return 1;
}

unsigned sys_get(struct RAB *rab)
{
	char inFileName[NAM$C_MAXRSS + 1];
	FileData_t fileData;
	struct FAB *fab;        // Pointer to FAB
	unsigned char *recbuff;
	int sts, readRaw;
	unsigned cpylen, reclen;
	unsigned delim, rfm;
	int dellen;

	rab->rab$b_horBytes = 0;    /* No record header bytes */
	rab->rab$b_eorBytes = 0;    /* No record trailer bytes */
	rab->rab$w_eor = rab->rab$w_usz; /* Place in buffer where EOR chars start (outside buffer means there aren't any) */
	rab->rab$b_vfcs = 0;        /* assume no VFC bytes found */
	rab->rab$w_rsz = 0;         /* assume no bytes read */
	fileData.rab = rab;
	fab = rab->rab$l_fab;
	fileData.fab = fab;
	if ( !fab->fab$w_ifi )
	{
		printf("sys_get(): Unexpected 0 for fab$w_ifi. File must already be closed.\n");
		return RMS$_BUG;
	}
	fileData.fcb = ifi_table[fab->fab$w_ifi]->wcf_fcb;
	fileData.vioc = NULL;

	rfm = fab->fab$b_rfm;
	reclen = rab->rab$w_usz;
	recbuff = rab->rab$l_ubf;
	delim = 0;
	readRaw = 0;
	/* If we got a record error and want to switch to read raw */
	if ( (rab->rab$w_flg & RAB$M_BIN) || ((rab->rab$w_flg & (RAB$M_FAL | RAB$M_RCE)) == (RAB$M_FAL | RAB$M_RCE)) )
		readRaw = 1;  /* read raw */
	if ( !readRaw && (rab->rab$w_flg & RAB$M_RAT) )
	{
		/* delim: 0=no delimiter, 1=strmlf, 2=strmcr, 3=stream*/
		switch (rfm)
		{
		case FAB$C_STMLF:
			delim = 1;
			break;
		case FAB$C_STMCR:
			delim = 2;
			break;
		case FAB$C_STM:
			delim = 3;
			break;
		default:
			break;
		}
	}
	/* This seems suboptimal since we probably had a chunk with 2k (4x512) in it but this might shift the entire buffer and cause another read? */
	fileData.stBlock = ((rab->rab$w_rfa[1] << 16) + rab->rab$w_rfa[0]);
	fileData.stBlock += rab->rab$w_rfa[2] / 512;
	fileData.fbIndex = rab->rab$w_rfa[2] % 512;
	if ( fileData.stBlock == 0 )
		fileData.stBlock = 1;
	fileData.nextByte = fileData.stBlock;
	fileData.nextByte *= 512;
	fileData.nextByte += fileData.fbIndex;
	fileData.eofByte = VMSSWAP(fileData.fcb->head->fh2$w_recattr.fat$l_efblk);
	fileData.eofByte *= 512;
	fileData.eofByte += VMSWORD(fileData.fcb->head->fh2$w_recattr.fat$w_ffbyte);
	rab->rab$w_rsz = 0;
	if ( fileData.nextByte >= fileData.eofByte )
		return RMS$_EOF;
	fileData.segLen = 0;
	fileData.recordCounter = 0;
	fileData.fBuff = NULL;

	if ( !readRaw && (rfm == FAB$C_VAR || rfm == FAB$C_VFC) )
	{
		vmsword *lenptr;
		sts = getFileData("Entry", &fileData, 0);
		if ( !(sts & 1) )
		{
			sys_getFileName(fab,inFileName,sizeof(inFileName));
			printf("sys_get(): Bad internal record structure. Expected record's byte count. Found EOF.\n");
			printf("-sys_get()-I-FILE: From file '%s'\n", inFileName);
			return RMS$_RSZ;	/* Bad file record size */
		}
		if ( fileData.segLen - fileData.fbIndex < 2 )
		{
			sys_getFileName(fab,inFileName,sizeof(inFileName));
			printf("sys_get(): Bad internal record structure. segLen=0x%X, fbIndex=0x%X, diff=0x%X. Expected it to be >= 2 to hold record's byte count.\n",
				   fileData.segLen
				   , fileData.fbIndex
				   , fileData.segLen - fileData.fbIndex
				  );
			printf("-sys_get()-I-FILE: From file '%s'\n", inFileName);
			if ( fileData.vioc )
				deaccesschunk(fileData.vioc, 0, 0, 1);
			return RMS$_RSZ;	/* Bad file record size */
		}
		lenptr = (vmsword *)(fileData.fBuff + fileData.fbIndex);
		reclen = VMSWORD(*lenptr);
//        if ( reclen > 100 )
//        {
//            printf("sys_get(): Found a reclen of 0x%X > 0x%X\n", reclen, 100 );
//        }
		if ( reclen > rab->rab$w_usz || (fab->fab$w_mrs && reclen > fab->fab$w_mrs) )
		{
			sys_getFileName(fab, inFileName, sizeof(inFileName));
			if ( !(rab->rab$w_flg & RAB$M_FAL) )
			{
				if ( fileData.vioc )
					deaccesschunk(fileData.vioc, 0, 0, 0);
				printf("%%sys_get()-E-BADCNT: Failed to fetch valid record count. Not recoverable. blocks=0x%X, seglen=0x%X, fbIndex=0x%X, reclen=0x%X, usz=0x%X, mrs=0x%X\n",
					   fileData.blocks
					   ,fileData.segLen
					   ,fileData.fbIndex
					   ,reclen
					   ,rab->rab$w_usz
					   ,fab->fab$w_mrs
					  );
				printf("-sys_get()-I-FILE: From file '%s'\n", inFileName);
				return RMS$_RSZ;	/* Bad record size */
			}
			if ( !(rab->rab$w_flg & RAB$M_RCE) )
			{
				/* Only report this once */
				printf("%%sys_get()-E-BADCNT: Failed to fetch valid record count. blocks=0x%X, segLen=0x%X, fbIndex=0x%X, reclen=0x%X, usz=0x%X, tot=%u. Switching to raw read.\n",
					   fileData.blocks
					   ,fileData.segLen
					   ,fileData.fbIndex
					   ,reclen
					   ,rab->rab$w_usz
					   ,rab->rab$l_tot
					  );
				printf("-sys_get()-I-FILE: From file '%s'\n", inFileName);
			}
			rab->rab$w_flg |= RAB$M_RCE;
			readRaw = 1;
			delim = 0;
		}
		else
		{
			/* VAR or VFC record and reclen has been determined to be something reasonable */
			int fsz = fab->fab$b_fsz;
			++fileData.recordCounter;
			fileData.fbIndex += 2;        /* account for record size bytes */
			fileData.nextByte += 2;         /* advance file byte count too */
			if ( (rab->rab$w_flg & RAB$M_VFC) && rfm == FAB$C_VFC )
			{
				char msg[132];
				msg[0] = 0;
				sts = 1;
				do
				{
					if ( fileData.segLen - fileData.fbIndex < fsz  )
					{
						/* oops. Ran out of buffer after getting record size bytes. Get next block */
						if ( fileData.segLen - fileData.fbIndex > 0 )
						{
							snprintf(msg, sizeof(msg), "sys_get()-E-VFC, Internal error getting VFC bytes. Ran out of buffer but index of %d is not the expected 0\n",
									 fileData.segLen - fileData.fbIndex);
							break;
						}
						fileData.stBlock += fileData.fbIndex / 512;
						sts = getFileData("VFC bytes", &fileData, 1);
						if ( !(sts & 1) )
						{
							if ( sts == SS$_ENDOFFILE )
							{
								printf("sys_get()-E-FILE: Something bad happened. Got EOF trying to get VFC bytes. Not recoverable.\n");
								sys_getFileName(fab,inFileName,sizeof(inFileName));
								printf("-sys_get()-I-FILE: From file '%s'\n", inFileName);
								sts = RMS$_EOF;
							}
							return sts;
						}
					}
					if ( rab->rab$l_rhb && rab->rab$b_rhbSiz )
					{
						memcpy(rab->rab$l_rhb, fileData.fBuff + fileData.fbIndex, rab->rab$b_rhbSiz);
						rab->rab$b_vfcs = fsz;
					}
					fileData.fbIndex += fsz;  /* account for VFC bytes */
					fileData.nextByte += fsz;
					reclen -= fsz;  /* remove the VFC bytes from record length */
				} while ( 0 );
				if ( msg[0] )
				{
					sys_getFileName(fab,inFileName,sizeof(inFileName));
					fputs(msg, stdout);
					printf("-sys_get()-I-FILE: From file '%s'\n", inFileName);
				}
				if ( !(sts & 1) )
					return sts;
			}   // VFC
		}       // record size check
	}           // VAR or VFC

	if ( readRaw )
	{
		reclen = MAX_RMSREC;        // Assume the maximum record length
		if ( reclen > rab->rab$w_usz )
			reclen = rab->rab$w_usz; // But can't be more than the provided buffer
		reclen &= -4;           // Just for grins and chuckles, make default record length a multiple of 4
	}
	cpylen = 0;
	dellen = 0;
	while ( cpylen < reclen )
	{
		int maxCpyLen;
		if ( fileData.nextByte >= fileData.eofByte )
		{
			/* ran off end of file. */
			if ( fileData.vioc )
				deaccesschunk(fileData.vioc, 0, 0, 1);
			rab->rab$w_rsz = cpylen;
			rab->rab$w_rfa[0] = fileData.stBlock & 0xffff;
			rab->rab$w_rfa[1] = fileData.stBlock >> 16;
			rab->rab$w_rfa[2] = fileData.fbIndex;
			sts = (cpylen || fileData.recordCounter) ? 1 : RMS$_EOF;
			return sts;

		}
#if 0
		if ( fileData.nextByte >= 0xB000 && fileData.nextByte < 0xB800 )
		{
			printf("Place to put breakpoint. fileData.nextByte=0x%lX, stBlock=0x%X, blocks=0x%X, segLen=0x%X, fbIndex=0x%X. vioc=%p\n",
				   fileData.nextByte, fileData.stBlock, fileData.blocks, fileData.segLen, fileData.fbIndex, (void *)fileData.vioc );
		}
#endif
		if ( !fileData.vioc || fileData.segLen - fileData.fbIndex <= 0 )
		{
			/* ran out of buffer or it's the first fetch */
			fileData.stBlock += fileData.fbIndex / 512;
			sts = getFileData("Rcd-continue", &fileData, fileData.vioc ? 1 : 0);
			if ( (sts & 1) == 0 )
			{
				if ( sts == SS$_ENDOFFILE )
				{
					rab->rab$w_rsz = cpylen;
					rab->rab$w_rfa[0] = fileData.stBlock & 0xffff;
					rab->rab$w_rfa[1] = fileData.stBlock >> 16;
					rab->rab$w_rfa[2] = fileData.fbIndex;
					sts = (cpylen || fab->fab$b_rfm == FAB$C_VFC) ? 1 : RMS$_EOF;
				}
				return sts;
			}
#if 0
			if ( fileData.nextByte == 0xB580 || fileData.nextByte == 0xB600 )
			{
				unsigned char *ptr = (unsigned char *)fileData.fBuff+fileData.fbIndex;
				printf("fileData.nextByte=0x%lX, stBlock=0x%X, blocks=0x%X, segLen=0x%X, fbIndex=0x%X, tot=0x%X\n",
					   fileData.nextByte, fileData.stBlock, fileData.blocks, fileData.segLen, fileData.fbIndex, rab->rab$l_tot );
				printf("buff: %02X %02X %02X %02X %02X %02X %02X %02X\n",
					   ptr[0],ptr[1],ptr[2],ptr[3],ptr[4],ptr[5],ptr[6],ptr[7] );
			}
#endif
		}
		maxCpyLen = fileData.segLen - fileData.fbIndex;
		if ( maxCpyLen > reclen )
			maxCpyLen = reclen;
		if ( delim )
		{
			char *ptr = fileData.fBuff + fileData.fbIndex;
			/* delim: 1=strmlf, 2=strmcr, 3=stream*/
			if ( delim >= 3 )
			{
				/* stream_crlf */
				while ( maxCpyLen-- > 0 )
				{
					char ch = *ptr++;
					/* Stop on crlf */
					if ( dellen == 1 && ch == '\n' )
					{
						dellen = 2;
						delim = 99;
						break;
					}
					dellen = 0;
					if ( ch == '\r' )
						dellen = 1;
				}
			}
			else
			{
				/* Look for either a \n or a \r */
				char term = '\r';               /* Assume cr */
				if ( delim == 1 )
					term = '\n';    /* Nope, lf */
				while ( maxCpyLen-- > 0 )
				{
					if ( *ptr++ == term )
					{
						dellen = 1;             /* found it */
						delim = 99;
						break;
					}
				}
			}
			/* Copy all but the end of line character(s) */
			maxCpyLen = ptr - (fileData.fBuff + fileData.fbIndex) - dellen;
		}
		/* Limit the copy to whatever is left in the file */
		if ( maxCpyLen > (int)(fileData.eofByte - fileData.nextByte) )
			maxCpyLen = fileData.eofByte - fileData.nextByte;
		/* Limit amount to copy to roon im user's buffer or VAR/VFC record size */
		if ( maxCpyLen > reclen - cpylen )
			maxCpyLen = reclen - cpylen;
		/* copy whatever there is to copy */
		if ( maxCpyLen )
		{
			memcpy(recbuff, fileData.fBuff + fileData.fbIndex, maxCpyLen);
			/* Advance buffer pointer and global counters */
			recbuff += maxCpyLen;
			cpylen += maxCpyLen;
			if ( !(rab->rab$w_flg & RAB$M_RCE) )
				rab->rab$l_tot += maxCpyLen;
		}
		/* Advance raw file indicies */
		fileData.fbIndex += maxCpyLen + dellen;
		fileData.nextByte += maxCpyLen + dellen;
		if ( !readRaw && ((fileData.fbIndex & 1) && (rfm == FAB$C_VAR || rfm == FAB$C_VFC)) )
		{
			/* Skip alignment byte */
			++fileData.fbIndex;
			++fileData.nextByte;
		}
		if ( fileData.nextByte >= fileData.eofByte || (delim == 0 && cpylen >= reclen) || delim == 99 )
		{
			/* reached end of file or end of record */
			break;
		}
	}
	if ( fileData.vioc )
		deaccesschunk(fileData.vioc, 0, 0, 1);
	rab->rab$w_rsz = cpylen;
	rab->rab$w_rfa[0] = fileData.stBlock & 0xffff;
	rab->rab$w_rfa[1] = fileData.stBlock >> 16;
	rab->rab$w_rfa[2] = fileData.fbIndex;
	if (    (sts & 1)
		 && !(rab->rab$w_flg & RAB$M_RCE)
		 && (    (rab->rab$w_flg & RAB$M_RAT)
#ifdef RAB$M_VARMRS
		     ||  (rab->rab$w_flg & RAB$M_VARMRS)
#endif
		    )
	   )
	{
		sts = handleEndOfRecord(&fileData);
	}
	return sts;
}


/* put for sequential files */

unsigned sys_put(struct RAB *rab)
{
	unsigned char *buffer, *recbuff;
	unsigned block, blocks, offset;
	unsigned cpylen, reclen;
	unsigned delim, rfm, sts;
	struct VIOC *vioc;
	struct FCB *fcb = ifi_table[rab->rab$l_fab->fab$w_ifi]->wcf_fcb;

	reclen = rab->rab$w_rsz;
	recbuff = rab->rab$l_rbf;
	delim = 0;
	switch (rfm = rab->rab$l_fab->fab$b_rfm)
	{
	case FAB$C_STMLF:
		if ( reclen < 1 )
		{
			delim = 1;
		}
		else
		{
			if ( recbuff[reclen] != '\n' )
				delim = 1;
		}
		break;
	case FAB$C_STMCR:
		if ( reclen < 1 )
		{
			delim = 2;
		}
		else
		{
			if ( recbuff[reclen] != '\r' )
				delim = 2;
		}
		break;
	case FAB$C_STM:
		if ( reclen < 2 )
		{
			delim = 3;
		}
		else
		{
			if ( recbuff[reclen - 1] != '\r' || recbuff[reclen] != '\n' )
				delim = 3;
		}
		break;
	case FAB$C_VFC:
		reclen += rab->rab$l_fab->fab$b_fsz;
		break;
	case FAB$C_FIX:
		if ( reclen != rab->rab$l_fab->fab$w_mrs )
			return RMS$_RSZ;
		break;
	}

	block = VMSSWAP(fcb->head->fh2$w_recattr.fat$l_efblk);
	offset = VMSWORD(fcb->head->fh2$w_recattr.fat$w_ffbyte);

	sts = accesschunk(fcb, block, &vioc, (char **)&buffer, &blocks, 1);
	if ( (sts & 1) == 0 )
		return sts;

	if ( rfm == FAB$C_VAR || rfm == FAB$C_VFC )
	{
		vmsword *lenptr = (vmsword *)(buffer + offset);
		*lenptr = VMSWORD(reclen);
		offset += 2;
	}

	cpylen = 0;
	while ( 1 )
	{
		int seglen = blocks * 512 - offset;
		if ( seglen > reclen - cpylen )
			seglen = reclen - cpylen;
		if ( rfm == FAB$C_VFC && cpylen < rab->rab$l_fab->fab$b_fsz )
		{
			unsigned fsz = rab->rab$l_fab->fab$b_fsz - cpylen;
			if ( fsz > seglen )
				fsz = seglen;
			if ( rab->rab$l_rhb )
			{
				memcpy(buffer + offset, rab->rab$l_rhb + cpylen, fsz);
			}
			else
			{
				memset(buffer + offset, 0, fsz);
			}
			cpylen += fsz;
			offset += fsz;
			seglen -= fsz;
		}
		if ( seglen )
		{
			memcpy(buffer + offset, recbuff, seglen);
			recbuff += seglen;
			cpylen += seglen;
			offset += seglen;
		}
		if ( delim && offset < blocks * 512 )
		{
			offset++;
			switch (delim)
			{
			case 1:
				*buffer = '\n';
				delim = 0;
				break;
			case 2:
				*buffer = '\r';
				delim = 0;
				break;
			case 3:
				*buffer = '\r';
				if ( offset < blocks * 512 )
				{
					offset++;
					*buffer = '\n';
					delim = 0;
				}
				else
				{
					delim = 2;
				}
				break;
			}
		}
		sts = deaccesschunk(vioc, block, blocks, 1);
		if ( (sts & 1) == 0 )
			return sts;
		block += blocks;
		if ( cpylen >= reclen && delim == 0 )
		{
			break;
		}
		else
		{
			sts = accesschunk(fcb, block, &vioc, (char **)&buffer, &blocks, 1);
			if ( (sts & 1) == 0 )
				return sts;
			offset = 0;
		}
	}
	if ( (offset & 1) && (rfm == FAB$C_VAR || rfm == FAB$C_VFC) )
		offset++;
	block += offset / 512;
	offset %= 512;
	fcb->head->fh2$w_recattr.fat$l_efblk = VMSSWAP(block);
	fcb->head->fh2$w_recattr.fat$w_ffbyte = VMSWORD(offset);
	rab->rab$w_rfa[0] = block & 0xffff;
	rab->rab$w_rfa[1] = block >> 16;
	rab->rab$w_rfa[2] = offset;
	return sts;
}

/* display to fill fab & xabs with info from the file header... */

unsigned sys_display(struct FAB *fab)
{
	struct XABDAT *xab = fab->fab$l_xab;

	struct HEAD *head = ifi_table[fab->fab$w_ifi]->wcf_fcb->head;
	unsigned short *pp = (unsigned short *)head;
	struct IDENT *id = (struct IDENT *)(pp + head->fh2$b_idoffset);
	int ifi_no = fab->fab$w_ifi;
	if ( ifi_no == 0 || ifi_no >= IFI_MAX )
		return RMS$_IFI;
	fab->fab$l_alq = VMSSWAP(head->fh2$w_recattr.fat$l_hiblk);
	fab->fab$b_bks = head->fh2$w_recattr.fat$b_bktsize;
	fab->fab$w_deq = VMSWORD(head->fh2$w_recattr.fat$w_defext);
	fab->fab$b_fsz = head->fh2$w_recattr.fat$b_vfcsize;
	fab->fab$w_gbc = VMSWORD(head->fh2$w_recattr.fat$w_gbc);
	fab->fab$w_mrs = VMSWORD(head->fh2$w_recattr.fat$w_maxrec);
	fab->fab$b_org = head->fh2$w_recattr.fat$b_rtype & 0xf0;
	fab->fab$b_rfm = head->fh2$w_recattr.fat$b_rtype & 0x0f;
	fab->fab$b_rat = head->fh2$w_recattr.fat$b_rattrib;
	while ( xab != NULL )
	{
		switch (xab->xab$b_cod)
		{
		case XAB$C_DAT:
			memcpy(&xab->xab$q_bdt, id->fi2$q_bakdate, sizeof(id->fi2$q_bakdate));
			memcpy(&xab->xab$q_cdt, id->fi2$q_credate, sizeof(id->fi2$q_credate));
			memcpy(&xab->xab$q_edt, id->fi2$q_expdate, sizeof(id->fi2$q_expdate));
			memcpy(&xab->xab$q_rdt, id->fi2$q_revdate, sizeof(id->fi2$q_revdate));
			xab->xab$w_rvn = id->fi2$w_revision;
			break;
		case XAB$C_FHC:
			{
				struct XABFHC *fhc = (struct XABFHC *)xab;
				fhc->xab$b_atr = head->fh2$w_recattr.fat$b_rattrib;
				fhc->xab$b_bkz = head->fh2$w_recattr.fat$b_bktsize;
				fhc->xab$w_dxq = VMSWORD(head->fh2$w_recattr.fat$w_defext);
				fhc->xab$l_ebk = VMSSWAP(head->fh2$w_recattr.fat$l_efblk);
				fhc->xab$w_ffb = VMSWORD(head->fh2$w_recattr.fat$w_ffbyte);
				if ( fhc->xab$l_ebk == 0 )
				{
					fhc->xab$l_ebk = fab->fab$l_alq;
					if ( fhc->xab$w_ffb == 0 )
						fhc->xab$l_ebk++;
				}
				fhc->xab$w_gbc = VMSWORD(head->fh2$w_recattr.fat$w_gbc);
				fhc->xab$l_hbk = VMSSWAP(head->fh2$w_recattr.fat$l_hiblk);
				fhc->xab$b_hsz = head->fh2$w_recattr.fat$b_vfcsize;
				fhc->xab$w_lrl = VMSWORD(head->fh2$w_recattr.fat$w_maxrec);
				fhc->xab$w_verlimit = VMSWORD(head->fh2$w_recattr.fat$w_versions);
			}
			break;
		case XAB$C_PRO:
			{
				struct XABPRO *pro = (struct XABPRO *)xab;
				pro->xab$w_pro = VMSWORD(head->fh2$w_fileprot);
				memcpy(&pro->xab$l_uic, &head->fh2$l_fileowner, 4);
			}
		}
		xab = xab->xab$l_nxt;
	}

	return 1;
}


/* close a file */

unsigned sys_close(struct FAB *fab)
{
	int sts;
	int ifi_no = fab->fab$w_ifi;
	if ( ifi_no < 1 || ifi_no >= IFI_MAX )
		return RMS$_IFI;
	sts = deaccessfile(ifi_table[ifi_no]->wcf_fcb);
	if ( sts & 1 )
	{
		ifi_table[ifi_no]->wcf_fcb = NULL;
		if ( ifi_table[ifi_no]->wcf_status & STATUS_TMPWCC )
		{
			cleanup_wcf(ifi_table[ifi_no]);
			if ( fab->fab$l_nam != NULL )
				fab->fab$l_nam->nam$l_wcc = 0;
		}
		fab->fab$w_ifi = 0;
		ifi_table[ifi_no] = NULL;
	}
	return sts;
}


/* open a file */

unsigned sys_open(struct FAB *fab)
{
	unsigned sts;
	int ifi_no = 1;
	int wcc_flag = 0;
	struct WCCFILE *wccfile = NULL;
	struct NAM *nam = fab->fab$l_nam;
	if ( fab->fab$w_ifi != 0 )
		return RMS$_IFI;
	while ( ifi_table[ifi_no] != NULL && ifi_no < IFI_MAX )
		ifi_no++;
	if ( ifi_no >= IFI_MAX )
		return RMS$_IFI;
	if ( nam != NULL )
	{
		wccfile = (struct WCCFILE *)nam->nam$l_wcc;
	}
	if ( wccfile == NULL )
	{
		sts = do_parse(fab, &wccfile);
		if ( sts & 1 )
		{
			wcc_flag = 1;
			if ( wccfile->wcf_status & STATUS_WILDCARD )
			{
				sts = RMS$_WLD;
			}
			else
			{
				sts = do_search(fab, wccfile);
			}
			wccfile->wcf_status |= STATUS_TMPWCC;
		}
	}
	else
	{
		sts = 1;
	}
	if ( sts & 1 )
		sts = accessfile(wccfile->wcf_vcb, &wccfile->wcf_fid, &wccfile->wcf_fcb,
						 fab->fab$b_fac & (FAB$M_PUT | FAB$M_UPD));
	if ( sts & 1 )
	{
		struct HEAD *head = wccfile->wcf_fcb->head;
		ifi_table[ifi_no] = wccfile;
		fab->fab$w_ifi = ifi_no;
		if ( head->fh2$w_recattr.fat$b_rtype == 0 )
			head->fh2$w_recattr.fat$b_rtype = FAB$C_STMLF;
		sys_display(fab);
	}
	if ( wcc_flag && ((sts & 1) == 0) )
	{
		cleanup_wcf(wccfile);
		if ( nam != NULL )
			nam->nam$l_wcc = 0;
	}
	return sts;
}


/* blow away a file */

unsigned sys_erase(struct FAB *fab)
{
	unsigned sts;
	int ifi_no = 1;
	int wcc_flag = 0;
	struct WCCFILE *wccfile = NULL;
	struct NAM *nam = fab->fab$l_nam;
	if ( fab->fab$w_ifi != 0 )
		return RMS$_IFI;
	while ( ifi_table[ifi_no] != NULL && ifi_no < IFI_MAX )
		ifi_no++;
	if ( ifi_no >= IFI_MAX )
		return RMS$_IFI;
	if ( nam != NULL )
	{
		wccfile = (struct WCCFILE *)fab->fab$l_nam->nam$l_wcc;
	}
	if ( wccfile == NULL )
	{
		sts = do_parse(fab, &wccfile);
		if ( sts & 1 )
		{
			wcc_flag = 1;
			if ( wccfile->wcf_status & STATUS_WILDCARD )
			{
				sts = RMS$_WLD;
			}
			else
			{
				sts = do_search(fab, wccfile);
			}
		}
	}
	else
	{
		sts = 1;
	}
	if ( sts & 1 )
	{
		struct fibdef fibblk;
		struct dsc_descriptor fibdsc, serdsc;
		fibdsc.dsc_w_length = sizeof(struct fibdef);
		fibdsc.dsc_a_pointer = (char *)&fibblk;
		serdsc.dsc_w_length = wccfile->wcf_wcd.wcd_reslen;
		serdsc.dsc_a_pointer = wccfile->wcf_result + wccfile->wcf_wcd.wcd_prelen;
		memcpy(&fibblk.fib$w_did_num, &wccfile->wcf_wcd.wcd_dirid, sizeof(struct fiddef));
		fibblk.fib$w_nmctl = 0;
		fibblk.fib$l_acctl = 0;
		fibblk.fib$w_fid_num = 0;
		fibblk.fib$w_fid_seq = 0;
		fibblk.fib$b_fid_rvn = 0;
		fibblk.fib$b_fid_nmx = 0;
		fibblk.fib$l_wcc = 0;
		sts = direct(wccfile->wcf_vcb, &fibdsc, &serdsc, NULL, NULL, 1);
		if ( sts & 1 )
		{
			sts = accesserase(wccfile->wcf_vcb, &wccfile->wcf_fid);
		}
		else
		{
			printf("Direct status is %d\n", sts);
		}
	}
	if ( wcc_flag )
	{
		cleanup_wcf(wccfile);
		if ( nam != NULL )
			nam->nam$l_wcc = 0;
	}
	return sts;
}


unsigned sys_create(struct FAB *fab)
{
	unsigned sts;
	int ifi_no = 1;
//    int wcc_flag = 0;
	struct WCCFILE *wccfile = NULL;
	struct NAM *nam = fab->fab$l_nam;
	if ( fab->fab$w_ifi != 0 )
		return RMS$_IFI;
	while ( ifi_table[ifi_no] != NULL && ifi_no < IFI_MAX )
		ifi_no++;
	if ( ifi_no >= IFI_MAX )
		return RMS$_IFI;
	if ( nam != NULL )
	{
		wccfile = (struct WCCFILE *)fab->fab$l_nam->nam$l_wcc;
	}
	if ( wccfile == NULL )
	{
		sts = do_parse(fab, &wccfile);
		if ( (sts & 1) )
		{
//            wcc_flag = 1;
			if ( wccfile->wcf_status & STATUS_WILDCARD )
			{
				sts = RMS$_WLD;
			}
			else
			{
				sts = do_search(fab, wccfile);
				if ( sts == RMS$_FNF )
					sts = 1;
			}
		}
	}
	else
	{
		sts = 1;
	}
	if ( (sts & 1) )
	{
		struct fibdef fibblk;
		struct dsc_descriptor fibdsc; //,serdsc;
		fibdsc.dsc_w_length = sizeof(struct fibdef);
		fibdsc.dsc_a_pointer = (char *)&fibblk;
//        serdsc.dsc_w_length = wccfile->wcf_wcd.wcd_reslen;
//        serdsc.dsc_a_pointer = wccfile->wcf_result + wccfile->wcf_wcd.wcd_prelen;
		memcpy(&fibblk.fib$w_did_num, &wccfile->wcf_wcd.wcd_dirid, sizeof(struct fiddef));
		fibblk.fib$w_nmctl = 0;
		fibblk.fib$l_acctl = 0;
		fibblk.fib$w_did_num = 11;
		fibblk.fib$w_did_seq = 1;
		fibblk.fib$b_did_rvn = 0;
		fibblk.fib$b_did_nmx = 0;
		fibblk.fib$l_wcc = 0;
		sts = update_create(wccfile->wcf_vcb, (struct fiddef *)&fibblk.fib$w_did_num,
							"TEST.FILE;1", (struct fiddef *)&fibblk.fib$w_fid_num, &wccfile->wcf_fcb);
		if ( (sts & 1) )
		{
			sts = direct(wccfile->wcf_vcb, &fibdsc, &wccfile->wcf_wcd.wcd_serdsc, NULL, NULL, 2);
			if ( (sts & 1) )
			{
				sts = update_extend(wccfile->wcf_fcb, 100, 0);
				ifi_table[ifi_no] = wccfile;
				fab->fab$w_ifi = ifi_no;
			}
		}
	}
	cleanup_wcf(wccfile);
	if ( nam != NULL )
		nam->nam$l_wcc = 0;
	return sts;
}

unsigned sys_extend(struct FAB *fab)
{
	int sts;
	int ifi_no = fab->fab$w_ifi;
	if ( ifi_no < 1 || ifi_no >= IFI_MAX )
		return RMS$_IFI;
	sts = update_extend(ifi_table[ifi_no]->wcf_fcb,
						fab->fab$l_alq - ifi_table[ifi_no]->wcf_fcb->hiblock, 0);
	return sts;
}

typedef struct
{
	int errSts;
	const char *errMsg;
} RmsErrors_t;

static const RmsErrors_t RmsErrors[] =
{
	{ RMS$_RTB, "Record too large for user's buffer"  },
	{ RMS$_EOF, "End of file"  },
	{ RMS$_FNF, "File not found"  },
	{ RMS$_NMF, "No matching file"  },
	{ RMS$_WCC, "Invalid wildcard context"  },
	{ RMS$_BUG, "Internal bug?"  },
	{ RMS$_DIR, "Bad directory syntax"  },
	{ RMS$_ESS, "Bad extension syntax"  },
	{ RMS$_FNM, "Bad filename syntax"  },
	{ RMS$_IFI, "Bad file index?"  },
	{ RMS$_NAM, "No filename provided"  },
	{ RMS$_RSS, "Filename size problem"  },
	{ RMS$_RSZ, "Bad record size"  },
	{ RMS$_WLD, "Failed to find file via wildcard?"  },
	{ RMS$_DNF, "Directory not found"  },
	{ 0, NULL }
};

void sys_error_str(int rmsSts, char *dest, int destLen)
{
	const RmsErrors_t *ptr = RmsErrors;
	while ( ptr->errMsg )
	{
		if ( ptr->errSts == rmsSts )
		{
			strncpy(dest, ptr->errMsg, destLen);
			return;
		}
		++ptr;
	}
	snprintf(dest, destLen, "Undefined error %d", rmsSts);
}

