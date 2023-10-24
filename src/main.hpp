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
    private:
        bool closed = false;
        std::mutex closedLock;

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

    // std::vector<Connection> connections;
    // std::list<Connection> connections;
    // std::array<Connection, MAX_CONNECTIONS> connections;
    Connection *connections[MAX_CONNECTIONS];
    // Connection *connections[MAX_CONNECTIONS]
    MagicType nextFreeConnection = 0; // = Amount of connections
    std::mutex connectionsLock;

    void connection_destroy_by_id(MagicType connId)
    { // This shits so easy when you use C style arrays
        connectionsLock.lock();
        if (connId < nextFreeConnection) // We valid
        {
            delete (&connections[connId]);
            if (connId < nextFreeConnection - 1) // We are not in the last position
            {                                    // Swap
                connections[connId] = connections[nextFreeConnection - 1];
            }
            nextFreeConnection--;
        }
        connectionsLock.unlock();
    }

    void connection_destroy(Connection *connection)
    {
        connectionsLock.lock();
        connection->idLock.lock();
        MagicType connId = connection->id;
        if (connId < nextFreeConnection) // We valid
        {
            connection->idLock.unlock();
            delete (&connection);
            if (connId < nextFreeConnection - 1) // We are not in the last position
            {                                    // Swap
                connections[connId] = connections[nextFreeConnection - 1];
            }
            nextFreeConnection--;
        }
        else
            connection->idLock.unlock();
        connectionsLock.unlock();
    }

    // Returns the connection id
    void connnection_register(Connection *connection)
    {
        connectionsLock.lock();
        if (nextFreeConnection < MAX_CONNECTIONS)
        {
            connection->idLock.lock();
            connection->id = nextFreeConnection;
            connections[nextFreeConnection] = connection;
            nextFreeConnection++;
            connection->idLock.unlock();
        }
        else
            throw std::invalid_argument("Max connections");
        connectionsLock.unlock();
    }

    Connection *connection_create(std::string ip, int port)
    {
        Connection *conn = new Connection(ip, port);
        connnection_register(conn);
        return conn;
    }

    void connection_socket_close(Connection *connection, int errorCode)
    {
        connection->idLock.lock();
        log_info("Connection %d closed: %d", connection->id, errorCode);
        connection->deletedLock.lock();
        if (!connection->deleted)
        {
            connection->deleted = true;
            connection->deletedLock.unlock();
            connectionsLock.lock();
            if (connection->id < nextFreeConnection) // We valid
            {
                MagicType connId = connection->id;
                connection->idLock.unlock();
                delete connection;
                if (connId < nextFreeConnection - 1) // We are not in the last position
                {                                    // Swap
                    connections[connId] = connections[nextFreeConnection - 1];
                }
                nextFreeConnection--;
            }
            else
            {
                connection->idLock.unlock();
            }
            connectionsLock.unlock();
        }
        else
        {
            connection->idLock.unlock();
            connection->deletedLock.unlock();
        }
    }

    void Connection::createSocket()
    {
        (*socket) = TCPSocket<>([](int errorCode, std::string errorMessage)
                                { log_info("Socket creation error: %d : %s", errorCode, errorMessage); });

        socket->onRawMessageReceived = [this](const char *message, int length)
        {
            idLock.lock();
            buffer *buffer = api_make_buffer_message(id, message, length);
            idLock.unlock();
            api_buffer_write(buffer);
        };

        socket->onSocketClosed = [this](int errorCode)
        { connection_socket_close(this, errorCode); };
        // socket->onSocketClosed = [this](int errorCode)
        // {
        //     idLock.lock();
        //     log_info("Connection %d closed: %d", id, errorCode);
        //     deletedLock.lock();
        //     if (!deleted)
        //     {
        //         deleted = true;
        //         deletedLock.unlock();
        //         connectionsLock.lock();
        //         if (id < nextFreeConnection) // We valid
        //         {
        //             MagicType connId = id;
        //             idLock.unlock();
        //             delete this;
        //             if (connId < nextFreeConnection - 1) // We are not in the last position
        //             {                                    // Swap
        //                 connections[connId] = connections[nextFreeConnection - 1];
        //             }
        //             nextFreeConnection--;
        //         }
        //         else
        //         {
        //             idLock.unlock();
        //         }
        //         connectionsLock.unlock();
        //     }
        //     else
        //     {
        //         idLock.unlock();
        //         deletedLock.unlock();
        //     }
        // };

        socket->Connect(
            ip, port, [this] { // TODO Send accept to api out
                idLock.lock();
                log_info("Connection %d accepted", id);
                idLock.unlock();
                this->setAccepted();
            },
            [this](int errorCode, std::string errorMessage)
            {
                // TODO Connection refused
                //
                this->setAccepted(false);
                log_info("Connection failed: %d : %s", errorCode, errorMessage);
            });
    }

}
