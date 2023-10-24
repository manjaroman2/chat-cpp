#include "api.hpp"
#include <iostream>
#include <stdio.h>

int main(int argc, char **argv)
{
    MagicType magic, connId;
    MessageLengthType messageLength;
    char magicBuffer[Api::MAGIC_TYPE_SIZE];
    char messageLengthBuffer[Api::MESSAGE_LENGTH_TYPE_SIZE];
    char messageBuffer[Api::MAX_MESSAGE_LENGTH];
    Api::buffer_read_all(STDIN_FILENO, magicBuffer, Api::MAGIC_TYPE_SIZE);
    Api::buffer_read_all(STDIN_FILENO, messageLengthBuffer, Api::MESSAGE_LENGTH_TYPE_SIZE);
    // Convert 2 Bytes to ushort
    memcpy(&messageLength, &messageLengthBuffer, Api::MESSAGE_LENGTH_TYPE_SIZE);
    Api::buffer_read_all(STDIN_FILENO, messageBuffer, messageLength);
    memcpy(&magic, magicBuffer, Api::MAGIC_TYPE_SIZE);
    switch (magic)
    {
    case Api::LOG_INFO:
        std::cout << "LOG_INFO: " << messageBuffer << std::endl;
        break;
    }
    return 0;
}