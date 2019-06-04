/**
********************************************************************************
* @file
* @author   Liu Qiang
* @version  0.2.1
* @since    2019.04.19
* @date     2019.04.22
* @brief    debug
********************************************************************************
**/
#ifndef __DEBUG_VODMX_H__
#define __DEBUG_VODMX_H__

#include <stdio.h>      // printf
#include <string.h>     // strrchr





typedef enum {
    LOGLEVEL_ID_NONE = -1,  /**< level -1 - NONE   nothing/quiet */

    LOGLEVEL_ID_ERROR,      /**< level  0 - ERROR  emergency/urgent */
    LOGLEVEL_ID_WARN,       /**< level  1 - WARN   critical/important */
    LOGLEVEL_ID_INFO,       /**< level  2 - INFO   notify/infomation */
    LOGLEVEL_ID_DEBUG,      /**< level  3 - DEBUG  debug */
    LOGLEVEL_ID_TRACE,      /**< level  4 - TRACE  redundancy/supplement */

    LOGLEVEL_ID_MAX
} LogLevel_Id_t;



// #define LOGLEVEL_NOTHING        LOGLEVEL_ID_NONE
// #define LOGLEVEL_EMERGENCY      LOGLEVEL_ID_ERROR
// #define LOGLEVEL_CRITICAL       LOGLEVEL_ID_WARN
// #define LOGLEVEL_NORMAL         LOGLEVEL_ID_INFO
// #define LOGLEVEL_NOTIFY         LOGLEVEL_ID_DEBUG
// #define LOGLEVEL_ALL            LOGLEVEL_ID_TRACE




#define SHORT_FILE strrchr( __FILE__, '/' ) ? strrchr( __FILE__, '/' ) + 1 : __FILE__


// #define DEBUG_LOG( ... )                Debug_Log( LOGLEVEL_ID_INFO,   SHORT_FILE, __LINE__, 0, __VA_ARGS__ )
#define DEBUG_LOG_ERROR( ... )          Debug_Log( LOGLEVEL_ID_ERROR,  SHORT_FILE, __LINE__, 0, __VA_ARGS__ )
#define DEBUG_LOG_WARN( ... )           Debug_Log( LOGLEVEL_ID_WARN,   SHORT_FILE, __LINE__, 0, __VA_ARGS__ )
#define DEBUG_LOG_INFO( ... )           Debug_Log( LOGLEVEL_ID_INFO,   SHORT_FILE, __LINE__, 0, __VA_ARGS__ )
#define DEBUG_LOG_DEBUG( ... )          Debug_Log( LOGLEVEL_ID_DEBUG,  SHORT_FILE, __LINE__, 0, __VA_ARGS__ )
#define DEBUG_LOG_TRACE( ... )          Debug_Log( LOGLEVEL_ID_TRACE,  SHORT_FILE, __LINE__, 0, __VA_ARGS__ )

// #define DEBUG_LOG_LFPRE( ... )          Debug_Log( LOGLEVEL_ID_INFO,   SHORT_FILE, __LINE__, 1, __VA_ARGS__ )
#define DEBUG_LOG_ERROR_LFPRE( ... )    Debug_Log( LOGLEVEL_ID_ERROR,  SHORT_FILE, __LINE__, 1, __VA_ARGS__ )
#define DEBUG_LOG_WARN_LFPRE( ... )     Debug_Log( LOGLEVEL_ID_WARN,   SHORT_FILE, __LINE__, 1, __VA_ARGS__ )
#define DEBUG_LOG_INFO_LFPRE( ... )     Debug_Log( LOGLEVEL_ID_INFO,   SHORT_FILE, __LINE__, 1, __VA_ARGS__ )
#define DEBUG_LOG_DEBUG_LFPRE( ... )    Debug_Log( LOGLEVEL_ID_DEBUG,  SHORT_FILE, __LINE__, 1, __VA_ARGS__ )
#define DEBUG_LOG_TRACE_LFPRE( ... )    Debug_Log( LOGLEVEL_ID_TRACE,  SHORT_FILE, __LINE__, 1, __VA_ARGS__ )
// #define DEBUG_LOG( ... )
// #define DEBUG_LOG_INFO( ... )
// #define DEBUG_LOG_WARN( ... )
// #define DEBUG_LOG_ERROR( ... )





/*******************************************************************************
* @brief    set debug-log level
* @param    level       : [ro] debug level
* @retval   0           : success
*******************************************************************************/
extern int
Debug_Log_SetLevel(
    LogLevel_Id_t   level );


/*******************************************************************************
* @brief    print debug log
* @param    level       : [ro] debug level
* @param    file_name   : [ro] file name
* @param    line        : [ro] line in file
* @param    pre_lr      : [ro] print prefix LR
* @retval   0           : success
* @retval   -1          : failed
*******************************************************************************/
extern int
Debug_Log(
    LogLevel_Id_t   level,
    const char*     file_name,
    int             line,
    int             pre_lr,
    ...
);





#endif