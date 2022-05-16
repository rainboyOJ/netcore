#pragma once

#include "basic.hpp"
#include "io_context.h"

namespace netcore {

    struct Protocol
    {
        static Protocol ip_v4()
        {
            return {};
        }
    };

    enum class AddressType {
        IpV4,
        IpV6,
    };

    //对地址的封装
    struct Address
    {

        // ip_v32 host byte order
        Address(uint32_t v32);

        // if we bind on this address,
        // We will listen to all network card(s)
        Address();

        std::string to_string() const;

        static Address Any()
        {
            return {};
        }

        union
        {
            uint32_t m_int4;
            in_addr m_addr4;
            in6_addr m_addr6;
        };
        AddressType m_address_type;
    };
    

    struct Endpoint
    {
        Endpoint(Address address, uint16_t port);
        Endpoint();

        Address address() const noexcept {
            return m_address;
        }

        uint16_t port() const noexcept { 
            return m_port;
        }
        
    private:
        Address m_address;
        uint16_t m_port;
    };
} // end namespace netcore

