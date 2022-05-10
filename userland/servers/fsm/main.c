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

#include <stdio.h>
#include <chcore/types.h>
#include <chcore/fsm.h>
#include <chcore/fakefs.h>
#include <chcore/ipc.h>
#include <chcore/internal/raw_syscall.h>
#include <chcore/internal/server_caps.h>
#include <chcore/procm.h>
#include <chcore/fs/defs.h>
#include "mount_info.h"
#include "fsm.h"

extern struct spinlock fsmlock;


extern struct list_head fsm_mount_info_mapping;

/* Mapping a pair of client_badge and fd to a mount_point_info_node struct*/
void fsm_set_mount_info_withfd(u64 client_badge, int fd, 
						struct mount_point_info_node* mount_point_info) {

	struct client_fd_info_node *private_iter;
	for_each_in_list(private_iter, struct client_fd_info_node, node, &fsm_mount_info_mapping) {
		if (private_iter->client_badge == client_badge) {
			private_iter->mount_point_info[fd] = mount_point_info;
			return;
		}
	}
	struct client_fd_info_node *n = (struct client_fd_info_node *)malloc(sizeof(struct client_fd_info_node));
	n->client_badge = client_badge;
	int i;
	for (i = 0; i < MAX_SERVER_ENTRY_PER_CLIENT; i++)
		n->mount_point_info[i] = NULL;

	n->mount_point_info[fd] = mount_point_info;
	/* Insert node to fsm_server_entry_mapping */
	list_append(&n->node, &fsm_mount_info_mapping);
}


/* Get a mount_point_info_node struct with a pair of client_badge and fd*/
struct mount_point_info_node* fsm_get_mount_info_withfd(u64 client_badge, int fd) {
	struct client_fd_info_node *n;
	for_each_in_list(n, struct client_fd_info_node, node, &fsm_mount_info_mapping)
		if (n->client_badge == client_badge)
			return n->mount_point_info[fd];
	return NULL;
}

void strip_path(struct mount_point_info_node *mpinfo, char* path) {
	if(strcmp(mpinfo->path, "/")) {
		char* s = path;
		int i, len_after_strip;
		len_after_strip = strlen(path) - mpinfo->path_len;
		if(len_after_strip == 0) {
			path[0] = '/';
			path[1] = '\0';
		} else {
			for(i = 0; i < len_after_strip; ++i) {
				path[i] = path[i + mpinfo->path_len];
			}
			path[i] = '\0';
		}
	}
}

/* You could add new functions here as you want. */
/* LAB 5 TODO BEGIN */
// static struct ipc_struct *fs_ipc_struct = NULL;

static struct ipc_struct *fakefs_ipc_struct = NULL;

static struct ipc_struct *tmpfs_ipc_struct = NULL;

static void connect_fakefs_server(int fakefs_cap)
{
        fakefs_ipc_struct = ipc_register_client(fakefs_cap);
        chcore_assert(fakefs_ipc_struct);
}

void connect_tmpfs_server(void)
{
        int tmpfs_cap = __chcore_get_tmpfs_cap();
        tmpfs_ipc_struct = ipc_register_client(tmpfs_cap);
}

bool checkFakefs(char pathname[]){
	// printf("pathname is %s\n", pathname);
	bool isFakefs = false;
	char *path = pathname;
	char *fakefsPath = "/fakefs";
	while(path && *path != '\0'){
		if(*fakefsPath == *path){
			path++;
			fakefsPath++;
			if((!fakefsPath && !path) || (*fakefsPath == '\0' && *path == '\0')){
				// printf("in fakefs\n");
				return true;
			}
		}
		else{
			if((!fakefsPath || *fakefsPath == '\0') && path){
				// printf("in fakefs\n");
				return true;
			}
			break;
		}
	}
	// printf("in tmpfs\n");

	return false;
}


/* LAB 5 TODO END */


void fsm_server_dispatch(struct ipc_msg *ipc_msg, u64 client_badge)
{
	int ret;
	bool ret_with_cap = false;
	struct fs_request *fr;
	fr = (struct fs_request *)ipc_get_msg_data(ipc_msg);
	struct mount_point_info_node *mpinfo = NULL;

	/* You could add code here as you want.*/
	/* LAB 5 TODO BEGIN */
	connect_tmpfs_server();

	/* LAB 5 TODO END */

	spinlock_lock(&fsmlock);

	switch(fr->req) {
		case FS_REQ_MOUNT:
			// printf("fs mount\n");
			ret = fsm_mount_fs(fr->mount.fs_path, fr->mount.mount_path); // path=(device_name), path2=(mount_point)
			connect_fakefs_server(ret);
			break;
		case FS_REQ_UMOUNT:
			// printf("fs unmount\n");
			ret = fsm_umount_fs(fr->mount.fs_path);
			break;
		case FS_REQ_GET_FS_CAP:
			// printf("fs get_fs cap\n");
			mpinfo = get_mount_point(fr->getfscap.pathname, strlen(fr->getfscap.pathname));
			strip_path(mpinfo, fr->getfscap.pathname);
			ipc_msg->cap_slot_number = 1;
			ipc_set_msg_cap(ipc_msg, 0, mpinfo->fs_cap);
			// printf("mpinfo->fs_cap is %d\n", mpinfo->fs_cap);
			ret_with_cap = true;
			break;

		/* LAB 5 TODO BEGIN */
		case FS_REQ_CREAT:{
			printf("fs create %s\n", fr->creat.pathname);
			if(checkFakefs(fr->creat.pathname)){
				char *path = "/fakefs";
				mpinfo = get_mount_point(path, strlen(path));
			}
			else{
				char *path = "/";
				mpinfo = get_mount_point(path, strlen(path));
			}
			strip_path(mpinfo, fr->creat.pathname);
			struct ipc_msg *ipc_msg_fs = ipc_create_msg(
				mpinfo->_fs_ipc_struct, sizeof(struct fs_request), 0);
			chcore_assert(ipc_msg_fs);
			struct fs_request * fr_fs =
					(struct fs_request *)ipc_get_msg_data(ipc_msg_fs);
			fr_fs->req = FS_REQ_CREAT;
			strcpy(fr_fs->creat.pathname, fr->creat.pathname);
			ret = ipc_call(mpinfo->_fs_ipc_struct, ipc_msg_fs);
			ipc_destroy_msg(mpinfo->_fs_ipc_struct, ipc_msg_fs);

			break;
		}
		case FS_REQ_OPEN:{
			// printf("fs open %s\n", fr->open.pathname);
			
			if(checkFakefs(fr->open.pathname)){
				char *path = "/fakefs";
				mpinfo = get_mount_point(path, strlen(path));
				// printf("mpinfo path is %s\n", mpinfo->path);
			}
			else{
				char *path = "/";
				mpinfo = get_mount_point(path, strlen(path));
				// printf("mpinfo path is %s\n", mpinfo->path);
			}
			strip_path(mpinfo, fr->open.pathname);
			struct ipc_msg *ipc_msg_fs = ipc_create_msg(
				mpinfo->_fs_ipc_struct, sizeof(struct fs_request), 0);
			chcore_assert(ipc_msg_fs);
			struct fs_request * fr_fs =
				(struct fs_request *)ipc_get_msg_data(ipc_msg_fs);
			fr_fs->req = FS_REQ_OPEN;
			strcpy(fr_fs->open.pathname, fr->open.pathname);
			// printf("fr_fs->open.pathname is %s\n", fr_fs->open.pathname);
			fr_fs->open.flags = O_RDONLY;
			fr_fs->open.new_fd = fr->open.new_fd;
			ret = ipc_call(mpinfo->_fs_ipc_struct, ipc_msg_fs);
			// printf("ret is %d\n", ret);
			fsm_set_mount_info_withfd(client_badge, ret, mpinfo);
			ipc_destroy_msg(mpinfo->_fs_ipc_struct, ipc_msg_fs);

			break;
		}
		case FS_REQ_READ:{
			// printf("fs read %d\n", fr->read.fd);
			mpinfo = fsm_get_mount_info_withfd(client_badge, fr->read.fd);
			int cnt = fr->read.count;
            struct ipc_msg *ipc_msg_fs = ipc_create_msg(
                    mpinfo->_fs_ipc_struct, sizeof(struct fs_request), 0);
            chcore_assert(ipc_msg_fs);
            struct fs_request * fr_fs =
                    (struct fs_request *)ipc_get_msg_data(ipc_msg_fs);
            fr_fs->req = FS_REQ_READ;
            fr_fs->read.fd = fr->read.fd;
            fr_fs->read.count = cnt;
            ret = ipc_call(mpinfo->_fs_ipc_struct, ipc_msg_fs);
			char *buf = (void *)fr;
            if(ret > 0) {
                memcpy(buf, ipc_get_msg_data(ipc_msg_fs), ret);
            }
            ipc_destroy_msg(mpinfo->_fs_ipc_struct, ipc_msg_fs);

			break;
		}

		// case FS_REQ_WRITE:{
		// 	printf("fs write\n");
		// 	int file_fd = fr->write.fd;
		// 	int cnt = fr->write.count;
		// 	struct ipc_msg *ipc_msg_fs = ipc_create_msg(
        //         fs_ipc_struct, sizeof(struct fs_request) + cnt + 1, 0);
		// 	chcore_assert(ipc_msg_fs);
		// 	struct fs_request * fr_fs =
		// 			(struct fs_request *)ipc_get_msg_data(ipc_msg_fs);

		// 	fr_fs->req = FS_REQ_WRITE;
		// 	fr_fs->write.fd = file_fd;
		// 	fr_fs->write.count = cnt;
		// 	char *buf = (void *)fr + sizeof(struct fs_request);
		// 	printf("we want write in buf is %s\n", buf);
		// 	ipc_set_msg_data(ipc_msg_fs, buf, sizeof(struct fs_request), cnt + 1);

		// 	ret = ipc_call(fs_ipc_struct, ipc_msg_fs);

		// 	ipc_destroy_msg(fs_ipc_struct, ipc_msg_fs);
			
		// 	break;
		// }
		// case FS_REQ_LSEEK:{
		// 	printf("fs lseek\n");
		// 	struct ipc_msg *ipc_msg_fs = ipc_create_msg(
        //         fs_ipc_struct, sizeof(struct fs_request), 0);
		// 	chcore_assert(ipc_msg_fs);
		// 	struct fs_request * fr_fs =
		// 			(struct fs_request *)ipc_get_msg_data(ipc_msg_fs);
		// 	fr_fs->req = FS_REQ_LSEEK;
		// 	fr_fs->lseek.fd = fr->lseek.fd;
		// 	fr_fs->lseek.offset = 0;
		// 	fr_fs->lseek.whence = 0;
		// 	ret = ipc_call(fs_ipc_struct, ipc_msg_fs);
		// 	ipc_destroy_msg(fs_ipc_struct, ipc_msg_fs);

		// 	break;
		// }
		// case FS_REQ_MKDIR:{
		// 	printf("fs mkdir\n");
		// 	struct ipc_msg *ipc_msg_fs;
		// 	if(checkFakefs(fr->mkdir.pathname)){
		// 		fs_ipc_struct = fakefs_ipc_struct;
		// 	}
		// 	else{
		// 		fs_ipc_struct = tmpfs_ipc_struct;
		// 	}
		// 	ipc_msg_fs = ipc_create_msg(
		// 		fs_ipc_struct, sizeof(struct fs_request), 0);
		// 	chcore_assert(ipc_msg_fs);
		// 	struct fs_request * fr_fs =
		// 			(struct fs_request *)ipc_get_msg_data(ipc_msg_fs);
		// 	fr_fs->req = FS_REQ_MKDIR;
		// 	strcpy(fr_fs->mkdir.pathname, fr->mkdir.pathname);
		// 	ret = ipc_call(fs_ipc_struct, ipc_msg_fs);
		// 	ipc_destroy_msg(fs_ipc_struct, ipc_msg_fs);
			
		// 	break;
		// }
		// case FS_REQ_GET_SIZE:{
		// 	printf("fs get size\n");
		// 	struct ipc_msg *ipc_msg_fs;
		// 	if(checkFakefs(fr->getsize.pathname)){
		// 		fs_ipc_struct = fakefs_ipc_struct;
		// 	}
		// 	else{
		// 		fs_ipc_struct = tmpfs_ipc_struct;
		// 	}
		// 	ipc_msg_fs = ipc_create_msg(
		// 		fs_ipc_struct, sizeof(struct fs_request), 0);
		// 	chcore_assert(ipc_msg_fs);
		// 	struct fs_request * fr_fs =
		// 			(struct fs_request *)ipc_get_msg_data(ipc_msg_fs);

		// 	fr_fs->req = FS_REQ_GET_SIZE;
		// 	strcpy(fr_fs->getsize.pathname, fr->getsize.pathname);

		// 	ret = ipc_call(fs_ipc_struct, ipc_msg_fs);
		// 	ipc_destroy_msg(fs_ipc_struct, ipc_msg_fs);

		// 	break;
		// }

		case FS_REQ_GETDENTS64:{
			// printf("fs getdents64 %d\n", fr->getdents64.fd);
			mpinfo = fsm_get_mount_info_withfd(client_badge, fr->getdents64.fd);
			struct ipc_msg *ipc_msg_fs = ipc_create_msg(mpinfo->_fs_ipc_struct, 512, 0);
			struct fs_request * fr_fs = (struct fs_request *)ipc_get_msg_data(ipc_msg_fs);
			fr_fs->req = FS_REQ_GETDENTS64;
			fr_fs->getdents64.fd = fr->getdents64.fd;
			fr_fs->getdents64.count = fr->getdents64.count;
			ret = ipc_call(mpinfo->_fs_ipc_struct, ipc_msg_fs);
			ipc_set_msg_data(ipc_msg, ipc_get_msg_data(ipc_msg_fs), 0, ret);
			ipc_destroy_msg(mpinfo->_fs_ipc_struct, ipc_msg_fs);

			break;
		}
		/* LAB 5 TODO END */

		default:
			printf("[Error] Strange FS Server request number %d\n", fr->req);
			ret = -EINVAL;
		break;

	}
	
	spinlock_unlock(&fsmlock);

	if(ret_with_cap) {
		ipc_return_with_cap(ipc_msg, ret);
	} else {
		ipc_return(ipc_msg, ret);
	}
}


int main(int argc, char *argv[])
{

	init_fsm();

	ipc_register_server(fsm_server_dispatch);

	while (1) {
		__chcore_sys_yield();
	}
	return 0;
}
