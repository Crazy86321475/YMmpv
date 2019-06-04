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
#include "option_vodmx.h"

#include "lq/debug_vodmx.h"
#include "lq/img_convert.h"





static const Vodmx_Options_t _Vodmx_options_default =
{
    .out_by         = OPTION_VODMX_OUTBY_DEF,
    .out_w          = OPTION_VODMX_WIDTH_DEF,
    .out_h          = OPTION_VODMX_HEIGHT_DEF,
    .out_fps        = 0,

    .out_x          = 0,
    .out_y          = 0,

    .out_light      = OPTION_VODMX_LIGHT_DEF,
    .out_gamma_r    = OPTION_VODMX_GAMMA_DEF,
    .out_gamma_g    = OPTION_VODMX_GAMMA_DEF,
    .out_gamma_b    = OPTION_VODMX_GAMMA_DEF,
    .pix_unit       = 256,

    .HDMI_extend  = 0,
    .dbg            = LOGLEVEL_ID_NONE,
    // .out_fps        = 0,
    .scaler         = SCALER_ID_POINT,
    // .out_fs         = 0,
};


#define OPT_BASE_STRUCT Vodmx_Options_t
const struct m_sub_options Vodmx_options =
{
    .opts = ( const m_option_t[] )
    {
        OPT_CHOICE(     "out_by",       out_by,         0,
            (
                { "none",   OPTION_VODMX_OUTBY_NONE },
                { "fb0",    OPTION_VODMX_OUTBY_FB0 },
                { "fb1",    OPTION_VODMX_OUTBY_FB1 },
                { "geth",   OPTION_VODMX_OUTBY_GETH }
            ),
            OPTDEF_INT( OPTION_VODMX_OUTBY_DEF ) ),

        OPT_INTRANGE(   "out_w",        out_w,          0,
            OPTION_VODMX_WIDTH_MIN,
            OPTION_VODMX_WIDTH_MAX,
            OPTDEF_INT( OPTION_VODMX_WIDTH_DEF ) ),
        OPT_INTRANGE(   "out_h",        out_h,          0,
            OPTION_VODMX_HEIGHT_MIN,
            3840,//OPTION_VODMX_HEIGHT_MAX, //if in HDMI_extend mode, out height can beyond max.
            OPTDEF_INT( OPTION_VODMX_HEIGHT_DEF ) ),
        OPT_INT(        "out_fps",      out_fps,        0 ),

        OPT_INT(        "out_x",        out_x,          0 ),
        OPT_INT(        "out_y",        out_y,          0 ),

        OPT_INTRANGE(   "out_light",    out_light,      0,
            OPTION_VODMX_LIGHT_MIN,
            OPTION_VODMX_LIGHT_MAX,
            OPTDEF_INT( OPTION_VODMX_LIGHT_DEF ) ),
        OPT_FLOATRANGE( "out_gamma_r",  out_gamma_r,    0,
            OPTION_VODMX_GAMMA_MIN,
            OPTION_VODMX_GAMMA_MAX,
            OPTDEF_FLOAT( OPTION_VODMX_GAMMA_DEF ) ),
        OPT_FLOATRANGE( "out_gamma_g",  out_gamma_g,    0,
            OPTION_VODMX_GAMMA_MIN,
            OPTION_VODMX_GAMMA_MAX,
            OPTDEF_FLOAT( OPTION_VODMX_GAMMA_DEF ) ),
        OPT_FLOATRANGE( "out_gamma_b",  out_gamma_b,    0,
            OPTION_VODMX_GAMMA_MIN,
            OPTION_VODMX_GAMMA_MAX,
            OPTDEF_FLOAT( OPTION_VODMX_GAMMA_DEF ) ),

        OPT_INT(        "pix_unit",     pix_unit,       0,
            OPTDEF_INT( 256 ) ),

        OPT_CHOICE(   "HDMI_extend",    HDMI_extend,      0,
            (
                { "no",   OPTION_VODMX_HDMIOUT_MODE_NORMAL },
                { "yes",    OPTION_VODMX_HDMIOUT_MODE_EXTEND }
            ),
            OPTDEF_INT( OPTION_VODMX_HDMIOUT_MODE_NORMAL ) ),
            
        OPT_INTRANGE(   "dbg",          dbg,            0,
            LOGLEVEL_ID_NONE,
            LOGLEVEL_ID_MAX,
            OPTDEF_INT( LOGLEVEL_ID_NONE ) ),

        OPT_INT(        "scaler",       scaler,         0,
            OPTDEF_INT( SCALER_ID_POINT ) ),
        // OPT_INT(        "out_fs",       out_fs,         0 ),
        { 0 }
    },
    .size = sizeof( Vodmx_Options_t ),
    .defaults = &_Vodmx_options_default,
};




