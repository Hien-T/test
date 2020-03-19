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

    using InGetMsg = my_proto::message::Get<InputMsg>;
    using InSetMsg = my_proto::message::Set<InputMsg>;

    void handle(InGetMsg& msg);
    void handle(InSetMsg& msg);
    void handle(InputMsg&);

    void setBrightness(std::int32_t val);
    void setContrast(std::int32_t val);
    void setContrastEnhancement(std::int32_t val);
    void setTemperature(std::int32_t val);
    void setPressure(std::int32_t val);

    void getVersion();
    void getCbit();
private:

    using OutputMsg = 
        my_proto::Message<
            comms::option::WriteIterator<std::back_insert_iterator<std::vector<std::uint8_t> > >,
            comms::option::LengthInfoInterface,
            comms::option::IdInfoInterface
        >;

    using GetReportMsg = my_proto::message::GetReport<OutputMsg>;
    using SetReportMsg = my_proto::message::SetReport<OutputMsg>;

    using AllInputMessages = my_proto::input::ServerInputMessages<InputMsg>;

    using Frame = my_proto::frame::Frame<InputMsg, AllInputMessages>;

    using GetResultType = GetReportMsg::Field_result::ValueType;
    using SetResultType = SetReportMsg::Field_result::ValueType;

    void terminateSession();
    void processInput();
    void sendMsg(const OutputMsg& msg);

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
