#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <cstring>
#include <ctime>
#include <arpa/inet.h>
#include <csignal>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iomanip>
#include <sys/poll.h>
#include <filesystem>

//               -- CONSTANTS --                //
// This is meant to be changed at later stages  //
constexpr int DEFAULT_PORT = 8080;
constexpr int BUFFER_SIZE = 4096;
constexpr int MAX_CONCURRENT_CONNECTIONS = 10;
constexpr int MAX_REQUEST_SIZE = 1024 * 1024;   // 1MB
constexpr int MAX_RESPONSE_SIZE = 1024 * 1024;  // 1MB
constexpr int POLL_TIMEOUT = 1000;              // Timeout in ms for poll


//                -- GLOBALS --                 //
int serverSocket;
bool isRunning = true;
std::unordered_map<std::string, std::time_t> fileModificationTimes;
const std::unordered_map<int, std::string> statusCodes = {
        {200, "200 OK"},
        {400, "400 Bad Request"},
        {403, "403 Forbidden"},
        {404, "404 Not Found"},
        {500, "500 Internal Server Error"}
};
const std::string debugJsCode = R"(
<!-- APPENDED BY CLUMZEE -->
<!-- THIS IS NOT MEANT TO RUN IN PRODUCTION -->
<!-- DISABLE DEBUG MODE FOR PRODUCTION -->

<script>
setInterval(function() {
    fetch(window.location.href + "?checkUpdate=true")
    .then(function(response) {
        return response.text();
    })
    .then(function(data) {
        if (data === "update") {
            location.reload();
        }
    });
}, 500);
</script>
<!-- APPENDAGE END -->
)";

std::mutex connectionMutex; // Mutex for concurrent connections

struct ServerConfig {
    std::string rootDirectory;
    std::string defaultFile;
    std::vector<std::string> virtualHosts;
    bool enableLoadBalancing{};
    bool enableLogging{};
    bool enableDebug{};
    int port{};
};

struct HttpRequest {
    std::string method;
    std::string path;
    std::string httpVersion;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
};

struct HttpResponse {
    int statusCode;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
};


// Function to handle HTTP request
HttpResponse handleRequest(const HttpRequest &request, const ServerConfig &config) {
    HttpResponse response;

    // Validate request method
    if (request.method != "GET" && request.method != "POST") {
        response.statusCode = 400;
        response.body = "Invalid request method.";
        return response;
    }

    // If this is an update check
    if (config.enableDebug && request.path.find("?checkUpdate=true") != std::string::npos) {
        std::string filePath = request.path.substr(0, request.path.find("?checkUpdate=true"));
        filePath = config.rootDirectory + std::filesystem::path(filePath).lexically_normal().string();
        std::time_t newModificationTime = std::filesystem::last_write_time(filePath).time_since_epoch().count();
        if (fileModificationTimes[filePath] != newModificationTime) {
            fileModificationTimes[filePath] = newModificationTime;
            response.statusCode = 200;
            response.body = "update";
            // Log when an update is detected
            if (config.enableLogging) {
                std::time_t now = std::time(nullptr);
                std::cout << "[" << std::put_time(std::localtime(&now), "%Y-%m-%d %H:%M:%S") << "] ";
                std::cout << "Update detected for " << filePath << std::endl;
            }
        } else {
            response.statusCode = 200;
            response.body = "no update";
        }
        return response;
    }

    // Sanitize and validate request path
    std::string sanitizedPath = std::filesystem::path(request.path).lexically_normal().string();
    std::string filePath = config.rootDirectory + sanitizedPath;
    if (filePath.back() == '/')
        filePath += config.defaultFile;
    if (filePath.find("..") != std::string::npos ||
        filePath.substr(0, config.rootDirectory.size()) != config.rootDirectory) {
        response.statusCode = 403;
        response.body = "Forbidden.";
        return response;
    }

    // Read file contents
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        response.statusCode = 404;
        response.body = "File not found.";
        return response;
    }

    std::stringstream fileContents;
    fileContents << file.rdbuf();
    file.close();

    // Check if any error occurred while reading the file
    if (file.bad()) {
        response.statusCode = 500;
        response.body = "Internal Server Error.";
        return response;
    }

    // Set response body
    response.body = fileContents.str();

    // If debug is enabled, append the JavaScript code
    if (config.enableDebug && filePath.substr(filePath.size() - 5) == ".html") {
        response.body += debugJsCode;
        fileModificationTimes[filePath] = std::filesystem::last_write_time(filePath).time_since_epoch().count();
    }

    // Set response headers
    response.statusCode = 200;
    response.headers["Content-Length"] = std::to_string(response.body.size());
    response.headers["Content-Type"] = "text/html";

    return response;
}

// Function to parse HTTP request
HttpRequest parseRequest(const std::string &requestData) {
    HttpRequest request;

    // Split request into lines
    std::istringstream requestStream(requestData);
    std::string line;
    std::getline(requestStream, line);
    std::istringstream lineStream(line);
    lineStream >> request.method >> request.path >> request.httpVersion;

    // Check if the request line is valid
    if (request.method.empty() || request.path.empty() || request.httpVersion.empty()) {
        request.method = "";
        request.path = "";
        request.httpVersion = "";
        return request;
    }

    // Read headers
    while (std::getline(requestStream, line) && line != "\r") {
        size_t pos = line.find(":");
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            // Trim leading spaces from value
            value.erase(0, value.find_first_not_of(" "));
            request.headers[key] = value;
        }
    }

    // Read body
    std::stringstream requestBodyStream;
    requestBodyStream << requestStream.rdbuf();
    request.body = requestBodyStream.str();

    return request;
}

// Function to format HTTP response
std::string formatResponse(const HttpResponse &response) {
    std::stringstream responseStream;
    responseStream << "HTTP/1.1 " << statusCodes.at(response.statusCode) << "\r\n";
    for (const auto &header: response.headers) {
        responseStream << header.first << ": " << header.second << "\r\n";
    }
    responseStream << "\r\n" << response.body;

    return responseStream.str();
}

// Function to handle client connection
void handleConnection(int clientSocket, const ServerConfig &config, sockaddr_in clientAddress) {
    // Read request data
    char *buffer = new char[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    std::string requestData;
    while (requestData.find("\r\n\r\n") == std::string::npos && requestData.size() < MAX_REQUEST_SIZE) {
        int bytesRead = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
        if (bytesRead <= 0) {
            break;
        }
        requestData += buffer;
        memset(buffer, 0, BUFFER_SIZE);
    }

    // Parse and handle request
    HttpRequest request = parseRequest(requestData);

    // Log request, but not for checkUpdate requests
    if (config.enableLogging && request.path.find("?checkUpdate=true") == std::string::npos) {
        std::time_t now = std::time(nullptr);
        std::string clientIP = inet_ntoa(clientAddress.sin_addr);
        std::cout << "[" << std::put_time(std::localtime(&now), "%Y-%m-%d %H:%M:%S") << "] ";
        std::cout << "Received request from " << clientIP << ": " << request.method << " " << request.path << std::endl;
    }

    HttpResponse response = handleRequest(request, config);

    // Log response, but not for checkUpdate requests
    if (config.enableLogging && request.path.find("?checkUpdate=true") == std::string::npos) {
        std::time_t now = std::time(nullptr);
        std::cout << "[" << std::put_time(std::localtime(&now), "%Y-%m-%d %H:%M:%S") << "] ";
        std::cout << "Response to " << inet_ntoa(clientAddress.sin_addr) << ": " << statusCodes.at(response.statusCode)
                  << std::endl;
    }

    // Format and send response
    std::string responseData = formatResponse(response);
    send(clientSocket, responseData.c_str(), responseData.size(), 0);

    // Close connection
    close(clientSocket);

    // Release resources
    delete[] buffer;

    // Release mutex
    connectionMutex.unlock();
}


void signalHandler(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received.\n";

    // cleanup and close up stuff here
    isRunning = false;
    close(serverSocket); // Close the server socket

    // Release mutex if locked
    if (connectionMutex.try_lock())
        connectionMutex.unlock();
}

// Function to start the server
void startServer(const ServerConfig &config) {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    std::signal(SIGHUP, signalHandler);
    // Create server socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        std::cerr << "Failed to create server socket." << std::endl;
        return;
    }

    // Set SO_REUSEADDR socket option
    int reuseAddr = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &reuseAddr, sizeof(reuseAddr)) == -1) {
        std::cerr << "Failed to set SO_REUSEADDR socket option." << std::endl;
        close(serverSocket);
        return;
    }

    // Bind server socket
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(config.port);
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    if (bind(serverSocket, reinterpret_cast<sockaddr *>(&serverAddress), sizeof(serverAddress)) == -1) {
        std::cerr << "Failed to bind server socket." << std::endl;
        close(serverSocket);
        return;
    }

    if (listen(serverSocket, MAX_CONCURRENT_CONNECTIONS) == -1) {
        std::cerr << "Failed to listen for connections." << std::endl;
        close(serverSocket);
        return;
    }

    std::cout << "Server live at http://" << config.virtualHosts[0] << ":" << config.port << std::endl;

    isRunning = true;

    // Create pollfd structure for server socket
    struct pollfd serverPollfd{};
    serverPollfd.fd = serverSocket;
    serverPollfd.events = POLLIN;

    // Accept and handle connections
    while (isRunning) {
        // Poll for incoming connections or shutdown signal
        int pollResult = poll(&serverPollfd, 1, POLL_TIMEOUT);
        if (pollResult == -1) {
            if (errno == EINTR) {
                // Interrupted by a signal, continue the loop
                continue;
            } else {
                std::cerr << "Failed to poll for connections." << std::endl;
                break;
            }
        } else if (pollResult == 0) {
            // Timeout, no new connections or signals
            continue;
        }

        if (serverPollfd.revents & POLLIN) {
            // Accept new connection
            sockaddr_in clientAddress{};
            socklen_t clientAddressSize = sizeof(clientAddress);
            int clientSocket = accept(serverSocket, reinterpret_cast<sockaddr *>(&clientAddress), &clientAddressSize);
            if (clientSocket == -1) {
                std::cerr << "Failed to accept connection." << std::endl;
                continue;
            }

            // Limit concurrent connections
            connectionMutex.lock();

            // Handle connection in a separate thread
            std::thread connectionThread(handleConnection, clientSocket, config, clientAddress);
            connectionThread.detach();
        }
    }

    // Close server socket
    close(serverSocket);
}

int main() {
    ServerConfig config;
    config.rootDirectory = ".";
    config.defaultFile = "index.html";
    config.virtualHosts = {"localhost"};
    config.enableLoadBalancing = true;
    config.enableLogging = true;
    // Debug allows you to view realtime fs changes, this should not be used in a production environment as it can be obviously slow.
    config.enableDebug = true;
    config.port = 3333;

    startServer(config);

    return 0;
}