#include <iostream>
#include <fstream>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>

using boost::asio::ip::tcp;

class TCPServer {
public:
    TCPServer(boost::asio::io_service& io_service, unsigned short port)
        : acceptor_(io_service, tcp::endpoint(tcp::v4(), port)),
          socket_(io_service) {
        start_accept();
    }

private:
    void start_accept() {
        acceptor_.async_accept(socket_,
            boost::bind(&TCPServer::handle_accept, this, boost::asio::placeholders::error));
    }

    void handle_accept(const boost::system::error_code& error) {
        if (!error) {
            std::cout << "Client connected.\n";
            start_read_file_name();
        }
        start_accept();
    }

    void start_read_file_name() {
        auto buffer = std::make_shared<std::array<char, 256>>(); // Буфер для имени файла
        socket_.async_read_some(boost::asio::buffer(*buffer),
            [this, buffer](const boost::system::error_code& error, std::size_t bytes_transferred) {
                if (!error) {
                    file_name_ = std::string(buffer->data(), bytes_transferred);
                    std::cout << "Receiving file: " << file_name_ << "\n";
                    start_read_file_content();
                } else {
                    std::cerr << "Error reading file name: " << error.message() << "\n";
                }
            });
    }

    void start_read_file_content() {
        auto buffer = std::make_shared<std::array<char, 4096>>();
        boost::asio::async_read(socket_, boost::asio::buffer(*buffer),
            [this, buffer](const boost::system::error_code& error, std::size_t bytes_transferred) {
                
                if (!error || error == boost::asio::error::eof) {
                    if (bytes_transferred > 0) {
                        std::ofstream outfile(file_name_, std::ios::binary | std::ios::app);
                        outfile.write(buffer->data(), bytes_transferred);
                        outfile.close();
                    }

                    if (error == boost::asio::error::eof) {
                        std::cout << "File received and saved as: " << file_name_ << "\n";
                        send_thanks_file();
                    } else {
                        start_read_file_content();
                    }
                } else {
                    std::cerr << "Error reading file content: " << error.message() << "\n";
                }
            });
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
        boost::asio::async_write(socket_, boost::asio::buffer(*response),
            [this](const boost::system::error_code& error, std::size_t) {
                if (!error) {
                    std::cout << "thanks.txt sent to client.\n";
                } else {
                    std::cerr << "Error sending thanks.txt: " << error.message() << "\n";
                }
            });
    }

    tcp::acceptor acceptor_;
    tcp::socket socket_;
    std::string file_name_;
};

int main() {
    try {
        boost::asio::io_service io_service;
        TCPServer server(io_service, 12345);
        io_service.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
}