#include "tcpsocket.hpp"
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <fmt/core.h>
#include <assert.h>

/* ** API specification **
 *  The magic byte(s) encode
 *     1. The fundamental logic for API communication
 *     2. The connection numbers
 *
 *  The message length (ML) bytes encode
 *     1. The message length without the magic byte.
 *     2. Sometimes it encodes the connection number (fundamental connection logic)
 *     This is a deliberate design choice, done to save bytes.
 *
 *  A message is all the bytes following the ML. It has to be encodable by the MLENGTH.
 *  For example if MLENGTH is unsigned short, then the MAX_MESSAGE_LENGTH is 65535.
 */
namespace Api
{
#define LOG_FILENO STDOUT_FILENO
#define API_IN_FILENO STDIN_FILENO
#define API_OUT_FILENO STDOUT_FILENO

// 1 Byte of magic can hold 255 states
#define MagicType unsigned char
// 2 Bytes, encodes message length up to 65535 bytes = 64 KB
#define MessageLengthType unsigned short

#define MAX_VAL(TYPE) (TYPE) ~0

    const char MAGIC_TYPE_SIZE = sizeof(MagicType);
    const char MESSAGE_LENGTH_TYPE_SIZE = sizeof(MessageLengthType);
    const char PREFIX_SIZE = MAGIC_TYPE_SIZE + MESSAGE_LENGTH_TYPE_SIZE;
    const MessageLengthType MAX_MESSAGE_LENGTH = MAX_VAL(MessageLengthType);
    const int MAX_FULL_MESSAGE_SIZE = MAX_MESSAGE_LENGTH + PREFIX_SIZE;
    const int MAX_PRE_MESSAGE_LENGTH = MAX_MESSAGE_LENGTH * 4;

    enum Magic
    {
        DISCONNECT = MAX_VAL(MagicType),
        CONNECT = DISCONNECT - 1,

        REQUEST_CONNECT = CONNECT - 1,
        ACCEPT_CONNECT = REQUEST_CONNECT - 1,
        CREATE_CONNECT = ACCEPT_CONNECT - 1,

        LOG_INFO = CREATE_CONNECT - 1,
        LOG_ERROR = LOG_INFO - 1,

        MAX_CONNECTIONS = LOG_ERROR - 1
    };

    void buffer_read_all(int fd, char *buf, int len)
    {
        int m = read(fd, buf, len);
        int d = len - m;
        while (d > 0)
        {
            buf += m;
            m = read(fd, buf, d);
            d -= m;
        }
    }

    void buffer_send_socket_all(TCPSocket<> *socket, char *buf, int len)
    {
        int m = socket->Send(buf, len);
        int d = len - m;
        while (d > 0)
        {
            buf += m;
            m = socket->Send(buf, d);
            d -= m;
        }
    }

    int buffer_write_all_len(int fd, char *buf, int len)
    {
        int m = write(fd, buf, len);
        int d = len - m;
        while (d > 0)
        {
            buf += m;
            m = write(fd, buf, d);
            d -= m;
        }
        free(buf);
        return len;
    }
    int buffer_write_all(int fd, char *buf) { return buffer_write_all_len(fd, buf, strlen(buf)); }

    // Make buffer methods
    char *make_buffer_special(MagicType mag, MagicType mag_as_message_length)
    {
        char *prefix_buffer = (char *)malloc(PREFIX_SIZE);
        memcpy(prefix_buffer, &mag, MAGIC_TYPE_SIZE);
        (*prefix_buffer) += MAGIC_TYPE_SIZE;
        memcpy(prefix_buffer, &mag_as_message_length, MESSAGE_LENGTH_TYPE_SIZE);
        return prefix_buffer;
    }

    char *make_buffer(MagicType mag, const char *message_buffer, MessageLengthType message_length)
    {
        if (message_length > MAX_MESSAGE_LENGTH)
        {
            std::string text = "[make_buffer] message length is bigger than ";
            text += std::to_string(MAX_MESSAGE_LENGTH);
            perror(text.c_str());
            exit(1);
        }
        char *full_message_buffer = (char *)malloc(PREFIX_SIZE + message_length);
        memcpy(full_message_buffer, &mag, MAGIC_TYPE_SIZE);
        (*full_message_buffer) += MAGIC_TYPE_SIZE;
        memcpy(full_message_buffer, &message_length, MESSAGE_LENGTH_TYPE_SIZE);
        (*full_message_buffer) += MESSAGE_LENGTH_TYPE_SIZE;
        memcpy(full_message_buffer, message_buffer, message_length);
        return full_message_buffer;
    }
    // Api out calls
    int api_req_connect(MagicType cn)
    { //
        return buffer_write_all(
            API_OUT_FILENO,
            make_buffer_special(Magic::REQUEST_CONNECT, cn));
    }

    // Log calls
    template <typename... T>
    inline int log(MagicType log, fmt::format_string<T...> fmt, T &&...args)
    {
        char buf[MAX_MESSAGE_LENGTH];
        int n = fmt::format_to_n(buf, MAX_MESSAGE_LENGTH, fmt, std::forward<T>(args)...).size;
        assert(n <= MAX_MESSAGE_LENGTH);
        return buffer_write_all_len(API_OUT_FILENO, make_buffer(log, buf, n), n);
    }
    template <typename... T>
    inline int log_info(fmt::format_string<T...> fmt, T &&...args) { return log(LOG_INFO, fmt, std::forward<T>(args)...); }
    template <typename... T>
    inline int log_error(fmt::format_string<T...> fmt, T &&...args) { return log(LOG_ERROR, fmt, std::forward<T>(args)...); }

    // Api calls
    inline int api(MagicType connId, const char *message, MessageLengthType length) { return buffer_write_all_len(API_OUT_FILENO, make_buffer(connId, message, length), length + PREFIX_SIZE); }
    inline int api_message(MagicType connId, const char *message, MessageLengthType length) { return api(connId, message, length); }

    inline int api_special(MagicType mag, MagicType mag_as_message_length) { return buffer_write_all_len(API_OUT_FILENO, make_buffer_special(mag, mag_as_message_length), PREFIX_SIZE); }
    inline int api_create_connect(MagicType connId) { return api_special(Magic::CREATE_CONNECT, connId); }

}
