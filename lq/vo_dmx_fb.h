/**
********************************************************************************
* @file
* @author   Liu Qiang
* @version  1.0.0
* @since    2019.04.11
* @date     2019.05.24
* @brief    vo-framebuffer
********************************************************************************
**/
#ifndef __VO_DMX_FB_H__
#define __VO_DMX_FB_H__

#include "video/mp_image.h"     // mp_image_t

#include "lq/option_vodmx.h"





typedef enum
{
    VO_FB_ID_0,     /**< fb0 */
    VO_FB_ID_1,     /**< fb1 */

    VO_FB_ID_MAX,
} Vo_Fb_ID_t;





/*******************************************************************************
* @brief    init vo-fb
* @note     
* @param    fb_id       : [ro] framebuffer id/index
* @param    myopts      : [ro] my option context
* @retval   0           : success
* @retval   -1          : failed
*******************************************************************************/
extern int
Vo_Fb_Init(
    Vo_Fb_ID_t          fb_id,
    Vodmx_Options_t*    myopts );

/*******************************************************************************
* @brief    deinit vo-fb
* @note     
* @retval   0           : success
* @retval   -1          : failed
*******************************************************************************/
extern int
Vo_Fb_DeInit(
    void );

/*******************************************************************************
* @brief    send rgb data to dmx by fb
* @note     
* @param    img_rgb     : [ro] image of rgb
* @retval   0           : success
* @retval   -1          : failed
*******************************************************************************/
extern int
Vo_Fb_Draw(
    mp_image_t* img_rgb );


extern int Vo_Fb_Draw_Blank(void);


#endif