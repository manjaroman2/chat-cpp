#include "api.hpp"
#include "tcpsocket.hpp"
#include <mutex>
#include <functional>
#include <vector>
#include <algorithm>
#include <stdint.h>

namespace Api
{

    class Connection
    {
    private:
        std::array<char, MAX_PRE_MESSAGE_LENGTH> preMessageBuffer;

        MagicType id;
        std::mutex id_lock;
        bool closed = false;
        std::mutex closedLock;
        bool accepted = false;
        std::mutex acceptedLock;

    public:
        std::string ip;
        int port;
        TCPSocket<> *socket;
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
            if (!isClosed())
                socket->Close();
        }
        void createSocket()
        {
            (*socket) = TCPSocket<>([](int errorCode, std::string errorMessage)
                                    { log_info("Socket creation error: %d : %s", errorCode, errorMessage); });

            socket->onRawMessageReceived = [this](const char *message, int length)
            {
                api_message(getId(), message, length);
            };

            socket->onSocketClosed = [this](int errorCode)
            {
                log_info("Connection %d closed: %d", getId(), errorCode);
                this->setClosed();
            };

            socket->Connect(
                ip, port, [this] { // TODO Send accept to api out
                    log_info("Connection %d accepted", getId());
                    this->setAccepted();
                },
                [this](int errorCode, std::string errorMessage)
                {
                    // TODO Connection refused
                    this->setAccepted(false);
                    log_info("Connection failed: %d : %s", errorCode, errorMessage);
                });
        }

        void setId(MagicType newId)
        {
            id_lock.lock();
            id = newId;
            id_lock.unlock();
        }

        MagicType getId()
        {
            MagicType idCopy;
            id_lock.lock();
            idCopy = id;
            id_lock.unlock();
            return idCopy;
        }

        void sendMessage(char *messageBuffer, MessageLengthType messageLength)
        {
            buffer_send_socket_all(socket, messageBuffer, messageLength);
        }

        void setClosed()
        {
            closedLock.lock();
            closed = true;
            closedLock.unlock();
        }

        bool isClosed()
        {
            bool closedCopy;
            closedLock.lock();
            closedCopy = closed;
            closedLock.unlock();
            return closedCopy;
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

    std::vector<Connection> connections;
    std::mutex connectionsLock;

    void connection_destroy_by_id(MagicType connId)
    {
        connectionsLock.lock();
        if (connId < connections.size() - 1)
            std::iter_swap(connections.begin() + connId, connections.end() - 1);
        // std::swap(connections[connId], connections.back());
        connections.pop_back();
        connectionsLock.unlock();
    }

    void connection_destroy(Connection connection)
    {
        connectionsLock.lock();
        MagicType id = connection.getId();
        if (id < connections.size() - 1)
        {
            std::iter_swap(connections.begin() + id, connections.end() - 1);
        }
        // std::swap(connections[id], connections.back());
        connections.pop_back();
        connectionsLock.unlock();
    }

    // Returns the connection id
    MagicType connnection_register(Connection *connection)
    {
        connectionsLock.lock();
        MagicType id = connections.size();
        connection->setId(id);
        connections.push_back(*connection);
        connectionsLock.unlock();
        return id;
    }
}
