# 🚀 Clumzee: The Agile HTTP Server

Clumzee is an elegantly designed, lightweight HTTP server crafted in the power of C++. This nimble server efficiently serves static files from any specified directory, with support for GET and POST requests, along with essential error handling mechanisms. The server is flexible, allowing customization through an array of configuration options.


## 🎁 Features

- 📂 Efficiently serve static files over HTTP
- 🔄 Complete support for GET and POST requests
- 🛡️ Fundamental error handling for invalid requests
- 🔧 Configurable server options for tailor-made solutions
- 🚦 Handle concurrent connections with a manageable limit
- 📝 Logging of requests and responses for traceability and auditing
- 🐞 Enable debug mode for real-time observation of file system changes

## 🚀 Getting Started

Ignite your journey with Clumzee by following these steps. Ensure you have the right environment setup before you commence.

### Prerequisites

- 🔧 A C++ compiler that has arms wide open for the C++17 standard.
- 🖥️ A Linux or macOS operating system. We must reluctantly inform that Windows does not receive official support at the moment.

### Step 1: Clone the Repository

Start by cloning the Clumzee repository from GitHub. Open your terminal and run the following commands:
```bash
git clone https://github.com/letrad/clumzee.git
cd clumzee
```
### Step 2: Compile Clumzee
Compile the Clumzee server using a C++ compiler:
```bash
g++ -std=c++17 -o clumzee main.cpp
```

### Step 3: Run the Server
Now, all set to launch Clumzee! Use the following command to start the server:
```bash
./clumzee
```
By default, the server listens on port 3333 and serves files from the current directory.

Access the server from a web browser:
[localhost:3333](http://localhost:3333)

## 🔧 Configuration
For now, the server can be configured by modifying the ServerConfig struct in the main function of the main.cpp file. You can change the following settings:
- `rootDirectory`: The root directory from which the server serves files.
- `defaultFile`: The default file to serve when a directory is requested.
- `virtualHosts`: A list of virtual hosts the server responds to.
- `enableLoadBalancing`: Enable load balancing between multiple server instances.
- `enableLogging`: Enable logging of requests and responses.
- `enableDebug`: Enable debug mode for real-time file update tracking.
- `port`: The port number on which the server listens for connections.

## 📝 Contributions
Contributions to the project are welcome. If you find any issues or have suggestions for improvements, please open an issue or submit a pull request.