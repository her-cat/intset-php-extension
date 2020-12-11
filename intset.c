/*
  +----------------------------------------------------------------------+
  | PHP Version 7                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2017 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: her-cat <i@her-cat.com>                                      |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "ext/standard/info.h"
#include "php_intset.h"

/* True global resources - no need for thread safety here */
static int le_intset;

static uint8_t _intsetValueEncoding(int64_t v) {
    if (v < INT32_MIN || v > INT32_MAX)
        return INTSET_ENC_INT64;
    else if (v < INT16_MIN || v > INT16_MAX)
        return INTSET_ENC_INT32;
    else
        return INTSET_ENC_INT16;
}

static int64_t _intsetGetEncoded(intset *is, int pos, uint8_t enc) {
    int64_t v64;
    int32_t v32;
    int16_t v16;

    if (enc == INTSET_ENC_INT64) {
        memcpy(&v64, ((int64_t *)is->contents)+pos, sizeof(v64));
        return v64;
    } else if (enc == INTSET_ENC_INT32) {
        memcpy(&v32, ((int32_t *)is->contents)+pos, sizeof(v32));
        return v32;
    } else {
        memcpy(&v16, ((int16_t *)is->contents)+pos, sizeof(v16));
        return v16;
    }
}

static int64_t _intsetGet(intset *is, int pos) {
    return _intsetGetEncoded(is, pos, is->encoding);
}

static void _intsetSet(intset *is, int pos, int64_t value) {
    if (is->encoding == INTSET_ENC_INT64) {
        ((int64_t *)is->contents)[pos] = value;
    } else if (is->encoding == INTSET_ENC_INT32) {
        ((int32_t *)is->contents)[pos] = value;
    } else {
        ((int16_t *)is->contents)[pos] = value;
    }
}

static intset *intsetResize(intset *is, uint32_t len) {
    return erealloc(is, len * is->encoding);
}

/* 搜索 value 在 intset 中的位置。 */
static uint8_t intsetSearch(intset *is, uint64_t value, uint32_t *pos) {
    int min = 0, max = is->length - 1, mid = -1;
    int64_t cur = -1;

    /* intset 为空直接返回 0 */
    if (is->length == 0) {
        if (pos) *pos = 0;
        return 0;
    } else {
        /* value 大于 intset 中最大的值时，pos 为 inset 的长度，
         * value 小于 intset 中最小的值时，post 为 0，
         * 不管是大于还是小于，都需要对 intset 进行扩容。 */
        if (value > _intsetGet(is, max)) {
            if (pos) *pos = is->length;
            return 0;
        } else if (value < _intsetGet(is, 0)) {
            *pos = 0;
            return 0;
        }
    }

    /* 二分查找法确定 value 是否在 intset 中。 */
    while (max >= min) {
        /* 右移一位表示除以二。 */
        mid = ((unsigned int)min + (unsigned int)max) >> 1;
        /* 获取处于 mid 的值。 */
        cur = _intsetGet(is, mid);
        /* value > cur 时，表示 value 可能在右边值更大的区域，
         * value < cur 时，表示 value 可能在左边值更小的区域，
         * 其他情况说明找到了 value 所在的位置。 */
        if (value > cur) {
            min = mid + 1;
        } else if (value < cur) {
            max = mid - 1;
        } else {
            break;
        }
    }

    if (value == cur) {
        if (pos) *pos = mid;
        return 1;
    } else {
        if (pos) *pos = min;
        return 0;
    }
}

static intset *intsetUpgradeAndAdd(intset *is, int64_t value) {
    uint8_t curenc = is->encoding;
    uint8_t newenc = _intsetValueEncoding(value);
    int length = is->length;
    int prepend = value < 0 ? 1 : 0;

    /* 先设置新的编码和扩容。 */
    is->encoding = newenc;
    is = intsetResize(is, is->length + 1);

    /* 从后向前升级，这样不会覆盖旧值。
     * 注意，prepend 变量用于确保 intset 开头或结尾有一个空位，用来存储新值 */
    while (length--)
        _intsetSet(is, length + prepend, _intsetGetEncoded(is, length, curenc));

    /* 在 intset 开头或者末尾处设置新值。 */
    if (prepend)
        _intsetSet(is, 0, value);
    else
        _intsetSet(is, is->length, value);

    is->length = is->length + 1;
    return is;
}

static void intsetMoveTail(intset *is, uint32_t from, uint32_t to) {
    void *src, *dst;
    uint32_t bytes = is->length - from;
    uint32_t encoding = is->encoding;

    if (encoding == INTSET_ENC_INT64) {
        src = (int64_t *)is->contents + from;
        dst = (int64_t *)is->contents + to;
        bytes *= sizeof(int64_t);
    } else if (encoding == INTSET_ENC_INT32) {
        src = (int32_t *)is->contents + from;
        dst = (int32_t *)is->contents + to;
        bytes *= sizeof(int32_t);
    } else {
        src = (int16_t *)is->contents + from;
        dst = (int16_t *)is->contents + to;
        bytes *= sizeof(int16_t);
    }
    memmove(dst, src, bytes);
}

static void intsetDestroy(zend_resource *rsrc TSRMLS_DC){
    intset *is = (intset *) rsrc->ptr;
    if (is->length > 0) {
        efree(is->contents);
    }
    efree(is);
}

intset *intsetNew(void) {
    intset *is = emalloc(sizeof(intset));
    is->encoding = INTSET_ENC_INT16;
    is->length = 0;

    return is;
}

intset *intsetAdd(intset *is, int64_t value, uint8_t *success) {
    uint8_t valuec = _intsetValueEncoding(value);
    uint32_t pos;
    if (success) *success = 1;

    if (valuec > is->encoding) {
        return intsetUpgradeAndAdd(is, value);
    } else {
        if (intsetSearch(is, value, &pos)) {
            if (success) *success = 0;
            return is;
        }

        is = intsetResize(is, is->length + 1);
        if (pos < is->length) intsetMoveTail(is, pos, pos + 1);
    }

    _intsetSet(is, pos, value);
    is->length = is->length + 1;
    return is;
}

intset *intsetRemove(intset *is, int64_t value, uint8_t *success) {
    uint8_t valenc = _intsetValueEncoding(value);
    uint32_t pos;

    if (valenc <= is->encoding && intsetSearch(is, value, &pos)) {
        uint32_t len = is->length;

        if (success) *success = 1;

        if (pos < (len - 1)) intsetMoveTail(is, pos + 1, pos);
        is = intsetResize(is, len - 1);
        is->length = len - 1;
    }
    return is;
}

uint8_t intsetFind(intset *is, int64_t value) {
    uint8_t valenc = _intsetValueEncoding(value);
    return valenc <= is->encoding && intsetSearch(is, value, NULL);
}

int64_t intsetRandom(intset *is) {
    return _intsetGet(is, rand() % is->length);
}

uint8_t intsetGet(intset *is, uint32_t pos, int64_t *value) {
    if (pos < is->length) {
        *value = _intsetGet(is, pos);
        return 1;
    }
    return 0;
}

uint32_t intsetLen(const intset *is) {
    return is->length;
}

size_t intsetBlobLen(intset *is) {
    return sizeof(intset) + is->length * is->encoding;
}

/* {{{ proto resource create_intset()
    */
PHP_FUNCTION(intset_create)
{
    if (ZEND_NUM_ARGS() != 0) WRONG_PARAM_COUNT

    intset *is;

    is = intsetNew();
    if (!is) {
        RETURN_NULL()
    }

    RETURN_RES(zend_register_resource(is, le_intset))
}
/* }}} */

/* {{{ proto resource intset_add(resource is, long value)
    */
PHP_FUNCTION(intset_add)
{
    zend_long value;
    uint8_t success;
    zval *isp = NULL;
    intset *is = NULL;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rl", &isp, &value) == FAILURE)
        return;

    if ((is = zend_fetch_resource(Z_RES_P(isp), INTSET_TYPE, le_intset)) == NULL) {
        RETURN_FALSE
    }

    intsetAdd(is, value, &success);

    RETURN_BOOL(success)
}
/* }}} */

/* {{{ proto bool intset_remove(resource is, long value)
    */
PHP_FUNCTION(intset_remove)
{
    zend_long value;
    uint8_t success;
    zval *isp = NULL;
    intset *is = NULL;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rl", &isp, &value) == FAILURE)
        return;

    if ((is = zend_fetch_resource(Z_RES_P(isp), INTSET_TYPE, le_intset)) == NULL) {
        RETURN_FALSE
    }

    intsetRemove(is, value, &success);

    RETURN_BOOL(success)
}
/* }}} */

/* {{{ proto bool intset_find(resource is, long value)
    */
PHP_FUNCTION(intset_find)
{
    zend_long value;
    zval *isp = NULL;
    intset *is = NULL;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rl", &isp, &value) == FAILURE)
        return;

    if ((is = zend_fetch_resource(Z_RES_P(isp), INTSET_TYPE, le_intset)) == NULL) {
        RETURN_FALSE
    }

    RETURN_BOOL(intsetFind(is, value))
}
/* }}} */

/* {{{ proto long intset_random(resource is)
    */
PHP_FUNCTION(intset_random)
{
    zval *isp = NULL;
    intset *is = NULL;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &isp) == FAILURE)
        return;

    if ((is = zend_fetch_resource(Z_RES_P(isp), INTSET_TYPE, le_intset)) == NULL) {
        RETURN_FALSE
    }

    RETURN_LONG(intsetRandom(is))
}
/* }}} */

/* {{{ proto long intset_get(resource is, int pos)
    */
PHP_FUNCTION(intset_get)
{
    zend_long pos, value;
    zval *isp = NULL;
    intset *is = NULL;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rl", &isp, &pos) == FAILURE)
        return;

    if ((is = zend_fetch_resource(Z_RES_P(isp), INTSET_TYPE, le_intset)) == NULL) {
        RETURN_FALSE
    }

    if (intsetGet(is, pos, &value)) {
        RETURN_LONG(value)
    }

    RETURN_FALSE
}
/* }}} */

/* {{{ proto int intset_len(resource is)
    */
PHP_FUNCTION(intset_len)
{
    zval *isp = NULL;
    intset *is = NULL;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &isp) == FAILURE)
        return;

    if ((is = zend_fetch_resource(Z_RES_P(isp), INTSET_TYPE, le_intset)) == NULL) {
        RETURN_FALSE
    }

    RETURN_LONG(intsetLen(is))
}
/* }}} */

/* {{{ proto long intset_blob_len(resource is)
    */
PHP_FUNCTION(intset_blob_len)
{
    zval *isp = NULL;
    intset *is = NULL;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &isp) == FAILURE)
        return;

    if ((is = zend_fetch_resource(Z_RES_P(isp), INTSET_TYPE, le_intset)) == NULL) {
        RETURN_FALSE
    }

    RETURN_LONG(intsetBlobLen(is))
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(intset)
{
    le_intset = zend_register_list_destructors_ex(intsetDestroy, NULL, INTSET_TYPE, module_number);
    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(intset)
{
    return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(intset)
{
#if defined(COMPILE_DL_INTSET) && defined(ZTS)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif
    return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(intset)
{
    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(intset)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "intset support", "enabled");
    php_info_print_table_end();
}
/* }}} */

/* {{{ intset_functions[]
 *
 * Every user visible function must have an entry in intset_functions[].
 */
const zend_function_entry intset_functions[] = {
    PHP_FE(intset_create, NULL)
    PHP_FE(intset_add, NULL)
    PHP_FE(intset_remove, NULL)
    PHP_FE(intset_find, NULL)
    PHP_FE(intset_random, NULL)
    PHP_FE(intset_get, NULL)
    PHP_FE(intset_len, NULL)
    PHP_FE(intset_blob_len, NULL)
    PHP_FE_END    /* Must be the last line in intset_functions[] */
};
/* }}} */

/* {{{ intset_module_entry
 */
zend_module_entry intset_module_entry = {
    STANDARD_MODULE_HEADER,
    "intset",
    intset_functions,
    PHP_MINIT(intset),
    PHP_MSHUTDOWN(intset),
    PHP_RINIT(intset),        /* Replace with NULL if there's nothing to do at request start */
    PHP_RSHUTDOWN(intset),    /* Replace with NULL if there's nothing to do at request end */
    PHP_MINFO(intset),
    PHP_INTSET_VERSION,
    STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_INTSET
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(intset)
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
