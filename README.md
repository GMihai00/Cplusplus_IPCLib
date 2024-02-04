# Cplusplus_IPCLib

This project aims to provide an easier to use C++ IPC library. It's build to be easy to debug, fast and versatile. The idea of creating this project was due to not finding any easy to use libraries for sending messages over the network while writing my license degree. It first started as a quick local IPC that can be found in IPC.vcxproj under net/legacy_fast_ipc. Despite being unmaintained code(Won't invest time into fixing bugs found), if you still want to use it, I will have a section describing it at the end. The bread and butter of this project is a web-based IPC system. During the development of this project I managed to cover almost all HTTP1.0 messages, providing a client for http and https as well as a server using http (Note: Planning to add a https one as well). While developing the library, I was following what Postman does and tried to include all the features that I found there, apart from that, I added "Transfer-Encoding: chucked" compatibility as I remarked that Postman doesn't support it(While debugging the library with Postman I saw that I get random data inside my body when sending messages with that flag on.) Right now it supports sync and asynchronous sending/handling messages, gzip compressing, formats bodies to json and follows redirects (can't follow from http to https and backwards due to design flow a.t.m.). By using the library you are able to code servers in no time, in few lines of code, almost like using a high level language like python.

# Table of Contents

- [Installation](#installation)
  - [MSVC-VCPKG](#msvc-vcpkg)
  - [CMAKE](#cmake)
- [Usage](#usage)
  - [Web-based](#web-based)
    - [Creating a server](#creating-a-server)
    - [Creating a client](#creating-a-client)
  - [Legacy](#legacy)
    - [Message format](#message-format)
    - [Server](#server)
    - [Client](#client)
  - [Docker Test Image](#docker-test-image)
- [Contributing](#contributing)
- [License](#license)

# Installation

## MSVC-VCPKG
Note: Windows Only
Requirements: vcpkg, MSVC Compiler(CMake GCC support will be added soon), Visual Studio is suggested to be used

1. Clone the repository: `git clone [https://github.com/GMihai00/Cplusplus_IPCLib.git](https://github.com/GMihai00/Cplusplus_IPCLib.git)`
2. Navigate to the project directory: `cd yourproject`
3. Update submodules: git submodule update --init --recursive
4. Install openssl and boost using vcpkg if not already on system
5. Change to windows directory
6. Open IPC.sln and build project IPC, this will generate a static library
7. Libraries to be added as Additional Dependencies: IPC.lib;libz-static.lib;libcrypto.lib;libssl.lib; (Make sure library directory has been added to Library directories)
8. Include headers depending on your needs (web_server.hpp/secure_web_client.hpp/web_client.hpp for web-based newer IPC client.hpp and server.hpp for older local IPC)


## CMAKE

Requirements: openssl installed on system, cmake, make, gcc, g++

1. Clone the repository: `git clone [https://github.com/GMihai00/Cplusplus_IPCLib.git](https://github.com/GMihai00/Cplusplus_IPCLib.git)`
2. Navigate to the project directory: `cd yourproject`
3. Update submodules: git submodule update --init --recursive
4. Install openssl if not already on system
5. Change to cross-platform directory
6. run cmake + run make to build IPC lib
7. Libraries to be added as Additional Dependencies: IPC and all other dependencies that can be found inside CMake depending on platform 
8. Include headers depending on your needs (web_server.hpp/secure_web_client.hpp/web_client.hpp for web-based newer IPC client.hpp and server.hpp for older local IPC)

# Usage

## Web-based

### Creating a server 

```cpp
#include <iostream>

#include "net/web_server.hpp"

constexpr auto HOST = "127.0.0.1";
constexpr auto PORT = 54321;

int main() try
{
	net::web_server server(HOST, PORT);

	if (auto ret = server.start(); !ret)
	{
		std::cerr << ret.message();
		return 1;
	}

    // infinite loop to prevent main thread exit
    while (true)
	{
	  std::this_thread::sleep_for(std::chrono::seconds(1));
	}

    return 0;
}
catch (const std::exception& err)
{
	std::cerr << err.what();
	return 1;
}
```

HTTPS Server
```cpp
#include <iostream>

#include "net/secure_web_server.hpp"

constexpr auto HOST = "127.0.0.1";
constexpr auto PORT = 54321;

int main() try
{
	net::secure_web_server server(HOST, "server.pem", "dh4096.pem", PORT);

	if (auto ret = server.start(); !ret)
	{
		std::cerr << ret.message();
		return 1;
	}

    // infinite loop to prevent main thread exit
    while (true)
	{
	  std::this_thread::sleep_for(std::chrono::seconds(1));
	}

    return 0;
}
catch (const std::exception& err)
{
	std::cerr << err.what();
	return 1;
}
```

**Adding mappings**

**Note**: Mappings can be added even at runtime, there is no need to add them before starting the server, but it is recommended to do so before starting. 

**Fixed address mapping**
```cpp
...

net::async_req_handle_callback test_callback = []       (std::shared_ptr<net::http_request> req) {

	bool ok = false;

	req->get_json_body(); 
	// req->get_body_raw() for raw data

	// handle request
	if (ok)
	{
		std::vector<uint8_t> body_data; 
		
		/* add data to body if needed
		Ex: std::string data = "smth_to_send_back" 
		body_data = std::vector<uint8_t>(data.begin(), 
			data.end());*/

		return net::http_response(200, "OK", 
			nullptr, body_data);
	}
	else
	{
		return net::http_response(400, "Bad request");
	}
};

server.add_mapping(net::request_type::GET, 
	"/test", test_callback);

...
```

**Volatile address mapping**
```cpp
...

net::async_req_regex_handle_callback test_regex_callback =  [](std::shared_ptr<net::http_request>, const std::smatch& matches) {

	if (matches.size() < 3)
	{
		return net::http_response(400, "Bad request");
	}

	if (!matches[2].matched)
	{
		return net::http_response(400, "Bad request");
	}

	int id = 0;

	try
	{
		id = std::stoi(matches[2]);
	}
	catch (...)
	{
		return net::http_response(400, "Bad request");
	}

	nlohmann::json smth_to_send = 
		nlohmann::json({ {"id", id}});

	std::string data = smth_to_send.dump();

	auto body_data = std::vector<uint8_t>(data.begin(), data.end());

	return net::http_response(200, "OK", 
		nullptr, body_data);
};

std::regex test_pattern(R"(^(/test/id=(\d+))$)");

server.add_regex_mapping(net::request_type::GET, test_pattern, test_regex_callback);

...
```

**Adding constraints and events on connection**
```cpp
#include "net/web_server.hpp"

class test_web_server : public web_server
{
protected:
  bool can_client_connect(const std::shared_ptr<boost::asio::ip::tcp::socket> client) noexcept override
  {
    bool can_connect = true;
    
    // check constraints
    
    return can_connect;
  }
  void on_client_connect(const std::shared_ptr<boost::asio::ip::tcp::socket> client) noexcept override
  {
    // Ex: send event  
  }
  
  void on_client_disconnect(const std::shared_ptr<boost::asio::ip::tcp::socket> client) noexcept override
  {
    // Ex: send event  
  }
};

```

**Performance tweaking**

You can set a gap to the number of clients that can connect to the server to prevent overloading. Also, apart from that you can set the number of working threads that handle incoming requests. By default, it's left as 4 as many computers this day have at least 4 cores.

```cpp
#include <iostream>

#include "net/web_server.hpp"

constexpr auto HOST = "127.0.0.1";
constexpr auto PORT = 54321;

// default 1000
constexpr auto MAXIMUM_NR_CONNECTIONS = 100;

// default 4
constexpr auto NR_THREADS = 10;

int main() try
{
	net::web_server server(HOST, PORT,MAXIMUM_NR_CONNECTIONS, NR_THREADS);
  ...

```
### Creating a client

```cpp

#include <iostream>
#include "net/web_client.hpp"

constexpr auto URL = "127.0.0.1";
constexpr auto PORT = 54321;
  
int main()
{
  net::web_client web_client{};
  
	if (!web_client.connect(URL, PORT))
	{
	  	std::cerr << "Failed to connect to server";
		return;
	}
}
```

**HTTPS client**
```cpp

// by default nullptr
auto verify_certificate_callback =  [](bool preverified, boost::asio::ssl::verify_context& ctx) -> bool {
        // Your custom verification logic here
        // You can access preverified 
        // and verify_context as needed
        // Return true if the certificate is accepted, 
        // false otherwise
        
        // Example: Always accept the certificate
        return true;  
    };
    
std::vector<std::string> cert_files = { R"(..\..\..\external\boost_asio\example\cpp11\ssl\ca.pem)" };

net::secure_web_client web_client{cert_files, verify_certificate_callback};

if (!web_client.connect(URL))
{
	std::cerr << "Failed to connect to server";
	return;
}
``` 

**Sending basic empty request**

```cpp

...
std::string method = "/test";

net::http_request req(net::request_type::GET,
	method, net::content_type::any);
	
auto response = web_client.send(std::move(req));

if (!response.second)
{
	std::cerr << "Failed to get response err: " 
		<< response.second .message();
	return 5;
}
...
```
**Sending additional header flags**
```cpp
...

nlohmann::json additional_header_data = nlohmann::json({
	{"Accept", "*/*"},
	{"Connection", "keep-alive"}
	});

net::http_request req(net::request_type::GET,
	method, net::content_type::any,
	additional_header_data);
	
auto timeout = 5000; // miliseconds

auto response = web_client.send(std::move(req), timeout);

// handle recieved message

response->get_json_body(); 
// response->get_body_raw() for raw data

... 

```

**Adding a body**
```cpp
...

nlohmann::json additional_header_data = nlohmann::json({
	{"Accept", "*/*"},
	{"Connection", "keep-alive"}
	});

std::string body_data = R"({"a": 1, "b": 1 })";

net::http_request req(net::request_type::GET,
	method, net::content_type::any,
	additional_header_data,
	std::vector<uint8_t>(body_data.begin(), body_data.end()));
	
auto timeout = 5000;

auto response = web_client.send(std::move(req), timeout);
...

```

**Compressing data**
```cpp
...
req.gzip_compress_body();

auto timeout = 5000;
auto follow_redirects = true;

web_client.send(std::move(req), timeout, follow_redirects);
...

```
**Sending messages asynchronously**
```cpp

nlohmann::json additional_header_data = nlohmann::json({
{"Accept", "*/*"},
{"Connection", "keep-alive"},
{"Accept-Encoding", "gzip, deflate, br"}
});

net::http_request req(net::request_type::GET, method, net::content_type::any, additional_header_data);

net::async_get_callback req_callback;

bool can_stop = false;

req_callback = [&can_stop, &method, &url, &web_client, &req_callback](std::shared_ptr<net::ihttp_message> response, utile::web_error err_msg) {
	if (!response)
	{
		std::cerr << "Failed: " << err_msg.message();
		exit(5);
	}

	auto is_encoded = response->is_body_encoded();
  
	auto response_data_decoded = 
		response->to_string(is_encoded);
  
    // in case raw undecoded data is needed
	if (is_encoded)
	{
		auto response_data_encoded = response->to_string();
		...
	}

	can_stop = true;
};

web_client.send_async(std::move(req), req_callback);

// prevent main loop return
while (!can_stop)
{
	std::this_thread::sleep_for(std::chrono::seconds(1));
}

```

## Legacy

### Message format

**Note**: Data will be presented in json format for easier understanding, it 
is being sent as raw bits through the sockets.
```json
{
  "header": 
  {
    "id": "<nr>",
    "type":  "<nr>",
    "size_t": "<nr>"
  },
  "body": "<raw_bytes>"
}
```

**Note**: Messages can take 4 types of data: basic data types, json, std::vector<uint8_t> representing raw bytes and anything inheriting serializable_data. This is due to them copying raw data to the message body. So to be able to store them within messages we would need to have a way to serialize and deserialize data as well as take it's size.

**Basic data types**
```cpp
...
net::message<TestingMessage> msg;
msg.m_header.m_type = TestingMessage::OK_MESSAGE;

msg << false;
...
```

**Vector of bytes**
```cpp
...
net::message<TestingMessage> msg;
msg.m_header.m_type = TestingMessage::BIG_DATA;

std::vector<uint8_t> data(100000, 1);

msg << data;
...
```

**Json**

```cpp
...
net::message<TestingMessage> msg;
msg.m_header.m_type = TestingMessage::BIG_DATA;

auto json_data = nlohman::json::parse(R"({"a": 1})");

msg << json_data;
...
```

**Note**: Be careful when reading data from messages as if you add in this order A B C you would retrieve C B A. This is done due to avoiding more data allocations of std::vector container.

**Example**

```cpp
...
net::message<TestingMessage> msg;
msg.m_header.m_type = TestingMessage::OK_MESSAGE;

auto var1 = false;
auto var2 = true;

msg << var1;
msg << var2;

bool var1_cpy;
bool var2_cpy;

msg >> var2_cpy;
msg >> var1_cpy;
...
```
### Server

**Note**: To be able to create a server you will need to implement the server interface. It has almost the same methods as with the web server but methods mapping is no longer done trough callbacks, but through only one method "on_message", making it harder to scale.

```cpp
#include "net/server.hpp"

class test_server : public net::server<TestingMessage>
{
protected:
  void on_message(std::shared_ptr<net::connection<TestingMessage>> client, net::message<TestingMessage>& msg) noexcept override
  {
    switch (msg.m_header.m_type)
    {
    case TestingMessage::TEST:
    	// do smth
    	break;
    ...
    }
  }
};

constexpr auto URL = "127.0.0.1";
constexpr auto PORT = 54321;

int main()
{
  test_server server(URL, PORT);
  
  server.start();
  
  // infinite loop to prevent main thread exit
  while (true)
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

  return 0;
}
```

### Client

```cpp
#include "net/client.hpp"

constexpr auto URL = "127.0.0.1";
constexpr auto PORT = 54321;

int main()
{
  net::client<TestingMessage> client{};
  
  if (!client.connect(URL, PORT))
  {
  	std::cerr << "Failed to connect to the server";
  	return 1;
  }
  
  return 0;
}
```

**Sending messages**

**Note**: Sending is always done asynchronously with the help of a message queue.

```cpp
...
net::message<TestingMessage> msg;
msg.m_header.m_type = TestingMessage::BIG_DATA;

std::vector<uint8_t> data(100000, 1);

msg << data;

client.send(msg);

auto ans = client.wait_for_answear(5000);

if (ans == std::nullopt)
{
	std::cerr << "Test failed timeout reciving answear";
  return;
}

auto& msg = ans.value().m_msg;

// handle response
...

```

## Docker Test Image

To be able to run the docker image please use windows container as the executable is using msvc redistributables.
It will spawn a server that can be connected to from localhost:54321. It only has one method available for testing purposes "/test" that 
is supposed to receive a json body of form "{"a": <nr>, "b": <nr>}".

Build container:

docker build --tag <image_name> .

To run container:

docker run -p 54321:54321 <image_name>

Note: Change to root directory of the projects before running any kind of docker related commands. The server is build on a ubuntu revision and the project will be built inside the container, no need for manual building before running the commands.

# Contributing

1. Fork the repository
2. Create a new branch: `git checkout -b feature-name`
3. Make your changes and commit them: `git commit -m 'Description of changes'`
4. Push to the branch: `git push origin feature-name`
5. Submit a pull request

# License

This project is licensed under the [MIT License](LICENSE.MD).
