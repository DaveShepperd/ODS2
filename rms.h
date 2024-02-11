/* RMS.h v1.3   RMS routine definitions */

/*
        This is part of ODS2 written by Paul Nankervis,
        email address:  Paulnank@au1.ibm.com

        ODS2 is distributed freely for all members of the
        VMS community to use. However all derived works
        must maintain comments in their source to acknowledge
        the contibution of the original author.
*/

#ifndef RMS$_RTB

#include "vmstime.h"

#define RMS$_RTB 98728	/* Record too large for user's buffer */
#define RMS$_EOF 98938	/* End of file */
#define RMS$_FNF 98962	/* File not found */
#define RMS$_NMF 99018	/* No matching file */
#define RMS$_WCC 99050	/* Invalid wildcard context */
#define RMS$_BUG 99380	/* Internal bug? */
#define RMS$_DIR 99532	/* Bad directory syntax */
#define RMS$_ESS 99588	/* Bad extension syntax */
#define RMS$_FNM 99628	/* Bad filename syntax */
#define RMS$_IFI 99684	/* Bad file index? */
#define RMS$_NAM 99804	/* No filename provided */
#define RMS$_RSS 99988	/* Filename size problem */
#define RMS$_RSZ 100004	/* Bad output record size */
#define RMS$_WLD 100164	/* Failed to find file via wildcard? */
#define RMS$_DNF 114762	/* Directory not found */

#define NAM$C_MAXRSS 255
#define NAM$M_SYNCHK 1
#define FAB$M_NAM 0x1000000

#define XAB$C_DAT 18
#define XAB$C_FHC 29
#define XAB$C_PRO 19


struct XABDAT {
    void *xab$l_nxt;	// Pointer to next XAB struct
    unsigned char xab$b_cod;	// Code ID'ing this struct as a XABDAT (preset to XAB$C_DAT)
	unsigned char xab$b_bln;	// Block length
    unsigned short xab$w_rvn;	// Number of times file opened for write
    VMSTIME xab$q_bdt;	// Backup date
    VMSTIME xab$q_cdt;	// Creation date
    VMSTIME xab$q_edt;	// Expiration date
    VMSTIME xab$q_rdt;	// Revision date
	VMSTIME xab$q_acc;	// Last accessed time
	VMSTIME xab$q_att;	// Last time file attributes modified
	VMSTIME xab$q_mod;	// Last time file modified
};

#ifdef RMS$INITIALIZE
struct XABDAT cc$rms_xabdat = {
	 NULL					// nxt
	,XAB$C_DAT				// cod
	,sizeof(struct XABDAT)	// bln
	,0						// rvn
    ,VMSTIME_ZERO			// bdt
	,VMSTIME_ZERO			// cdt
    ,VMSTIME_ZERO			// edt
	,VMSTIME_ZERO			// rdt
	,VMSTIME_ZERO			// acc
	,VMSTIME_ZERO			// att
	,VMSTIME_ZERO			// mod
};
#else
extern struct XABDAT cc$rms_xabdat;
#endif



struct XABFHC {
    void *xab$l_nxt;				// Pointer to next XAB struct
    unsigned char xab$b_cod;		// Code ID'ing this struct as a XABFHC (preset to XAB$C_FHC)
	unsigned char xab$b_bln;		// length of this struct
    unsigned char xab$b_atr;		// Record attributes (same as fab$b_rat)
    unsigned char xab$b_bkz;		// Bucket size (same as fab$b_bsz)
	unsigned char xab$b_hsz;		// Fixed length control header size (same as fab$b_fsz)
    unsigned short xab$w_dxq;		// Default file extension quantity (same as fab$w_deq)
	unsigned short xab$w_ffb;		// First free byte in end of file block
	unsigned short xab$w_gbc;		// Default global buffer count
	unsigned short xab$w_verlimit;	// Version limit for this file
	unsigned short xab$w_lrl;		// Longest record length 
    int xab$l_ebk;					// End of file block
    int xab$l_hbk;					// Higest virtual block in file (same as fab$l_alq)
};

#ifdef RMS$INITIALIZE
struct XABFHC cc$rms_xabfhc = {
	 NULL						// Next
	,XAB$C_FHC					// cod
	,sizeof(struct XABFHC)		// bln
	,0							// atr
	,0							// bkz
	,0							// hsz
	,0							// dxq
	,0							// ffb
	,0							// gbc
	,0							// verlimit
	,0							// lrl
	,0							// ebk
	,0							// hbk
};
#else
extern struct XABFHC cc$rms_xabfhc;
#endif



struct XABPRO {
    void *xab$l_nxt;
    int xab$b_cod;
    int xab$w_pro;
    int xab$l_uic;
};

#ifdef RMS$INITIALIZE
struct XABPRO cc$rms_xabpro = {NULL,XAB$C_PRO,0,0};
#else
extern struct XABPRO cc$rms_xabpro;
#endif



#define NAM$M_WILDCARD 0x100

struct NAM {
    unsigned short nam$w_did_num;
    unsigned short nam$w_did_seq;
    unsigned char nam$b_did_rvn;
    unsigned char nam$b_did_nmx;
    unsigned short nam$w_fid_num;
    unsigned short nam$w_fid_seq;
    unsigned char nam$b_fid_rvn;
    unsigned char nam$b_fid_nmx;
    int nam$b_ess;
    int nam$b_rss;
    int nam$b_esl;
    char *nam$l_esa;
    int nam$b_rsl;
    char *nam$l_rsa;
    int nam$b_dev;
    char *nam$l_dev;
    int nam$b_dir;
    char *nam$l_dir;
    int nam$b_name;
    char *nam$l_name;
    int nam$b_type;
    char *nam$l_type;
    int nam$b_ver;
    char *nam$l_ver;
    void *nam$l_wcc;
    int nam$b_nop;
    int nam$l_fnb;
};

#ifdef RMS$INITIALIZE
struct NAM cc$rms_nam = {0,0,0,0,0,0,0,0,0,0,0,NULL,0,NULL,0,NULL,0,NULL,0,NULL,0,NULL,0,NULL,0,0,0};
#else
extern struct NAM cc$rms_nam;
#endif

/* Indicies in rab$w_rfa[] array*/
#define RAB$C_SEQ 0	// 0 and 1 form a 32 bit block number
#define RAB$C_RFA 2	// index into block for next record to get

/* Flags found in rab$w_flg */
#define RAB$M_SPC 1		/* Read STREAM records as just raw data similar to fixed but not exactly like fixed */
#define RAB$M_FAL 2		/* Continue reading file on record count error. Set RAB$M_RCE and rab$l_tot becomes frozen at that place */
#define RAB$M_RCE 4		/* Set in rab$w_flg by sys_get() when record count error found. Rest of file is plain binary */

struct RAB {
    struct FAB *rab$l_fab;
    char *rab$l_ubf;
    char *rab$l_rhb;
    char *rab$l_rbf;
    unsigned rab$w_usz;
    unsigned rab$w_rsz;
    int rab$b_rac;
    unsigned rab$l_tot;			/* total bytes read from file so far. */
    unsigned short rab$w_flg;		/* Custom record processing flag(s) */
    unsigned short rab$w_rfa[3];
};

#ifdef RMS$INITIALIZE
struct RAB cc$rms_rab = {NULL,NULL,NULL,NULL,0,0,0,0,0,{0,0,0}};
#else
extern struct RAB cc$rms_rab;
#endif



#define FAB$C_SEQ 0
#define FAB$C_REL 16
#define FAB$C_IDX 32
#define FAB$C_HSH 48

#define FAB$M_FTN 1
#define FAB$M_CR  2
#define FAB$M_PRN 4
#define FAB$M_BLK 8

#define FAB$M_PUT 0x1
#define FAB$M_GET 0x2
#define FAB$M_DEL 0x4
#define FAB$M_UPD 0x8
#define FAB$M_TRN 0x10
#define FAB$M_BIO 0x20
#define FAB$M_BRO 0x40
#define FAB$M_EXE 0x80

#define FAB$C_UDF 0
#define FAB$C_FIX 1
#define FAB$C_VAR 2
#define FAB$C_VFC 3
#define FAB$C_STM 4
#define FAB$C_STMLF 5
#define FAB$C_STMCR 6

struct FAB {
    struct NAM *fab$l_nam;
    int fab$w_ifi;
    char *fab$l_fna;
    char *fab$l_dna;
    int fab$b_fns;
    int fab$b_dns;
    int fab$l_alq;
    int fab$b_bks;
    int fab$w_deq;
    int fab$b_fsz;
    int fab$w_gbc;
    int fab$w_mrs;
    int fab$l_fop;
    int fab$b_org;
    int fab$b_rat;
    int fab$b_rfm;
    int fab$b_fac;
    void *fab$l_xab;
};

#ifdef RMS$INITIALIZE
struct FAB cc$rms_fab = {NULL,0,NULL,NULL,0,0,0,0,0,0,0,0,0,0,0,0,0,NULL};
#else
extern struct FAB cc$rms_fab;
#endif


#ifndef NO_DOLLAR
#define sys$search      sys_search
#define sys$parse       sys_parse
#define sys$setddir     sys_setddir
#define sys$connect     sys_connect
#define sys$disconnect  sys_disconnect
#define sys$get         sys_get
#define sys$display     sys_display
#define sys$close       sys_close
#define sys$open        sys_open
#define sys$create      sys_create
#define sys$erase       sys_erase
#endif

unsigned sys_search(struct FAB *fab);
unsigned sys_parse(struct FAB *fab);
unsigned sys_connect(struct RAB *rab);
unsigned sys_disconnect(struct RAB *rab);
unsigned sys_get(struct RAB *rab);
unsigned sys_put(struct RAB *rab);
unsigned sys_display(struct FAB *fab);
unsigned sys_close(struct FAB *fab);
unsigned sys_open(struct FAB *fab);
unsigned sys_create(struct FAB *fab);
unsigned sys_erase(struct FAB *fab);
unsigned sys_extend(struct FAB *fab);
unsigned sys_setddir(struct dsc_descriptor *newdir,unsigned short *oldlen,
                     struct dsc_descriptor *olddir);
void sys_error_str(int rmsSts, char *dest, int destLen);
#endif
