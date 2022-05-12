#include "protocol.h"

namespace netcore {
    
    Address::Address(uint32_t v32)
    {
        m_int4 = htonl(v32);
        m_address_type = AddressType::IpV4;
    }

    Address::Address()
    {
        if(INADDR_ANY != 0) {
            m_int4 = htonl(INADDR_ANY);
        } else {
            m_int4 = 0;
        }
        m_address_type = AddressType::IpV4;
    }

    std::string Address::to_string() const 
    {
        char buf[256];
        if (m_address_type == AddressType::IpV4) {
            inet_ntop(AF_INET, &m_addr4, buf, sizeof(buf));
            return buf;
        } else if (m_address_type == AddressType::IpV6) {
            inet_ntop(AF_INET6, &m_addr6, buf, sizeof(buf));
            return buf;
        }
        return "";
    }

//=== endpoint

    Endpoint::Endpoint(Address address, uint16_t port)
    {
        m_address = address;
        m_port = port;
    }

    Endpoint::Endpoint() : Endpoint(Address(), 0)
    {
    }


} // end namespace netcore

