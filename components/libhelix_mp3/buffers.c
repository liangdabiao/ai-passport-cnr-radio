/* ***** BEGIN LICENSE BLOCK *****
    Version: RCSL 1.0/RPSL 1.0

    Portions Copyright (c) 1995-2002 RealNetworks, Inc. All Rights Reserved.

    The contents of this file, and the files included with this file, are
    subject to the current version of the RealNetworks Public Source License
    Version 1.0 (the "RPSL") available at
    http://www.helixcommunity.org/content/rpsl unless you have licensed
    the file under the RealNetworks Community Source License Version 1.0
    (the "RCSL") available at http://www.helixcommunity.org/content/rcsl,
    in which case the RCSL will apply. You may also obtain the license terms
    directly from RealNetworks.  You may not use this file except in
    compliance with the RPSL or, if you have a valid RCSL with RealNetworks
    applicable to this file, the RCSL.  Please see the applicable RPSL or
    RCSL for the rights, obligations and limitations governing use of the
    contents of the file.

    This file is part of the Helix DNA Technology. RealNetworks is the
    developer of the Original Code and owns the copyrights in the portions
    it created.

    This file, and the files included with this file, is distributed and made
    available on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
    EXPRESS OR IMPLIED, AND REALNETWORKS HEREBY DISCLAIMS ALL SUCH WARRANTIES,
    INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
    FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.

    Technology Compatibility Kit Test Suite(s) Location:
      http://www.helixcommunity.org/content/tck

    Contributor(s):

 * ***** END LICENSE BLOCK ***** */

/**************************************************************************************
    Fixed-point MP3 decoder
    Jon Recker (jrecker@real.com), Ken Cooke (kenc@real.com)
    June 2003

    buffers.c - allocation and freeing of internal MP3 decoder buffers

    All memory allocation for the codec is done in this file, so if you don't want
    to use other the default system malloc() and free() for heap management this is
    the only file you'll need to change.
 **************************************************************************************/

//#include "hlxclib/stdlib.h"		/* for malloc, free */
#include <stdlib.h>
#include <string.h>
#include "coder.h"

/* ---------------------------------------------------------------------------
 * NR patch (net-radio, 2026-10-23): ESP32-C3 启动堆只有 ~90KB，装不下
 * WiFi 驱动（动态 ~25KB）+ 解码器状态（~18KB 单块）+ 应用缓冲。把解码器
 * 的全部状态改成本文件内的静态 arena（进 .bss，不占堆），malloc/free 不再
 * 使用。上游注释本就把本文件定为「唯一需要改的分配点」，故改动收敛在此。
 * ------------------------------------------------------------------------- */
static union {
    unsigned long long force_align;
    unsigned char bytes[sizeof(MP3DecInfo) + sizeof(FrameHeader) +
                        sizeof(SideInfo) + sizeof(ScaleFactorInfo) +
                        sizeof(HuffmanInfo) + sizeof(DequantInfo) +
                        sizeof(IMDCTInfo) + sizeof(SubbandInfo) + 64];
} mp3Arena;
static unsigned int mp3ArenaUsed;

static void *ArenaAlloc(unsigned int nBytes) {
    const unsigned int aligned = (mp3ArenaUsed + 7u) & ~7u;
    if (aligned + nBytes > sizeof(mp3Arena.bytes)) {
        return 0;
    }
    void *p = &mp3Arena.bytes[aligned];
    mp3ArenaUsed = aligned + nBytes;
    return p;
}

/**************************************************************************************
    Function:    ClearBuffer

    Description: fill buffer with 0's

    Inputs:      pointer to buffer
                number of bytes to fill with 0

    Outputs:     cleared buffer

    Return:      none

    Notes:       slow, platform-independent equivalent to memset(buf, 0, nBytes)
 **************************************************************************************/
#define ClearBuffer(buf, nBytes) memset(buf, 0, nBytes) //fb
/*
    static void ClearBuffer(void *buf, int nBytes)
    {
	int i;
	unsigned char *cbuf = (unsigned char *)buf;

	for (i = 0; i < nBytes; i++)
		cbuf[i] = 0;

	//fb
	memset(buf, 0, nBytes)

	return;
    }
*/
/**************************************************************************************
    Function:    AllocateBuffers

    Description: allocate all the memory needed for the MP3 decoder

    Inputs:      none

    Outputs:     none

    Return:      pointer to MP3DecInfo structure (initialized with pointers to all
                  the internal buffers needed for decoding, all other members of
                  MP3DecInfo structure set to 0)

    Notes:       if one or more mallocs fail, function frees any buffers already
                  allocated before returning
 **************************************************************************************/
MP3DecInfo *AllocateBuffers(void) {
    MP3DecInfo *mp3DecInfo;
    FrameHeader *fh;
    SideInfo *si;
    ScaleFactorInfo *sfi;
    HuffmanInfo *hi;
    DequantInfo *di;
    IMDCTInfo *mi;
    SubbandInfo *sbi;

    mp3DecInfo = (MP3DecInfo *)ArenaAlloc(sizeof(MP3DecInfo));
    if (!mp3DecInfo) {
        return 0;
    }
    ClearBuffer(mp3DecInfo, sizeof(MP3DecInfo));

    fh = (FrameHeader *)ArenaAlloc(sizeof(FrameHeader));
    si = (SideInfo *)ArenaAlloc(sizeof(SideInfo));
    sfi = (ScaleFactorInfo *)ArenaAlloc(sizeof(ScaleFactorInfo));
    hi = (HuffmanInfo *)ArenaAlloc(sizeof(HuffmanInfo));
    di = (DequantInfo *)ArenaAlloc(sizeof(DequantInfo));
    mi = (IMDCTInfo *)ArenaAlloc(sizeof(IMDCTInfo));
    sbi = (SubbandInfo *)ArenaAlloc(sizeof(SubbandInfo));

    mp3DecInfo->FrameHeaderPS = (void *)fh;
    mp3DecInfo->SideInfoPS = (void *)si;
    mp3DecInfo->ScaleFactorInfoPS = (void *)sfi;
    mp3DecInfo->HuffmanInfoPS = (void *)hi;
    mp3DecInfo->DequantInfoPS = (void *)di;
    mp3DecInfo->IMDCTInfoPS = (void *)mi;
    mp3DecInfo->SubbandInfoPS = (void *)sbi;

    if (!fh || !si || !sfi || !hi || !di || !mi || !sbi) {
        FreeBuffers(mp3DecInfo);	/* safe to call - only frees memory that was successfully allocated */
        return 0;
    }

    /* important to do this - DSP primitives assume a bunch of state variables are 0 on first use */
    ClearBuffer(fh,  sizeof(FrameHeader));
    ClearBuffer(si,  sizeof(SideInfo));
    ClearBuffer(sfi, sizeof(ScaleFactorInfo));
    ClearBuffer(hi,  sizeof(HuffmanInfo));
    ClearBuffer(di,  sizeof(DequantInfo));
    ClearBuffer(mi,  sizeof(IMDCTInfo));
    ClearBuffer(sbi, sizeof(SubbandInfo));

    return mp3DecInfo;
}

#define SAFE_FREE(x)	{if (x)	free(x);	(x) = 0;}	/* helper macro */

/**************************************************************************************
    Function:    FreeBuffers

    Description: frees all the memory used by the MP3 decoder

    Inputs:      pointer to initialized MP3DecInfo structure

    Outputs:     none

    Return:      none

    Notes:       safe to call even if some buffers were not allocated (uses SAFE_FREE)
 **************************************************************************************/
void FreeBuffers(MP3DecInfo *mp3DecInfo) {
    if (!mp3DecInfo) {
        return;
    }

    /* NR patch: 静态 arena 不释放，只断开指针（重复 Init 前重置用）。 */
    mp3DecInfo->FrameHeaderPS = 0;
    mp3DecInfo->SideInfoPS = 0;
    mp3DecInfo->ScaleFactorInfoPS = 0;
    mp3DecInfo->HuffmanInfoPS = 0;
    mp3DecInfo->DequantInfoPS = 0;
    mp3DecInfo->IMDCTInfoPS = 0;
    mp3DecInfo->SubbandInfoPS = 0;
}
