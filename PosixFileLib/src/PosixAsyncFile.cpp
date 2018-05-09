#include "PosixFileLib/PosixAsyncFile.h"

#include <aio.h>
#include <signal.h>
#include <sys/signalfd.h>

#include <iostream>

namespace
{

static const uint32_t SIGNAL_NO = SIGRTMIN + 3;

static char g_SignalBuffer[8192];

static std::mutex g_RunningOperationsMutex;
static size_t g_RunningOperations(0);

static std::once_flag stream_descriptor_intialization_flag;
static std::unique_ptr<boost::asio::posix::stream_descriptor> g_signalSocketPtr;


static int32_t InstallSignalFd(uint32_t signalNo)
{
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, signalNo);

    /* Block signals so that they aren't handled
       according to their default dispositions */
    sigprocmask(SIG_BLOCK, &mask, NULL);

    int sfd = signalfd(-1, &mask, SFD_NONBLOCK);
    if(sfd < 0)
    {
        //This is not expected!
        std::cout << "signalfd returned " << sfd << std::endl;
        std::terminate();
    }
    return sfd;
}

struct IoOperation
{
    aiocb m_aioCb = {0};
    boost::asio::io_service& m_io_service;
    PosixAsyncFile::FileHandle::TOperationFinishedCb m_callback;

    IoOperation(int32_t fd, char* buf, size_t bufSize, size_t offset, 
                const PosixAsyncFile::FileHandle::TOperationFinishedCb& callback, boost::asio::io_service& io_service) : m_callback(callback), m_io_service(io_service)
    {
        m_aioCb.aio_buf = buf;
        m_aioCb.aio_fildes = fd;
        m_aioCb.aio_nbytes = bufSize;
        m_aioCb.aio_offset = offset;
        m_aioCb.aio_sigevent.sigev_notify = SIGEV_SIGNAL;
        m_aioCb.aio_sigevent.sigev_signo = SIGNAL_NO;
        m_aioCb.aio_sigevent.sigev_value.sival_ptr = this;
    }
};

}


namespace PosixAsyncFile
{

FileHandle::FileHandle(boost::asio::io_service& io_service, uint32_t fd) : m_fd(fd), m_io_service(io_service)
{
    //Initialize global stream_descriptor for reading signals
    std::call_once(stream_descriptor_intialization_flag, [&io_service]()
    {
        g_signalSocketPtr.reset(new boost::asio::posix::stream_descriptor(io_service, InstallSignalFd(SIGNAL_NO)));
    });
}

void FileHandle::AsyncWrite(size_t offset, const char* buf, size_t bufSize, const TOperationFinishedCb& writeFinishedCb)
{
    IoOperation* ioOp = new IoOperation(m_fd, const_cast<char*>(buf), bufSize, offset, writeFinishedCb, m_io_service);

    int retval = aio_write(&ioOp->m_aioCb);
    assert(retval == 0);

    {
        std::lock_guard<std::mutex> l(g_RunningOperationsMutex);
        if(g_RunningOperations++ == 0)
        {
            g_signalSocketPtr->async_read_some(boost::asio::buffer(g_SignalBuffer, sizeof(g_SignalBuffer)), 
                std::bind(&FileHandle::SignalHandler, std::placeholders::_1, std::placeholders::_2));
        }
    }
}

void FileHandle::AsyncRead(size_t offset, char* buf, size_t bufSize, const TOperationFinishedCb& readFinishedCb)
{
    IoOperation* ioOp = new IoOperation(m_fd, buf, bufSize, offset, readFinishedCb, m_io_service);

    int retval = aio_read(&ioOp->m_aioCb);
    assert(retval == 0);

    {
        std::lock_guard<std::mutex> l(g_RunningOperationsMutex);
        if(g_RunningOperations++ == 0)
        {
            g_signalSocketPtr->async_read_some(boost::asio::buffer(g_SignalBuffer, sizeof(g_SignalBuffer)), 
                std::bind(&FileHandle::SignalHandler, std::placeholders::_1, std::placeholders::_2));
        }
    }
}

/*static*/ void FileHandle::SignalHandler(const boost::system::error_code& ec, size_t bytes_transferred)
{
    if(!ec)
    {
        assert((bytes_transferred % sizeof(signalfd_siginfo)) == 0);
#ifndef NDEBUG
        std::cout << "SignalHandler I/O completion signal received with "<< (bytes_transferred/sizeof(signalfd_siginfo)) <<" signals" << std::endl;
#endif
        ptrdiff_t offset = 0;
        size_t finishedOperations = 0;
        while(bytes_transferred)
        {
            struct signalfd_siginfo* si = reinterpret_cast<struct signalfd_siginfo*>(g_SignalBuffer + offset);
            bytes_transferred -= sizeof(struct signalfd_siginfo);
            offset += sizeof(struct signalfd_siginfo);
            IoOperation* ioOp = reinterpret_cast<IoOperation*>(si->ssi_ptr);
            aiocb* aioStruct = &ioOp->m_aioCb;
            int retVal = aio_return(aioStruct);
            if(retVal >= 0)
            {
                //I/O finished successfully
#ifndef NDEBUG
                std::cout << "Transferred " << retVal << " Bytes from fd=" << aioStruct->aio_fildes << ". I/O-Operation finished." << std::endl;
#endif
                ioOp->m_io_service.post(std::bind(ioOp->m_callback, retVal == 0 ? boost::asio::error::eof : boost::system::error_code(), retVal));
            }
            else
            {
                int32_t error = aio_error(aioStruct);
#ifndef NDEBUG
                std::cout << "Error: " << retVal << " errno: " << error << std::endl;
#endif
                ioOp->m_io_service.post(std::bind(ioOp->m_callback, boost::system::error_code(error, boost::system::system_category()), 0));
            }
            ++finishedOperations;
            //IO_Operation is finished and can be deleted
            delete ioOp;
        }

        {
            std::lock_guard<std::mutex> l(g_RunningOperationsMutex);
            if(g_RunningOperations -= finishedOperations)
            {
                g_signalSocketPtr->async_read_some(boost::asio::buffer(g_SignalBuffer, sizeof(g_SignalBuffer)), 
                        std::bind(&FileHandle::SignalHandler, std::placeholders::_1/*boost::asio::placeholders::error*/, std::placeholders::_2/*boost::asio::placeholders::bytes_transferred*/));
            }
            else
            {
#ifndef NDEBUG
                std::cout << "No more Operations. Stopping loop." << std::endl;
#endif
            }
        }

    }
    else
    {
        //This is not expected!
        std::cout << ec.message() << std::endl;
        std::terminate();
    }
}


}
