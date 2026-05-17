#include "codec.h"

#include <boost/asio/buffer.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/system/error_code.hpp>
#include <cassert>
#include <iostream>
#include <string>

using namespace im;

int main() {
    Codec codec;
    boost::asio::streambuf encoded;
    const std::string body(700 * 1024, 'x');
    codec.encode(MsgType::FILE_DOWNLOAD_CHUNK, body, encoded);

    std::string wire(6 + body.size(), '\0');
    boost::asio::buffer_copy(boost::asio::buffer(wire), encoded.data(), wire.size());

    boost::asio::streambuf incoming;
    std::ostream out(&incoming);
    out.write(wire.data(), 3);
    out.write(wire.data() + 3, 11);
    out.write(wire.data() + 14, static_cast<std::streamsize>(wire.size() - 14));

    boost::system::error_code ec;
    MessagePtr decoded = codec.decode(incoming, ec);
    assert(decoded);
    assert(!ec);
    assert(decoded->type == MsgType::FILE_DOWNLOAD_CHUNK);
    assert(decoded->body == body);
    assert(incoming.size() == 0);

    std::cout << "codec self-test passed\n";
    return 0;
}
