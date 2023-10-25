#pragma once

#include "api.hpp"
#include <async-sockets/tcpsocket.hpp>
#include <async-sockets/tcpserver.hpp>
#include <mutex>
#include <algorithm>
#include <ranges>
#include <format>
#include <stdint.h>

namespace Delta
{
    class Connection
    {
    public: // Everything is public as per recommendation by Terry Davis
        MagicType id;
        std::mutex idLock;
        bool accepted = false;
        std::mutex acceptedLock;
        bool deleted = false;
        std::mutex deletedLock;
        std::string ip;
        int port;
        TCPSocket<> *socket;
        std::array<char, Api::MAX_PRE_MESSAGE_LENGTH> preMessageBuffer;
        char *preMessageBufferFreeSpace = preMessageBuffer.begin();
        std::mutex preMessageBufferLock;

        Connection(std::string ip, int port) // Constructor overload bad??
        {
            this->ip = ip.c_str();
            this->port = port;
        }
        Connection(TCPSocket<> *newSocket)
        {
            this->socket = newSocket;
            this->ip = socket->remoteAddress().c_str();
            this->port = socket->remotePort();
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
        void registerWith();
        void destory();
        void socketHandleClose(int errorCode);

        void socketSendMessage(char *messageBuffer, MessageLengthType messageLength) { Api::buffer_send_socket_all(socket, messageBuffer, messageLength); }

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
            int m = ((uintptr_t)preMessageBufferFreeSpace) / Api::MAX_MESSAGE_LENGTH;
            char *iter = preMessageBuffer.begin();
            for (int i = 0; i < m; i++)
            {
                iter += i * Api::MAX_MESSAGE_LENGTH;
                func(iter, Api::MAX_MESSAGE_LENGTH);
            }
            int n = ((uintptr_t)preMessageBufferFreeSpace) - m * Api::MAX_MESSAGE_LENGTH;
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
                Api::log_info("Message buffer overflow from %s:%d by %d bytes", ip, port, d);
                return;
            }
            memcpy(preMessageBufferFreeSpace, buffer, length);
            preMessageBufferFreeSpace += length;
            preMessageBufferLock.unlock();
        }
    };

}