/**
********************************************************************************
* @file
* @author   Liu Qiang
* @version  0.1.0
* @since    2019.01.18
* @date     2019.04.20
* @brief    vo-geth
********************************************************************************
**/
#ifndef __VO_DMX_GMAC_H__
#define __VO_DMX_GMAC_H__

#include "video/mp_image.h" // mp_image_t

#include "lq/option_vodmx.h"

#define GETH_THREAD_STATE_STOP  false
#define GETH_THREAD_STATE_RUN   true




/*******************************************************************************
* @brief    init vo-geth
* @note     do not call agian times unless call Vo_Geth_DeInit
* @param    pix_num     : [ro] num of pixel per packet
* @param    myopts      : [ro] my option context
* @retval   0           : success
* @retval   -1          : failed
*******************************************************************************/
extern int
Vo_Geth_Init(
    int                 pix_num,
    Vodmx_Options_t*    myopts );

/*******************************************************************************
* @brief    deinit vo-geth
* @note     
* @retval   0           : success
* @retval   -1          : failed
*******************************************************************************/
extern int
Vo_Geth_DeInit(
    void );

/*******************************************************************************
* @brief    send rgb-data to dmx by geth
* @note     
* @param    img_rgb     : [ro] image of rgb
* @retval   0           : success
* @retval   -1          : failed
*******************************************************************************/

void SetGethThreadState(bool state);
uint32_t GetGethOutFrameNum(void);
void SetGethOutFps(uint32_t out_fps);
int Vo_Geth_queue(mp_image_t *img_rgb);
void* _thread_Geth_out(void *arg );

/*******************************************************************************
* @brief    send rgb-data blank to dmx by geth
* @note     
* @param    out_w       : [ro] out width
* @param    out_h       : [ro] out height
* @retval   0           : success
* @retval   -1          : failed
*******************************************************************************/
extern int
Vo_Geth_Draw_Blank(
    int out_w,
    int out_h );





#endif
