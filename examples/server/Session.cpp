#include "Session.h"

#include <iostream>
#include <iomanip>
#include <iterator>
#include <map>

#include "comms/units.h"
#include "comms/process.h"

namespace my_proto
{

namespace server
{

namespace
{

using CategoryAttr = Session::InGetMsg::Field_categoryAttr; // Alias to GET message field
using CategoryAttrData = Session::InSetMsg::Field_categoryAttrData; // Alias to SET message field

template <typename std::size_t TCategoryIdx>
class GetCategoryDispatchHelper
{
public:
    template <typename TField>
    static void dispatch(Session& session, const TField& field)
    {
        static_cast<void>(session);
        static_cast<void>(field);
        std::cout << "Proper getX() member function in Session is not implemented!" << std::endl;
        assert(!"Not Implemented");
    }
};

template <>
class GetCategoryDispatchHelper<(std::size_t)CategoryAttr::FieldIdx_ir>
{
public:
    template <typename TField>
    static void dispatch(Session& session, const TField& field)
    {
        using IrAttr = my_proto::field::IrAttrVal;
        using GetFunc = void (Session::*)(); // Pointer to getX() member function of Session

        static const std::map<IrAttr, GetFunc> Map = {
            std::make_pair(IrAttr::Version, &Session::getVersion),
            std::make_pair(IrAttr::Cbit, &Session::getCbit),
        };

        auto attr = field.field_attr().value();
        auto iter = Map.find(attr);
        if (iter == Map.end()) {
            std::cout << "Proper getX() member function in Session is not implemented!" << std::endl;
            assert(!"Not Implemented");
            return;
        }

        auto func = iter->second;
        (session.*func)(); // Call member function;
    }
};

class GetCategoryDispatcher
{
public:
    GetCategoryDispatcher(Session& session) : m_session(session) {}

    template <std::size_t TCategoryIdx, typename TField>
    void operator()(const TField& field)
    {
        std::cout << "GetCategoryDispatcher: processing field: " << field.name() << std::endl;

        GetCategoryDispatchHelper<TCategoryIdx>::dispatch(m_session, field);
    }

private:
    Session& m_session;
};


// ----------------------------------------
template <std::size_t TCategoryIdx, std::size_t TAttrIdx>
class SetDispatchHelper
{
public:
    // Default implementation, if invoked means BUG
    template <typename T>
    static void dispatch(Session& session, T val)
    {
        static_cast<void>(session);
        static_cast<void>(val);
        std::cout << "Proper dispatching for indexes " << TCategoryIdx << " and " << TAttrIdx <<
            " is not implemented or should no be called" << std::endl;
        assert(!"Not implemented");
    }
};

template <>
class SetDispatchHelper<
        (std::size_t)CategoryAttrData::FieldIdx_ir,
        (std::size_t)CategoryAttrData::Field_ir::Field_attrData::FieldIdx_brightness>
{
public:
    template <typename T>
    static void dispatch(Session& session, T val)
    {
        session.setBrightness(val);
    }
};

template <>
class SetDispatchHelper<
        (std::size_t)CategoryAttrData::FieldIdx_ir,
        (std::size_t)CategoryAttrData::Field_ir::Field_attrData::FieldIdx_contrast>
{
public:
    template <typename T>
    static void dispatch(Session& session, T val)
    {
        session.setContrast(val);
    }
};

template <>
class SetDispatchHelper<
        (std::size_t)CategoryAttrData::FieldIdx_ir,
        (std::size_t)CategoryAttrData::Field_ir::Field_attrData::FieldIdx_contrastEnhancement>
{
public:
    template <typename T>
    static void dispatch(Session& session, T val)
    {
        session.setContrastEnhancement(val);
    }
};

template <>
class SetDispatchHelper<
        (std::size_t)CategoryAttrData::FieldIdx_sensor,
        (std::size_t)CategoryAttrData::Field_sensor::Field_attrData::FieldIdx_temperature>
{
public:
    template <typename T>
    static void dispatch(Session& session, T val)
    {
        session.setTemperature(val);
    }
};

template <>
class SetDispatchHelper<
        (std::size_t)CategoryAttrData::FieldIdx_sensor,
        (std::size_t)CategoryAttrData::Field_sensor::Field_attrData::FieldIdx_pressure>
{
public:
    template <typename T>
    static void dispatch(Session& session, T val)
    {
        session.setPressure(val);
    }
};


template <std::size_t TCategoryIdx>
class SetCategoryAttrDispatcher
{
public:
    SetCategoryAttrDispatcher(Session& session) : m_session(session) {}

    template <std::size_t TAttrIdx, typename TField>
    void operator()(const TField& field)
    {
        std::cout << "SetCategoryAttrDispatcher: processing field: " << field.name() << std::endl;
        SetDispatchHelper<TCategoryIdx, TAttrIdx>::dispatch(m_session, field.field_val().value());
    }

private:
    Session& m_session;
};

class SetCategoryDispatcher
{
public:
    SetCategoryDispatcher(Session& session) : m_session(session) {}

    template <std::size_t TCategoryIdx, typename TField>
    void operator()(const TField& field)
    {
        std::cout << "SetCategoryDispatcher: processing field: " << field.name() << std::endl;
        auto& attrData = field.field_attrData(); // Also a variant

        static_cast<void>(field);
        attrData.currFieldExec(SetCategoryAttrDispatcher<TCategoryIdx>(m_session));
    }

private:
    Session& m_session;
};

} // namespace

void Session::start()
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
                terminateSession();
                return;
            }

            std::cout << "<-- " << std::hex;
            std::copy_n(m_readBuf.begin(), bytesCount, std::ostream_iterator<unsigned>(std::cout, " "));
            std::cout << std::dec << std::endl;

            m_inputBuf.insert(m_inputBuf.end(), m_readBuf.begin(), m_readBuf.begin() + bytesCount);
            processInput();
            start();            
        });
}

void Session::handle(InGetMsg& msg)
{
    static_cast<void>(msg);
    std::cout << "Received GET" << std::endl;

    // A bit of Template-Meta-Programming to properly dispatch into right getX function
    // Accessing the vairant field, see my_proto::field::CategoryAttr for reference
    msg.field_categoryAttr().currFieldExec(GetCategoryDispatcher(*this));
}


void Session::handle (InSetMsg& msg)
{
    static_cast<void>(msg);
    std::cout << "Received SET" << std::endl;

    // A bit of Template-Meta-Programming to properly dispatch into right setX function
    // Accessing the vairant field, see my_proto::field::CategoryAttrData for reference
    msg.field_categoryAttrData().currFieldExec(SetCategoryDispatcher(*this));
}

void Session::handle(InputMsg& msg)
{
    // Uses polymorphic name retrieval thanks to usage of comms::option::NameInterface option
    std::cerr << "WARNING: Unexpected message received: " << msg.name() << std::endl;
}

void Session::setBrightness(std::int32_t val)
{
    std::cout << __FUNCTION__ << ": val=" << val << std::endl;

    SetReportMsg respMsg;
    respMsg.field_result().value() = SetResultType::Success;
    respMsg.field_categoryAttr().initField_ir().field_attr().value() =
            SetReportMsg::Field_categoryAttr::Field_ir::Field_attr::ValueType::Brightness;
    sendMsg(respMsg);
}

void Session::setContrast(std::int32_t val)
{
    std::cout << __FUNCTION__ << ": val=" << val << std::endl;
    SetReportMsg respMsg;
    respMsg.field_result().value() = SetResultType::Success;
    respMsg.field_categoryAttr().initField_ir().field_attr().value() =
            SetReportMsg::Field_categoryAttr::Field_ir::Field_attr::ValueType::Contrast;
    sendMsg(respMsg);
}

void Session::setContrastEnhancement(std::int32_t val)
{
    std::cout << __FUNCTION__ << ": val=" << val << std::endl;
    SetReportMsg respMsg;
    respMsg.field_result().value() = SetResultType::Success;
    respMsg.field_categoryAttr().initField_ir().field_attr().value() =
            SetReportMsg::Field_categoryAttr::Field_ir::Field_attr::ValueType::ContrastEnhancement;
    sendMsg(respMsg);
}

void Session::setTemperature(std::int32_t val)
{
    std::cout << __FUNCTION__ << ": val=" << val << std::endl;

    SetReportMsg respMsg;
    respMsg.field_result().value() = SetResultType::Success;
    respMsg.field_categoryAttr().initField_sensor().field_attr().value() =
            SetReportMsg::Field_categoryAttr::Field_sensor::Field_attr::ValueType::Temperature;
    sendMsg(respMsg);
}

void Session::setPressure(std::int32_t val)
{
    std::cout << __FUNCTION__ << ": val=" << val << std::endl;

    SetReportMsg respMsg;
    respMsg.field_result().value() = SetResultType::Success;
    respMsg.field_categoryAttr().initField_sensor().field_attr().value() =
            SetReportMsg::Field_categoryAttr::Field_sensor::Field_attr::ValueType::Pressure;
    sendMsg(respMsg);
}

void Session::getVersion()
{
    std::cout << __FUNCTION__ << std::endl;

    GetReportMsg respMsg;
    respMsg.field_result().value() = GetResultType::Success;
    respMsg
        .field_categoryAttrData() // access CategoryAttrData variant
        .initField_ir() // Initialize it as IR
        .field_attrData() // Access AttrData member
        .initField_version() // Initialize it as Version
        .field_val() // Access its Val field
        .value() = "Some Version"; // Assign version string
    sendMsg(respMsg);
}

void Session::getCbit()
{
    std::cout << __FUNCTION__ << std::endl;

    GetReportMsg respMsg;
    respMsg.field_result().value() = GetResultType::Success;
    auto& data =
        respMsg
            .field_categoryAttrData() // access CategoryAttrData variant
            .initField_ir() // Initialize it as IR
            .field_attrData() // Access AttrData member
            .initField_cbit() // Initialize it as Cbig
            .field_val() // Access its Val field
            .value(); // Access the internal storage (std::vector of int fields);

    assert(data.empty()); // The vector should be empty at the beginning;
    data.resize(4);
    data[0].value() = 3;
    data[1].value() = 12;
    data[2].value() = 31;
    data[3].value() = 802;
    sendMsg(respMsg);
}

void Session::terminateSession()
{
    std::cout << "Terminating session to " << m_remote << std::endl;
    if (m_termCb) {
        m_socket.get_io_service().post(
            [this]()
            {
                m_termCb();
            });
    }
}

void Session::processInput()
{
    if (!m_inputBuf.empty()) {
        auto consumed = comms::processAllWithDispatch(&m_inputBuf[0], m_inputBuf.size(), m_frame, *this);
        m_inputBuf.erase(m_inputBuf.begin(), m_inputBuf.begin() + consumed);
    }
}

void Session::sendMsg(const OutputMsg& msg)
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

    std::cout << "INFO: Sending back\n";
    std::cout << "--> " << std::hex;
    std::copy(outputBuf.begin(), outputBuf.end(), std::ostream_iterator<unsigned>(std::cout, " "));
    std::cout << std::dec << std::endl;
    m_socket.send(boost::asio::buffer(outputBuf));
}

//void Session::sendAck(my_proto::MsgId id)
//{
//    my_proto::message::Ack<OutputMsg> msg;
//    msg.field_msgId().value() = id;


//}

} // namespace server

} // namespace my_proto
