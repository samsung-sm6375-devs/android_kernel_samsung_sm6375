/*
 * Synaptics TCM touchscreen driver
 *
 * Copyright (C) 2017-2018 Synaptics Incorporated. All rights reserved.
 *
 * Copyright (C) 2017-2018 Scott Lin <scott.lin@tw.synaptics.com>
 * Copyright (C) 2018-2019 Ian Su <ian.su@tw.synaptics.com>
 * Copyright (C) 2018-2019 Joey Zhou <joey.zhou@synaptics.com>
 * Copyright (C) 2018-2019 Yuehao Qiu <yuehao.qiu@synaptics.com>
 * Copyright (C) 2018-2019 Aaron Chen <aaron.chen@tw.synaptics.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * INFORMATION CONTAINED IN THIS DOCUMENT IS PROVIDED "AS-IS," AND SYNAPTICS
 * EXPRESSLY DISCLAIMS ALL EXPRESS AND IMPLIED WARRANTIES, INCLUDING ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE,
 * AND ANY WARRANTIES OF NON-INFRINGEMENT OF ANY INTELLECTUAL PROPERTY RIGHTS.
 * IN NO EVENT SHALL SYNAPTICS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, PUNITIVE, OR CONSEQUENTIAL DAMAGES ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OF THE INFORMATION CONTAINED IN THIS DOCUMENT, HOWEVER CAUSED
 * AND BASED ON ANY THEORY OF LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, AND EVEN IF SYNAPTICS WAS ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE. IF A TRIBUNAL OF COMPETENT JURISDICTION DOES
 * NOT PERMIT THE DISCLAIMER OF DIRECT DAMAGES OR ANY OTHER DAMAGES, SYNAPTICS'
 * TOTAL CUMULATIVE LIABILITY TO ANY PARTY SHALL NOT EXCEED ONE HUNDRED U.S.
 * DOLLARS.
 */

#include "ovt_tcm_sec_fn.h"

static struct testing_hcd *sec_fn_test;

static void sec_factory_print_frame(struct ovt_tcm_hcd *tcm_hcd, unsigned char *buf, 
	struct sec_factory_test_mode *mode)
{
	int i = 0;
	int j = 0;
	unsigned char *pStr = NULL;
	unsigned char pTmp[16] = { 0 };
	int lsize;
	short data;

	input_info(true, tcm_hcd->pdev->dev.parent, "%s\n", __func__);

	lsize = 7 * (tcm_hcd->cols + 1);

	pStr = kzalloc(lsize, GFP_KERNEL);
	if (pStr == NULL)
		return;

	memset(pStr, 0x0, lsize);
	snprintf(pTmp, sizeof(pTmp), "      TX");
	strlcat(pStr, pTmp, lsize);

	for (i = 0; i < tcm_hcd->cols; i++) {
		snprintf(pTmp, sizeof(pTmp), " %02d ", i);
		strlcat(pStr, pTmp, lsize);
	}

	input_raw_info(true, tcm_hcd->pdev->dev.parent, "%s\n", pStr);
	memset(pStr, 0x0, lsize);
	snprintf(pTmp, sizeof(pTmp), " +");
	strlcat(pStr, pTmp, lsize);

	for (i = 0; i < tcm_hcd->cols; i++) {
		snprintf(pTmp, sizeof(pTmp), "----");
		strlcat(pStr, pTmp, lsize);
	}

	input_raw_info(true, tcm_hcd->pdev->dev.parent, "%s\n", pStr);

	for (i = 0; i < tcm_hcd->rows; i++) {
		memset(pStr, 0x0, lsize);
		snprintf(pTmp, sizeof(pTmp), "Rx%02d | ", i);
		strlcat(pStr, pTmp, lsize);
		for (j = 0; j < tcm_hcd->cols; j++) {
			data = le2_to_uint(&buf[(j + (tcm_hcd->cols * i)) * 2]);
			snprintf(pTmp, sizeof(pTmp), " %5d", data);

			if (i == 0 && j == 0)
				mode->min = mode->max = data;

			mode->min = min(mode->min, data);
			mode->max = max(mode->max, data);

			strlcat(pStr, pTmp, lsize);
		}
		input_raw_info(true, tcm_hcd->pdev->dev.parent, "%s\n", pStr);
	}
	input_info(true, tcm_hcd->pdev->dev.parent, "%d %d\n", mode->min, mode->max);
	kfree(pStr);
}

int testing_run_prod_test_item(struct testing_hcd *sec_fn_test, enum test_code test_code)
{
	int retval;
	struct ovt_tcm_hcd *tcm_hcd = sec_fn_test->tcm_hcd;

	if (tcm_hcd->features.dual_firmware &&
			tcm_hcd->id_info.mode != MODE_PRODUCTIONTEST_FIRMWARE) {
		retval = tcm_hcd->switch_mode(tcm_hcd, FW_MODE_PRODUCTION_TEST);
		if (retval < 0) {
			input_err(true, tcm_hcd->pdev->dev.parent,
					"Failed to run production test firmware\n");
			return retval;
		}
	} else if (IS_NOT_FW_MODE(tcm_hcd->id_info.mode) ||	tcm_hcd->app_status != APP_STATUS_OK) {
		input_err(true, tcm_hcd->pdev->dev.parent,
				"Identifying mode = 0x%02x\n", tcm_hcd->id_info.mode);
		return -ENODEV;
	}

	LOCK_BUFFER(sec_fn_test->out);

	retval = ovt_tcm_alloc_mem(tcm_hcd, &sec_fn_test->out, 1);
	if (retval < 0) {
		input_err(true, tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for sec_fn_test->out.buf\n");
		UNLOCK_BUFFER(sec_fn_test->out);
		return retval;
	}

	sec_fn_test->out.buf[0] = test_code;

	LOCK_BUFFER(sec_fn_test->resp);

	retval = tcm_hcd->write_message(tcm_hcd, CMD_PRODUCTION_TEST, sec_fn_test->out.buf, 1,
				&sec_fn_test->resp.buf, &sec_fn_test->resp.buf_size,
				&sec_fn_test->resp.data_length, 	NULL, 0);
	if (retval < 0) {
		input_err(true, tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n", STR(CMD_PRODUCTION_TEST));
		UNLOCK_BUFFER(sec_fn_test->resp);
		UNLOCK_BUFFER(sec_fn_test->out);
		return retval;
	}

	UNLOCK_BUFFER(sec_fn_test->resp);
	UNLOCK_BUFFER(sec_fn_test->out);

	return 0;
}

int run_test(struct ovt_tcm_hcd *tcm_hcd, struct sec_factory_test_mode *mode, enum test_code test_code)
{
	int retval = 0;
	unsigned char *buf;
	unsigned int idx;

	char temp[SEC_CMD_STR_LEN] = { 0 };

	input_dbg(true, tcm_hcd->pdev->dev.parent,
			"Start testing\n");

	memset(tcm_hcd->print_buf, 0, tcm_hcd->rows * tcm_hcd->cols * CMD_RESULT_WORD_LEN);

	retval = testing_run_prod_test_item(sec_fn_test, test_code);
	if (retval < 0) {
		input_err(true, tcm_hcd->pdev->dev.parent,
				"Failed to run test\n");
		goto exit;
	}

	buf = sec_fn_test->resp.buf;

	sec_factory_print_frame(tcm_hcd, buf, mode);

	if (mode->allnode) {
		for (idx = 0; idx < tcm_hcd->rows * tcm_hcd->cols; idx++) {
			tcm_hcd->pFrame[idx] = (short)le2_to_uint(&buf[idx * 2]);
			snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", tcm_hcd->pFrame[idx]);
			strlcat(tcm_hcd->print_buf, temp, tcm_hcd->rows * tcm_hcd->cols * CMD_RESULT_WORD_LEN);

			memset(temp, 0x00, SEC_CMD_STR_LEN);			
		}
	} else {
		snprintf(tcm_hcd->print_buf, SEC_CMD_STR_LEN, "%d,%d", mode->min, mode->max);
	}

	return retval;

exit:
	return retval;
}


int test_sensitivity(void)
{
	int retval;
	unsigned char *buf;
	unsigned int idx;
	struct ovt_tcm_hcd *tcm_hcd = sec_fn_test->tcm_hcd;

	input_dbg(true, tcm_hcd->pdev->dev.parent, "Start testing\n");

	retval = testing_run_prod_test_item(sec_fn_test, TEST_SENSITIVITY);
	if (retval < 0) {
		input_err(true, tcm_hcd->pdev->dev.parent, "Failed to run test\n");
		goto exit;
	}

	LOCK_BUFFER(sec_fn_test->resp);

	buf = sec_fn_test->resp.buf;

	for (idx = 0; idx < SENSITIVITY_POINT_CNT; idx++) {
		tcm_hcd->pFrame[idx] = (short)le2_to_uint(&buf[idx * 2]);
	}

	UNLOCK_BUFFER(sec_fn_test->resp);

	retval = 0;

	return retval;

exit:
	return retval;
}

int raw_data_gap_x(struct sec_cmd_data *sec, struct sec_factory_test_mode *mode)
{
	int retval = 0;
	unsigned int idx;
	struct ovt_tcm_hcd *tcm_hcd = sec_fn_test->tcm_hcd;
	char *buff;
	char temp[SEC_CMD_STR_LEN] = { 0 };
	short node_gap = 0;

	input_dbg(true, tcm_hcd->pdev->dev.parent, "Start testing\n");

	buff = kzalloc(tcm_hcd->rows * tcm_hcd->cols * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff)
		goto error_buff_alloc_mem;

	for (idx = 0; idx < tcm_hcd->cols * tcm_hcd->rows; idx++) {
		if ((idx + tcm_hcd->cols) % (tcm_hcd->cols) != 0) {
			node_gap = ((tcm_hcd->pFrame[idx - 1] - tcm_hcd->pFrame[idx]) * 100) / tcm_hcd->pFrame[idx];

			snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", node_gap);
			strlcat(buff, temp, tcm_hcd->cols * tcm_hcd->rows * CMD_RESULT_WORD_LEN);
			memset(temp, 0x00, SEC_CMD_STR_LEN);
		}
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, tcm_hcd->cols * tcm_hcd->rows * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	retval = 0;
	kfree(buff);
error_buff_alloc_mem:
	return retval;
}

int raw_data_gap_y(struct sec_cmd_data *sec, struct sec_factory_test_mode *mode)
{
	int retval = 0;
	unsigned int idx;
	struct ovt_tcm_hcd *tcm_hcd = sec_fn_test->tcm_hcd;
	char *buff;
	char temp[SEC_CMD_STR_LEN] = { 0 };
	short node_gap = 0;

	input_dbg(true, tcm_hcd->pdev->dev.parent,
			"Start testing\n");

	buff = kzalloc(tcm_hcd->rows * tcm_hcd->cols * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff)
		goto error_buff_alloc_mem;

	for (idx = tcm_hcd->cols; idx < tcm_hcd->cols * tcm_hcd->rows; idx++) {
			node_gap = ((tcm_hcd->pFrame[idx - tcm_hcd->cols] - tcm_hcd->pFrame[idx]) * 100) / tcm_hcd->pFrame[idx];

			snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", node_gap);
			strlcat(buff, temp, tcm_hcd->cols * tcm_hcd->rows * CMD_RESULT_WORD_LEN);
			memset(temp, 0x00, SEC_CMD_STR_LEN);
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, tcm_hcd->cols * tcm_hcd->rows * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	retval = 0;
	kfree(buff);
error_buff_alloc_mem:
	return retval;
}

int test_abs_cap(struct sec_cmd_data *sec, struct sec_factory_test_mode *mode)
{
	int retval = 0;
	struct ovt_tcm_hcd *tcm_hcd = sec_fn_test->tcm_hcd;

	input_dbg(true, tcm_hcd->pdev->dev.parent, "Start testing\n");

	retval = run_test(tcm_hcd, mode, TEST_PT7_DYNAMIC_RANGE);
	if (retval < 0)
		goto exit;

	sec_cmd_set_cmd_result(sec, tcm_hcd->print_buf, strlen(tcm_hcd->print_buf));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		if (tcm_hcd->lcdoff_test) {
			sec_cmd_set_cmd_result_all(sec, tcm_hcd->print_buf, strnlen(tcm_hcd->print_buf, sizeof(tcm_hcd->print_buf)), "LP_ABS_CAP");
		} else {
			sec_cmd_set_cmd_result_all(sec, tcm_hcd->print_buf, strnlen(tcm_hcd->print_buf, sizeof(tcm_hcd->print_buf)), "ABS_CAP");
		}
	}
	sec->cmd_state = SEC_CMD_STATUS_OK;

	return retval;

exit:
	snprintf(tcm_hcd->print_buf, sizeof(tcm_hcd->print_buf), "NG");
	sec_cmd_set_cmd_result(sec, tcm_hcd->print_buf, strnlen(tcm_hcd->print_buf, sizeof(tcm_hcd->print_buf)));

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		if (tcm_hcd->lcdoff_test) {
			sec_cmd_set_cmd_result_all(sec, tcm_hcd->print_buf, strnlen(tcm_hcd->print_buf, sizeof(tcm_hcd->print_buf)), "LP_ABS_CAP");
		} else {
			sec_cmd_set_cmd_result_all(sec, tcm_hcd->print_buf, strnlen(tcm_hcd->print_buf, sizeof(tcm_hcd->print_buf)), "ABS_CAP");
		}
	}
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	return retval;
}

int test_noise(struct sec_cmd_data *sec, struct sec_factory_test_mode *mode)
{
	int retval = 0;
	struct ovt_tcm_hcd *tcm_hcd = sec_fn_test->tcm_hcd;

	input_dbg(true, tcm_hcd->pdev->dev.parent, "Start testing\n");

	retval = run_test(tcm_hcd, mode, TEST_PT10_DELTA_NOISE);
	if (retval < 0)
		goto exit;

	sec_cmd_set_cmd_result(sec, tcm_hcd->print_buf, strlen(tcm_hcd->print_buf));
	
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		if (tcm_hcd->lcdoff_test) {
			sec_cmd_set_cmd_result_all(sec, tcm_hcd->print_buf, strnlen(tcm_hcd->print_buf, sizeof(tcm_hcd->print_buf)), "LP_NOISE");
		} else {
			sec_cmd_set_cmd_result_all(sec, tcm_hcd->print_buf, strnlen(tcm_hcd->print_buf, sizeof(tcm_hcd->print_buf)), "NOISE");
		}
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;

	return retval;

exit:
	snprintf(tcm_hcd->print_buf, sizeof(tcm_hcd->print_buf), "NG");
	sec_cmd_set_cmd_result(sec, tcm_hcd->print_buf, strnlen(tcm_hcd->print_buf, sizeof(tcm_hcd->print_buf)));

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		if (tcm_hcd->lcdoff_test) {
			sec_cmd_set_cmd_result_all(sec, tcm_hcd->print_buf, strnlen(tcm_hcd->print_buf, sizeof(tcm_hcd->print_buf)), "LP_NOISE");
		} else {
			sec_cmd_set_cmd_result_all(sec, tcm_hcd->print_buf, strnlen(tcm_hcd->print_buf, sizeof(tcm_hcd->print_buf)), "NOISE");
		}
	}
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	return retval;
}

int test_open_short(struct sec_cmd_data *sec, struct sec_factory_test_mode *mode)
{
	int retval = 0;
	struct ovt_tcm_hcd *tcm_hcd = sec_fn_test->tcm_hcd;

	input_dbg(true, tcm_hcd->pdev->dev.parent, "Start testing\n");

	retval = run_test(tcm_hcd, mode, TEST_PT11_OPEN_DETECTION);
	if (retval < 0)
		goto exit;

	sec_cmd_set_cmd_result(sec, tcm_hcd->print_buf, strlen(tcm_hcd->print_buf));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, tcm_hcd->print_buf, strnlen(tcm_hcd->print_buf, sizeof(tcm_hcd->print_buf)), "OPEN_SHORT");

	sec->cmd_state = SEC_CMD_STATUS_OK;

	return retval;

exit:
	snprintf(tcm_hcd->print_buf, sizeof(tcm_hcd->print_buf), "NG");
	sec_cmd_set_cmd_result(sec, tcm_hcd->print_buf, strnlen(tcm_hcd->print_buf, sizeof(tcm_hcd->print_buf)));

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, tcm_hcd->print_buf, strnlen(tcm_hcd->print_buf, sizeof(tcm_hcd->print_buf)), "OPEN_SHORT");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	return retval;
}

int test_check_connection(struct sec_cmd_data *sec)
{
	int retval = 0;
	struct ovt_tcm_hcd *tcm_hcd = sec_fn_test->tcm_hcd;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	input_dbg(true, tcm_hcd->pdev->dev.parent, "Start testing\n");

	retval = testing_run_prod_test_item(sec_fn_test, TEST_CHECK_CONNECT);
	if (retval < 0) {
		input_err(true, tcm_hcd->pdev->dev.parent, "Failed to run test\n");
		goto exit;
	}

	input_err(true, tcm_hcd->pdev->dev.parent, "%d\n", sec_fn_test->resp.buf[0]);

	if (sec_fn_test->resp.buf[0] == 0)
		goto exit;

	snprintf(buff, sizeof(buff), "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	return retval;

exit:
	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	return retval;
}

int test_fw_crc(struct sec_cmd_data *sec)
{
	int retval = 0;
	struct ovt_tcm_hcd *tcm_hcd = sec_fn_test->tcm_hcd;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	input_dbg(true, tcm_hcd->pdev->dev.parent,
			"Start testing\n");

	retval = testing_run_prod_test_item(sec_fn_test, TEST_FW_CRC);
	if (retval < 0) {
		input_err(true, tcm_hcd->pdev->dev.parent,
				"Failed to run test\n");
		goto exit;
	}

	input_err(true, tcm_hcd->pdev->dev.parent,
				"%d\n", sec_fn_test->resp.buf[0]);

	if (sec_fn_test->resp.buf[0] == 0)
		goto exit;

	snprintf(buff, sizeof(buff), "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	return retval;

exit:
	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	return retval;
}

int test_fw_ram(struct sec_cmd_data *sec)
{
	extern int ovt_tcm_wait_hdl(struct ovt_tcm_hcd *tcm_hcd);
	int retval = 0;
	struct ovt_tcm_hcd *tcm_hcd = sec_fn_test->tcm_hcd;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	input_dbg(true, tcm_hcd->pdev->dev.parent,
			"Start testing fw ram\n");

	retval = testing_run_prod_test_item(sec_fn_test, TEST_SRAM_TEST_START_CMD_NO_RESPONSE);
	if (retval < 0) {
		input_err(true, tcm_hcd->pdev->dev.parent,
				"will get error response when do TEST_SRAM_TEST_START_CMD_NO_RESPONSE, ignore it and wait for HDL Done\n");
	}

	retval = ovt_tcm_wait_hdl(tcm_hcd);
	if (retval < 0) {
		input_err(true, tcm_hcd->pdev->dev.parent,
			"Timed out waiting for completion of host download\n");
		atomic_set(&tcm_hcd->host_downloading, 0);
		retval = -EIO;
		goto exit;
	} else {
		retval = 0;
	}

	retval = testing_run_prod_test_item(sec_fn_test, TEST_SRAM_TEST_GET_TEST_RESULT);
	if (retval < 0) {
		input_err(true, tcm_hcd->pdev->dev.parent,
				"fail to get response of command TEST_SRAM_TEST_GET_TEST_RESULT\n");
		goto exit;
	}

	input_err(true, tcm_hcd->pdev->dev.parent,
				"buf[0]:%x  buf[1]:%x  buf[2]:%x buf[3]:%x buf[4]:%x buf[5]:%x\n", sec_fn_test->resp.buf[0],
				sec_fn_test->resp.buf[1], sec_fn_test->resp.buf[2], sec_fn_test->resp.buf[3], sec_fn_test->resp.buf[4],
				sec_fn_test->resp.buf[5]);

	if ((sec_fn_test->resp.buf[0] == 0x4f) && (sec_fn_test->resp.buf[1] == 0x00)) {  //0x4f and 0x00 is ok case
		snprintf(buff, sizeof(buff), "%d", TEST_RESULT_PASS);
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	} else {
		snprintf(buff, sizeof(buff), "%d", TEST_RESULT_FAIL);
		input_err(true, tcm_hcd->pdev->dev.parent,
				"fail to check stram test resp value\n");
	}
	sec->cmd_state = SEC_CMD_STATUS_OK;
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "SRAM");

	return retval;

exit:
	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "SRAM");

	return retval;
}

#define NOISE_DELTA_MAX 100
int ovt_tcm_get_face_area(int *data_sum, struct sec_factory_test_mode *mode)
{
	int retval;
	unsigned char *resp_buf;
	unsigned int resp_buf_size;
	unsigned int resp_length;
	unsigned char out_buf;
    int row_start, row_end, col_start, col_end, rows, cols;
    int r, c, idx;
	struct ovt_tcm_app_info *app_info;
	struct ovt_tcm_hcd *tcm_hcd = sec_fn_test->tcm_hcd;

	app_info = &tcm_hcd->app_info;
	printk("[sec_input] %s %d\n",__func__,__LINE__);
	rows = le2_to_uint(app_info->num_of_image_rows);
	cols = le2_to_uint(app_info->num_of_image_cols);

	resp_buf = NULL;
	resp_buf_size = 0;
	
	printk("[sec_input] %s %d\n",__func__,__LINE__);

	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_GET_FACE_AREA,
			NULL,
            0,
			&resp_buf,
			&resp_buf_size,
			&resp_length,
			NULL,
			0);
	if (retval < 0) {
		input_err(true, tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_GET_FACE_AREA));
		goto exit;
	}

	if (resp_length < 4) {
		input_err(true, tcm_hcd->pdev->dev.parent,
				"Invalid data length\n");
		retval = -EINVAL;
		goto exit;
	}

    row_start = resp_buf[0];
    row_end = resp_buf[1];
    col_start = resp_buf[2];
    col_end = resp_buf[3];
	printk("[sec_input] %s %d\n",__func__,__LINE__);
    if ((row_start > row_end) || (row_end > rows)) {
		input_err(true, tcm_hcd->pdev->dev.parent,
				"Invalid row parameter:%d %d %d %d\n", row_start, row_end, col_start, col_end);
    }
    if ((col_start > col_end) || (col_end > cols)) {
		input_err(true, tcm_hcd->pdev->dev.parent,
				"Invalid cols parameter:%d %d %d %d\n", row_start, row_end, col_start, col_end);
    }

    out_buf = 195;
	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_PRODUCTION_TEST,
			&out_buf,
            1,
			&resp_buf,
			&resp_buf_size,
			&resp_length,
			NULL,
			0);
	if (retval < 0) {
		input_err(true, tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_PRODUCTION_TEST));
		goto exit;
	}

	if (resp_length != rows * cols * 2) {
		input_err(true, tcm_hcd->pdev->dev.parent,
				"Invalid face delta length:%d\n", resp_length);
		retval = -EINVAL;
		goto exit;
	}
    idx = row_start * cols + col_start;
    *data_sum = 0;

    for (r = row_start; r <= row_end; r++) {
      for (c = col_start; c <= col_end; c++) {
            int data;
            data = (short)le2_to_uint(&resp_buf[idx * 2]);

			if (r == row_start && c == col_start)
				mode->min = mode->max = data;
			mode->min = min(mode->min, (short)data);
			mode->max = max(mode->max, (short)data);

            *data_sum += data;
            if (data > NOISE_DELTA_MAX) {
                retval = -EINVAL;
                goto exit;
            }
            idx++;
      }

    }
	printk("[sec_input] %s %d\n",__func__,__LINE__);
    retval = 0;
exit:
	kfree(resp_buf);
	
	printk("[sec_input] %s %d\n",__func__,__LINE__);

    input_err(true, tcm_hcd->pdev->dev.parent,
            "success to do face test\n");
	return retval;
}

int get_proximity(void) {
	
	int sum, ret;
	struct sec_factory_test_mode mode;
	printk("[sec_input] %s %d\n",__func__,__LINE__);
	ret = ovt_tcm_get_face_area(&sum, &mode);
	if(ret == 0) {
		return sum;
	}
	return -1;	
}

int test_init(struct ovt_tcm_hcd *tcm_hcd)
{
	sec_fn_test = kzalloc(sizeof(*sec_fn_test), GFP_KERNEL);
	if (!sec_fn_test) {
		input_err(true, tcm_hcd->pdev->dev.parent,
			"Failed to allocate memory for sec_fn_test\n");
		return -ENOMEM;
	}

	sec_fn_test->tcm_hcd = tcm_hcd;

	INIT_BUFFER(sec_fn_test->out, false);
	INIT_BUFFER(sec_fn_test->resp, false);
	INIT_BUFFER(sec_fn_test->report, false);
	INIT_BUFFER(sec_fn_test->process, false);
	INIT_BUFFER(sec_fn_test->output, false);

	return 0;
}
