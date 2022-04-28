/*
 * Copyright (c) 2022 Institute of Parallel And Distributed Systems (IPADS)
 * ChCore-Lab is licensed under the Mulan PSL v1.
 * You can use this software according to the terms and conditions of the Mulan PSL v1.
 * You may obtain a copy of Mulan PSL v1 at:
 *     http://license.coscl.org.cn/MulanPSL
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v1 for more details.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

enum tmpfs_ipc_request {
        TMPFS_IPC_REQ_TEST,
        TMPFS_IPC_REQ_MAX,
};

struct tmpfs_ipc_data {
        enum tmpfs_ipc_request request;
        union {
                struct {
                        struct {
                                int arg1;
                                int arg2;
                        } args;
                        struct {
                                int fd;
                        } returns;
                } test;
        };
};

/**
 * Just for test.
 * @param arg1 First argument.
 * @param arg2 Second argument.
 */
int chcore_tmpfs_test(int arg1, int arg2);

#ifdef __cplusplus
}
#endif
