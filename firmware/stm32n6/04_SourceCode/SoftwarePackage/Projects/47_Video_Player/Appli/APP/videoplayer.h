/**
 ****************************************************************************************************
 * @file        videoplay.h
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
 
#ifndef __VIDEOPLAYER_H
#define __VIDEOPLAYER_H

#include "main.h"
#include "avi.h"
#include "ff.h"


#define AVI_AUDIO_BUF_SIZE      1024 * 5            /* 定义avi解码时,音频buf大小. */
#define AVI_AUDIO_BUF_NUM       10                   /* 音频buf的数量 */

#define AVI_VIDEO_BUF_SIZE      1024 * 260          /* 260定义avi解码时,视频buf大小. */

/******************************************************************************************/

uint16_t video_get_tnum(char *path);                                 /* 得到path路径下，目标文件的总数 */
void video_time_show(FIL *favi, AVI_INFO *aviinfo);                  /* 显示当前播放时间 */
void video_info_show(AVI_INFO *aviinfo);                             /* 显示当前视频文件的相关信息 */
void video_bmsg_show(uint8_t *name, uint16_t index, uint16_t total); /* 视频基本信息显示 */
void video_play(void);                                               /* 播放视频 */
void audio_sai_dma_callback(uint8_t node); 						     /* 音频数据SAI DMA传输回调函数 */
uint8_t video_play_mjpeg(uint8_t *pname);                            /* 播放一个mjpeg文件 */
uint8_t video_seek(FIL *favi, AVI_INFO *aviinfo, uint8_t *mbuf);     /* AVI文件查找 */

#endif

