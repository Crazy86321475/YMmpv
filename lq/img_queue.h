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
#ifndef __IMG_QUEUE_H__
#define __IMG_QUEUE_H__

#include <stdbool.h>    // bool
#include <stdint.h>     // uint32_t
#include <pthread.h>    // pthread_xxx





typedef struct
{
    void**              pp_unit;    /**< ptr of unit-array */
    uint32_t            size;       /**< size of queue */
    uint32_t            pos_recv;   /**< position of recv in queue */
    uint32_t            pos_send;   /**< position of send in queue */
    bool                is_full;    /**< flag of full */
    pthread_mutex_t     mutex;      /**< lock */
    uint32_t            count;
} Img_Queue_t;





/*******************************************************************************
* @brief    init
* @note     
* @param    size        : [ro] queue size
* @return   [*]         : ptr of Img_Queue handle
* @retval   NULL        : init failed.
*******************************************************************************/
extern Img_Queue_t*
Img_Queue_Init(
    uint32_t size );

/*******************************************************************************
* @brief    deinit
* @note     
* @param    pp_queue    : [rw] Img_Queue handle
* @return   [void]
*******************************************************************************/
extern void
Img_Queue_Deinit(
    Img_Queue_t** pp_queue );

/*******************************************************************************
* @brief    return full-status
* @note     
* @param    p_queue     : [ro] Img_Queue handle
* @retval   true        : full.
* @retval   false       : not full.
*******************************************************************************/
extern bool
Img_Queue_Is_Full(
    Img_Queue_t* p_queue );

/*******************************************************************************
* @brief    return empty-status
* @note     
* @param    p_queue     : [ro] Img_Queue handle
* @retval   true        : empty.
* @retval   false       : not empty.
*******************************************************************************/
extern bool
Img_Queue_Is_Empty(
    Img_Queue_t* p_queue );

/*******************************************************************************
* @brief    put to queue
* @note     non-block
* @param    p_queue     : [rw] Img_Queue handle
* @param    p_unit      : [ro] ptr of unit
* @retval   0           : success
* @retval   -1          : failed
*******************************************************************************/
extern int
Img_Queue_Send(
    Img_Queue_t*    p_queue,
    void*           p_unit );

/*******************************************************************************
* @brief    get from queue
* @note     non-block
* @param    p_queue     : [rw] Img_Queue handle
* @return   [*]         : ptr of unit. NULL if recv failed.
*******************************************************************************/
extern void*
Img_Queue_Recv(
    Img_Queue_t* p_queue );





#endif
