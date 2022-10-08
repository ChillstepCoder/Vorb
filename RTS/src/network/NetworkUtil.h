#pragma once

extern std::atomic_bool sHasInitYojimbo;

#include <yojimbo/yojimbo.h>

namespace NetworkUtil {

    // https://stackoverflow.com/questions/5760302/when-i-do-getaddrinfo-for-localhost-i-dont-receive-127-0-0-1
    nString getLocalIP();

    // https://stackoverflow.com/questions/39566240/how-to-get-the-external-ip-address-in-c
    bool getWebsite(const nString& url, OUT nString& websiteHtml);

    yojimbo::Address getExternalIP(uint16_t port, bool isIpv6);

}