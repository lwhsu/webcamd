/*-
 * Copyright (c) 2010-2011 Hans Petter Selasky. All rights reserved.
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
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/filio.h>

#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <sched.h>
#include <grp.h>
#include <pwd.h>

#include <libutil.h>

#include <cuse4bsd.h>

#include <webcamd_hal.h>

static cuse_open_t v4b_open;
static cuse_close_t v4b_close;
static cuse_read_t v4b_read;
static cuse_write_t v4b_write;
static cuse_ioctl_t v4b_ioctl;
static cuse_poll_t v4b_poll;

static struct cuse_methods v4b_methods = {
	.cm_open = v4b_open,
	.cm_close = v4b_close,
	.cm_read = v4b_read,
	.cm_write = v4b_write,
	.cm_ioctl = v4b_ioctl,
	.cm_poll = v4b_poll,
};

static const char *devnames[F_V4B_MAX] = {

	[F_V4B_VIDEO] = "video%d",

	[F_V4B_DVB_AUDIO] = "dvb/adapter%d/audio0",
	[F_V4B_DVB_CA] = "dvb/adapter%d/ca0",
	[F_V4B_DVB_DEMUX] = "dvb/adapter%d/demux0",
	[F_V4B_DVB_DVR] = "dvb/adapter%d/dvr0",

	[F_V4B_DVB_FRONTEND] = "dvb/adapter%d/frontend0",
	[F_V4B_DVB_OSD] = "dvb/adapter%d/osd0",
	[F_V4B_DVB_SEC] = "dvb/adapter%d/sec0",
	[F_V4B_DVB_VIDEO] = "dvb/adapter%d/video0",

	[F_V4B_LIRC] = "lirc%d",

	[F_V4B_EVDEV] = "input/event%d",
};

static int u_unit = 0;
static int u_addr = 0;
static int u_index = 0;
static int u_videodev = -1;
static int do_fork = 0;
static int do_realtime = 1;
static int do_hal_register = 0;
static struct pidfh *local_pid = NULL;
static gid_t gid;
static uid_t uid;
static int gid_found;
static int uid_found;
static int vtuner_client;
static int vtuner_server;

#define	CHR_MODE 0660

char	global_fw_prefix[128] = {"/boot/modules"};
int	webcamd_unit;

#define	v4b_errx(code, fmt, ...) do {			\
	fprintf(stderr, fmt "\n",## __VA_ARGS__);	\
	exit(code);					\
} while (0)

static void
v4b_work_exec_hup(int dummy)
{

}

static void *
v4b_work(void *arg)
{
	signal(SIGHUP, v4b_work_exec_hup);

	while (1) {
		if (cuse_wait_and_process() != 0)
			break;
	}

	exit(0);			/* we are done */

	return (NULL);
}

static int
v4b_convert_error(int error)
{
	;				/* indent fix */
	if (error < 0) {
		switch (error) {
		case -EBUSY:
			error = CUSE_ERR_BUSY;
			break;
		case -EWOULDBLOCK:
			error = CUSE_ERR_WOULDBLOCK;
			break;
		case -EINVAL:
			error = CUSE_ERR_INVALID;
			break;
		case -ENOMEM:
			error = CUSE_ERR_NO_MEMORY;
			break;
		case -EFAULT:
			error = CUSE_ERR_FAULT;
			break;
		case -EINTR:
			error = CUSE_ERR_SIGNAL;
			break;
		default:
			error = CUSE_ERR_OTHER;
			break;
		}
	}
	return (error);
}

static int
v4b_open(struct cuse_dev *cdev, int fflags)
{
	struct cdev_handle *handle;
	int f_v4b;
	int fflags_linux;

	f_v4b = (int)(long)cuse_dev_get_priv0(cdev);

	switch (fflags & (CUSE_FFLAG_WRITE | CUSE_FFLAG_READ)) {
	case (CUSE_FFLAG_WRITE | CUSE_FFLAG_READ):
		fflags_linux = O_RDWR;
		break;
	case CUSE_FFLAG_READ:
		fflags_linux = O_RDONLY;
		break;
	case CUSE_FFLAG_WRITE:
		fflags_linux = O_WRONLY;
		break;
	default:
		return (CUSE_ERR_INVALID);
	}

	need_timer(1);

	/* try to open the device */
	handle = linux_open(f_v4b, fflags_linux);

	if (handle == NULL) {
		need_timer(0);
		return (CUSE_ERR_INVALID);
	}
	cuse_dev_set_per_file_handle(cdev, handle);
	return (0);
}

static int
v4b_close(struct cuse_dev *cdev, int fflags)
{
	struct cdev_handle *handle;
	int error;

	handle = cuse_dev_get_per_file_handle(cdev);

	cuse_dev_set_per_file_handle(cdev, NULL);

	/* close device */
	error = linux_close(handle);

	if (error == 0)
		need_timer(0);

	return (v4b_convert_error(error));
}

static int
v4b_read(struct cuse_dev *cdev, int fflags,
    void *peer_ptr, int len)
{
	struct cdev_handle *handle;
	int error;

	handle = cuse_dev_get_per_file_handle(cdev);

	/* read from device */
	error = linux_read(handle, fflags & CUSE_FFLAG_NONBLOCK, peer_ptr, len);

	return (v4b_convert_error(error));
}

static int
v4b_write(struct cuse_dev *cdev, int fflags,
    const void *peer_ptr, int len)
{
	struct cdev_handle *handle;
	int error;

	handle = cuse_dev_get_per_file_handle(cdev);

	/* write to device */
	error = linux_write(handle, fflags & CUSE_FFLAG_NONBLOCK,
	    (uint8_t *)((const uint8_t *)peer_ptr - (const uint8_t *)0), len);

	return (v4b_convert_error(error));
}

static int
v4b_ioctl(struct cuse_dev *cdev, int fflags,
    unsigned long cmd, void *peer_data)
{
	struct v4l2_buffer buf;
	struct cdev_handle *handle;
	void *ptr;
	int error;

	handle = cuse_dev_get_per_file_handle(cdev);

	/* we support blocking/non-blocking I/O */
	if (cmd == FIONBIO || cmd == FIOASYNC)
		return (0);

	/* execute ioctl */
	error = linux_ioctl(handle, fflags & CUSE_FFLAG_NONBLOCK,
	    cmd, peer_data);

	if ((cmd == VIDIOC_QUERYBUF) && (error >= 0)) {

		if (copy_from_user(&buf, peer_data, sizeof(buf)) != 0) {
			error = -EFAULT;
			goto done;
		}
		ptr = linux_mmap(handle, fflags, NULL,
		    buf.length, buf.m.offset);

		if (ptr != MAP_FAILED) {
			buf.m.offset = cuse_vmoffset(ptr);
		} else {
			buf.m.offset = 0x80000000UL;
		}

		if (copy_to_user(peer_data, &buf, sizeof(buf)) != 0) {
			error = -EFAULT;
			goto done;
		}
	}
done:
	return (v4b_convert_error(error));
}

static int
v4b_poll(struct cuse_dev *cdev, int fflags, int events)
{
	struct cdev_handle *handle;
	int error;
	int revents;

	handle = cuse_dev_get_per_file_handle(cdev);

	/* poll device */
	error = linux_poll(handle);

	revents = 0;

	if (error & (POLLPRI | POLLIN | POLLRDNORM))
		revents |= events & CUSE_POLL_READ;

	if (error & (POLLOUT | POLLWRNORM))
		revents |= events & CUSE_POLL_WRITE;
#if 0
	/* currently we mask away any poll errors */
	if (error & (POLLHUP | POLLNVAL | POLLERR))
		revents |= events & CUSE_POLL_ERROR;
#endif
	return (revents);
}

static void
v4b_create(int unit)
{
	struct cdev_handle *handle;
	pthread_t dummy;
	unsigned int n;
	unsigned int p;
	int temp;
	char buf[128];

	for (n = 0; n != (F_V4B_MAX * F_V4B_SUBDEV_MAX); n++) {

		handle = linux_open(n, O_RDONLY);

		if (handle != NULL) {

			linux_close(handle);

			temp = (unit * F_V4B_SUBDEV_MAX) +
			    (n % F_V4B_SUBDEV_MAX);

			cuse_dev_create(&v4b_methods, (void *)(long)n,
			    0, uid, gid, CHR_MODE, devnames[n / F_V4B_SUBDEV_MAX],
			    temp);

			snprintf(buf, sizeof(buf), devnames[n / F_V4B_SUBDEV_MAX], temp);

			printf("Creating /dev/%s\n", buf);

			for (p = 0; p != 4; p++) {
				if (pthread_create(&dummy, NULL, v4b_work, NULL)) {
					v4b_errx(1, "Failed creating Video4BSD process");
				}
			}

			if (do_hal_register)
				hal_add_device(buf);
		}
	}
}

static void
usage(void)
{
	fprintf(stderr,
	    "usage: webcamd -d [ugen]<unit>.<addr> -i 0 -v -1 -B\n"
	    "	-d <USB device>\n"
	    "	-i <interface number>\n"
	    "	-m <parameter>=<value>\n"
	    "	-s Show available parameters\n"
	    "	-v <video device number>\n"
	    "	-B Run in background\n"
	    "	-f <firmware path> [%s]\n"
	    "	-r Do not set realtime priority\n"
	    "	-U <user> Set user for character devices\n"
	    "	-G <group> Set group for character devices\n"
	    "	-H Register device by HAL daemon\n"
	    "	-D <host:port:ndev:type> Connect to remote host instead of USB\n"
	    "	   Type values: DVB-S=1,DVB-C=2,DVB-T=4,DVB-S2=8\n"
	    "	-L <host:port:ndev> Make DVB device available from TCP/IP\n"
	    "	-h Print help\n",
	    global_fw_prefix
	);
	exit(1);
}

static void
v4b_exit(void)
{
	if (local_pid != NULL) {
		pidfile_remove(local_pid);
		local_pid = NULL;
	}
}

int
pidfile_create(int bus, int addr, int index)
{
	char buf[64];

	if (local_pid != NULL)
		return (0);

	snprintf(buf, sizeof(buf), "/var/run/webcamd."
	    "%d.%d.%d.pid", bus, addr, index);

	local_pid = pidfile_open(buf, 0600, NULL);
	if (local_pid == NULL) {
		return (EEXIST);
	} else {
		pidfile_write(local_pid);
	}

	printf("Attached ugen%d.%d[%d] to cuse unit %d\n",
	    bus, addr, index, u_videodev);

	return (0);
}

/*
 * The following three functions were copied from FreeBSD's chown
 * utility.
 */
static uid_t
id(const char *name, const char *type)
{
	uid_t val;
	char *ep;

	val = strtoul(name, &ep, 10);
	if (*ep != '\0')
		v4b_errx(1, "%s: illegal %s name", name, type);
	return (val);
}

static void
a_gid(const char *s)
{
	struct group *gr;

	gid = ((gr = getgrnam(s)) != NULL) ? gr->gr_gid : id(s, "group");
	gid_found = 1;
}

static void
a_uid(const char *s)
{
	struct passwd *pw;

	uid = ((pw = getpwnam(s)) != NULL) ? pw->pw_uid : id(s, "user");
	uid_found = 1;
}

int
main(int argc, char **argv)
{
	const char *params = "Bd:f:i:m:sv:hHrU:G:D:L:";
	char *ptr;
	int opt;

	while ((opt = getopt(argc, argv, params)) != -1) {
		switch (opt) {
		case 'd':
			ptr = optarg;

			if ((ptr[0] == 'u') &&
			    (ptr[1] == 'g') &&
			    (ptr[2] == 'e') &&
			    (ptr[3] == 'n'))
				ptr += 4;

			if (sscanf(ptr, "%d.%d", &u_unit, &u_addr) != 2)
				usage();
			break;

		case 'i':
			u_index = atoi(optarg);
			break;

		case 'D':
		case 'L':
		case 'm':
		case 's':
			break;

		case 'v':
			u_videodev = atoi(optarg);
			break;

		case 'B':
			do_fork = 1;
			break;

		case 'f':
			strlcpy(global_fw_prefix, optarg,
			    sizeof(global_fw_prefix));
			break;

		case 'r':
			do_realtime = 0;
			break;

		case 'H':
			do_hal_register = 1;
			break;

		case 'U':
			a_uid(optarg);
			break;

		case 'G':
			a_gid(optarg);
			break;

		default:
			usage();
			break;
		}
	}

	if (!uid_found)
		a_uid("webcamd");

	if (!gid_found)
		a_gid("webcamd");

	if (do_fork) {
		/* need to daemonise before creating any threads */

		if (u_addr == 0)
			v4b_errx(1, "-B options requires a valid -d option");

		if (pidfile_create(u_unit, u_addr, u_index)) {
			fprintf(stderr, "Webcamd is already running for "
			    "ugen%d.%d.%d\n", u_unit, u_addr, u_index);
			exit(1);
		}
		if (daemon(0, 0) != 0)
			v4b_errx(1, "Cannot daemonize");
	}
	atexit(&v4b_exit);

	if (cuse_init() != 0) {
		v4b_errx(1, "Could not open /dev/cuse. "
		    "Did you kldload cuse4bsd?");
	}
	if (u_videodev < 0) {
		if (cuse_alloc_unit_number(&u_videodev) != 0)
			v4b_errx(1, "Cannot allocate unique unit number");
	}
	webcamd_unit = u_videodev * F_V4B_SUBDEV_MAX;

	if (do_realtime != 0) {
		struct sched_param params;

		memset(&params, 0, sizeof(params));

		params.sched_priority = sched_get_priority_max(SCHED_FIFO) - 1;

		if (sched_setscheduler(getpid(), SCHED_FIFO, &params) == -1)
			printf("Cannot set realtime priority\n");
	}
	/* get all module parameters registered */

	linux_parm();

	/* process all module parameters */

	optreset = 1;
	optind = 1;

	while ((opt = getopt(argc, argv, params)) != -1) {
		switch (opt) {
			char *ndev;
			char *type;
			char *host;
			char *cport;

		case 'm':
			ptr = strstr(optarg, "=");
			if (ptr == NULL) {
				v4b_errx(1, "invalid parameter for "
				    "-m option: '%s'", optarg);
			}
			*ptr = 0;
			if (mod_set_param(optarg, ptr + 1) < 0) {
				fprintf(stderr, "WARNING: cannot set module "
				    "parameter '%s'='%s'\n", optarg, ptr + 1);
			}
			break;

		case 'D':
			host = optarg;
			cport = strstr(host, ":");
			if (cport == NULL) {
				v4b_errx(1, "invalid syntax for "
				    "-D option: '%s'", optarg);
			}
			*cport++ = 0;
			ndev = strstr(cport, ":");
			if (ndev == NULL) {
				v4b_errx(1, "invalid syntax for "
				    "-D option: '%s'", cport);
			}
			*ndev++ = 0;
			type = strstr(ndev, ":");
			if (type == NULL) {
				v4b_errx(1, "invalid syntax for "
				    "-D option: '%s'", ndev);
			}
			*type++ = 0;

			ptr = strstr(type, ":");
			if (ptr != NULL)
				*ptr = 0;

			if (mod_set_param("vtuner_client.devices", ndev) < 0 ||
			    mod_set_param("vtuner_client.type", type) < 0 ||
			    mod_set_param("vtuner_client.host", host) < 0 ||
			    mod_set_param("vtuner_client.cport", cport) < 0) {
				v4b_errx(1, "Cannot set all module "
				    "parameters for vTuner client.");
			}
			break;

		case 'L':
			host = optarg;
			cport = strstr(host, ":");
			if (cport == NULL) {
				v4b_errx(1, "invalid syntax for "
				    "-L option: '%s'", optarg);
			}
			*cport++ = 0;
			ndev = strstr(cport, ":");
			if (ndev == NULL) {
				v4b_errx(1, "invalid syntax for "
				    "-L option: '%s'", cport);
			}
			*ndev++ = 0;
			ptr = strstr(ndev, ":");
			if (ptr != NULL)
				*ptr = 0;

			if (mod_set_param("vtuner_server.devices", ndev) < 0 ||
			    mod_set_param("vtuner_server.host", host) < 0 ||
			    mod_set_param("vtuner_server.cport", cport) < 0) {
				v4b_errx(1, "Cannot set all module "
				    "parameters for vTuner server.");
			}
			break;

		case 's':
			printf("List of available parameters:\n");
			mod_show_params();
			exit(0);
			break;

		default:
			break;
		}
	}

	if (mod_get_int_param("vtuner_client.devices") != 0)
		vtuner_client = 1;

	if (mod_get_int_param("vtuner_server.devices") != 0)
		vtuner_server = 1;

	if (vtuner_client && vtuner_server)
		v4b_errx(1, "Cannot specify both vTuner server and client");

	/* run rest of Linux init code */

	linux_init();

	if (vtuner_client == 0) {
		if (usb_linux_probe_p(&u_unit, &u_addr, &u_index) < 0)
			v4b_errx(1, "Cannot find USB device");
	}
	if (vtuner_server == 0) {
		if (do_hal_register)
			hal_init(u_unit, u_addr, u_index);

		v4b_create(u_videodev);

		v4b_work(NULL);
	} else {
		while (1)
			pause();
	}
	return (0);
}

unsigned long
copy_to_user(void *to, const void *from, unsigned long n)
{
	int error;

	if (vtuner_server != 0) {
		memcpy(to, from, n);
		return (0);
	}
	error = cuse_copy_out(from, to, (int)n);

	return ((error != 0) ? n : 0);
}

unsigned long
copy_from_user(void *to, const void *from, unsigned long n)
{
	int error;

	if (vtuner_server != 0) {
		memcpy(to, from, n);
		return (0);
	}
	error = cuse_copy_in(from, to, (int)n);

	return ((error != 0) ? n : 0);
}

static uint32_t zero_alloc[1];

void   *
malloc_vm(size_t size)
{
	if (size == 0)
		return (zero_alloc);

	return (cuse_vmalloc(size));
}

void
free_vm(void *ptr)
{
	if (ptr == zero_alloc)
		return;

	cuse_vmfree(ptr);
}

int
check_signal(void)
{
	return (cuse_got_peer_signal() == 0 ||
	    thread_got_stopping() == 0);
}

void
poll_wakeup_internal(void)
{
	cuse_poll_wakeup();
}

struct cdev_handle *
get_current_cdev_handle(void)
{
	struct cuse_dev *cdev;

	cdev = cuse_dev_get_current(NULL);

	if (cdev == NULL)
		return (NULL);

	return (cuse_dev_get_per_file_handle(cdev));
}
