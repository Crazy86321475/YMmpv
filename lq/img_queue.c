/**
********************************************************************************
* @file
* @author   Liu Qiang
* @version  0.1.0
* @since    2018.04.23
* @date     2018.06.23
* @brief    image queue
********************************************************************************
**/
#include <stdio.h>      // printf
#include <stdlib.h>     // malloc

#include "img_queue.h"

#include "lq/debug_vodmx.h"





Img_Queue_t*
Img_Queue_Init(
    uint32_t size )
{
    Img_Queue_t* p_queue = NULL;
    
    if( 0 == size )
        goto EXIT_ERR;

    p_queue = calloc( size, sizeof( Img_Queue_t ) );
    if( NULL == p_queue )
        goto EXIT_ERR;

    p_queue->pp_unit = calloc( size, sizeof( void* ) );
    if( NULL == p_queue->pp_unit )
        goto EXIT_ERR;

    p_queue->size = size;
    p_queue->pos_recv = 0;
    p_queue->pos_send = 0;
    p_queue->is_full = false;
    p_queue->count = 0;
    pthread_mutex_init( &p_queue->mutex, NULL );
    return p_queue;

EXIT_ERR:
    if( p_queue )
    {
        if( p_queue->pp_unit )
            free( p_queue->pp_unit );
        free( p_queue );
    }

    return NULL;
}


void
Img_Queue_Deinit(
    Img_Queue_t** pp_queue )
{
    Img_Queue_t* p_queue = NULL;

    if( NULL == pp_queue )
        goto EXIT;

    p_queue = *pp_queue;
    if( NULL == p_queue )
        goto EXIT;

    pthread_mutex_destroy( &p_queue->mutex );

    if( p_queue->pp_unit )
        free( p_queue->pp_unit );

    free( p_queue );
    *pp_queue = NULL;

EXIT:
    return;
}


bool
Img_Queue_Is_Full(
    Img_Queue_t* p_queue )
{
    if( NULL == p_queue )
        return true;

    return p_queue->is_full;
}


bool
Img_Queue_Is_Empty(
    Img_Queue_t* p_queue )
{
    if( NULL == p_queue->pp_unit )
        return true;

    return ( ( p_queue->pos_send == p_queue->pos_recv ) && ( false == p_queue->is_full ) );
}


int
Img_Queue_Send(
    Img_Queue_t*    p_queue,
    void*           p_unit )
{
    int ret = -1;
    int err;

    if( NULL == p_unit )
        goto EXIT;

    if( NULL == p_queue )
        goto EXIT;

    err = pthread_mutex_lock( &p_queue->mutex );
    if( err )
    {
        DEBUG_LOG_WARN_LFPRE( "Img_Queue_Send pthread_mutex_lock failed.\n" );
        goto EXIT;
    }

    if( Img_Queue_Is_Full( p_queue ) )
        goto EXIT_FREE;

    p_queue->count++;
    p_queue->pp_unit[ p_queue->pos_send ] = p_unit;

    p_queue->pos_send = ( p_queue->pos_send + 1 ) % p_queue->size;
    p_queue->is_full = ( p_queue->pos_send == p_queue->pos_recv );

    ret = 0;


EXIT_FREE:
    pthread_mutex_unlock( &p_queue->mutex );
    
EXIT:
    return ret;
}


void*
Img_Queue_Recv(
    Img_Queue_t* p_queue )
{
    void* p_unit = NULL;
    int err;

    if( NULL == p_queue )
        goto EXIT;

    err = pthread_mutex_lock( &p_queue->mutex );
    if( err )
    {
        DEBUG_LOG_WARN_LFPRE( "Img_Queue_Recv pthread_mutex_lock failed.\n" );
        goto EXIT;
    }

    if( Img_Queue_Is_Empty( p_queue ) )
        goto EXIT_FREE;

    // DEBUG_LOG_INFO( "get - [---%d].\n", p_queue->pos_recv );
    p_unit = p_queue->pp_unit[ p_queue->pos_recv ];
    p_queue->pp_unit[ p_queue->pos_recv ] = NULL;
    p_queue->pos_recv = ( p_queue->pos_recv + 1 ) % p_queue->size;
    p_queue->is_full = false;


EXIT_FREE:
    pthread_mutex_unlock( &p_queue->mutex );
    
EXIT:
    return p_unit;
}




