#include "delta.hpp"

namespace Delta
{
    Connection *connections[Api::MAX_CONNECTIONS];
    MagicType nextFreeConnection = 0; // = Amount of connections
    std::mutex connectionsLock;

    // C Programmers would say this is bad but they can suck my balls

    void Connection::socketHandleClose(int errorCode)
    {
        idLock.lock();
        Api::log_info("Connection {} closed: {}", id, errorCode);
        deletedLock.lock();
        if (!deleted)
        {
            deleted = true;
            deletedLock.unlock();
            connectionsLock.lock();
            if (id < nextFreeConnection) // We valid
            {
                MagicType connId = id;
                idLock.unlock();
                delete this;
                if (connId < nextFreeConnection - 1) // We are not in the last position
                {                                    // Swap
                    connections[connId] = connections[nextFreeConnection - 1];
                }
                nextFreeConnection--;
            }
            else
            {
                idLock.unlock();
            }
            connectionsLock.unlock();
        }
        else
        {
            idLock.unlock();
            deletedLock.unlock();
        }
    }

    void Connection::createSocket()
    {
        (*socket) = TCPSocket<>([](int errorCode, std::string errorMessage)
                                { Api::log_info("Socket creation error: %d : %s", errorCode, errorMessage); });

        socket->onRawMessageReceived = [this](const char *message, int length)
        {
            idLock.lock();
            Api::buffer *buffer = Api::api_make_buffer_message(id, message, length);
            idLock.unlock();
            Api::api_buffer_write(buffer);
        };

        socket->onSocketClosed = [this](int errorCode)
        { this->socketHandleClose(errorCode); };

        socket->Connect(
            ip, port, [this] { // TODO Send accept to api out
                idLock.lock();
                Api::log_info("Connection %d accepted", id);
                idLock.unlock();
                this->setAccepted();
            },
            [this](int errorCode, std::string errorMessage)
            {
                // TODO Connection refused
                // Maybe retry logic
                this->setAccepted(false);
                Api::log_info("Connection failed: %d : %s", errorCode, errorMessage);
            });
    }

    void Connection::registerWith()
    {
        connectionsLock.lock();
        if (nextFreeConnection < Api::MAX_CONNECTIONS)
        {
            idLock.lock();
            id = nextFreeConnection;
            connections[nextFreeConnection] = this;
            nextFreeConnection++;
            idLock.unlock();
        }
        else
            throw std::invalid_argument("Max connections");
        connectionsLock.unlock();
    }

    void Connection::destory()
    {
        connectionsLock.lock();
        idLock.lock();
        MagicType connId = id;
        if (connId < nextFreeConnection) // We valid
        {
            idLock.unlock();
            delete this;
            if (connId < nextFreeConnection - 1) // We are not in the last position
            {                                    // Swap
                connections[connId] = connections[nextFreeConnection - 1];
            }
            nextFreeConnection--;
        }
        else
            idLock.unlock();
        connectionsLock.unlock();
    }

    void serve()
    {
        char magicBuffer[Api::MAGIC_TYPE_SIZE];
        char messageLengthBuffer[Api::MESSAGE_LENGTH_TYPE_SIZE];
        char messageBuffer[Api::MAX_MESSAGE_LENGTH];
        MessageLengthType messageLength;
        MagicType magic, connId;

        Connection *connection = nullptr;
        // buffer *buffer;

        std::string ip;
        int port;
        while (true)
        {
            Api::buffer_read_all(STDIN_FILENO, magicBuffer, Api::MAGIC_TYPE_SIZE);
            Api::buffer_read_all(STDIN_FILENO, messageLengthBuffer, Api::MESSAGE_LENGTH_TYPE_SIZE);
            // Convert 2 Bytes to ushort
            memcpy(&messageLength, &messageLengthBuffer, Api::MESSAGE_LENGTH_TYPE_SIZE);
            Api::buffer_read_all(STDIN_FILENO, messageBuffer, messageLength);
            memcpy(&magic, magicBuffer, Api::MAGIC_TYPE_SIZE);
            switch (magic)
            {
            case Api::Magic::CONNECT: // Client requests CONNECT to socket
            {
                connectionsLock.lock();
                if (nextFreeConnection == Api::MAX_CONNECTIONS)
                {
                    connectionsLock.unlock();
                    Api::log_error("  Connection limit reached {}", Api::MAX_CONNECTIONS);
                    break;
                }
                connectionsLock.unlock();
                ip = strtok(messageBuffer, ":");
                port = atoi(strtok(NULL, ":"));
                connection = new Connection(ip, port);
                connection->registerWith();
                connection->createSocket();

                // Send confirmation of CONNECT to client
                connection->idLock.lock();
                Api::buffer *buffer = Api::api_make_buffer_connect(connection->id);
                connection->idLock.unlock();
                Api::api_buffer_write(buffer);
                break;
            }
            case Api::Magic::DISCONNECT: // Client requests DISCONNECT from socket
            {
                connId = (MagicType)messageLength;
                connectionsLock.lock();
                if (connId > nextFreeConnection - 1) // Is valid Connection
                {
                    connectionsLock.unlock();
                    Api::log_error("  Connection {} is invalid", connId);
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
                Api::buffer *buffer = Api::api_make_buffer_disconnect(connId);
                api_buffer_write(buffer);
                break;
            }
            case Api::Magic::ACCEPT_CONNECT: // Client wants to ACCEPT_CONNECT an incoming connection
            {
                connId = (MagicType)messageLength;
                connectionsLock.lock();
                if (connId > nextFreeConnection - 1) // Is valid Connection
                {
                    connectionsLock.unlock();
                    Api::log_error("  Connection {} is invalid", connId);
                    break;
                }
                connection = connections[connId];
                connectionsLock.unlock();
                connection->acceptedLock.lock();
                if (connection->accepted) // Is not already accepted
                {
                    connection->acceptedLock.unlock();
                    Api::log_error("  Connection {} was already accepted", connId);
                    break;
                }
                connection->accepted = true;
                connection->acceptedLock.unlock();
                // Process preMessageBuffer
                connection->iteratePreMessageBufferChunks([&connId](char *iter, MessageLengthType length) { //
                    Api::buffer *buffer = Api::api_make_buffer_message(connId, iter, length);
                    Api::api_buffer_write(buffer);
                });
                break;
            }
            case Api::Magic::LOG_INFO || Api::Magic::LOG_ERROR:
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
                    Api::log_error("  Connection {} is invalid", connId);
                    break;
                }
                connection = connections[connId];
                connectionsLock.unlock();
                if (connection->isAccepted())
                {
                    Api::log_error("  Connection {} is not accepted", connId);
                    break;
                }
                connection->socketSendMessage(messageBuffer, messageLength);

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
        {
            listen_port = atoi(argv[1]);
        }
        // Initialize server socket..
        TCPServer<> tcpServer;

        tcpServer.onNewConnection = [](TCPSocket<> *newSocket)
        {
            Connection *connection = nullptr;
            if (nextFreeConnection < Api::MAX_CONNECTIONS)
            {
                connection = new Connection(newSocket);
                connection->idLock.lock();
                connectionsLock.lock();
                connection->id = nextFreeConnection;
                connections[nextFreeConnection] = connection;
                nextFreeConnection++;
                connectionsLock.unlock();
            }
            else
            {
                newSocket->Close();
                return;
            }
            Api::buffer *buffer;
            buffer = Api::api_make_buffer_request_connect(connection->id);
            connection->idLock.unlock();
            api_buffer_write(buffer);
            Api::log_info("New client: [%s:%d]", connection->ip, connection->port);
            connection->socket->onRawMessageReceived = [&connection, &buffer](const char *message, int length)
            {
                if (length > Api::MAX_MESSAGE_LENGTH) // Incoming message is too long, abort
                    return connection->socket->Close();
                if (connection->isAccepted()) // Connection accepted
                {
                    connection->idLock.lock();
                    buffer = Api::api_make_buffer_message(connection->id, message, length);
                    connection->idLock.unlock();
                    api_buffer_write(buffer);
                }
                else
                { // Save messages to buffer while connection is not accepted
                    connection->preMessageBufferLock.lock();
                    int d = length - (connection->preMessageBuffer.end() - connection->preMessageBufferFreeSpace);
                    if (d > 0)
                    {
                        connection->preMessageBufferLock.unlock();
                        connection->socket->Send(std::format("preMessageBuffer overflow by {} bytes", d).c_str());
                        return;
                    }
                    // We copy some arbitary bytes from a stranger on the internet into memory
                    // This should be safe though, operating systems store this in non-executable memory
                    // As long as we don't overflow the buffer, we should be fine
                    memcpy(connection->preMessageBufferFreeSpace, message, length);
                    connection->preMessageBufferFreeSpace += length;
                    connection->preMessageBufferLock.unlock();
                    Api::log_info("Message from the Client %s:%d with %d bytes into preMessageBuffer",
                                  connection->ip, connection->port, length);
                }
            };

            connection->socket->onSocketClosed = [&connection](int errorCode)
            { connection->socketHandleClose(errorCode); };
        };

        // Bind the server to a port.
        tcpServer.Bind(listen_port, [](int errorCode, std::string errorMessage)
                       { Api::log_info("Binding failed: {} : {}", errorCode, errorMessage); });

        // Start Listening the server.
        tcpServer.Listen([](int errorCode, std::string errorMessage)
                         { Api::log_info("Listening failed: {} : {}", errorCode, errorMessage); });

        Api::log_info("TCP Server started on port {}", listen_port);

        serve();

        // Close the server before exiting the program.
        tcpServer.Close();

        return 0;
    }

}

int main(int argc, char **argv)
{
    return Delta::main(argc, argv);
}