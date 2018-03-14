#pragma once

#include <boost/asio.hpp>

namespace PosixAsyncFile
{

class FileHandle
{
public:
    
    typedef std::function<void(boost::system::error_code, size_t)> TOperationFinishedCb;
    
    FileHandle(boost::asio::io_service& io_service, uint32_t fd);

    void AsyncWrite(size_t offset, const char* buf, size_t bufSize, const TOperationFinishedCb& writeFinishedCb);
    void AsyncRead(size_t offset, char* buf, size_t bufSize, const TOperationFinishedCb& readFinishedCb);

private:

    static void SignalHandler(const boost::system::error_code& ec, size_t bytesTransferred);

    uint32_t m_fd;
};

}
