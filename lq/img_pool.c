/**
********************************************************************************
* @file
* @author   Liu Qiang
* @version  0.1.2
* @since    2018.04.23
* @date     2019.04.20
* @brief    image pool
********************************************************************************
**/
#include "img_pool.h"

#include <pthread.h>    // pthread_xxx

#include "lq/debug_vodmx.h"





Img_Pool_t*
Img_Pool_Init(
    uint32_t        size,
    enum mp_imgfmt  fmt,
    uint32_t        width,
    uint32_t        height )
{
    Img_Pool_t* p_pool = NULL;
    mp_image_t* img_new;
    int index = 0;
    
    if( ( 0 == size ) && ( 0 == width ) && ( 0 == height ) )
        goto EXIT_ERR;

    p_pool = calloc( 1, sizeof( Img_Pool_t ) );
    if( NULL == p_pool )
        goto EXIT_ERR;

    p_pool->size            = size;
    p_pool->img_info.fmt    = fmt;
    p_pool->img_info.w      = width;
    p_pool->img_info.h      = height;

    p_pool->unit_list = calloc( size, sizeof( Img_Unit_t ) );
    if( NULL == p_pool->unit_list )
        goto EXIT_ERR;

    // yuv alloc
    while( index < size )
    {
        img_new = mp_image_alloc( fmt, width, height );
        if( NULL == img_new )
        {
            DEBUG_LOG_ERROR( "Img_Pool_Init mp_image_alloc failed.\n" );
            goto EXIT_ERR;
        }
        p_pool->unit_list[index].img = img_new;
        p_pool->unit_list[index].state = POOL_UNIT_STATE_FREE;
        index ++;
    }

    pthread_mutex_init( &p_pool->mutex, NULL );
    
    return p_pool;


EXIT_ERR:
    if( p_pool )
    {
        Img_Pool_Deinit( &p_pool );
        p_pool = NULL;
    }

    return NULL;
}



void
Img_Pool_Deinit(
    Img_Pool_t** pp_pool )
{
    Img_Pool_t* p_pool;
    int index = 0;

    if( NULL == pp_pool )
        goto EXIT;

    p_pool = *pp_pool;
    if( NULL == p_pool )
        goto EXIT;
    *pp_pool = NULL;

    pthread_mutex_destroy( &p_pool->mutex );

    if( p_pool->unit_list )
    {
        for( index = 0; index < p_pool->size; index++ )
        {
            if( p_pool->unit_list[index].img )
                talloc_free( p_pool->unit_list[index].img );
        }
        free( p_pool->unit_list );
    }

    free( p_pool );

EXIT:
    return;
}



/* set new params, do realloc when get unit by Img_Pool_Get() */
int
Img_Pool_UpdateParams(
    Img_Pool_t*     p_pool,
    enum mp_imgfmt  fmt,
    uint32_t        width,
    uint32_t        height )
{
    int ret = -1;
    int err;


    if( NULL == p_pool )
        goto EXIT;
    
    if( ( 0 == width ) && ( 0 == height ) )
        goto EXIT;

    err = pthread_mutex_lock( &p_pool->mutex );
    if( err )
    {
        DEBUG_LOG_WARN_LFPRE( "Img_Pool_UpdateParams pthread_mutex_lock failed.\n" );
        goto EXIT;
    }

    p_pool->img_info.fmt    = fmt;
    p_pool->img_info.w      = width;
    p_pool->img_info.h      = height;

    pthread_mutex_unlock( &p_pool->mutex );

    ret = 0;
    
EXIT:
    return ret;
}



Img_Unit_t*
Img_Pool_Get(
    Img_Pool_t* p_pool )
{
    Img_Unit_t*         p_unit = NULL;
    // Img_Unit_State_t*   p_state = NULL;
    int err;
    int index;

    if( NULL == p_pool )
        goto EXIT;

    err = pthread_mutex_lock( &p_pool->mutex );
    if( err )
    {
        DEBUG_LOG_WARN_LFPRE( "Img_Pool_Get pthread_mutex_lock failed.\n" );
        goto EXIT;
    }

    /* try to find a free unit */
    for( index = 0; index < p_pool->size; index++ )
    {
        if( POOL_UNIT_STATE_FREE == p_pool->unit_list[index].state )
        {
            p_pool->unit_list[index].state = POOL_UNIT_STATE_USED;
            p_unit = p_pool->unit_list + index;
            break;
        }
    }

    /* pool is busy, all unit is used */
    if( NULL == p_unit )
        goto EXIT_UNLOCK;

    /* last mp_image_alloc() is failed, try again */
    if( NULL == p_unit->img )
    {
        p_unit->img = mp_image_alloc(
            p_pool->img_info.fmt, p_pool->img_info.w, p_pool->img_info.h );
    }
    /* if image-param is changed, realloc pool unit */
    else if(
        ( p_pool->img_info.fmt != p_unit->img->imgfmt ) ||
        ( p_pool->img_info.w   != p_unit->img->w      ) ||
        ( p_pool->img_info.h   != p_unit->img->h      ) )
    {
        DEBUG_LOG_INFO_LFPRE( "Img_Pool_Get image-param change idx:[%d].\n", index );
        talloc_free( p_unit->img );
        p_unit->img = mp_image_alloc(
            p_pool->img_info.fmt, p_pool->img_info.w, p_pool->img_info.h );
    }

    /* if last mp_image_alloc is failed, return NULL */
    if( NULL == p_unit->img )
    {
        p_unit->state = POOL_UNIT_STATE_FREE;
        p_unit = NULL;
        DEBUG_LOG_ERROR( "Img_Pool_Get mp_image_alloc failed.\n" );
    }
    

EXIT_UNLOCK:
    pthread_mutex_unlock( &p_pool->mutex );

EXIT:
    return p_unit;
}



int
Img_Pool_Put(
    Img_Pool_t*     p_pool,
    Img_Unit_t*     p_unit )
{
    int ret = -1;
    int err;
    int index;

    if( NULL == p_unit )
        goto EXIT;
    
    if( NULL == p_pool )
        goto EXIT;

    err = pthread_mutex_lock( &p_pool->mutex );
    if( err )
    {
        DEBUG_LOG_WARN_LFPRE( "Img_Pool_Put pthread_mutex_lock failed.\n" );
        goto EXIT;
    }

    /* give back unit, turn USED to FREE */
    for( index = 0; index < p_pool->size; index++ )
    {
        if( ( p_unit == &p_pool->unit_list[index] ) &&
            ( POOL_UNIT_STATE_USED == p_pool->unit_list[index].state ) )
        {
            p_pool->unit_list[index].state = POOL_UNIT_STATE_FREE;
            ret = 0;
            break;
        }
    }

    pthread_mutex_unlock( &p_pool->mutex );

EXIT:
    return ret;
}




