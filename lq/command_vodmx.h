/**
********************************************************************************
* @file
* @author   Liu Qiang
* @version  1.0.1
* @since    2019.04.08
* @date     2019.04.17
* @brief    command-my
********************************************************************************
**/
#ifndef __COMMAND_VODMX_H__
#define __COMMAND_VODMX_H__

#include "options/m_property.h" // m_property





#define VODMX_COMMAND_PROPERTIES                   \
    {"my-out_light",    vodmx_property_light},     \
    {"my-out_gamma_r",  vodmx_property_gamma_r},   \
    {"my-out_gamma_g",  vodmx_property_gamma_g},   \
    {"my-out_gamma_b",  vodmx_property_gamma_b},    \
    {"my-out_fps",      vodmx_property_out_fps},    \
    {"my-HDMI_extend",  vodmx_property_HDMI_extend},





/*******************************************************************************
* @brief    property set/get result from ipc command - out-light
* @param    ctx         : [ro] MPContext
* @param    prop        : [ro] ptr of property-obj
* @param    action      : [ro] cmd type
* @param    arg         : [rw] ptr of value
* @retval   M_PROPERTY_OK               : success
* @retval   M_PROPERTY_INVALID_FORMA    : data-format is invalid
* @retval   M_PROPERTY_NOT_IMPLEMENTED  : cmd is not support
*******************************************************************************/
extern int
vodmx_property_light(
    void*               ctx,
    struct m_property*  prop,
    int                 action,
    void*               arg );

/*******************************************************************************
* @brief    property set/get result from ipc command - out-gamma-red
* @param    ctx         : [ro] MPContext
* @param    prop        : [ro] ptr of property-obj
* @param    action      : [ro] cmd type
* @param    arg         : [rw] ptr of value
* @retval   M_PROPERTY_OK               : success
* @retval   M_PROPERTY_INVALID_FORMA    : data-format is invalid
* @retval   M_PROPERTY_NOT_IMPLEMENTED  : cmd is not support
*******************************************************************************/
extern int
vodmx_property_gamma_r(
    void*               ctx,
    struct m_property*  prop,
    int                 action,
    void*               arg );

/*******************************************************************************
* @brief    property set/get result from ipc command - out-gamma-green
* @param    ctx         : [ro] MPContext
* @param    prop        : [ro] ptr of property-obj
* @param    action      : [ro] cmd type
* @param    arg         : [rw] ptr of value
* @retval   M_PROPERTY_OK               : success
* @retval   M_PROPERTY_INVALID_FORMA    : data-format is invalid
* @retval   M_PROPERTY_NOT_IMPLEMENTED  : cmd is not support
*******************************************************************************/
extern int
vodmx_property_gamma_g(
    void*               ctx,
    struct m_property*  prop,
    int                 action,
    void*               arg );

/*******************************************************************************
* @brief    property set/get result from ipc command - out-gamma-blue
* @param    ctx         : [ro] MPContext
* @param    prop        : [ro] ptr of property-obj
* @param    action      : [ro] cmd type
* @param    arg         : [rw] ptr of value
* @retval   M_PROPERTY_OK               : success
* @retval   M_PROPERTY_INVALID_FORMA    : data-format is invalid
* @retval   M_PROPERTY_NOT_IMPLEMENTED  : cmd is not support
*******************************************************************************/
extern int
vodmx_property_gamma_b(
    void*               ctx,
    struct m_property*  prop,
    int                 action,
    void*               arg );

/*******************************************************************************
* @brief    property set/get result from ipc command - out-fps
* @param    ctx         : [ro] MPContext
* @param    prop        : [ro] ptr of property-obj
* @param    action      : [ro] cmd type
* @param    arg         : [rw] ptr of value
* @retval   M_PROPERTY_OK               : success
* @retval   M_PROPERTY_INVALID_FORMA    : data-format is invalid
* @retval   M_PROPERTY_NOT_IMPLEMENTED  : cmd is not support
*******************************************************************************/
extern int
vodmx_property_out_fps(
    void*               ctx,
    struct m_property*  prop,
    int                 action,
    void*               arg );

extern int
vodmx_property_HDMI_extend(
    void*               ctx,
    struct m_property*  prop,
    int                 action,
    void*               arg );

#endif