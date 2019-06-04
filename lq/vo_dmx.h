/**
********************************************************************************
* @file
* @author   Liu Qiang
* @version  1.0.0
* @since    2018.04.23
* @date     2019.05.28
* @brief    vo-dmx512
* @note     yuv -> yuv-resize -> rgb
********************************************************************************
**/
#ifndef __VO_DMX_H__
#define __VO_DMX_H__

#include <stdbool.h>        // bool
// #include <stdlib.h>

#include "player/core.h"    // MPContext
#include "video/mp_image.h" // mp_image_params
#include "video/vdpau.h"    // vdp_functions






/*******************************************************************************
* @brief    init
* @note     
* @param    params      : [ro] image params of yuv-src
* @param    mpctx       : [ro] context of MPV
* @retval   0           : success
* @retval   -1          : failed
*******************************************************************************/
extern int
Vo_Dmx_Init(
    struct mp_image_params* params,
    struct MPContext*       mpctx );

/*******************************************************************************
* @brief    deinit
* @note     
* @return   [void]
*******************************************************************************/
extern void
Vo_Dmx_Deinit(
    void );

/*******************************************************************************
* @brief    return init-status
* @note     
* @retval   true        : inited already.
* @retval   false       : not inited.
*******************************************************************************/
extern bool
Vo_Dmx_Is_Inited(
    void );

/*******************************************************************************
* @brief    change yuv-src param
* @note     it will result in realloc of yuv pool
* @param    params      : [ro] image params of yuv-src
* @retval   0           : success
* @retval   -1          : failed.
*******************************************************************************/
extern int
Vo_Dmx_ChangeParam(
    struct mp_image_params* params );

/*******************************************************************************
* @brief    handle of image output via dmx
* @note     yuv -> rgb -> vo
* @param    vdp         : [ro] context of vdpau function
* @param    frame       : [ro] image frame of yuv-src
* @retval   0           : success
* @retval   -1          : failed
*******************************************************************************/
extern int
Vo_Dmx_Process(
    struct vdp_functions*   vdp,
    mp_image_t*             frame );





#endif