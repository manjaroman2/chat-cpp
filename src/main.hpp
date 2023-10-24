#include "api.hpp"
#include "tcpsocket.hpp"
#include <mutex>
#include <functional>
#include <stdint.h>
#include <exception>
#include <stdexcept>

namespace Api
{

    class Connection
    {
    public:
        MagicType id;
        std::mutex idLock;
        bool accepted = false;
        std::mutex acceptedLock;
        bool deleted = false;
        std::mutex deletedLock;
        std::string ip;
        int port;
        TCPSocket<> *socket;
        std::array<char, MAX_PRE_MESSAGE_LENGTH> preMessageBuffer;
        char *preMessageBufferFreeSpace = preMessageBuffer.begin();
        std::mutex preMessageBufferLock;

        Connection(std::string ip, int port)
        {
            // preMessageBuffer.begin()
            this->ip = ip;
            this->port = port;
        }
        ~Connection()
        {
            deletedLock.lock();
            if (!deleted)
            {
                deleted = true;
                deletedLock.unlock();
                socket->Close();
            }
            else
                deletedLock.unlock();
        }

        void createSocket();

        void sendMessage(char *messageBuffer, MessageLengthType messageLength)
        {
            buffer_send_socket_all(socket, messageBuffer, messageLength);
        }

        void setAccepted(bool newAccepted = true)
        {
            acceptedLock.lock();
            accepted = newAccepted;
            acceptedLock.unlock();
        }

        bool isAccepted()
        {
            bool acceptedCopy;
            acceptedLock.lock();
            acceptedCopy = accepted;
            acceptedLock.unlock();
            return acceptedCopy;
        }

        template <typename Func>
        void iteratePreMessageBufferChunks(Func func)
        {
            preMessageBufferLock.lock();
            int m = ((uintptr_t)preMessageBufferFreeSpace) / MAX_MESSAGE_LENGTH;
            char *iter = preMessageBuffer.begin();
            for (int i = 0; i < m; i++)
            {
                iter += i * MAX_MESSAGE_LENGTH;
                func(iter, MAX_MESSAGE_LENGTH);
            }
            int n = ((uintptr_t)preMessageBufferFreeSpace) - m * MAX_MESSAGE_LENGTH;
            if (n > 0)
            {
                func(iter, n);
            }
            preMessageBufferLock.unlock();
        }

        void addToPreMessageBuffer(const char *buffer, int length)
        {
            preMessageBufferLock.lock();
            int d = length - (preMessageBuffer.end() - preMessageBufferFreeSpace);
            if (d > 0)
            {
                log_info("Message buffer overflow from %s:%d by %d bytes", ip, port, d);
                return;
            }
            memcpy(preMessageBufferFreeSpace, buffer, length);
            preMessageBufferFreeSpace += length;
            preMessageBufferLock.unlock();
        }
    };

}
