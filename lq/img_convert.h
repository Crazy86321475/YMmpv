/**
********************************************************************************
* @file
* @author   Liu Qiang
* @version  1.0.0
* @since    2018.04.23
* @date     2019.05.28
* @brief    image conversion
********************************************************************************
**/
#ifndef __IMG_CONVERT_H__
#define __IMG_CONVERT_H__

#include "video/mp_image.h"     // mp_image_t
#include "video/sws_utils.h"    // mp_sws_context





typedef enum
{
    SCALER_ID_POINT,                /*< very fast,  low-quality if zoom-in */
    SCALER_ID_FAST_BILINEAR,        /*< fast,       medium-quality */
    SCALER_ID_BILINEAR,             /*< slow,       medium-quality */

    SCALER_ID_MAX
} Scaler_ID_t;

typedef struct
{
    int                     id;
    pthread_t               tid_yuv2rgb;
    struct mp_sws_context*  sws_ctx;
} Img_Convert_t;





/*******************************************************************************
* @brief    init image-convert for yuv->rgb
* @note     
* @param    img_convert : [rw] obj of img_convert
* @param    scaler_id   : [ro] id of scaler
* @retval   0           : success
* @retval   -1          : failed
*******************************************************************************/
extern int
Img_Convert_Init(
    Img_Convert_t*  img_convert,
    Scaler_ID_t     scaler_id );

/*******************************************************************************
* @brief    do yuv->rgb
* @note     this work is time-consuming
* @param    img_convert : [rw] obj of img_convert
* @param    img_dst     : [rw] image of dst
* @param    img_src     : [ro] image of src
* @retval   0           : success
* @retval   -1          : failed
*******************************************************************************/
extern int
Img_Convert_Do(
    Img_Convert_t*  img_convert,
    mp_image_t*     img_dst,
    mp_image_t*     img_src );





#endif