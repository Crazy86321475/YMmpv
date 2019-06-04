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
#include "vo_dmx.h"

#include <stdint.h>             // uint32_t
#include <pthread.h>            // pthread_xxx
#include <unistd.h>             // write close; usleep
#include <sys/time.h>           // gettimeofday


// #include "video/img_format.h"
// #include "video/out/vo.h"
// #include "video/out/x11_common.h"

#include "lq/config_vodmx.h"
#include "lq/option_vodmx.h"
#include "lq/img_pool.h"
#include "lq/img_queue.h"
#include "lq/img_convert.h"
#include "lq/vo_dmx_fb.h"
#include "lq/vo_dmx_geth.h"
#include "lq/debug_vodmx.h"





#define THREAD_POOL_SIZE_YUV2RGB    ( 5 )
#define IMG_QUEUE_SIZE_YUV          ( 2 )
#define IMG_QUEUE_SIZE_RGB          ( 2 )
#define IMG_POOL_SIZE_YUV           ( THREAD_POOL_SIZE_YUV2RGB + IMG_QUEUE_SIZE_YUV )
#define IMG_POOL_SIZE_RGB           ( THREAD_POOL_SIZE_YUV2RGB + IMG_QUEUE_SIZE_RGB + 1 )





typedef enum
{
    VO_DMX_FREE,
    VO_DMX_INITED,
    VO_DMX_DEINITING,
} _Vo_Dmx_Status_t;


typedef struct
{
    /* vodmx options. set in cmdline, change by IPC */
    Vodmx_Options_t*    myopts;

    /* image-pool */
    Img_Pool_t*         p_img_pool_yuv;
    Img_Pool_t*         p_img_pool_rgb;

    /* image-queue */
    Img_Queue_t*        p_img_queue_yuv;
    Img_Queue_t*        p_img_queue_rgb;

    /* image-index of in & out, ensure image output is ordered */
    uint32_t            idx_in_last;
    uint32_t            idx_out_last;

    /* flag of init-state. ture: inited, false: not-inited. */
    bool                init_flag;

    /* vo-thread id. */
    pthread_t           tid_fb_out; //fb0/1
    pthread_t           tid_geth_out;

    /* image-convert scaler. */
    Img_Convert_t       sws[THREAD_POOL_SIZE_YUV2RGB];

    /* test. use to print count of yuv-in & rgb-out */
    int                 count_yuv;
    int                 count_rgb;
    int                 count_rgb_outdate;

    _Vo_Dmx_Status_t    status;

    // struct timeval  time_rgb_out_last;
    // struct timeval  time_yuv_get_last;
// mpctx->d_video->fps
} Vo_Dmx_Context_t;





static Vo_Dmx_Context_t* _vodmx_ctx = NULL;





static void* _thread_yuv2rgb( void* arg );
static void* _thread_rgb_out( void* arg );

// static bool _img2file_rgb( mp_image_t* img_rgb );
// static bool _img2file_yuv( mp_image_t* img_yuv );

static void _image_info_print( mp_image_t* img , char* img_name );


/* Init */
int
Vo_Dmx_Init(
    struct mp_image_params* params,
    struct MPContext*       mpctx  )
{
    int             ret = -1;
    struct MPOpts*  opts;
    enum mp_imgfmt  img_fmt_src;
    int             err;

struct timeval start,stop;
gettimeofday(&start,0);

    printf( "=====> Vo_Dmx_Init Start.\n" );

    /* step-0.1 : valid check */
    if( ( NULL == params ) ||
        ( 0 == params->w ) ||
        ( 0 == params->h ) ||
        ( NULL == mpctx  ) )
    {
        goto EXIT;
    }

    /* step-0.2 : init status check */
    if( _vodmx_ctx )
    {
        ret = 0;
        goto EXIT;
    }

    /* vodmx init start ... */
    /* step-1 : alloc context of vo-dmx */
    _vodmx_ctx = calloc( 1, sizeof( Vo_Dmx_Context_t ) );
    if( NULL == _vodmx_ctx )
    {
        goto EXIT;
    }

    /* set image-index of in/out */
    _vodmx_ctx->idx_in_last  = 0;
    _vodmx_ctx->idx_out_last = 0xFFFFFFFF;
    
    /* step-2 : get option */
    opts = mpctx->opts;
    _vodmx_ctx->myopts = opts->vodmx_opts;

    /* set debug level */
    Debug_Log_SetLevel( _vodmx_ctx->myopts->dbg );

    /* print option */
    DEBUG_LOG_INFO( ">>>>> my option params <<<<<\n" );
    DEBUG_LOG_INFO( "    dbg            :[%d]\n", _vodmx_ctx->myopts->dbg );
    DEBUG_LOG_INFO( "    out_by         :[%d]\n", _vodmx_ctx->myopts->out_by );
    DEBUG_LOG_INFO( "    out_w          :[%d]\n", _vodmx_ctx->myopts->out_w );
    DEBUG_LOG_INFO( "    out_h          :[%d]\n", _vodmx_ctx->myopts->out_h );
    DEBUG_LOG_INFO( "    out_fps        :[%d]\n", _vodmx_ctx->myopts->out_fps );
    DEBUG_LOG_INFO( "    out_x          :[%d]\n", _vodmx_ctx->myopts->out_x );
    DEBUG_LOG_INFO( "    out_y          :[%d]\n", _vodmx_ctx->myopts->out_y );
    DEBUG_LOG_INFO( "    out_light      :[%d]\n", _vodmx_ctx->myopts->out_light );
    DEBUG_LOG_INFO( "    out_gamma_r    :[%f]\n", _vodmx_ctx->myopts->out_gamma_r );
    DEBUG_LOG_INFO( "    out_gamma_g    :[%f]\n", _vodmx_ctx->myopts->out_gamma_g );
    DEBUG_LOG_INFO( "    out_gamma_b    :[%f]\n", _vodmx_ctx->myopts->out_gamma_b );
    DEBUG_LOG_INFO( "    pix_unit       :[%d]\n", _vodmx_ctx->myopts->pix_unit );
    DEBUG_LOG_INFO( "    scaler         :[%d]\n", _vodmx_ctx->myopts->scaler );
    
    /* step-3 : set out w/h as first video, if they are not set when mpv start */
    if( 0 == _vodmx_ctx->myopts->out_w )
        _vodmx_ctx->myopts->out_w = params->w;
    if( 0 == _vodmx_ctx->myopts->out_h )
        _vodmx_ctx->myopts->out_h = params->h;

    DEBUG_LOG_INFO( "image-src pool (yuv): w/h [%d/%d]\n",
        params->w, params->h );
    DEBUG_LOG_INFO( "image-dst pool (rgb): w/h [%d/%d]\n",
        _vodmx_ctx->myopts->out_w, _vodmx_ctx->myopts->out_h );

    // gettimeofday( &_vodmx_ctx->time_rgb_out_last, 0 );
    // gettimeofday( &_vodmx_ctx->time_yuv_get_last, 0 );

    /* step-4 : init vo-channel */
    switch( _vodmx_ctx->myopts->out_by )
    {
        case OPTION_VODMX_OUTBY_FB0:
            Vo_Fb_Init( VO_FB_ID_0, _vodmx_ctx->myopts );
            DEBUG_LOG_INFO( "vo_dmx via:[fb0]\n" );
            break;

        case OPTION_VODMX_OUTBY_FB1:
            Vo_Fb_Init( VO_FB_ID_1, _vodmx_ctx->myopts );
            DEBUG_LOG_INFO( "vo_dmx via:[fb1]\n" );
            break;

        case OPTION_VODMX_OUTBY_GETH:
            Vo_Geth_Init( _vodmx_ctx->myopts->pix_unit, _vodmx_ctx->myopts );
			Vo_Fb_Init( VO_FB_ID_1, _vodmx_ctx->myopts );
            DEBUG_LOG_INFO( "vo_dmx via:[geth]\n" );
            break;

        case OPTION_VODMX_OUTBY_NONE:
        default:
            DEBUG_LOG_INFO( "vo_dmx via:[none]\n" );
            break;
    }

    /* step-5 : init img-pool */
    img_fmt_src = params->imgfmt;
    if( IMGFMT_VDPAU == img_fmt_src )
    {
        img_fmt_src = IMGFMT_420P;
        // img_fmt_src = IMGFMT_NV12;
    }

    /* step-5.1 : init img-pool of yuv */
    _vodmx_ctx->p_img_pool_yuv = Img_Pool_Init(
        IMG_POOL_SIZE_YUV,
        img_fmt_src,
        params->w,
        params->h );
    if( NULL == _vodmx_ctx->p_img_pool_yuv )
    {
        DEBUG_LOG_ERROR( "Img_Pool_Init yuv failed !\n" );
        goto EXIT_ERR;
    }

    /* step-5.2 : init img-pool of rgb */
    _vodmx_ctx->p_img_pool_rgb = Img_Pool_Init(
        IMG_POOL_SIZE_RGB,
        IMG_FMT_OUT,
        _vodmx_ctx->myopts->out_w,
        _vodmx_ctx->myopts->out_h );
    if( NULL == _vodmx_ctx->p_img_pool_rgb )
    {
        DEBUG_LOG_ERROR( "Img_Pool_Init rgb failed !\n" );
        goto EXIT_ERR;
    }

    /* step-7 : init img-queue of yuv */
    _vodmx_ctx->p_img_queue_yuv = Img_Queue_Init( IMG_QUEUE_SIZE_YUV );
    if( NULL == _vodmx_ctx->p_img_queue_yuv )
    {
        DEBUG_LOG_ERROR( "Img_Queue_Init yuv failed !\n" );
        goto EXIT_ERR;
    }

    /* step-8 : init img-queue of rgb */
    _vodmx_ctx->p_img_queue_rgb = Img_Queue_Init( IMG_QUEUE_SIZE_RGB );
    if( NULL == _vodmx_ctx->p_img_queue_rgb )
    {
        DEBUG_LOG_ERROR( "Img_Queue_Init rgb failed !\n" );
        goto EXIT_ERR;
    }

    /* step-9 : alloc sws */
    for( int i = 0; i < THREAD_POOL_SIZE_YUV2RGB; ++i )
    {
        err = Img_Convert_Init( _vodmx_ctx->sws + i, ( Scaler_ID_t )_vodmx_ctx->myopts->scaler );
        _vodmx_ctx->sws[i].id = i;
    }

    /* step-10 : create _thread_yuv2rgb */
    for( int i = 0; i < THREAD_POOL_SIZE_YUV2RGB; ++i )
    {
        err = pthread_create(
            &_vodmx_ctx->sws[i].tid_yuv2rgb,
            NULL,
            _thread_yuv2rgb,
            &_vodmx_ctx->sws[i] );
        if( err != 0 )
        {
            DEBUG_LOG_ERROR( "pthread_create _thread_yuv2rgb failed !\n" );
            goto EXIT_ERR;
        }
    }

    /* step-11 : create _thread_rgb_out */
    err = pthread_create(
        &_vodmx_ctx->tid_fb_out,
        NULL,
        _thread_rgb_out,
        NULL );
    if( err != 0 )
    {
        DEBUG_LOG_ERROR( "pthread_create _thread_rgb_out failed !\n" );
        goto EXIT_ERR;
    }

    /* step-12 : create _thread_Geth_out */
    SetGethOutFps(_vodmx_ctx->myopts->out_fps);
    SetGethThreadState(GETH_THREAD_STATE_RUN);
    err = pthread_create(
        &_vodmx_ctx->tid_geth_out,
        NULL,
        _thread_Geth_out,
        NULL );
    if( err != 0 )
    {
        DEBUG_LOG_ERROR( "pthread_create _thread_Geth_out failed !\n" );
        goto EXIT_ERR;
    }

    /* vodmx init done */
    _vodmx_ctx->status = VO_DMX_INITED;
gettimeofday( &stop,0 );
DEBUG_LOG_DEBUG( "Vo_Dmx_Init [%ld]us\n",(stop.tv_sec - start.tv_sec) * 1000000 + ( stop.tv_usec - start.tv_usec ) );
    
    printf( "<===== Vo_Dmx_Init End.\n" );
    
    ret = 0;


EXIT_ERR:
    if( ret )
        Vo_Dmx_Deinit( );
EXIT:
    if( ret )
        printf( "Vo_Dmx_Init failed.\n" );
    return ret;
}



void
Vo_Dmx_Deinit(
    void )
{
    int res;
struct timeval start,stop;
gettimeofday(&start,0);

    DEBUG_LOG_INFO( "Vo_Dmx_Deinit Start.\n" );
    if( NULL == _vodmx_ctx )
    {
        DEBUG_LOG_INFO( "Vo_Dmx_Deinit - NOT INITED.\n" );
        goto EXIT;
    }
    _vodmx_ctx->count_rgb = _vodmx_ctx->p_img_queue_rgb->count;
    _vodmx_ctx->status = VO_DMX_DEINITING;

    /* step-1 : free thread */
    if( _vodmx_ctx->tid_fb_out )
    {
        pthread_cancel( _vodmx_ctx->tid_fb_out );
        res = pthread_join( _vodmx_ctx->tid_fb_out, NULL );
        if( res )
        {
            DEBUG_LOG_ERROR( "pthread_join[%d] - rgb2out\n", res );
        }
        Vo_Fb_Draw_Blank();
        _vodmx_ctx->tid_fb_out = 0;
    }

    for( int i = 0; i < THREAD_POOL_SIZE_YUV2RGB; ++i )
    {
        if( _vodmx_ctx->sws[i].tid_yuv2rgb )
        {
            pthread_cancel( _vodmx_ctx->sws[i].tid_yuv2rgb );
            res = pthread_join( _vodmx_ctx->sws[i].tid_yuv2rgb, NULL );
            if( res )
            {
                DEBUG_LOG_ERROR( "pthread_join[%d] - yuv2rgb[%d]\n", res, i );
            }
            _vodmx_ctx->sws[i].tid_yuv2rgb = 0;
        }
    }

    /* step-2 : free sws */
    for( int i = 0; i < THREAD_POOL_SIZE_YUV2RGB; ++i )
    {
        if( _vodmx_ctx->sws[i].sws_ctx )
        {
            talloc_free( _vodmx_ctx->sws[i].sws_ctx );
            _vodmx_ctx->sws[i].sws_ctx = NULL;
        }
    }

    /* step-3 : free queue */
    if( _vodmx_ctx->p_img_queue_yuv )
    {
        Img_Queue_Deinit( &_vodmx_ctx->p_img_queue_yuv );
    }

    if( _vodmx_ctx->p_img_queue_rgb )
    {
        Img_Queue_Deinit( &_vodmx_ctx->p_img_queue_rgb );
    }

    /* step-4 : free pool */
    if( _vodmx_ctx->p_img_pool_rgb )
    {
        Img_Pool_Deinit( &_vodmx_ctx->p_img_pool_rgb );
    }

    if( _vodmx_ctx->p_img_pool_yuv )
    {
        Img_Pool_Deinit( &_vodmx_ctx->p_img_pool_yuv );
    }

    /* step-5 : deinit vo-channel */
    /* step-5.1 : deinit Vo_FB */
    Vo_Fb_DeInit( );

    /* step-5.2 : deinit Vo_Geth */
    SetGethThreadState(GETH_THREAD_STATE_STOP);
    if( _vodmx_ctx->tid_geth_out ) {
        res = pthread_join( _vodmx_ctx->tid_geth_out, NULL );
        if( res )
        {
            DEBUG_LOG_ERROR( "pthread_join[%d] - _thread_Geth_out\n", res );
        }
        _vodmx_ctx->tid_geth_out = 0;
    }
    uint32_t gethOut = GetGethOutFrameNum();
    Vo_Geth_DeInit();
    printf("CPK: _vodmx_ctx->count_yuv:%d, _vodmx_ctx->count_rgb:%d, Geth_out:%u\n",
            _vodmx_ctx->count_yuv, _vodmx_ctx->count_rgb, gethOut);

    /* step-6 : free ctx */
    free( _vodmx_ctx );
    _vodmx_ctx = NULL;


EXIT:
gettimeofday( &stop,0 );
DEBUG_LOG_DEBUG( "Vo_Dmx_Deinit [%ld]us\n",(stop.tv_sec - start.tv_sec) * 1000000 + ( stop.tv_usec - start.tv_usec ) );
    DEBUG_LOG_INFO( "Vo_Dmx_Deinit End.\n" );
    return;
}



bool
Vo_Dmx_Is_Inited(
    void )
{
    if( _vodmx_ctx && ( VO_DMX_INITED == _vodmx_ctx->status ) )
        return true;
    else
        return false;
}



int
Vo_Dmx_ChangeParam(
    struct mp_image_params* params )
{
    int ret = -1;
    int err;
    enum mp_imgfmt  img_fmt_src;


    /* init status check */
    if( NULL == _vodmx_ctx )
    {
        goto EXIT;
    }

    /* valid check */
    if( ( NULL == params ) ||
        ( 0 == params->w ) ||
        ( 0 == params->h ) )
    {
        goto EXIT;
    }

    /* get new params */
    img_fmt_src = params->imgfmt;
    if( IMGFMT_VDPAU == img_fmt_src )
    {
        img_fmt_src = IMGFMT_420P;
        // img_fmt_src = IMGFMT_NV12;
    }
    DEBUG_LOG_INFO( "Vo_Dmx_ChangeParam : fmt=[%d] w/h=[%d/%d]\n",
        img_fmt_src, params->w, params->h );

    /* update params */
    err = Img_Pool_UpdateParams(
        _vodmx_ctx->p_img_pool_yuv,
        img_fmt_src, params->w, params->h );
    if( err )
    {
        goto EXIT;
    }

    ret = 0;


EXIT:
    if( ret )
    {
        DEBUG_LOG_ERROR( "Vo_Dmx_ChangeParam failed.\n" );
    }
    return ret;
}



// vo->in->current_frame->current
// _img_frame_current = frame->current;
// _img_frame_current = vc->current_image;

// vc->current_image    IMGFMT_VDPAU / IMGFMT_VDPAU_OUTPUT
// frame->current       IMGFMT_VDPAU / IMGFMT_VDPAU_OUTPUT / IMGFMT_420P / ... ?

// vo_queue_frame()
// vo->in->frame_queued
// ()
// ==> [vo->in->frame_queued] ==> [vo->in->current_frame->...] ==> [frame->current]

// mp_vdpau_upload_video_surface() :
// ==> [vc->current_image]

// video_to_output_surface() :> render_video_to_output_surface() :> mp_vdpau_mixer_render() :> vdp->video_mixer_render()
// ==> [vc->output_surfaces]
int
Vo_Dmx_Process(
    struct vdp_functions*   vdp,
    mp_image_t*             frame )
{
// struct timeval start,stop;
    int ret = -1;
    int err;
    mp_image_t* img_yuv = NULL;
    Img_Unit_t* img_ctx = NULL;
    // struct timeval current;


    /* init status check */
    if( NULL == _vodmx_ctx )
    {
        goto EXIT;
    }

    /* valid check */
    if( NULL == frame )
    {
        goto EXIT;
    }

    // test
    _vodmx_ctx->count_yuv++;

    // // fps-yuv-get control
    // gettimeofday( &current, 0 );

    _image_info_print( frame, "img_frame" );

    /* step-1 : get img_ctx */
    img_ctx = Img_Pool_Get( _vodmx_ctx->p_img_pool_yuv );
    if( NULL == img_ctx )
    {
        DEBUG_LOG_TRACE_LFPRE( "Img_Pool_Get YUV failed.\n" );
        goto EXIT;
    }
    img_yuv = img_ctx->img;

    /* step-2 : get yuv */
// gettimeofday( &start, 0 );
    if( IMGFMT_VDPAU == frame->imgfmt && vdp )
    {
        // if HW-DEC. fmt = IMGFMT_VDPAU
        VdpStatus vdp_st;
        VdpVideoSurface surface;
        uint8_t* plane_tmp;

        if( NULL == vdp )
            goto EXIT_ERR;

        surface = ( uintptr_t )frame->planes[3];
        // DEBUG_LOG_INFO( "VdpVideoSurface surface: [%d].\n", surface );
        if (surface <= 0)
        {
            DEBUG_LOG_ERROR_LFPRE( "surface <= 0.\n" );
            goto EXIT_ERR;
        }

        vdp_st = vdp->video_surface_get_bits_y_cb_cr(
            surface,
            VDP_YCBCR_FORMAT_YV12,
            ( void* const* )img_yuv->planes,
            img_yuv->stride );
        if( vdp_st != VDP_STATUS_OK )
        {
            DEBUG_LOG_ERROR_LFPRE( "video_surface_get_bits_y_cb_cr failed.\n" );
            goto EXIT_ERR;
        }

        // /* put yuv to VDPAU. show. */
        // /* fix libvdpau_sunxi out problem */
        // /* BUG: make img bad sometimes, it happened when seek. */
        // vdp_st = vdp->video_surface_put_bits_y_cb_cr(
        //     surface,
        //     VDP_YCBCR_FORMAT_YV12,
        //     ( const void* const* )img_yuv->planes,
        //     img_yuv->stride);
        // if( vdp_st != VDP_STATUS_OK )
        // {
        //     DEBUG_LOG_ERROR( "video_surface_put_bits_y_cb_cr failed.\n" );
        //     // goto EXIT_ERR;
        // }

        plane_tmp = img_yuv->planes[1];
        img_yuv->planes[1] = img_yuv->planes[2];
        img_yuv->planes[2] = plane_tmp;
        // DEBUG_LOG_INFO( "video_surface_get_bits_y_cb_cr success.\n" );
    }
    else
    {
        /* if SW-DEC. fmt = IMGFMT_YUV_XXXX */
        mp_image_copy( img_yuv, frame );
    }
    mp_image_copy_attributes( img_yuv, frame );
// gettimeofday( &stop, 0 );
// DEBUG_LOG_DEBUG_LFPRE( "get_yuv [%ld]us\n",
//     (stop.tv_sec - start.tv_sec) * 1000000 + ( stop.tv_usec - start.tv_usec ) );

    /* step-3 : put yuv to yuv2rgb thread */
    img_ctx->id = _vodmx_ctx->idx_in_last;
    err = Img_Queue_Send( _vodmx_ctx->p_img_queue_yuv, img_ctx );
    if( err )
    {
        DEBUG_LOG_TRACE_LFPRE( "Img_Queue_Send YUV failed !\n" );
        goto EXIT_ERR;
    }
    _vodmx_ctx->idx_in_last++;

    // _vodmx_ctx->time_yuv_get_last.tv_sec = current.tv_sec;
    // _vodmx_ctx->time_yuv_get_last.tv_usec = current.tv_usec;
    ret = 0;


EXIT_ERR:
    if( ret )
    {
        if( _vodmx_ctx->p_img_pool_yuv && img_ctx )
        {
            Img_Pool_Put( _vodmx_ctx->p_img_pool_yuv, img_ctx );
        }
    }

EXIT:
    return ret;
}





/*******************************************************************************
* @brief    thread unit of yuv2rgb
* @note     do yuv2rgb
* @param    arg     : [rw] arg of thread. point to a 'sws_ctx'
* @return   [void*]
*******************************************************************************/
static void*
_thread_yuv2rgb(
    void* arg )
{
    int err;
struct timeval start,stop;
    mp_image_t* img_yuv;
    mp_image_t* img_rgb;
    Img_Convert_t* sws;
    Img_Unit_t* img_unit_yuv = NULL;
    Img_Unit_t* img_unit_rgb = NULL;
    pthread_t   tid;


    if( NULL == arg )
    {
        DEBUG_LOG_ERROR_LFPRE( "_thread_yuv2rgb [arg] is NULL.\n" );
        return NULL;
    }
    sws = ( Img_Convert_t* )arg;

    tid = pthread_self();
    DEBUG_LOG_DEBUG( "_thread_yuv2rgb tid:[0x%08X] is running.\n", tid );

    while( 1 )
    {
        /* get img-rgb unit from pool */
        img_unit_rgb = Img_Pool_Get( _vodmx_ctx->p_img_pool_rgb );
        if( NULL == img_unit_rgb )
        {
            usleep( 5 * 1000 );
            continue;
        }
        img_rgb = img_unit_rgb->img;

        /* recv img-yuv unit from yuv-queue */
        while ( 1 )
        {
            img_unit_yuv = Img_Queue_Recv( _vodmx_ctx->p_img_queue_yuv );
            if( img_unit_yuv && img_unit_yuv->img )
                break;
            usleep( 5 * 1000 );
        }
        img_yuv = img_unit_yuv->img;
        
        // [SIGMA]--------------------------------------- ?
        // if remove. show:'[ffmpeg] swscaler: No accelerated colorspace conversion found from yuv420p to bgr24.'
        mp_image_copy_attributes( img_rgb, img_yuv );

        _image_info_print( img_yuv, "img_yuv" );
        _image_info_print( img_rgb, "img_rgb" );

        img_unit_rgb->id = img_unit_yuv->id;

        /* yuv -> rgb */
gettimeofday( &start, 0 );
        err = Img_Convert_Do( sws, img_rgb, img_yuv );
        /* put img-rgb unit back to pool */
        Img_Pool_Put( _vodmx_ctx->p_img_pool_yuv, img_unit_yuv );
        if( err )
        {
            DEBUG_LOG_ERROR_LFPRE( "Img_Convert yuv->rgb failed !\n" );
            continue;
        }
gettimeofday( &stop, 0 );
DEBUG_LOG_TRACE_LFPRE( "Img_Convert yuv->rgb [%6ld]us (id:[%d])\n",
    ( stop.tv_sec - start.tv_sec ) * 1000000 + ( stop.tv_usec - start.tv_usec ), sws->id );

        /* send img-rgb to rgb-queue */
        // while ( 1 )
        {
            err = Img_Queue_Send( _vodmx_ctx->p_img_queue_rgb, img_unit_rgb );
            if( err )
                DEBUG_LOG_DEBUG_LFPRE( "Img_Queue_Send rgb failed !\n" );
            // if( 0 == err )
            //     break;
            // usleep( 5 * 1000 );
        }
    }

    return NULL;
}



/*******************************************************************************
* @brief    thread unit of rgb-out
* @note     do rgb-out
* @param    arg     : [ro] arg of thread. NULL, Nothing
* @return   [void*]
*******************************************************************************/
static void*
_thread_rgb_out(
    void* arg )
{
    Img_Unit_t*     img_unit_rgb = NULL;
    int             count_timeout = 0;
    //int             fps_us;
    //struct timeval  time_out_last;
    struct timeval  time_print_outinfo_last;
    struct timeval  time_tmp;
    int             interval_us;


    //gettimeofday( &time_out_last, 0 );
    // time_out_last.tv_sec  = 0;
    // time_out_last.tv_usec = 0;
    gettimeofday( &time_print_outinfo_last, 0 );
    // time_print_outinfo_last.tv_sec  = 0;
    // time_print_outinfo_last.tv_usec = 0;
    DEBUG_LOG_DEBUG( "_thread_rgb_out tid:[0x%08X] is running !\n", pthread_self() );

    while( 1 )
    {
        /* step-1 : get img-rgb unit */
        img_unit_rgb = Img_Queue_Recv( _vodmx_ctx->p_img_queue_rgb );
        if( NULL == img_unit_rgb )
        {
            usleep( 5 * 1000 );

            /* [vo-geth] draw blank every 100ms if no image out */
            if( OPTION_VODMX_OUTBY_GETH == _vodmx_ctx->myopts->out_by )
            {
                count_timeout++;
                if( count_timeout > 20 )
                {
                    count_timeout = 0;
                    Vo_Geth_Draw_Blank(
                        _vodmx_ctx->myopts->out_w,
                        _vodmx_ctx->myopts->out_h );
                }
            }
            continue;
        }
        count_timeout = 0;
#if 0
        /* step-2 : out-fps control */
        if( _vodmx_ctx->myopts->out_fps > 0 )
        {
            gettimeofday( &time_tmp, 0 );
            interval_us = ( time_tmp.tv_sec - time_out_last.tv_sec ) * 1000000 + ( time_tmp.tv_usec - time_out_last.tv_usec );
            // refresh everytime because out-fps could be changed with IPC-command
            fps_us = ( 1000000 / _vodmx_ctx->myopts->out_fps );
            if( interval_us < fps_us )
            {
                DEBUG_LOG_TRACE( "out-fps control delay:[%d]us\n", fps_us - interval_us );
                usleep( fps_us - interval_us );
            }
            gettimeofday( &time_out_last, 0 );
        }
#endif
        /* step-3 : rgb -> out */
        if( ( img_unit_rgb->id > _vodmx_ctx->idx_out_last ) ||
            (   ( 0 == ( img_unit_rgb->id         & 0x80000000 ) ) &&
                ( 0 != ( _vodmx_ctx->idx_out_last & 0x80000000 ) ) ) )
        {
            switch( _vodmx_ctx->myopts->out_by )
            {
                case OPTION_VODMX_OUTBY_FB0:
                case OPTION_VODMX_OUTBY_FB1:
                    Vo_Fb_Draw( img_unit_rgb->img );
                    break;

                case OPTION_VODMX_OUTBY_GETH:
                    Vo_Geth_queue( img_unit_rgb->img );
                    Vo_Fb_Draw( img_unit_rgb->img );
                    break;

                case OPTION_VODMX_OUTBY_NONE:
                default:
                    break;
            }
            _vodmx_ctx->idx_out_last = img_unit_rgb->id;
            //_vodmx_ctx->count_rgb++;
        }
        else
        {
            /* if idx_out_last > idx_in_last, this image is outdate and will be drop. */
            _vodmx_ctx->count_rgb_outdate++;
            DEBUG_LOG_DEBUG_LFPRE( "img-rgb is outdate. id:[%d] last:[%d] sum:[%d]\n",
                img_unit_rgb->id, _vodmx_ctx->idx_out_last, _vodmx_ctx->count_rgb_outdate );
        }

        /* step-4 : put img-rgb unit */
        Img_Pool_Put( _vodmx_ctx->p_img_pool_rgb, img_unit_rgb );

        // _vodmx_ctx->count_rgb++;
        // if( _vodmx_ctx->count_rgb % 1000 == 0 )
        // if( _vodmx_ctx->count_rgb % ( _vodmx_ctx->myopts->out_fps * 10 ) == 0 )
        gettimeofday( &time_tmp, 0 );
        interval_us =
            ( time_tmp.tv_sec  - time_print_outinfo_last.tv_sec ) * 1000000 +
            ( time_tmp.tv_usec - time_print_outinfo_last.tv_usec );
        if( interval_us >= 10 * 1000000 )
        {
            time_print_outinfo_last.tv_sec  = time_tmp.tv_sec;
            time_print_outinfo_last.tv_usec = time_tmp.tv_usec;
            DEBUG_LOG_DEBUG_LFPRE( "=====yuv/rgb/rgb_outdate [%d/%d/%d]\n",
                _vodmx_ctx->count_yuv,
                _vodmx_ctx->count_rgb,
                _vodmx_ctx->count_rgb_outdate );
        }
    }
    
    return NULL;
}



// static bool
// _img2file_rgb(
//     mp_image_t* img_rgb )
// {
//     int fd;
//     uint8_t* src;
//     int count;
//     int out_w = 1024;
//     int out_h = 600;

//     if( NULL == img_rgb )
//         goto EXIT_ERR;

//     if( img_rgb->num_planes != 1 )
//         goto EXIT_ERR;

//     fd = open( "/lq/mount/nfs/rgb", O_CREAT|O_WRONLY, 0777 );
//     if( fd < 0 )
//     {
//         DEBUG_LOG_ERROR( "file open failed\n" );
//         goto EXIT_ERR;
//     }


//     // RGB
//     count = 0;
//     src = img_rgb->planes[0];
//     out_w = ( out_w > img_rgb->w ) ? img_rgb->w : out_w;
//     out_h = ( out_h > img_rgb->h ) ? img_rgb->h : out_h;
//     for( int h = 0; h < out_h; h++ )
//     {
//         count = write( fd, src, out_w * 4 );
//         if( count < out_w )
//         {
//             DEBUG_LOG_ERROR( "write rgb failed when src:%d, num:%d, done:%d.\n", src - img_rgb->planes[0], out_w, count  );
//             break;
//         }
//         src += img_rgb->stride[0];
//     }
//     // if( count == out_w * 4 )
//     // {
//     //     DEBUG_LOG_INFO( "write rgb to file success.\n" );
//     // }
//     close( fd );
//     return true;

// EXIT_ERR:
//     return false;
// }


// static bool
// _img2file_yuv(
//     mp_image_t* img_yuv )
// {
//     int fd;
//     uint8_t* src;
//     int count;

//     if( NULL == img_yuv )
//         goto EXIT_ERR;

//     if( img_yuv->num_planes != 3 )
//         goto EXIT_ERR;

//     fd = open( "/lq/mount/nfs/yuv", O_CREAT|O_WRONLY, 0777 );
//     if( fd < 0 )
//     {
//         DEBUG_LOG_ERROR( "file open failed\n" );
//         goto EXIT_ERR;
//     }

//     // Y
//     count = 0;
//     src = img_yuv->planes[0];
//     for( int h = 0; h < img_yuv->h; h++ )
//     {
//         count = write( fd, src, img_yuv->w );
//         if( count < img_yuv->w )
//         {
//             DEBUG_LOG_ERROR( "write Y failed when src:%d, num:%d, done:%d.\n", src - img_yuv->planes[0], img_yuv->w, count  );
//             break;
//         }
//         src += img_yuv->stride[0];
//     }
//     if( count == img_yuv->w )
//     {
//         DEBUG_LOG_INFO( "write rgb to file success.\n" );
//     }

//     // U
//     src = img_yuv->planes[1];
//     for( int h = 0; h < img_yuv->h / 2; h++ )
//     {
//         count = write( fd, src, img_yuv->w / 2 );
//         if( count < img_yuv->w /2 )
//         {
//             DEBUG_LOG_ERROR( "write U failed when src:0x%X, num:%d, done:%d.\n", src - img_yuv->planes[1], img_yuv->w, count  );
//             break;
//         }
//         src += img_yuv->stride[1];
//     }

//     // V ...
//     src = img_yuv->planes[2];
//     for( int h = 0; h < img_yuv->h / 2; h++ )
//     {
//         count = write( fd, src, img_yuv->w / 2 );
//         if( count < img_yuv->w /2 )
//         {
//             DEBUG_LOG_ERROR( "write V failed when src:0x%X, num:%d, done:%d.\n", src - img_yuv->planes[2], img_yuv->w, count  );
//             break;
//         }
//         src += img_yuv->stride[2];
//     }

//     // if( src == img_yuv->w * img_yuv->h )
//     // {
//         // DEBUG_LOG_INFO( "write yuv to file success.\n" );
//     // }
//     // mp_image_unrefp(&img_yuv);

//     close( fd );
//     return true;

// EXIT_ERR:
//     return false;
// }



/*******************************************************************************
* @brief    print image params
* @note     
* @param    img         : [ro] image
* @param    img_name    : [ro] name of image
* @return   [void]
*******************************************************************************/
static void
_image_info_print(
    mp_image_t* img,
    char*       img_name )
{
    /*
    if( !img  )
        return;
    if( !img_name )
        img_name = "null";

    DEBUG_LOG_INFO( "[Info] [%s] ", img_name );
    DEBUG_LOG_INFO( "w/h:%dx%d, ", img->w, img->h );
    DEBUG_LOG_INFO( "num_planes:%d, ", img->num_planes );
    DEBUG_LOG_INFO( "stride0:%d, ", img->stride[0] );
    DEBUG_LOG_INFO( "params.w/h:%dx%d, ", img->params.w, img->params.h );
    DEBUG_LOG_INFO( "params.d_w/d_h:%dx%d, ", img->params.d_w, img->params.d_h );

    DEBUG_LOG_INFO( "imgfmt:%d, ", img->imgfmt );
    DEBUG_LOG_INFO( "params.imgfmt:%d, ", img->params.imgfmt );

    DEBUG_LOG_INFO( "fmt.avformat:%d, ", img->fmt.avformat );
    DEBUG_LOG_INFO( "fmt.flags:0x%X, ", img->fmt.flags );

    printf( "\n" );
    */
}




    
    // data = img->planes[0]
    // size = img->stride[0] * img->h
    // talloc_free(img);


    // lq test ....................................
    // if( NULL == libvdpau_sunxi )
    // {
    //     libvdpau_sunxi = dlopen("/usr/lib/vdpau/libvdpau_sunxi.so", RTLD_LAZY);
    //     if (!libvdpau_sunxi) {
    //         DEBUG_LOG_INFO( "Failed to open libvdpau_sunxi.so: %s", dlerror());
    //     }
    // }
    // if( ( NULL == handle_get) &&  libvdpau_sunxi )
    // {
    //     handle_get = dlsym(libvdpau_sunxi, "handle_get" );
    //     if (!handle_get) {
    //         fprintf(stderr, "Failed to find the symbol handle_get\n" );
    //     }
    // }

    // if( handle_get )
    // {
        // struct vdpctx* vc = vo->priv;
        // VdpOutputSurface surface = vc->output_surfaces[vc->surface_num];
        // output_surface_ctx_t* os = handle_get(surface);
        // if( !os )
        // {
        //     DEBUG_LOG_ERROR( "os null !\n" );
        // }
        // else
        // {
        //     if( os->vs )
        //     {
        //         DEBUG_LOG_INFO( "os->vs->source_format : %d \n", os->vs->source_format );
        //     }
        //     else
        //     {
        //         DEBUG_LOG_ERROR( "os->vs is NULL \n" );
        //     }
        // }
    // }
        // printf( "\n" );


// output_surface_ctx_t;
// VdpHandle;
// void* handle_get(VdpHandle handle);

//     struct vdpctx* vc = vo->priv;
//     VdpOutputSurface surface = vc->output_surfaces[vc->surface_num];
//     output_surface_ctx_t* out = handle_get(surface);
//     if (!out)
//     {
//     }
//         return VDP_STATUS_INVALID_HANDLE;
//     out->vs->source_format
    
    
    
    
//     VdpDeviceCreateX11* pvdp_device_create_x11;
    
//     void* libvdpau = dlopen("../src/.libs/libvdpau.so", RTLD_LAZY);
//     if (!libvdpau) {
//         fprintf(stderr, "Failed to open libvdpau.so: %s", dlerror());
//         return FAIL;
//     }
//     pvdp_device_create_x11 = dlsym(libvdpau, "vdp_device_create_x11" );
//     if (!pvdp_device_create_x11) {
//         fprintf(stderr, "Failed to find the symbol vdp_device_create_x11\n" );
//         return FAIL;
//     }
//     dlclose(libvdpau);
