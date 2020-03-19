#include "Client.h"

#include <iostream>
#include <sstream>

#include "my_proto/MsgId.h"
#include "comms/units.h"
#include "comms/process.h"

namespace my_proto
{

namespace client
{

Client::Client(
        boost::asio::io_service& io, 
        const std::string& server,
        std::uint16_t port)    
  : m_socket(io),
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
    readDataFromServer();
    readDataFromStdin();
    return true;
}

void Client::handle(InGetReportMsg& msg)
{
    std::cout << "INFO: Received " << msg.doName() << std::endl;
    // TODO: verify correct response (similar to server detecting what SET operation is performed)

    m_timer.cancel();
    readDataFromStdin();
}

void Client::handle(InSetReportMsg& msg)
{
    std::cout << "INFO: Received " << msg.doName() << std::endl;
    // TODO: verify correct response (similar to server detecting what GET operation is performed)

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
    std::cout << "\nEnter (new) operation index: " << std::endl;
    do {
        // Unfortunatelly Windows doesn't provide an easy way to 
        // asynchronously read from stdin with boost::asio,
        // read synchronously. DON'T COPY-PASTE TO PRODUCTION CODE!!!
        unsigned sendIdx = 0U;
        std::cin >> sendIdx;
        if (!std::cin.good()) {
            std::cerr << "ERROR: Unexpected input, use numeric value" << std::endl;
            std::cin.clear();
            std::cin.ignore();
            break;
        }

        using SendFunc = void (Client::*)();
        static const SendFunc Map[] = {
            &Client::sendSetBrighness, // 0
            &Client::sendSetContrast, // 1
            &Client::sendSetContrastEnhancement, // 2
            &Client::sendSetTemperature, // 3
            &Client::sendSetPressure,  // 4
            &Client::sendGetVersion,  // 5
        };
        static const std::size_t MapSize = std::extent<decltype(Map)>::value;
        if (MapSize <= sendIdx) {
            std::cerr << "ERROR: Unknown send operation" << std::endl;
            break;
        }

        auto func = Map[sendIdx];
        (this->*func)();
        return; // Don't read STDIN right away, wait for response
    } while (false);

    m_socket.get_io_service().post(
        [this]()
        {
            readDataFromStdin();
        });
}

void Client::sendSetBrighness()
{
    std::cout << __FUNCTION__ << std::endl;
    OutSetMsg msg;
    msg.field_categoryAttrData().initField_ir().field_attrData().initField_brightness().field_val().value() = 1000;
    sendMessage(msg);
}

void Client::sendSetContrast()
{
    std::cout << __FUNCTION__ << std::endl;
    OutSetMsg msg;
    msg.field_categoryAttrData().initField_ir().field_attrData().initField_contrast().field_val().value() = 2000;
    sendMessage(msg);
}

void Client::sendSetContrastEnhancement()
{
    std::cout << __FUNCTION__ << std::endl;
    OutSetMsg msg;
    msg.field_categoryAttrData().initField_ir().field_attrData().initField_contrastEnhancement().field_val().value() = 2000;
    sendMessage(msg);
}

void Client::sendSetTemperature()
{
    std::cout << __FUNCTION__ << std::endl;
    OutSetMsg msg;
    msg.field_categoryAttrData().initField_sensor().field_attrData().initField_temperature().field_val().value() = 3000;
    sendMessage(msg);
}

void Client::sendSetPressure()
{
    std::cout << __FUNCTION__ << std::endl;
    OutSetMsg msg;
    msg.field_categoryAttrData().initField_sensor().field_attrData().initField_pressure().field_val().value() = 4000;
    sendMessage(msg);
}

void Client::sendGetVersion()
{
    std::cout << __FUNCTION__ << std::endl;
    using Attr = OutGetMsg::Field_categoryAttr::Field_ir::Field_attr::ValueType; // Alias to my_proto::field::IrAttr::ValueType enum;
    OutGetMsg msg;
    msg.field_categoryAttr().initField_ir().field_attr().value() = Attr::Version;
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

    waitForResponse();
}

void Client::waitForResponse()
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
    if (!m_inputBuf.empty()) {
        auto consumed = comms::processAllWithDispatch(&m_inputBuf[0], m_inputBuf.size(), m_frame, *this);
        m_inputBuf.erase(m_inputBuf.begin(), m_inputBuf.begin() + consumed);
    }
}

} // namespace client

} // namespace my_proto
