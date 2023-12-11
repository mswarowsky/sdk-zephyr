/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <mbedtls/platform_time.h>
#include <zephyr/kernel.h>

mbedtls_ms_time_t mbedtls_ms_time(void)
{
	return (mbedtls_ms_time_t)k_uptime_get();
}
