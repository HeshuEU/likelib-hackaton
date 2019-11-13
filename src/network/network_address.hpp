#pragma once

#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <cstddef>
#include <string_view>


namespace network
{

// TODO: think about naming of this class
class NetworkAddress
{
  public:
    NetworkAddress(const std::string_view& address_with_port);
    NetworkAddress(const std::string_view& address, std::size_t port);

    std::string toString() const;

    unsigned short getPort() const noexcept;

    operator boost::asio::ip::address_v4() const;

    operator boost::asio::ip::tcp::endpoint() const;

  private:
    boost::asio::ip::address_v4 _address;

    // unsigned short is used in Boost to represent port number
    unsigned short _port;
};

} // namespace network
