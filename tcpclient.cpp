#include <iostream>
#include <fstream>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>

using boost::asio::ip::tcp;

class AsyncClient {
public:
    AsyncClient(boost::asio::io_service& io_service, const std::string& host, const std::string& port)
        : socket_(io_service),
          resolver_(io_service) {
        auto endpoints = resolver_.resolve(host, port);
        boost::asio::async_connect(socket_, endpoints,
            boost::bind(&AsyncClient::connect, this, boost::asio::placeholders::error));
    }

private:
    void connect(const boost::system::error_code& error) {
        if (!error) {
            std::cout << "Connected to server. Enter the file name to send (e.g., test.jpg): ";
            std::getline(std::cin, file_name);
            send_file_name();
        } else {
            std::cerr << "Connection failed: " << error.message() << "\n";
        }
    }

    void send_file_name() {
        auto buffer = std::make_shared<std::string>(file_name);
        boost::asio::async_write(socket_, boost::asio::buffer(*buffer),
            boost::bind(&AsyncClient::send_file_content, this, boost::asio::placeholders::error));
    }

    void send_file_content(const boost::system::error_code& error) {
        if (!error) {
            std::ifstream infile(file_name, std::ios::binary);
            if (!infile.is_open()) {
                std::cerr << "Error: Unable to open file " << file_name << "\n";
                return;
            }

            auto buffer = std::make_shared<std::array<char, 4096>>();
            while (!infile.eof()) {
                infile.read(buffer->data(), buffer->size());
                std::streamsize bytes_read = infile.gcount();
                boost::asio::write(socket_, boost::asio::buffer(buffer->data(), bytes_read));
            }
            infile.close();

            receive_file();
        } else {
            std::cerr << "Error sending file name: " << error.message() << "\n";
        }
    }

    void receive_file() {
        auto buffer = std::make_shared<std::array<char, 4096>>();
        socket_.async_read_some(boost::asio::buffer(*buffer),
            [this, buffer](const boost::system::error_code& error, std::size_t bytes_transferred) {
                if (!error) {
                    std::ofstream outfile("received_thanks.txt", std::ios::binary);
                    outfile.write(buffer->data(), bytes_transferred);
                    outfile.close();
                    std::cout << "Received file 'received_thanks.txt' from server.\n";
                } else if (error == boost::asio::error::eof) {
                    std::cout << "File transfer complete.\n";
                } else {
                    std::cerr << "Error receiving file: " << error.message() << "\n";
                }
            });
    }

    tcp::socket socket_;
    tcp::resolver resolver_;
    std::string file_name;
};

int main() {
    try {
        boost::asio::io_service io_service;
        AsyncClient client(io_service, "127.0.0.1", "12345");
        io_service.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}
