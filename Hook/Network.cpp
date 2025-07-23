// Network.cpp 负责 MapleStory 客户端的网络相关 Hook，包括网络初始化、域名解析、数据包加解密、服务器重定向等。
// 通过 Hook Winsock 相关 API，实现对客户端网络行为的控制和修改。
//
// 主要功能：
// 1. InitWSAData/ClearupWSA：初始化和清理 Winsock。
// 2. FixDomain：修正客户端连接的域名，绕过官方服务器。
// 3. Redirect：重定向登录/频道服务器地址。
// 4. RecvXOR/SendXOR：对收发数据包进行 XOR 加解密。
// 5. WSPStartup/WSPConnect/WSPGetPeerName/recv/send 等 Hook 实现底层网络控制。
//
// 全局变量说明：
// - wsaData：保存 Winsock 初始化信息，防止重复初始化。
// - gProcTable：保存原始 Winsock SPI 函数表，用于后续 Hook。
// - gLoginSockAddr/gChannelSockAddr：保存登录/频道服务器的 sockaddr 结构体。
// - recvXOR/sendXOR：收发数据包的 XOR 密钥。
// - gMapleVersion：客户端版本号，用于资源挂载和兼容性判断。
#include <winsock2.h>
#include <ws2spi.h>
#include <ws2tcpip.h>
#include "Network.h"
#include "Resources/AOBList.h"

#include "Share/Funcs.h"
#include "Share/Tool.h"

#pragma comment(lib, "ws2_32.lib")

namespace {

	// officialIP：官方服务器 IP，defaultIP：本地回环地址，defaultPort：默认端口
	const std::string officialIP = "221.231.130.70";
	const std::string defaultIP = "127.0.0.1";
	const USHORT defaultPort = 8484;

	// Winsock 相关全局变量
	static WSADATA wsaData{};
	static WSPPROC_TABLE gProcTable = { 0 };

	// 登录/频道服务器地址
	static struct sockaddr* gLoginSockAddr = nullptr;
	static struct sockaddr* gChannelSockAddr = nullptr;
	// 数据包 XOR 密钥
	static unsigned char recvXOR = 0x00;
	static unsigned char sendXOR = 0x00;
	// MapleStory 版本号
	static BYTE gMapleVersion;

	// 判断端口是否为 Web 端口（80/443），用于跳过网页请求
	// 参数：name - 指向 sockaddr_in 结构体的指针
	// 返回值：true 表示是 Web 端口，false 表示不是
	bool IsWebPort(sockaddr_in* name) {
		WORD wPort = ntohs(name->sin_port);
		if (wPort == 80 || wPort == 443) {
			return true;
		}
		return false;
	}

	// 将 sockaddr 结构体转换为字符串 IP
	// 参数：name - 指向 sockaddr 结构体的指针
	// 返回值：IP 字符串，支持 IPv4/IPv6
	std::string IP2Str(const struct sockaddr* name) {
		std::string ipStr = "";
		char ipBuf[INET6_ADDRSTRLEN];
		if (name->sa_family == AF_INET) {
			sockaddr_in* addr_in = (sockaddr_in*)name;
			inet_ntop(AF_INET, &addr_in->sin_addr, ipBuf, sizeof(ipBuf));
			ipStr = std::string(ipBuf);
		}
		else if (name->sa_family == AF_INET6) {
			sockaddr_in6* addr_in6 = (sockaddr_in6*)name;
			inet_ntop(AF_INET6, &addr_in6->sin6_addr, ipBuf, sizeof(ipBuf));
			ipStr = std::string(ipBuf);
		}
		else {
			DEBUG(L"Unknown family");
		}
		return ipStr;
	}

	// 判断字符串是否为 IPv4 地址
	// 参数：addr - IP 字符串
	// 返回值：true 表示是 IPv4，false 表示不是
	bool IsIPv4(const std::string& addr) {
		struct sockaddr_in sa {};
		return inet_pton(AF_INET, addr.c_str(), &(sa.sin_addr)) == 1;
	}
	// 判断字符串是否为 IPv6 地址
	// 参数：addr - IP 字符串
	// 返回值：true 表示是 IPv6，false 表示不是
	bool IsIPv6(const std::string& addr) {
		struct sockaddr_in6 sa6 {};
		return inet_pton(AF_INET6, addr.c_str(), &(sa6.sin6_addr)) == 1;
	}

	// 域名解析，优先使用 getaddrinfo 获取 sockaddr
	// 参数：ppName - 输出，指向 sockaddr* 的指针
	//      addr - 域名或 IP 字符串
	//      port - 端口号
	// 返回值：true 表示解析成功，false 表示失败
	bool ResolveDomain(struct sockaddr** ppName, const std::string& addr, const USHORT port) {
		struct addrinfo hints {};
		struct addrinfo* pResult = nullptr;

		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;

		int err = getaddrinfo(addr.c_str(), NULL, &hints, &pResult);
		if (err != 0) {
			SCANRES(err);
			if (err == WSANOTINITIALISED) {
				DEBUG(L"WSAStartup hasn't initialized.");
			}
			DEBUG(L"Failed to get addr info from domain");
			return false;
		}

		if (pResult->ai_family == AF_INET) {
			// IPv4 结果
			struct sockaddr_in* addr_in = new struct sockaddr_in();
			memcpy(addr_in, pResult->ai_addr, sizeof(struct sockaddr_in));
			addr_in->sin_port = htons(port);
			*ppName = (struct sockaddr*)addr_in;
		}
		else if (pResult->ai_family == AF_INET6) {
			// IPv6 结果
			struct sockaddr_in6* addr_in6 = new struct sockaddr_in6();
			memcpy(addr_in6, pResult->ai_addr, sizeof(struct sockaddr_in6));
			addr_in6->sin6_port = htons(port);
			*ppName = (struct sockaddr*)addr_in6;
		}
		else {
			DEBUG(L"Unknown family");
		}

		freeaddrinfo(pResult);

		return *ppName != nullptr;
	}

	// 初始化 sockaddr 结构体，支持 IPv4/IPv6/域名
	// 参数：ppName - 输出，指向 sockaddr* 的指针
	//      addr - 域名或 IP 字符串
	//      port - 端口号
	// 逻辑：优先尝试 IPv4/IPv6，失败则尝试域名解析，最后回退到默认地址
	void InitSockAddr(struct sockaddr** ppName, const std::string& addr, const USHORT port) {
		if (*ppName != nullptr) {
			DEBUG(L"SockAddr has already been initialized");
			return;
		}
		if (IsIPv4(addr)) {
			// 直接使用 IPv4
			struct sockaddr_in* pAddrIn = new struct sockaddr_in();
			pAddrIn->sin_family = AF_INET;
			inet_pton(AF_INET, addr.c_str(), &pAddrIn->sin_addr);
			pAddrIn->sin_port = htons(port);
			*ppName = (struct sockaddr*)pAddrIn;
			return;
		}
		if (IsIPv6(addr)) {
			// 直接使用 IPv6
			struct sockaddr_in6* pAddrIn6 = new struct sockaddr_in6();
			pAddrIn6->sin6_family = AF_INET6;
			inet_pton(AF_INET6, addr.c_str(), &pAddrIn6->sin6_addr);
			pAddrIn6->sin6_port = htons(port);
			*ppName = (struct sockaddr*)pAddrIn6;
			return;
		}
		if (ResolveDomain(ppName, addr, port)) {
			// 域名解析成功
			DEBUG(L"Resolve domain ok");
			return;
		}
		// 所有尝试失败，使用默认地址
		struct sockaddr_in* pAddrIn = new struct sockaddr_in();
		pAddrIn->sin_family = AF_INET;
		inet_pton(AF_INET, defaultIP.c_str(), &(pAddrIn->sin_addr));
		pAddrIn->sin_port = htons(defaultPort);
		*ppName = (struct sockaddr*)pAddrIn;
	}

	// Hook: WSPGetPeerName，获取对端地址时拦截，跳过网页端口和官方 IP
	// 参数：s - 套接字句柄
	//      name - 输出，保存对端地址
	//      namelen - 地址长度
	//      lpErrno - 错误码
	// 返回值：SOCKET_ERROR 表示失败，否则为 0
	int WINAPI WSPGetPeerName_Hook(SOCKET s, struct sockaddr* name, LPINT namelen, LPINT lpErrno) {
		int res = gProcTable.lpWSPGetPeerName(s, name, namelen, lpErrno);

		if (res == SOCKET_ERROR) {
			DEBUG(L"[WSPGetPeerName] ErrorCode: ");
			SCANRES(res);
		}

		sockaddr_in* addr_in = (sockaddr_in*)name;

		if (IsWebPort(addr_in)) {
			// 拦截网页端口，返回错误
			return SOCKET_ERROR;
		}

		if (inet_pton(AF_INET, officialIP.c_str(), &addr_in->sin_addr) == 1) {
			// 如果是官方 IP，输出日志
			DEBUG(L"[Peername] bypass " + Str2WStr(officialIP));
		}
		else {
			DEBUG(L"Failed to redirect ip");
		}
		return res;
	}

	// Hook: WSPConnect，连接服务器时拦截，重定向到自定义服务器
	// 参数：s - 套接字句柄
	//      name - 目标地址
	//      namelen - 地址长度
	//      lpCallerData/lpCalleeData/lpSQOS/lpGQOS/lpErrno - 见 Winsock 文档
	// 返回值：0 表示成功，SOCKET_ERROR 表示失败
	int WINAPI WSPConnect_Hook(SOCKET s, const struct sockaddr* name, int namelen, LPWSABUF lpCallerData, LPWSABUF lpCalleeData, LPQOS lpSQOS, LPQOS lpGQOS, LPINT lpErrno) {

		sockaddr_in* addr_in = (sockaddr_in*)name;

		if (IsWebPort(addr_in)) {
			// 拦截网页端口
			return SOCKET_ERROR;
		}

		std::string serverReturnedIP = IP2Str(name);
		USHORT serverReturnedPort = ntohs(addr_in->sin_port);
		if (serverReturnedPort == defaultPort) {
			// 登录服务器，重定向到本地/自定义服务器
			DEBUG(L"Connect to login server");
			name = gLoginSockAddr;
		}
		else {
			// 频道服务器，重定向并同步端口
			DEBUG(L"Connect to channel server with port:" + std::to_wstring(serverReturnedPort));
			if (gChannelSockAddr->sa_family == AF_INET6) {
				sockaddr_in6* addr_ipv6 = (sockaddr_in6*)gChannelSockAddr;
				addr_ipv6->sin6_port = addr_in->sin_port;
			}
			else {
				sockaddr_in* addr_ipv4 = (sockaddr_in*)gChannelSockAddr;
				addr_ipv4->sin_port = addr_in->sin_port;
			}
			name = gChannelSockAddr;
		}

		return gProcTable.lpWSPConnect(s, name, namelen, lpCallerData, lpCalleeData, lpSQOS, lpGQOS, lpErrno);
	}

	// Hook: WSPStartup，Winsock SPI 启动时拦截，替换函数表
	// 参数：wVersionRequested - 请求的 Winsock 版本
	//      lpWSPData/lpProtocolInfo/UpcallTable/lpProcTable - 见 Winsock 文档
	// 返回值：0 表示成功，非 0 表示失败
	static auto _WSPStartup = decltype(&WSPStartup)(GetProcAddress(LoadLibraryW(L"MSWSOCK"), "WSPStartup"));
	int WINAPI WSPStartup_Hook(WORD wVersionRequested, LPWSPDATA lpWSPData, LPWSAPROTOCOL_INFOW lpProtocolInfo, WSPUPCALLTABLE UpcallTable, LPWSPPROC_TABLE lpProcTable) {

		int ret = _WSPStartup(wVersionRequested, lpWSPData, lpProtocolInfo, UpcallTable, lpProcTable);
		gProcTable = *lpProcTable;

		// 替换函数表中的 Connect 和 GetPeerName 为自定义 Hook
		lpProcTable->lpWSPConnect = (decltype(lpProcTable->lpWSPConnect))WSPConnect_Hook;
		lpProcTable->lpWSPGetPeerName = (decltype(lpProcTable->lpWSPGetPeerName))WSPGetPeerName_Hook;

		return ret;
	}

	// Hook: recv，接收数据包时拦截，进行 XOR 解密
	// 参数：s - 套接字句柄
	//      buf - 数据缓冲区
	//      len - 缓冲区长度
	//      flags - 标志
	// 返回值：实际接收字节数，SOCKET_ERROR 表示失败
	static auto _recv = decltype(&recv)(GetProcAddress(GetModuleHandleW(L"Ws2_32.dll"), "recv"));
	int WINAPI recv_Hook(SOCKET s, char* buf, int len, int flags) {
		int res = _recv(s, buf, len, flags);
		if (res == SOCKET_ERROR) {
			return res;
		}
		if (res == 0) {
			// 连接被对端关闭
			DEBUG(L"Connection was closed by peer");
			return res;
		}
		if (recvXOR != 0x00) {
			// 对接收到的数据进行 XOR 解密
			for (int i = 0; i < res; i++) {
				buf[i] ^= recvXOR;
			}
		}
		return res;
	}

	// Hook: send，发送数据包时拦截，进行 XOR 加密
	// 参数：s - 套接字句柄
	//      buf - 数据缓冲区
	//      len - 缓冲区长度
	//      flags - 标志
	// 返回值：实际发送字节数，SOCKET_ERROR 表示失败
	static auto _send = decltype(&send)(GetProcAddress(GetModuleHandleW(L"Ws2_32.dll"), "send"));
	int WINAPI send_Hook(SOCKET s, char* buf, int len, int flags) {
		if (sendXOR != 0x00) {
			// 对发送的数据进行 XOR 加密
			for (int i = 0; i < len; i++) {
				buf[i] ^= sendXOR;
			}
		}
		int res = _send(s, buf, len, flags);
		if (res == SOCKET_ERROR) {
			return res;
		}
		if (res == 0) {
			// 连接被对端关闭
			DEBUG(L"Connection was closed by peer");
			return res;
		}
		return res;
	}

}

namespace Network {

	// 初始化 Winsock，防止重复初始化
	// 返回值：true 表示初始化成功，false 表示已初始化或失败
	bool InitWSAData() {
		if (wsaData.wVersion != 0) {
			DEBUG(L"wsaData has already been initialized");
			return false;
		}
		int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (res != 0) {
			DEBUG(L"Failed to init WSAData");
			SCANRES(res);
			return false;
		}
		return true;
	}

	// 清理 Winsock
	// 返回值：无
	void ClearupWSA() {
		int res = WSACleanup();
		if (res != 0) {
			DEBUG(L"Failed to clear up WSA");
			return;
		}
		DEBUG(L"WSACleanup ok");
	}

	// 修正客户端域名，绕过官方服务器，设置 Maple 版本号
	// 参数：r - Rosemary 对象，用于内存扫描和字符串替换
	// 返回值：true 表示所有域名修正成功，false 表示部分失败
	bool FixDomain(Rosemary& r) {
		// 通过内存扫描定位版本号，并修正登录域名为 127.0.0.1
		ULONG_PTR uAddr = 0;
		size_t index;
		for (index = 0; index < AOB_Scan_CPatchException__CPatchException_Addrs.size(); index++)
		{
			uAddr = r.Scan(AOB_Scan_CPatchException__CPatchException_Addrs[index]);
			if (uAddr > 0) {
				break;
			}
		}
		if (uAddr == 0) {
			DEBUG(L"Unable to find CPatchException::CPatchException")
		}
		else {
			auto bAddr = (BYTE*)uAddr;
			if (index == 0) {
				// 66 C7 07 ?? 00
				gMapleVersion = bAddr[0x3];
			}
			else {
				// BA ?? 00 00 00
				gMapleVersion = bAddr[0x1];
			}
		}
		// 替换所有登录域名为 127.0.0.1，防止客户端连接官方服务器
		return r.StringPatch("mxdlogin6.poptang.com", "127.0.0.1") && // CMS88(00BABABC)
			r.StringPatch("mxdlogin5.poptang.com", "127.0.0.1") && // CMS88(00BABAD4)
			r.StringPatch("mxdlogin3.poptang.com", "127.0.0.1") && // CMS88(00BABAEC)
			r.StringPatch("mxdlogin2.poptang.com", "127.0.0.1") && // CMS88(00BABB04)
			r.StringPatch("mxdlogin.poptang.com", "127.0.0.1"); // CMS88(00BABB1C)
	}

	// 重定向登录/频道服务器地址，安装 WSPStartup Hook
	// 参数：serverAddr - 服务器地址
	//      loginServerPort - 登录服务器端口
	// 返回值：true 表示 Hook 安装成功，false 表示失败
	bool Redirect(const std::string& serverAddr, const unsigned short loginServerPort) {
		InitSockAddr(&gLoginSockAddr, serverAddr, loginServerPort);
		InitSockAddr(&gChannelSockAddr, serverAddr, 0);
		return SHOOK(true, &_WSPStartup, WSPStartup_Hook);
	}

	// 安装 recv Hook，实现数据包 XOR 解密
	// 参数：XOR - 解密用的 XOR 密钥
	// 返回值：true 表示 Hook 安装成功，false 表示失败
	bool RecvXOR(const unsigned char XOR) {
		recvXOR = XOR;
		return SHOOK(true, &_recv, recv_Hook);
	}

	// 安装 send Hook，实现数据包 XOR 加密
	// 参数：XOR - 加密用的 XOR 密钥
	// 返回值：true 表示 Hook 安装成功，false 表示失败
	bool SendXOR(const unsigned char XOR) {
		sendXOR = XOR;
		return SHOOK(true, &_send, send_Hook);
	}

	// 获取 MapleStory 版本号
	// 返回值：指向版本号的 BYTE 指针
	const BYTE* GetMapleVersion() {
		return &gMapleVersion;
	}

}