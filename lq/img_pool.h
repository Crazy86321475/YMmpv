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
#ifndef __IMG_POOL_H__
#define __IMG_POOL_H__

#include <stdbool.h>            // bool
#include <stdint.h>             // uint32_t

#include "video/mp_image.h"     // mp_image_t
#include "video/img_format.h"   // mp_imgfmt





/*******************************************************************************
* @brief    state of image unit
* @note     
*******************************************************************************/
typedef enum
{
    POOL_UNIT_STATE_UNKNOW  = 0,    /**< not inited, cannot used */
    POOL_UNIT_STATE_FREE    = 1,    /**< image-unit is free */
    POOL_UNIT_STATE_USED    = 2     /**< image-unit is used/busy */
} Img_Unit_State_t;

/*******************************************************************************
* @brief    image unit
* @note     
*******************************************************************************/
typedef struct
{
    mp_image_t*         img;    /**< buf of image-unit */
    uint32_t            id;     /**< test. id for order */
    Img_Unit_State_t    state;  /**< state of image-unit */
} Img_Unit_t;

/*******************************************************************************
* @brief    info of image pool
* @note     
*******************************************************************************/
typedef struct
{
    int             w;
    int             h;
    enum mp_imgfmt  fmt;
} Img_Info_t;

/*******************************************************************************
* @brief    handle of image pool
* @note     
*******************************************************************************/
typedef struct
{
    uint32_t            size;       /**< size of pool */
    Img_Unit_t*         unit_list;  /**< list of image-unit */
    Img_Info_t          img_info;   /**< image params */
    pthread_mutex_t     mutex;      /**< lock */
} Img_Pool_t;





/*******************************************************************************
* @brief    init
* @note     
* @param    size        : [ro] pool size
* @param    fmt         : [ro] image param - format
* @param    width       : [ro] image param - width
* @param    height      : [ro] image param - heigh
* @return   [*]         : ptr of Img_Pool handle, NULL if init failed.
*******************************************************************************/
extern Img_Pool_t*
Img_Pool_Init(
    uint32_t        size,
    enum mp_imgfmt  fmt,
    uint32_t        width,
    uint32_t        height );

/*******************************************************************************
* @brief    deinit
* @note     
* @param    pp_pool     : [rw] Img_Pool handle
* @return   [void]
*******************************************************************************/
extern void
Img_Pool_Deinit(
    Img_Pool_t**    pp_pool );

/*******************************************************************************
* @brief    change image param of Img_Pool
* @note     it will result in realloc of image-unit in pool
* @param    p_pool      : [rw] Img_Pool handle
* @param    fmt         : [ro] image param - format
* @param    width       : [ro] image param - width
* @param    height      : [ro] image param - heigh
* @retval   0           : success
* @retval   -1          : failed
*******************************************************************************/
extern int
Img_Pool_UpdateParams(
    Img_Pool_t*     p_pool,
    enum mp_imgfmt  fmt,
    uint32_t        width,
    uint32_t        height );

/*******************************************************************************
* @brief    get a image-unit from pool
* @note     non-block
* @param    p_pool      : [rw] Img_Pool handle
* @return   [*]         : ptr of unit. NULL if get failed.
*******************************************************************************/
extern Img_Unit_t*
Img_Pool_Get(
    Img_Pool_t*     p_pool );

/*******************************************************************************
* @brief    put a image-unit to pool
* @note     non-block
* @param    p_pool      : [rw] Img_Pool handle
* @param    img_ctx     : [ro] ptr of unit
* @retval   0           : success
* @retval   -1          : failed
*******************************************************************************/
extern int
Img_Pool_Put(
    Img_Pool_t*     p_pool,
    Img_Unit_t*     img_ctx );





#endif