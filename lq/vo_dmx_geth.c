/**
********************************************************************************
* @file
* @author   Liu Qiang
* @version  1.0.0
* @since    2019.01.18
* @date     2019.05.25
* @brief    vo-geth
********************************************************************************
**/
#include "vo_dmx_geth.h"

#include <stdio.h>      // printf
// #include <string.h>
#include <stdint.h>     // uint32_t, uint8_t
#include <stdbool.h>    // true/false
#include <stdlib.h>     // atof
#include <math.h>       // pow

// #include <sys/types.h>
// #include <sys/socket.h>
// #include <netinet/in.h>
#include <arpa/inet.h>  // inet_addr
#include <unistd.h>     // close
#include <net/if.h>     // ifreq

#include <sys/time.h>   // gettimeofday

#include "lq/option_vodmx.h"
#include "lq/debug_vodmx.h"
#include "lq/img_queue.h"





#define SERVER_IF           "eth0"
// #define SERVER_IP           "192.168.1.255"
#define SERVER_IP           "255.255.255.255"
#define SERVER_PORT         ( 61166 )

#define DEFAULT_PIXEL_UNIT  ( 256 )
#define DEFAULT_BYTES_UNIT  ( 773 )     // 3 + 3 * 256
#define IMG_QUEUE_SIZE_RGB2GETH     ( 2 )


#define SEND_TEST_MARCO 1


typedef enum
{
    FUN_CODE_RGBDATA    = 0x30,         /**< data frame */
    FUN_CODE_FRAMESYNC  = 0x33,         /**< sync frame */
} Fun_Code_t;

typedef struct
{
    uint8_t buf[DEFAULT_BYTES_UNIT];    /**< buf */
    float   gamma;                      /**< gamma param, last save */
    bool    is_calc;                    /**< gamma param, last save */
} SyncFrame_t;

typedef struct
{
    uint8_t*    buf;                /**< send-buf */
    int         buf_len;            /**< len of send-buf, 'number of bytes' sent per packet */
    int         pixel_per_packet;   /**< 'number of pixels' sent per packet */
} DataFrame_t;



static Img_Queue_t *g_img_queue_rgb2geth;



// socket-fd of udp
static int                  _geth_fd = -1;

// address of server
static struct sockaddr_in   _ser_addr;
static socklen_t            _ser_addr_len;

// rgb-data frame buffer
// static uint8_t*             _frame_rgbdata = NULL;
static DataFrame_t*         _frame_rgbdata = NULL;

// sync frame buffer, save gamma table.
// 3 sync frame: 0-r, 1-g, 2-b
static SyncFrame_t          _frame_sync[3];
static unsigned int         _frame_sync_idx = 0;

// my-option
static Vodmx_Options_t*     _myopts_ptr = NULL;
static float*               _myopts_gamma_ptr[3];





static int
_Vo_Geth_SendRGB(
    uint8_t* rgb_data,
    uint32_t pixel_num );

static int
_Vo_Geth_SendRGB_Blank(
    uint32_t pixel_num );

static int
_Vo_Geth_SendSync(
    void );

static int
_GammaTable_Calc_8to16(
    uint8_t*    gamma_table,
    float       gamma );





int
Vo_Geth_Init(
    int                 pix_num,
    Vodmx_Options_t*    myopts )
{
    int             ret = -1;
    int             res;
    int             opt_broadcast;
    struct ifreq    opt_interface;


    /* check init status */
    if( _geth_fd >= 0 )
    {
        // inited already.
        ret = 0;
        goto EXIT;
    }

    /* check param */
    if( NULL == myopts )
    {
        // param invalid.
        goto EXIT;
    }

    /* set my-options */
    _myopts_ptr = myopts;
    _myopts_gamma_ptr[0] = &myopts->out_gamma_r;
    _myopts_gamma_ptr[1] = &myopts->out_gamma_g;
    _myopts_gamma_ptr[2] = &myopts->out_gamma_b;

    /* alloc data-frame */
    _frame_rgbdata = calloc( 1, sizeof( DataFrame_t ) );
    if( NULL == _frame_rgbdata )
    {
        DEBUG_LOG_ERROR( "malloc fail!\n" );
        goto EXIT;
    }

    /* set 'number of pixels' sent per packet */
    _frame_rgbdata->pixel_per_packet = pix_num;
    if( _frame_rgbdata->pixel_per_packet <= 0 )
        _frame_rgbdata->pixel_per_packet = DEFAULT_PIXEL_UNIT;

    /* set 'number of bytes' sent per packet */
    _frame_rgbdata->buf_len = 3 + 3 * _frame_rgbdata->pixel_per_packet + 2;

    /* alloc sendbuf in data-frame */
    _frame_rgbdata->buf = malloc( _frame_rgbdata->buf_len );
    if( NULL == _frame_rgbdata->buf )
    {
        DEBUG_LOG_ERROR( "malloc fail!\n" );
        goto EXIT;
    }

    /* create socket. IPV4 UDP */
    _geth_fd = socket( AF_INET, SOCK_DGRAM, 0 );
    if( _geth_fd < 0 )
    {
        DEBUG_LOG_ERROR( "create socket fail!\n" );
        goto EXIT;
    }

    /* bind netif */
    strncpy( opt_interface.ifr_ifrn.ifrn_name, SERVER_IF, IFNAMSIZ );
    res = setsockopt(
        _geth_fd, SOL_SOCKET, SO_BINDTODEVICE,
        ( char* )&opt_interface, sizeof( opt_interface ) );
    if( res < 0 )
    {
        perror( "setsockopt SO_BINDTODEVICE fail!\n" );
        goto EXIT;
    }

    /* enable broadcast */
    opt_broadcast = 1;
    res = setsockopt(
        _geth_fd, SOL_SOCKET, SO_BROADCAST,
        ( char* )&opt_broadcast, sizeof( opt_broadcast ) );
    if( res == -1 )
    {
        DEBUG_LOG_ERROR( "setsockopt SO_BROADCAST fail!\n" );
        goto EXIT;
    }
    
    /* set server address */
    memset( &_ser_addr, 0, sizeof(_ser_addr) );
    _ser_addr.sin_family = AF_INET;
    _ser_addr.sin_addr.s_addr = inet_addr( SERVER_IP );
    _ser_addr.sin_port = htons( SERVER_PORT );
    _ser_addr_len = sizeof( _ser_addr );
    DEBUG_LOG_INFO( "Vo_Geth dst-ip/port:[%s:%d]\n", SERVER_IP, SERVER_PORT );
    g_img_queue_rgb2geth = Img_Queue_Init(IMG_QUEUE_SIZE_RGB2GETH);
    
    if (!g_img_queue_rgb2geth) {
        DEBUG_LOG_ERROR( "Creat rgb2geth queue failed!\n" );
        goto EXIT;
    }

    ret = 0;
    DEBUG_LOG_INFO("Vo_Geth_Init ok!\n");

EXIT:
    if( ret )
    {
        /* free if error */
        Vo_Geth_DeInit( );
    }
    return ret;
}



int
Vo_Geth_DeInit(
    void )
{
    if( _geth_fd >= 0 )
    {
        close( _geth_fd );
        _geth_fd = -1;
    }
    
    if( _frame_rgbdata )
    {
        if( _frame_rgbdata->buf )
            free( _frame_rgbdata->buf );

        free( _frame_rgbdata );
        _frame_rgbdata = NULL;
    }

    Img_Queue_Deinit(&g_img_queue_rgb2geth);
    return 0;
}

static int g_out_fps_geth = 0;
int Vo_Geth_queue(mp_image_t *img_rgb)
{
    int ret = -1;
    /* out-fps control */
    int             fps_us;
    static struct timeval time_out_last = {0,0};
    struct timeval  time_tmp;
    int             interval_us;

    if( g_out_fps_geth > 0 && (time_out_last.tv_sec + time_out_last.tv_usec) != 0 ) {
        gettimeofday( &time_tmp, 0 );
        interval_us = ( time_tmp.tv_sec - time_out_last.tv_sec ) * 1000000 +
            ( time_tmp.tv_usec - time_out_last.tv_usec );
        // refresh everytime because out-fps could be changed with IPC-command
        fps_us = ( 1000000 / g_out_fps_geth );
        if( interval_us < fps_us )
        {
            DEBUG_LOG_TRACE( "fps ctl:Time has not arrived:left[%d]us\n",
                fps_us - interval_us );
            goto EXIT;
        }
    }
    
    if (!g_img_queue_rgb2geth->is_full) {
        mp_image_t *img_bak = mp_image_new_copy(img_rgb);
        if (!img_bak) {
            DEBUG_LOG_ERROR( "cp new img to bak failed.\n" );
            goto EXIT;
        }
        if (Img_Queue_Send(g_img_queue_rgb2geth, img_bak)) {
            DEBUG_LOG_ERROR( "is full err!!!!.\n" );
            talloc_free(img_bak);
            goto EXIT;
        }
    } else {
        DEBUG_LOG_ERROR("Geth queue is Full!\n");
        goto EXIT;
    }
    /* put to send success */
    gettimeofday(&time_out_last, 0);
    ret = 0;
EXIT:
    return ret;
}


static bool g_geth_thread_state = GETH_THREAD_STATE_STOP;

void SetGethThreadState(bool state)
{
    g_geth_thread_state = state;
}
uint32_t GetGethOutFrameNum(void)
{
    return g_img_queue_rgb2geth ? g_img_queue_rgb2geth->count : 0;
}
void SetGethOutFps(uint32_t out_fps)
{
    g_out_fps_geth = out_fps;
}

void* _thread_Geth_out(void *arg )
{
    uint8_t*    src;
    int         out_w = 0;
    int         out_h = 0;

    struct timeval start,stop = {0,0};
    float ms;
    float mbytes;


    if( _geth_fd < 0 )
    {
        DEBUG_LOG_ERROR_LFPRE( "Vo_Geth is not inited.\n" );
        goto EXIT;
    }
    mp_image_t *img_rgb;
    while(g_geth_thread_state == GETH_THREAD_STATE_RUN) {
        img_rgb = (mp_image_t*)Img_Queue_Recv(g_img_queue_rgb2geth);
        if( NULL == img_rgb ) {
            usleep(1e4);
            continue;
        }
        
        if( img_rgb->num_planes != 1 )
            goto EXIT;

        out_w = img_rgb->w;
        out_h = img_rgb->h;
        src = img_rgb->planes[0];

    gettimeofday( &start, 0 );

        /* step-1 : send rgb-data-frame */
        _Vo_Geth_SendRGB( src, out_w * out_h );

        /* step-2 : send sync-frame */
        _Vo_Geth_SendSync( );

    gettimeofday( &stop, 0 );
        ms = (stop.tv_sec - start.tv_sec) * 1000.0 +
            ( stop.tv_usec - start.tv_usec ) / 1000.0;
        mbytes = 1.0 * out_w * out_h * 3 / 1000000;
        DEBUG_LOG_DEBUG_LFPRE("Vo_Geth_Draw [%.2f]MB, [%.2f]ms, [%.2f]Mbps\n",
            mbytes, ms, mbytes*8/(ms/1000) );
        /* send over , free img_bak */
        talloc_free(img_rgb);
    }

EXIT:
/* check: memery allocated units in queue should be free */
    while (img_rgb = (mp_image_t*)Img_Queue_Recv(g_img_queue_rgb2geth)) {
        talloc_free(img_rgb);
    }
    /* clear screen by send a blank frame */
    if (0 != (out_w + out_h)) {
        usleep(1e4);
        Vo_Geth_Draw_Blank(out_w, out_h);
    }
    return NULL;
}



int
Vo_Geth_Draw_Blank(
    int out_w,
    int out_h )
{
    int ret = -1;
    
struct timeval start,stop;
float ms;
float mbytes;


    if( _geth_fd < 0 )
    {
        DEBUG_LOG_ERROR_LFPRE( "Vo_Geth is not inited.\n" );
        goto EXIT;
    }

gettimeofday( &start, 0 );

    /* step-1 : send rgb-data-frame blank */
    _Vo_Geth_SendRGB_Blank( out_w * out_h );

    /* step-2 : send sync-frame */
    _Vo_Geth_SendSync( );

gettimeofday( &stop, 0 );
ms = (stop.tv_sec - start.tv_sec) * 1000.0 + ( stop.tv_usec - start.tv_usec ) / 1000.0;
mbytes = 1.0 * out_w * out_h * 3 / 1000000;
DEBUG_LOG_DEBUG_LFPRE( "Vo_Geth_Draw_Blank [%.2f]MB, [%.2f]ms, [%.2f]Mbps\n", mbytes, ms, mbytes*8/(ms/1000) );


    ret = 0;


EXIT:
    return ret;
}



/*******************************************************************************
* @brief    send rgb-data packets
* @note     
* @param    rgb_data    : [ro] rgb data
* @param    pixel_num   : [ro] num of pixels to send
* @retval   0           : success
* @retval   -1          : failed
*******************************************************************************/
static int
_Vo_Geth_SendRGB(
    uint8_t* rgb_data,
    uint32_t pixel_num )
{
    int         ret = -1;
    int         res;
    uint8_t*    sendbuf;
    int         sequence_id;
    int         remain_pix;
    uint8_t*    ptr;


    if( NULL == _frame_rgbdata )
        goto EXIT;

    if( false == ( rgb_data && pixel_num ) )
        goto EXIT;

    sendbuf = _frame_rgbdata->buf;
    sequence_id = 0;
    remain_pix = pixel_num;
    ptr = rgb_data;

    /* construct & send */
    sendbuf[0] = FUN_CODE_RGBDATA;
    while( remain_pix > 0 )
    {
        sendbuf[1] = ( uint8_t )( 0xFF & ( sequence_id >> 8 ) );
        sendbuf[2] = ( uint8_t )( 0xFF & ( sequence_id ) );

        if( remain_pix >= _frame_rgbdata->pixel_per_packet )
        {
            memcpy( sendbuf + 3, ptr, 3 * _frame_rgbdata->pixel_per_packet );
            ptr += 3 * _frame_rgbdata->pixel_per_packet;
            remain_pix -= _frame_rgbdata->pixel_per_packet;
        }
        else
        {
            memcpy( sendbuf + 3, ptr, remain_pix * 3 );
            remain_pix = 0;
        }
        sequence_id++;
#if SEND_TEST_MARCO
        res = sendto( _geth_fd, sendbuf, _frame_rgbdata->buf_len, 0,
            ( struct sockaddr* )&_ser_addr, _ser_addr_len );
        if( -1 == res )
        {
            DEBUG_LOG_ERROR_LFPRE( "_Vo_Geth_SendRGB sendto failed.\n" );
            goto EXIT;
        }
#endif
    }

    ret = 0;


EXIT:
    return ret;
}



/*******************************************************************************
* @brief    send rgb-data blank packets
* @note     
* @param    pixel_num   : [ro] num of pixels to send
* @retval   0           : success
* @retval   -1          : failed
*******************************************************************************/
static int
_Vo_Geth_SendRGB_Blank(
    uint32_t pixel_num )
{
    int         ret = -1;
    int         res;
    uint8_t*    sendbuf;
    int         sequence_id;
    int         remain_send;


    if( NULL == _frame_rgbdata )
        goto EXIT;

    if( 0 == pixel_num )
        goto EXIT;

    sendbuf = _frame_rgbdata->buf;
    sequence_id = 0;
    remain_send = pixel_num / _frame_rgbdata->pixel_per_packet;
    if( pixel_num % _frame_rgbdata->pixel_per_packet )
        remain_send++;

    /* construct & send */
    memset( sendbuf + 3, 0, 3 * _frame_rgbdata->pixel_per_packet );
    sendbuf[0] = FUN_CODE_RGBDATA;
    while( remain_send-- > 0 )
    {
        sendbuf[1] = ( uint8_t )( 0xFF & ( sequence_id >> 8 ) );
        sendbuf[2] = ( uint8_t )( 0xFF & ( sequence_id ) );

        sequence_id++;
#if SEND_TEST_MARCO
        res = sendto( _geth_fd, sendbuf, _frame_rgbdata->buf_len, 0,
            ( struct sockaddr* )&_ser_addr, _ser_addr_len );
        if( -1 == res )
        {
            DEBUG_LOG_ERROR_LFPRE( "_Vo_Geth_SendRGB_Blank sendto failed.\n" );
            goto EXIT;
        }
#endif
    }

    ret = 0;


EXIT:
    return ret;
}



/*******************************************************************************
* @brief    send sync packets
* @note     
* @retval   0           : success
* @retval   -1          : failed
*******************************************************************************/
static int
_Vo_Geth_SendSync(
    void )
{
    int             ret = -1;
    int             res;
    SyncFrame_t*    syncframe;
    uint8_t*        sendbuf;


    syncframe = _frame_sync + _frame_sync_idx;

    /* update gamma table if gamma changed */
    if( ( false == syncframe->is_calc ) ||
        ( fabs( syncframe->gamma - *_myopts_gamma_ptr[_frame_sync_idx] ) >= 0.000001 ) )
    {
        syncframe->gamma = *_myopts_gamma_ptr[_frame_sync_idx];
        syncframe->is_calc = true;
        _GammaTable_Calc_8to16( syncframe->buf + 3, syncframe->gamma );
    }

    /* step-1 : construct sync frame */
    sendbuf = syncframe->buf;
    sendbuf[0] = FUN_CODE_FRAMESYNC;        // func-id
    sendbuf[1] = _myopts_ptr->out_light;    // brightness
    sendbuf[2] = 0x01 << _frame_sync_idx;   // gamma_idx: r,g,b

    /* step-2 : send sync frame */
#if SEND_TEST_MARCO
    res = sendto( _geth_fd, sendbuf, DEFAULT_BYTES_UNIT, 0,
        ( struct sockaddr* )&_ser_addr, _ser_addr_len );
    if( -1 == res )
    {
        goto EXIT;
    }
#endif
    /* step-3 : gamma-idx update */
    _frame_sync_idx = ( _frame_sync_idx + 1 ) % 3;

    ret = 0;


EXIT:
    return ret;
}



/*******************************************************************************
* @brief    calc gamma table from 8bit to 16bit
* @note     3byte per level, 256 levels, 768byte total.
*           2bytes is valid data and 1byte for reserve.
*           1st byte is 0 for reserve, 2nd byte is high byte, 3rd byte is low byte.
*           It takes about 20~130 us
* @param    gamma_table : [wo] gamma table buf
* @param    gamma       : [ro] gamma param. 0.1-3.0
* @retval   0           : success
* @retval   -1          : failed
*******************************************************************************/
static int
_GammaTable_Calc_8to16(
    uint8_t*    gamma_table,
    float       gamma )
{
    int ret = -1;
    int input;
    int output;
    float tmp;
    float gamma_reciprocal;


    /* valid check */
    if( NULL == gamma_table )
    {
        goto EXIT;
    }

    if( gamma < 0.01 )
    {
        goto EXIT;
    }

    /* calc */
    gamma_reciprocal = 1.0 / gamma;
    for( input = 0; input < 256; input++ )
    {
        /* [0-255] -> [0.0-1.0] */
        tmp = input / 255.0F;

        /* Gamma conversion */
        tmp = ( float ) pow( tmp, gamma_reciprocal );

        /* [0.0-1.0] -> [0-65535] */
        output = ( int )( tmp * 65535.0 );

        *gamma_table++ = 0x00;
        *gamma_table++ = ( uint8_t ) 0xFF & ( output >> 8 );
        *gamma_table++ = ( uint8_t ) 0xFF & ( output );
    }

    ret = 0;


EXIT:
    return ret;
}




