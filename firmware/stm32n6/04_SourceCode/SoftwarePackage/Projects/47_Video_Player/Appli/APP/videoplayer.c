/**
 ****************************************************************************************************
 * @file        videoplay.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-03-18
 * @brief       视频播放器 应用代码
 * @license     Copyright (c) 2023-2035, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 N647开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:zhengdianyuanzi.tmall.com
 *
 ****************************************************************************************************
 */
 
#include "videoplayer.h"
#include "ff.h"
#include "malloc.h"
#include "./UART/uart.h"
#include "./ES8388/es8388.h"
#include "./ES8388/es8388_sai.h"
#include "./LED/led.h"
#include "./RGBLCD/rgblcd.h"
#include "./KEY/key.h"
#include "./exfuns/exfuns.h"
#include "text.h"
#include "ffconf.h"
#include "./exfuns/fattester.h"
#include "mjpeg.h"
#include "avi.h"
#include "./BTIM/btim.h"
#include "string.h"


extern uint16_t g_avi_frame;                /* 播放帧率 */
extern volatile uint8_t g_avi_frameup;      /* 视频播放时隙控制变量,当等于1的时候,可以更新下一帧视频 */
extern TIM_HandleTypeDef htim7; /* TIM句柄 */
extern SAI_HandleTypeDef hsai_BlockA1;

static void es8388_sai_tx_dma_cplt_user_cb(void);

volatile uint8_t g_avi_sai_playbuf = 0;         /* 即将播放的音频帧缓冲编号 */
uint8_t *p_avi_sai_buf[AVI_AUDIO_BUF_NUM];  /* 音频缓冲帧 */
uint8_t audio_frame_buffer_reader = 0;    /* 即将播放的音频帧缓冲编号 */

static void es8388_sai_tx_dma_cplt_user_cb(void)
{
    if (++g_avi_sai_playbuf == AVI_AUDIO_BUF_NUM)
    {
    	g_avi_sai_playbuf = 0;
    }
    es8388_sai_tx_dma((uint8_t *)&p_avi_sai_buf[g_avi_sai_playbuf][0], g_avix.AudioBufSize / 2);

}

/**
 * @brief       音频数据SAI DMA传输回调函数
 * @param       node：节点
 * @retval      无
 */
void audio_sai_dma_callback(uint8_t node)
{
    g_avi_sai_playbuf = node;
}

/**
 * @brief       得到path路径下,目标文件的总个数
 * @param       path    : 路径
 * @retval      总有效文件数
 */
uint16_t video_get_tnum(char *path)
{
    uint8_t res;
    uint16_t rval = 0;
    DIR tdir;                           /* 临时目录 */
    FILINFO *tfileinfo;                 /* 临时文件信息 */
    
    tfileinfo = (FILINFO *)mymalloc(SRAMIN, sizeof(FILINFO));   /* 申请内存 */
    res = f_opendir(&tdir, (const TCHAR *)path);                /* 打开目录 */

    if (res == FR_OK && tfileinfo)
    {
        while (1)                       /* 查询总的有效文件数 */
        {
            res = f_readdir(&tdir, tfileinfo);                  /* 读取目录下的一个文件 */

            if (res != FR_OK || tfileinfo->fname[0] == 0)
            {
                break;                  /* 错误了/到末尾了,退出 */
            }

            res = exfuns_file_type(tfileinfo->fname);

            if ((res & 0XF0) == 0X60)   /* 取高四位,看看是不是视频文件 */
            {
                rval++;                 /* 有效文件数增加1 */
            }
        }
    }

    myfree(SRAMIN, tfileinfo);          /* 释放内存 */
    return rval;
}

/**
 * @brief       显示当前播放时间
 * @param       favi:当前播放的视频文件
 * @param       aviinfo : AVI控制结构体
 * @retval      无
 */
void video_time_show(FIL *favi, AVI_INFO *aviinfo)
{
    static uint32_t oldsec;                                         /* 上一次的播放时间 */
    uint8_t *buf;
    uint32_t totsec = 0;                                            /* video文件总时间 */
    uint32_t cursec;                                                /* 当前播放时间 */

    totsec = (aviinfo->SecPerFrame / 1000) * aviinfo->TotalFrame;   /* 歌曲总长度(单位:ms) */
    totsec /= 1000;                                                 /* 秒钟数 */
    cursec = ((double)favi->fptr / favi->obj.objsize) * totsec;     /* 当前播放到多少秒了? */

    if (oldsec != cursec)             /* 需要更新显示时间 */
    {
        buf = mymalloc(SRAMIN, 100);  /* 申请100字节内存 */
        oldsec = cursec;

        sprintf((char *)buf, "播放时间:%02ld:%02ld:%02ld/%02ld:%02ld:%02ld", cursec / 3600, (cursec % 3600) / 60, cursec % 60, totsec / 3600, (totsec % 3600) / 60, totsec % 60);
        text_show_string(10, 90, rgblcddev.width - 10, 16, (char *)buf, 16, 0, 0, BLUE);   /* 显示播放时间 */

        myfree(SRAMIN, buf);
    }
}

/**
 * @brief       显示当前视频文件的相关信息
 * @param       aviinfo : AVI控制结构体
 * @retval      无
 */
void video_info_show(AVI_INFO *aviinfo)
{
    uint8_t *buf;

    buf = mymalloc(SRAMIN, 100);    /* 申请100字节内存 */

    sprintf((char *)buf, "声道数:%d,采样率:%ld", aviinfo->Channels, aviinfo->SampleRate * 10);
    text_show_string(10, 50, rgblcddev.width - 10, 16, (char *)buf, 16, 0, 0, RED);    /* 显示歌曲名字 */

    sprintf((char *)buf, "帧率:%ld帧", 1000 / (aviinfo->SecPerFrame / 1000));
    text_show_string(10, 70, rgblcddev.width - 10, 16, (char *)buf, 16, 0, 0, RED);    /* 显示歌曲名字 */

    myfree(SRAMIN, buf);
}

/**
 * @brief       显示视频基本信息显示
 * @param       name    : 视频名字
 * @param       index   : 当前索引
 * @param       total   : 总文件数
 * @retval      无
 */
void video_bmsg_show(uint8_t *name, uint16_t index, uint16_t total)
{
    uint8_t *buf;
    buf = mymalloc(SRAMIN, 100);            /* 申请100字节内存 */

    text_show_string(10, 10, 64, 16, "文件名:", 16, 0, 0, RED);   /* 显示文件名 */
    text_show_string(74, 10, rgblcddev.width - 74, 16, (char *)name, 16, 0, 1, RED);   /* 显示文件名 */

    sprintf((char *)buf, "索引:%d/%d", index, total);
    text_show_string(10, 30, rgblcddev.width - 10, 16, (char *)buf, 16, 0, 0, RED);   /* 显示索引 */

    myfree(SRAMIN, buf);
}

/**
 * @brief       播放视频
 * @param       无
 * @retval      无
 */
void video_play(void)
{
    uint8_t res;
    DIR vdir;                   /* 目录 */
    FILINFO *vfileinfo;         /* 文件信息 */
    uint8_t *pname;             /* 带路径的文件名 */
    uint16_t totavinum;         /* 视频文件总数 */
    uint16_t curindex;          /* 视频文件当前索引 */
    uint8_t key;                /* 键值 */
    uint32_t temp;
    uint32_t *voffsettbl;       /* 视频文件off block索引表 */

    while (f_opendir(&vdir, "0:/VIDEO"))    /* 打开视频文件夹 */
    {
        text_show_string(60, 190, 240, 16, "VIDEO文件夹错误!", 16, 0, 0, RED);
        HAL_Delay(200);
        rgblcd_fill(60, 190, 240, 206, WHITE); /* 清除显示 */
        HAL_Delay(200);
    }

    totavinum = video_get_tnum("0:/VIDEO"); /* 得到总有效文件数 */

    while (totavinum == 0)               /* 视频文件总数为0 */
    {
        text_show_string(60, 190, 240, 16, "没有视频文件!", 16, 0, 0,RED);
        HAL_Delay(200);
        rgblcd_fill(60, 190, 240, 146, WHITE); /* 清除显示 */
        HAL_Delay(200);
    }

    vfileinfo = (FILINFO *)mymalloc(SRAMIN, sizeof(FILINFO));           /* 为长文件缓存区分配内存 */
    pname = mymalloc(SRAMIN, 2 * FF_MAX_LFN + 1);                       /* 为带路径的文件名分配内存 */
    voffsettbl = mymalloc(SRAMIN, 4 * totavinum);                       /* 申请4*totavinum个字节的内存,用于存放视频文件索引 */

    while (vfileinfo == NULL || pname == NULL || voffsettbl == NULL)    /* 内存分配出错 */
    {
        text_show_string(60, 190, 240, 16, "内存分配失败!", 16, 0, 0, RED);
        HAL_Delay(200);
        rgblcd_fill(60, 190, 240, 146, WHITE); /* 清除显示 */
        HAL_Delay(200);
    }

    /* 记录索引 */
    res = f_opendir(&vdir, "0:/VIDEO");     /* 打开目录 */

    if (res == FR_OK)
    {
        curindex = 0;   /* 当前索引为0 */

        while (1)       /* 全部查询一遍 */
        {
            temp = vdir.dptr;                                   /* 记录当前offset */
            res = f_readdir(&vdir, vfileinfo);                  /* 读取目录下的一个文件 */

            if (res != FR_OK || vfileinfo->fname[0] == 0)
            {
                break; /* 错误了/到末尾了,退出 */
            }

            res = exfuns_file_type(vfileinfo->fname);

            if ((res & 0XF0) == 0X60)         /* 取高四位,看看是不是音乐文件 */
            {
                voffsettbl[curindex] = temp;  /* 记录索引 */
                curindex++;
            }
        }
    }

    curindex = 0;                                           /* 从0开始显示 */
    res = f_opendir(&vdir, (const TCHAR *)"0:/VIDEO");      /* 打开目录 */

    while (res == FR_OK)    /* 打开成功 */
    {
        dir_sdi(&vdir, voffsettbl[curindex]);               /* 改变当前目录索引 */
        res = f_readdir(&vdir, vfileinfo);                  /* 读取目录下的一个文件 */

        if (res != FR_OK || vfileinfo->fname[0] == 0)
        {
            break;          /* 错误了/到末尾了,退出 */
        }

        strcpy((char *)pname, "0:/VIDEO/");                     /* 复制路径(目录) */
        strcat((char *)pname, (const char *)vfileinfo->fname);  /* 将文件名接在后面 */
        
        rgblcd_clear(WHITE);                                       /* 先清屏 */
        video_bmsg_show((uint8_t *)vfileinfo->fname, curindex + 1, totavinum); /* 显示名字,索引等信息 */
        
        key = video_play_mjpeg(pname);                          /* 播放这个音频文件 */

        if (key == KEY2_PRES)                                   /* 上一个视频 */
        {
            if (curindex)
            {
                curindex--;
            }
            else 
            {
                curindex = totavinum - 1;
            }
        }
        else if (key == KEY0_PRES)  /* 下一个视频 */
        {
            curindex++;

            if (curindex >= totavinum)
            {
                curindex = 0;       /* 到末尾的时候,自动从头开始 */
            }
        }
        else
        {
            break;                  /* 产生了错误 */
        }
    }

    myfree(SRAMIN, vfileinfo);          /* 释放内存 */
    myfree(SRAMIN, pname);              /* 释放内存 */
    myfree(SRAMIN, voffsettbl);         /* 释放内存 */
}

/**
 * @brief       播放一个MJPEG文件
 * @param       pname   : 文件名
 * @retval      执行结果
 *   @arg       KEY0_PRES , 下一个视频
 *   @arg       KEY1_PRES , 上一个视频
 *   @arg       其他      , 错误
 */
uint8_t video_play_mjpeg(uint8_t *pname)
{
    uint8_t *framebuf;      /* 视频解码buf */
    uint8_t *pbuf;          /* buf指针 */
    FIL *favi;
    uint8_t  res = 0;
    uint8_t i = 0;
    uint16_t offset = 0;
    UINT nr;
    uint8_t key;
    uint8_t saisavebuf;   /* 即将缓存的视频 */

    for (i = 0; i < AVI_AUDIO_BUF_NUM; i++)
    {
        p_avi_sai_buf[i] = mymalloc(SRAMIN, AVI_AUDIO_BUF_SIZE);   /* 申请音频内存 */

        if (p_avi_sai_buf[i] == NULL)                              /* 申请失败, 直接退出 */
        {
            break;
        }

        memset(p_avi_sai_buf[i], 0, AVI_AUDIO_BUF_SIZE);            /* 数据清零 */
    }

    favi = (FIL *)mymalloc(SRAMIN, sizeof(FIL));                    /* 申请favi内存 */
    framebuf = mymalloc(SRAMIN, AVI_VIDEO_BUF_SIZE) ;               /* 申请视频buf */

    if (!framebuf)  /* 只要最后这个视频buf申请失败, 前面的申请失不失败都不重要, 总之就是失败了 */
    {
        printf("memory error!\r\n");
        res = 0XFF;
    }

    while (res == 0)
    {
        res = f_open(favi, (char *)pname, FA_READ);

        if (res == 0)
        {
            pbuf = framebuf;
            res = f_read(favi, pbuf, AVI_VIDEO_BUF_SIZE, &nr);  /* 开始读取 */
            if (res)
            {
                printf("fread error:%d\r\n", res);
                break;
            }

            /* 开始avi解析 */
            res = avi_init(pbuf, AVI_VIDEO_BUF_SIZE);           /* avi解析 */


            if (res || g_avix.Width > rgblcddev.width)
            {
                printf("avi err:%d,width:%ld\r\n", res, g_avix.Width);
                res = KEY0_PRES;
                break;
            }

            video_info_show(&g_avix);

            __HAL_TIM_SET_AUTORELOAD(&htim7, g_avix.SecPerFrame / 100 - 1);   /* 设置计数值 */
            TIM7->CR1 |= 1 << 0;										      /* 开启定时器 */
            HAL_TIM_Base_Start_IT(&htim7);

            offset = avi_srarch_id(pbuf, AVI_VIDEO_BUF_SIZE, "movi");       /* 寻找movi ID */
            avi_get_streaminfo(pbuf + offset + 4);                          /* 获取流信息 */
            f_lseek(favi, offset + 12);                                     /* 跳过标志ID,读地址偏移到流数据开始处 */

            if (rgblcddev.height <= g_avix.Height)
            {
                res = mjpeg_init((rgblcddev.width - g_avix.Width) / 2, (rgblcddev.height - g_avix.Height) / 2, g_avix.Width, g_avix.Height);              /* JPG解码初始化 */
            }
            else
            {
                res = mjpeg_init((rgblcddev.width - g_avix.Width) / 2, 110 + (rgblcddev.height - 110 - g_avix.Height) / 2, g_avix.Width, g_avix.Height);  /* JPG解码初始化 */
            }

            if (g_avix.SampleRate)                              /* 有音频信息,才初始化 */
            {
                es8388_sai_cfg(0, 3);                           /* 飞利浦标准,16位数据长度 */
                es8388_sai_clock_config(g_avix.SampleRate);
                es8388_sai_init_tx(2, g_avix.SampleRate, 16);
                es8388_sai_tx_dma_cplt_user_cb_register(es8388_sai_tx_dma_cplt_user_cb); /* DMA传输完成回调函数 */

                /* 开启SAI播放 */
                es8388_sai_tx_dma((uint8_t *)&p_avi_sai_buf[0][0], g_avix.AudioBufSize / 2);
                g_avi_sai_playbuf = 0;
                saisavebuf = 0;
            }

            while (1)               /* 播放循环 */
            {
                if (g_avix.StreamID == AVI_VIDS_FLAG)           /* 视频流 */
                {
                    pbuf = framebuf;
                    f_read(favi, pbuf, g_avix.StreamSize + 8, &nr);   /* 读入整帧+下一数据流ID信息 */
                    SCB_CleanInvalidateDCache();
                    res = mjpeg_decode(pbuf, g_avix.StreamSize);

                    if (res)
                    {
                        printf("decode error!\r\n");
                    }

                    while (g_avi_frameup == 0);                 /* 待时间到达(在TIM的中断里面设置为1) */

                    g_avi_frameup = 0;                          /* 标志清零 */
                    g_avi_frame++;
                }
                else                /* 音频流 */
                {
                    video_time_show(favi, &g_avix);             /* 显示当前播放时间 */

                    if (++saisavebuf == AVI_AUDIO_BUF_NUM)
                    {
                        saisavebuf = 0;
                    }

                    while (saisavebuf == g_avi_sai_playbuf);  /* 等待下次碰撞，读快于放 */

                    f_read(favi, p_avi_sai_buf[saisavebuf], g_avix.StreamSize + 8, &nr);    /* 填充p_avi_sai_buf */
                    SCB_CleanInvalidateDCache_by_Addr(p_avi_sai_buf[saisavebuf], g_avix.StreamSize + 8);                    /* 清空D catch */
                    pbuf = p_avi_sai_buf[saisavebuf];
                }

                key = key_scan(0);

                if (key == KEY0_PRES || key == KEY2_PRES)       /* KEY0/KEY2按下,播放下一个/上一个视频 */
                {
                    res = key;
                    break;
                }
                else if (key == KEY1_PRES || key == WKUP_PRES)
                {
                	hsai_BlockA1.Instance->CR1 &= ~SAI_xCR1_DMAEN;
                    video_seek(favi, &g_avix, framebuf);
                    pbuf = framebuf;
                    hsai_BlockA1.Instance->CR1 |= SAI_xCR1_DMAEN;
                }

                if (avi_get_streaminfo(pbuf + g_avix.StreamSize))     /* 读取下一帧 流标志 */
                {
                    printf("frame error \r\n");
                    res = KEY0_PRES;
                    break;
                }
            }

            es8388_sai_init_tx(2, g_avix.SampleRate, 16);       /* 重新初始化，关闭SAI中断 */
            TIM7->CR1 &= ~(1 << 0);                             /* 关闭定时器7 */
            mjpeg_free();                                       /* 释放内存 */
            f_close(favi);                                      /* 关闭文件 */
        }
    }

    /* 释放内存 */
    for (i = 0; i < AVI_AUDIO_BUF_NUM; i++)
    {
        myfree(SRAMIN, p_avi_sai_buf[i]);
    }

    myfree(SRAMIN, framebuf);
    myfree(SRAMIN, favi);

    return res;
}

/**
 * @brief       AVI文件查找
 * @param       favi    : AVI文件
 * @param       aviinfo : AVI信息结构体
 * @param       mbuf    : 数据缓冲区
 * @retval      执行结果
 *   @arg       0    , 成功
 *   @arg       其他 , 错误
 */
uint8_t video_seek(FIL *favi, AVI_INFO *aviinfo, uint8_t *mbuf)
{
    uint32_t fpos = favi->fptr;
    uint8_t *pbuf;
    uint16_t offset;
    UINT br;
    uint32_t delta;
    uint32_t totsec;
    uint8_t key;
    
    totsec = (aviinfo->SecPerFrame / 1000) * aviinfo->TotalFrame;
    totsec /= 1000;                           /* 秒钟数. */
    delta = (favi->obj.objsize / totsec) * 5; /* 每次前进5秒钟的数据量 */

    while (1)
    {
        key = key_scan(1);

        if (key == WKUP_PRES)           /* 快进 */
        {
            if (fpos < favi->obj.objsize)
            {
                fpos += delta;
            }

            if (fpos > (favi->obj.objsize - AVI_VIDEO_BUF_SIZE))
            {
                fpos = favi->obj.objsize - AVI_VIDEO_BUF_SIZE;
            }
        }
        else if (key == KEY1_PRES)      /* 快退 */
        {
            if (fpos > delta)
            {
                fpos -= delta;
            }
            else 
            {
                fpos = 0;
            }
        }
        else
        {
            break;
        }

        f_lseek(favi, fpos);
        f_read(favi, mbuf, AVI_VIDEO_BUF_SIZE, &br);    /* 读入整帧+下一数据流ID信息 */
        pbuf = mbuf;

        if (fpos == 0)                                  /* 从0开始,得先寻找movi ID */
        {
            offset = avi_srarch_id(pbuf, AVI_VIDEO_BUF_SIZE, "movi");
        }
        else 
        {
            offset = 0;
        }

        offset += avi_srarch_id(pbuf + offset, AVI_VIDEO_BUF_SIZE, g_avix.VideoFLAG);   /* 寻找视频帧 */
        avi_get_streaminfo(pbuf + offset);              /* 获取流信息 */
        f_lseek(favi, fpos + offset + 8);               /* 跳过标志ID,读地址偏移到流数据开始处 */

        if (g_avix.StreamID == AVI_VIDS_FLAG)
        {
            f_read(favi, mbuf, g_avix.StreamSize + 8, &br); /* 读入整帧 */
            SCB_CleanInvalidateDCache();
            mjpeg_decode(mbuf, g_avix.StreamSize);          /* 显示视频帧 */
        }
        else
        {
            printf("error flag");
        }

        video_time_show(favi, &g_avix);
    }

    return 0;
}






