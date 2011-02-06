#include <sstream>
#include <iostream>

#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <asm-generic/socket.h>
using namespace std;

#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#include <natus/natus.h>
using namespace natus;

static Value socket_from_sock(Value& ctx, int sock, int domain, int type, int protocol);

static Value throwexc_eai(const Value& ctx, int error) {
	const char* type = "IOError";
	switch (error) {
	case EAI_SYSTEM:
		return throwException(ctx, errno);
	case EAI_MEMORY:
		return NULL;
	case EAI_ADDRFAMILY:
	case EAI_AGAIN:
	case EAI_BADFLAGS:
	case EAI_FAIL:
	case EAI_FAMILY:
	case EAI_NODATA:
	case EAI_NONAME:
	case EAI_SERVICE:
	case EAI_SOCKTYPE:
	default:
		break;
	}

	return throwException(ctx, type, gai_strerror(error), error);
}

class SocketClass : public Class {
	virtual Class::Flags getFlags() {
		return Class::FlagGetProperty;
	}

	virtual Value get(Value& obj, string key) {
		sockaddr_storage addr;
		socklen_t len = sizeof(sockaddr_storage);
		char name[1024], port[21];
		int status = 0;

		int fd = (long) obj.getPrivate("posix.fd");

		if (key == "remoteAddress" || key == "remotePort")
			status = getpeername(fd, (sockaddr*) &addr, &len);
		else if (key == "localAddress" || key == "localPort")
			status = getsockname(fd, (sockaddr*) &addr, &len);
		else
			return obj.newUndefined().toException(); // Don't intercept on other properties
		if (status < 0)
			return throwException(obj, errno);

		status = getnameinfo((sockaddr*) &addr, len, name, 1024, port, 21, NI_NUMERICHOST | NI_NUMERICSERV);
		if (status < 0)
			return throwexc_eai(obj, status);

		if (key.find("Address") != string::npos)
			return obj.newString(name);
		return obj.newNumber(atoi(port));
	}
};

static Value socket_accept(Value& ths, Value& fnc, vector<Value>& arg) {
	int fd = (long) ths.getPrivate("posix.fd");

	int newsock = accept(fd, NULL, NULL);
	if (newsock < 0) return throwException(ths, errno);
	return socket_from_sock(ths, newsock, ths.get("domain").toInt(), ths.get("type").toInt(), ths.get("protocol").toInt());
}

static Value socket_bind(Value& ths, Value& fnc, vector<Value>& arg) {
	Value exc = checkArguments(ths, arg, "|s(sn)");
	if (exc.isException()) return exc;

	int    fd = (long) ths.getPrivate("posix.fd");
	string ip = "0.0.0.0";
	string port;

	if (arg.size() > 0) {
		ip = arg[0].toString();
		if (arg.size() > 1)
			port = arg[1].toString();
	}

	struct addrinfo* ai = NULL;
	int status = getaddrinfo(ip.c_str(), port.empty() ? NULL : port.c_str(), NULL, &ai);
	if (status != 0) {
		freeaddrinfo(ai);
		return throwexc_eai(ths, status);
	}
	if (bind(fd, ai->ai_addr, ai->ai_addrlen) < 0) {
		freeaddrinfo(ai);
		return throwException(ths, errno);
	}
	freeaddrinfo(ai);
	return ths.newUndefined();
}

static Value socket_connect(Value& ths, Value& fnc, vector<Value>& arg) {
	Value exc = checkArguments(ths, arg, "s(sn)");
	if (exc.isException()) return exc;

	int fd = (long) ths.getPrivate("posix.fd");

	struct addrinfo* ai = NULL;
	int status = getaddrinfo(arg[0].toString().c_str(), arg[1].toString().c_str(), NULL, &ai);
	if (status != 0) {
		freeaddrinfo(ai);
		return throwexc_eai(ths, status);
	}
	if (connect(fd, ai->ai_addr, ai->ai_addrlen) < 0) {
		freeaddrinfo(ai);
		return throwException(ths, errno);
	}
	freeaddrinfo(ai);
	return ths.newUndefined();
}

static Value socket_listen(Value& ths, Value& fnc, vector<Value>& arg) {
	Value exc = checkArguments(ths, arg, "|n");
	if (exc.isException()) return exc;

	int fd = (long) ths.getPrivate("posix.fd");
	if (listen(fd, arg.size() > 0 ? arg[0].toInt() : 1024) < 0)
		return throwException(ths, errno);
	return ths.newUndefined();
}

static Value fd_read(Value& ths, Value& fnc, vector<Value>& arg) {
	Value exc = checkArguments(ths, arg, "|n");
	if (exc.isException()) return exc;

	int fd = (long) ths.getPrivate("posix.fd");

	int bs = arg.size() > 0 ? arg[0].toInt() : 1024;
	char *buff = new char[bs];
	ssize_t rcvd = read(fd, buff, bs);
	if (rcvd < 0) {
		delete[] buff;
		return throwException(ths, errno);
	}
	string ret = string(buff, rcvd);
	delete[] buff;
	return ths.newString(ret);
}

static Value socket_receive(Value& ths, Value& fnc, vector<Value>& arg) {
	Value exc = checkArguments(ths, arg, "|n");
	if (exc.isException()) return exc;

	int fd = (long) ths.getPrivate("posix.fd");

	int bs = arg.size() > 0 ? arg[0].toInt() : 1024;
	char *buff = new char[bs];
	ssize_t rcvd = recv(fd, buff, bs, 0);
	if (rcvd < 0) {
		delete[] buff;
		return throwException(ths, errno);
	}
	string ret = string(buff, rcvd);
	delete[] buff;
	return ths.newString(ret);
}

static Value socket_send(Value& ths, Value& fnc, vector<Value>& arg) {
	Value exc = checkArguments(ths, arg, "s");
	if (exc.isException()) return exc;

	int fd = (long) ths.getPrivate("posix.fd");

	string buff = arg[0].toString();
	ssize_t snt = send(fd, buff.c_str(), buff.length(), 0);
	if (snt < 0) return throwException(ths, errno);
	return ths.newNumber(snt);
}

static Value socket_shutdown(Value& ths, Value& fnc, vector<Value>& arg) {
	Value exc = checkArguments(ths, arg, "|n");
	if (exc.isException()) return exc;

	int fd = (long) ths.getPrivate("posix.fd");
	if (shutdown(fd, arg.size() > 0 ? arg[0].toInt() : SHUT_RDWR) < 0)
		return throwException(ths, errno);
	return ths.newUndefined();
}

static Value fd_write(Value& ths, Value& fnc, vector<Value>& arg) {
	Value exc = checkArguments(ths, arg, "s");
	if (exc.isException()) return exc;

	int fd = (long) ths.getPrivate("posix.fd");
	string buff = arg[0].toString();
	ssize_t snt = write(fd, buff.c_str(), buff.length());
	if (snt < 0) return throwException(ths, errno);
	return ths.newNumber(snt);
}

static Value fd_close(Value& ths, Value& fnc, vector<Value>& arg) {
	int fd = (long) ths.getPrivate("posix.fd");
	if (close(fd) < 0)
		return throwException(ths, errno);
	return ths.newUndefined();
}

static Value socket_ctor(Value& ths, Value& fnc, vector<Value>& arg) {
	Value exc = checkArguments(ths, arg, "|nnn");
	if (exc.isException()) return exc;

	int domain = AF_INET, type = SOCK_STREAM, prot = 0;
	if (arg.size() > 0) {
		domain = arg[0].toInt();
		if (arg.size() > 1) {
			type = arg[1].toInt();
			if (arg.size() > 2)
				prot = arg[2].toInt();
		}
	}

	int fd = socket(domain, type, prot);
	if (fd < 0) return throwException(ths, errno);
	return socket_from_sock(ths, fd, domain, type, prot);
}

static Value socket_from_sock(Value& ctx, int sock, int domain, int type, int protocol) {
	Value obj = ctx.newObject(new SocketClass);
	obj.setPrivate("posix.fd", (void*) sock);
	obj.set("accept",        socket_accept);
	obj.set("bind",          socket_bind);
	obj.set("connect",       socket_connect);
	obj.set("close",         fd_close);
	obj.set("listen",        socket_listen);
	obj.set("read",          fd_read);
	obj.set("receive",       socket_receive);
	obj.set("recv",          socket_receive);
	obj.set("send",          socket_send);
	obj.set("shutdown",      socket_shutdown);
	obj.set("write",         fd_write);
	obj.set("isConnected",   false);
	obj.set("isReadable",    false);
	obj.set("isWritable",    false);
	obj.set("localAddress",  "");
	obj.set("localPort",     -1);
	obj.set("remoteAddress", "");
	obj.set("remotePort",    -1);
	obj.set("domain",        domain);
	obj.set("type",          type);
	obj.set("protocol",      protocol);
	return obj;
}

#define OK(x) ok = (x) || ok
#define NCONST(macro) OK(base.set("exports." # macro, macro))

extern "C" bool natus_require(Value& base) {
	bool ok = false;

	// Objects
	OK(base.set("exports.Socket", socket_ctor));

	// Constants
	NCONST(AF_APPLETALK);
	NCONST(AF_ASH);
	NCONST(AF_ATMPVC);
	NCONST(AF_ATMSVC);
	NCONST(AF_AX25);
	NCONST(AF_BRIDGE);
	NCONST(AF_DECnet);
	NCONST(AF_ECONET);
	NCONST(AF_INET);
	NCONST(AF_INET6);
	NCONST(AF_IPX);
	NCONST(AF_IRDA);
	NCONST(AF_KEY);
	NCONST(AF_LLC);
	NCONST(AF_NETBEUI);
	NCONST(AF_NETLINK);
	NCONST(AF_NETROM);
	NCONST(AF_PACKET);
	NCONST(AF_PPPOX);
	NCONST(AF_ROSE);
	NCONST(AF_ROUTE);
	NCONST(AF_SECURITY);
	NCONST(AF_SNA);
	NCONST(AF_TIPC);
	NCONST(AF_UNIX);
	NCONST(AF_UNSPEC);
	NCONST(AF_WANPIPE);
	NCONST(AF_X25);
	NCONST(SOCK_DGRAM);
	NCONST(SOCK_RAW);
	NCONST(SOCK_RDM);
	NCONST(SOCK_SEQPACKET);
	NCONST(SOCK_STREAM);
	NCONST(SHUT_RD);
	NCONST(SHUT_RDWR);
	NCONST(SHUT_WR);
	NCONST(SOL_IP);
	NCONST(SOL_SOCKET);
	NCONST(SOL_TCP);
	NCONST(SOL_UDP);
	NCONST(SO_DEBUG);
	NCONST(SO_REUSEADDR);
	NCONST(SO_TYPE);
	NCONST(SO_ERROR);
	NCONST(SO_DONTROUTE);
	NCONST(SO_BROADCAST);
	NCONST(SO_SNDBUF);
	NCONST(SO_RCVBUF);
	NCONST(SO_SNDBUFFORCE);
	NCONST(SO_RCVBUFFORCE);
	NCONST(SO_KEEPALIVE);
	NCONST(SO_OOBINLINE);
	NCONST(SO_NO_CHECK);
	NCONST(SO_PRIORITY);
	NCONST(SO_LINGER);
	NCONST(SO_BSDCOMPAT);
	NCONST(SO_PASSCRED);
	NCONST(SO_PEERCRED);
	NCONST(SO_RCVLOWAT);
	NCONST(SO_SNDLOWAT);
	NCONST(SO_RCVTIMEO);
	NCONST(SO_SNDTIMEO);
	NCONST(SO_SECURITY_AUTHENTICATION);
	NCONST(SO_SECURITY_ENCRYPTION_TRANSPORT);
	NCONST(SO_SECURITY_ENCRYPTION_NETWORK);
	NCONST(SO_BINDTODEVICE);
	NCONST(SO_ATTACH_FILTER);
	NCONST(SO_DETACH_FILTER);
	NCONST(SO_PEERNAME);
	NCONST(SO_TIMESTAMP);
	NCONST(SO_ACCEPTCONN);
	NCONST(SO_PEERSEC);
	NCONST(SO_PASSSEC);
	NCONST(SO_TIMESTAMPNS);
	NCONST(SO_MARK);
	NCONST(SO_TIMESTAMPING);
	NCONST(SO_PROTOCOL);
	NCONST(SO_DOMAIN);
	NCONST(SO_RXQ_OVFL);
	NCONST(TCP_CORK);
	NCONST(TCP_KEEPIDLE);
	NCONST(TCP_KEEPINTVL);
	NCONST(TCP_KEEPCNT);
	NCONST(TCP_SYNCNT);
	NCONST(TCP_LINGER2);
	NCONST(TCP_DEFER_ACCEPT);
	NCONST(TCP_WINDOW_CLAMP);
	NCONST(TCP_INFO);
	NCONST(TCP_CONGESTION);
	NCONST(TCP_MD5SIG);
	NCONST(TCP_MD5SIG_MAXKEYLEN);

	return ok;
}
