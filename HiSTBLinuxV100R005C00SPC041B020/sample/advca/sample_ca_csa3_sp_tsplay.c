/*********************************************************************************
*
*  Copyright (C) 2014 Hisilicon Technologies Co., Ltd.  All rights reserved. 
*
*  This program is confidential and proprietary to Hisilicon Technologies Co., Ltd.
*  (Hisilicon), and may not be copied, reproduced, modified, disclosed to
*  others, published or used, in whole or in part, without the express prior
*  written permission of Hisilicon.
*
***********************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>

#include <assert.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include "hi_unf_common.h"
#include "hi_unf_ecs.h"
#include "hi_unf_avplay.h"
#include "hi_unf_sound.h"
#include "hi_unf_disp.h"
#include "hi_unf_vo.h"
#include "hi_unf_descrambler.h"
#include "hi_unf_advca.h"
#include "hi_unf_demux.h"
#include "hi_adp_hdmi.h"
#include "hi_adp_mpi.h"

#include "HA.AUDIO.MP3.decode.h"
#include "HA.AUDIO.MP2.decode.h"
#include "HA.AUDIO.AAC.decode.h"
#include "sample_ca_common.h"

FILE                *g_pTsFile = HI_NULL;
static pthread_t    g_TsThd;
static pthread_mutex_t g_TsMutex;
static HI_BOOL      g_bStopTsThread = HI_FALSE;
HI_HANDLE           g_TsBuf;
PMT_COMPACT_TBL     *g_pProgTbl = HI_NULL;
HI_S32              g_CwType = 0;
HI_S32              g_KeyLadderType = 0;
#define             PLAY_DMX_ID  0

/*===============================================================*/
/* stream: NoPCD3_DSC_Emi0x0001_01.02.ts
** Notes : If you want to test Conax CSA3 keyladder(2 level) or SP Keyladder, please set CSA3_ROOT_KEY 
**         or SP_ROOT_KEY first,then use g_u8CSA3Rootkey or g_u8SPRootkey encrypt g_u8ClearSessionkey
**         by TDES-ECB. The output is g_u8EncrptedSessionKey. Build this sample.
*/
//g_u8CSA3Rootkey:         A5 03 03 03 03 03 03 03 03 03 03 03 03 03 03 03
//g_u8SPRootkey:           A5 03 03 03 03 03 03 03 03 03 03 03 03 03 03 03
//g_u8ClearSessionkey:     70 F1 A2 D4 D6 C5 D0 F1 85 34 23 CC 08 D4 03 AC
//g_u8EncrptedSessionKey:  A8 E1 7F A0 5F 9E 47 FF 16 11 0B 59 4F 7C 17 F6
static HI_U8 g_u8EncrptedSessionKey[16] = {0xA8,0xE1,0x7F,0xA0,0x5F,0x9E,0x47,0xFF,0x16,0x11,0x0B,0x59,0x4F,0x7C,0x17,0xF6};

//g_u8_CSA3_rootKey:         a5 03 03 03 03 03 03 03 03 03 03 03 03 03 03 03
//g_u8_CSA3_ClearOddKey:     d9 ec 13 d8 fa fd d9 d0 38 08 8f cf a0 df 69 e8
//g_u8_CSA3_ClearEvenKey:    69 d7 ef 2f a8 84 52 7e f1 70 31 92 db 72 59 a6
//g_u8_CSA3_EncrytedOddKey:  B0 E5 C6 C5 AD E8 14 0E 4B E2 83 90 BD 4B 27 42
//g_u8_CSA3_EncrytedEvenKey: 51 FA F0 0A 3B 67 59 42 27 C4 80 7E 39 B1 85 10
static HI_U8 g_u8_CSA3_ClearOddKey[16]     = {0xd9,0xec,0x13,0xd8,0xfa,0xfd,0xd9,0xd0,0x38,0x08,0x8f,0xcf,0xa0,0xdf,0x69,0xe8};
static HI_U8 g_u8_CSA3_ClearEvenKey[16]    = {0x69,0xd7,0xef,0x2f,0xa8,0x84,0x52,0x7e,0xf1,0x70,0x31,0x92,0xdb,0x72,0x59,0xa6};
static HI_U8 g_u8_CSA3_EncrytedOddKey[16]  = {0xB0,0xE5,0xC6,0xC5,0xAD,0xE8,0x14,0x0E,0x4B,0xE2,0x83,0x90,0xBD,0x4B,0x27,0x42};
static HI_U8 g_u8_CSA3_EncrytedEvenKey[16] = {0x51,0xFA,0xF0,0x0A,0x3B,0x67,0x59,0x42,0x27,0xC4,0x80,0x7E,0x39,0xB1,0x85,0x10};

HI_S32 loadcsa3cw(HI_HANDLE hKey, HI_U32 CwType, HI_U32 ProgNum)
{
    HI_S32 Ret;

    if ((ProgNum < 2) || (ProgNum > 3))  //只需要测试前面2节目，后面的节目与Nosc3有关，暂时不测试。
    {
        return HI_SUCCESS;
    }

    /* load cw*/
    if (0 == CwType)
    {
        Ret = HI_UNF_DMX_SetDescramblerOddKey(hKey, g_u8_CSA3_ClearOddKey);
        Ret |= HI_UNF_DMX_SetDescramblerEvenKey(hKey, g_u8_CSA3_ClearEvenKey);
        if (HI_SUCCESS != Ret)
        {
            HI_DEBUG_ADVCA("call HI_UNF_DMX_SetDescramblerKey failed\n");
            return HI_FAILURE;
        }
    }
    else
    {
        HI_UNF_ADVCA_KEYLADDER_ATTR_S stKeyladderAttr;
        HI_UNF_ADVCA_OTP_FUSE_E enOtpFuse;
        HI_UNF_ADVCA_OTP_ATTR_S stOtpFuseAttr;

        memset(&stKeyladderAttr, 0, sizeof(HI_UNF_ADVCA_KEYLADDER_ATTR_S));
        stKeyladderAttr.unKlAttr.stCSA3KlAttr.enCsa3KlAttr = HI_UNF_ADVCA_KEYLADDER_CSA3_ATTR_ALG;
        stKeyladderAttr.unKlAttr.stCSA3KlAttr.enAlgType = HI_UNF_ADVCA_ALG_TYPE_TDES;
        Ret = HI_UNF_ADVCA_SetKeyLadderAttr(HI_UNF_ADVCA_KEYLADDER_CSA3, &stKeyladderAttr);	
        
        enOtpFuse = HI_UNF_ADVCA_OTP_CSA3_KL_LEVEL_SEL;
        memset(&stOtpFuseAttr, 0, sizeof(stOtpFuseAttr));
        Ret = HI_UNF_ADVCA_GetOtpFuse(enOtpFuse, &stOtpFuseAttr);
        if(Ret != HI_SUCCESS)
        {
            HI_DEBUG_ADVCA("call HI_UNF_ADVCA_GetOtpFuse get CSA3 Key-ladder level failed\n");
            return HI_FAILURE;
        }
        printf("CSA3 key-level stage is:0x%x\n", stOtpFuseAttr.unOtpFuseAttr.stKeyladderLevSel.enKeyladderLevel);
        if(stOtpFuseAttr.unOtpFuseAttr.stKeyladderLevSel.enKeyladderLevel >= HI_UNF_ADVCA_KEYLADDER_LEV2)
        {
            printf("Level 1 is setting\n");
            memset(&stKeyladderAttr, 0, sizeof(HI_UNF_ADVCA_KEYLADDER_ATTR_S));
            stKeyladderAttr.unKlAttr.stCSA3KlAttr.enCsa3KlAttr = HI_UNF_ADVCA_KEYLADDER_CSA3_ATTR_SESSION_KEY;
    		stKeyladderAttr.unKlAttr.stCSA3KlAttr.enStage = HI_UNF_ADVCA_KEYLADDER_LEV1;
            memcpy(stKeyladderAttr.unKlAttr.stCSA3KlAttr.u8SessionKey, g_u8EncrptedSessionKey, 16);
            Ret |= HI_UNF_ADVCA_SetKeyLadderAttr(HI_UNF_ADVCA_KEYLADDER_CSA3, &stKeyladderAttr);
        }
      	
        Ret |= HI_UNF_DMX_SetDescramblerOddKey(hKey, g_u8_CSA3_EncrytedOddKey);
        Ret |= HI_UNF_DMX_SetDescramblerEvenKey(hKey, g_u8_CSA3_EncrytedEvenKey);
        if (HI_SUCCESS != Ret)
        {
            HI_DEBUG_ADVCA("call HI_UNF_DMX_SetDescramblerEvenKey failed\n");
            return HI_FAILURE;
        }
    }

    return HI_SUCCESS;
}

HI_S32 loadspcw(HI_HANDLE hKey, HI_U32 CwType, HI_U32 ProgNum)
{
    HI_S32 Ret;

    if ((ProgNum < 2) || (ProgNum > 3))  //只需要测试前面2节目，后面的节目与Nosc3有关，暂时不测试。
    {
        return HI_SUCCESS;
    }

    /* load cw*/
    if (0 == CwType)
    {
        Ret = HI_UNF_DMX_SetDescramblerOddKey(hKey, g_u8_CSA3_ClearOddKey);
        Ret |= HI_UNF_DMX_SetDescramblerEvenKey(hKey, g_u8_CSA3_ClearEvenKey);
        if (HI_SUCCESS != Ret)
        {
            HI_DEBUG_ADVCA("call HI_UNF_DMX_SetDescramblerKey failed\n");
            return HI_FAILURE;
        }
    }
    else
    {
        HI_UNF_ADVCA_KEYLADDER_ATTR_S stKeyladderAttr;
        HI_UNF_ADVCA_OTP_FUSE_E enOtpFuse;
        HI_UNF_ADVCA_OTP_ATTR_S stOtpFuseAttr;

        memset(&stKeyladderAttr, 0, sizeof(HI_UNF_ADVCA_KEYLADDER_ATTR_S));
        stKeyladderAttr.unKlAttr.stSPKlAttr.enSPKlAttr = HI_UNF_ADVCA_KEYLADDER_SP_ATTR_ENABLE;
        stKeyladderAttr.unKlAttr.stSPKlAttr.bEnable = HI_TRUE;
        Ret = HI_UNF_ADVCA_SetKeyLadderAttr(HI_UNF_ADVCA_KEYLADDER_SP, &stKeyladderAttr);	

        memset(&stKeyladderAttr, 0, sizeof(HI_UNF_ADVCA_KEYLADDER_ATTR_S));
        stKeyladderAttr.unKlAttr.stSPKlAttr.enSPKlAttr = HI_UNF_ADVCA_KEYLADDER_SP_ATTR_ALG;
        stKeyladderAttr.unKlAttr.stSPKlAttr.enAlgType = HI_UNF_ADVCA_ALG_TYPE_TDES;
        Ret = HI_UNF_ADVCA_SetKeyLadderAttr(HI_UNF_ADVCA_KEYLADDER_SP, &stKeyladderAttr);

        enOtpFuse = HI_UNF_ADVCA_OTP_SP_KL_LEVEL_SEL;
        memset(&stOtpFuseAttr, 0, sizeof(stOtpFuseAttr));
        Ret = HI_UNF_ADVCA_GetOtpFuse(enOtpFuse, &stOtpFuseAttr);
        if(Ret != HI_SUCCESS)
        {
            HI_DEBUG_ADVCA("call HI_UNF_ADVCA_GetOtpFuse get SP Key-ladder level failed\n");
            return HI_FAILURE;
        }
        printf("SP key-level stage is:0x%x\n", stOtpFuseAttr.unOtpFuseAttr.stKeyladderLevSel.enKeyladderLevel);
        if(stOtpFuseAttr.unOtpFuseAttr.stKeyladderLevSel.enKeyladderLevel >= HI_UNF_ADVCA_KEYLADDER_LEV2)
        {
            printf("Level 1 is setting\n");
            memset(&stKeyladderAttr, 0, sizeof(HI_UNF_ADVCA_KEYLADDER_ATTR_S));
            stKeyladderAttr.unKlAttr.stSPKlAttr.enSPKlAttr = HI_UNF_ADVCA_KEYLADDER_SP_ATTR_SESSION_KEY;
    		stKeyladderAttr.unKlAttr.stSPKlAttr.enStage = HI_UNF_ADVCA_KEYLADDER_LEV1;
            memcpy(stKeyladderAttr.unKlAttr.stSPKlAttr.u8SessionKey, g_u8EncrptedSessionKey, 16);
            Ret |= HI_UNF_ADVCA_SetKeyLadderAttr(HI_UNF_ADVCA_KEYLADDER_SP, &stKeyladderAttr);
        }

        memset(&stKeyladderAttr, 0, sizeof(HI_UNF_ADVCA_KEYLADDER_ATTR_S));
        stKeyladderAttr.unKlAttr.stSPKlAttr.enSPKlAttr = HI_UNF_ADVCA_KEYLADDER_SP_ATTR_DSC_MODE;
        stKeyladderAttr.unKlAttr.stSPKlAttr.enDscMode = HI_UNF_ADVCA_SP_DSC_MODE_PAYLOAD_CSA3;
        Ret |= HI_UNF_ADVCA_SetKeyLadderAttr(HI_UNF_ADVCA_KEYLADDER_SP, &stKeyladderAttr);

        Ret |= HI_UNF_DMX_SetDescramblerOddKey(hKey, g_u8_CSA3_EncrytedOddKey);
        Ret |= HI_UNF_DMX_SetDescramblerEvenKey(hKey, g_u8_CSA3_EncrytedEvenKey);
        if (HI_SUCCESS != Ret)
        {
            HI_DEBUG_ADVCA("call HI_UNF_DMX_SetDescramblerEvenKey failed\n");
            return HI_FAILURE;
        }
		//After use the SP keyladder, deactive the SP keyladder
        stKeyladderAttr.unKlAttr.stSPKlAttr.enSPKlAttr = HI_UNF_ADVCA_KEYLADDER_SP_ATTR_ENABLE;
        stKeyladderAttr.unKlAttr.stSPKlAttr.bEnable = HI_FALSE;
        Ret = HI_UNF_ADVCA_SetKeyLadderAttr(HI_UNF_ADVCA_KEYLADDER_SP, &stKeyladderAttr);      
    }

    return HI_SUCCESS;
}


HI_S32 AVPLAY_SetCW(HI_HANDLE hKey, HI_U32 ProgNum)
{
    if (0 == g_KeyLadderType)
    {
        loadspcw(hKey, g_CwType, ProgNum);
        printf("=====loadspcw\n");
    }
    else if (1 == g_KeyLadderType)
    {
        loadcsa3cw(hKey, g_CwType, ProgNum);
        printf("=====loadcsa3cw\n");
    }

    return HI_SUCCESS;
}

HI_S32 AVPLAY_AttachDescramble(HI_HANDLE hAvplay,HI_HANDLE *phKey)
{
    HI_S32 Ret;
    HI_UNF_DMX_DESCRAMBLER_ATTR_S stDesramblerAttr;
    HI_HANDLE hKey;
    HI_HANDLE hDmxVidChn,hDmxAudChn;

	memset(&stDesramblerAttr,0,sizeof(HI_UNF_DMX_DESCRAMBLER_ATTR_S));
    if (0 == g_CwType)
    {
        stDesramblerAttr.enCaType = HI_UNF_DMX_CA_NORMAL;
    }
    else
    {
        stDesramblerAttr.enCaType = HI_UNF_DMX_CA_ADVANCE;
    }

    if (0 == g_KeyLadderType)
    {
        stDesramblerAttr.enDescramblerType = HI_UNF_DMX_DESCRAMBLER_TYPE_CSA3; // For SPKeyLadder
    }
    else if (1 == g_KeyLadderType)
    {
        stDesramblerAttr.enDescramblerType = HI_UNF_DMX_DESCRAMBLER_TYPE_CSA3; // For CSA3Keyladder
    }
    Ret = HI_UNF_DMX_CreateDescramblerExt(0, &stDesramblerAttr, &hKey);
    if (HI_SUCCESS != Ret)
    {
        HI_DEBUG_ADVCA("call HI_UNF_DMX_CreateDescramblerExt failed %#x\n",Ret);
        return HI_FAILURE;
    }

    Ret = HI_UNF_AVPLAY_GetDmxVidChnHandle(hAvplay,&hDmxVidChn);
    Ret |= HI_UNF_AVPLAY_GetDmxAudChnHandle(hAvplay,&hDmxAudChn);
    Ret = HI_UNF_DMX_AttachDescrambler(hKey,hDmxVidChn);
    if (HI_SUCCESS != Ret)
    {
        HI_DEBUG_ADVCA("call HI_UNF_DMX_AttachDescrambler hDmxVidChn failed\n");        
        (HI_VOID)HI_UNF_DMX_DestroyDescrambler(hKey);
        return HI_FAILURE;
    }

    Ret = HI_UNF_DMX_AttachDescrambler(hKey,hDmxAudChn);
    if (HI_SUCCESS != Ret)
    {
        HI_DEBUG_ADVCA("call HI_UNF_DMX_AttachDescrambler hDmxAudChn failed\n");                
        (HI_VOID)HI_UNF_DMX_DetachDescrambler(hKey,hDmxVidChn);
        (HI_VOID)HI_UNF_DMX_DestroyDescrambler(hKey);
        return HI_FAILURE;
    }
    *phKey = hKey;

    return HI_SUCCESS;
}

HI_S32 AVPLAY_DetachDescramble(HI_HANDLE hAvplay,HI_HANDLE hKey)
{
    HI_S32 Ret;
    HI_HANDLE hDmxVidChn,hDmxAudChn;

    Ret = HI_UNF_AVPLAY_GetDmxVidChnHandle(hAvplay,&hDmxVidChn);
    Ret |= HI_UNF_AVPLAY_GetDmxAudChnHandle(hAvplay,&hDmxAudChn);
    Ret |= HI_UNF_DMX_DetachDescrambler(hKey,hDmxVidChn);
    Ret |= HI_UNF_DMX_DetachDescrambler(hKey,hDmxAudChn);
    Ret |= HI_UNF_DMX_DestroyDescrambler(hKey);
    
    return Ret;
}

HI_VOID *TsTthread(HI_VOID *args)
{
    HI_UNF_STREAM_BUF_S   StreamBuf;
    HI_U32            Readlen;
    HI_S32            Ret;

    while (!g_bStopTsThread)
    {
        pthread_mutex_lock(&g_TsMutex);
        Ret = HI_UNF_DMX_GetTSBuffer(g_TsBuf, 188*50, &StreamBuf, 1000);
        if (Ret != HI_SUCCESS )
        {
            pthread_mutex_unlock(&g_TsMutex);
            continue;
        }

        Readlen = fread(StreamBuf.pu8Data, sizeof(HI_S8), 188*50, g_pTsFile);
        if(Readlen <= 0)
        {
            HI_DEBUG_ADVCA("read ts file end and rewind!\n");
            rewind(g_pTsFile);
            pthread_mutex_unlock(&g_TsMutex);
            continue;
        }

        Ret = HI_UNF_DMX_PutTSBuffer(g_TsBuf, Readlen);
        if (Ret != HI_SUCCESS )
        {
            HI_DEBUG_ADVCA("call HI_UNF_DMX_PutTSBuffer failed.\n");
        }
        pthread_mutex_unlock(&g_TsMutex);
    }

    Ret = HI_UNF_DMX_ResetTSBuffer(g_TsBuf);
    if (Ret != HI_SUCCESS )
    {
        HI_DEBUG_ADVCA("call HI_UNF_DMX_ResetTSBuffer failed.\n");
    }

    return HI_NULL;
}


HI_S32 main(HI_S32 argc,HI_CHAR *argv[])
{
    HI_S32                  Ret;
    HI_HANDLE               hWin;
    HI_UNF_AVPLAY_OPEN_OPT_S maxCapbility;
    HI_HANDLE               hAvplay;
    HI_HANDLE               hKey;
    HI_UNF_AVPLAY_ATTR_S        AvplayAttr;
    HI_UNF_SYNC_ATTR_S          SyncAttr;
    HI_UNF_AVPLAY_STOP_OPT_S    Stop;
    HI_CHAR                 InputCmd[32];
    HI_UNF_ENC_FMT_E   enFormat = HI_UNF_ENC_FMT_1080i_50;
    HI_U32             ProgNum;

    HI_HANDLE                   hTrack;
    HI_UNF_AUDIOTRACK_ATTR_S    stTrackAttr;
#ifdef __HI3716MV310__	
    HI_HANDLE sthMixer;
    HI_UNF_MIXER_ATTR_S stMixerAttr;
#endif
    if(argc < 3)
    {
        printf("Usage: sample_tsplay file CwType KeyLadderType\n"
               "CwType: 0-ClearCw  1-TDES\n"
               "KeyLadderType:   sp csa3");
        return 0;
    }

    g_CwType = (0 == atoi(argv[2]))?(0):(1);
    enFormat = HI_UNF_ENC_FMT_1080i_50;
    maxCapbility.enCapLevel = HI_UNF_VCODEC_CAP_LEVEL_FULLHD;
    maxCapbility.enDecType = HI_UNF_VCODEC_DEC_TYPE_NORMAL;
    maxCapbility.enProtocolLevel = HI_UNF_VCODEC_PRTCL_LEVEL_H264;
    if (!strncmp(argv[3], "sp", 2))
    {
        g_KeyLadderType = 0;
    }
    else if (!strncmp(argv[3], "csa3", 4))
    {
        g_KeyLadderType = 1;
    }
    else
    {
        g_KeyLadderType = 0;
    }
    
    g_pTsFile = fopen(argv[1], "rb");
    if (!g_pTsFile)
	{
        HI_DEBUG_ADVCA("open file %s error!\n", argv[1]);
		return -1;
	}

    (HI_VOID)HI_SYS_Init();
#ifndef __HI3716MV310__
    HIADP_MCE_Exit();
#endif
    Ret = HI_UNF_ADVCA_Init();
    if (HI_SUCCESS != Ret)
    {
        HI_DEBUG_ADVCA("call HI_UNF_ADVCA_Init failed\n");
        goto SYS_DEINIT;
    }

    Ret = HIADP_Snd_Init();
    if (HI_SUCCESS != Ret)
    {
        HI_DEBUG_ADVCA("call SndInit failed.\n");
        goto CA_DEINIT;
    }

    Ret = HIADP_Disp_Init(enFormat);
    if (HI_SUCCESS != Ret)
    {
        HI_DEBUG_ADVCA("call HIADP_Disp_Init failed.\n");
        goto SND_DEINIT;
    }

    Ret = HIADP_VO_Init(HI_UNF_VO_DEV_MODE_NORMAL);
    Ret |= HIADP_VO_CreatWin(HI_NULL,&hWin);
    if (HI_SUCCESS != Ret)
    {
        HI_DEBUG_ADVCA("call VoInit failed.\n");
        (HI_VOID)HIADP_VO_DeInit();
        goto DISP_DEINIT;
    }

    Ret = HI_UNF_DMX_Init();
#ifdef __HI3716MV310__
    Ret |= HI_UNF_DMX_AttachTSPort(PLAY_DMX_ID, HI_UNF_DMX_PORT_3_RAM);
#else	
    Ret = HI_UNF_DMX_AttachTSPort(PLAY_DMX_ID,HI_UNF_DMX_PORT_RAM_0);
#endif	
    if (HI_SUCCESS != Ret)
    {
        HI_DEBUG_ADVCA("call VoInit failed.\n");
        goto VO_DEINIT;
    }
#ifdef __HI3716MV310__
    Ret = HI_UNF_DMX_CreateTSBuffer(HI_UNF_DMX_PORT_3_RAM, 0x200000, &g_TsBuf);
#else
    Ret = HI_UNF_DMX_CreateTSBuffer(HI_UNF_DMX_PORT_RAM_0, 0x200000, &g_TsBuf);
#endif
    if (Ret != HI_SUCCESS)
    {
        HI_DEBUG_ADVCA("call HI_UNF_DMX_CreateTSBuffer failed.\n");
        goto DMX_DEINIT;
    }

    Ret = HIADP_AVPlay_RegADecLib();
    if (HI_SUCCESS != Ret)
    {
        HI_DEBUG_ADVCA("call RegADecLib failed.\n");
        goto TSBUF_FREE;
    }

    Ret = HI_UNF_AVPLAY_Init();
    if (Ret != HI_SUCCESS)
    {
        HI_DEBUG_ADVCA("call HI_UNF_AVPLAY_Init failed.\n");
        goto DMX_DEINIT;
    }

    Ret = HI_UNF_AVPLAY_GetDefaultConfig(&AvplayAttr, HI_UNF_AVPLAY_STREAM_TYPE_TS);
    Ret |= HI_UNF_AVPLAY_Create(&AvplayAttr, &hAvplay);
    if (Ret != HI_SUCCESS)
    {
        HI_DEBUG_ADVCA("call HI_UNF_AVPLAY_Create failed.\n");
        goto AVPLAY_DEINIT;
    }

    Ret = HI_UNF_AVPLAY_ChnOpen(hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_VID, &maxCapbility);
    if (Ret != HI_SUCCESS)
    {
        HI_DEBUG_ADVCA("call HI_UNF_AVPLAY_ChnOpen failed.\n");
        goto AVPLAY_DESTROY;
    }

    Ret = HI_UNF_AVPLAY_ChnOpen(hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_AUD, HI_NULL);
    if (Ret != HI_SUCCESS)
    {
        HI_DEBUG_ADVCA("call HI_UNF_AVPLAY_ChnOpen failed.\n");
        goto VCHN_CLOSE;
    }

    Ret = AVPLAY_AttachDescramble(hAvplay,&hKey);
    if (HI_SUCCESS != Ret)
    {
        HI_DEBUG_ADVCA("call AVPLAY_AttachDescramble failed.\n");
        goto ACHN_CLOSE;
    }

    Ret = HI_UNF_VO_AttachWindow(hWin, hAvplay);
    Ret |= HI_UNF_VO_SetWindowEnable(hWin, HI_TRUE);
    if (Ret != HI_SUCCESS)
    {
        HI_DEBUG_ADVCA("call HI_UNF_VO_SetWindowEnable failed.\n");
        goto WIN_DETACH;
    }

#ifndef __HI3716MV310__
    Ret = HI_UNF_SND_GetDefaultTrackAttr(HI_UNF_SND_TRACK_TYPE_MASTER, &stTrackAttr);
    if (Ret != HI_SUCCESS)
    {
        printf("call HI_UNF_SND_GetDefaultTrackAttr failed.\n");
        goto WIN_DETACH;
    }
    //Ret = HI_UNF_SND_Mixer_Open(HI_UNF_SND_0, &hTrack, &stMixerAttr);
    Ret = HI_UNF_SND_CreateTrack(HI_UNF_SND_0, &stTrackAttr, &hTrack);	
    if (Ret != HI_SUCCESS)
    {
        printf("call HI_UNF_SND_CreateTrack failed.\n");
        goto WIN_DETACH;
    }
#endif	

#ifdef 	__HI3716MV310__
    Ret = HI_UNF_SND_Attach(HI_UNF_SND_0, hAvplay, HI_UNF_SND_MIX_TYPE_MASTER, 100);
#else
    Ret = HI_UNF_SND_Attach(hTrack, hAvplay);
#endif
    if (Ret != HI_SUCCESS)
    {
        HI_DEBUG_ADVCA("call HI_SND_Attach failed.\n");
        goto TRACK_DESTROY;
    }
    
    Ret = HI_UNF_AVPLAY_GetAttr(hAvplay, HI_UNF_AVPLAY_ATTR_ID_SYNC, &SyncAttr);
    SyncAttr.enSyncRef = HI_UNF_SYNC_REF_NONE;
    Ret |= HI_UNF_AVPLAY_SetAttr(hAvplay, HI_UNF_AVPLAY_ATTR_ID_SYNC, &SyncAttr);
    if (Ret != HI_SUCCESS)
    {
        HI_DEBUG_ADVCA("call HI_UNF_AVPLAY_SetAttr failed.\n");
        goto SND_DETACH;
    }

    (HI_VOID)pthread_mutex_init(&g_TsMutex,NULL);
    g_bStopTsThread = HI_FALSE;
    (HI_VOID)pthread_create(&g_TsThd, HI_NULL, TsTthread, HI_NULL);

    HIADP_Search_Init();
    Ret = HIADP_Search_GetAllPmt(0, &g_pProgTbl);
    if (Ret != HI_SUCCESS)
    {
        HI_DEBUG_ADVCA("call HIADP_Search_GetAllPmt failed.\n");
        goto PSISI_FREE;
    }

#if 0
    HI_DEBUG_ADVCA("select channel number to start play\n");
    memset(InputCmd, 0, 30);
    fgets(InputCmd, 30, stdin);
    ProgNum = atoi(InputCmd);
#endif
    ProgNum = 0;

    pthread_mutex_lock(&g_TsMutex);
    rewind(g_pTsFile);
    (HI_VOID)HI_UNF_DMX_ResetTSBuffer(g_TsBuf);
    pthread_mutex_unlock(&g_TsMutex);
    Ret = AVPLAY_SetCW(hKey, ProgNum);
    Ret |= HIADP_AVPlay_PlayProg(hAvplay,g_pProgTbl,ProgNum,HI_TRUE);
    if (Ret != HI_SUCCESS)
    {
        HI_DEBUG_ADVCA("call SwitchProg failed.\n");
        goto AVPLAY_STOP;
    }

    while(1)
    {
        printf("please input 'q' to quit!\n");
        memset(InputCmd, 0, 30);
        (HI_VOID)fgets(InputCmd, 30, stdin);
        if ('q' == InputCmd[0])
        {
            HI_DEBUG_ADVCA("prepare to quit!\n");
            break;
        }

        ProgNum = atoi(InputCmd);
        if (ProgNum == 0)
            ProgNum = 1;

        pthread_mutex_lock(&g_TsMutex);
        rewind(g_pTsFile);
        (HI_VOID)HI_UNF_DMX_ResetTSBuffer(g_TsBuf);
        pthread_mutex_unlock(&g_TsMutex);
        Ret = AVPLAY_SetCW(hKey, ProgNum);
        Ret |= HIADP_AVPlay_PlayProg(hAvplay,g_pProgTbl,ProgNum-1,HI_TRUE);
        if (Ret != HI_SUCCESS)
        {
            HI_DEBUG_ADVCA("call SwitchProgfailed!\n");
            break;
        }
    }

AVPLAY_STOP:
    Stop.enMode = HI_UNF_AVPLAY_STOP_MODE_BLACK;
    Stop.u32TimeoutMs = 0;
    (HI_VOID)HI_UNF_AVPLAY_Stop(hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_VID | HI_UNF_AVPLAY_MEDIA_CHAN_AUD, &Stop);

PSISI_FREE:
    HIADP_Search_DeInit();

    g_bStopTsThread = HI_TRUE;
    (HI_VOID)pthread_join(g_TsThd, HI_NULL);  
    (HI_VOID)pthread_mutex_destroy(&g_TsMutex);

SND_DETACH:
    (HI_VOID)HI_UNF_SND_Detach(hTrack, hAvplay);

TRACK_DESTROY:
#ifdef __HI3716MV310__	
    HI_UNF_SND_Mixer_Close(hTrack);
#else
    HI_UNF_SND_DestroyTrack(hTrack);
#endif	
    
WIN_DETACH:
    (HI_VOID)HI_UNF_VO_SetWindowEnable(hWin,HI_FALSE);
    (HI_VOID)HI_UNF_VO_DetachWindow(hWin, hAvplay);

    (HI_VOID)AVPLAY_DetachDescramble(hAvplay,hKey);

ACHN_CLOSE:
    (HI_VOID)HI_UNF_AVPLAY_ChnClose(hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_AUD);

VCHN_CLOSE:
    (HI_VOID)HI_UNF_AVPLAY_ChnClose(hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_VID);

AVPLAY_DESTROY:
    (HI_VOID)HI_UNF_AVPLAY_Destroy(hAvplay);
    
AVPLAY_DEINIT:
    (HI_VOID)HI_UNF_AVPLAY_DeInit();

TSBUF_FREE:
    (HI_VOID)HI_UNF_DMX_DestroyTSBuffer(g_TsBuf);
    
DMX_DEINIT:
    (HI_VOID)HI_UNF_DMX_DetachTSPort(0);
    (HI_VOID)HI_UNF_DMX_DeInit();

VO_DEINIT:
    (HI_VOID)HI_UNF_VO_DestroyWindow(hWin);
    (HI_VOID)HIADP_VO_DeInit();

DISP_DEINIT:
	(HI_VOID)HIADP_Disp_DeInit();

SND_DEINIT:
    (HI_VOID)HIADP_Snd_DeInit();

CA_DEINIT:
    (HI_VOID)HI_UNF_ADVCA_DeInit();

SYS_DEINIT:
    (HI_VOID)HI_SYS_DeInit();
    (HI_VOID)fclose(g_pTsFile);
    g_pTsFile = HI_NULL;

    return Ret;
}


