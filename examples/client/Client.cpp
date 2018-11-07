#include "Client.h"

#include <iostream>
#include <sstream>

#include "demo1/MsgId.h"
#include "demo1/AllMessages.h"

namespace demo1
{

namespace client
{

Client::Client(
        boost::asio::io_service& io, 
        const std::string& server,
        std::uint16_t port)    
  : m_socket(io),
    m_stdin(io, ::dup(STDIN_FILENO)),
    m_timer(io),
    m_server(server),
    m_port(port)
{
    if (m_server.empty()) {
        m_server = "localhost";
    }
}

bool Client::start()
{
    boost::asio::ip::tcp::resolver resolver(m_socket.get_io_service());
    auto query = boost::asio::ip::tcp::resolver::query(m_server, std::to_string(m_port));

    boost::system::error_code ec;
    auto iter = resolver.resolve(query, ec);
    if (ec) {
        std::cerr << "ERROR: Failed to resolve \"" << m_server << ':' << m_port << "\" " <<
            "with error: " << ec.message() << std::endl; 
        return false;
    }

    auto endpoint = iter->endpoint();
    m_socket.connect(endpoint, ec);
    if (ec) {
        std::cerr << "ERROR: Failed to connect to \"" << endpoint << "\" " <<
            "with error: " << ec.message() << std::endl; 
        return false;
    }

    std::cout << "INFO: Connected to " << m_socket.remote_endpoint() << std::endl;

    assert(m_stdin.is_open());
    readDataFromServer();
    readDataFromStdin();
    return true;
}

void Client::handle(InAckMsg& msg)
{
    if (msg.field_msgId().value() != m_sentId) {
        std::cerr << "WARNING: Ack for the wrong ID: " << (unsigned)msg.field_msgId().value() << std::endl;
        return;
    }

    std::cout << "INFO: Message acknowledged" << std::endl;
    m_timer.cancel();
    readDataFromStdin();
}

void Client::handle(InputMsg&)
{
    std::cerr << "WARNING: Unexpected message received" << std::endl;
}

void Client::readDataFromServer()
{
    m_socket.async_read_some(
        boost::asio::buffer(m_readBuf),
        [this](const boost::system::error_code& ec, std::size_t bytesCount)
        {
            if (ec == boost::asio::error::operation_aborted) {
                return;
            }

            if (ec) {
                std::cerr << "ERROR: Failed to read with error: " << ec.message() << std::endl;
                m_socket.get_io_service().stop();
                return;
            }

            std::cout << "<-- " << std::hex;
            std::copy_n(m_readBuf.begin(), bytesCount, std::ostream_iterator<unsigned>(std::cout, " "));
            std::cout << std::dec << std::endl;

            m_inputBuf.insert(m_inputBuf.end(), m_readBuf.begin(), m_readBuf.begin() + bytesCount);
            processInput();
            readDataFromServer();            
        });
}

void Client::readDataFromStdin()
{
    std::cout << "\nEnter (new) message ID to send: " << std::endl;
    m_sentId = demo1::MsgId_Ack;
    m_stdinBuf.consume(m_stdinBuf.size());
    boost::asio::async_read_until(
        m_stdin,
        m_stdinBuf,
        '\n',
        [this](const boost::system::error_code& ec, std::size_t bytesCount)
        {
            static_cast<void>(bytesCount);
            do {
                if (ec) {
                    std::cerr << "ERROR: Failed to read from stdin with error: " << ec << std::endl;
                    break;
                }

                //std::cout << "Read " << bytesCount << " bytes" << std::endl;
                std::istream stream(&m_stdinBuf);
                unsigned msgId = 0U;
                stream >> msgId;
                if (!stream.good()) {
                    std::cerr << "ERROR: Unexpected input, use numeric value" << std::endl;
                    break;
                }

                using SendFunc = void (Client::*)();
                static const std::map<unsigned, SendFunc> Map = {
                    std::make_pair(demo1::MsgId_SimpleInts, &Client::sendSimpleInts)
                };

                auto iter = Map.find(msgId);
                if (iter == Map.end()) {
                    std::cerr << "ERROR: Unknown message ID, try another one" << std::endl;
                    break;
                }

                auto func = iter->second;
                (this->*func)();
                return; // Don't read STDIN right away, wait for ACK first
            } while (false);

            readDataFromStdin();
        }
    );
}

void Client::sendSimpleInts()
{
    demo1::message::SimpleInts<OutputMsg> msg;
    msg.field_f9().value() = 12345;
    msg.field_f10().value() = 678910;
    // Keep default value of other fields
    sendMessage(msg);
}

void Client::sendMessage(const OutputMsg& msg)
{
    std::vector<std::uint8_t> outputBuf;
    outputBuf.reserve(m_frame.length(msg));
    auto iter = std::back_inserter(outputBuf);
    auto es = m_frame.write(msg, iter, outputBuf.max_size());
    if (es == comms::ErrorStatus::UpdateRequired) {
        auto updateIter = &outputBuf[0];
        es = m_frame.update(updateIter, outputBuf.size());
    }

    if (es != comms::ErrorStatus::Success) {
        assert(!"Unexpected error");
        return;
    }

    std::cout << "INFO: Sending message: " << msg.name() << '\n';
    std::cout << "--> " << std::hex;
    std::copy(outputBuf.begin(), outputBuf.end(), std::ostream_iterator<unsigned>(std::cout, " "));
    std::cout << std::dec << std::endl;
    m_socket.send(boost::asio::buffer(outputBuf));

    m_sentId = msg.getId();
    waitForAck();
}

void Client::waitForAck()
{
    m_timer.expires_from_now(boost::posix_time::seconds(2));
    m_timer.async_wait(
        [this](const boost::system::error_code& ec)
        {
            if (ec == boost::asio::error::operation_aborted) {
                return;
            }

            std::cerr << "ERROR: Previous message hasn't been acknowledged" << std::endl;
            readDataFromStdin();
        });
}

void Client::processInput()
{
    std::size_t consumed = 0U;
    while (consumed < m_inputBuf.size()) {
        // Smart pointer to the message object.
        Frame::MsgPtr msgPtr; 

        // Get the iterator for reading
        auto begIter = comms::readIteratorFor<InputMsg>(&m_inputBuf[0] + consumed);
        auto iter = begIter;

        // Do the read
        auto es = m_frame.read(msgPtr, iter, m_inputBuf.size() - consumed);
        if (es == comms::ErrorStatus::NotEnoughData) {
            break; // Not enough data in the buffer, stop processing
        } 
    
        if (es == comms::ErrorStatus::ProtocolError) {
            // Something is not right with the data, remove one character and try again
            std::cerr << "WARNING: Corrupted buffer" << std::endl;
            ++consumed;
            continue;
        }

        if (es == comms::ErrorStatus::Success) {
            assert(msgPtr); // If read is successful, msgPtr is expected to hold a valid pointer
            msgPtr->dispatch(*this); // Call appropriate handle() function
        }

        // The iterator for reading has been advanced, update the difference
        consumed += std::distance(begIter, iter);
    }

    m_inputBuf.erase(m_inputBuf.begin(), m_inputBuf.begin() + consumed);
}

} // namespace client

} // namespace demo1
