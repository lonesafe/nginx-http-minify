/*
 * Copyright (C) skysbird
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include "ngx_jsmin.h"
#include "ngx_cssmin.h"

typedef struct
{
    ngx_flag_t enable;
    ngx_hash_t types;
    ngx_array_t *types_keys;
} ngx_http_minify_conf_t;

typedef struct
{
    u_int all_end;
    u_char *body;
    ngx_buf_t *new_b;
    u_char *tempChar;
} ngx_http_minify_filter_ctx_t;

static ngx_str_t ngx_http_minify_default_types[] = {
    ngx_string("application/x-javascript"),
    ngx_string("application/javascript"),
    ngx_string("text/javascript"),
    ngx_string("text/css"),
    ngx_null_string};

static ngx_command_t ngx_http_minify_filter_commands[] = {

    {ngx_string("minify"),
     NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_HTTP_LIF_CONF | NGX_CONF_FLAG,
     ngx_conf_set_flag_slot,
     NGX_HTTP_LOC_CONF_OFFSET,
     offsetof(ngx_http_minify_conf_t, enable),
     NULL},

    {ngx_string("minify_types"),
     NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_1MORE,
     ngx_http_types_slot,
     NGX_HTTP_LOC_CONF_OFFSET,
     offsetof(ngx_http_minify_conf_t, types_keys),
     &ngx_http_minify_default_types[0]},

    ngx_null_command};

static ngx_int_t ngx_http_minify_filter_init(ngx_conf_t *cf);
static void *ngx_http_minify_create_conf(ngx_conf_t *cf);
static char *ngx_http_minify_merge_conf(ngx_conf_t *cf, void *parent, void *child);
static ngx_int_t ngx_http_minify_buf_in_memory(ngx_buf_t *buf, ngx_http_request_t *r, ngx_http_minify_filter_ctx_t *ctx);
static u_char *strAddstr(ngx_http_request_t *r, u_char *str1, u_char *str2, ngx_http_minify_filter_ctx_t *ctx);
static u_char *getChar(ngx_http_request_t *r, u_char *pos, u_char *last);

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
// static ngx_int_t ngx_http_minify_buf(ngx_buf_t *buf, ngx_http_request_t *r, ngx_open_file_info_t *of);

static ngx_int_t
ngx_http_minify_header_filter(ngx_http_request_t *r)
{
    ngx_http_minify_filter_ctx_t *ctx;
    ngx_http_minify_conf_t *conf;
    if (r->headers_out.status == NGX_HTTP_NOT_MODIFIED)
    {
        return ngx_http_next_header_filter(r);
    }

    ctx = ngx_http_get_module_ctx(r, ngx_http_minify_filter_module);

    if (ctx)
    {
        ngx_http_set_ctx(r, NULL, ngx_http_minify_filter_module);
        return ngx_http_next_header_filter(r);
    }

    conf = ngx_http_get_module_loc_conf(r, ngx_http_minify_filter_module);

    if (!conf->enable || (r->headers_out.status != NGX_HTTP_OK && r->headers_out.status != NGX_HTTP_FORBIDDEN && r->headers_out.status != NGX_HTTP_NOT_FOUND) || (r->headers_out.content_encoding && r->headers_out.content_encoding->value.len) || ngx_http_test_content_type(r, &conf->types) == NULL || r->header_only)
    {
        return ngx_http_next_header_filter(r);
    }
    ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_minify_filter_ctx_t));
    if (ctx == NULL)
    {
        return NGX_ERROR;
    }

    ngx_http_set_ctx(r, ctx, ngx_http_minify_filter_module);

    ngx_http_clear_content_length(r);
    return ngx_http_next_header_filter(r);
}

static ngx_int_t
ngx_http_minify_body_filter(ngx_http_request_t *r, ngx_chain_t *in)
{
    // ngx_int_t rc;
    // ngx_str_t *filename;
    // ngx_uint_t level;
    ngx_chain_t *cl;
    // ngx_open_file_info_t of;
    ngx_http_minify_conf_t *conf;
    // ngx_http_core_loc_conf_t *ccf;
    ngx_http_minify_filter_ctx_t *ctx;

    ctx = ngx_http_get_module_ctx(r, ngx_http_minify_filter_module);

    if (ctx == NULL)
    {
        return ngx_http_next_body_filter(r, in);
    }

    // ccf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    conf = ngx_http_get_module_loc_conf(r, ngx_http_minify_filter_module);
    if (!conf->enable)
    {
        return ngx_http_next_body_filter(r, in);
    }

    if (ngx_http_test_content_type(r, &conf->types) == NULL)
    {
        return ngx_http_next_body_filter(r, in);
    }

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http minify filter");
    ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0, "进来");
    for (cl = in; cl; cl = cl->next)
    {
        ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0, "是否是最后啦(%d)(%d)", cl->buf->last_buf, cl->buf->last_in_chain);
        //判断是否最后了
        ctx->all_end = cl->buf->last_buf;
        ctx->new_b = cl->buf;
        u_char *now_buffer = getChar(r, ctx->new_b->pos, ctx->new_b->last);
        ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0, "获取到newbuffer啦");
        ctx->body = strAddstr(r, ctx->body, now_buffer, ctx);
        ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0, "组合body啦");
        // b = cl->buf;
        // if (cl->buf->in_file)
        // {
        //     ngx_memzero(&of, sizeof(ngx_open_file_info_t));

        //     of.read_ahead = ccf->read_ahead;
        //     of.directio = ccf->directio;
        //     of.valid = ccf->open_file_cache_valid;
        //     of.min_uses = ccf->open_file_cache_min_uses;
        //     of.errors = ccf->open_file_cache_errors;
        //     of.events = ccf->open_file_cache_events;

        //     filename = &cl->buf->file->name;
        //     if (ngx_open_cached_file(ccf->open_file_cache, filename, &of, r->pool) != NGX_OK)
        //     {
        //         switch (of.err)
        //         {

        //         case 0:
        //             return NGX_HTTP_INTERNAL_SERVER_ERROR;

        //         case NGX_ENOENT:
        //         case NGX_ENOTDIR:
        //         case NGX_ENAMETOOLONG:

        //             level = NGX_LOG_ERR;
        //             rc = NGX_HTTP_NOT_FOUND;
        //             break;

        //         case NGX_EACCES:

        //             level = NGX_LOG_ERR;
        //             rc = NGX_HTTP_FORBIDDEN;
        //             break;

        //         default:

        //             level = NGX_LOG_CRIT;
        //             rc = NGX_HTTP_INTERNAL_SERVER_ERROR;
        //             break;
        //         }

        //         if (rc != NGX_HTTP_NOT_FOUND || ccf->log_not_found)
        //         {
        //             ngx_log_error(level, r->connection->log, of.err,
        //                           "%s \"%V\" failed", of.failed, filename);
        //         }

        //         return rc;
        //     }

        //     if (!of.is_file)
        //     {
        //         ngx_log_error(NGX_LOG_CRIT, r->connection->log, ngx_errno,
        //                       "\"%V\" is not a regular file", filename);
        //         return NGX_HTTP_NOT_FOUND;
        //     }

        //     if (of.size == 0)
        //     {
        //         continue;
        //     }

        //     ngx_http_minify_buf(b, r, &of);
        // }
        // else
        // {

        // if (b->pos == NULL)
        // {
        //     continue;
        // }

        // ngx_http_minify_buf_in_memory(b, r, ctx);
        // }
    }
    if (ctx->all_end)
    {
        ngx_buf_t *out = ngx_create_temp_buf(r->pool, ngx_strlen(ctx->body));
        out->pos = ctx->body;
        out->end = out->pos;
        out->last = out->pos + ngx_strlen(ctx->body);
        in->buf = out;
        in->next = NULL;
        ngx_http_minify_buf_in_memory(ctx->new_b, r, ctx);
        return ngx_http_next_body_filter(r, in);
    }
    else
    {
        return NGX_OK;
    }
}

/**
 * @brief 获取一段char
 *
 * @param pos
 * @param last
 * @return u_char*
 */
static u_char *getChar(ngx_http_request_t *r, u_char *pos, u_char *last)
{
    u_char *e = (u_char *)ngx_pcalloc(r->pool, 4096);
    ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0, "内存分配啦（%d）", ngx_strlen(e));

    int i = 0;
    for (;;)
    {
        // ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0, "循环开始啦getChar(%d)", pos - last);

        if (pos >= last)
        {
            ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0, "跳出了(%d)(%d)", *pos, *last);
            break;
        }
        // ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0, "还在循环(%d)(%d)", *pos, *last);
        u_char c = pos[0];
        // ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0, "获取char");
        ++pos;
        // ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0, "增加pos");
        e[i] = c;
        // ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0, "赋值给e");
        i++;
    }
    ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0, "信息返回啦");

    return e;
}

/**
 *  拼接两个char
 *
 */
static u_char *strAddstr(ngx_http_request_t *r, u_char *str1, u_char *str2, ngx_http_minify_filter_ctx_t *ctx)
{
    // ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0, "开始拼接啦(%d)(%d)", str1, str2);
    // char *str1_ = (char *)str1;
    // ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0, "转换1(%d)", str1_);
    // char *str2_ = (char *)str2;
    // ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0, "转换2(%d)", str2_);
    // int str1_len;
    // if (str1_ == 0)
    // {
    //     ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0, "唉呀str1 0");
    //     str1_len = 0;
    // }
    // else
    // {
    //     str1_len = strlen(str1_);
    //     ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0, "str1的长度是(%d)", str1_len);
    // }
    // int str2_len;
    // if (str2_ == 0)
    // {
    //     ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0, "唉呀str2 0");
    //     str2_len = 0;
    // }
    // else
    // {
    //     str2_len = strlen(str2_);
    //     ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0, "str2的长度是(%d)", str2_len);
    // }
    // ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0, "两个char都转换完了(%d)(%d)", str1_len, str2_len);
    // if (str2_len == 0 && str1_len == 0)
    // {
    //     return str1;
    // }
    int str1_len, str2_len;
    /* Find the current size of tokens1 and tokens 2 */
    for (str1_len = 0; (str1[str1_len] != '\0'); str1_len++)
        ;
    for (str2_len = 0; str2[str2_len] != '\0'; str2_len++)
        ;
    u_char *str3 = ngx_pcalloc(r->pool, 1 + str1_len + str2_len);
    // char str3[1 + str1_len + str2_len];
    ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0, "创建str3(%d)(%d)", ngx_strlen(str3), 1 + str1_len + str2_len);
    ngx_memcpy(str3, str1, str1_len * sizeof(u_char *));
    ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0, "第一个拷贝完了");
    ngx_memcpy(str3 + str1_len, str2, str2_len * sizeof(u_char *));
    // strcat(str1_, str2_);
    ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0, "拼接成功啦(%d)(%d)", str1_len, str2_len);
    str3[str1_len + str2_len] = '\0';
    ctx->tempChar = str3;
    return ctx->tempChar;
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
    buf->memory = 1;
    buf->in_file = 0;

    return NGX_OK;
}

// static ngx_int_t
// ngx_http_minify_buf(ngx_buf_t *buf, ngx_http_request_t *r,
//                     ngx_open_file_info_t *of)
// {
//     ngx_buf_t *b = NULL, *dst = NULL, *min_dst = NULL;
//     ngx_int_t size;
//     ngx_file_t *src_file;

//     src_file = ngx_pcalloc(r->pool, sizeof(ngx_file_t));
//     if (src_file == NULL)
//     {
//         return NGX_HTTP_INTERNAL_SERVER_ERROR;
//     }

//     src_file->fd = of->fd;
//     src_file->name = buf->file->name;
//     src_file->log = r->connection->log;
//     src_file->directio = of->is_directio;

//     size = of->size;

//     b = ngx_calloc_buf(r->pool);
//     if (b == NULL)
//     {
//         return NGX_HTTP_INTERNAL_SERVER_ERROR;
//     }

//     b->start = ngx_palloc(r->pool, size + 1);
//     b->pos = b->start;
//     b->last = b->start;
//     b->end = b->last + size;
//     b->temporary = 1;

//     dst = b;

// #if (NGX_HAVE_FILE_AIO)
//     ssize_t n;
//     ngx_output_chain_ctx_t *ctx;
//     ctx = ngx_http_get_module_ctx(r, ngx_http_minify_filter_module);
//     if (ctx->aio_handler)
//     {
//         n = ngx_file_aio_read(src_file, dst->pos, (size_t)size, 0,
//                               ctx->pool);

//         if (n == NGX_AGAIN)
//         {
//             ctx->aio_handler(ctx, src_file);
//             return NGX_AGAIN;
//         }
//     }
//     else
//     {
//         n = ngx_read_file(src_file, dst->pos, (size_t)size, 0);
//     }
// #else

//     ngx_read_file(src_file, dst->pos, (size_t)size, 0);
// #endif

//     dst->end[0] = 0;

//     b = ngx_calloc_buf(r->pool);
//     if (b == NULL)
//     {
//         return NGX_HTTP_INTERNAL_SERVER_ERROR;
//     }

//     b->start = ngx_palloc(r->pool, size);
//     b->pos = b->start;
//     b->last = b->start;
//     b->end = b->last + size;
//     b->temporary = 1;

//     min_dst = b;
//     const char *header_content_type = (const char *)r->headers_out.content_type.data;
//     if (strstr(header_content_type, (const char *)ngx_http_minify_default_types[0].data) != 0 ||
//         strstr(header_content_type, (const char *)ngx_http_minify_default_types[1].data) != 0 ||
//         strstr(header_content_type, (const char *)ngx_http_minify_default_types[2].data) != 0)
//     {
//         jsmin(dst, min_dst);
//     }
//     else if (strstr(header_content_type, (const char *)ngx_http_minify_default_types[3].data) != 0)
//     {
//         cssmin(dst, min_dst);
//     }
//     else
//     {
//         return NGX_HTTP_INTERNAL_SERVER_ERROR;
//     }

//     buf->start = min_dst->start;
//     buf->pos = min_dst->pos;
//     buf->last = min_dst->last;
//     buf->end = min_dst->end;
//     buf->memory = 1;
//     buf->in_file = 0;

//     return NGX_OK;
// }

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
