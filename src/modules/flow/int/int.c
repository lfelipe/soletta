/*
 * This file is part of the Soletta Project
 *
 * Copyright (C) 2015 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "int-gen.h"

#include "sol-flow-internal.h"
#include "sol-types.h"
#include "sol-str-table.h"
#include "sol-mainloop.h"

#include <sol-util.h>
#include <limits.h>
#include <errno.h>

// =============================================================================
// IRANGE INC/DEC
// =============================================================================

struct accumulator_data {
    struct sol_irange val;
    int32_t init_val;
};

static int
accumulator_open(struct sol_flow_node *node, void *data, const struct sol_flow_node_options *options)
{
    int32_t tmp;
    struct accumulator_data *mdata = data;
    const struct sol_flow_node_type_int_accumulator_options *opts =
        (const struct sol_flow_node_type_int_accumulator_options *)options;

    SOL_FLOW_NODE_OPTIONS_SUB_API_CHECK
        (options, SOL_FLOW_NODE_TYPE_INT_ACCUMULATOR_OPTIONS_API_VERSION, -EINVAL);
    mdata->val = opts->setup_value;

    // Sanitizing options input
    if (mdata->val.max < mdata->val.min) {
        SOL_WRN("Max (%" PRId32 ") should be greater than Min (%" PRId32 "). Switching both values.",
            mdata->val.max, mdata->val.min);
        tmp = mdata->val.max;
        mdata->val.max = mdata->val.min;
        mdata->val.min = tmp;
    }

    if (mdata->val.val > mdata->val.max || mdata->val.val < mdata->val.min) {
        SOL_WRN("Value (%" PRId32 ") should be in %" PRId32 " - %" PRId32 " range,"
            " switching it to %" PRId32 "", mdata->val.val, mdata->val.min,
            mdata->val.max, mdata->val.min);
        mdata->val.val = mdata->val.min;
    }

    if (!mdata->val.step) {
        mdata->val.step = 1;
        SOL_WRN("Step can't be zero. Using (%" PRId32 ") instead.", mdata->val.step);
    } else if (mdata->val.step < 0) {
        mdata->val.step = -mdata->val.step;
        SOL_WRN("Step (-%" PRId32 ") can't be a negative value. Using (%" PRId32 ") instead.",
            mdata->val.step, mdata->val.step);
    }

    mdata->init_val = opts->setup_value.val;

    return sol_flow_send_irange_packet(node,
        SOL_FLOW_NODE_TYPE_INT_ACCUMULATOR__OUT__OUT,
        &mdata->val);
}

static int
inc_process(struct sol_flow_node *node, void *data, uint16_t port, uint16_t conn_id,
    const struct sol_flow_packet *packet)
{
    struct accumulator_data *mdata = data;

    mdata->val.val += mdata->val.step;
    if (mdata->val.val > mdata->val.max) {
        mdata->val.val = mdata->val.min;
        sol_flow_send_empty_packet(node, SOL_FLOW_NODE_TYPE_INT_ACCUMULATOR__OUT__OVERFLOW);
    }

    return sol_flow_send_irange_packet(node,
        SOL_FLOW_NODE_TYPE_INT_ACCUMULATOR__OUT__OUT,
        &mdata->val);
}

static int
dec_process(struct sol_flow_node *node, void *data, uint16_t port, uint16_t conn_id,
    const struct sol_flow_packet *packet)
{
    struct accumulator_data *mdata = data;

    mdata->val.val -= mdata->val.step;
    if (mdata->val.val < mdata->val.min) {
        mdata->val.val = mdata->val.max;
        sol_flow_send_empty_packet(node, SOL_FLOW_NODE_TYPE_INT_ACCUMULATOR__OUT__UNDERFLOW);
    }

    return sol_flow_send_irange_packet(node,
        SOL_FLOW_NODE_TYPE_INT_ACCUMULATOR__OUT__OUT,
        &mdata->val);
}

static int
reset_process(struct sol_flow_node *node, void *data, uint16_t port, uint16_t conn_id,
    const struct sol_flow_packet *packet)
{
    struct accumulator_data *mdata = data;

    mdata->val.val = mdata->init_val;

    return sol_flow_send_irange_packet(node,
        SOL_FLOW_NODE_TYPE_INT_ACCUMULATOR__OUT__OUT,
        &mdata->val);
}

// =============================================================================
// IRANGE IN RANGE
// =============================================================================

static int
inrange_open(struct sol_flow_node *node, void *data, const struct sol_flow_node_options *options)
{
    struct sol_irange *mdata = data;
    const struct sol_flow_node_type_int_inrange_options *opts;

    SOL_FLOW_NODE_OPTIONS_SUB_API_CHECK(options, SOL_FLOW_NODE_TYPE_INT_INRANGE_OPTIONS_API_VERSION, -EINVAL);

    opts = (const struct sol_flow_node_type_int_inrange_options *)options;
    if (opts->range.max >= opts->range.min) {
        mdata->min = opts->range.min;
        mdata->max = opts->range.max;
    } else {
        SOL_WRN("min (%" PRId32 ") should be smaller than max (%" PRId32 ").",
            opts->range.min, opts->range.max);
        mdata->min = opts->range.max;
        mdata->max = opts->range.min;
    }

    return 0;
}

static int
inrange_process(struct sol_flow_node *node, void *data, uint16_t port, uint16_t conn_id,
    const struct sol_flow_packet *packet)
{
    int r;
    int32_t value;
    struct sol_irange *mdata = data;

    r = sol_flow_packet_get_irange_value(packet, &value);
    SOL_INT_CHECK(r, < 0, r);

    return sol_flow_send_boolean_packet(node,
        SOL_FLOW_NODE_TYPE_INT_INRANGE__OUT__OUT,
        value >= mdata->min && value <= mdata->max);
}

// =============================================================================
// IRANGE MIN / MAX
// =============================================================================

struct irange_min_max_data {
    int val[2];
    int (*func) (int var0, int var1);
    uint16_t port;
    bool val_initialized[2];
};

static int
irange_val_isless(int var0, int var1)
{
    return var0 < var1;
}

static int
min_open(struct sol_flow_node *node, void *data, const struct sol_flow_node_options *options)
{
    struct irange_min_max_data *mdata = data;

    mdata->port = SOL_FLOW_NODE_TYPE_INT_MIN__OUT__OUT;
    mdata->func = irange_val_isless;

    return 0;
}

static int
irange_val_isgreater(int var0, int var1)
{
    return var0 > var1;
}

static int
max_open(struct sol_flow_node *node, void *data, const struct sol_flow_node_options *options)
{
    struct irange_min_max_data *mdata = data;

    mdata->port = SOL_FLOW_NODE_TYPE_INT_MAX__OUT__OUT;
    mdata->func = irange_val_isgreater;

    return 0;
}

static int
min_max_process(struct sol_flow_node *node, void *data, uint16_t port, uint16_t conn_id, const struct sol_flow_packet *packet)
{
    struct irange_min_max_data *mdata = data;
    int *result;
    int r;

    r = sol_flow_packet_get_irange_value(packet, &mdata->val[port]);
    SOL_INT_CHECK(r, < 0, r);

    mdata->val_initialized[port] = true;
    if (!(mdata->val_initialized[0] && mdata->val_initialized[1]))
        return 0;

    if (mdata->func(mdata->val[0], mdata->val[1]))
        result = &mdata->val[0];
    else
        result = &mdata->val[1];

    return sol_flow_send_irange_value_packet(node, mdata->port, *result);
}

// =============================================================================
// IRANGE ABS
// =============================================================================

static int
abs_process(struct sol_flow_node *node, void *data, uint16_t port, uint16_t conn_id, const struct sol_flow_packet *packet)
{
    int32_t value, result;
    int r;

    r = sol_flow_packet_get_irange_value(packet, &value);
    SOL_INT_CHECK(r, < 0, r);

    result = abs(value);

    return sol_flow_send_irange_value_packet(node,
        SOL_FLOW_NODE_TYPE_INT_ABS__OUT__OUT, result);
}

// =============================================================================
// IRANGE FILTER
// =============================================================================

struct int_filter_data {
    int max;
    int min;
    bool range_override;
};

static int
int_filter_open(struct sol_flow_node *node, void *data, const struct sol_flow_node_options *options)
{
    struct int_filter_data *mdata = data;
    const struct sol_flow_node_type_int_filter_options *opts;

    SOL_FLOW_NODE_OPTIONS_SUB_API_CHECK(options, SOL_FLOW_NODE_TYPE_INT_FILTER_OPTIONS_API_VERSION, -EINVAL);

    opts = (const struct sol_flow_node_type_int_filter_options *)options;
    if (opts->max.val >= opts->min.val) {
        mdata->min = opts->min.val;
        mdata->max = opts->max.val;
    } else {
        SOL_DBG("min (%" PRId32 ") should be smaller than max (%" PRId32 ").",
            opts->min.val, opts->max.val);
        mdata->min = opts->max.val;
        mdata->max = opts->min.val;
    }
    mdata->range_override = opts->range_override;

    return 0;
}

static int
int_filter_process(struct sol_flow_node *node, void *data, uint16_t port, uint16_t conn_id, const struct  sol_flow_packet *packet)
{
    struct sol_irange value;
    int r;
    struct int_filter_data *mdata = data;

    r = sol_flow_packet_get_irange(packet, &value);
    SOL_INT_CHECK(r, < 0, r);

    if (value.val >= mdata->min && value.val <= mdata->max ) {
        if (mdata->range_override) {
            value.min = mdata->min;
            value.max = mdata->max;
            value.step = 1;
        }
        return sol_flow_send_irange_packet(node, SOL_FLOW_NODE_TYPE_INT_FILTER__OUT__OUT, &value);
    }
    return 0;
}


// =============================================================================
// IRANGE ARITHMETIC
// =============================================================================


struct irange_arithmetic_data {
    struct sol_irange var0;
    struct sol_irange var1;
    int (*func) (const struct sol_irange *var0, const struct sol_irange *var1, struct sol_irange *result);
    uint16_t port;
    bool var0_initialized : 1;
    bool var1_initialized : 1;
};

static int
addition_open(struct sol_flow_node *node, void *data, const struct sol_flow_node_options *options)
{
    struct irange_arithmetic_data *mdata = data;

    mdata->port = SOL_FLOW_NODE_TYPE_INT_ADDITION__OUT__OUT;
    mdata->func = sol_irange_addition;

    return 0;
}

static int
division_open(struct sol_flow_node *node, void *data, const struct sol_flow_node_options *options)
{
    struct irange_arithmetic_data *mdata = data;

    mdata->port = SOL_FLOW_NODE_TYPE_INT_DIVISION__OUT__OUT;
    mdata->func = sol_irange_division;

    return 0;
}

static int
modulo_open(struct sol_flow_node *node, void *data, const struct sol_flow_node_options *options)
{
    struct irange_arithmetic_data *mdata = data;

    mdata->port = SOL_FLOW_NODE_TYPE_INT_MODULO__OUT__OUT;
    mdata->func = sol_irange_modulo;

    return 0;
}

static int
multiplication_open(struct sol_flow_node *node, void *data, const struct sol_flow_node_options *options)
{
    struct irange_arithmetic_data *mdata = data;

    mdata->port = SOL_FLOW_NODE_TYPE_INT_MULTIPLICATION__OUT__OUT;
    mdata->func = sol_irange_multiplication;

    return 0;
}

static int
subtraction_open(struct sol_flow_node *node, void *data, const struct sol_flow_node_options *options)
{
    struct irange_arithmetic_data *mdata = data;

    mdata->port = SOL_FLOW_NODE_TYPE_INT_SUBTRACTION__OUT__OUT;
    mdata->func = sol_irange_subtraction;

    return 0;
}

static int
operator_process(struct sol_flow_node *node, void *data, uint16_t port, uint16_t conn_id, const struct sol_flow_packet *packet)
{
    struct irange_arithmetic_data *mdata = data;
    struct sol_irange value;
    int r;

    r = sol_flow_packet_get_irange(packet, &value);
    SOL_INT_CHECK(r, < 0, r);

    if (port == 0) {
        if (mdata->var0_initialized &&
            sol_irange_equal(&mdata->var0, &value))
            return 0;
        mdata->var0 = value;
        mdata->var0_initialized = true;
    } else {
        if (mdata->var1_initialized &&
            sol_irange_equal(&mdata->var1, &value))
            return 0;
        mdata->var1 = value;
        mdata->var1_initialized = true;
    }

    if (!(mdata->var0_initialized && mdata->var1_initialized))
        return 0;

    r = mdata->func(&mdata->var0, &mdata->var1, &value);
    if (r < 0) {
        sol_flow_send_error_packet(node, -r, sol_util_strerrora(-r));
        return r;
    }

    return sol_flow_send_irange_packet(node, mdata->port, &value);
}

// =============================================================================
// IRANGE CONSTRAIN
// =============================================================================

struct irange_constrain_data {
    struct sol_irange val;
    bool use_input_range : 1;
};

static int
irange_constrain_open(struct sol_flow_node *node, void *data, const struct sol_flow_node_options *options)
{
    struct irange_constrain_data *mdata = data;
    const struct sol_flow_node_type_int_constrain_options *opts;

    opts = (const struct sol_flow_node_type_int_constrain_options *)options;
    mdata->val = opts->range;
    mdata->use_input_range = opts->use_input_range;

    return 0;
}

static void
irange_constrain(struct sol_irange *value)
{
    value->val -= (value->val - value->min) % value->step;
    if (value->val < value->min) value->val = value->min;
    if (value->val > value->max) value->val = value->max;
}

static int
irange_constrain_process(struct sol_flow_node *node, void *data, uint16_t port, uint16_t conn_id, const struct sol_flow_packet *packet)
{
    struct irange_constrain_data *mdata = data;
    int r;
    struct sol_irange value;

    r = sol_flow_packet_get_irange(packet, &value);
    SOL_INT_CHECK(r, < 0, r);

    if (!mdata->use_input_range) {
        value.min = mdata->val.min;
        value.max = mdata->val.max;
        value.step = mdata->val.step;
    }

    irange_constrain(&value);
    mdata->val = value;

    return sol_flow_send_irange_packet(node,
        SOL_FLOW_NODE_TYPE_INT_CONSTRAIN__OUT__OUT,
        &mdata->val);
}

// =============================================================================
// IRANGE BUFFER
// =============================================================================

struct irange_buffer_data {
    struct sol_flow_node *node;
    struct sol_timeout *timer;
    int32_t *input_queue;
    int32_t (*normalize_cb)(const int32_t *const values, size_t len);
    size_t cur_len;
    size_t n_samples;
    uint32_t timeout;
};

// =============================================================================
// Normalizing Functions
// =============================================================================

static int32_t
_normalize_mean(const int32_t *const values, const size_t len)
{
    int64_t sum = 0;

    for (size_t i = 0; i < len; i++)
        sum += values[i];

    return sum / len;
}

static int
int_cmp(const void *data1, const void *data2)
{
    const int32_t *i1 = data1;
    const int32_t *i2 = data2;

    return (*i1 > *i2) - (*i1 < *i2);
}

static int32_t
_normalize_median(const int32_t *const values, const size_t len)
{
    int32_t *copy_queue;
    int32_t ret;

    copy_queue = calloc(len, sizeof(*copy_queue));
    SOL_NULL_CHECK(copy_queue, 0);

    memcpy(copy_queue, values, len * sizeof(*copy_queue));

    qsort(copy_queue, len, sizeof(*copy_queue), int_cmp);

    if (len % 2)
        ret = copy_queue[len / 2];
    else
        ret = _normalize_mean(&copy_queue[len / 2 - 1], 2);

    free(copy_queue);

    return ret;
}

static const struct sol_str_table_ptr table[] = {
    SOL_STR_TABLE_PTR_ITEM("mean", _normalize_mean),
    SOL_STR_TABLE_PTR_ITEM("median", _normalize_median),
    { }
};

// =============================================================================

static int
_irange_buffer_do(struct irange_buffer_data *mdata)
{
    int32_t result;

    if (!mdata->cur_len)
        return 0;

    result = mdata->normalize_cb(mdata->input_queue, mdata->cur_len);

    return sol_flow_send_irange_value_packet(mdata->node,
        SOL_FLOW_NODE_TYPE_INT_BUFFER__OUT__OUT, result);
}

static bool
_timeout(void *data)
{
    struct irange_buffer_data *mdata = data;

    _irange_buffer_do(mdata);
    mdata->cur_len = 0;
    return true;
}

static void
_reset(void *data)
{
    struct irange_buffer_data *mdata = data;

    mdata->cur_len = 0;

    if (mdata->timer)
        sol_timeout_del(mdata->timer);
    if (mdata->timeout)
        mdata->timer = sol_timeout_add(mdata->timeout, _timeout, mdata);
}

static int
irange_buffer_reset(struct sol_flow_node *node, void *data, uint16_t port, uint16_t conn_id,
    const struct sol_flow_packet *packet)
{
    _reset(data);
    return 0;
}

static int
irange_buffer_timeout(struct sol_flow_node *node, void *data, uint16_t port, uint16_t conn_id,
    const struct sol_flow_packet *packet)
{
    int r;
    int32_t timeout;
    struct irange_buffer_data *mdata = data;

    r = sol_flow_packet_get_irange_value(packet, &timeout);
    SOL_INT_CHECK(r, < 0, r);

    if (timeout < 0)
        SOL_WRN("Invalid 'timeout' value: '%" PRId32 "'. Skipping it.", timeout);

    mdata->timeout = timeout;

    if (mdata->timer)
        sol_timeout_del(mdata->timer);
    if (mdata->timeout)
        mdata->timer = sol_timeout_add(mdata->timeout, _timeout, mdata);

    return r;
}

static int
irange_buffer_process(struct sol_flow_node *node, void *data, uint16_t port, uint16_t conn_id,
    const struct sol_flow_packet *packet)
{
    int r;
    struct irange_buffer_data *mdata = data;

    r = sol_flow_packet_get_irange_value(packet, &mdata->input_queue[mdata->cur_len]);
    SOL_INT_CHECK(r, < 0, r);

    if (mdata->n_samples <= ++mdata->cur_len) {
        r = _irange_buffer_do(mdata);
        _reset(data);
    }

    return r;
}

static int
irange_buffer_open(struct sol_flow_node *node, void *data,
    const struct sol_flow_node_options *options)
{
    struct irange_buffer_data *mdata = data;
    const struct sol_flow_node_type_int_buffer_options *opts =
        (const struct sol_flow_node_type_int_buffer_options *)options;

    mdata->node = node;
    mdata->cur_len = 0;

    SOL_FLOW_NODE_OPTIONS_SUB_API_CHECK
        (options, SOL_FLOW_NODE_TYPE_INT_BUFFER_OPTIONS_API_VERSION, -EINVAL);

    SOL_INT_CHECK(opts->timeout.val, < 0, -EINVAL);
    SOL_INT_CHECK(opts->samples.val, == 0, -EINVAL);

    mdata->n_samples = opts->samples.val;
    mdata->timeout = opts->timeout.val;
    mdata->normalize_cb = sol_str_table_ptr_lookup_fallback
                              (table, sol_str_slice_from_str(opts->operation), _normalize_mean);

    mdata->input_queue = calloc(mdata->n_samples, sizeof(*mdata->input_queue));
    SOL_NULL_CHECK(mdata->input_queue, -ENOMEM);

    if (mdata->timeout > 0)
        mdata->timer = sol_timeout_add(mdata->timeout, _timeout, mdata);

    return 0;
}

static void
irange_buffer_close(struct sol_flow_node *node, void *data)
{
    struct irange_buffer_data *mdata = data;

    if (mdata->timer) {
        sol_timeout_del(mdata->timer);
        mdata->timer = NULL;
    }

    free(mdata->input_queue);
}

// =============================================================================
// IRANGE BITWISE
// =============================================================================


struct bitwise_data {
    int in0;
    int in1;
    int result;
    bool in0_init : 1;
    bool in1_init : 1;
    bool sent_first : 1;
};

static int
two_port_process(struct sol_flow_node *node, void *data, uint16_t port_in, uint16_t port_out, const struct sol_flow_packet *packet, int (*func)(int, int))
{
    struct bitwise_data *mdata = data;
    int r;
    struct sol_irange in_value;
    struct sol_irange out_value = { .min = INT32_MIN, .step = 1, .max = INT32_MAX };

    r = sol_flow_packet_get_irange(packet, &in_value);
    SOL_INT_CHECK(r, < 0, r);

    if (port_in) {
        mdata->in1 = in_value.val;
        mdata->in1_init = true;
    } else {
        mdata->in0 = in_value.val;
        mdata->in0_init = true;
    }

    if (!(mdata->in0_init && mdata->in1_init))
        return 0;

    out_value.val = func(mdata->in0, mdata->in1);
    if (out_value.val == mdata->result && mdata->sent_first)
        return 0;

    mdata->result = out_value.val;
    mdata->sent_first = true;

    return sol_flow_send_irange_packet(node, port_out, &out_value);
}

static int
and_func(int in0, int in1)
{
    return in0 & in1;
}

static int
and_process(struct sol_flow_node *node, void *data, uint16_t port, uint16_t conn_id, const struct sol_flow_packet *packet)
{
    return two_port_process(node, data, port, SOL_FLOW_NODE_TYPE_INT_BITWISE_AND__OUT__OUT, packet, and_func);
}

static int
or_func(int in0, int in1)
{
    return in0 | in1;
}

static int
or_process(struct sol_flow_node *node, void *data, uint16_t port, uint16_t conn_id, const struct sol_flow_packet *packet)
{
    return two_port_process(node, data, port, SOL_FLOW_NODE_TYPE_INT_BITWISE_OR__OUT__OUT, packet, or_func);
}

static int
xor_func(int in0, int in1)
{
    return in0 ^ in1;
}

static int
xor_process(struct sol_flow_node *node, void *data, uint16_t port, uint16_t conn_id, const struct sol_flow_packet *packet)
{
    return two_port_process(node, data, port, SOL_FLOW_NODE_TYPE_INT_BITWISE_XOR__OUT__OUT, packet, xor_func);
}

static int
validate_shift(const struct sol_flow_packet *packet)
{
    struct sol_irange in;
    int r;

    r = sol_flow_packet_get_irange(packet, &in);
    SOL_INT_CHECK(r, < 0, r);
    if ((unsigned)in.val > (sizeof(in.val) * CHAR_BIT - 1) || in.val < 0)
        return -EINVAL;
    return 0;
}

static int
shift_left_func(int in0, int in1)
{
    return in0 << in1;
}

static int
shift_left_process(struct sol_flow_node *node, void *data, uint16_t port, uint16_t conn_id, const struct sol_flow_packet *packet)
{
    int r = 0;

    if (port == SOL_FLOW_NODE_TYPE_INT_SHIFT_LEFT__IN__SHIFT)
        r = validate_shift(packet);
    if (r < 0)
        return r;
    return two_port_process(node, data, port, SOL_FLOW_NODE_TYPE_INT_SHIFT_LEFT__OUT__OUT, packet, shift_left_func);
}

static int
shift_right_func(int in0, int in1)
{
    return (unsigned)in0 >> in1;
}

static int
shift_right_process(struct sol_flow_node *node, void *data, uint16_t port, uint16_t conn_id, const struct sol_flow_packet *packet)
{
    int r = 0;

    if (port == SOL_FLOW_NODE_TYPE_INT_SHIFT_RIGHT__IN__SHIFT)
        r = validate_shift(packet);
    if (r < 0)
        return r;
    return two_port_process(node, data, port, SOL_FLOW_NODE_TYPE_INT_SHIFT_RIGHT__OUT__OUT, packet, shift_right_func);
}

static int
not_process(struct sol_flow_node *node, void *data, uint16_t port, uint16_t conn_id, const struct sol_flow_packet *packet)
{
    int r;
    struct sol_irange in_value;
    struct sol_irange out_value = { .min = INT32_MIN, .step = 1, .max = INT32_MAX };

    r = sol_flow_packet_get_irange(packet, &in_value);
    SOL_INT_CHECK(r, < 0, r);

    out_value.val = ~in_value.val;
    return sol_flow_send_irange_packet(node, SOL_FLOW_NODE_TYPE_INT_BITWISE_NOT__OUT__OUT, &out_value);
}


// =============================================================================
// IRANGE COMPARISON
// =============================================================================

struct irange_comparison_data {
    bool (*func) (int32_t var0, int32_t var1);
    int32_t var0;
    int32_t var1;
    uint16_t port;
    bool var0_initialized : 1;
    bool var1_initialized : 1;
};

static bool
irange_val_equal(int32_t var0, int32_t var1)
{
    return var0 == var1;
}

static int
equal_open(struct sol_flow_node *node, void *data, const struct sol_flow_node_options *options)
{
    struct irange_comparison_data *mdata = data;

    mdata->port = SOL_FLOW_NODE_TYPE_INT_EQUAL__OUT__OUT;
    mdata->func = irange_val_equal;

    return 0;
}

static bool
irange_val_less(int32_t var0, int32_t var1)
{
    return var0 < var1;
}

static int
less_open(struct sol_flow_node *node, void *data, const struct sol_flow_node_options *options)
{
    struct irange_comparison_data *mdata = data;

    mdata->port = SOL_FLOW_NODE_TYPE_INT_LESS__OUT__OUT;
    mdata->func = irange_val_less;

    return 0;
}

static bool
irange_val_less_or_equal(int32_t var0, int32_t var1)
{
    return var0 <= var1;
}

static int
less_or_equal_open(struct sol_flow_node *node, void *data, const struct sol_flow_node_options *options)
{
    struct irange_comparison_data *mdata = data;

    mdata->port = SOL_FLOW_NODE_TYPE_INT_LESS_OR_EQUAL__OUT__OUT;
    mdata->func = irange_val_less_or_equal;

    return 0;
}

static bool
irange_val_greater(int32_t var0, int32_t var1)
{
    return var0 > var1;
}

static int
greater_open(struct sol_flow_node *node, void *data, const struct sol_flow_node_options *options)
{
    struct irange_comparison_data *mdata = data;

    mdata->port = SOL_FLOW_NODE_TYPE_INT_GREATER__OUT__OUT;
    mdata->func = irange_val_greater;

    return 0;
}

static bool
irange_val_greater_or_equal(int32_t var0, int32_t var1)
{
    return var0 >= var1;
}

static int
greater_or_equal_open(struct sol_flow_node *node, void *data, const struct sol_flow_node_options *options)
{
    struct irange_comparison_data *mdata = data;

    mdata->port = SOL_FLOW_NODE_TYPE_INT_EQUAL__OUT__OUT;
    mdata->func = irange_val_greater_or_equal;

    return 0;
}

static bool
irange_val_not_equal(int32_t var0, int32_t var1)
{
    return var0 != var1;
}

static int
not_equal_open(struct sol_flow_node *node, void *data, const struct sol_flow_node_options *options)
{
    struct irange_comparison_data *mdata = data;

    mdata->port = SOL_FLOW_NODE_TYPE_INT_NOT_EQUAL__OUT__OUT;
    mdata->func = irange_val_not_equal;

    return 0;
}

static int
comparison_process(struct sol_flow_node *node, void *data, uint16_t port, uint16_t conn_id, const struct sol_flow_packet *packet)
{
    struct irange_comparison_data *mdata = data;
    int r;
    struct sol_irange in_value;
    bool output;

    r = sol_flow_packet_get_irange(packet, &in_value);
    SOL_INT_CHECK(r, < 0, r);

    if (port == 0) {
        mdata->var0_initialized = true;
        mdata->var0 = in_value.val;
    } else if (port == 1) {
        mdata->var1_initialized = true;
        mdata->var1 = in_value.val;
    }

    /* only send something after both variables values are received */
    if (!(mdata->var0_initialized && mdata->var1_initialized))
        return 0;

    output = mdata->func(mdata->var0, mdata->var1);

    return sol_flow_send_boolean_packet(node, mdata->port, output);
}

// =============================================================================
// IRANGE MAP
// =============================================================================


struct irange_map_data {
    struct sol_irange input;
    struct sol_irange output;
    struct sol_irange output_value;
    bool use_input_range : 1;
};

static int
irange_map_open(struct sol_flow_node *node, void *data, const struct sol_flow_node_options *options)
{
    struct irange_map_data *mdata = data;
    const struct sol_flow_node_type_int_map_options *opts;

    SOL_FLOW_NODE_OPTIONS_SUB_API_CHECK(options, SOL_FLOW_NODE_TYPE_INT_MAP_OPTIONS_API_VERSION, -EINVAL);

    opts = (const struct sol_flow_node_type_int_map_options *)options;

    mdata->use_input_range = opts->use_input_range;

    mdata->input = opts->input_range;

    if (!mdata->use_input_range && (mdata->input.min >= mdata->input.max)) {
        SOL_WRN("Invalid range: input max must to be bigger than min");
        return -EINVAL;
    }

    mdata->output = opts->output_range;

    /* We allow output.min > output.max to invert range, but
     * when sending the packet, min and max must to be in correct
     * order */
    if (mdata->output.min < mdata->output.max) {
        mdata->output_value.min = mdata->output.min;
        mdata->output_value.max = mdata->output.max;
    } else {
        mdata->output_value.max = mdata->output.min;
        mdata->output_value.min = mdata->output.max;
    }

    if (opts->output_range.step < 1) {
        SOL_WRN("Output step need to be > 0");
        return -EDOM;
    }
    mdata->output_value.step = opts->output_range.step;

    return 0;
}

static int
_map(int64_t in_value, int64_t in_min, int64_t in_max, int64_t out_min, int64_t out_max, int64_t out_step, int32_t *out_value)
{
    int64_t result;

    if ((in_max - in_min) == out_min) {
        SOL_WRN("Input max - input min == output min");
        return -EDOM;
    }

    result = (in_value - in_min) * (out_max - out_min) /
             (in_max - in_min) + out_min;
    result -= (result - out_min) % out_step;
    *out_value = result;

    return 0;
}

static int
irange_map_process(struct sol_flow_node *node, void *data, uint16_t port, uint16_t conn_id, const struct sol_flow_packet *packet)
{
    struct irange_map_data *mdata = data;
    struct sol_irange in_value;
    int32_t out_value;
    int r;

    r = sol_flow_packet_get_irange(packet, &in_value);
    SOL_INT_CHECK(r, < 0, r);

    if (mdata->use_input_range) {
        if (in_value.min >= in_value.max) {
            SOL_WRN("Invalid range: input max must to be bigger than min");
            return -EINVAL;
        }
        r = _map(in_value.val, in_value.min, in_value.max, mdata->output.min,
            mdata->output.max, mdata->output_value.step, &out_value);
    } else
        r = _map(in_value.val, mdata->input.min, mdata->input.max,
            mdata->output.min, mdata->output.max, mdata->output_value.step,
            &out_value);

    SOL_INT_CHECK(r, < 0, r);

    mdata->output_value.val = out_value;

    return sol_flow_send_irange_packet(node,
        SOL_FLOW_NODE_TYPE_INT_MAP__OUT__OUT,
        &mdata->output_value);
}

#include "int-gen.c"
