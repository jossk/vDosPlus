#pragma once

#if !defined __LAZYFLAGS_H
#define __LAZYFLAG_H

// Flag Handling
Bit32u get_CF(void);
Bit32u get_AF(void);
Bit32u get_ZF(void);
Bit32u get_SF(void);
Bit32u get_OF(void);
Bit32u get_PF(void);

Bitu FillFlags(void);
void FillFlagsNoCFOF(void);
void DestroyConditionFlags(void);

#include "regs.h"

struct LazyFlags {
    GenReg32 var1, var2, res;
	Bitu type;
//	Bitu prev_type;
	Bitu oldcf;
};

#define lf_var1b lflags.var1.byte[BL_INDEX]
#define lf_var2b lflags.var2.byte[BL_INDEX]
#define lf_resb lflags.res.byte[BL_INDEX]

#define lf_var1w lflags.var1.word[W_INDEX]
#define lf_var2w lflags.var2.word[W_INDEX]
#define lf_resw lflags.res.word[W_INDEX]

#define lf_var1d lflags.var1.dword[DW_INDEX]
#define lf_var2d lflags.var2.dword[DW_INDEX]
#define lf_resd lflags.res.dword[DW_INDEX]


extern LazyFlags lflags;

#define SETFLAGSb(FLAGB)													\
	{																		\
	SETFLAGBIT(OF,get_OF());												\
	lflags.type=t_UNKNOWN;													\
	CPU_SetFlags(FLAGB,FMASK_NORMAL & 0xff);								\
	}

#define LoadCF SETFLAGBIT(CF, get_CF());
#define LoadZF SETFLAGBIT(ZF, get_ZF());
#define LoadSF SETFLAGBIT(SF, get_SF());
#define LoadOF SETFLAGBIT(OF, get_OF());
#define LoadAF SETFLAGBIT(AF, get_AF());

#define TFLG_O		(get_OF())
#define TFLG_NO		(!get_OF())
#define TFLG_B		(get_CF())
#define TFLG_NB		(!get_CF())
#define TFLG_Z		(get_ZF())
#define TFLG_NZ		(!get_ZF())
#define TFLG_BE		(get_CF() || get_ZF())
#define TFLG_NBE	(!get_CF() && !get_ZF())
#define TFLG_S		(get_SF())
#define TFLG_NS		(!get_SF())
#define TFLG_P		(get_PF())
#define TFLG_NP		(!get_PF())
#define TFLG_L		((get_SF() != 0) != (get_OF() != 0))
#define TFLG_NL		((get_SF() != 0) == (get_OF() != 0))
#define TFLG_LE		(get_ZF() || ((get_SF() != 0) != (get_OF() != 0)))
#define TFLG_NLE	(!get_ZF() && ((get_SF() != 0) == (get_OF() != 0)))

//Types of Flag changing instructions
enum {
	t_UNKNOWN=0,
	t_ADDb,		t_ADDw,		t_ADDd, 
	t_ORb,		t_ORw,		t_ORd, 
	t_ADCb,		t_ADCw,		t_ADCd,
	t_SBBb,		t_SBBw,		t_SBBd,
	t_ANDb,		t_ANDw,		t_ANDd,
	t_SUBb,		t_SUBw,		t_SUBd,
	t_XORb,		t_XORw,		t_XORd,
	t_CMPb,		t_CMPw,		t_CMPd,
	t_INCb,		t_INCw,		t_INCd,
	t_DECb,		t_DECw,		t_DECd,
	t_TESTb,	t_TESTw,	t_TESTd,
	t_SHLb,		t_SHLw,		t_SHLd,
	t_SHRb,		t_SHRw,		t_SHRd,
	t_SARb,		t_SARw,		t_SARd,
	t_ROLb,		t_ROLw,		t_ROLd,
	t_RORb,		t_RORw,		t_RORd,
	t_RCLb,		t_RCLw,		t_RCLd,
	t_RCRb,		t_RCRw,		t_RCRd,
	t_NEGb,		t_NEGw,		t_NEGd,
	
	t_DSHLw,	t_DSHLd,
	t_DSHRw,	t_DSHRd,
	t_MUL,		t_DIV,
	t_NOTDONE,
	t_LASTFLAG
};

inline void DestroyConditionFlags(void)	{
	lflags.type = t_UNKNOWN;
}


#endif
