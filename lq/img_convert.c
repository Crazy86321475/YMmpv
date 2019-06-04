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
#include "img_convert.h"

#include <libswscale/swscale.h> // FFmpeg.libswscale - SWS_BILINEAR

#include "lq/debug_vodmx.h"





// #define VODMX_SWS_SCALER            ( SWS_FAST_BILINEAR )
// #define VODMX_SWS_SCALER            ( SWS_BILINEAR )
// #define VODMX_SWS_SCALER            ( SWS_POINT )
static const char* scaler_name[ SCALER_ID_MAX ] =
{
    [ SCALER_ID_POINT           ] = "POINT",
    [ SCALER_ID_FAST_BILINEAR   ] = "FAST_BILINEAR",
    [ SCALER_ID_BILINEAR        ] = "BILINEAR",
};

static const int scaler_swsflag[ SCALER_ID_MAX ] =
{
    [ SCALER_ID_POINT           ] = SWS_POINT,
    [ SCALER_ID_FAST_BILINEAR   ] = SWS_FAST_BILINEAR,
    [ SCALER_ID_BILINEAR        ] = SWS_BILINEAR,
};





int
Img_Convert_Init(
    Img_Convert_t*  img_convert,
    Scaler_ID_t     scaler_id )
{
    int ret = -1;


    // param check
    if( NULL == img_convert )
    {
        goto EXIT;
    }

    if( ( scaler_id <  0 ) ||
        ( scaler_id >= SCALER_ID_MAX ) )
    {
        scaler_id = SCALER_ID_POINT;
        DEBUG_LOG_INFO( "scaler_id is invalid, use default scaler.\n" );
    }
    DEBUG_LOG_INFO( "Img_Convert_Init use scaler (%s).\n", scaler_name[scaler_id] );

    // alloc sws-context
    img_convert->sws_ctx = mp_sws_alloc( NULL );
    if( NULL == img_convert->sws_ctx )
    {
        DEBUG_LOG_ERROR( "mp_sws_alloc failed.\n" );
        goto EXIT;
    }

    // set scaler
    img_convert->sws_ctx->flags = scaler_swsflag[scaler_id];

    ret = 0;

EXIT:
    return ret;
}



int
Img_Convert_Do(
    Img_Convert_t*  img_convert,
    mp_image_t*     img_dst,
    mp_image_t*     img_src )
{
    int                     ret = -1;
    int                     err;
    struct mp_sws_context*  sws_ctx;


    if( ( img_convert && img_src && img_dst ) == false )
        goto EXIT;

    sws_ctx = img_convert->sws_ctx;
    if( NULL == sws_ctx )
        goto EXIT;

// int mp_image_swscale(mp_image_t* dst, mp_image_t* src, int my_sws_flags);
// extern const int mp_sws_hq_flags;
// extern const int mp_sws_fast_flags;
// const int mp_sws_fast_flags = SWS_BILINEAR;
//     if (mp_image_swscale(img_dst, img_src, mp_sws_fast_flags) < 0)
    // if (mp_image_swscale(img_dst, img_src, mp_sws_hq_flags) < 0)
    // if (mp_image_swscale(img_dst, img_src, SWS_BILINEAR) < 0)
    
    err = mp_sws_scale( sws_ctx, img_dst, img_src );
    if( err < 0 )
    {
        goto EXIT;
    }

    ret = 0;


EXIT:
    return ret;
}




