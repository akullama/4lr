// udp_server.cpp
#include <iostream>
#include <fstream>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>

using boost::asio::ip::udp;

class UDPServer {
public:
    UDPServer(boost::asio::io_service& io_service, unsigned short port)
        : socket_(io_service, udp::endpoint(udp::v4(), port)),
          file_name_(),
          receiving_file_(false) {
        start_receive();
    }

private:
    void start_receive() {
        buffer_.fill(0);
        socket_.async_receive_from(
            boost::asio::buffer(buffer_),
            remote_endpoint_,
            boost::bind(&UDPServer::handle_receive, this,
                        boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred));
    }

    void handle_receive(const boost::system::error_code& error, std::size_t bytes_transferred) {
        if (!error && bytes_transferred > 0) {
            if (!receiving_file_) {
                // First packet should be the file name
                file_name_ = std::string(buffer_.data(), bytes_transferred);
                std::cout << "Receiving file: " << file_name_ << "\n";
                outfile_.open(file_name_, std::ios::binary);
                if (!outfile_.is_open()) {
                    std::cerr << "Error: Unable to open file " << file_name_ << " for writing.\n";
                    return;
                }
                receiving_file_ = true;
            } else {
                if (bytes_transferred == 0) {
                    // Empty packet indicates end of file
                    outfile_.close();
                    std::cout << "File received and saved as: " << file_name_ << "\n";
                    send_thanks_file();
                    receiving_file_ = false;
                } else {
                    // Write the received data to the file
                    outfile_.write(buffer_.data(), bytes_transferred);
                }
            }
        } else {
            std::cerr << "Error receiving data: " << error.message() << "\n";
        }

        // Continue receiving
        start_receive();
    }

    void send_thanks_file() {
        std::ifstream infile("thanks.txt", std::ios::binary);
        if (!infile.is_open()) {
            std::cerr << "Error: Unable to open thanks.txt\n";
            return;
        }

        std::string content((std::istreambuf_iterator<char>(infile)), std::istreambuf_iterator<char>());
        infile.close();

        auto response = std::make_shared<std::string>(content);
        socket_.async_send_to(
            boost::asio::buffer(*response),
            remote_endpoint_,
            [this, response](const boost::system::error_code& error, std::size_t /*bytes_transferred*/) {
                if (!error) {
                    std::cout << "Response 'thanks.txt' sent to client.\n";
                } else {
                    std::cerr << "Error sending response: " << error.message() << "\n";
                }
            });
    }

    udp::socket socket_;
    udp::endpoint remote_endpoint_;
    std::array<char, 4096> buffer_;
    std::string file_name_;
    std::ofstream outfile_;
    bool receiving_file_;
};

int main() {
    try {
        boost::asio::io_service io_service;
        UDPServer server(io_service, 12345);
        std::cout << "UDP Server is running on port 12345.\n";
        io_service.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}
