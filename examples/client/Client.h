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

    using InAckMsg = my_proto::message::Ack<InputMsg>;
    
    void handle(InAckMsg& msg);
    void handle(InputMsg&);

private:
    using Socket = boost::asio::ip::tcp::socket;

    using OutputMsg = 
        my_proto::Message<
            comms::option::WriteIterator<std::back_insert_iterator<std::vector<std::uint8_t> > >,
            comms::option::LengthInfoInterface,
            comms::option::IdInfoInterface,
            comms::option::NameInterface
        >;

    using AllInputMessages = my_proto::input::ClientInputMessages<InputMsg>;

    using Frame = my_proto::frame::Frame<InputMsg, AllInputMessages>;


    void readDataFromServer();
    void readDataFromStdin();
    void sendSimpleInts();
    void sendScaledInts();
    void sendFloats();
    void sendEnums();
    void sendSets();
    void sendBitfields();
    void sendStrings();
    void sendDatas();
    void sendLists();
    void sendOptionals();
    void sendVariants();
    void sendMessage(const OutputMsg& msg);
    void waitForAck();
    void processInput();

    Socket m_socket;
    boost::asio::deadline_timer m_timer;
    std::string m_server;
    std::uint16_t m_port = 0U;
    Frame m_frame;
    my_proto::MsgId m_sentId = my_proto::MsgId_Ack;
    boost::array<std::uint8_t, 32> m_readBuf;
    std::vector<std::uint8_t> m_inputBuf;
};

} // namespace client

} // namespace my_proto
