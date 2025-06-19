#include <winsock2.h>
#include <ws2spi.h>
#include <ws2tcpip.h>
#include "Network.h"
#include "Resources/AOBList.h"

#include "Share/Funcs.h"
#include "Share/Tool.h"

#pragma comment(lib, "ws2_32.lib")

namespace {

	const std::string officialIP = "221.231.130.70";
	const std::string defaultIP = "127.0.0.1";
	const USHORT defaultPort = 8484;

	static WSADATA wsaData{};
	static WSPPROC_TABLE gProcTable = { 0 };

	static struct sockaddr* gLoginSockAddr = nullptr;
	static struct sockaddr* gChannelSockAddr = nullptr;
	static unsigned char recvXOR = 0x00;
	static unsigned char sendXOR = 0x00;
	static BYTE gMapleVersion;

	bool IsWebPort(sockaddr_in* name) {
		WORD wPort = ntohs(name->sin_port);
		if (wPort == 80 || wPort == 443) {
			return true;
		}
		return false;
	}

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

	bool IsIPv4(const std::string& addr) {
		struct sockaddr_in sa {};
		return inet_pton(AF_INET, addr.c_str(), &(sa.sin_addr)) == 1;
	}

	bool IsIPv6(const std::string& addr) {
		struct sockaddr_in6 sa6 {};
		return inet_pton(AF_INET6, addr.c_str(), &(sa6.sin6_addr)) == 1;
	}

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
			struct sockaddr_in* addr_in = new struct sockaddr_in();
			memcpy(addr_in, pResult->ai_addr, sizeof(struct sockaddr_in));
			addr_in->sin_port = htons(port);
			*ppName = (struct sockaddr*)addr_in;
		}
		else if (pResult->ai_family == AF_INET6) {
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

	void InitSockAddr(struct sockaddr** ppName, const std::string& addr, const USHORT port) {
		if (*ppName != nullptr) {
			DEBUG(L"SockAddr has already been initialized");
			return;
		}
		if (IsIPv4(addr)) {
			// Try to use IPv4
			struct sockaddr_in* pAddrIn = new struct sockaddr_in();
			pAddrIn->sin_family = AF_INET;
			inet_pton(AF_INET, addr.c_str(), &pAddrIn->sin_addr);
			pAddrIn->sin_port = htons(port);
			*ppName = (struct sockaddr*)pAddrIn;
			return;
		}
		if (IsIPv6(addr)) {
			// Try to use IPv6
			struct sockaddr_in6* pAddrIn6 = new struct sockaddr_in6();
			pAddrIn6->sin6_family = AF_INET6;
			inet_pton(AF_INET6, addr.c_str(), &pAddrIn6->sin6_addr);
			pAddrIn6->sin6_port = htons(port);
			*ppName = (struct sockaddr*)pAddrIn6;
			return;
		}
		if (ResolveDomain(ppName, addr, port)) {
			// Try to resolve domain
			DEBUG(L"Resolve domain ok");
			return;
		}
		// Use default addr
		struct sockaddr_in* pAddrIn = new struct sockaddr_in();
		pAddrIn->sin_family = AF_INET;
		inet_pton(AF_INET, defaultIP.c_str(), &(pAddrIn->sin_addr));
		pAddrIn->sin_port = htons(defaultPort);
		*ppName = (struct sockaddr*)pAddrIn;
	}

	// Use WSPGetPeerName_Hook or Disable CWvsApp::CallUpdate skip peername check
	int WINAPI WSPGetPeerName_Hook(SOCKET s, struct sockaddr* name, LPINT namelen, LPINT lpErrno) {
		int res = gProcTable.lpWSPGetPeerName(s, name, namelen, lpErrno);

		if (res == SOCKET_ERROR) {
			DEBUG(L"[WSPGetPeerName] ErrorCode: ");
			SCANRES(res);
		}

		sockaddr_in* addr_in = (sockaddr_in*)name;

		if (IsWebPort(addr_in)) {
			return SOCKET_ERROR;
		}

		if (inet_pton(AF_INET, officialIP.c_str(), &addr_in->sin_addr) == 1) {
			DEBUG(L"[Peername] bypass " + Str2WStr(officialIP));
		}
		else {
			DEBUG(L"Failed to redirect ip");
		}
		return res;
	}

	int WINAPI WSPConnect_Hook(SOCKET s, const struct sockaddr* name, int namelen, LPWSABUF lpCallerData, LPWSABUF lpCalleeData, LPQOS lpSQOS, LPQOS lpGQOS, LPINT lpErrno) {

		sockaddr_in* addr_in = (sockaddr_in*)name;

		if (IsWebPort(addr_in)) {
			return SOCKET_ERROR;
		}

		std::string serverReturnedIP = IP2Str(name);
		USHORT serverReturnedPort = ntohs(addr_in->sin_port);
		if (serverReturnedPort == defaultPort) {
			DEBUG(L"Connect to login server");
			name = gLoginSockAddr;
		}
		else {
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

	static auto _WSPStartup = decltype(&WSPStartup)(GetProcAddress(LoadLibraryW(L"MSWSOCK"), "WSPStartup"));
	int WINAPI WSPStartup_Hook(WORD wVersionRequested, LPWSPDATA lpWSPData, LPWSAPROTOCOL_INFOW lpProtocolInfo, WSPUPCALLTABLE UpcallTable, LPWSPPROC_TABLE lpProcTable) {

		int ret = _WSPStartup(wVersionRequested, lpWSPData, lpProtocolInfo, UpcallTable, lpProcTable);
		gProcTable = *lpProcTable;

		lpProcTable->lpWSPConnect = (decltype(lpProcTable->lpWSPConnect))WSPConnect_Hook;
		lpProcTable->lpWSPGetPeerName = (decltype(lpProcTable->lpWSPGetPeerName))WSPGetPeerName_Hook;

		return ret;
	}

	// Client need to recover original packet through XOR during recv
	static auto _recv = decltype(&recv)(GetProcAddress(GetModuleHandleW(L"Ws2_32.dll"), "recv"));
	int WINAPI recv_Hook(SOCKET s, char* buf, int len, int flags) {
		int res = _recv(s, buf, len, flags);
		if (res == SOCKET_ERROR) {
			return res;
		}
		if (res == 0) {
			DEBUG(L"Connection was closed by peer");
			return res;
		}
		if (recvXOR != 0x00) {
			for (int i = 0; i < res; i++) {
				buf[i] ^= recvXOR;
			}
		}
		return res;
	}

	// Server need to recover original packet through XOR during recv
	static auto _send = decltype(&send)(GetProcAddress(GetModuleHandleW(L"Ws2_32.dll"), "send"));
	int WINAPI send_Hook(SOCKET s, char* buf, int len, int flags) {
		if (sendXOR != 0x00) {
			for (int i = 0; i < len; i++) {
				buf[i] ^= sendXOR;
			}
		}
		int res = _send(s, buf, len, flags);
		if (res == SOCKET_ERROR) {
			return res;
		}
		if (res == 0) {
			DEBUG(L"Connection was closed by peer");
			return res;
		}
		return res;
	}

}

namespace Network {

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

	void ClearupWSA() {
		int res = WSACleanup();
		if (res != 0) {
			DEBUG(L"Failed to clear up WSA");
			return;
		}
		DEBUG(L"WSACleanup ok");
	}

	bool FixDomain(Rosemary& r) {
		// Set Maple Version from CPatchException::CPatchException
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
		// Using IPv6 as local DNS for these domains may increase startup time
		return r.StringPatch("mxdlogin6.poptang.com", "127.0.0.1") && // CMS88(00BABABC)
			r.StringPatch("mxdlogin5.poptang.com", "127.0.0.1") && // CMS88(00BABAD4)
			r.StringPatch("mxdlogin3.poptang.com", "127.0.0.1") && // CMS88(00BABAEC)
			r.StringPatch("mxdlogin2.poptang.com", "127.0.0.1") && // CMS88(00BABB04)
			r.StringPatch("mxdlogin.poptang.com", "127.0.0.1"); // CMS88(00BABB1C)
	}

	bool Redirect(const std::string& serverAddr, const unsigned short loginServerPort) {
		InitSockAddr(&gLoginSockAddr, serverAddr, loginServerPort);
		InitSockAddr(&gChannelSockAddr, serverAddr, 0);
		return SHOOK(true, &_WSPStartup, WSPStartup_Hook);
	}

	bool RecvXOR(const unsigned char XOR) {
		recvXOR = XOR;
		return SHOOK(true, &_recv, recv_Hook);
	}

	bool SendXOR(const unsigned char XOR) {
		sendXOR = XOR;
		return SHOOK(true, &_send, send_Hook);
	}

	const BYTE* GetMapleVersion() {
		return &gMapleVersion;
	}

}