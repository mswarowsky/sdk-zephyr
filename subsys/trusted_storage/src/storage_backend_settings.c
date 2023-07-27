/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

#include "storage_backend.h"

LOG_MODULE_REGISTER(internal_trusted_storage_settings, CONFIG_PSA_TRUSTED_STORAGE_LOG_LEVEL);

/* UID as uint64_t in hexadecimal representation length */
#define TRUSTED_STORAGE_SETTINGS_BACKEND_FILENAME_UID_LENGTH (sizeof(uint64_t) * 2)

/* Storage pattern: prefix, uid low, uid high, suffix */
#define TRUSTED_STORAGE_SETTINGS_BACKEND_FILENAME_PATTERN "%s%08x%08x%s"

/* Max filename length aligned on Settings File backend max length */
#define TRUSTED_STORAGE_SETTINGS_BACKEND_FILENAME_MAX_LENGTH 32

struct load_object_info {
	uint8_t *data;
	size_t size;
	int ret;
};

/* Helper to fill filename with a suffix */
static psa_status_t create_filename(char *filename, const size_t filename_size, const char *prefix,
				    const psa_storage_uid_t uid, const char *suffix)
{
	int ret;

	ret = snprintf(filename, filename_size, TRUSTED_STORAGE_SETTINGS_BACKEND_FILENAME_PATTERN,
		       prefix, (unsigned int)((uid) >> 32), (unsigned int)((uid)&0xffffffff),
		       suffix);
	/* snprintf doc:
	 * Notice that only when this returned value is non-negative and less than n, the string has
	 * been completely written
	 */
	if (ret < 0 || ret >= filename_size) {
		return PSA_ERROR_STORAGE_FAILURE;
	}

	return PSA_SUCCESS;
}

/*
 * Reads the object content
 * if object size is larger only read the provided size
 * is object is smaller, return with error
 */
static int storage_settings_load_object(const char *key, size_t len, settings_read_cb read_cb,
					void *cb_arg, void *param)
{
	struct load_object_info *info = param;

	if (len < info->size) {
		info->ret = -EINVAL;
	} else {
		info->ret = read_cb(cb_arg, info->data, info->size);
	}

	/*
	 * This returned value isn't necessarily kept
	 * so also keep it in the load_object_info structure
	 */
	return info->ret;
}

static psa_status_t error_to_psa_error(int errorno)
{

	switch (errorno) {
	case 0:
		return PSA_SUCCESS;
	case -ENOENT:
		return PSA_ERROR_DOES_NOT_EXIST;
	case -ENODATA:
		return PSA_ERROR_DATA_CORRUPT;
	default:
		return PSA_ERROR_STORAGE_FAILURE;
	}
}

psa_status_t storage_get_object(const psa_storage_uid_t uid, const char *prefix, const char *suffix,
				uint8_t *object_data, const size_t object_size)
{
	char path[TRUSTED_STORAGE_SETTINGS_BACKEND_FILENAME_MAX_LENGTH + 1];
	struct load_object_info info;
	int ret;
	psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;

	if (object_size == 0 || object_data == NULL || prefix == NULL || suffix == NULL) {
		return PSA_ERROR_INVALID_ARGUMENT;
	}

	status = create_filename(path, TRUSTED_STORAGE_SETTINGS_BACKEND_FILENAME_MAX_LENGTH + 1,
				 prefix, uid, suffix);
	if (status != PSA_SUCCESS) {
		return status;
	}

	info.data = object_data;
	info.size = object_size;
	/* Set a fallback error if storage_settings_load_object isn't called */
	info.ret = -ENOENT;

	ret = settings_load_subtree_direct(path, storage_settings_load_object, &info);
	if (ret < 0) {
		return error_to_psa_error(ret);
	}

	if (info.ret < 0) {
		return error_to_psa_error(info.ret);
	}

	return PSA_SUCCESS;
}

psa_status_t storage_set_object(const psa_storage_uid_t uid, const char *prefix, char *suffix,
				const uint8_t *object_data, const size_t object_size)
{
	psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
	char path[TRUSTED_STORAGE_SETTINGS_BACKEND_FILENAME_MAX_LENGTH + 1];

	if (object_size == 0 || object_data == NULL || prefix == NULL || suffix == NULL) {
		return PSA_ERROR_INVALID_ARGUMENT;
	}

	status = create_filename(path, TRUSTED_STORAGE_SETTINGS_BACKEND_FILENAME_MAX_LENGTH + 1,
				 prefix, uid, suffix);
	if (status != PSA_SUCCESS) {
		return status;
	}

	return error_to_psa_error(settings_save_one(path, object_data, object_size));
}

psa_status_t storage_remove_object(const psa_storage_uid_t uid, const char *prefix,
				   const char *suffix)
{
	psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
	char path[TRUSTED_STORAGE_SETTINGS_BACKEND_FILENAME_MAX_LENGTH + 1];

	if (prefix == NULL || suffix == NULL) {
		return PSA_ERROR_INVALID_ARGUMENT;
	}

	status = create_filename(path, TRUSTED_STORAGE_SETTINGS_BACKEND_FILENAME_MAX_LENGTH + 1,
				 prefix, uid, suffix);
	if (status != PSA_SUCCESS) {
		return status;
	}
	return error_to_psa_error(settings_delete(path));
}

static int storage_settings_init(void)
{
	int ret;

	ret = settings_subsys_init();
	if (ret != 0) {
		LOG_ERR("%s failed (ret %d)", __func__, ret);
	}

	return ret;
}

SYS_INIT(storage_settings_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
