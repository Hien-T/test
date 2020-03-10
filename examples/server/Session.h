#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <boost/asio.hpp>
#include <boost/array.hpp>

#include "my_proto/Message.h"
#include "my_proto/input/ServerInputMessages.h"
#include "my_proto/frame/Frame.h"

namespace my_proto
{

namespace server
{

class Session
{
public:
    using Socket = boost::asio::ip::tcp::socket;
    using TermCallback = std::function<void ()>;

    explicit Session(Socket&& sock) 
      : m_socket(std::move(sock)),
        m_remote(m_socket.remote_endpoint()) 
    {
    };

    template <typename TFunc>
    void setTerminateCallback(TFunc&& func)
    {
        m_termCb = std::forward<TFunc>(func);
    }

    void start();

    using InputMsg = 
        my_proto::Message<
            comms::option::ReadIterator<const std::uint8_t*>,
            comms::option::Handler<Session>,
            comms::option::NameInterface
        >;

    using InSimpleInts = my_proto::message::SimpleInts<InputMsg>;
    using InScaledInts = my_proto::message::ScaledInts<InputMsg>;
    using InFloats = my_proto::message::Floats<InputMsg>;
    using InEnums = my_proto::message::Enums<InputMsg>;
    using InSets = my_proto::message::Sets<InputMsg>;
    using InBitfields = my_proto::message::Bitfields<InputMsg>;
    using InStrings = my_proto::message::Strings<InputMsg>;
    using InDatas = my_proto::message::Datas<InputMsg>;
    using InLists = my_proto::message::Lists<InputMsg>;
    using InOptionals = my_proto::message::Optionals<InputMsg>;
    using InVariants = my_proto::message::Variants<InputMsg>;

    void handle(InSimpleInts& msg);
    void handle(InScaledInts& msg);
    void handle(InFloats& msg);
    void handle(InEnums& msg);
    void handle(InSets& msg);
    void handle(InBitfields& msg);
    void handle(InStrings& msg);
    void handle(InDatas& msg);
    void handle(InLists& msg);
    void handle(InOptionals& msg);
    void handle(InVariants& msg);
    void handle(InputMsg&);

private:

    using OutputMsg = 
        my_proto::Message<
            comms::option::WriteIterator<std::back_insert_iterator<std::vector<std::uint8_t> > >,
            comms::option::LengthInfoInterface,
            comms::option::IdInfoInterface
        >;

    using AllInputMessages = my_proto::input::ServerInputMessages<InputMsg>;

    using Frame = my_proto::frame::Frame<InputMsg, AllInputMessages>;

    void terminateSession();
    void processInput();
    void sendAck(my_proto::MsgId id);

    Socket m_socket;
    TermCallback m_termCb;    
    boost::array<std::uint8_t, 1024> m_readBuf;
    std::vector<std::uint8_t> m_inputBuf;
    Frame m_frame;
    Socket::endpoint_type m_remote;
}; 

using SessionPtr = std::unique_ptr<Session>;

} // namespace server

} // namespace my_proto
