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
#include "vo_dmx_fb.h"

#include <stdio.h>          // printf
// #include <sys/types.h>
// #include <string.h>
#include <stdbool.h>        // true/false

#include <fcntl.h>          // open; O_WRONLY,
#include <unistd.h>         // close
#include <sys/ioctl.h>      // ioctl

#include <sys/mman.h>       // mmap

#include <linux/fb.h>       // framebuffer

#include "video/mp_image.h" // mp_image_t

#include "lq/config_vodmx.h"
#include "lq/option_vodmx.h"
#include "lq/debug_vodmx.h"





#define MIN( a, b )     ( ((a)<(b)) ? (a) : (b) )





// // file-fd of framebuffer
// static int _fbx_fd = -1;

// width/heigth of framebuffer. equals screen width/heigth.
static int _fbx_w = 0;
static int _fbx_h = 0;

// mmap mem-ptr of framebuffer
static uint8_t* _fbx_mapmem = MAP_FAILED;

// dev-node path of framebuffer
static const char* _fbx_path_array[ VO_FB_ID_MAX ] =
{
    [ VO_FB_ID_0 ]  = "/dev/fb0",
    [ VO_FB_ID_1 ]  = "/dev/fb1",
};

// my-option
static Vodmx_Options_t* _myopts_ptr = NULL;





int
Vo_Fb_Init(
    Vo_Fb_ID_t          fb_id,
    Vodmx_Options_t*    myopts )
{
    struct fb_var_screeninfo    fb_var;
    int                         ret     = -1;
    int                         _fbx_fd = -1;
    // struct vo* vo = mpctx->video_out;
    // struct vo_x11_state* x11 = vo->x11;
    

    /* check init status */
    // do it if draw by mmap
    if( _fbx_mapmem != MAP_FAILED )
    {
        // inited already.
        ret = 0;
        goto EXIT;
    }
    // // do it if draw by read/write
    // if( _fbx_fd >= 0 )
    // {
    //     // inited already.
    //     ret = 0;
    //     goto EXIT;
    // }

    /* check param */
    if( fb_id < 0 || fb_id >= VO_FB_ID_MAX )
    {
        // param invalid.
        goto EXIT;
    }
    
    if( NULL == myopts )
    {
        // param invalid.
        goto EXIT;
    }

    /* set my-options */
    _myopts_ptr = myopts;

    /* open fb */
    _fbx_fd = open( _fbx_path_array[fb_id], O_RDWR, 0777 );
    if( _fbx_fd < 0 )
    {
        DEBUG_LOG_ERROR( "FB:[%s] open failed.\n", _fbx_path_array[fb_id] );
        goto EXIT;
    }
    DEBUG_LOG_INFO( "FB:[%s] open success.\n", _fbx_path_array[fb_id] );

    /* get screen size */
    ioctl ( _fbx_fd, FBIOGET_VSCREENINFO, &fb_var );
    _fbx_w = fb_var.xres;
    _fbx_h = fb_var.yres;
    // _fbx_w = vo->x11->ws_width;
    // _fbx_h = vo->x11->ws_height;
    // screen_bpp = fb_var.bits_per_pixel;
    DEBUG_LOG_INFO( "FB w/h:[%dx%d].\n", _fbx_w, _fbx_h );

    /* mmap */
    _fbx_mapmem = mmap(
        NULL,
        _fbx_w * _fbx_h * IMG_PIXEL_SIZE_OUT,
        PROT_READ|PROT_WRITE,
        MAP_SHARED,
        _fbx_fd,
        0 );
    if( _fbx_mapmem == MAP_FAILED )
    {
        DEBUG_LOG_ERROR( "Vo_Fb_Init mmap failed.\n" );
        goto EXIT;
    }
    DEBUG_LOG_INFO( "Vo_Fb_Init mmap success.\n" );

    ret = 0;


EXIT:
    // do it if draw by mmap
    if( _fbx_fd >= 0 )
    {
        close( _fbx_fd );
    }

    if( ret )
    {
        /* free if error */
        Vo_Fb_DeInit( );
    }

    return ret;
}



int
Vo_Fb_DeInit(
    void )
{
    // // do it if draw by read/write
    // if( _fbx_fd >= 0 )
    // {
    //     close( _fbx_fd );
    //     _fbx_fd = -1;
    // }

    // do it if draw by mmap
    if( _fbx_mapmem != MAP_FAILED )
    {
        munmap( _fbx_mapmem, _fbx_w * _fbx_h * IMG_PIXEL_SIZE_OUT );
        _fbx_mapmem = MAP_FAILED;
    }

    _myopts_ptr = NULL;


    return 0;
}



// do it if draw by mmap
int
Vo_Fb_Draw(
    mp_image_t* img_rgb )
{
    int ret = -1;
    uint8_t* src;
    int out_w;
    int out_h;
    int out_x;
    int out_y;


    if( MAP_FAILED == _fbx_mapmem )
    {
        DEBUG_LOG_ERROR( "Vo_FB is not inited.\n" );
        goto EXIT;
    }

    if( NULL == img_rgb )
        goto EXIT;

    if( img_rgb->num_planes != 1 )
        goto EXIT;

    /* get out size of rgb-image, cut content that is not in the view */
    /* param: image-w/h, screen-w/h, offset-x/y */
    out_x = _myopts_ptr->out_x;
    out_y = _myopts_ptr->out_y;
    if( out_x > _fbx_w || out_y > _fbx_h )
    {
        ret = -2;
        goto EXIT;
    }
    if (_myopts_ptr->HDMI_extend == OPTION_VODMX_HDMIOUT_MODE_NORMAL ||
        img_rgb->w > (_fbx_w>>1)) {
        out_w = MIN( _fbx_w, img_rgb->w );
        out_h = MIN( _fbx_h, img_rgb->h );
        if( out_x + out_w > _fbx_w )
        {
            // out_w = out_w - ( out_x + out_w - _fbx_w );
            out_w = _fbx_w - out_x;
        }
        if( out_y + out_h > _fbx_h )
        {
            // out_h = out_h - ( out_y + out_h - _fbx_h );
            out_h = _fbx_h - out_y;
        }

        /* get data of rgb-image */
        src = img_rgb->planes[0];

        /* out. rgb -> framebuffer */
        // DEBUG_LOG_INFO( "out_w/h=%d/%d,stride=%d, scr_w/h=%d/%d,stride=%d.\n",
        //     out_w, out_h, img_rgb->stride[0],
        //     _fbx_w, _fbx_h, 0 );
        for( int h = 0; h < out_h; h++ )
        {
            memcpy(
                _fbx_mapmem + IMG_PIXEL_SIZE_OUT * ( _fbx_w * ( h + out_y ) + out_x ),
                src,
                out_w * IMG_PIXEL_SIZE_OUT );
            src += img_rgb->stride[0];
        }
    }
    else if (_myopts_ptr->HDMI_extend == OPTION_VODMX_HDMIOUT_MODE_EXTEND) {
        //img_rgb->w <= _fbx_w  out_w = section_num_in_line * img_rgb->w;
        
        src = img_rgb->planes[0];
        int section_num_in_line = (_fbx_w - out_x) / img_rgb->w;
        if (section_num_in_line < 1) {
            goto EXIT;
        }
        out_w = section_num_in_line * img_rgb->w;
        out_h = MIN( _fbx_h, (img_rgb->h / section_num_in_line));
        uint8_t *dst = _fbx_mapmem;
        //printf("img_rgb->stride[0]:%d, section_num_in_line:%d\n",
            //img_rgb->stride[0], section_num_in_line);
        for( int h = 0; h < out_h; h++ )
        {
            dst = _fbx_mapmem + IMG_PIXEL_SIZE_OUT * ( _fbx_w * ( h + out_y ) + out_x );
            //printf("dst:%p\n", dst);
            for (int i = 0; i < section_num_in_line; i++) {
                memcpy(dst, src, img_rgb->w * IMG_PIXEL_SIZE_OUT );
                dst += (img_rgb->w * IMG_PIXEL_SIZE_OUT);
                src += img_rgb->stride[0];
            }
        }
    } else {
        goto EXIT;
    }
    ret = 0;


EXIT:
    return ret;
}

int Vo_Fb_Draw_Blank(void)
{
    int ret = -1;
    if( MAP_FAILED == _fbx_mapmem ) {
        DEBUG_LOG_ERROR( "Vo_FB is not inited.\n" );
        goto EXIT;
    }

    /* get out size of rgb-image, cut content that is not in the view */
    int out_w = _myopts_ptr->out_w;
    int out_h = _myopts_ptr->out_h;
    if (0 == (out_w + out_h))
        goto EXIT;
    /* param: image-w/h, screen-w/h, offset-x/y */
    int out_x = _myopts_ptr->out_x;
    int out_y = _myopts_ptr->out_y;
    if( out_x > _fbx_w || out_y > _fbx_h ) {
        ret = -2;
        goto EXIT;
    }
    if (_myopts_ptr->HDMI_extend == OPTION_VODMX_HDMIOUT_MODE_NORMAL ||
        out_w > (_fbx_w>>1)) {
        out_w = MIN( _fbx_w, out_w );
        out_h = MIN( _fbx_h, out_w );
        if( out_x + out_w > _fbx_w ) {
            // out_w = out_w - ( out_x + out_w - _fbx_w );
            out_w = _fbx_w - out_x;
        }
        if( out_y + out_h > _fbx_h ) {
            // out_h = out_h - ( out_y + out_h - _fbx_h );
            out_h = _fbx_h - out_y;
        }
        for( int h = 0; h < out_h; h++ ) {
            memset(
                _fbx_mapmem + IMG_PIXEL_SIZE_OUT * ( _fbx_w * ( h + out_y ) + out_x ),
                0,
                out_w * IMG_PIXEL_SIZE_OUT );
        }
    }
    else if (_myopts_ptr->HDMI_extend == OPTION_VODMX_HDMIOUT_MODE_EXTEND) {
        //img_rgb->w <= _fbx_w  out_w = section_num_in_line * img_rgb->w;
        int section_num_in_line = (_fbx_w - out_x) / out_w;
        if (section_num_in_line < 1) {
            goto EXIT;
        }
        out_w = section_num_in_line * out_w;
        out_h = MIN( _fbx_h, (out_h / section_num_in_line));
        uint8_t *dst = _fbx_mapmem;
        for( int h = 0; h < out_h; h++ ) {
            dst = _fbx_mapmem + IMG_PIXEL_SIZE_OUT * ( _fbx_w * ( h + out_y ) + out_x );
            for (int i = 0; i < section_num_in_line; i++) {
                memset(dst, 0, out_w * IMG_PIXEL_SIZE_OUT );
                dst += (out_w * IMG_PIXEL_SIZE_OUT);
            }
        }
    } else {
        goto EXIT;
    }
    ret = 0;
EXIT:
    return ret;
}


// // do it if draw by read/write
// int
// Vo_Fb_Draw(
//     mp_image_t* img_rgb )
// {
//     int ret = -1;
//     uint8_t* src;
//     int count;
//     int out_w;
//     int out_h;
//     int out_x;
//     int out_y;


//     if( _fbx_fd < 0 )
//     {
//         DEBUG_LOG_ERROR( "Vo_FB is not inited.\n" );
//         goto EXIT;
//     }

//     if( NULL == img_rgb )
//         goto EXIT;

//     if( img_rgb->num_planes != 1 )
//         goto EXIT;

//     /* get out size of rgb-image, cut content that is not in the view */
//     /* param: image-w/h, screen-w/h, offset-x/y */
//     out_x = _myopts_ptr->out_x;
//     out_y = _myopts_ptr->out_y;
//     if( out_x > _fbx_w || out_y > _fbx_h )
//     {
//         ret = -2;
//         goto EXIT;
//     }
//     out_w = MIN( _fbx_w, img_rgb->w );
//     out_h = MIN( _fbx_h, img_rgb->h );
//     if( out_x + out_w > _fbx_w )
//     {
//         // out_w = out_w - ( out_x + out_w - _fbx_w );
//         out_w = _fbx_w - out_x;
//     }
//     if( out_y + out_h > _fbx_h )
//     {
//         // out_h = out_h - ( out_y + out_h - _fbx_h );
//         out_h = _fbx_h - out_y;
//     }

//     /* get data of rgb-image */
//     src = img_rgb->planes[0];

//     /* out. rgb -> framebuffer */
//     // DEBUG_LOG_INFO( "out_w/h=%d/%d,stride=%d, scr_w/h=%d/%d,stride=%d.\n",
//     //     out_w, out_h, img_rgb->stride[0],
//     //     _fbx_w, _fbx_h, 0 );
//     count = 0;
//     lseek( _fbx_fd, 0, SEEK_SET );
//     for( int h = 0; h < out_h; h++ )
//     {
//         lseek( _fbx_fd,
//             IMG_PIXEL_SIZE_OUT * ( _fbx_w * ( h + out_y ) + out_x ),
//             SEEK_SET );
//         count = write( _fbx_fd, src, out_w * IMG_PIXEL_SIZE_OUT );
//         if( count < out_w * IMG_PIXEL_SIZE_OUT )
//         {
//             DEBUG_LOG_ERROR( "write fb0 failed.\n" );
//             goto EXIT;
//         }
//         src += img_rgb->stride[0];
//     }

//     ret = 0;


// EXIT:
//     return ret;
// }




