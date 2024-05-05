#define MODULE_NAME	ODS2

/*     Ods2.c v1.6   Mainline ODS2 program   */

/*
		This is part of ODS2 written by Paul Nankervis,
		email address:  Paulnank@au1.ibm.com

		ODS2 is distributed freely for all members of the
		VMS community to use. However all derived works
		must maintain comments in their source to acknowledge
		the contibution of the original author.

		The modules in ODS2 are:-

				ACCESS.C        Routines for accessing ODS2 disks
				CACHE.C         Routines for managing memory cache
				DEVICE.C        Routines to maintain device information
				DIRECT.C        Routines for handling directories
				ODS2.C          The mainline program
				PHYVMS.C        Routine to perform physical I/O
				RMS.C           Routines to handle RMS structures
				VMSTIME.C       Routines to handle VMS times

		On non-VMS platforms PHYVMS.C should be replaced as follows:-

				OS/2            PHYOS2.C
				Windows 95/NT   PHYNT.C

		For example under OS/2 the program is compiled using the GCC
		compiler with the single command:-

				gcc -fdollars-in-identifiers ods2.c,rms.c,direct.c,
					  access.c,device.c,cache.c,phyos2.c,vmstime.c
*/

/* Modified by:
 *
 *   31-AUG-2001 01:04	Hunter Goatley <goathunter@goatley.com>
 *
 *	For VMS, added routine getcmd() to read commands with full
 *	command recall capabilities.
 *
 */

/*  This version will compile and run using normal VMS I/O by
	defining VMSIO
*/

/*  This is the top level set of routines. It is fairly
	simple minded asking the user for a command, doing some
	primitive command parsing, and then calling a set of routines
	to perform whatever function is required (for example COPY).
	Some routines are implemented in different ways to test the
	underlying routines - for example TYPE is implemented without
	a NAM block meaning that it cannot support wildcards...
	(sorry! - could be easily fixed though!)
*/

#ifndef n_elts
#define n_elts(x) (int)(sizeof(x)/sizeof((x)[0]))
#endif

#ifdef VMS
	#ifdef __DECC
		#pragma module MODULE_NAME VERSION
	#else
		#ifdef vaxc
			#module MODULE_NAME VERSION
		#endif /* vaxc */
	#endif /* __DECC */
#endif /* VMS */

#define DEBUGx on
#define VMSIOx on

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <getopt.h>
#if USE_STRERROR
	#include <errno.h>
#endif
#if USE_UTIME
	#include <utime.h>
#endif
#if USE_READLINE
	#include <readline/readline.h>
	#include <readline/history.h>
	#ifndef READLINE_HISTORY_FILENAME
		#define READLINE_HISTORY_FILENAME ".ods2_history"	/* Filename of readline history file */
	#endif
	#ifndef READLINE_HISTORY_LINES
		#define READLINE_HISTORY_LINES (256)				/* maximum number of lines maintained in history file */
	#endif
	#include <fcntl.h>
	#include <unistd.h>
#endif

#ifdef VMSIO
	#include <ssdef.h>
	#include <descrip.h>
	#include <starlet.h>
	#include <rms.h>
	#include <fiddef.h>
	#define sys_parse       sys$parse
	#define sys_search      sys$search
	#define sys_open        sys$open
	#define sys_close       sys$close
	#define sys_connect     sys$connect
	#define sys_disconnect  sys$disconnect
	#define sys_get         sys$get
	#define sys_put         sys$put
	#define sys_create      sys$create
	#define sys_erase       sys$erase
	#define sys_extend      sys$extend
	#define sys_asctim      sys$asctim
	#define sys_setddir     sys$setddir
	#define dsc_descriptor  dsc$descriptor
	#define dsc_w_length    dsc$w_length
	#define dsc_a_pointer   dsc$a_pointer

#else
	#include "ssdef.h"
	#include "descrip.h"
	#include "access.h"
	#include "rms.h"
	#include "phyio.h"
#endif

#include "version.h"

#if MSYS2 || MINGW
	#define MKDIR(a,b) mkdir(a)
	#define OPEN_FLAGS O_RDONLY|O_BINARY
#else
	#define MKDIR(a,b) mkdir(a,b)
	#define OPEN_FLAGS O_RDONLY
#endif

static char cDelim = ';';
int cmdVerbose;

/* keycomp: routine to compare parameter to a keyword - case insensitive! */

int keycomp(char *param, const char *keywrd)
{
	while ( *param != '\0' )
	{
		if ( tolower(*param++) != *keywrd++ )
			return 0;
	}
	return 1;
}


/* checkquals: routine to find a qualifer in a list of possible values */

int checkquals(const char *const qualset[], int qualc, char *qualv[])
{
	int result = 0;
	while ( qualc-- > 0 )
	{
		int i = 0;
		while ( qualset[i] != NULL )
		{
			if ( keycomp(qualv[qualc], qualset[i]) )
			{
				result |= 1 << i;
				i = -1;
				break;
			}
			i++;
		}
		if ( i >= 0 )
			printf("%%ODS2-W-ILLQUAL, Unknown qualifer '%s' ignored\n", qualv[qualc]);
	}
	return result;
}

static const char* getRfmAndRats(struct FAB *fab, char *ratsBuf, int ratsBufSize, int addDelim)
{
	static const char *const Rfms[] =
	{
		 "UNDEF"    // 0
		,"FIXED"    // 1
		,"VAR"      // 2
		,"VFC"      // 3
		,"STMCRLF"  // 4
		,"STMLF"	// 5
		,"STMCR"	// 6
	};
	int rIdx = fab->fab$b_rfm;
	int len;
	char lclDelim[2];
	const char *dPtr;
	
	lclDelim[0] = cDelim;
	lclDelim[1] = 0;
	dPtr = addDelim ? lclDelim : "";
	if ( rIdx < 0 || rIdx > FAB$C_STMCR )
		rIdx = 0;
	len = 0;
	if ( (fab->fab$b_rat & FAB$M_FTN) )
	{
		len += snprintf(ratsBuf + len, ratsBufSize - len, "%sFTN", dPtr);
		dPtr = lclDelim;
	}
	if ( (fab->fab$b_rat & FAB$M_CR) )
	{
		len += snprintf(ratsBuf + len, ratsBufSize - len, "%sCR", dPtr);
		dPtr = lclDelim;
	}
	if ( (fab->fab$b_rat & FAB$M_PRN) )
	{
		len += snprintf(ratsBuf + len, ratsBufSize - len, "%sPRN", dPtr);
		dPtr = lclDelim;
	}
	if ( (fab->fab$b_rat & FAB$M_BLK) )
	{
		len += snprintf(ratsBuf + len, ratsBufSize - len, "%sBLK", dPtr);
		dPtr = lclDelim;
	}
	if ( !len )
		snprintf(ratsBuf + len, ratsBufSize - len, "%sNONE", dPtr);
	return Rfms[rIdx];
}

/* dir: a directory routine */

static const char *const dirquals[] = { "date", "file", "size", "full", NULL };
#define OPT_DIR_DATE	(1)
#define OPT_DIR_FILE	(2)
#define OPT_DIR_SIZE	(4)
#define OPT_DIR_FULL	(8)

unsigned dir(int argc, char *argv[], int qualc, char *qualv[])
{
	char res[NAM$C_MAXRSS + 1], rsa[NAM$C_MAXRSS + 1];
	char defaultName[] = "*.*;*";
	int sts, options;
	int filecount = 0;
	struct NAM nam = cc$rms_nam;
	struct FAB fab = cc$rms_fab;
	struct XABDAT dat = cc$rms_xabdat;
	struct XABFHC fhc = cc$rms_xabfhc;
	nam.nam$l_esa = res;
	nam.nam$b_ess = NAM$C_MAXRSS;
	fab.fab$l_nam = &nam;
	fab.fab$l_xab = &dat;
	dat.xab$l_nxt = &fhc;
	fab.fab$l_fna = argv[1];
	fab.fab$b_fns = strlen(fab.fab$l_fna);
	fab.fab$l_dna = defaultName;
	fab.fab$b_dns = strlen(fab.fab$l_dna);
	options = checkquals(dirquals, qualc, qualv);
	if ( (options & OPT_DIR_FULL) )
		options |= OPT_DIR_SIZE | OPT_DIR_DATE | OPT_DIR_FILE;
	sts = sys_parse(&fab);
	if ( sts & 1 )
	{
		char dir[NAM$C_MAXRSS + 1];
		int namelen;
		int dirlen = 0;
		int dirfiles = 0, dircount = 0;
		int dirblocks = 0, totblocks = 0;
		int printcol = 0;
#ifdef DEBUG
		res[nam.nam$b_esl] = '\0';
		printf("Parse: %s\n", res);
#endif
		nam.nam$l_rsa = rsa;
		nam.nam$b_rss = NAM$C_MAXRSS;
		fab.fab$l_fop = FAB$M_NAM;
		while ( (sts = sys_search(&fab)) & 1 )
		{
			if ( dirlen != nam.nam$b_dev + nam.nam$b_dir ||
				 memcmp(rsa, dir, nam.nam$b_dev + nam.nam$b_dir) != 0 )
			{
				if ( dirfiles > 0 )
				{
					if ( printcol > 0 )
						printf("\n");
					printf("\nTotal of %d file%s", dirfiles, (dirfiles == 1 ? "" : "s"));
					if ( (options & OPT_DIR_SIZE) )
					{
						printf(", %d block%s.\n", dirblocks, (dirblocks == 1 ? "" : "s"));
					}
					else
					{
						fputs(".\n", stdout);
					}
				}
				dirlen = nam.nam$b_dev + nam.nam$b_dir;
				memcpy(dir, rsa, dirlen);
				dir[dirlen] = '\0';
				printf("\nDirectory %s\n\n", dir);
				filecount += dirfiles;
				totblocks += dirblocks;
				dircount++;
				dirfiles = 0;
				dirblocks = 0;
				printcol = 0;
			}
			rsa[nam.nam$b_rsl] = '\0';
			namelen = nam.nam$b_name + nam.nam$b_type + nam.nam$b_ver;
			if ( options == 0 )
			{
				if ( printcol > 0 )
				{
					int newcol = (printcol + 20) / 20 * 20;
					if ( newcol + namelen >= 80 )
					{
						fputs("\n", stdout);
						printcol = 0;
					}
					else
					{
						printf("%*s", newcol - printcol, " ");
						printcol = newcol;
					}
				}
				fputs(rsa + dirlen, stdout);
				printcol += namelen;
			}
			else
			{
				if ( namelen > 18 )
				{
					printf("%s\n                   ", rsa + dirlen);
				}
				else
				{
					printf("%-19s", rsa + dirlen);
				}
				sts = sys_open(&fab);
				if ( (sts & 1) == 0 )
				{
					printf("Open error: %d\n", sts);
				}
				else
				{
					sts = sys_close(&fab);
					if ( (options & OPT_DIR_FILE) )
					{
						char fileid[100];
						snprintf(fileid, sizeof(fileid), "(%d,%d,%d)",
								 (nam.nam$b_fid_nmx << 16) | nam.nam$w_fid_num,
								 nam.nam$w_fid_seq, nam.nam$b_fid_rvn);
						printf("  %-22s", fileid);
					}
					if ( (options & OPT_DIR_SIZE) )
					{
						unsigned filesize = fhc.xab$l_ebk;
						if ( fhc.xab$w_ffb == 0 )
							filesize--;
						printf("%9d", filesize);
						dirblocks += filesize;
					}
					if ( (options & OPT_DIR_DATE) )
					{
						char tim[24];
						struct dsc_descriptor timdsc;
						timdsc.dsc_w_length = 23;
						timdsc.dsc_a_pointer = tim;
						sts = sys_asctim(0, &timdsc, dat.xab$q_cdt, 0);
						if ( (sts & 1) == 0 )
							printf("Asctim error: %d\n", sts);
						tim[23] = '\0';
						printf("  %s", tim);
					}
					if ( (options & OPT_DIR_FULL) )
					{
						/* keep the compiler happy about using ll in format string on 32 bit systems */
						unsigned long long fSize;
						char ratsBuf[100];
						const char *rfmType;
						rfmType = getRfmAndRats(&fab, ratsBuf, sizeof(ratsBuf), 0);
						fSize = fhc.xab$l_ebk;
						if ( !fSize )
							++fSize;
						fSize *= 512;
						fSize += fhc.xab$w_ffb - 512;
						printf("\n\tfab: fns=%d, dns=%d, alq=%d, deq=%d, mrs=%d, org=%d, rat=0x%X(%s), rfm=%d(%s), size in bytes: %llu."
							   , fab.fab$b_fns   // Filespec size
							   , fab.fab$b_dns   // Filespec size
							   , fab.fab$l_alq   // Allocation qty (blocks)
							   , fab.fab$w_deq   // Extension qty (blocks)
							   , fab.fab$w_mrs   // Maximum record size
							   , fab.fab$b_org   // File organization
							   , fab.fab$b_rat   // Record attributes
							   , ratsBuf
							   , fab.fab$b_rfm   // Record format
							   , rfmType
							   , fSize           // File size in bytes
							  );
					}
					printf("\n");
				}
			}
			dirfiles++;
		}
		if ( sts == RMS$_NMF )
			sts = 1;
		if ( printcol > 0 )
			printf("\n");
		if ( dirfiles > 0 )
		{
			printf("\nTotal of %d file%s", dirfiles, (dirfiles == 1 ? "" : "s"));
			if ( (options & OPT_DIR_SIZE) )
			{
				printf(", %d block%s.\n", dirblocks, (dirblocks == 1 ? "" : "s"));
			}
			else
			{
				fputs(".\n", stdout);
			}
			filecount += dirfiles;
			totblocks += dirblocks;
			if ( dircount > 1 )
			{
				printf("\nGrand total of %d director%s, %d file%s",
					   dircount, (dircount == 1 ? "y" : "ies"),
					   filecount, (filecount == 1 ? "" : "s"));
				if ( (options & OPT_DIR_SIZE) )
				{
					printf(", %d block%s.\n", totblocks, (totblocks == 1 ? "" : "s"));
				}
				else
				{
					fputs(".\n", stdout);
				}
			}
		}
	}
	if ( sts & 1 )
	{
		if ( filecount < 1 )
			printf("%%DIRECT-W-NOFILES, no files found\n");
	}
	else
	{
		printf("%%DIR-E-ERROR Status: %d\n", sts);
	}
	return sts;
}


/* copy: a file copy routine */

static const char *const copyquals[] = { "binary", "crlf", "dir", "ignore", "lf", "quiet", "stream", "test", "time", "verbose", "vfc", NULL };
#define OPT_COPY_BINARY		(0x001)	/* Copy binary mode */
#define OPT_COPY_CRLF   	(0x002)	/* Terminate records with crlf instead of normal lf */
#define OPT_COPY_DIRS   	(0x004)	/* Go ahead and copy the .dir files as well as making directories */
#define OPT_COPY_IGNORE		(0x008)	/* Ignore record count errors. Keep reading/writing if one found */
#define OPT_COPY_LF			(0x010)	/* Terminate records with just lf */
#define OPT_COPY_QUIET		(0x020)	/* No squawking about the copy except if errors */
#define OPT_COPY_STREAM		(0x040)	/* Ignore record handling in stream files. Treat them as raw data. */
#define OPT_COPY_TEST		(0x080)	/* Just test the copy procedure */
#define OPT_COPY_TIME		(0x100)	/* Include timestamps on file creation */
#define OPT_COPY_VERBOSE	(0x200)	/* Squawk what is being done */
#define OPT_COPY_VFC		(0x400)	/* Interpret carriage control on VFC formatte files */

typedef struct
{
	struct NAM *nam;
	struct FAB *fab;
	struct RAB *rab;
	struct XABDAT *dat;
	struct XABFHC *fhc;
	const char *outFileName;
	int testMode;
	int options;
	int records;
	int badRecords;
	int totWrite;
	char errMsg[128];
} RW_Params_t;

typedef enum
{
	RW_Sts_Good,
	RW_Sts_Fatal,
	RW_Sts_Retry
} RW_Sts_t;

static RW_Sts_t copy_rw(RW_Params_t *params, int retry)
{
	unsigned char realRec[2 + MAX_RMSREC + 128 + 2];  /* record buff is large enough to hold all potential vfc and eor characters */
	FILE *tof;
	int sts;
	RW_Sts_t rwSts = RW_Sts_Good;
	unsigned char *recPtr;

	tof = NULL;
	if ( !params->testMode )
	{
		tof = fopen(params->outFileName, "wb");
	}
	if ( !params->testMode && tof == NULL )
	{
#if USE_STRERROR
		printf("-COPY-I-FOPEN, Could not open %s for output: %s\n", params->outFileName, strerror(errno));
#else
		printf("-COPY-I-FOPEN, Could not open %s for output: errno=%d\n", params->outFileName, errno);
#endif
		return RW_Sts_Fatal;
	}
	SCacheEna = 0;
	sts = RMS$_EOF;
	if ( !retry && params->testMode < 3 )
	{
		recPtr = realRec;
		/* Tell sys_get() to interpret VFC bytes if there are any */
		params->rab->rab$l_rhb = (params->options & OPT_COPY_VFC) ? recPtr : NULL;
		params->rab->rab$b_rhbSiz = 2;  /* Leave two bytes in front as room for potential vfc record leader */
		recPtr += params->rab->rab$b_rhbSiz;
		params->rab->rab$l_ubf = recPtr;
		params->rab->rab$w_usz = sizeof(realRec) - params->rab->rab$b_rhbSiz; /* Total size of user buffer */
		if ( (cmdVerbose&CMD_VERBOSE_WR) || (params->options & OPT_COPY_VERBOSE) )
		{
			printf("%%COPY-I-OPEN, Opened '%s' for output. testMode=%d. flg=0x%X, rfm=0x%X, rat=0x%X, rab$l_rhb=%p, rab$l_ubf=%p\n",
				   params->outFileName,
				   params->testMode,
				   params->rab->rab$w_flg,
				   params->fab->fab$b_rfm,
				   params->fab->fab$b_rat,
				   (void *)params->rab->rab$l_rhb,
				   (void *)params->rab->rab$l_ubf);
		}
		while ( 1 )
		{
			unsigned rcdLen;

			sts = sys_get(params->rab);
			if ( !retry && (cmdVerbose&CMD_VERBOSE_RD) )
				printf("%%COPY-I-READ, sys_get() returned %d. rsz=%d\n", sts, params->rab->rab$w_rsz);
			if ( !(sts & 1) )
				break;
			rcdLen = params->rab->rab$w_rsz;
			recPtr = params->rab->rab$l_ubf;
			if ( params->rab->rab$b_vfcs )
			{
				if ( params->rab->rab$l_rhb[0] )
				{
					recPtr -= 2;
					rcdLen += 2;
				}
				else if ( params->rab->rab$l_rhb[1] )
				{
					recPtr -= 1;
					rcdLen += 1;
				}
			}
			if ( fwrite(recPtr, 1, rcdLen, tof) != rcdLen )
			{
#if USE_STRERROR
				printf("-COPY-E-FWRITE of record failed to %s: %s\n", params->outFileName, strerror(errno));
#else
				printf("-COPY-E-FWRITE of record failed to %s: errno=%d\n", params->outFileName, errno);
#endif
				rwSts = RW_Sts_Fatal;
				break;
			}
			if ( rwSts == RW_Sts_Good && !retry )
			{
				params->totWrite += rcdLen;
				if ( !(params->rab->rab$w_flg & RAB$M_RCE) )
					params->records++;
				else
					++params->badRecords;
				if ( tof && (cmdVerbose&CMD_VERBOSE_WR) )
				{
					unsigned char *uRecPtr = (unsigned char *)recPtr;
					printf("%%COPY-I-WRITE, wrote a total of %d(0x%X) bytes. recPtr=%p, rcdLen=%d(0x%X), rab$w_flg=0x%X. goodRecords=%d, badRecords=%d\n",
						   params->totWrite, params->totWrite, (void *)recPtr, rcdLen, rcdLen, params->rab->rab$w_flg, params->records, params->badRecords);
					printf("\tbuff (%p): %02X %02X %02X %02X %02X %02X %02X %02X\n",
						   (void *)uRecPtr, uRecPtr[0], uRecPtr[1], uRecPtr[2], uRecPtr[3], uRecPtr[4], uRecPtr[5], uRecPtr[6], uRecPtr[7]);

				}
			}
		}
	}
	SCacheEna = 1;
	if ( tof )
	{
		if ( fclose(tof) )
		{
#if USE_STRERROR
			printf("%%COPY-F-FCLOSE Failed to fclose(%s): %s\n", params->outFileName, strerror(errno));
#else
			printf("%%COPY-F-FCLOSE Failed to fclose(%s): errno=%d\n", params->outFileName, errno);
#endif
		}
		else
		{
			if ( (cmdVerbose&CMD_VERBOSE_WR) )
				printf("%%COPY-I-CLOSE, fclose(%s)\n", params->outFileName);
#if USE_UTIME
			if ( (params->options & OPT_COPY_TIME) )
			{
				struct utimbuf utb;
				utb.actime = utb.modtime = vmstime_to_unix(params->dat->xab$q_cdt);
				if ( utb.actime )
				{
					if ( utime(params->outFileName, &utb) < 0 )
					{
#if USE_STRERROR
						printf("%%COPY-E-utime(): failed for %s: %s\n", params->outFileName, strerror(errno));
#else
						printf("%%COPY-E-utime(): failed for %s: errno=%d\n", params->outFileName, errno);
#endif
						rwSts = RW_Sts_Fatal;
					}
					else if ( (cmdVerbose&CMD_VERBOSE_WR) )
					{
						unsigned long long hisTime = utb.actime;
						printf("%%COPY-I-TIME, Reset times on '%s' to %llu\n", params->outFileName, hisTime);
					}
				}
			}
#endif
		}
		tof = NULL;
	}
	if ( !retry && rwSts == RW_Sts_Good && (params->rab->rab$w_flg & RAB$M_RCE) )
		rwSts = RW_Sts_Retry;
	return rwSts;
}

unsigned copy(int argc, char *argv[], int qualc, char *qualv[])
{
	int sts, options, testMode;
	char errMsg[128];
	struct NAM nam = cc$rms_nam;
	struct FAB fab = cc$rms_fab;
	struct XABDAT dat = cc$rms_xabdat;
	struct XABFHC fhc = cc$rms_xabfhc;
	char res[NAM$C_MAXRSS + 1], rsa[NAM$C_MAXRSS + 1];
	int filecount = 0;
	
	nam.nam$l_esa = res;
	nam.nam$b_ess = NAM$C_MAXRSS;
	fab.fab$l_nam = &nam;
	fab.fab$l_fna = argv[1];
	fab.fab$b_fns = strlen(fab.fab$l_fna);
	fab.fab$l_xab = &dat;
	dat.xab$l_nxt = &fhc;
	options = checkquals(copyquals, qualc, qualv);
	if ( (options & OPT_COPY_TEST) )
		options &= ~OPT_COPY_QUIET; /* If /test option, then always disable /quiet */
#if WIN32
	if ( !(options&OPT_COPY_LF) )
	options |= OPT_COPY_CRLF;   /* Default to crlf on Windows systems */
#endif
	sts = sys_parse(&fab);
	if ( sts & 1 )
	{
		nam.nam$l_rsa = rsa;
		nam.nam$b_rss = NAM$C_MAXRSS;
		fab.fab$l_fop = FAB$M_NAM;
		while ( (sts = sys_search(&fab)) & 1 )
		{
			sts = sys_open(&fab);
			testMode = (options & OPT_COPY_TEST) ? 1 : 0;
			if ( (sts & 1) == 0 )
			{
				/* Have to call perror first since printf will clobber errno. Not VMS display order, but oh well */
				printf("%%COPY-F-OPENFAIL, Open error: %d\n", sts);
				sys_error_str(sts, errMsg, sizeof(errMsg) - 1);
				errMsg[sizeof(errMsg) - 1] = 0;
				printf("-COPY-F-ERR: %s\n", errMsg);
				continue;
			}
			else
			{
				struct RAB rab = cc$rms_rab;
				rab.rab$l_fab = &fab;
				if ( (sts = sys_connect(&rab)) & 1 )
				{
					char name[NAM$C_MAXRSS + 128];
					char *outFname = name, *outFnameTemplate = argv[2];
					int dot = 0, semi = 0;
					RW_Params_t rwParams;

					if ( !nam.nam$b_dir && !nam.nam$b_name && nam.nam$b_type <= 1 )
					{
						/* Skip files with blank name and type */
						sys_close(&fab);
						if ( (cmdVerbose&CMD_VERBOSE_FN) )
						{
							char inpFileName[NAM$C_MAXRSS+1];
							int inLen = nam.nam$b_dir + nam.nam$b_name + nam.nam$b_ver;
							memcpy(inpFileName,nam.nam$l_dir,inLen);
							inpFileName[inLen] = 0;
							printf("%%COPY-I-FNPARSE, Skipped Parsing input file with blank names: '%s'\n", inpFileName);
						}
						continue;
					}
					if ( (cmdVerbose&CMD_VERBOSE_FN) )
					{
						char inpFileName[NAM$C_MAXRSS+1];
						int inLen = nam.nam$b_dir + nam.nam$b_name + nam.nam$b_type + nam.nam$b_ver;
						memcpy(inpFileName,nam.nam$l_dir,inLen);
						inpFileName[inLen] = 0;
						printf("%%COPY-I-FNPARSE, Parsing input file: %s\n", inpFileName);
					}
					
					if ( (strncmp(outFnameTemplate, "[*]", 3) == 0) && (nam.nam$b_dir > 0) )
					{
						char *tmp = nam.nam$l_dir;
						int ii;
						if ( (cmdVerbose&CMD_VERBOSE_FN) )
							printf("%%COPY-I-FNPARSE, Found wildcard in dir of output template: %s. Looping through directories\n", outFnameTemplate);
						for ( ii = 0; ii < nam.nam$b_dir; ii++ )
						{
							if ( (*tmp == '.') || (*tmp == ']') )
							{
								struct stat st;
								*outFname = 0;
								if ( stat(name, &st) || !S_ISDIR(st.st_mode) )
								{
									if ( (cmdVerbose&CMD_VERBOSE_FN) )
										printf("%%COPY-I-FNPARSE, Checked for directory: %s. Need to make one.\n", name);
									if ( !testMode )
									{
										if ( MKDIR(name, 0777) )
										{
											printf("%%COPY-E-MKDIR, failed mkdir('%s')\n", name);
											sys_error_str(sts, errMsg, sizeof(errMsg) - 1);
											errMsg[sizeof(errMsg) - 1] = 0;
											printf("-COPY-I-ERR: %s\n", errMsg);
										}
										else if ( (cmdVerbose&CMD_VERBOSE_DIR) || (options & OPT_COPY_VERBOSE) )
											printf("%%COPY-I-MKDIR, Created directory '%s'\n", name);
									}
									else if ( !(options & OPT_COPY_QUIET) )
										printf("%%COPY-I-INFO, Would have created directory: %s\n", name);
								}
								else
								{
									if ( (cmdVerbose&CMD_VERBOSE_FN) )
										printf("%%COPY-I-FNPARSE, Directory check failed. Might already exist: %s.\n", name);
								}
								*outFname++ = '/';
							}
							else if ( *tmp != '[' )
							{
								*outFname++ = *tmp;
							}
							if ( *tmp++ == ']' )
								break;
						}
						outFnameTemplate += 3;
						if ( (cmdVerbose&CMD_VERBOSE_FN) )
							printf("%%COPY-I-FNPARSE, Finished with directories: '%s'. Preparing output filename from template: %s.\n", name, outFnameTemplate);
						while ( *outFnameTemplate != '\0' )
						{
							if ( *outFnameTemplate == '*' )
							{
								outFnameTemplate++;	/* Eat the wildcard */
								if ( dot )
								{
									/* we are processing a type */
									memcpy(outFname, nam.nam$l_type + 1, nam.nam$b_type - 1);
									outFname += nam.nam$b_type - 1;
								}
								else if ( semi )
								{
									/* we are processing a version */
									memcpy(outFname, nam.nam$l_ver + 1, nam.nam$b_ver - 1);
									outFname += nam.nam$b_ver - 1;
								}
								else
								{
									unsigned length = nam.nam$b_name;
									/* If template does not include a type and version, then wildcard everything */
									if ( *outFnameTemplate == '\0' )
										length += nam.nam$b_type+nam.nam$b_ver;
									memcpy(outFname, nam.nam$l_name, length);
									outFname += length;
								}
							}
							else
							{
								if ( *outFnameTemplate == ';' )
								{
									semi = 1;
								}
								else if ( *outFnameTemplate == '.' )
								{
									dot = 1;
								}
								else
								{
									if ( strchr(":]\\/", *outFnameTemplate) )
									{
										dot = 0;
										semi = 0;
									}
								}
								*outFname++ = *outFnameTemplate++;
							}
						}
						*outFname = '\0';
						if ( (cmdVerbose&CMD_VERBOSE_FN) )
							printf("%%COPY-I-FNPARSE, Complete output filename computed as: %s.\n", name);
					}
					else if ( strchr(outFnameTemplate, '*') )
					{
						outFname = name;
						memcpy(outFname, nam.nam$l_name, nam.nam$b_name);
						outFname += nam.nam$b_name;
						if ( nam.nam$b_type )
						{
							memcpy(outFname, nam.nam$l_type, nam.nam$b_type);
							outFname += nam.nam$b_type;
							if ( nam.nam$b_ver )
							{
								memcpy(outFname, nam.nam$l_ver, nam.nam$b_ver);
								outFname += nam.nam$b_ver;
							}
						}
						*outFname = 0;
						if ( (cmdVerbose&CMD_VERBOSE_FN) )
							printf("%%COPY-I-FNPARSE, Wildcards in output template. Output filename computed as: %s.\n", name);
					}
					else
					{
						strncpy(name, outFnameTemplate, sizeof(name));
						if ( (cmdVerbose&CMD_VERBOSE_FN) )
							printf("%%COPY-I-FNPARSE, No wildcards in output template. Output filename computed as: %s.\n", name);
					}
					if ( !(options & OPT_COPY_DIRS) )
					{
						char *verPtr, *extPtr;
						verPtr = strrchr(name, ';');
						if ( verPtr )
							*verPtr = 0;
						extPtr = strrchr(name, '.');
						if ( extPtr && !strcmp(extPtr, ".DIR") )
						{
							testMode = 3;
							if ( (cmdVerbose&CMD_VERBOSE_FN) )
								printf("%%COPY-I-FNPARSE, Skipping write of directory file: %s\n", name);
						}
						if ( verPtr )
							*verPtr = ';';
					}
					if ( cDelim != ';' )
					{
						char *cPtr;
						cPtr = strchr(name,';');
						if ( cPtr )
						{
							*cPtr = cDelim;	/* Swap version delimiter */
							if ( (cmdVerbose&CMD_VERBOSE_FN) )
								printf("%%COPY-I-FNPARSE, Converted version delimiter. Outfilename is now: %s.\n", name);
						}
					}
					/* Tell sys_get() our record header and trailer buffers are to be shared with ubf */
					rab.rab$w_flg = RAB$M_BUF_SHARED;
					/* Tell sys_get() to get in binary mode or not */
					rab.rab$w_flg |= (options & OPT_COPY_BINARY) ? RAB$M_BIN : 0;
					/* Tell sys_get() whether /ignore option set */
					rab.rab$w_flg |= (options & OPT_COPY_IGNORE) ? RAB$M_FAL : 0;
					/* Tell sys_get() whether to add crlf instead of just lf */
					rab.rab$w_flg |= (options & OPT_COPY_CRLF) ? RAB$M_CRLF : 0;
					if ( !(rab.rab$w_flg & RAB$M_BIN) )
					{
						unsigned short extraFlg;
						/* Tell sys_get() whether or not and to handle RAT at EOR */
						extraFlg = 0;
						switch (fab.fab$b_rfm)
						{
						case FAB$C_VFC:
							if ( fab.fab$b_fsz && fab.fab$b_fsz != 2 )
								printf("%%COPY-W-VFC, file's VFC size is %d. Only a size of 2 is supported. No VFC handling.\n", fab.fab$b_fsz);
							else
								extraFlg = RAB$M_VFC;
							/* Fall through to VAR */
						case FAB$C_VAR:
							if ( (fab.fab$b_rat & PRINT_ATTR) )
								rab.rab$w_flg |= RAB$M_RAT | extraFlg;
#ifdef RAB$M_VARMRS
							else if ( !fab.fab$w_mrs )
								rab.rab$w_flg |= RAB$M_VARMRS;
#endif
							else
								rab.rab$w_flg |= RAB$M_BIN;
							break;
						case FAB$C_STMLF:
						case FAB$C_STMCR:
						case FAB$C_STM:
							if ( (options & OPT_COPY_STREAM) )
								rab.rab$w_flg |= RAB$M_BIN | RAB$M_NOTAG;
							else
							if ( (fab.fab$b_rat & PRINT_ATTR) )
								rab.rab$w_flg |= RAB$M_RAT;
							else
								rab.rab$w_flg |= RAB$M_BIN;
							break;
						case FAB$C_FIX:
							if ( (fab.fab$b_rat & PRINT_ATTR) )
								rab.rab$w_flg |= RAB$M_RAT;
							else
								rab.rab$w_flg |= RAB$M_BIN;
							break;
						default:
							break;
						}
					}
					memset(&rwParams, 0, sizeof(rwParams));
					rwParams.dat = &dat;
					rwParams.fab = &fab;
					rwParams.fhc = &fhc;
					rwParams.nam = &nam;
					rwParams.outFileName = name;
					rwParams.options = options;
					rwParams.rab = &rab;
					rwParams.testMode = testMode;
					/* Read the file */
					sts = copy_rw(&rwParams, 0);
					switch ( sts )
					{
					case RW_Sts_Good:
						break;
					case RW_Sts_Fatal:
						break;
					case RW_Sts_Retry:
#if 0
						/* Re-read/write the whole file again but as binary */
						sys_disconnect(&rab);
						sys_connect(&rab);
						rab.rab$w_flg |= RAB$M_BIN|RAB$M_RETRY;
						sts = copy_rw(&rwParams, 1);
#endif
						break;
					}
					filecount++;
					sys_disconnect(&rab);
#ifdef RAB$M_VARMRS
					if ( (rab.rab$w_flg & RAB$M_VARMRS) )
						fab.fab$w_mrs = rab.rab$w_tmpmrs;
#endif
					rsa[nam.nam$b_rsl] = '\0';
					if ( testMode )
						sts = RW_Sts_Good;
					if ( !testMode && (((rab.rab$w_flg & (RAB$M_FAL | RAB$M_RCE)) == (RAB$M_FAL | RAB$M_RCE)) || (!(rab.rab$w_flg & RAB$M_NOTAG) && (rab.rab$w_flg & RAB$M_BIN))) )
					{
						char *newFName;
						int nFNLen = 128;
						int sLen = strlen(name);
						newFName = (char *)malloc(sLen+nFNLen);
						if ( newFName )
						{
							const char *rfm;
							char ratsBuf[nFNLen];
							int nLen=0;

							newFName[0] = 0;
							strncat(newFName, name, sLen);
							rfm = getRfmAndRats(&fab,ratsBuf,sizeof(ratsBuf),1);
							if ( fab.fab$b_rfm == FAB$C_VFC )
								nLen += snprintf(newFName+sLen, nFNLen, "%c%s%d%c%d%s", cDelim, rfm, fab.fab$b_fsz, cDelim, fab.fab$w_mrs, ratsBuf);
							else
								nLen += snprintf(newFName+sLen, nFNLen, "%c%s%c%d%s", cDelim, rfm, cDelim, fab.fab$w_mrs, ratsBuf);
							if ( (rab.rab$w_flg&RAB$M_RETRY) || ((rab.rab$w_flg & (RAB$M_FAL | RAB$M_RCE)) == (RAB$M_FAL | RAB$M_RCE)) )
							{
								nLen += snprintf(newFName + sLen + nLen, nFNLen - nLen, "%ccorrupt_at_offset_%d", cDelim, rab.rab$l_tot);
							}
							if ( rename(name, newFName) < 0 )
							{
#if USE_STRERROR
								printf("%%COPY-E-Rename, Failed to rename '%s' to '%s': %s\n", name, newFName, strerror(errno));
#else
								printf("%COPY-E-Rename, failed to rename '%s' to '%s': errno=%d\n", name, newFName, strerror(errno));
#endif
							}
							else
							{
								const char *what = "saving in binary";
								if ( (rab.rab$w_flg&RAB$M_RETRY) || ((rab.rab$w_flg & (RAB$M_FAL | RAB$M_RCE)) == (RAB$M_FAL | RAB$M_RCE)) )
									what = "errors";
								printf("%%COPY-W-RENAME, Renamed '%s' to '%s' due to %s\n", name, newFName, what);
							}
							free(newFName);
						}
						else
						{
							printf("%%COPY-F-NOMEM: Out of memory needed to rename %s\n", name);
						}
						sts = RW_Sts_Good;
					}
					if ( sts != RW_Sts_Fatal )
					{
						if ( testMode < 3 )
						{
							const char *goodPlural = (rwParams.records == 1 ? "" : "s");
							if ( rwParams.badRecords )
							{
								const char *badPlural = (rwParams.badRecords == 1 ? "" : "s");
								const char *const Fmts[] =
								{
									"%%COPY-W-COPIED, %s copied to %s (%d good record%s, %d bad record%s)\n",
									"%%COPY-I-NOT_COPIED, would have copied %s to %s (%d good record%s, %d bad record%s)\n"
								};
								printf(Fmts[(options & OPT_COPY_TEST) ? 1 : 0], rsa, name, rwParams.records, goodPlural, rwParams.badRecords, badPlural);
								if ( (rab.rab$w_flg & RAB$M_FAL) )
									printf("-COPY-W-REC, First bad record count starts at offset %u\n", rab.rab$l_tot);
							}
							else if ( !(options & OPT_COPY_QUIET) )
							{
								const char *const Fmts[] =
								{
									"%%COPY-S-COPIED, %s copied to %s (%d record%s)\n",
									"%%COPY-I-NOT_COPIED, would have copied %s to %s (%d record%s)\n"
								};
								printf(Fmts[(options & OPT_COPY_TEST) ? 1 : 0], rsa, name, rwParams.records, goodPlural);
							}
						}
					}
#if 0
					else
					{
						char ratsBuf[128];
						const char *rfm;
						rfm = getRfmAndRats(&fab, ratsBuf, sizeof(ratsBuf),"_");
						printf("%%COPY-F-getRfmAndRats: %d for %s, rfm=%s, rat=%s\n", sts, rsa, rfm, ratsBuf);
						sts = 1;
					}
#endif
				}
				sys_close(&fab);
			}
		}
		if ( sts == RMS$_NMF )
			sts = 1;
	}
	if ( (sts & 1) )
	{
		if ( filecount > 0 )
			printf("%%COPY-S-NEWFILES, %d file%s created\n",
				   filecount, (filecount == 1 ? "" : "s"));
	}
	else
	{
		char errMsg[128];
		sys_error_str(sts, errMsg, sizeof(errMsg));
		printf("%%COPY-F-PARSE. Status: %d\n"
			   "%%COPY-I-PARSE, %s\n",
			   sts
			   , errMsg
			  );
	}
	return sts;
}

/* import: a file copy routine */


unsigned import(int argc, char *argv[], int qualc, char *qualv[])
{
	int sts;
	FILE *fromf;
	fromf = fopen(argv[1], "rb");
	if ( fromf != NULL )
	{
		struct FAB fab = cc$rms_fab;
		fab.fab$l_fna = argv[2];
		fab.fab$b_fns = strlen(fab.fab$l_fna);
		if ( (sts = sys_create(&fab)) & 1 )
		{
			struct RAB rab = cc$rms_rab;
			rab.rab$l_fab = &fab;
			if ( (sts = sys_connect(&rab)) & 1 )
			{
				char rec[MAX_RMSREC + 2];
				rab.rab$l_rbf = (unsigned char *)rec;
				rab.rab$w_usz = MAX_RMSREC;
				while ( fgets(rec, sizeof(rec), fromf) != NULL )
				{
					rab.rab$w_rsz = strlen(rec);
					sts = sys_put(&rab);
					if ( (sts & 1) == 0 )
						break;
				}
				sys_disconnect(&rab);
			}
			sys_close(&fab);
		}
		fclose(fromf);
		if ( !(sts & 1) )
		{
			printf("%%IMPORT-F-ERROR Status: %d\n", sts);
		}
	}
	else
	{
		printf("Can't open %s\n", argv[1]);
	}
	return sts;
}


/* diff: a simple file difference routine */

unsigned diff(int argc, char *argv[], int qualc, char *qualv[])
{
	int sts;
	struct FAB fab = cc$rms_fab;
	FILE *tof;
	int records = 0;
	fab.fab$l_fna = argv[1];
	fab.fab$b_fns = strlen(fab.fab$l_fna);
	tof = fopen(argv[2], "rb");
	if ( tof == NULL )
	{
		printf("Could not open file %s\n", argv[1]);
		sts = 0;
	}
	else
	{
		if ( (sts = sys_open(&fab)) & 1 )
		{
			struct RAB rab = cc$rms_rab;
			rab.rab$l_fab = &fab;
			if ( (sts = sys_connect(&rab)) & 1 )
			{
				char rec[MAX_RMSREC + 2], cpy[MAX_RMSREC + 1];
				rab.rab$l_ubf = (unsigned char *)rec;
				rab.rab$w_usz = MAX_RMSREC;
				while ( (sts = sys_get(&rab)) & 1 )
				{
					strcpy(rec + rab.rab$w_rsz, "\n");
					fgets(cpy, MAX_RMSREC, tof);
					if ( strcmp(rec, cpy) != 0 )
					{
						printf("%%DIFF-F-DIFFERENT Files are different!\n");
						sts = 4;
						break;
					}
					else
					{
						records++;
					}
				}
				sys_disconnect(&rab);
			}
			sys_close(&fab);
		}
		fclose(tof);
		if ( sts == RMS$_EOF )
			sts = 1;
	}
	if ( sts & 1 )
	{
		printf("%%DIFF-I-Compared %d records\n", records);
	}
	else
	{
		printf("%%DIFF-F-Error %d in difference\n", sts);
	}
	return sts;
}


/* typ: a file TYPE routine */

unsigned typ(int argc, char *argv[], int qualc, char *qualv[])
{
	int sts;
	int records = 0;
	struct FAB fab = cc$rms_fab;
	fab.fab$l_fna = argv[1];
	fab.fab$b_fns = strlen(fab.fab$l_fna);
	if ( (sts = sys_open(&fab)) & 1 )
	{
		struct RAB rab = cc$rms_rab;
		rab.rab$l_fab = &fab;
		if ( (sts = sys_connect(&rab)) & 1 )
		{
			char rec[MAX_RMSREC + 2];
			rab.rab$l_ubf = (unsigned char *)rec;
			rab.rab$w_usz = MAX_RMSREC;
			while ( (sts = sys_get(&rab)) & 1 )
			{
				unsigned rsz = rab.rab$w_rsz;
				if ( fab.fab$b_rat & PRINT_ATTR )
					rec[rsz++] = '\n';
				rec[rsz++] = '\0';
				fputs(rec, stdout);
				records++;
			}
			sys_disconnect(&rab);
		}
		sys_close(&fab);
		if ( sts == RMS$_EOF )
			sts = 1;
	}
	if ( (sts & 1) == 0 )
	{
		printf("%%TYPE-F-ERROR Status: %d\n", sts);
	}
	return sts;
}



/* search: a simple file search routine */

unsigned search(int argc, char *argv[], int qualc, char *qualv[])
{
	char defaultName[] = "";
	int sts = 0;
	int filecount = 0;
	int findcount = 0;
	char res[NAM$C_MAXRSS + 1], rsa[NAM$C_MAXRSS + 1];
	struct NAM nam = cc$rms_nam;
	struct FAB fab = cc$rms_fab;
	register char *searstr = argv[2];
	register char firstch = tolower(*searstr++);
	register char *searend = searstr + strlen(searstr);
	{
		char *str = searstr;
		while ( str < searend )
		{
			*str = tolower(*str);
			str++;
		}
	}
	nam = cc$rms_nam;
	fab = cc$rms_fab;
	nam.nam$l_esa = res;
	nam.nam$b_ess = NAM$C_MAXRSS;
	fab.fab$l_nam = &nam;
	fab.fab$l_fna = argv[1];
	fab.fab$b_fns = strlen(fab.fab$l_fna);
	fab.fab$l_dna = defaultName;
	fab.fab$b_dns = strlen(fab.fab$l_dna);
	sts = sys_parse(&fab);
	if ( sts & 1 )
	{
		nam.nam$l_rsa = rsa;
		nam.nam$b_rss = NAM$C_MAXRSS;
		fab.fab$l_fop = FAB$M_NAM;
		while ( (sts = sys_search(&fab)) & 1 )
		{
			sts = sys_open(&fab);
			if ( (sts & 1) == 0 )
			{
				printf("%%SEARCH-F-OPENFAIL, Open error: %d\n", sts);
			}
			else
			{
				struct RAB rab = cc$rms_rab;
				rab.rab$l_fab = &fab;
				if ( (sts = sys_connect(&rab)) & 1 )
				{
					int printname = 1;
					char rec[MAX_RMSREC + 2];
					filecount++;
					rab.rab$l_ubf = (unsigned char *)rec;
					rab.rab$w_usz = MAX_RMSREC;
					while ( (sts = sys_get(&rab)) & 1 )
					{
						char *strng = rec;
						char *strngend = strng + (rab.rab$w_rsz - (searend - searstr));
						while ( strng < strngend )
						{
							register char ch = *strng++;
							if ( ch == firstch || (ch >= 'A' && ch <= 'Z' && ch + 32 == firstch) )
							{
								register char *str = strng;
								register char *cmp = searstr;
								while ( cmp < searend )
								{
									register char ch2 = *str++;
									ch = *cmp;
									if ( ch2 != ch && (ch2 < 'A' || ch2 > 'Z' || ch2 + 32 != ch) )
										break;
									cmp++;
								}
								if ( cmp >= searend )
								{
									findcount++;
									rec[rab.rab$w_rsz] = '\0';
									if ( printname )
									{
										rsa[nam.nam$b_rsl] = '\0';
										printf("\n******************************\n%s\n\n", rsa);
										printname = 0;
									}
									fputs(rec, stdout);
									if ( fab.fab$b_rat & PRINT_ATTR )
										fputc('\n', stdout);
									break;
								}
							}
						}
					}
					sys_disconnect(&rab);
				}
				if ( sts == SS$_NOTINSTALL )
				{
					printf("%%SEARCH-W-NOIMPLEM, file operation not implemented\n");
					sts = 1;
				}
				sys_close(&fab);
			}
		}
		if ( sts == RMS$_NMF || sts == RMS$_FNF )
			sts = 1;
	}
	if ( sts & 1 )
	{
		if ( filecount < 1 )
		{
			printf("%%SEARCH-W-NOFILES, no files found\n");
		}
		else
		{
			if ( findcount < 1 )
				printf("%%SEARCH-I-NOMATCHES, no strings matched\n");
		}
	}
	else
	{
		printf("%%SEARCH-F-ERROR Status: %d\n", sts);
	}
	return sts;
}


/* del: you don't want to know! */

unsigned del(int argc, char *argv[], int qualc, char *qualv[])
{
	int sts = 0;
	struct NAM nam = cc$rms_nam;
	struct FAB fab = cc$rms_fab;
	char res[NAM$C_MAXRSS + 1], rsa[NAM$C_MAXRSS + 1];
	int filecount = 0;
	nam.nam$l_esa = res;
	nam.nam$b_ess = NAM$C_MAXRSS;
	fab.fab$l_nam = &nam;
	fab.fab$l_fna = argv[1];
	fab.fab$b_fns = strlen(fab.fab$l_fna);
	sts = sys_parse(&fab);
	if ( sts & 1 )
	{
		if ( nam.nam$b_ver < 2 )
		{
			printf("%%DELETE-F-NOVER, you must specify a version!!\n");
		}
		else
		{
			nam.nam$l_rsa = rsa;
			nam.nam$b_rss = NAM$C_MAXRSS;
			fab.fab$l_fop = FAB$M_NAM;
			while ( (sts = sys_search(&fab)) & 1 )
			{
				sts = sys_erase(&fab);
				if ( (sts & 1) == 0 )
				{
					printf("%%DELETE-F-DELERR, Delete error: %d\n", sts);
				}
				else
				{
					filecount++;
					rsa[nam.nam$b_rsl] = '\0';
					printf("%%DELETE-I-DELETED, Deleted %s\n", rsa);
				}
			}
			if ( sts == RMS$_NMF )
				sts = 1;
		}
		if ( sts & 1 )
		{
			if ( filecount < 1 )
			{
				printf("%%DELETE-W-NOFILES, no files deleted\n");
			}
		}
		else
		{
			printf("%%DELETE-F-ERROR Status: %d\n", sts);
		}
	}
	return sts;
}

/* test: you don't want to know! */
struct VCB *test_vcb;

unsigned test(int argc, char *argv[], int qualc, char *qualv[])
{
	int sts = 0;
	struct fiddef fid;
	sts = update_create(test_vcb, NULL, "Test.File", &fid, NULL);
	printf("Test status of %d (%s)\n", sts, argv[1]);
	return sts;
}

/* more test code... */

unsigned extend(int argc, char *argv[], int qualc, char *qualv[])
{
	int sts;
	struct FAB fab = cc$rms_fab;
	fab.fab$l_fna = argv[1];
	fab.fab$b_fns = strlen(fab.fab$l_fna);
	fab.fab$b_fac = FAB$M_UPD;
	if ( (sts = sys_open(&fab)) & 1 )
	{
		fab.fab$l_alq = 32;
		sts = sys_extend(&fab);
		sys_close(&fab);
	}
	if ( (sts & 1) == 0 )
	{
		printf("%%EXTEND-F-ERROR Status: %d\n", sts);
	}
	return sts;
}




/* show: the show command */

unsigned show(int argc, char *argv[], int qualc, char *qualv[])
{
	unsigned sts = 1;
	if ( keycomp(argv[1], "default") )
	{
		unsigned short curlen;
		char curdir[NAM$C_MAXRSS + 1];
		struct dsc_descriptor curdsc;
		curdsc.dsc_w_length = NAM$C_MAXRSS;
		curdsc.dsc_a_pointer = curdir;
		if ( (sts = sys_setddir(NULL, &curlen, &curdsc)) & 1 )
		{
			curdir[curlen] = '\0';
			printf(" %s\n", curdir);
		}
		else
		{
			printf("Error %d getting default\n", sts);
		}
	}
	else
	{
		if ( keycomp(argv[1], "time") )
		{
			unsigned sts;
			char timstr[24];
			unsigned short timlen;
			struct dsc_descriptor timdsc;
			timdsc.dsc_w_length = 20;
			timdsc.dsc_a_pointer = timstr;
			sts = sys_asctim(&timlen, &timdsc, NULL, 0);
			if ( sts & 1 )
			{
				timstr[timlen] = '\0';
				printf("  %s\n", timstr);
			}
			else
			{
				printf("%%SHOW-W-TIMERR error %d\n", sts);
			}
		}
		else
		{
			printf("%%SHOW-W-WHAT '%s'?\n", argv[1]);
		}
	}
	return sts;
}

unsigned setdef_count = 0;

void setdef(char *newdef)
{
	register unsigned sts;
	struct dsc_descriptor defdsc;
	defdsc.dsc_a_pointer = newdef;
	defdsc.dsc_w_length = strlen(defdsc.dsc_a_pointer);
	if ( (sts = sys_setddir(&defdsc, NULL, NULL)) & 1 )
	{
		setdef_count++;
	}
	else
	{
		printf("Error %d setting default to %s\n", sts, newdef);
	}
}

/* set: the set command */

unsigned set(int argc, char *argv[], int qualc, char *qualv[])
{
	unsigned sts = 1;
	if ( keycomp(argv[1], "default") )
	{
		setdef(argv[2]);
	}
	else
	{
		printf("%%SET-W-WHAT '%s'?\n", argv[1]);
	}
	return sts;
}


#ifndef VMSIO

/* The bits we need when we don't have real VMS routines underneath... */

unsigned dodismount(int argc, char *argv[], int qualc, char *qualv[])
{
	struct DEV *dev;
	register int sts = device_lookup(strlen(argv[1]), argv[1], 0, &dev);
	if ( sts & 1 )
	{
		if ( dev->vcb != NULL )
		{
			sts = dismount(dev->vcb);
		}
		else
		{
			sts = SS$_DEVNOTMOUNT;
		}
	}
	if ( (sts & 1) == 0 )
		printf("%%DISMOUNT-E-STATUS Error: %d\n", sts);
	return sts;
}


static const char *const mouquals[] = { "write", NULL };

unsigned domount(int argc, char *argv[], int qualc, char *qualv[])
{
	char *dev = argv[1];
	char *lab = argv[2];
	int sts = 1, devices = 0;
	char *devs[100], *labs[100];
	int options = checkquals(mouquals, qualc, qualv);
	while ( *lab != '\0' )
	{
		labs[devices++] = lab;
		while ( *lab != ',' && *lab != '\0' )
			lab++;
		if ( *lab != '\0' )
		{
			*lab++ = '\0';
		}
		else
		{
			break;
		}
	}
	devices = 0;
	while ( *dev != '\0' )
	{
		devs[devices++] = dev;
		while ( *dev != ',' && *dev != '\0' )
			dev++;
		if ( *dev != '\0' )
		{
			*dev++ = '\0';
		}
		else
		{
			break;
		}
	}
	if ( devices > 0 )
	{
		unsigned i;
		struct VCB *vcb;
		sts = mount(options, devices, devs, labs, &vcb);
		if ( sts & 1 )
		{
			for ( i = 0; i < vcb->devices; i++ )
				if ( vcb->vcbdev[i].dev != NULL )
					printf("%%MOUNT-I-MOUNTED, Volume %12.12s mounted on %s\n",
						   vcb->vcbdev[i].home.hm2$t_volname, vcb->vcbdev[i].dev->devnam);
			if ( setdef_count == 0 )
			{
				char *colon, defdir[256];
				strcpy(defdir, vcb->vcbdev[0].dev->devnam);
				colon = strchr(defdir, ':');
				if ( colon != NULL )
					*colon = '\0';
				strcpy(defdir + strlen(defdir), ":[000000]");
				setdef(defdir);
				test_vcb = vcb;
			}
		}
		else
		{
			printf("Mount failed with %d\n", sts);
		}
	}
	return sts;
}


void direct_show(void);
void phyio_show(void);

/* statis: print some simple statistics */

unsigned statis(int argc, char *argv[], int qualc, char *qualv[])
{
	printf("Statistics:-\n");
	direct_show();
	cache_show();
	phyio_show();
	return 1;
}

#endif


/* help: a routine to print a pre-prepared help text... */

static unsigned help(int argc, char *argv[], int qualc, char *qualv[])
{
	printf("\nODS2 %s\n", VERSION);
	printf(" Please send problems/comments to Paulnank@au1.ibm.com\n");
	printf(" Commands are:\n");
	printf("  copy        difference      directory     exit\n");
	printf("  help        mount           show          search\n"
		   "  set         type\n"
		   " Where:\n"
		  );
	printf("  copy [options] <from_VMS> <to_localhost>\n"
		   "      options can be zero or more of:\n"
		   "         /binary  - indicating to copy file(s) as binary.\n"
		   "         /crlf    - terminate records having RAT's with CRLF (Windows default)\n"
		   "         /dirs    - copy .DIR files as well as making directories.\n"
		   "         /ignore  - ignore errors and keep reading file(s)\n"
		   "         /lf      - terminate records having RAT's with LF (Linux default;cancels /crlf)\n"
		   "         /quiet   - copy without much squawking.\n"
		   "         /stream  - copy stream files as raw data.\n"
		   "         /test    - don't copy, but instead show what would have been done.\n"
		   "         /time    - copy creation and modification times to destination file too.\n"
		   "         /verbose - squawk with lots of diagnostics\n"
		   "         /vfc     - interpret the carriage control on VFC formatted files.\n"
		   "      <from_VMS>:\n"
		   "         is a filename in the VMS syntax\n"
		   "      <to_localhost>:\n"
		   "         is a filename in the VMS syntax but what will be written\n"
		  );
	printf(" difference <file1> <file2>\n"
		   "    Displays the difference between two files\n"
		  );
	printf(" directory [options] <file>\n"
		   "    Displays the directory contents and/or file details.\n"
		   "    options can be zero or more of:\n"
		   "    /date       - show dates\n"
		   "    /size       - show sizes\n"
		   "    /file       - show internal file ID's\n"
		   "    /full       - show all details of file (forces /date/size/file)\n"
		   "    <file>      - is the VMS specification of file and/or directory. Can include wildcards\n"
		  );
	printf(" exit\n"
		   "    Quietly quit the program.\n"
		  );
	printf(" help\n"
		   "    Displays this message.\n"
		  );
	printf(" mount file0 <...filen>\n"
		   "    Mounts file(s). Multiple files are expected to be volume sets (not checked).\n"
		   "    NOTE: On Unix (like) systems, filename cannot contain '/' characters.\n"
		   "          Suggest using symlinks as a workaround.\n"
		  );
	printf(" Examples:\n"
		   "    $ mount e:       (Mounts DOS/Windows physical disk E:)\n"
		   "    $ mount disk.img (Mounts virtual disk image)\n"
		  );
	printf("    $ search e:[vms_common.decc*...]*.h rms$_wld\n");
	printf("    $ set default e:[sys0.sysmgr]\n");
	printf("    $ copy *.com;-1 c:\\*.* (copies from -> to)\n");
	printf("    $ copy /quiet/time [*...]*.* [*]*.*  (copy all files while maintaing directory hierarchy)\n");
	printf("    $ copy/binary *.bin;-1 c:\\*.* (copy files in binary)\n");
	printf("    $ copy/quiet *.bin;-1 c:\\*.* (copy files quietly except for errors)\n");
	printf("    $ copy/test from-bla-bla  to-bla-bla  (just reports what it would have copied)\n");
	printf("    $ directory/file/size/date [-.sys*...].%%\n");
	printf("    $ exit\n");
	return 1;
}


/* informaion about the commands we know... */

const struct CMDSET
{
	const char *name;
	unsigned (*proc)(int argc, char *argv[], int qualc, char *qualv[]);
	int minlen;
	int minargs;
	int maxargs;
	int maxquals;
} cmdset[] = {
	{
		"copy", copy, 3, 3, 3, 15
	},
	{
		"import", import, 3, 3, 3, 0
	},
	{
		"delete", del, 3, 2, 2, 0
	},
	{
		"difference", diff, 3, 3, 3, 0
	},
	{
		"directory", dir, 3, 1, 2, 6
	},
	{
		"exit", NULL, 2, 0, 0, 0
	},
	{
		"extend", extend, 3, 2, 2, 0
	},
	{
		"help", help, 2, 1, 1, 0
	},
	{
		"quit", NULL, 2, 0, 0, 0
	},
	{
		"show", show, 2, 2, 2, 0
	},
	{
		"search", search, 3, 3, 3, 0
	},
	{
		"set", set, 3, 2, 3, 0
	},
#ifndef VMSIO
	{
		"dismount", dodismount, 3, 2, 2, 0
	},
	{
		"mount", domount, 3, 2, 3, 2
	},
	{
		"statistics", statis, 3, 1, 1, 0
	},
#endif
	{
		"test", test, 4, 2, 2, 0
	},
	{
		"type", typ, 3, 2, 2, 0
	},
	{
		NULL, NULL, 0, 0, 0, 0
	}
};


/* cmdexecute: identify and execute a command */

int cmdexecute(int argc, char *argv[], int qualc, char *qualv[])
{
	char *ptr = argv[0];
	const struct CMDSET *cmd = cmdset;
	unsigned cmdsiz = strlen(ptr);
	while ( *ptr != '\0' )
	{
		*ptr = tolower(*ptr);
		ptr++;
	}
	while ( cmd->name != NULL )
	{
		if ( cmdsiz >= cmd->minlen && cmdsiz <= strlen(cmd->name) )
		{
			if ( keycomp(argv[0], cmd->name) )
			{
				if ( cmd->proc == NULL )
				{
					return 0;
				}
				else
				{
					if ( argc < cmd->minargs || argc > cmd->maxargs )
					{
						printf("%%ODS2-E-PARAMS, Incorrect number of command parameters\n");
					}
					else
					{
						if ( qualc > cmd->maxquals )
						{
							printf("%%ODS2-E-QUALS, Too many command qualifiers\n");
						}
						else
						{
							(*cmd->proc)(argc, argv, qualc, qualv);
#ifndef VMSIO
							/* cache_flush();  */
#endif
						}
					}
					return 1;
				}
			}
		}
		cmd++;
	}
	printf("%%ODS2-E-ILLCMD, Illegal or ambiguous command '%s'\n", argv[0]);
	return 1;
}

/* cmdsplit: break a command line into its components */

int cmdsplit(char *str)
{
	int argc = 0, qualc = 0;
	char *argv[32], *qualv[32], empty[] = "";
	char *sp = str;
	int i;
	for ( i = 0; i < 32; i++ )
		argv[i] = qualv[i] = empty;
	while ( *sp != '\0' )
	{
		while ( *sp == ' ' )
			sp++;
		if ( *sp != '\0' )
		{
			if ( *sp == '/' )
			{
				*sp++ = '\0';
				qualv[qualc++] = sp;
			}
			else
			{
				argv[argc++] = sp;
			}
			while ( *sp != ' ' && *sp != '/' && *sp != '\0' )
				sp++;
			if ( *sp == '\0' )
			{
				break;
			}
			else
			{
				if ( *sp != '/' )
					*sp++ = '\0';
			}
		}
	}
	if ( argc > 0 )
		return cmdexecute(argc, argv, qualc, qualv);
	return 1;
}

#ifdef VMS
	#include <smgdef.h>
	#include <smg$routines.h>

char* getcmd(char *inp, char *prompt)
{
	struct dsc_descriptor prompt_d = { strlen(prompt), DSC$K_DTYPE_T,
		DSC$K_CLASS_S, prompt };
	struct dsc_descriptor input_d = { 1024, DSC$K_DTYPE_T,
		DSC$K_CLASS_S, inp };
	int status;
	char *retstat;
	static unsigned long key_table_id = 0;
	static unsigned long keyboard_id = 0;

	if ( key_table_id == 0 )
	{
		status = smg$create_key_table(&key_table_id);
		if ( status & 1 )
			status = smg$create_virtual_keyboard(&keyboard_id);
		if ( !(status & 1) )
			return (NULL);
	}

	status = smg$read_composed_line(&keyboard_id, &key_table_id,
									&input_d, &prompt_d, &input_d, 0, 0, 0, 0, 0, 0, 0);

	if ( status == SMG$_EOF )
		retstat = NULL;
	else
	{
		inp[input_d.dsc_w_length] = '\0';
		retstat = inp;
	}

	return (retstat);
}
#endif /* VMS */


/* main: the simple mainline of this puppy... */

#if defined(READLINE_HISTORY_FILENAME) && READLINE_HISTORY_LINES
static void preLoadHistory(void)
{
	struct stat st;
	int sts;
	/* Start with details of history file */
	sts = stat(READLINE_HISTORY_FILENAME, &st);
	if ( sts )
	{
/*		perror("Unable to stat " READLINE_HISTORY_FILENAME ":"); */
		return;
	}
	if ( st.st_size > 0 )
	{
		char **lines;
		/* Get a place to drop pointers into pre-initialised to NULL */
		lines = (char **)calloc(READLINE_HISTORY_LINES, sizeof(char *));
		/* Get a place to read the entire history file into */
		char *buff = (char *)malloc(st.st_size + 1);
		if ( buff && lines )
		{
			int ifd;
			/* Open the history file */
			ifd = open(READLINE_HISTORY_FILENAME, OPEN_FLAGS);
			if ( ifd >= 0 )
			{
				/* Read the entire history file */
				sts = read(ifd, buff, st.st_size);
				if ( sts > 0 )
				{
					int ii, index = 0;
					char *linePtr = buff;
					/* make sure there's a nul at the end of the buffer */
					buff[sts] = 0;
					/* loop through the text isolating and saving pointers to line starts */
					while ( linePtr < buff + st.st_size && *linePtr )
					{
						char *lineEnd;

						/* save the line start */
						lines[index] = linePtr;
						/* advance the line index */
						++index;
						/* wrap if necessary */
						if ( index >= READLINE_HISTORY_LINES )
							index = 0;
						/* find the end of the line */
						lineEnd = strchr(linePtr, '\n');
						if ( !lineEnd )
							break;  /* No end of line, so we're done */
						/* replace the newline with a nul */
						*lineEnd = 0;
						/* point to next line */
						linePtr = lineEnd + 1;
					}
					/* Loop through the saved line pointers and put them in the history */
					/* Although we start by putting the lines into history earliest to latest */
					for ( ii = 0; ii < READLINE_HISTORY_LINES; ++ii )
					{
						linePtr = lines[index];
						if ( linePtr )
							add_history(linePtr);
						++index;
						if ( index >= READLINE_HISTORY_LINES )
							index = 0;
					}
				}
			}
			close(ifd);
		}
		if ( lines )
			free(lines);
		if ( buff )
			free(buff);
	}
}

static void add_to_history_file(char *ptr)
{
	FILE *ofp = fopen(READLINE_HISTORY_FILENAME, "ab");
	if ( ofp )
	{
		fprintf(ofp, "%s\n", ptr);
		fclose(ofp);
	}
}
#endif	/* READLINE_HISTORY_xxx */

static struct option long_options[] = 
{
	 {"delimiter", optional_argument, NULL, 'd' }
	,{"help", no_argument, NULL, 'h'}
	,{"verbose", required_argument, NULL, 'v' }
	,{NULL,0,NULL,0}
};

int main(int argc, char *argv[])
{
#define STRSIZE 2048
	char cc, str[STRSIZE], *endp;
	FILE *atfile = NULL;
	int option_index;
	
	cDelim = ';';	/* Default version cDelimiter */
	while ( ( cc = getopt_long ( argc, argv, "d:hv:", long_options, &option_index )) != EOF )
	{
		switch ( cc )
		{
		case 'd':	/* cDelimiter */
			if ( !optarg )
			{
				printf("Defaulting version delimiter to '_'\n");
				cDelim = '_';
				break;
			}
			if ( !isprint(optarg[0]) )
			{
				printf("Argument to d (delimiter) must be printable. Is 0x%02X\n", optarg[0]);
				return 1;
			}
			cDelim = optarg[0];
			break;
		case 'h':
			printf("ODS2 %s\n", VERSION);
			printf("Usage:  %s [-v n][-d c][-h]\n",
					 argv[0] );
			printf("Where:\n"
				   "-d or   --delimit[=c] - Set filename delimiter (default is ';')\n"
				   "-h or   --help        - This message\n"
				   "-v n or --verbose=n   - Set verbose bit mask to n\n"
				   "                        0x01 - show directory operations\n"
				   "                        0x02 - show file read operations\n"
				   "                        0x04 - show file write operations\n"
				   "                        0x08 - show filename parsing operations\n"
				   );
			return 0;
		case 'v':
			endp = NULL;
			cmdVerbose = strtoul(optarg,&endp,0);
			if ( !endp || *endp || cmdVerbose < 0 || cmdVerbose >= CMD_VERBOSE_MAX)
			{
				printf("Bad --verbose parameter: '%s'. Must be a number 0x00 <= n < 0x%X\n", optarg, CMD_VERBOSE_MAX);
				return 1;
			}
			break;
		}
	}
	
	printf(" ODS2 %s\n", VERSION);
#if defined(READLINE_HISTORY_FILENAME) && READLINE_HISTORY_LINES
	preLoadHistory();
#endif
	while ( 1 )
	{
		char *ptr;
		if ( atfile != NULL )
		{
			if ( fgets(str, sizeof(str), atfile) == NULL )
			{
				fclose(atfile);
				atfile = NULL;
				*str = '\0';
			}
			else
			{
				ptr = strchr(str, '\n');
				if ( ptr != NULL )
					*ptr = '\0';
				printf("$> %s\n", str);
			}
		}
		else
		{
#ifdef VMS
			if ( getcmd(str, "$> ") == NULL )
				break;
#else
#if !USE_READLINE
			printf("$> ");
			if ( fgets(str, STRSIZE, stdin) == NULL )
				break;
			ptr = strchr(str, '\n');
			if ( ptr != NULL )
				*ptr = '\0';
#else	/* USE_READLINE */
			char *tptr;
			if ( !(ptr = readline("$> ")) )
				break;
			tptr = ptr;
			while ( isspace(*tptr) )
				++tptr;
			if ( *tptr )
			{
				add_history(ptr);
#if defined(READLINE_HISTORY_FILENAME) && READLINE_HISTORY_LINES
				add_to_history_file(ptr);
#endif
			}
			strncpy(str, ptr, STRSIZE);
			free(ptr);
			ptr = NULL;
#endif	/* USE_READLINE */
#endif
		}
		ptr = str;
		while ( *ptr == ' ' || *ptr == '\t' )
			ptr++;
		if ( strlen(ptr) && *ptr != '!' )
		{
			if ( *ptr == '@' )
			{
				if ( atfile != NULL )
				{
					printf("%%ODS2-W-INDIRECT, indirect indirection not permitted\n");
				}
				else
				{
					if ( (atfile = fopen(ptr + 1, "r")) == NULL )
					{
						perror("%%Indirection failed");
						printf("\n");
					}
				}
			}
			else
			{
				if ( (cmdsplit(ptr) & 1) == 0 )
					break;
			}
		}
	}
	if ( atfile != NULL )
		fclose(atfile);
	return 1;
}
