#pragma once

#include "Share/Rosemary.h"

namespace Network {
	bool InitWSAData(); // Ready for Winsock
	void ClearupWSA(); // Free Winsock
	bool FixDomain(Rosemary& r); // Unable to show ads window if not set
	bool Redirect(const std::string& serverAddr, const unsigned short loginServerPort); // Redirect connect addr and port
	bool RecvXOR(const unsigned char XOR); // XOR all packet from server
	bool SendXOR(const unsigned char XOR); // XOR all packet from client
	const BYTE* GetMapleVersion(); // Set maple version from FixDomain
}