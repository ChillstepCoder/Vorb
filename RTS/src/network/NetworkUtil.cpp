#include "stdafx.h"
#include "NetworkUtil.h"

// For getLocalIP() gethostbyname
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <windows.h>
#include <iostream>
#include <winsock2.h>
#include <locale>
#include <sstream>
#include <ws2tcpip.h>

#define USE_IPV6 1
constexpr char IPV4_API_WEBSITE[] = "api.ipify.org";
constexpr char IPV6_API_WEBSITE[] = "api6.ipify.org";
static char lineBuffer[200][80] = { ' ' };

namespace NetworkUtil {

    nString getLocalIP()
    {
        WORD wVersionRequested = MAKEWORD(2, 2);
        WSADATA wsaData;
        if (WSAStartup(wVersionRequested, &wsaData) != 0)
            return 0;
        char local[255] = { 0 };
        gethostname(local, sizeof(local));
        hostent* ph = gethostbyname(local);
        if (ph == NULL)
            return 0;
        in_addr addr;
        memcpy(&addr, ph->h_addr_list[0], sizeof(in_addr));
        sprintf_s(local, "%d.%d.%d.%d", addr.S_un.S_un_b.s_b1, addr.S_un.S_un_b.s_b2, addr.S_un.S_un_b.s_b3, addr.S_un.S_un_b.s_b4);
        return nString(local);
    }

    bool getWebsite(const nString& url, OUT nString& websiteHtml)
    {
        WSADATA wsaData;
        SOCKET Socket;
        nString get_http = "GET / HTTP/1.1\r\nHost: " + url + "\r\nConnection: close\r\n\r\n";

        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cout << "WSAStartup failed while trying to connect to http://api.ipify.org/\n";
            return false;
        }

        // https://beej.us/guide/bgnet/html/#getaddrinfoprepare-to-launch
        int err;
        struct addrinfo hints;
        struct addrinfo* servinfo;
        memset(&hints, 0, sizeof(hints));
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_family = AF_UNSPEC;
        hints.ai_flags = AI_PASSIVE;
        if (err = getaddrinfo(url.c_str(), "80", &hints, &servinfo) != 0) {
            fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(err));
            std::cout << "Failed to get address info for " << url << std::endl;
            return false;
        }

        char hostBuffer[INET6_ADDRSTRLEN];
        //https://docs.microsoft.com/en-us/windows/win32/api/ws2tcpip/nf-ws2tcpip-inet_ntop
        if (servinfo->ai_family == AF_INET) { // IPV4
            struct sockaddr_in* ipv4 = (struct sockaddr_in*)servinfo->ai_addr;
            void* addr = &(ipv4->sin_addr);
            if (inet_ntop(servinfo->ai_family, addr, hostBuffer, sizeof hostBuffer) == nullptr) {
                std::cout << "inet_ntop failed for " << url << std::endl;
                freeaddrinfo(servinfo);
                return false;
            }
        }
        else { // IPV6
            struct sockaddr_in6* ipv6 = (struct sockaddr_in6*)servinfo->ai_addr;
            void* addr = &(ipv6->sin6_addr);
            if (inet_ntop(servinfo->ai_family, addr, hostBuffer, sizeof hostBuffer) == nullptr) {
                std::cout << "inet_ntop failed for " << url << std::endl;
                freeaddrinfo(servinfo);
                return false;
            }
        }

        if ((Socket = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == INVALID_SOCKET) {
            std::cout << "Could not open socket to http://api.ipify.org/\n";
            char errorStr[512];
            strerror_s(errorStr, errno);
            std::cout << errorStr << std::endl;
            freeaddrinfo(servinfo);
            return false;
        }

        if (connect(Socket, servinfo->ai_addr, servinfo->ai_addrlen) != 0) {
            std::cout << "Could not connect to http://api.ipify.org/\n";
            char errorStr[512];
            strerror_s(errorStr, errno);
            std::cout << errorStr << std::endl;

            wchar_t* s = NULL;
            FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, WSAGetLastError(),
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPWSTR)&s, 0, NULL);
            fprintf(stdout, "%S\n", s);

            LocalFree(s);
            closesocket(Socket);

            freeaddrinfo(servinfo);
        }
        freeaddrinfo(servinfo);
        send(Socket, get_http.c_str(), strlen(get_http.c_str()), 0);

        int nDataLength;
        char buffer[2000];
        while ((nDataLength = recv(Socket, buffer, 2000, 0)) > 0) {
            int i = 0;
            while (buffer[i] >= 32 || buffer[i] == '\n' || buffer[i] == '\r') {

                websiteHtml += buffer[i];
                ++i;
            }
        }
        closesocket(Socket);
        WSACleanup();
        return true;
    }

    yojimbo::Address getExternalIP(uint16_t port) {

        // PUBLIC
        std::locale local;
        nString website_HTML;
        char ip_address[256];
        int i = 0, bufLen = 0, j = 0, lineCount = 0;
        int lineIndex = 0, posIndex = 0;

        // We will use this website to find our IPV6
        if (!getWebsite(USE_IPV6 ? IPV6_API_WEBSITE : IPV4_API_WEBSITE, website_HTML)) {
            return yojimbo::Address();
        }

        for (size_t i = 0; i < website_HTML.length(); ++i) website_HTML[i] = tolower(website_HTML[i], local);

        std::istringstream ss(website_HTML);
        std::string stoken;

#if USE_IPV6
        while (getline(ss, stoken, '\n')) {


            strcpy_s(lineBuffer[lineIndex], stoken.c_str());
            int dot = 0;
            for (int ii = 0; ii < strlen(lineBuffer[lineIndex]); ii++) {

                if (lineBuffer[lineIndex][ii] == ':') dot++;
                if (dot >= 7) {
                    dot = 0;
                    strcpy_s(ip_address, lineBuffer[lineIndex]);
                }
            }

            lineIndex++;
        }
#else

        while (getline(ss, stoken, '\n')) {

            //cout <<"-->"<< stoken.c_str() << '\n';

            strcpy_s(lineBuffer[lineIndex], stoken.c_str());
            int dot = 0;
            for (int ii = 0; ii < strlen(lineBuffer[lineIndex]); ii++) {

                if (lineBuffer[lineIndex][ii] == '.') dot++;
                if (dot >= 3) {
                    dot = 0;
                    strcpy_s(ip_address, lineBuffer[lineIndex]);
                }
            }

            lineIndex++;
        }
#endif

        return yojimbo::Address(ip_address, port);
    }

};