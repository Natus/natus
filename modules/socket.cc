#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#ifdef __linux__
#include <asm-generic/socket.h>
#endif
using namespace std;

#include "iocommon.hpp"

#define PRIV_POSIX_FD "posix::fd"

static Value socket_from_sock(Value& ctx, int sock, int domain, int type, int protocol);

static Value throwExceptionEAI(const Value& ctx, int error) {
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
		return Class::FlagGet;
	}

	virtual Value get(Value& obj, Value& key) {
		sockaddr_storage addr;
		socklen_t len = sizeof(sockaddr_storage);
		char name[1024], port[21];
		int status = 0;

		int fd = obj.getPrivate<long>(PRIV_POSIX_FD);

		if (key.to<UTF8>() == "remoteAddress" || key.to<UTF8>() == "remotePort")
			status = getpeername(fd, (sockaddr*) &addr, &len);
		else if (key.to<UTF8>() == "localAddress" || key.to<UTF8>() == "localPort")
			status = getsockname(fd, (sockaddr*) &addr, &len);
		else
			return obj.newUndefined().toException(); // Don't intercept on other properties
		if (status < 0)
			return throwException(obj, errno);

		status = getnameinfo((sockaddr*) &addr, len, name, 1024, port, 21, NI_NUMERICHOST | NI_NUMERICSERV);
		if (status < 0)
			return throwExceptionEAI(obj, status);

		if (key.to<UTF8>().find("Address") != string::npos)
			return obj.newString(name);
		return obj.newNumber(atoi(port));
	}
};

static Value socket_accept(Value& fnc, Value& ths, Value& arg) {
	int fd = ths.getPrivate<long>(PRIV_POSIX_FD);

	int newsock = accept(fd, NULL, NULL);
	if (newsock < 0) return throwException(ths, errno);
	return socket_from_sock(ths, newsock, ths.get("domain").to<int>(), ths.get("type").to<int>(), ths.get("protocol").to<int>());
}

static Value socket_bind(Value& fnc, Value& ths, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "|s(sn)");

	int    fd = ths.getPrivate<long>(PRIV_POSIX_FD);
	string ip = "0.0.0.0";
	string port;

	if (arg.get("length").to<int>() > 0) {
		ip = arg[0].to<UTF8>();
		if (arg.get("length").to<int>() > 1)
			port = arg[1].to<UTF8>();
	}

	struct addrinfo* ai = NULL;
	int status = getaddrinfo(ip.c_str(), port.empty() ? NULL : port.c_str(), NULL, &ai);
	if (status != 0) {
		freeaddrinfo(ai);
		return throwExceptionEAI(ths, status);
	}
	if (bind(fd, ai->ai_addr, ai->ai_addrlen) < 0) {
		freeaddrinfo(ai);
		return throwException(ths, errno);
	}
	freeaddrinfo(ai);
	return ths.newUndefined();
}

static Value socket_connect(Value& fnc, Value& ths, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "s(sn)");

	int fd = ths.getPrivate<long>(PRIV_POSIX_FD);

	struct addrinfo* ai = NULL;
	int status = getaddrinfo(arg[0].to<UTF8>().c_str(), arg[1].to<UTF8>().c_str(), NULL, &ai);
	if (status != 0) {
		freeaddrinfo(ai);
		return throwExceptionEAI(ths, status);
	}
	if (connect(fd, ai->ai_addr, ai->ai_addrlen) < 0) {
		freeaddrinfo(ai);
		return throwException(ths, errno);
	}
	freeaddrinfo(ai);
	return ths.newUndefined();
}

static Value socket_listen(Value& fnc, Value& ths, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "n");

	int fd = ths.getPrivate<long>(PRIV_POSIX_FD);
	if (listen(fd, arg.get("length").to<int>() > 0 ? arg[0].to<int>() : 1024) < 0)
		return throwException(ths, errno);
	return ths.newUndefined();
}

static Value socket_receive(Value& fnc, Value& ths, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "|n");

	int fd = ths.getPrivate<long>(PRIV_POSIX_FD);

	int bs = arg.get("length").to<int>() > 0 ? arg[0].to<int>() : 1024;
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

static Value socket_send(Value& fnc, Value& ths, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "s");

	int fd = ths.getPrivate<long>(PRIV_POSIX_FD);

	string buff = arg[0].to<UTF8>();
	ssize_t snt = send(fd, buff.c_str(), buff.length(), 0);
	if (snt < 0) return throwException(ths, errno);
	return ths.newNumber(snt);
}

static Value socket_shutdown(Value& fnc, Value& ths, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "|n");

	int fd = ths.getPrivate<long>(PRIV_POSIX_FD);
	if (shutdown(fd, arg.get("length").to<int>() > 0 ? arg[0].to<int>() : SHUT_RDWR) < 0)
		return throwException(ths, errno);
	return ths.newUndefined();
}

static Value socket_ctor(Value& fnc, Value& ths, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "|nnn");

	int domain = AF_INET, type = SOCK_STREAM, prot = 0;
	if (arg.get("length").to<int>() > 0) {
		domain = arg[0].to<int>();
		if (arg.get("length").to<int>() > 1) {
			type = arg[1].to<int>();
			if (arg.get("length").to<int>() > 2)
				prot = arg[2].to<int>();
		}
	}

	int fd = socket(domain, type, prot);
	if (fd < 0) return throwException(ths, errno);
	return socket_from_sock(ths, fd, domain, type, prot);
}

static Value socket_from_sock(Value& ctx, int sock, int domain, int type, int protocol) {
	Value obj = ctx.newObject(new SocketClass);
	if (obj.isException()) return obj;
	stream_from_fd(obj, sock);

	obj.set("accept",        socket_accept);
	obj.set("bind",          socket_bind);
	obj.set("connect",       socket_connect);
	obj.set("listen",        socket_listen);
	obj.set("receive",       socket_receive);
	obj.set("recv",          socket_receive);
	obj.set("send",          socket_send);
	obj.set("shutdown",      socket_shutdown);
	obj.set("isConnected",   false);
	obj.set("isReadable",    false);
	obj.set("isWritable",    false);
	obj.set("domain",        domain);
	obj.set("type",          type);
	obj.set("protocol",      protocol);
	return obj;
}

#define OK(x) ok = (!x.isException()) || ok
#define NCONST(macro) OK(mod.setRecursive("exports." # macro, macro))

extern "C" bool NATUS_MODULE_INIT(ntValue* module) {
	Value mod(module, false);
	bool ok = false;

	// Objects
	OK(mod.setRecursive("exports.Socket", socket_ctor));

	// Constants
#ifdef AF_APPLETALK
	NCONST(AF_APPLETALK);
#endif
#ifdef AF_ASH
	NCONST(AF_ASH);
#endif
#ifdef AF_ATMPVC
	NCONST(AF_ATMPVC);
#endif
#ifdef AF_ATMSVC
	NCONST(AF_ATMSVC);
#endif
#ifdef AF_AX25
	NCONST(AF_AX25);
#endif
#ifdef AF_BRIDGE
	NCONST(AF_BRIDGE);
#endif
#ifdef AF_DECnet
	NCONST(AF_DECnet);
#endif
#ifdef AF_ECONET
	NCONST(AF_ECONET);
#endif
#ifdef AF_INET
	NCONST(AF_INET);
#endif
#ifdef AF_INET6
	NCONST(AF_INET6);
#endif
#ifdef AF_IPX
	NCONST(AF_IPX);
#endif
#ifdef AF_IRDA
	NCONST(AF_IRDA);
#endif
#ifdef AF_KEY
	NCONST(AF_KEY);
#endif
#ifdef AF_LLC
	NCONST(AF_LLC);
#endif
#ifdef AF_NETBEUI
	NCONST(AF_NETBEUI);
#endif
#ifdef AF_NETLINK
	NCONST(AF_NETLINK);
#endif
#ifdef AF_NETROM
	NCONST(AF_NETROM);
#endif
#ifdef AF_PACKET
	NCONST(AF_PACKET);
#endif
#ifdef AF_PPPOX
	NCONST(AF_PPPOX);
#endif
#ifdef AF_ROSE
	NCONST(AF_ROSE);
#endif
#ifdef AF_ROUTE
	NCONST(AF_ROUTE);
#endif
#ifdef AF_SECURITY
	NCONST(AF_SECURITY);
#endif
#ifdef AF_SNA
	NCONST(AF_SNA);
#endif
#ifdef AF_TIPC
	NCONST(AF_TIPC);
#endif
#ifdef AF_UNIX
	NCONST(AF_UNIX);
#endif
#ifdef AF_UNSPEC
	NCONST(AF_UNSPEC);
#endif
#ifdef AF_WANPIPE
	NCONST(AF_WANPIPE);
#endif
#ifdef AF_X25
	NCONST(AF_X25);
#endif
#ifdef SOCK_DGRAM
	NCONST(SOCK_DGRAM);
#endif
#ifdef SOCK_RAW
	NCONST(SOCK_RAW);
#endif
#ifdef SOCK_RDM
	NCONST(SOCK_RDM);
#endif
#ifdef SOCK_SEQPACKET
	NCONST(SOCK_SEQPACKET);
#endif
#ifdef SOCK_STREAM
	NCONST(SOCK_STREAM);
#endif
#ifdef SHUT_RD
	NCONST(SHUT_RD);
#endif
#ifdef SHUT_RDWR
	NCONST(SHUT_RDWR);
#endif
#ifdef SHUT_WR
	NCONST(SHUT_WR);
#endif
#ifdef SOL_IP
	NCONST(SOL_IP);
#endif
#ifdef SOL_SOCKET
	NCONST(SOL_SOCKET);
#endif
#ifdef SOL_TCP
	NCONST(SOL_TCP);
#endif
#ifdef SOL_UDP
	NCONST(SOL_UDP);
#endif
#ifdef SO_DEBUG
	NCONST(SO_DEBUG);
#endif
#ifdef SO_REUSEADDR
	NCONST(SO_REUSEADDR);
#endif
#ifdef SO_TYPE
	NCONST(SO_TYPE);
#endif
#ifdef SO_ERROR
	NCONST(SO_ERROR);
#endif
#ifdef SO_DONTROUTE
	NCONST(SO_DONTROUTE);
#endif
#ifdef SO_BROADCAST
	NCONST(SO_BROADCAST);
#endif
#ifdef SO_SNDBUF
	NCONST(SO_SNDBUF);
#endif
#ifdef SO_RCVBUF
	NCONST(SO_RCVBUF);
#endif
#ifdef SO_SNDBUFFORCE
	NCONST(SO_SNDBUFFORCE);
#endif
#ifdef SO_RCVBUFFORCE
	NCONST(SO_RCVBUFFORCE);
#endif
#ifdef SO_KEEPALIVE
	NCONST(SO_KEEPALIVE);
#endif
#ifdef SO_OOBINLINE
	NCONST(SO_OOBINLINE);
#endif
#ifdef SO_NO_CHECK
	NCONST(SO_NO_CHECK);
#endif
#ifdef SO_PRIORITY
	NCONST(SO_PRIORITY);
#endif
#ifdef SO_LINGER
	NCONST(SO_LINGER);
#endif
#ifdef SO_BSDCOMPAT
	NCONST(SO_BSDCOMPAT);
#endif
#ifdef SO_PASSCRED
	NCONST(SO_PASSCRED);
#endif
#ifdef SO_PEERCRED
	NCONST(SO_PEERCRED);
#endif
#ifdef SO_RCVLOWAT
	NCONST(SO_RCVLOWAT);
#endif
#ifdef SO_SNDLOWAT
	NCONST(SO_SNDLOWAT);
#endif
#ifdef SO_RCVTIMEO
	NCONST(SO_RCVTIMEO);
#endif
#ifdef SO_SNDTIMEO
	NCONST(SO_SNDTIMEO);
#endif
#ifdef SO_SECURITY_AUTHENTICATION
	NCONST(SO_SECURITY_AUTHENTICATION);
#endif
#ifdef SO_SECURITY_ENCRYPTION_TRANSPORT
	NCONST(SO_SECURITY_ENCRYPTION_TRANSPORT);
#endif
#ifdef SO_SECURITY_ENCRYPTION_NETWORK
	NCONST(SO_SECURITY_ENCRYPTION_NETWORK);
#endif
#ifdef SO_BINDTODEVICE
	NCONST(SO_BINDTODEVICE);
#endif
#ifdef SO_ATTACH_FILTER
	NCONST(SO_ATTACH_FILTER);
#endif
#ifdef SO_DETACH_FILTER
	NCONST(SO_DETACH_FILTER);
#endif
#ifdef SO_PEERNAME
	NCONST(SO_PEERNAME);
#endif
#ifdef SO_TIMESTAMP
	NCONST(SO_TIMESTAMP);
#endif
#ifdef SO_ACCEPTCONN
	NCONST(SO_ACCEPTCONN);
#endif
#ifdef SO_PEERSEC
	NCONST(SO_PEERSEC);
#endif
#ifdef SO_PASSSEC
	NCONST(SO_PASSSEC);
#endif
#ifdef SO_TIMESTAMPNS
	NCONST(SO_TIMESTAMPNS);
#endif
#ifdef SO_MARK
	NCONST(SO_MARK);
#endif
#ifdef SO_TIMESTAMPING
	NCONST(SO_TIMESTAMPING);
#endif
#ifdef SO_PROTOCOL
	NCONST(SO_PROTOCOL);
#endif
#ifdef SO_DOMAIN
	NCONST(SO_DOMAIN);
#endif
#ifdef SO_RXQ_OVFL
	NCONST(SO_RXQ_OVFL);
#endif
#ifdef TCP_CORK
	NCONST(TCP_CORK);
#endif
#ifdef TCP_KEEPIDLE
	NCONST(TCP_KEEPIDLE);
#endif
#ifdef TCP_KEEPINTVL
	NCONST(TCP_KEEPINTVL);
#endif
#ifdef TCP_KEEPCNT
	NCONST(TCP_KEEPCNT);
#endif
#ifdef TCP_SYNCNT
	NCONST(TCP_SYNCNT);
#endif
#ifdef TCP_LINGER2
	NCONST(TCP_LINGER2);
#endif
#ifdef TCP_DEFER_ACCEPT
	NCONST(TCP_DEFER_ACCEPT);
#endif
#ifdef TCP_WINDOW_CLAMP
	NCONST(TCP_WINDOW_CLAMP);
#endif
#ifdef TCP_INFO
	NCONST(TCP_INFO);
#endif
#ifdef TCP_CONGESTION
	NCONST(TCP_CONGESTION);
#endif
#ifdef TCP_MD5SIG
	NCONST(TCP_MD5SIG);
#endif
#ifdef TCP_MD5SIG_MAXKEYLEN
	NCONST(TCP_MD5SIG_MAXKEYLEN);
#endif

	return ok;
}
