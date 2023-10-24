#include "tcpsocket.hpp"
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <assert.h>
#include <format>
#include <utility>
#include <magic_enum_all.hpp>
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

    enum Magic : MagicType
    {
        DISCONNECT = MAX_VAL(MagicType),
        CONNECT = DISCONNECT - 1,

        REQUEST_CONNECT = CONNECT - 1,
        ACCEPT_CONNECT = REQUEST_CONNECT - 1,

        LOG_INFO = ACCEPT_CONNECT - 1,
        LOG_ERROR = LOG_INFO - 1,

        MAX_CONNECTIONS = LOG_ERROR - 1

    };

    typedef struct
    {
        char *buf;
        int len;
    } buffer;

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

    int buffer_write(int fd, buffer *buffer)
    {
        int m = write(fd, buffer->buf, buffer->len);
        int d = buffer->len - m;
        while (d > 0)
        {
            buffer->buf += m;
            m = write(fd, buffer->buf, d);
            d -= m;
        }
        free(buffer->buf);
        free(buffer);
        return buffer->len;
    }

    // Make buffer methods
    buffer *make_buffer_special(MagicType mag, MagicType mag_as_message_length)
    {
        buffer *prefix_buffer = (buffer *)malloc(sizeof(buffer));
        prefix_buffer->len = PREFIX_SIZE;
        prefix_buffer->buf = (char *)malloc(prefix_buffer->len);
        memcpy(prefix_buffer->buf, &mag, MAGIC_TYPE_SIZE);
        memcpy(prefix_buffer->buf + MAGIC_TYPE_SIZE, &mag_as_message_length, MESSAGE_LENGTH_TYPE_SIZE);
        return prefix_buffer;
    }

    buffer *make_buffer(MagicType mag, const char *message_buffer, MessageLengthType message_length)
    {
        if (message_length > MAX_MESSAGE_LENGTH)
        {
            std::string text = "[make_buffer] message length is bigger than ";
            text += std::to_string(MAX_MESSAGE_LENGTH);
            perror(text.c_str());
            exit(1);
        }
        buffer *full_message_buffer = (buffer *)malloc(sizeof(buffer));
        full_message_buffer->len = PREFIX_SIZE + message_length;
        full_message_buffer->buf = (char *)malloc(full_message_buffer->len);
        memcpy(full_message_buffer->buf, &mag, MAGIC_TYPE_SIZE);
        memcpy(full_message_buffer->buf + MAGIC_TYPE_SIZE, &message_length, MESSAGE_LENGTH_TYPE_SIZE);
        memcpy(full_message_buffer->buf + MAGIC_TYPE_SIZE + MESSAGE_LENGTH_TYPE_SIZE, message_buffer, message_length);
        return full_message_buffer;
    }

    // Log calls
    template <typename... T>
    const int log(MagicType log, std::format_string<T...> fmt, T &&...args)
    {
        std::string str;
        auto it = std::back_inserter(str);
        std::format_to_n_result r = std::format_to_n(it, MAX_MESSAGE_LENGTH, fmt, std::forward<T>(args)...);
        int n = r.size;
        assert(n <= MAX_MESSAGE_LENGTH);
        return buffer_write(LOG_FILENO, make_buffer(log, str.c_str(), n));
    }
    template <typename... T>
    auto log_info(std::format_string<T...> fmt, T &&...args) { return log(LOG_INFO, fmt, std::forward<T>(args)...); }
    template <typename... T>
    auto log_error(std::format_string<T...> fmt, T &&...args) { return log(LOG_ERROR, fmt, std::forward<T>(args)...); }

    // Make buffers for api
    buffer *api_make_buffer_message(MagicType connId, const char *message, MessageLengthType length) { return make_buffer(connId, message, length); }
    buffer *api_make_buffer_connect(MagicType connId) { return make_buffer_special(Magic::CONNECT, connId); }
    buffer *api_make_buffer_disconnect(MagicType connId) { return make_buffer_special(Magic::DISCONNECT, connId); }
    buffer *api_make_buffer_request_connect(MagicType connId) { return make_buffer_special(Magic::REQUEST_CONNECT, connId); }

    inline int api_buffer_write(buffer *buffer) { return buffer_write(API_OUT_FILENO, buffer); }
}

template <>
struct std::formatter<Api::Magic> : formatter<std::string_view>
{
    auto format(Api::Magic magic, std::format_context &ctx) const;
};

auto std::formatter<Api::Magic>::format(Api::Magic magic, std::format_context &ctx) const
{
    std::string_view name = magic_enum::enum_name(magic);
    return std::formatter<std::string_view>::format(name, ctx);
}
