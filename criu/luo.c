#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "cr_options.h"
#include "linux/liveupdate.h"
#include "log.h"
#include "luo.h"

#define LUO_DEVICE "/dev/liveupdate"

bool luo_mode_enabled(void)
{
	return opts.luo_session != NULL;
}

static int luo_dev_open(void)
{
	int fd = open(LUO_DEVICE, O_RDWR | O_CLOEXEC);

	if (fd < 0) {
		int ret = -errno;

		pr_perror("luo: cannot open %s", LUO_DEVICE);
		return ret;
	}
	return fd;
}

static int luo_check_name(const char *name)
{
	if (!name || strlen(name) >= LIVEUPDATE_SESSION_NAME_LENGTH) {
		pr_err("luo: session name must be 1-%d characters\n", LIVEUPDATE_SESSION_NAME_LENGTH - 1);
		return -EINVAL;
	}
	return 0;
}

int luo_session_create(const char *name)
{
	struct liveupdate_ioctl_create_session arg = { 0 };
	int dev, ret;

	ret = luo_check_name(name);
	if (ret)
		return ret;

	dev = luo_dev_open();
	if (dev < 0)
		return dev;

	arg.size = sizeof(arg);
	strncpy((char *)arg.name, name, LIVEUPDATE_SESSION_NAME_LENGTH - 1);

	ret = ioctl(dev, LIVEUPDATE_IOCTL_CREATE_SESSION, &arg);
	close(dev);
	if (ret) {
		ret = -errno;
		pr_err("luo: CREATE_SESSION(%s) failed: %d\n", name, ret);
		return ret;
	}
	return arg.fd;
}

int luo_session_retrieve(const char *name)
{
	struct liveupdate_ioctl_retrieve_session arg = { 0 };
	int dev, ret;

	ret = luo_check_name(name);
	if (ret)
		return ret;

	dev = luo_dev_open();
	if (dev < 0)
		return dev;

	arg.size = sizeof(arg);
	strncpy((char *)arg.name, name, LIVEUPDATE_SESSION_NAME_LENGTH - 1);

	ret = ioctl(dev, LIVEUPDATE_IOCTL_RETRIEVE_SESSION, &arg);
	close(dev);
	if (ret) {
		ret = -errno;
		pr_err("luo: RETRIEVE_SESSION(%s) failed: %d\n", name, ret);
		return ret;
	}
	return arg.fd;
}

int luo_preserve_fd(int session_fd, int fd, uint64_t token)
{
	struct liveupdate_session_preserve_fd arg = {
		.size = sizeof(arg),
		.fd = fd,
		.token = token,
	};

	if (ioctl(session_fd, LIVEUPDATE_SESSION_PRESERVE_FD, &arg)) {
		int ret = -errno;

		pr_err("luo: PRESERVE_FD(fd=%d, token=%llu) failed: %d\n", fd, (unsigned long long)token, ret);
		return ret;
	}
	return 0;
}

int luo_retrieve_fd(int session_fd, uint64_t token)
{
	struct liveupdate_session_retrieve_fd arg = {
		.size = sizeof(arg),
		.token = token,
	};

	if (ioctl(session_fd, LIVEUPDATE_SESSION_RETRIEVE_FD, &arg)) {
		int ret = -errno;

		pr_err("luo: RETRIEVE_FD(token=%llu) failed: %d\n", (unsigned long long)token, ret);
		return ret;
	}
	return arg.fd;
}

int luo_session_finish(int session_fd)
{
	struct liveupdate_session_finish arg = {
		.size = sizeof(arg),
	};

	if (ioctl(session_fd, LIVEUPDATE_SESSION_FINISH, &arg)) {
		int ret = -errno;

		pr_err("luo: FINISH failed: %d\n", ret);
		return ret;
	}
	return 0;
}

int luo_preserve_memfd(int fd, uint64_t token)
{
	static int session_fd = -1;

	if (session_fd < 0) {
		session_fd = luo_session_create(opts.luo_session);
		if (session_fd < 0)
			return session_fd;
	}

	return luo_preserve_fd(session_fd, fd, token);
}
