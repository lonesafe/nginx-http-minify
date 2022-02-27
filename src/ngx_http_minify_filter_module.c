
/*
 * Copyright (C) skysbird
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include "ngx_jsmin.h"
#include "ngx_cssmin.h"
#define NGX_HTTP_IMAGE_BUFFERED 0x08
typedef struct
{
    size_t length;
    u_int end;
} ngx_http_minify_filter_ctx_t;
typedef struct
{
    ngx_flag_t enable;
    ngx_hash_t types;
    ngx_array_t *types_keys;
} ngx_http_minify_conf_t;

static ngx_str_t ngx_http_minify_default_types[] = {
    ngx_string("application/x-javascript"),
    ngx_string("application/javascript"),
    ngx_string("text/javascript"),
    ngx_string("text/css"),
    ngx_null_string};

/**
 * 处理nginx.conf中的配置命令解析
 * 例如：
 * location / {
 *  	minify on;
 * }
 * 当用户请求:http://127.0.0.1/的时候，请求会跳转到minify这个配置上
 * minify的命令行解析回调函数：ngx_http_minify
 */
static ngx_command_t ngx_http_minify_filter_commands[] = {

    {ngx_string("minify"),
     NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_HTTP_LIF_CONF | NGX_CONF_FLAG,
     ngx_conf_set_flag_slot,
     NGX_HTTP_LOC_CONF_OFFSET,
     offsetof(ngx_http_minify_conf_t, enable),
     NULL},

    ngx_null_command};

static ngx_int_t ngx_http_minify_filter_init(ngx_conf_t *cf);
static void *ngx_http_minify_create_conf(ngx_conf_t *cf);
static char *ngx_http_minify_merge_conf(ngx_conf_t *cf, void *parent,
                                        void *child);
/**
 * 模块上下文
 */
static ngx_http_module_t ngx_http_minify_filter_module_ctx = {
    NULL,                        /* preconfiguration */
    ngx_http_minify_filter_init, /* postconfiguration */

    NULL, /* create main configuration */
    NULL, /* init main configuration */

    NULL, /* create server configuration */
    NULL, /* merge server configuration */

    ngx_http_minify_create_conf, /* create location configuration */
    ngx_http_minify_merge_conf   /* merge location configuration */
};

/**
 * 模块的定义
 */
ngx_module_t ngx_http_minify_filter_module = {
    NGX_MODULE_V1,
    &ngx_http_minify_filter_module_ctx, /* module context */
    ngx_http_minify_filter_commands,    /* module directives */
    NGX_HTTP_MODULE,                    /* module type */
    NULL,                               /* init master */
    NULL,                               /* init module */
    NULL,                               /* init process */
    NULL,                               /* init thread */
    NULL,                               /* exit thread */
    NULL,                               /* exit process */
    NULL,                               /* exit master */
    NGX_MODULE_V1_PADDING};

static ngx_http_output_header_filter_pt ngx_http_next_header_filter;
static ngx_http_output_body_filter_pt ngx_http_next_body_filter;
static ngx_int_t ngx_http_minify_buf_in_memory(ngx_buf_t *buf, ngx_http_request_t *r, ngx_http_minify_filter_ctx_t *ctx);

static ngx_int_t
ngx_http_minify_body_filter(ngx_http_request_t *r, ngx_chain_t *in)
{
    ngx_buf_t *b;
    ngx_chain_t *cl;
    ngx_http_minify_conf_t *conf;

    ngx_http_minify_filter_ctx_t *ctx;
    ctx = ngx_http_get_module_ctx(r, ngx_http_minify_filter_module);
    if (ctx == NULL)
    {
        return ngx_http_next_body_filter(r, in);
    }
    const char *header_content_type = (const char *)r->headers_out.content_type.data;
    if (!(strstr(header_content_type, (const char *)ngx_http_minify_default_types[0].data) != 0 ||
          strstr(header_content_type, (const char *)ngx_http_minify_default_types[1].data) != 0 ||
          strstr(header_content_type, (const char *)ngx_http_minify_default_types[2].data) != 0 ||
          strstr(header_content_type, (const char *)ngx_http_minify_default_types[3].data) != 0))
    {
        return ngx_http_next_body_filter(r, in);
    }

    conf = ngx_http_get_module_loc_conf(r, ngx_http_minify_filter_module);
    if (!conf->enable)
    {
        return ngx_http_next_body_filter(r, in);
    }
    ngx_log_error(3, r->connection->log, 0, "*************************b->last_buf-----------------%d", in->buf);
    for (cl = in; cl; cl = cl->next)
    {
        b = cl->buf;
        ngx_log_error(3, r->connection->log, 0, "$$$$$$$$$$$$$$$$$$$$$for last_in_chain-----------------%d", b->last_in_chain);
        if (b->last_buf)
        {
            ngx_log_error(3, r->connection->log, 0, "结束");
            ctx->end = 1;
        }
        if (b == NULL)
        {
            ngx_log_error(3, r->connection->log, 0, "####################b == NULL-----------------%d", b->last_buf);
            continue;
        }
        ctx->length += b->last - b->pos;
        ngx_http_minify_buf_in_memory(b, r, ctx);
        ngx_log_error(3, r->connection->log, 0, "---------------b->last_buf---------%d------%d", ctx->length, b->last_buf);
        
    }
    if (ctx->end)
    {
        r->headers_out.content_length_n = ctx->length;
        ngx_log_error(3, r->connection->log, 0, "文件长度%d-----%d", r->headers_out.content_length_n, r->headers_out.content_length_n);
        ngx_http_next_header_filter(r);
        return ngx_http_next_body_filter(r, in);
    }
    ngx_log_error(3, r->connection->log, 0, "等待下次执行%d", ctx->length);
    return NGX_AGAIN;
}

static ngx_int_t
ngx_http_minify_header_filter(ngx_http_request_t *r)
{

    ngx_http_minify_conf_t *conf = ngx_http_get_module_loc_conf(r, ngx_http_minify_filter_module);

    if (!conf->enable || (r->headers_out.status != NGX_HTTP_OK && r->headers_out.status != NGX_HTTP_FORBIDDEN && r->headers_out.status != NGX_HTTP_NOT_FOUND) || (r->headers_out.content_encoding && r->headers_out.content_encoding->value.len) || ngx_http_test_content_type(r, &conf->types) == NULL || r->header_only)
    {
        return ngx_http_next_header_filter(r);
    }
    // ngx_log_error(3, r->connection->log, 0, "获取文本长度：%d", r->headers_out.content_length_n);
    // ngx_http_clear_content_length(r);
    // ngx_log_error(3, r->connection->log, 0, "映射ctx");
    ngx_http_minify_filter_ctx_t *ctx = ngx_http_get_module_ctx(r, ngx_http_minify_filter_module);

    if (ctx == NULL)
    {
        ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_minify_filter_ctx_t));
        if (ctx == NULL)
        {
            return NGX_ERROR;
        }
    }
    ctx->end = 0;
    ngx_http_set_ctx(r, ctx, ngx_http_minify_filter_module);
    // r->headers_out.content_length_n = ctx->length;
    // ngx_log_error(3, r->connection->log, 0, "映射ctx完成：%d", r->headers_out.content_length_n);
    r->main_filter_need_in_memory = 1;
    r->allow_ranges = 0;
    return NGX_OK;
}

static ngx_int_t
ngx_http_minify_buf_in_memory(ngx_buf_t *buf, ngx_http_request_t *r, ngx_http_minify_filter_ctx_t *ctx)
{
    ngx_buf_t *b = NULL, *dst = NULL, *min_dst = NULL;
    ngx_int_t size;

    size = buf->end - buf->start;

    dst = buf;
    dst->end[0] = 0;

    b = ngx_calloc_buf(r->pool);
    if (b == NULL)
    {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    b->start = ngx_palloc(r->pool, size);
    b->pos = b->start;
    b->last = b->start;
    b->end = b->last + size;
    b->temporary = 1;

    min_dst = b;

    const char *header_content_type = (const char *)r->headers_out.content_type.data;

    if (strstr(header_content_type, (const char *)ngx_http_minify_default_types[0].data) != 0 ||
        strstr(header_content_type, (const char *)ngx_http_minify_default_types[1].data) != 0 ||
        strstr(header_content_type, (const char *)ngx_http_minify_default_types[2].data) != 0)
    {
        jsmin(dst, min_dst);
    }
    else if (strstr(header_content_type, (const char *)ngx_http_minify_default_types[3].data) != 0)
    {
        cssmin(dst, min_dst);
    }
    else
    {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    buf->start = min_dst->start;
    buf->pos = min_dst->pos;
    buf->last = min_dst->last;
    buf->end = min_dst->end;
    buf->memory = 0;
    buf->in_file = 0;
    return NGX_OK;
}

static void *
ngx_http_minify_create_conf(ngx_conf_t *cf)
{
    ngx_http_minify_conf_t *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_minify_conf_t));
    if (conf == NULL)
    {
        return NULL;
    }

    /*
     * set by ngx_pcalloc():
     *
     *     conf->bufs.num = 0;
     *     conf->types = { NULL };
     *     conf->types_keys = NULL;
     */

    conf->enable = NGX_CONF_UNSET;

    return conf;
}

static char *
ngx_http_minify_merge_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_minify_conf_t *prev = parent;
    ngx_http_minify_conf_t *conf = child;

    ngx_conf_merge_value(conf->enable, prev->enable, 0);

    if (ngx_http_merge_types(cf, &conf->types_keys, &conf->types,
                             &prev->types_keys, &prev->types,
                             ngx_http_minify_default_types) != NGX_OK)
    {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}

static ngx_int_t
ngx_http_minify_filter_init(ngx_conf_t *cf)
{
    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ngx_http_minify_header_filter;

    ngx_http_next_body_filter = ngx_http_top_body_filter;
    ngx_http_top_body_filter = ngx_http_minify_body_filter;

    return NGX_OK;
}
