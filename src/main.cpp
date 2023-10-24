#include "tcpserver.hpp"
#include "main.hpp"
#include <algorithm>
#include <ranges>
#include <format>

namespace Api
{
    Connection *connections[MAX_CONNECTIONS];
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
                // Maybe retry logic
                this->setAccepted(false);
                log_info("Connection failed: %d : %s", errorCode, errorMessage);
            });
    }

    // Start while(true) loop
    void serve()
    {
        char magicBuffer[MAGIC_TYPE_SIZE];
        char messageLengthBuffer[MESSAGE_LENGTH_TYPE_SIZE];
        char messageBuffer[MAX_MESSAGE_LENGTH];
        MessageLengthType messageLength;
        MagicType magic, connId;

        Connection *connection = nullptr;
        buffer *buffer;

        std::string ip;
        int port;
        while (true)
        {
            buffer_read_all(STDIN_FILENO, magicBuffer, MAGIC_TYPE_SIZE);
            buffer_read_all(STDIN_FILENO, messageLengthBuffer, MESSAGE_LENGTH_TYPE_SIZE);
            // Convert 2 Bytes to ushort
            memcpy(&messageLength, &messageLengthBuffer, MESSAGE_LENGTH_TYPE_SIZE);
            buffer_read_all(STDIN_FILENO, messageBuffer, messageLength);
            memcpy(&magic, magicBuffer, MAGIC_TYPE_SIZE);
            switch (magic)
            {
            case Magic::CONNECT: // Client requests CONNECT to socket
            {
                connectionsLock.lock();
                if (nextFreeConnection == MAX_CONNECTIONS)
                {
                    connectionsLock.unlock();
                    log_error("  Connection limit reached {}", MAX_CONNECTIONS);
                    break;
                }
                connectionsLock.unlock();
                ip = strtok(messageBuffer, ":");
                port = atoi(strtok(NULL, ":"));
                connection = connection_create(ip, port);
                connection->createSocket();

                // Send confirmation of CONNECT to client
                connection->idLock.lock();
                buffer = api_make_buffer_connect(connection->id);
                connection->idLock.unlock();
                api_buffer_write(buffer);
                break;
            }
            case Magic::DISCONNECT: // Client requests DISCONNECT from socket
            {
                connId = (MagicType)messageLength;
                connectionsLock.lock();
                if (connId > nextFreeConnection - 1) // Is valid Connection
                {
                    connectionsLock.unlock();
                    log_error("  Connection {} is invalid", connId);
                    break;
                }
                connection = connections[connId];
                delete connection;                   // Delete connection
                if (connId < nextFreeConnection - 1) // We are not in the last position
                {                                    // Put last pointer in hole
                    connections[connId] = connections[nextFreeConnection - 1];
                }
                nextFreeConnection--;
                connectionsLock.unlock();

                // Send confirmation of DISCONNECT to client
                buffer = api_make_buffer_disconnect(connId);
                api_buffer_write(buffer);
                break;
            }
            case Magic::ACCEPT_CONNECT: // Client wants to ACCEPT_CONNECT an incoming connection
            {
                connId = (MagicType)messageLength;
                connectionsLock.lock();
                if (connId > nextFreeConnection - 1) // Is valid Connection
                {
                    connectionsLock.unlock();
                    log_error("  Connection {} is invalid", connId);
                    break;
                }
                connection = connections[connId];
                connectionsLock.unlock();
                connection->acceptedLock.lock();
                if (connection->accepted) // Is not already accepted
                {
                    connection->acceptedLock.unlock();
                    log_error("  Connection {} was already accepted", connId);
                    break;
                }
                connection->accepted = true;
                connection->acceptedLock.unlock();
                // Process preMessageBuffer
                connection->iteratePreMessageBufferChunks([&connId, &buffer](char *iter, MessageLengthType length) { //
                    buffer = api_make_buffer_message(connId, iter, length);
                    api_buffer_write(buffer);
                });
                break;
            }
            case Magic::LOG_INFO || Magic::LOG_ERROR:
            {
                // Client should not send log messages
                break;
            }
            default: // Send message to one of connected sockets
            {
                connId = magic;
                connectionsLock.lock();
                if (connId > nextFreeConnection - 1)
                {
                    connectionsLock.unlock();
                    log_error("  Connection {} is invalid", connId);
                    break;
                }
                connection = connections[connId];
                connectionsLock.unlock();
                if (connection->isAccepted())
                {
                    log_error("  Connection {} is not accepted", connId);
                    break;
                }
                connection->sendMessage(messageBuffer, messageLength);

                // TODO Confirm message sent back to client
                break;
            }
            }
        } // while(true)
    }

    int main(int argc, char **argv)
    {
        int listen_port = 8888;
        if (argc > 1)
            listen_port = atoi(argv[1]);

        // Initialize server socket..
        TCPServer<> tcpServer;

        // When a new client connected:
        tcpServer.onNewConnection = [](TCPSocket<> *newClient)
        {
            Connection *connection = nullptr;
            if (nextFreeConnection < MAX_CONNECTIONS)
            {
                connection = new Connection(newClient->remoteAddress().c_str(), newClient->remotePort());
                connection->idLock.lock();
                connectionsLock.lock();
                connection->id = nextFreeConnection;
                connections[nextFreeConnection] = connection;
                nextFreeConnection++;
                connectionsLock.unlock();
            }
            else
            {
                newClient->Close();
                return;
            }
            buffer *buffer0 = api_make_buffer_request_connect(connection->id);
            connection->idLock.unlock();
            api_buffer_write(buffer0);
            log_info("New client: [%s:%d]", connection->ip, connection->port);
            newClient->onRawMessageReceived = [newClient, &connection](const char *message, int length)
            {
                if (length > MAX_MESSAGE_LENGTH) // Incoming message is too long, abort
                    return newClient->Close();
                connection->socket = newClient;
                if (connection->isAccepted()) // Connection accepted
                {
                    connection->idLock.lock();
                    buffer *buffer1 = api_make_buffer_message(connection->id, message, length);
                    connection->idLock.unlock();
                    api_buffer_write(buffer1);
                }
                else
                { // Save messages to buffer while connection is not accepted
                    connection->preMessageBufferLock.lock();
                    int d = length - (connection->preMessageBuffer.end() - connection->preMessageBufferFreeSpace);
                    if (d > 0)
                    {
                        connection->preMessageBufferLock.unlock();
                        newClient->Send(std::format("preMessageBuffer overflow by {} bytes", d).c_str());
                        return;
                    }
                    // We copy some arbitary bytes from a stranger on the internet into memory
                    // This should be safe though, operating systems store this in non-executable memory
                    // As long as we don't overflow the buffer, we should be fine
                    memcpy(connection->preMessageBufferFreeSpace, message, length);
                    connection->preMessageBufferFreeSpace += length;
                    connection->preMessageBufferLock.unlock();
                    log_info("Message from the Client %s:%d with %d bytes into preMessageBuffer",
                             connection->ip, connection->port, length);
                }
            };

            newClient->onSocketClosed = [&connection](int errorCode)
            { connection_socket_close(connection, errorCode); };
        };

        // Bind the server to a port.
        tcpServer.Bind(listen_port, [](int errorCode, std::string errorMessage)
                       { log_info("Binding failed: {} : {}", errorCode, errorMessage); });

        // Start Listening the server.
        tcpServer.Listen([](int errorCode, std::string errorMessage)
                         { log_info("Listening failed: {} : {}", errorCode, errorMessage); });

        log_info("TCP Server started on port {}", listen_port);

        serve();

        // Close the server before exiting the program.
        tcpServer.Close();

        return 0;
    }

}

int main(int argc, char **argv)
{
    return Api::main(argc, argv);
}