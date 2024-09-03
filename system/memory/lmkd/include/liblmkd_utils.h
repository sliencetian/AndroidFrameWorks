/*
 *  Copyright 2018 Google, Inc
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef _LIBLMKD_UTILS_H_
#define _LIBLMKD_UTILS_H_

#include <sys/cdefs.h>
#include <sys/types.h>

#include <lmkd.h>

__BEGIN_DECLS

/*
 * Connects to lmkd process and returns socket handle.
 * On success returns socket handle.
 * On error, -1 is returned, and errno is set appropriately.
 */
int lmkd_connect();

/*
 * Registers a process with lmkd and sets its oomadj score.
 * On success returns 0.
 * On error, -1 is returned.
 * In the case of error errno is set appropriately.
 */
int lmkd_register_proc(int sock, struct lmk_procprio *params);

/*
 * Registers a batch of processes with lmkd and sets its oomadj score.
 * On success returns 0.
 * On error, -1 is returned.
 * In the case of error errno is set appropriately.
 */
int lmkd_register_procs(int sock, struct lmk_procs_prio* params, const int proc_count);

/*
 * Unregisters a process previously registered with lmkd.
 * On success returns 0.
 * On error, -1 is returned.
 * In the case of error errno is set appropriately.
 */
int lmkd_unregister_proc(int sock, struct lmk_procremove *params);

enum update_props_result {
    UPDATE_PROPS_SUCCESS,
    UPDATE_PROPS_FAIL,
    UPDATE_PROPS_SEND_ERR,
    UPDATE_PROPS_RECV_ERR,
    UPDATE_PROPS_FORMAT_ERR,
};

/*
 * Updates lmkd properties.
 * In the case of ERR_SEND or ERR_RECV errno is set appropriately.
 */
enum update_props_result lmkd_update_props(int sock);

/*
 * Creates memcg directory for given process.
 * On success returns 0.
 * -1 is returned if path creation failed.
 * -2 is returned if tasks file open operation failed.
 * -3 is returned if tasks file write operation failed.
 * In the case of error errno is set appropriately.
 */
int create_memcg(uid_t uid, pid_t pid);

enum boot_completed_notification_result {
    BOOT_COMPLETED_NOTIF_SUCCESS,
    BOOT_COMPLETED_NOTIF_FAILS,
    BOOT_COMPLETED_NOTIF_ALREADY_HANDLED,
    BOOT_COMPLETED_NOTIF_SEND_ERR,
    BOOT_COMPLETED_NOTIF_RECV_ERR,
    BOOT_COMPLETED_NOTIF_FORMAT_ERR,
};

/*
 * Notify LMKD the device has finished booting up.
 */
enum boot_completed_notification_result lmkd_notify_boot_completed(int sock);

enum get_kill_count_err_result {
    GET_KILL_COUNT_SEND_ERR = -1,
    GET_KILL_COUNT_RECV_ERR = -2,
    GET_KILL_COUNT_FORMAT_ERR = -3,
};

/*
 * Get the number of kills LMKD has performed.
 * On success returns number of kills.
 * On error, get_kill_count_err_result integer value.
 */
int lmkd_get_kill_count(int sock, struct lmk_getkillcnt* params);

__END_DECLS

#endif /* _LIBLMKD_UTILS_H_ */
