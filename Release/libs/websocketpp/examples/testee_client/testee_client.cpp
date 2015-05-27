#include <websocketpp/config/asio_no_tls_client.hpp>

#include <websocketpp/client.hpp>

#include <iostream>

typedef websocketpp::client<websocketpp::config::asio_client> client;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

// pull out the type of messages sent by our config
typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

int case_count = 0;

void on_message(client* c, websocketpp::connection_hdl hdl, message_ptr msg) {
    client::connection_ptr con = c->get_con_from_hdl(hdl);

    if (con->get_resource() == "/getCaseCount") {
        std::cout << "Detected " << msg->get_payload() << " test cases."
                  << std::endl;
        case_count = atoi(msg->get_payload().c_str());
    } else {
        c->send(hdl, msg->get_payload(), msg->get_opcode());
    }
}

int main(int argc, char* argv[]) {
    // Create a server endpoint
    client c;

    std::string uri = "ws://localhost:9001";

    if (argc == 2) {
        uri = argv[1];
    }

    try {
        // We expect there to be a lot of errors, so suppress them
        c.clear_access_channels(websocketpp::log::alevel::all);
        c.clear_error_channels(websocketpp::log::elevel::all);

        // Initialize ASIO
        c.init_asio();

        // Register our handlers
        c.set_message_handler(bind(&on_message,&c,::_1,::_2));

        websocketpp::lib::error_code ec;
        client::connection_ptr con = c.get_connection(uri+"/getCaseCount", ec);
        c.connect(con);

        // Start the ASIO io_service run loop
        c.run();

        std::cout << "case count: " << case_count << std::endl;

        for (int i = 1; i <= case_count; i++) {
            c.reset();

            std::stringstream url;

            url << uri << "/runCase?case=" << i << "&agent="
                << websocketpp::user_agent;

            con = c.get_connection(url.str(), ec);

            c.connect(con);

            c.run();
        }

        std::cout << "done" << std::endl;

    } catch (websocketpp::exception const & e) {
        std::cout << e.what() << std::endl;
    }
}
