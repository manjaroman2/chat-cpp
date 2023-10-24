#include "tcpserver.hpp"
#include "main.hpp"
#include <algorithm>
#include <ranges>

namespace Api
{
    // Start while(true) loop
    void start_api()
    {
        char magicBuffer[MAGIC_TYPE_SIZE];
        char messageLengthBuffer[MESSAGE_LENGTH_TYPE_SIZE];
        char messageBuffer[MAX_MESSAGE_LENGTH];
        MessageLengthType messageLength;
        MagicType magic, connId;

        Connection *connection = nullptr;

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
            case Magic::CONNECT:
            {
                connectionsLock.lock();
                if (connections.size() == MAX_CONNECTIONS)
                {
                    connectionsLock.unlock();
                    log_error("  Connection limit reached (%d)", MAX_CONNECTIONS);
                    break;
                }
                connectionsLock.unlock();
                ip = strtok(messageBuffer, ":");
                port = atoi(strtok(NULL, ":"));
                // const std::string &r = std::string("test");
                const Connection &a = Connection(ip, port);

                // connection = Connection(ip, port);
                connnection_register(connection);
                api_create_connect(connection->getId());
                break;
            }
            case Magic::DISCONNECT:
            {
                connId = (MagicType)messageLength;
                connectionsLock.lock();
                if (connId > connections.size() - 1)
                {
                    connectionsLock.unlock();
                    log_error("  Connection %d is invalid", connId);
                    break;
                }
                connectionsLock.unlock();
                connection_destroy_by_id(connId);
                break;
            }
            case Magic::ACCEPT_CONNECT: // Need this to accept incoming messages
            {
                connId = (MagicType)messageLength;
                connectionsLock.lock();
                if (connId > connections.size() - 1)
                {
                    connectionsLock.unlock();
                    log_error("  Connection %d is invalid", connId);
                    break;
                }
                if (connections[connId].isAccepted())
                {
                    connectionsLock.unlock();
                    log_error("  Connection %d was already accepted", connId);
                    break;
                }
                connection = &connections[connId];
                connectionsLock.unlock();
                connection->setAccepted();
                connection->iteratePreMessageBufferChunks([&connId](char *iter, MessageLengthType length)
                                                          { api_message(connId, iter, length); });
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
                if (connId > connections.size() - 1)
                {
                    connectionsLock.unlock();
                    log_error("  Connection %d is invalid", connId);
                    break;
                }
                if (connections[connId].isAccepted())
                {
                    connectionsLock.unlock();
                    log_error("  Connection %d is not accepted", connId);
                    break;
                }
                connection = &connections[connId];
                connectionsLock.unlock();
                connection->sendMessage(messageBuffer, messageLength);
                break;
            }
            }
        } // while (true)
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
            // log_info("New client: [%s:%d]", newClient->remoteAddress().c_str(), newClient->remotePort());
            Connection connection = Connection(newClient->remoteAddress().c_str(), newClient->remotePort());
            MagicType id = connnection_register(&connection);
            api_req_connect(id);
            newClient->onRawMessageReceived = [newClient, &connection](const char *message, int length)
            {
                if (length > MAX_MESSAGE_LENGTH) // Incoming message is too long, abort
                    return newClient->Close();
                // Connection accepted
                if (connection.isAccepted())
                    api_message(connection.getId(), message, length);
                else
                { // Save messages to buffer while connection is not accepted
                    connection.addToPreMessageBuffer(message, length);
                    log_info("Message from the Client %s:%d with %d bytes into preMessageBuffer",
                             connection.ip, connection.port, length);
                    // TODO
                }
            };

            newClient->onSocketClosed = [newClient, &connection](int errorCode)
            {
                log_info(
                    "Socket closed: %s:%d -> %d",
                    newClient->remoteAddress().c_str(),
                    newClient->remotePort(),
                    errorCode);

                delete &connection;

                // TODO
            };
        };

        // Bind the server to a port.
        tcpServer.Bind(listen_port, [](int errorCode, std::string errorMessage)
                       { log_info("Binding failed: %d : %s", errorCode, errorMessage); });

        // Start Listening the server.
        tcpServer.Listen([](int errorCode, std::string errorMessage)
                         { log_info("Listening failed: %d : %s", errorCode, errorMessage); });

        log_info("TCP Server started on port %d", listen_port);

        start_api();

        // Close the server before exiting the program.
        tcpServer.Close();

        return 0;
    }

}
