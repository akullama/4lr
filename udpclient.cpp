// udp_client.cpp
#include <iostream>
#include <fstream>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>

using boost::asio::ip::udp;

class AsyncUDPClient {
public:
    AsyncUDPClient(boost::asio::io_service& io_service, const std::string& host, const std::string& port)
        : socket_(io_service),
          resolver_(io_service),
          server_endpoint_() {
        // Open the socket
        socket_.open(udp::v4());

        // Resolve the server address
        udp::resolver resolver(io_service);
        udp::resolver::query query(udp::v4(), host, port);
        server_endpoint_ = *resolver.resolve(query);

        // Start the process by sending the file name
        std::cout << "Enter the file name to send (e.g., test.jpg): ";
        std::getline(std::cin, file_name_);
        send_file_name();
    }

private:
    void send_file_name() {
        auto buffer = std::make_shared<std::string>(file_name_);
        socket_.async_send_to(
            boost::asio::buffer(*buffer),
            server_endpoint_,
            [this, buffer](const boost::system::error_code& error, std::size_t bytes_transferred) {
                if (!error) {
                    std::cout << "File name sent. Starting file transfer...\n";
                    send_file_content();
                } else {
                    std::cerr << "Error sending file name: " << error.message() << "\n";
                }
            });
    }

    void send_file_content() {
        infile_.open(file_name_, std::ios::binary);
        if (!infile_.is_open()) {
            std::cerr << "Error: Unable to open file " << file_name_ << "\n";
            return;
        }
        send_next_chunk();
    }

    void send_next_chunk() {
        if (infile_.eof()) {
            // Send an empty packet to indicate end of file
            auto buffer = std::make_shared<std::string>("");
            socket_.async_send_to(
                boost::asio::buffer(*buffer),
                server_endpoint_,
                [this, buffer](const boost::system::error_code& error, std::size_t /*bytes_transferred*/) {
                    if (!error) {
                        std::cout << "File transfer complete. Waiting for response...\n";
                        receive_response();
                    } else {
                        std::cerr << "Error sending end of file: " << error.message() << "\n";
                    }
                });
            return;
        }

        auto buffer = std::make_shared<std::array<char, 4096>>();
        infile_.read(buffer->data(), buffer->size());
        std::streamsize bytes_read = infile_.gcount();
        if (bytes_read > 0) {
            socket_.async_send_to(
                boost::asio::buffer(buffer->data(), bytes_read),
                server_endpoint_,
                [this, buffer](const boost::system::error_code& error, std::size_t /*bytes_transferred*/) {
                    if (!error) {
                        // Continue sending the next chunk
                        send_next_chunk();
                    } else {
                        std::cerr << "Error sending file content: " << error.message() << "\n";
                    }
                });
        }
    }

    void receive_response() {
        auto buffer = std::make_shared<std::array<char, 4096>>();
        socket_.async_receive_from(
            boost::asio::buffer(*buffer),
            sender_endpoint_,
            [this, buffer](const boost::system::error_code& error, std::size_t bytes_transferred) {
                if (!error && bytes_transferred > 0) {
                    std::ofstream outfile("received_thanks.txt", std::ios::binary);
                    outfile.write(buffer->data(), bytes_transferred);
                    outfile.close();
                    std::cout << "Received response file 'received_thanks.txt' from server.\n";
                } else if (error != boost::asio::error::eof) {
                    std::cerr << "Error receiving response: " << error.message() << "\n";
                }
            });
    }

    udp::socket socket_;
    udp::resolver resolver_;
    udp::endpoint server_endpoint_;
    udp::endpoint sender_endpoint_;
    std::string file_name_;
    std::ifstream infile_;
};

int main() {
    try {
        boost::asio::io_service io_service;
        AsyncUDPClient client(io_service, "127.0.0.1", "12345");
        io_service.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}
