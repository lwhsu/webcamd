/*-
 * Copyright (c) 2010 Hans Petter Selasky. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/mman.h>

#include <fcntl.h>

#include "../video4bsd/video4bsd.h"

static int f_videodev = -1;
static int u_videodev = -1;
static int f_usb = -1;
static int local_user = 0;
static void *mm_ptr = MAP_FAILED;

struct vm_allocation {
	uint8_t *ptr;
	uint32_t size;
};

static struct vm_allocation vm_allocations[V4B_ALLOC_UNIT_MAX];

static void set_mmap(void *);
static void find_mmap_size(struct cdev *cdev, uint32_t offset, uint32_t *psize, uint32_t *delta);

static int
open_video4bsd(int unit)
{
	char temp[32];

	snprintf(temp, sizeof(temp), "/dev/video_daemon%d", unit);

	f_videodev = open(temp, O_RDWR);

	return (f_videodev);
}

static void
usage(void)
{
	fprintf(stderr, "usage: webcamd -d [ugen]<unit>.<addr> -i 0 -v -1\n");
	exit(1);
}

static void
v4b_errx(int code, const char *str)
{
	fprintf(stderr, "%s\n", str);
	exit(code);
}

int
main(int argc, char **argv)
{
	static struct v4b_command cmd;
	const char *ptr;
	struct cdev *cdev;
	int unit = 0;
	int addr = 0;
	int index = 0;
	int opt;
	int err;
	uint32_t size;
	uint32_t delta;

	while ((opt = getopt(argc, argv, "d:i:v:h")) != -1) {
		switch (opt) {
		case 'd':
			ptr = optarg;

			if ((ptr[0] == 'u') &&
			    (ptr[1] == 'g') &&
			    (ptr[2] == 'e') &&
			    (ptr[3] == 'n'))
				ptr += 4;

			if (sscanf(ptr, "%d.%d", &unit, &addr) != 2)
				usage();
			break;

		case 'i':
			index = atoi(optarg);
			break;

		case 'v':
			u_videodev = atoi(optarg);
			break;
		default:
			usage();
			break;
		}
	}

	if (u_videodev < 0) {
		for (u_videodev = 0;; u_videodev++) {
			if (u_videodev == V4B_DEVICES_MAX)
				v4b_errx(1, "Cannot open /dev/video_daemonX");
			if (open_video4bsd(u_videodev) >= 0)
				break;
		}
	} else {
		if (open_video4bsd(u_videodev) < 0)
			v4b_errx(1, "Cannot open /dev/video_daemonX");
	}

	linux_init();

#ifdef V4B_DEBUG
	printf("Probing for %d.%d.%d\n", unit, addr, index);
#endif

	f_usb = usb_linux_probe(unit, addr, index);
	if (f_usb < 0)
		v4b_errx(1, "Cannot find USB device");

	cdev = usb_linux2cdev(f_usb);
	if (cdev == NULL)
		v4b_errx(1, "Cannot find USB character device");

	while (1) {
		if (ioctl(f_videodev, V4B_IOCTL_GET_COMMAND, &cmd) != 0)
			v4b_errx(1, "Cannot get next V4B command");
#ifdef V4B_DEBUG
		printf("Received command %d 0x%08x\n", cmd.command, (uint32_t)cmd.arg);
#endif
		switch (cmd.command) {
		case V4B_CMD_OPEN:
			/* make sure we are closed before open */
			err = linux_close(cdev);

			/* try to open the device */
			err = linux_open(cdev);
			if (ioctl(f_videodev, V4B_IOCTL_SYNC_COMMAND, &err) != 0)
				v4b_errx(1, "Cannot sync V4B command");
			break;
		case V4B_CMD_CLOSE:
			/* close device */
			err = linux_close(cdev);
			if (ioctl(f_videodev, V4B_IOCTL_SYNC_COMMAND, &err) != 0)
				v4b_errx(1, "Cannot sync V4B command");
			break;

		case V4B_CMD_READ:
			err = linux_read(cdev, cmd.ptr, cmd.arg);
			if (err >= 0)
				err = 0;
			if (ioctl(f_videodev, V4B_IOCTL_SYNC_COMMAND, &err) != 0)
				v4b_errx(1, "Cannot sync V4B command");
			break;

		case V4B_CMD_WRITE:
			err = linux_write(cdev, cmd.ptr, cmd.arg);
			if (err >= 0)
				err = 0;
			if (ioctl(f_videodev, V4B_IOCTL_SYNC_COMMAND, &err) != 0)
				v4b_errx(1, "Cannot sync V4B command");
			break;

		case V4B_CMD_IOCTL:
			err = linux_ioctl(cdev, cmd.arg, cmd.ptr);
			if (ioctl(f_videodev, V4B_IOCTL_SYNC_COMMAND, &err) != 0)
				v4b_errx(1, "Cannot sync V4B command");
			break;

		case V4B_CMD_MMAP:

			/* XXX V4L hack */
			find_mmap_size(cdev, cmd.arg, &size, &delta);
			if (size != 0) {
				if (delta == 0) {
					mm_ptr = linux_mmap(cdev, NULL, size, cmd.arg);
				}
				if (mm_ptr == MAP_FAILED) {
					err = EINVAL;
				} else {
					err = 0;
					set_mmap(mm_ptr + delta);
				}
			} else {
				err = EINVAL;
			}
			if (ioctl(f_videodev, V4B_IOCTL_SYNC_COMMAND, &err) != 0)
				v4b_errx(1, "Cannot sync V4B command");
			break;

		default:
			err = ENOTTY;
			if (ioctl(f_videodev, V4B_IOCTL_SYNC_COMMAND, &err) != 0)
				v4b_errx(1, "Cannot sync V4B command");
			break;
		}
#ifdef V4B_DEBUG
		printf("Status = %d\n", err);
#endif

	}
	return (0);
}

unsigned long
copy_to_user(void *to, const void *from, unsigned long n)
{
	struct v4b_data_chunk cmd = {
		.client_ptr = (uint8_t *)(from - (const void *)0),
		.peer_ptr = to,
		.length = n,
	};

	if (local_user == 0)
		return (ioctl(f_videodev, V4B_IOCTL_WRITE_DATA, &cmd) ? n : 0);

	memcpy(to, from, n);
	return (0);
}

unsigned long
copy_from_user(void *to, const void *from, unsigned long n)
{
	struct v4b_data_chunk cmd = {
		.client_ptr = to,
		.peer_ptr = (uint8_t *)(from - (const void *)0),
		.length = n,
	};

	if (local_user == 0)
		return (ioctl(f_videodev, V4B_IOCTL_READ_DATA, &cmd) ? n : 0);

	memcpy(to, from, n);
	return (0);
}

static void
find_mmap_size(struct cdev *cdev, uint32_t offset,
    uint32_t *psize, uint32_t *delta)
{
	struct v4l2_buffer buf = {0, 0, 0};
	int err;
	int i;

	local_user = 1;

	for (i = 0;; i++) {

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;

		err = linux_ioctl(cdev, VIDIOC_QUERYBUF, &buf);
		if (err) {
			*psize = 0;
			*delta = 0;
			break;
		}
		if ((offset >= buf.m.offset) && (offset <= (buf.m.offset + buf.length - 1))) {
			*psize = buf.length;
			*delta = offset - buf.m.offset;
			break;
		}
	}

	local_user = 0;
}

static void
set_mmap(void *ptr)
{
	struct v4b_mem_alloc cmd = {0, 0};
	unsigned int n;
	void *ptr_min;
	void *ptr_max;

	atomic_lock();
	for (n = 0; n != V4B_ALLOC_UNIT_MAX; n++) {
		if (vm_allocations[n].ptr == NULL)
			continue;

		ptr_min = vm_allocations[n].ptr;
		ptr_max = vm_allocations[n].ptr + vm_allocations[n].size - 1;

		if ((ptr >= ptr_min) && (ptr <= ptr_max)) {

			cmd.alloc_nr = n;
			cmd.page_count = (ptr - ptr_min) / PAGE_SIZE;

			atomic_unlock();
			ioctl(f_videodev, V4B_IOCTL_MAP_MEMORY, &cmd);
			return;
		}
	}
	atomic_unlock();
}

void   *
malloc_vm(size_t size)
{
	struct v4b_mem_alloc cmd = {0, 0};
	void *ptr;
	uint32_t n;
	int error;

	if (size == 0)
		size = 1;

	cmd.page_count = (size + PAGE_SIZE - 1) / PAGE_SIZE;

	printf("size = %d\n", (int)size);

	atomic_lock();
	for (n = 0; n != V4B_ALLOC_UNIT_MAX; n++) {
		if (vm_allocations[n].ptr != NULL)
			continue;

		vm_allocations[n].ptr = (uint8_t *)1;	/* reserve */
		atomic_unlock();

		printf("alloc_nr = %d\n", n);

		cmd.alloc_nr = n;

		error = ioctl(f_videodev, V4B_IOCTL_ALLOC_MEMORY, &cmd);
		if (error) {
			atomic_lock();
			vm_allocations[n].ptr = NULL;

			printf("Alloc failed %d\n", __LINE__);

			if (errno == EBUSY)
				continue;

			break;
		}
		ptr = mmap(NULL, size,
		    PROT_READ | PROT_WRITE,
		    MAP_SHARED, f_videodev, V4B_ALLOC_PAGES_MAX * n);

		if (ptr == MAP_FAILED) {
			ioctl(f_videodev, V4B_IOCTL_FREE_MEMORY, &cmd);
			atomic_lock();
			vm_allocations[n].ptr = NULL;

			printf("Alloc failed %d\n", __LINE__);

			break;
		}
		vm_allocations[n].ptr = ptr;
		vm_allocations[n].size = size;
		return (ptr);
	}
	atomic_unlock();

	return (NULL);
}

void
free_vm(void *ptr)
{
	struct v4b_mem_alloc cmd = {0, 0};
	unsigned int n;

	if (ptr == NULL)
		return;

	atomic_lock();
	for (n = 0; n != V4B_ALLOC_UNIT_MAX; n++) {
		if (vm_allocations[n].ptr != ptr)
			continue;

		atomic_unlock();

		cmd.alloc_nr = n;

		munmap(ptr, vm_allocations[n].size);

		ioctl(f_videodev, V4B_IOCTL_FREE_MEMORY, &cmd);

		atomic_lock();

		vm_allocations[n].ptr = NULL;
		vm_allocations[n].size = 0;

		break;
	}
	atomic_unlock();
}