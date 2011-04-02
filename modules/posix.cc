#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <climits>
#include <sysexits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/times.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <utime.h>
#include <fcntl.h>
#include <grp.h>
#include <signal.h>
#include <pty.h>
using namespace std;

#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#include <natus/natus.hpp>
using namespace natus;

#define doexc() ths.newString(strerror(errno)).toException()
#define doval(code, val) (code == 0 ? val : doexc())
#define doerr(code) return doval(code, ths.newUndefined())

static Value posix_WCOREDUMP(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "n");

	int status = arg[0].to<int>();
	return ths.newBool(WCOREDUMP(status));
}

static Value posix_WEXITSTATUS(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "n");

	int status = arg[0].to<int>();
	return ths.newNumber(WEXITSTATUS(status));
}

static Value posix_WIFCONTINUED(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "n");

	int status = arg[0].to<int>();
	return ths.newBool(WIFCONTINUED(status));
}

static Value posix_WIFEXITED(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "n");

	int status = arg[0].to<int>();
	return ths.newBool(WIFEXITED(status));
}

static Value posix_WIFSIGNALED(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "n");

	int status = arg[0].to<int>();
	return ths.newBool(WIFSIGNALED(status));
}

static Value posix_WIFSTOPPED(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "n");

	int status = arg[0].to<int>();
	return ths.newBool(WIFSTOPPED(status));
}

static Value posix_WSTOPSIG(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "n");

	int status = arg[0].to<int>();
	return ths.newNumber(WSTOPSIG(status));
}

static Value posix_WTERMSIG(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "n");

	int status = arg[0].to<int>();
	return ths.newNumber(WTERMSIG(status));
}

static Value posix_abort(Value& ths, Value& fnc, Value& arg) {
	abort();
	return ths.newUndefined();
}

static Value posix_access(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "sn");
	NATUS_CHECK_ORIGIN(ths, ("file://" + arg[0].to<UTF8>()).c_str());

	int res = access(arg[0].to<UTF8>().c_str(), arg[1].to<int>());
	if (res == 0)        return ths.newBool(true);
	if (errno == EACCES) return ths.newBool(false);
	return ths.newString(strerror(errno)).toException();
}

static Value posix_chdir(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "s");
	NATUS_CHECK_ORIGIN(ths, ("file://" + arg[0].to<UTF8>()).c_str());

	doerr(chdir(arg[0].to<UTF8>().c_str()));
}

static Value posix_chmod(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "sn");
	NATUS_CHECK_ORIGIN(ths, ("file://" + arg[0].to<UTF8>()).c_str());

	doerr(chmod(arg[0].to<UTF8>().c_str(), arg[1].to<int>()));
}

static Value posix_chown(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "snn");
	NATUS_CHECK_ORIGIN(ths, ("file://" + arg[0].to<UTF8>()).c_str());

	doerr(chown(arg[0].to<UTF8>().c_str(), arg[1].to<int>(), arg[2].to<int>()));
}

static Value posix_chroot(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "s");
	NATUS_CHECK_ORIGIN(ths, ("file://" + arg[0].to<UTF8>()).c_str());

	doerr(chroot(arg[0].to<UTF8>().c_str()));
}

static Value posix_close(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "n");

	doerr(close(arg[0].to<int>()));
}

static Value posix_ctermid(Value& ths, Value& fnc, Value& arg) {
	return ths.newString(ctermid(NULL));
}

static Value posix_dup(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "n");

	int fd = dup(arg[0].to<int>());
	if (fd < 0) return doexc();
	return ths.newNumber(fd);
}

static Value posix_dup2(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "nn");

	int fd = dup2(arg[0].to<int>(), arg[1].to<int>());
	if (fd < 0) return doexc();
	return ths.newNumber(fd);
}

static Value posix_execv(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "sa");

	const char** argv = new const char*[arg[1].get("length").to<int>()+1];
	memset(argv, 0, sizeof(char*) * (arg[1].get("length").to<int>() + 1));
	for (int i=0,j=0 ; i < arg[1].get("length").to<int>() ; i++) {
		if (!arg[1][i].isString()) continue;
		argv[j++] = arg[1][i].to<UTF8>().c_str();
	}

	execv(arg[0].to<UTF8>().c_str(), (char* const*) argv);
	delete[] argv;
	return doexc();
}

static Value posix_execve(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "sao");

	const char** argv = new const char*[arg[1].get("length").to<int>()+1];
	memset(argv, 0, sizeof(char*) * (arg[1].get("length").to<int>() + 1));
	for (int i=0,j=0 ; i < arg[1].get("length").to<int>() ; i++) {
		if (!arg[1][i].isString()) continue;
		argv[j++] = arg[1][i].to<UTF8>().c_str();
	}

	Value env = arg[2].enumerate();
	const char** envv = new const char*[env.get("length").to<int>() + 1];
	memset(envv, 0, sizeof(char*) * (env.get("length").to<int>() + 1));
	for (int i=0 ; i < env.get("length").to<int>() ; i++)
		envv[i] = strdup((env[i].to<UTF8>() + "=" + arg[2].get(env[i]).to<UTF8>()).c_str());

	execve(arg[0].to<UTF8>().c_str(), (char* const*) argv, (char* const*) envv);
	delete[] argv;
	for (int i=0 ; envv[i] ; i++)
		free((void*) envv[i]);
	delete[] envv;
	return doexc();
}

static Value posix_fchdir(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "n");

	doerr(fchdir(arg[0].to<int>()));
}

static Value posix_fchmod(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "nn");

	doerr(fchmod(arg[0].to<int>(), arg[1].to<int>()));
}

static Value posix_fchown(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "nnn");

	doerr(fchown(arg[0].to<int>(), arg[1].to<int>(), arg[2].to<int>()));
}

static Value posix_fdatasync(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "n");

	doerr(fdatasync(arg[0].to<int>()));
}

static Value posix_fork(Value& ths, Value& fnc, Value& arg) {
	pid_t pid = fork();
	if (pid == -1) return doexc();
	return ths.newNumber(pid);
}

static Value posix_forkpty(Value& ths, Value& fnc, Value& arg) {
	int amaster=0;
	pid_t pid = forkpty(&amaster, NULL, NULL, NULL);
	if (pid < 0) return doexc();

	return ths.newArrayBuilder((long) pid).newArrayBuilder((long) amaster);
}

static Value posix_fpathconf(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "nn");

	errno = 0;
	int res = fpathconf(arg[0].to<int>(), arg[1].to<int>());
	if (res == -1 && errno != 0) doexc();
	return ths.newNumber(res);
}

static Value posix_fstat(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "n");

	struct stat st;
	int res = fstat(arg[0].to<int>(), &st);
	if (res == -1) return doexc();

	Value stt = ths.newObject();
	stt.set("st_dev",     (double) st.st_dev);
	stt.set("st_ino",     (double) st.st_ino);
	stt.set("st_mode",    (double) st.st_mode);
	stt.set("st_nlink",   (double) st.st_nlink);
	stt.set("st_uid",     (double) st.st_uid);
	stt.set("st_gid",     (double) st.st_gid);
	stt.set("st_rdev",    (double) st.st_rdev);
	stt.set("st_szie",    (double) st.st_size);
	stt.set("st_blksize", (double) st.st_blksize);
	stt.set("st_blocks",  (double) st.st_blocks);
	stt.set("st_atime",   (double) st.st_atime);
	stt.set("st_mtime",   (double) st.st_mtime);
	stt.set("st_ctime",   (double) st.st_ctime);
	return stt;
}

static Value posix_fstatvfs(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "n");

	struct statvfs st;
	int res = fstatvfs(arg[0].to<int>(), &st);
	if (res == -1) return doexc();

	Value stt = ths.newObject();
	stt.set("f_bsize",   (double) st.f_bsize);
	stt.set("f_frsize",  (double) st.f_frsize);
	stt.set("f_blocks",  (double) st.f_blocks);
	stt.set("f_bfree",   (double) st.f_bfree);
	stt.set("f_bavail",  (double) st.f_bavail);
	stt.set("f_files",   (double) st.f_files);
	stt.set("f_ffree",   (double) st.f_ffree);
	stt.set("f_favail",  (double) st.f_favail);
	stt.set("f_fsid",    (double) st.f_fsid);
	stt.set("f_flag",    (double) st.f_flag);
	stt.set("f_namemax", (double) st.f_namemax);
	return stt;
}

static Value posix_fsync(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "n");

	doerr(fsync(arg[0].to<int>()));
}

static Value posix_ftruncate(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "nn");

	doerr(ftruncate(arg[0].to<int>(), arg[1].to<int>()));
}

static Value posix_getcwd(Value& ths, Value& fnc, Value& arg) {
	char *cwd = getcwd(NULL, 0);
	if (!cwd) return doexc();
	string scwd = cwd;
	free(cwd);
	return ths.newString(scwd);
}

static Value posix_getegid(Value& ths, Value& fnc, Value& arg) {
	return ths.newNumber(getegid());
}

static Value posix_geteuid(Value& ths, Value& fnc, Value& arg) {
	return ths.newNumber(geteuid());
}

static Value posix_getgid(Value& ths, Value& fnc, Value& arg) {
	return ths.newNumber(getgid());
}

static Value posix_getgroups(Value& ths, Value& fnc, Value& arg) {
	gid_t size = getgroups(0, NULL);

	gid_t* gids = new gid_t[size];
	if (getgroups(size, gids) < 0)
		return doexc();

	Value ret = ths.newArray();
	for (gid_t i=0 ; i < size ; i++)
		ret.newArrayBuilder((long) i);
	delete[] gids;
	return ret;
}

static Value posix_getloadavg(Value& ths, Value& fnc, Value& arg) {
	double ldavg[3];
	if (getloadavg(ldavg, 3) < 0)
		return ths.newString("Unknown error!").toException();

	return ths.newArrayBuilder(ldavg[0]).newArrayBuilder(ldavg[1]).newArrayBuilder(ldavg[2]);
}

static Value posix_getlogin(Value& ths, Value& fnc, Value& arg) {
	const char* name = getlogin();
	if (!name) return doexc();
	return ths.newString(name);
}

static Value posix_getpgid(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "n");

	pid_t pgid = getpgid(arg[0].to<int>());
	if (pgid < 0) doexc();
	return ths.newNumber(pgid);
}

static Value posix_getpgrp(Value& ths, Value& fnc, Value& arg) {
	return ths.newNumber(getpgrp());
}

static Value posix_getpid(Value& ths, Value& fnc, Value& arg) {
	return ths.newNumber(getpid());
}

static Value posix_getppid(Value& ths, Value& fnc, Value& arg) {
	return ths.newNumber(getppid());
}

static Value posix_getresgid(Value& ths, Value& fnc, Value& arg) {
	gid_t rgid, egid, sgid;
	if (getresgid(&rgid, &egid, &sgid) < 0)
		return doexc();

	return ths.newArrayBuilder((long) rgid).newArrayBuilder((long) egid).newArrayBuilder((long) sgid);
}

static Value posix_getresuid(Value& ths, Value& fnc, Value& arg) {
	uid_t ruid, euid, suid;
	if (getresgid(&ruid, &euid, &suid) < 0)
		return doexc();

	return ths.newArrayBuilder((long) ruid).newArrayBuilder((long) euid).newArrayBuilder((long) suid);
}

static Value posix_getsid(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "n");

	pid_t sid = getsid(arg[0].to<int>());
	if (sid < 0) doexc();
	return ths.newNumber(sid);
}

static Value posix_getuid(Value& ths, Value& fnc, Value& arg) {
	return ths.newNumber(getuid());
}

static Value posix_initgroups(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "sn");

	doerr(initgroups(arg[0].to<UTF8>().c_str(), arg[1].to<int>()));
}

static Value posix_isatty(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "n");

	return ths.newBool(isatty(arg[0].to<int>()));
}

static Value posix_kill(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "nn");

	doerr(kill(arg[0].to<int>(), arg[0].to<int>()));
}

static Value posix_killpg(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "nn");

	doerr(kill(arg[0].to<int>(), arg[0].to<int>()));
}

static Value posix_lchown(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "snn");

	doerr(lchown(arg[0].to<UTF8>().c_str(), arg[1].to<int>(), arg[2].to<int>()));
}

static Value posix_link(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "ss");

	doerr(link(arg[0].to<UTF8>().c_str(), arg[1].to<UTF8>().c_str()));
}

static Value posix_lseek(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "nnn");

	off_t offset = lseek(arg[0].to<int>(), arg[1].to<int>(), arg[2].to<int>());
	if (offset < 0) return doexc();
	return ths.newNumber(offset);
}

static Value posix_lstat(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "s");

	struct stat st;
	int res = lstat(arg[0].to<UTF8>().c_str(), &st);
	if (res == -1) return doexc();

	Value stt = ths.newObject();
	stt.set("st_dev",     (double) st.st_dev);
	stt.set("st_ino",     (double) st.st_ino);
	stt.set("st_mode",    (double) st.st_mode);
	stt.set("st_nlink",   (double) st.st_nlink);
	stt.set("st_uid",     (double) st.st_uid);
	stt.set("st_gid",     (double) st.st_gid);
	stt.set("st_rdev",    (double) st.st_rdev);
	stt.set("st_szie",    (double) st.st_size);
	stt.set("st_blksize", (double) st.st_blksize);
	stt.set("st_blocks",  (double) st.st_blocks);
	stt.set("st_atime",   (double) st.st_atime);
	stt.set("st_mtime",   (double) st.st_mtime);
	stt.set("st_ctime",   (double) st.st_ctime);
	return stt;
}

static Value posix_major(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "n");

	return ths.newNumber(major(arg[0].to<int>()));
}

static Value posix_makedev(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "nn");

	return ths.newNumber(makedev(arg[0].to<int>(), arg[0].to<int>()));
}

static Value posix_minor(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "n");

	return ths.newNumber(minor(arg[0].to<int>()));
}

static Value posix_mkdir(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "s");
	NATUS_CHECK_ORIGIN(ths, ("file://" + arg[0].to<UTF8>()).c_str());

	mode_t mode = 0777;
	if (arg.get("length").to<int>() > 1) {
		if (!arg[1].isNumber())
			return ths.newString("mode must be a number!").toException();
		mode = arg[1].to<int>();
	}
	doerr(mkdir(arg[0].to<UTF8>().c_str(), mode));
}

static Value posix_mkfifo(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "s");
	NATUS_CHECK_ORIGIN(ths, ("file://" + arg[0].to<UTF8>()).c_str());

	mode_t mode = 0666;
	if (arg.get("length").to<int>() > 1) {
		if (!arg[1].isNumber())
			return ths.newString("mode must be a number!").toException();
		mode = arg[1].to<int>();
	}
	doerr(mkfifo(arg[0].to<UTF8>().c_str(), mode));
}

static Value posix_mknod(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "s");
	NATUS_CHECK_ORIGIN(ths, ("file://" + arg[0].to<UTF8>()).c_str());

	mode_t mode = 0666;
	dev_t  dev = 0;
	if (arg.get("length").to<int>() > 1) {
		if (!arg[1].isNumber())
			return ths.newString("mode must be a number!").toException();
		mode = arg[1].to<int>();
		if (arg.get("length").to<int>() > 2) {
			if (!arg[2].isNumber())
				return ths.newString("dev must be a number!").toException();
			dev = arg[2].to<int>();
		}
	}
	doerr(mknod(arg[0].to<UTF8>().c_str(), mode, dev));
}

static Value posix_nice(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "n");

	errno = 0;
	int prio = nice(arg[0].to<int>());
	if (errno != 0) return doexc();
	return ths.newNumber(prio);
}

static Value posix_open(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "s|nn");
	NATUS_CHECK_ORIGIN(ths, ("file://" + arg[0].to<UTF8>()).c_str());

	int    flags = 0;
	mode_t mode  = 0666;
	if (arg.get("length").to<int>() > 1) {
		if (!arg[1].isNumber())
			return ths.newString("flags must be a number!").toException();
		flags = arg[1].to<int>();
		if (arg.get("length").to<int>() > 2) {
			if (!arg[2].isNumber())
				return ths.newString("mode must be a number!").toException();
			mode = arg[2].to<int>();
		}
	}
	int fd = open(arg[0].to<UTF8>().c_str(), flags, mode);
	if (fd < 0) return doexc();
	return ths.newNumber(fd);
}

static Value posix_openpty(Value& ths, Value& fnc, Value& arg) {
	int amaster, aslave;
	if (openpty(&amaster, &aslave, NULL, NULL, NULL) < 0)
		return doexc();

	return ths.newArrayBuilder((long) amaster).newArrayBuilder((long) aslave);
}

static Value posix_pathconf(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "sn");
	NATUS_CHECK_ORIGIN(ths, ("file://" + arg[0].to<UTF8>()).c_str());

	errno = 0;
	int res = pathconf(arg[0].to<UTF8>().c_str(), arg[1].to<int>());
	if (res == -1 && errno != 0) doexc();
	return ths.newNumber(res);
}

static Value posix_pipe(Value& ths, Value& fnc, Value& arg) {
	int fds[2];
	if (pipe(fds) < 0) return doexc();

	return ths.newArrayBuilder((long) fds[0]).newArrayBuilder((long) fds[1]);
}

static Value posix_read(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "nn");

	char* buffer = new char[arg[1].to<int>()];
	if (read(arg[0].to<int>(), buffer, arg[1].to<int>()) < 0)
		return doexc();
	string str = buffer;
	delete[] buffer;
	return ths.newString(str);
}

static Value posix_readlink(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "s");
	NATUS_CHECK_ORIGIN(ths, ("file://" + arg[0].to<UTF8>()).c_str());

	char buffer[4096];
	memset(buffer, 0, sizeof(char) * 4096);
	ssize_t rd = readlink(arg[0].to<UTF8>().c_str(), buffer, 4096);
	if (rd < 0) return doexc();
	return ths.newString(string(buffer, rd));
}

static Value posix_rename(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "ss");
	NATUS_CHECK_ORIGIN(ths, ("file://" + arg[0].to<UTF8>()).c_str());
	NATUS_CHECK_ORIGIN(ths, ("file://" + arg[1].to<UTF8>()).c_str());

	doerr(rename(arg[0].to<UTF8>().c_str(), arg[1].to<UTF8>().c_str()));
}

static Value posix_rmdir(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "s");
	NATUS_CHECK_ORIGIN(ths, ("file://" + arg[0].to<UTF8>()).c_str());

	doerr(rmdir(arg[0].to<UTF8>().c_str()));
}

static Value posix_setegid(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "n");

	doerr(setegid(arg[0].to<int>()));
}

static Value posix_seteuid(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "n");

	doerr(seteuid(arg[0].to<int>()));
}

static Value posix_setgid(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "n");

	doerr(setgid(arg[0].to<int>()));
}

static Value posix_setgroups(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "a");

	long    len = arg[0].get("length").to<int>();
	gid_t* list = new gid_t[len];
	for (int i=0 ; i < len ; i++)
		list[i] = arg[0][i].to<int>();
	if (setgroups(len, list) < 0) {
		delete[] list;
		return doexc();
	}
	delete[] list;
	return ths.newUndefined();
}

static Value posix_setpgid(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "nn");

	doerr(setpgid(arg[0].to<int>(), arg[1].to<int>()));
}

static Value posix_setpgrp(Value& ths, Value& fnc, Value& arg) {
	doerr(setpgrp());
}

static Value posix_setregid(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "nn");

	doerr(setregid(arg[0].to<int>(), arg[1].to<int>()));
}

static Value posix_setresgid(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "nnn");

	doerr(setresgid(arg[0].to<int>(), arg[1].to<int>(), arg[2].to<int>()));
}

static Value posix_setresuid(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "nnn");

	doerr(setresuid(arg[0].to<int>(), arg[1].to<int>(), arg[2].to<int>()));
}

static Value posix_setreuid(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "nn");

	doerr(setreuid(arg[0].to<int>(), arg[1].to<int>()));
}

static Value posix_setsid(Value& ths, Value& fnc, Value& arg) {
	pid_t sid = setsid();
	if (sid < 0) doexc();
	return ths.newNumber(sid);
}

static Value posix_setuid(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "n");

	doerr(setuid(arg[0].to<int>()));
}

static Value posix_stat(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "s");
	NATUS_CHECK_ORIGIN(ths, ("file://" + arg[0].to<UTF8>()).c_str());

	struct stat st;
	int res = stat(arg[0].to<UTF8>().c_str(), &st);
	if (res == -1) return doexc();

	Value stt = ths.newObject();
	stt.set("st_dev",     (double) st.st_dev);
	stt.set("st_ino",     (double) st.st_ino);
	stt.set("st_mode",    (double) st.st_mode);
	stt.set("st_nlink",   (double) st.st_nlink);
	stt.set("st_uid",     (double) st.st_uid);
	stt.set("st_gid",     (double) st.st_gid);
	stt.set("st_rdev",    (double) st.st_rdev);
	stt.set("st_szie",    (double) st.st_size);
	stt.set("st_blksize", (double) st.st_blksize);
	stt.set("st_blocks",  (double) st.st_blocks);
	stt.set("st_atime",   (double) st.st_atime);
	stt.set("st_mtime",   (double) st.st_mtime);
	stt.set("st_ctime",   (double) st.st_ctime);
	return stt;
}

static Value posix_statvfs(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "s");
	NATUS_CHECK_ORIGIN(ths, ("file://" + arg[0].to<UTF8>()).c_str());

	struct statvfs st;
	int res = statvfs(arg[0].to<UTF8>().c_str(), &st);
	if (res == -1) return doexc();

	Value stt = ths.newObject();
	stt.set("f_bsize",   (double) st.f_bsize);
	stt.set("f_frsize",  (double) st.f_frsize);
	stt.set("f_blocks",  (double) st.f_blocks);
	stt.set("f_bfree",   (double) st.f_bfree);
	stt.set("f_bavail",  (double) st.f_bavail);
	stt.set("f_files",   (double) st.f_files);
	stt.set("f_ffree",   (double) st.f_ffree);
	stt.set("f_favail",  (double) st.f_favail);
	stt.set("f_fsid",    (double) st.f_fsid);
	stt.set("f_flag",    (double) st.f_flag);
	stt.set("f_namemax", (double) st.f_namemax);
	return stt;
}

static Value posix_strerror(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "n");

	return ths.newString(strerror(arg[0].to<int>()));
}

static Value posix_symlink(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "ss");
	NATUS_CHECK_ORIGIN(ths, ("file://" + arg[0].to<UTF8>()).c_str());
	NATUS_CHECK_ORIGIN(ths, ("file://" + arg[1].to<UTF8>()).c_str());

	doerr(symlink(arg[0].to<UTF8>().c_str(), arg[1].to<UTF8>().c_str()));
}

static Value posix_sysconf(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "n");

	long res = sysconf(arg[0].to<int>());
	if (res < 0) return doexc();
	return ths.newNumber(res);
}

static Value posix_system(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "(sn)");
	NATUS_CHECK_ORIGIN(ths, "file:///");

	int res;
	if (arg[0].isString())
		res = system(arg[0].to<UTF8>().c_str());
	else
		res = system(NULL);
	if (res < 0) return doexc();
	return ths.newNumber(res);
}

static Value posix_tcgetpgrp(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "n");

	pid_t res = tcgetpgrp(arg[0].to<int>());
	if (res < 0) return doexc();
	return ths.newNumber(res);
}

static Value posix_tcsetpgrp(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "nn");

	doerr(tcsetpgrp(arg[0].to<int>(), arg[1].to<int>()));
}

static Value posix_tempnam(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "s|ss");

	string dir = "";
	string prefix = "";
	if (arg.get("length").to<int>() > 0) {
		dir = arg[0].to<UTF8>();
		if (arg.get("length").to<int>() > 1)
			prefix = arg[1].to<UTF8>();
	}
	char* name = tempnam(dir.c_str(), prefix.c_str());
	if (!name) return doexc();
	string nm = name;
	free(name);
	return ths.newString(nm);
}

static Value posix_times(Value& ths, Value& fnc, Value& arg) {
	struct tms t;
	clock_t c = times(&t);
	if (c == -1) return doexc();

	Value res = ths.newObject();
	res.set("tms_utime",  (double) t.tms_utime);
	res.set("tms_stime",  (double) t.tms_stime);
	res.set("tms_cutime", (double) t.tms_utime);
	res.set("tms_cstime", (double) t.tms_stime);
	res.set("tms_ticks",  (double) c);
	return res;
}

static Value posix_tmpnam(Value& ths, Value& fnc, Value& arg) {
	const char* name = tmpnam(NULL);
	if (!name) return ths.newNull();
	return ths.newString(name);
}

static Value posix_ttyname(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "n");

	const char* name = ttyname(arg[0].to<int>());
	if (!name) return doexc();
	return ths.newString(name);
}

static Value posix_umask(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "n");

	return ths.newNumber(umask(arg[0].to<int>()));
}

static Value posix_uname(Value& ths, Value& fnc, Value& arg) {
	struct utsname n;
	if (uname(&n) < 0) return doexc();

	Value res = ths.newObject();
	res.set("sysname",  n.sysname);
	res.set("nodename", n.nodename);
	res.set("release",  n.release);
	res.set("version",  n.version);
	res.set("machine",  n.machine);
	return res;
}

static Value posix_unlink(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "s");
	NATUS_CHECK_ORIGIN(ths, ("file://" + arg[0].to<UTF8>()).c_str());

	doerr(unlink(arg[0].to<UTF8>().c_str()));
}

static Value posix_utime(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "s(nN)|n");
	NATUS_CHECK_ORIGIN(ths, ("file://" + arg[0].to<UTF8>()).c_str());

	if (arg[1].isNull())
		doerr(utime(arg[0].to<UTF8>().c_str(), NULL));

	_exc = ensureArguments(arg, "snn");
	if (_exc.isException()) return _exc;

	struct utimbuf buf = {
		arg[1].to<int>(),
		arg[2].to<int>(),
	};
	doerr(utime(arg[0].to<UTF8>().c_str(), &buf));
}

static Value posix_wait(Value& ths, Value& fnc, Value& arg) {
	int status;
	pid_t pid = wait(&status);
	if (pid < 0) return doexc();
	return ths.newArrayBuilder((long) pid).newArrayBuilder((long) status);
}

static Value posix_waitpid(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "nn");

	int status;
	pid_t pid = waitpid(arg[0].to<int>(), &status, arg[1].to<int>());
	if (pid < 0) return doexc();

	return ths.newArrayBuilder((long) pid).newArrayBuilder((long) status);
}

static Value posix_write(Value& ths, Value& fnc, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "ns");

	string str = arg[1].to<UTF8>();
	ssize_t size = write(arg[0].to<int>(), str.c_str(), str.length());
	if (size < 0) return doexc();
	return ths.newNumber(size);
}

#define OK(x) ok = (!x.isException()) || ok
#define NCONST(macro) OK(base.setRecursive("exports." # macro, (long) macro))
#define NFUNC(func) OK(base.setRecursive("exports." # func, posix_ ## func))

extern "C" bool NATUS_MODULE_INIT(ntValue* module) {
	Value base(module, false);
	bool ok = false;

	// Functions
	NFUNC(WCOREDUMP);
	NFUNC(WEXITSTATUS);
	NFUNC(WIFCONTINUED);
	NFUNC(WIFEXITED);
	NFUNC(WIFSIGNALED);
	NFUNC(WIFSTOPPED);
	NFUNC(WSTOPSIG);
	NFUNC(WTERMSIG);
	NFUNC(abort);
	NFUNC(access);
	NFUNC(chdir);
	NFUNC(chmod);
	NFUNC(chown);
	NFUNC(chroot);
	NFUNC(close);
	NFUNC(ctermid);
	NFUNC(dup);
	NFUNC(dup2);
	NFUNC(execv);
	NFUNC(execve);
	NFUNC(fchdir);
	NFUNC(fchmod);
	NFUNC(fchown);
	NFUNC(fdatasync);
	NFUNC(fork);
	NFUNC(forkpty);
	NFUNC(fpathconf);
	NFUNC(fstat);
	NFUNC(fstatvfs);
	NFUNC(fsync);
	NFUNC(ftruncate);
	NFUNC(getcwd);
	NFUNC(getegid);
	NFUNC(geteuid);
	NFUNC(getgid);
	NFUNC(getgroups);
	NFUNC(getloadavg);
	NFUNC(getlogin);
	NFUNC(getpgid);
	NFUNC(getpgrp);
	NFUNC(getpid);
	NFUNC(getppid);
	NFUNC(getresgid);
	NFUNC(getresuid);
	NFUNC(getsid);
	NFUNC(getuid);
	NFUNC(initgroups);
	NFUNC(isatty);
	NFUNC(kill);
	NFUNC(killpg);
	NFUNC(lchown);
	NFUNC(link);
	NFUNC(lseek);
	NFUNC(lstat);
	NFUNC(major);
	NFUNC(makedev);
	NFUNC(minor);
	NFUNC(mkdir);
	NFUNC(mkfifo);
	NFUNC(mknod);
	NFUNC(nice);
	NFUNC(open);
	NFUNC(openpty);
	NFUNC(pathconf);
	NFUNC(pipe);
	NFUNC(read);
	NFUNC(readlink);
	NFUNC(rename);
	NFUNC(rmdir);
	NFUNC(setegid);
	NFUNC(seteuid);
	NFUNC(setgid);
	NFUNC(setgroups);
	NFUNC(setpgid);
	NFUNC(setpgrp);
	NFUNC(setregid);
	NFUNC(setresgid);
	NFUNC(setresuid);
	NFUNC(setegid);
	NFUNC(setresuid);
	NFUNC(setreuid);
	NFUNC(setsid);
	NFUNC(setuid);
	NFUNC(stat);
	NFUNC(statvfs);
	NFUNC(strerror);
	NFUNC(symlink);
	NFUNC(sysconf);
	NFUNC(system);
	NFUNC(tcgetpgrp);
	NFUNC(tcsetpgrp);
	NFUNC(tempnam);
	NFUNC(times);
	NFUNC(tmpnam);
	NFUNC(ttyname);
	NFUNC(umask);
	NFUNC(uname);
	NFUNC(unlink);
	NFUNC(utime);
	NFUNC(wait);
	NFUNC(waitpid);
	NFUNC(write);

	// Constants
	NCONST(EX_CANTCREAT);
	NCONST(EX_CONFIG);
	NCONST(EX_DATAERR);
	NCONST(EX_IOERR);
	NCONST(EX_NOHOST);
	NCONST(EX_NOINPUT);
	NCONST(EX_NOPERM);
	NCONST(EX_NOUSER);
	NCONST(EX_OK);
	NCONST(EX_OSERR);
	NCONST(EX_OSFILE);
	NCONST(EX_PROTOCOL);
	NCONST(EX_SOFTWARE);
	NCONST(EX_TEMPFAIL);
	NCONST(EX_UNAVAILABLE);
	NCONST(EX_USAGE);
	NCONST(F_OK);
	NCONST(NGROUPS_MAX);
	NCONST(O_APPEND);
	NCONST(O_ASYNC);
	NCONST(O_CREAT);
	NCONST(O_DIRECT);
	NCONST(O_DIRECTORY);
	NCONST(O_DSYNC);
	NCONST(O_EXCL);
	NCONST(O_LARGEFILE);
	NCONST(O_NDELAY);
	NCONST(O_NOATIME);
	NCONST(O_NOCTTY);
	NCONST(O_NOFOLLOW);
	NCONST(O_NONBLOCK);
	NCONST(O_RDONLY);
	NCONST(O_RDWR);
	NCONST(O_RSYNC);
	NCONST(O_SYNC);
	NCONST(O_TRUNC);
	NCONST(O_WRONLY);
	NCONST(R_OK);
	NCONST(ST_APPEND);
	NCONST(ST_MANDLOCK);
	NCONST(ST_NOATIME);
	NCONST(ST_NODEV);
	NCONST(ST_NODIRATIME);
	NCONST(ST_NOEXEC);
	NCONST(ST_NOSUID);
	NCONST(ST_RDONLY);
	NCONST(ST_RELATIME);
	NCONST(ST_SYNCHRONOUS);
	NCONST(ST_WRITE);
	NCONST(TMP_MAX);
	NCONST(WCONTINUED);
	NCONST(WNOHANG);
	NCONST(WUNTRACED);
	NCONST(W_OK);

	return ok;
}
