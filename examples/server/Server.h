#pragma once

#include <cstdint>
#include <list>

#include <boost/asio.hpp>

#include "Session.h"

namespace my_proto
{

namespace server    
{

class Server
{
public:
    Server(boost::asio::io_service& io, std::uint16_t port);

    bool start();
private:
    using Acceptor = boost::asio::ip::tcp::acceptor;
    using Socket = Session::Socket;

    void acceptNewConnection();

    Acceptor m_acceptor;
    Socket m_socket;
    std::uint16_t m_port = 0U;
    std::list<SessionPtr> m_sessions;
};

} // namespace server

} // namespace my_proto
