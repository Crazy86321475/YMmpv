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
#include "command_vodmx.h"

#include "options/m_property.h" // m_property
#include "player/core.h"        // MPContext

#include "option_vodmx.h"





int
vodmx_property_light(
    void*               ctx,
    struct m_property*  prop,
    int                 action,
    void*               arg )
{
    int             ret     = M_PROPERTY_OK;
    MPContext*      mpctx   = ctx;
    Vodmx_Options_t*   my_opts;


    my_opts = mpctx->opts->vodmx_opts;

    switch ( action )
    {
        case M_PROPERTY_SET:
        {
            int light = *( int* )arg;
            if( ( light >= OPTION_VODMX_LIGHT_MIN ) &&
                ( light <= OPTION_VODMX_LIGHT_MAX ) )
                my_opts->out_light = light;
            else
                ret = M_PROPERTY_INVALID_FORMAT;
            break;
        }

        case M_PROPERTY_GET:
            *( int* )arg = my_opts->out_light;
            break;

        case M_PROPERTY_GET_TYPE:
            *( struct m_option* )arg = ( struct m_option )
            {
                .type = CONF_TYPE_INT,
                .flags = CONF_RANGE,
                .min = OPTION_VODMX_LIGHT_MIN,
                .max = OPTION_VODMX_LIGHT_MAX,
            };
            break;

        default:
            ret = M_PROPERTY_NOT_IMPLEMENTED;
            break;
    }

    return ret;
}



int
vodmx_property_gamma_r(
    void*               ctx,
    struct m_property*  prop,
    int                 action,
    void*               arg )
{
    int             ret     = M_PROPERTY_OK;
    MPContext*      mpctx   = ctx;
    Vodmx_Options_t*   my_opts;


    my_opts = mpctx->opts->vodmx_opts;

    switch ( action )
    {
        case M_PROPERTY_SET:
        {
            float gamma_r = *( float* )arg;
            if( ( gamma_r >= OPTION_VODMX_GAMMA_MIN ) &&
                ( gamma_r <= OPTION_VODMX_GAMMA_MAX ) )
                my_opts->out_gamma_r = gamma_r;
            else
                ret = M_PROPERTY_INVALID_FORMAT;
            break;
        }

        case M_PROPERTY_GET:
            *( float* )arg = my_opts->out_gamma_r;
            break;

        case M_PROPERTY_GET_TYPE:
            *( struct m_option* )arg = ( struct m_option )
            {
                .type = CONF_TYPE_FLOAT,
                .flags = CONF_RANGE,
                .min = OPTION_VODMX_GAMMA_MIN,
                .max = OPTION_VODMX_GAMMA_MAX,
            };
            break;

        default:
            ret = M_PROPERTY_NOT_IMPLEMENTED;
            break;
    }

    return ret;
}



int
vodmx_property_gamma_g(
    void*               ctx,
    struct m_property*  prop,
    int                 action,
    void*               arg )
{
    int             ret     = M_PROPERTY_OK;
    MPContext*      mpctx   = ctx;
    Vodmx_Options_t*   my_opts;


    my_opts = mpctx->opts->vodmx_opts;

    switch ( action )
    {
        case M_PROPERTY_SET:
        {
            float gamma_g = *( float* )arg;
            if( ( gamma_g >= OPTION_VODMX_GAMMA_MIN ) &&
                ( gamma_g <= OPTION_VODMX_GAMMA_MAX ) )
                my_opts->out_gamma_g = gamma_g;
            else
                ret = M_PROPERTY_INVALID_FORMAT;
            break;
        }

        case M_PROPERTY_GET:
            *( float* )arg = my_opts->out_gamma_g;
            break;

        case M_PROPERTY_GET_TYPE:
            *( struct m_option* )arg = ( struct m_option )
            {
                .type = CONF_TYPE_FLOAT,
                .flags = CONF_RANGE,
                .min = OPTION_VODMX_GAMMA_MIN,
                .max = OPTION_VODMX_GAMMA_MAX,
            };
            break;

        default:
            ret = M_PROPERTY_NOT_IMPLEMENTED;
            break;
    }

    return ret;
}



int
vodmx_property_gamma_b(
    void*               ctx,
    struct m_property*  prop,
    int                 action,
    void*               arg )
{
    int             ret     = M_PROPERTY_OK;
    MPContext*      mpctx   = ctx;
    Vodmx_Options_t*   my_opts;


    my_opts = mpctx->opts->vodmx_opts;

    switch ( action )
    {
        case M_PROPERTY_SET:
        {
            float gamma_b = *( float* )arg;
            if( ( gamma_b >= OPTION_VODMX_GAMMA_MIN ) &&
                ( gamma_b <= OPTION_VODMX_GAMMA_MAX ) )
                my_opts->out_gamma_b = gamma_b;
            else
                ret = M_PROPERTY_INVALID_FORMAT;
            break;
        }

        case M_PROPERTY_GET:
            *( float* )arg = my_opts->out_gamma_b;
            break;

        case M_PROPERTY_GET_TYPE:
            *( struct m_option* )arg = ( struct m_option )
            {
                .type = CONF_TYPE_FLOAT,
                .flags = CONF_RANGE,
                .min = OPTION_VODMX_GAMMA_MIN,
                .max = OPTION_VODMX_GAMMA_MAX,
            };
            break;

        default:
            ret = M_PROPERTY_NOT_IMPLEMENTED;
            break;
    }

    return ret;
}

extern void SetGethOutFps(uint32_t out_fps);
int
vodmx_property_out_fps(
    void*               ctx,
    struct m_property*  prop,
    int                 action,
    void*               arg )
{
    int             ret     = M_PROPERTY_OK;
    MPContext*      mpctx   = ctx;
    Vodmx_Options_t*   my_opts;


    my_opts = mpctx->opts->vodmx_opts;

    switch ( action )
    {
        case M_PROPERTY_SET:
        {
            int fps = *( int* )arg;
            if( fps >= 0 ) {
                my_opts->out_fps = fps;
                SetGethOutFps(fps);
            } else
                ret = M_PROPERTY_INVALID_FORMAT;
            break;
        }

        case M_PROPERTY_GET:
            *( int* )arg = my_opts->out_fps;
            break;

        case M_PROPERTY_GET_TYPE:
            *( struct m_option* )arg = ( struct m_option )
            {
                .type = CONF_TYPE_INT,
            };
            break;

        default:
            ret = M_PROPERTY_NOT_IMPLEMENTED;
            break;
    }

    return ret;
}

int vodmx_property_HDMI_extend(
void *ctx, struct m_property *prop, int action, void *arg )
{
    int ret = M_PROPERTY_OK;
    MPContext*      mpctx   = ctx;
    Vodmx_Options_t*   my_opts;


    my_opts = mpctx->opts->vodmx_opts;

    switch ( action )
    {
    case M_PROPERTY_SET:
    {
        int tmp = *(int*)arg;
        if( ( tmp >= OPTION_VODMX_HDMIOUT_MODE_NORMAL ) &&
            ( tmp <= OPTION_VODMX_HDMIOUT_MODE_EXTEND ) )
            my_opts->HDMI_extend = tmp;
        else
            ret = M_PROPERTY_INVALID_FORMAT;
        break;
    }
    case M_PROPERTY_GET:
        *( int* )arg = my_opts->HDMI_extend;
        break;
    case M_PROPERTY_GET_TYPE:
        *( struct m_option* )arg = ( struct m_option )
        {
            .type = CONF_TYPE_INT,
            .flags = CONF_RANGE,
            .min = OPTION_VODMX_HDMIOUT_MODE_NORMAL,
            .max = OPTION_VODMX_HDMIOUT_MODE_EXTEND,
        };
        break;
    default:
        ret = M_PROPERTY_NOT_IMPLEMENTED;
        break;
    }
    return ret;
}

