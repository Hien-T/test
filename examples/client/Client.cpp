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

namespace
{
using CategoryAttr = Client::InSetReportMsg::Field_categoryAttr; // Alias to SET Report message field
using CategoryAttrData = Client::InGetReportMsg::Field_categoryAttrData; // Alias to GET Report message field

template <std::size_t TCategoryIdx, std::size_t TAttrIdx>
class GetReportDispatchHelper
{
public:
    // Default implementation, if invoked means BUG
    template <typename T>
    static void dispatch(Client& client, T val)
    {
        static_cast<void>(client);
        static_cast<void>(val);
        std::cout << "Proper dispatching for indexes " << TCategoryIdx << " and " << TAttrIdx <<
            " is not implemented or should no be called" << std::endl;
        assert(!"Not implemented");
    }
};

template <>
class GetReportDispatchHelper<
        (std::size_t)CategoryAttrData::FieldIdx_ir,
        (std::size_t)CategoryAttrData::Field_ir::Field_attrData::FieldIdx_version>
{
public:
    template <typename T>
    static void dispatch(Client& client, T val)
    {
        client.getReportVersion(val);
    }
};

template <>
class GetReportDispatchHelper<
        (std::size_t)CategoryAttrData::FieldIdx_ir,
        (std::size_t)CategoryAttrData::Field_ir::Field_attrData::FieldIdx_cbit>
{
public:
    template <typename T>
    static void dispatch(Client& client, T val)
    {
        client.getReportCbit(val);
    }
};

template <std::size_t TCategoryIdx>
class GetReportCategoryAttrDispatcher
{
public:
    GetReportCategoryAttrDispatcher(Client& client) : m_client(client) {}

    template <std::size_t TAttrIdx, typename TField>
    void operator()(const TField& field)
    {
        std::cout << "GetReportCategoryAttrDispatcher: processing field: " << field.name() << std::endl;
        GetReportDispatchHelper<TCategoryIdx, TAttrIdx>::dispatch(m_client,field.field_val().value());
    }
private:
    Client& m_client;
};

class GetReportCategoryDispatcher
{
public:
    GetReportCategoryDispatcher(Client& client) : m_client(client) {}

    template <std::size_t TCategoryIdx, typename TField>
    void operator()(const TField& field)
    {
        std::cout << "GetReportCategoryDispatcher: processing field: " << field.name() << std::endl;
        auto& attrData = field.field_attrData(); // Also a variant

        static_cast<void>(field);
        attrData.currFieldExec(GetReportCategoryAttrDispatcher<TCategoryIdx>(m_client));
    }
private:
    Client& m_client;
};


template <typename std::size_t TCategoryIdx>
class SetReportCategoryDispatchHelper
{
public:
    template <typename TField>
    static void dispatch(Client& client, const TField& field)
    {
        static_cast<void>(client);
        static_cast<void>(field);
        std::cout << "Proper SetReportX() member function in client is not implemented!" << std::endl;
        assert(!"Not Implemented");
    }
};

template <>
class SetReportCategoryDispatchHelper<(std::size_t)CategoryAttr::FieldIdx_ir>
{
public:
    template <typename TField>
    static void dispatch(Client& client, const TField& field)
    {
        using IrAttr = my_proto::field::IrAttrVal;
        using SetReportFunc = void (Client::*)(); // Pointer to setReportX() member function of Client

        static const std::map<IrAttr, SetReportFunc> Map = {
            std::make_pair(IrAttr::Brightness, &Client::setReportBrightness),
            std::make_pair(IrAttr::Contrast, &Client::setReportContrast),
            std::make_pair(IrAttr::ContrastEnhancement, &Client::setReportContrastEnhancement),
        };
    
        auto attr = field.field_attr().value();
        auto iter = Map.find(attr);
        if (iter == Map.end()) {
            std::cout << "Proper setReportX() member function in Client is not implemented!" << std::endl;
            assert(!"Not Implemented");
            return;
        }

        auto func = iter->second;
        (client.*func)(); // Call member function;
    }
};

template <>
class SetReportCategoryDispatchHelper<(std::size_t)CategoryAttr::FieldIdx_sensor>
{
public:
    template <typename TField>
    static void dispatch(Client& client, const TField& field)
    {
        using SensorAttr = my_proto::field::SensorAttrVal;
        using SetReportFunc = void (Client::*)(); // Pointer to setReportX() member function of Client

        static const std::map<SensorAttr, SetReportFunc> Map = {
            std::make_pair(SensorAttr::Temperature, &Client::setReportTemperature),
            std::make_pair(SensorAttr::Pressure, &Client::setReportSetPressure)
        };
    
        auto attr = field.field_attr().value();
        auto iter = Map.find(attr);
        if (iter == Map.end()) {
            std::cout << "Proper setReportX() member function in Client is not implemented!" << std::endl;
            assert(!"Not Implemented");
            return;
        }

        auto func = iter->second;
        (client.*func)(); // Call member function;
    }
};

class SetReportCategoryDispatcher
{
public:
    SetReportCategoryDispatcher(Client& client) : m_client(client) {}

    template <std::size_t TCategoryIdx, typename TField>
    void operator()(const TField& field)
    {
        std::cout << "SetReportCategoryDispatcher: processing field: " << field.name() << std::endl;
        
        SetReportCategoryDispatchHelper<TCategoryIdx>::dispatch(m_client, field);
    }

private:
    Client& m_client;
};

} // namespace
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
    static_cast<void>(msg);
    std::cout << "INFO: Received " << msg.doName() << std::endl;
    // TODO: verify correct response (similar to server detecting what SET operation is performed)
    msg.field_categoryAttrData().currFieldExec(GetReportCategoryDispatcher(*this));
    m_timer.cancel();
    readDataFromStdin();
}

void Client::handle(InSetReportMsg& msg)
{
    std::cout << "INFO: Received " << msg.doName() << std::endl;
    // TODO: verify correct response (similar to server detecting what GET operation is performed)
    msg.field_categoryAttr().currFieldExec(SetReportCategoryDispatcher(*this));

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
            &Client::sendGetCbit //6
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
void Client::sendGetCbit()
{
    std::cout << __FUNCTION__ << std::endl;
    using Attr = OutGetMsg::Field_categoryAttr::Field_ir::Field_attr::ValueType; // Alias to my_proto::field::IrAttr::ValueType enum;
    OutGetMsg msg;
    msg.field_categoryAttr().initField_ir().field_attr().value() = Attr::Cbit;
    sendMessage(msg);
}

void Client::setReportBrightness()
{
    std::cout << __FUNCTION__ << std::endl;
}

void Client::setReportContrast()
{
    std::cout << __FUNCTION__ << std::endl;
}  
    
void Client::setReportContrastEnhancement()
{
    std::cout << __FUNCTION__ << std::endl;
} 

void Client::setReportTemperature()
{
    std::cout << __FUNCTION__ << std::endl;
}
    
void Client::setReportSetPressure()
{
    std::cout << __FUNCTION__ << std::endl;
}

void Client::getReportVersion(auto &val)
{
     std::cout << __FUNCTION__ << ": val=" << val << std::endl;
}
    
void Client::getReportCbit(auto &data)
{
     std::cout << __FUNCTION__ << std::endl;

    for(int i=0; i < static_cast<int> (data.size());++i)
    {
        std::cout<< "data["<<std::dec <<i<<"] ="<<data[i].value()<<std::endl;
    }
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
