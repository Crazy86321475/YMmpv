/**
********************************************************************************
* @file
* @author   Liu Qiang
* @version  1.0.3
* @since    2018.04.23
* @date     2019.04.22
* @brief    option-vodmx
********************************************************************************
**/
#ifndef __OPTION_VODMX_H__
#define __OPTION_VODMX_H__

#include "options/m_option.h"   // m_sub_options





#define OPTION_VODMX_PREFIX         ( "my" )


#define OPTION_VODMX_WIDTH_MIN      ( 0 )
#define OPTION_VODMX_WIDTH_MAX      ( 1920 )
#define OPTION_VODMX_WIDTH_DEF      ( 0 )

#define OPTION_VODMX_HEIGHT_MIN     ( 0 )
#define OPTION_VODMX_HEIGHT_MAX     ( 1080 )
#define OPTION_VODMX_HEIGHT_DEF     ( 0 )

#define OPTION_VODMX_LIGHT_MIN      ( 0 )
#define OPTION_VODMX_LIGHT_MAX      ( 255 )
#define OPTION_VODMX_LIGHT_DEF      ( 100 )

#define OPTION_VODMX_GAMMA_MIN      ( 0.1 )
#define OPTION_VODMX_GAMMA_MAX      ( 3.0 )
#define OPTION_VODMX_GAMMA_DEF      ( 1.0 )

#define OPTION_VODMX_OUTBY_NONE     ( 0 )
#define OPTION_VODMX_OUTBY_FB0      ( 1 )
#define OPTION_VODMX_OUTBY_FB1      ( 2 )
#define OPTION_VODMX_OUTBY_GETH     ( 3 )
#define OPTION_VODMX_OUTBY_DEF      ( OPTION_VODMX_OUTBY_FB0 )


#define OPTION_VODMX_HDMIOUT_MODE_NORMAL    0
#define OPTION_VODMX_HDMIOUT_MODE_EXTEND    1



typedef struct Vodmx_Options_t
{
    // vo options
    int     out_by;         /**< vo channel. geth, hdmi */

    /*pk test multiply thread!*/
    int     out_w;          /**< vo width. */
    int     out_h;          /**< vo height. */
    int     out_fps;        /**< max fps. 0-auto */

    // vo-hdmi options
    int     out_x;          /**< vo x-offset. */
    int     out_y;          /**< vo y-offset. */

    // vo-geth options
    int     out_light;      /**< light param. */
    float   out_gamma_r;    /**< gamma param - red. */
    float   out_gamma_g;    /**< gamma param - green. */
    float   out_gamma_b;    /**< gamma param - blue. */
    int     pix_unit;       /**< num of pixel per packets. TEST!!! */

    int     HDMI_extend;  /* Have a extern mode stragely for led screen */

    int     dbg;            /**< debug. TEST!!! */

    // other options
    int     scaler;         /**< scaler. */
    // int     out_fs;
} Vodmx_Options_t;





extern const struct m_sub_options Vodmx_options;





#endif
