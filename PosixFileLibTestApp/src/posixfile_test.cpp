#include <thread>
#include <functional>

#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include <aio.h>
#include <signal.h>
#include <sys/signalfd.h>

#include <iostream>

#include "PosixFileLib/PosixAsyncFile.h"

static boost::asio::io_service g_ioService;

static const int NO_OF_THREADS = std::thread::hardware_concurrency();
static const size_t BUFSIZE = 8192;//65535;

static void ReadFinishedCb(const boost::system::error_code& ec, size_t bytes_transferred, char* buf, size_t offset, PosixAsyncFile::FileHandle* fileHandle, const char* filename)
{
    std::cout << "ReadFinishedCb called on " << filename << "with " << bytes_transferred << " bytes. Current offset=" << offset << std::endl;
    if(bytes_transferred == BUFSIZE)
    {
        //There is more data
        offset += bytes_transferred;
        
        fileHandle->AsyncRead(offset, buf, BUFSIZE, std::bind(&ReadFinishedCb, std::placeholders::_1, std::placeholders::_2, buf, offset, fileHandle, filename));
    }
    else
    {
        std::cout << "File " << filename << " finished reading." << std::endl;
        delete[] buf;
    }

}

static void WriteFinishedCb(const boost::system::error_code& ec, size_t bytes_transferred, char* buf, int fd, size_t offset)
{
    std::cout << "WriteFinishedCb called with " << bytes_transferred << std::endl;
}

int main(int argc, char **argv)
{
    std::cout << "PosixFile TestApp" << std::endl;
    if(argc < 2)
    {
      std::cout << "Usage " << argv[0] << " [pathToFile]... " << std::endl;
      return 1;
    }
    
    std::vector<std::unique_ptr<PosixAsyncFile::FileHandle>> posixAsyncFileHandles;
    for(int i = 1; i < argc; ++i)
    {
        
        const char* filename = argv[i];
        int filedes = open(filename, O_RDONLY);
        if(filedes < 0)
        {
            std::cout << "Open " << filename << " failed with error " << errno << std::endl;
            return 2;
        }
        
        posixAsyncFileHandles.emplace_back(new PosixAsyncFile::FileHandle(g_ioService, filedes));
        PosixAsyncFile::FileHandle* fileHandle = posixAsyncFileHandles.back().get();
        char* buf = new char[BUFSIZE];
        fileHandle->AsyncRead(0, buf, BUFSIZE, std::bind(&ReadFinishedCb, std::placeholders::_1, std::placeholders::_2, buf, 0, fileHandle, filename));

        /*
        int write_fd = open ("/tmp/mytest_file", O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU);
        if(write_fd<0)
        {
            std::cout << "open failed" << std::endl;
            std::terminate();
        }
        PosixAsyncFile::FileHandle* fileHandleWrite = new PosixAsyncFile::FileHandle(g_ioService, write_fd);
        char* writeBuf = new char[BUFSIZE];
        memset(writeBuf, 'a', BUFSIZE);
        fileHandleWrite->AsyncWrite(0, writeBuf, BUFSIZE, std::bind(&WriteFinishedCb, std::placeholders::_1, std::placeholders::_2, writeBuf, write_fd, 0));
        */
    }
    
    std::vector<std::thread> threadgroup;
    for(int i = 0; i < NO_OF_THREADS; i++)
    {
       threadgroup.emplace_back(boost::bind(&boost::asio::io_service::run, &g_ioService));
    }
    
    for(std::thread& t : threadgroup)
    {
        t.join();
    }
    
    return 0;
}
