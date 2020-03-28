#pragma once

#include <cstdint>
#include <string>
#include <iterator>
#include <vector>

#include <boost/asio.hpp>
#include <boost/array.hpp>

#include "my_proto/Message.h"
#include "my_proto/input/ClientInputMessages.h"
#include "my_proto/frame/Frame.h"

namespace my_proto
{

namespace client    
{

class Client
{
public:
    Client(boost::asio::io_service& io, const std::string& server, std::uint16_t port);

    bool start();

    using InputMsg = 
        my_proto::Message<
            comms::option::ReadIterator<const std::uint8_t*>,
            comms::option::Handler<Client> 
        >;

    using InGetReportMsg = my_proto::message::GetReport<InputMsg>;
    using InSetReportMsg = my_proto::message::SetReport<InputMsg>;
    
    void handle(InGetReportMsg& msg);
    void handle(InSetReportMsg& msg);
    void handle(InputMsg&);

    void setReportBrightness();   
    void setReportContrast();     
    void setReportContrastEnhancement(); 

    void setReportTemperature();
    void setReportSetPressure();

    void getReportVersion(auto &val);
    void getReportCbit(auto &data);

private:
    using Socket = boost::asio::ip::tcp::socket;

    using OutputMsg = 
        my_proto::Message<
            comms::option::WriteIterator<std::back_insert_iterator<std::vector<std::uint8_t> > >,
            comms::option::LengthInfoInterface,
            comms::option::IdInfoInterface,
            comms::option::NameInterface
        >;

    using OutSetMsg = my_proto::message::Set<OutputMsg>;
    using OutGetMsg = my_proto::message::Get<OutputMsg>;

    using AllInputMessages = my_proto::input::ClientInputMessages<InputMsg>;

    using Frame = my_proto::frame::Frame<InputMsg, AllInputMessages>;


    void readDataFromServer();
    void readDataFromStdin();
    void sendSetBrighness();
    void sendSetContrast();
    void sendSetContrastEnhancement();
    void sendSetTemperature();
    void sendSetPressure();
    void sendGetVersion();
    void sendGetCbit();
    void sendMessage(const OutputMsg& msg);
    void waitForResponse();
    void processInput();

    Socket m_socket;
    boost::asio::deadline_timer m_timer;
    std::string m_server;
    std::uint16_t m_port = 0U;
    Frame m_frame;
    boost::array<std::uint8_t, 32> m_readBuf;
    std::vector<std::uint8_t> m_inputBuf;
};

} // namespace client

} // namespace my_proto
