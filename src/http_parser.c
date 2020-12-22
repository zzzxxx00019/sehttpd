#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "http.h"

/* constant-time string comparison */
#define cst_strcmp(m, c0, c1, c2, c3) \
    *(uint32_t *) m == ((c3 << 24) | (c2 << 16) | (c1 << 8) | c0)

#define CR '\r'
#define LF '\n'
#define CRLFCRLF "\r\n\r\n"


int http_parse_request_line(http_request_t *r)
{
    uint8_t ch, *p, *m;

    enum {
        num_start = 0,
        num_method,
        num_spaces_before_uri,
        num_after_slash_in_uri,
        num_http,
        num_http_H,
        num_http_HT,
        num_http_HTT,
        num_http_HTTP,
        num_first_major_digit,
        num_major_digit,
        num_first_minor_digit,
        num_minor_digit,
        num_spaces_after_digit,
        num_almost_done
    } state;

    state = r->state;

#define DISPATCH()                             \
    {                                          \
        pi++;                                  \
        if (pi >= r->last) {                   \
            goto END;                          \
        }                                      \
        p = (uint8_t *) &r->buf[pi % MAX_BUF]; \
        ch = *p;                               \
        goto *dispatch_table[state];           \
    }

    static const void *dispatch_table[] = {
        &&s_start,
        &&s_method,
        &&s_spaces_before_uri,
        &&s_after_slash_in_uri,
        &&s_http,
        &&s_http_H,
        &&s_http_HT,
        &&s_http_HTT,
        &&s_http_HTTP,
        &&s_first_major_digit,
        &&s_major_digit,
        &&s_first_minor_digit,
        &&s_minor_digit,
        &&s_spaces_after_digit,
        &&s_almost_done,
    };

    size_t pi = r->pos;
    if (pi >= r->last)
        goto END;
    p = (uint8_t *) &r->buf[pi % MAX_BUF];
    ch = *p;
    goto *dispatch_table[state];

s_start:
    r->request_start = p;

    if (ch == CR || ch == LF)
        DISPATCH();


    if ((ch < 'A' || ch > 'Z') && ch != '_')
        return HTTP_PARSER_INVALID_METHOD;

    state = num_method;
    DISPATCH();

s_method:
    if (ch == ' ') {
        m = r->request_start;

        switch (p - m) {
        case 3:
            if (cst_strcmp(m, 'G', 'E', 'T', ' ')) {
                r->method = HTTP_GET;
                break;
            }
            break;

        case 4:
            if (cst_strcmp(m, 'P', 'O', 'S', 'T')) {
                r->method = HTTP_POST;
                break;
            }
        default:
            r->method = HTTP_UNKNOWN;
            break;
        }
        state = num_spaces_before_uri;
        DISPATCH();
    }

    if ((ch < 'A' || ch > 'Z') && ch != '_')
        return HTTP_PARSER_INVALID_METHOD;
    DISPATCH();

s_spaces_before_uri:
    if (ch == '/') {
        r->uri_start = p;
        state = num_after_slash_in_uri;
        DISPATCH();
    }

    switch (ch) {
    case ' ':
        DISPATCH();
    default:
        return HTTP_PARSER_INVALID_REQUEST;
    }
    DISPATCH();
s_after_slash_in_uri:
    switch (ch) {
    case ' ':
        r->uri_end = p;
        state = num_http;
        break;
    default:
        break;
    }
    DISPATCH();

s_http:
    switch (ch) {
    case ' ':
        break;
    case 'H':
        state = num_http_H;
        break;
    default:
        return HTTP_PARSER_INVALID_REQUEST;
    }
    DISPATCH();

s_http_H:
    switch (ch) {
    case 'T':
        state = num_http_HT;
        break;
    default:
        return HTTP_PARSER_INVALID_REQUEST;
    }
    DISPATCH();
s_http_HT:
    switch (ch) {
    case 'T':
        state = num_http_HTT;
        break;
    default:
        return HTTP_PARSER_INVALID_REQUEST;
    }
    DISPATCH();

s_http_HTT:
    switch (ch) {
    case 'P':
        state = num_http_HTTP;
        break;
    default:
        return HTTP_PARSER_INVALID_REQUEST;
    }
    DISPATCH();
s_http_HTTP:
    switch (ch) {
    case '/':
        state = num_first_major_digit;
        break;
    default:
        return HTTP_PARSER_INVALID_REQUEST;
    }
    DISPATCH();
s_first_major_digit:
    if (ch < '1' || ch > '9')
        return HTTP_PARSER_INVALID_REQUEST;

    r->http_major = ch - '0';
    state = num_major_digit;
    DISPATCH();
s_major_digit:
    if (ch == '.') {
        state = num_first_minor_digit;
        DISPATCH();
    }

    if (ch < '0' || ch > '9')
        return HTTP_PARSER_INVALID_REQUEST;

    r->http_major = r->http_major * 10 + ch - '0';
    DISPATCH();

s_first_minor_digit:
    if (ch < '0' || ch > '9')
        return HTTP_PARSER_INVALID_REQUEST;

    r->http_minor = ch - '0';
    state = num_minor_digit;
    DISPATCH();
s_minor_digit:
    if (ch == CR) {
        state = num_almost_done;
        DISPATCH();
    }

    if (ch == LF)
        goto done;

    if (ch == ' ') {
        state = num_spaces_after_digit;
        DISPATCH();
    }

    if (ch < '0' || ch > '9')
        return HTTP_PARSER_INVALID_REQUEST;

    r->http_minor = r->http_minor * 10 + ch - '0';
    DISPATCH();
s_spaces_after_digit:
    switch (ch) {
    case ' ':
        DISPATCH();
    case CR:
        state = num_almost_done;
        DISPATCH();
    case LF:
        goto done;
    default:
        return HTTP_PARSER_INVALID_REQUEST;
    }
    DISPATCH();
s_almost_done:
    r->request_end = p - 1;
    switch (ch) {
    case LF:
        goto done;
    default:
        return HTTP_PARSER_INVALID_REQUEST;
    }
END:
    r->pos = pi;
    r->state = state;

    return EAGAIN;

done:
    r->pos = pi + 1;

    if (!r->request_end)
        r->request_end = p;

    r->state = num_start;

    return 0;
}


int http_parse_request_body(http_request_t *r)
{
    uint8_t ch, *p;
    enum {
        num_start = 0,
        num_key,
        num_spaces_before_colon,
        num_spaces_after_colon,
        num_value,
        num_cr,
        num_crlf,
        num_crlfcr
    } state;

    state = r->state;
    assert(state == 0 && "state should be 0");

    http_header_t *hd;

#define DISPATCH()                             \
    {                                          \
        pi++;                                  \
        if (pi >= r->last) {                   \
            goto END;                          \
        }                                      \
        p = (uint8_t *) &r->buf[pi % MAX_BUF]; \
        ch = *p;                               \
        goto *dispatch_table[state];           \
    }

    static const void *dispatch_table[] = {&&s_start,
                                           &&s_key,
                                           &&s_spaces_before_colon,
                                           &&s_spaces_after_colon,
                                           &&s_value,
                                           &&s_cr,
                                           &&s_crlf,
                                           &&s_crlfcr};

    size_t pi = r->pos;
    if (pi >= r->last)
        goto END;
    p = (uint8_t *) &r->buf[pi % MAX_BUF];
    ch = *p;
    goto *dispatch_table[state];

s_start:
    if (ch == CR || ch == LF)
        DISPATCH();

    r->cur_header_key_start = p;
    state = num_key;
    DISPATCH();

s_key:
    if (ch == ' ') {
        r->cur_header_key_end = p;
        state = num_spaces_before_colon;
        DISPATCH();
    }

    if (ch == ':') {
        r->cur_header_key_end = p;
        state = num_spaces_after_colon;
        DISPATCH();
    }
    DISPATCH();

s_spaces_before_colon:
    if (ch == ' ')
        DISPATCH();
    if (ch == ':') {
        state = num_spaces_after_colon;
        DISPATCH();
    }
    return HTTP_PARSER_INVALID_HEADER;

s_spaces_after_colon:
    if (ch == ' ')
        DISPATCH();

    state = num_value;
    r->cur_header_value_start = p;
    DISPATCH();
s_value:
    if (ch == CR) {
        r->cur_header_value_end = p;
        state = num_cr;
    }

    if (ch == LF) {
        r->cur_header_value_end = p;
        state = num_crlf;
    }
    DISPATCH();
s_cr:
    if (ch == LF) {
        state = num_crlf;
        /* save the current HTTP header */
        hd = malloc(sizeof(http_header_t));
        hd->key_start = r->cur_header_key_start;
        hd->key_end = r->cur_header_key_end;
        hd->value_start = r->cur_header_value_start;
        hd->value_end = r->cur_header_value_end;

        list_add(&(hd->list), &(r->list));
        DISPATCH();
    }
    return HTTP_PARSER_INVALID_HEADER;

s_crlf:
    if (ch == CR) {
        state = num_crlfcr;
    } else {
        r->cur_header_key_start = p;
        state = num_key;
    }
    DISPATCH();
s_crlfcr:
    switch (ch) {
    case LF:
        goto done;
    default:
        return HTTP_PARSER_INVALID_HEADER;
    }
    DISPATCH();
END:
    r->pos = pi;
    r->state = state;

    return EAGAIN;

done:
    r->pos = pi + 1;

    if (!r->request_end)
        r->request_end = p;

    r->state = num_start;

    return 0;
}
