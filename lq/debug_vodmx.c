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
#include "debug_vodmx.h"

#include <stdarg.h>         // va_start





static LogLevel_Id_t    log_tag_work_max            = LOGLEVEL_ID_NONE;
static const char       log_tag[LOGLEVEL_ID_MAX]    = "EWI+=";





int
Debug_Log_SetLevel(
    LogLevel_Id_t   level )
{
    /* valid check */
    if( ( level >= LOGLEVEL_ID_MAX  ) ||
        ( level <  LOGLEVEL_ID_NONE ) )
    {
        return -1;
    }

    log_tag_work_max = level;
    return 0;
}


int
Debug_Log(
    LogLevel_Id_t   level,
    const char*     file_name,
    int             line,
    int             pre_lr,
    ... )
{
    va_list     args;
    char*       fmt;


    /* valid check */
    if( ( level >= LOGLEVEL_ID_MAX  ) ||
        ( level <  LOGLEVEL_ID_NONE ) )
    {
        return -1;
    }
    
    if( NULL == file_name )
    {
        return -1;
    }

    /* work level check */
    if( level > log_tag_work_max )
    {
        return 0;
    }

    /* step-1 : print prefix LFCR */
    if( pre_lr )
    {
        printf( "\r\n" );
    }

    /* step-2 : print fix-header */
    printf( "%c[%s:%4d] ", log_tag[level], file_name, line );
    // printf( "%c[%6d][%s:%4d] ", log_tag[level], OS_Time_Get( ), file_name, line );
    
    /* step-3 : print var-content */
    va_start( args, pre_lr );
    fmt = va_arg( args, char* );
    vprintf( fmt, args );
    va_end( args );
    
    // /* step-4 : print suffix LFCR */
    // printf( "\r\n" );
    
    
    return 0;
}




