# Simple HTTP Server in C (Built From Scratch)

This project is a minimal HTTP server written entirely in C, without using any external libraries or frameworks.  
It implements low-level networking using POSIX sockets and includes a manually written HTTP request parser.

The goal is to understand how HTTP works internally — request parsing, routing, content serving, headers, and responses — by building everything step-by-step.

# Architecture Diagram

        +--------------------------+
        |        Client            |
        +------------+-------------+
                     |
                     v (TCP)
        +------------+-------------+
        |     Linux Socket API     |
        |  (accept, recv, send)    |
        +------------+-------------+
                     |
         For each connection:
                     v
        +------------+-------------+
        |     pthread_create       |
        +------------+-------------+
                     |
                     v
        +------------+-------------+
        |    handle_client()       |
        |  - parse HTTP request    |
        |  - router lookup         |
        |  - static fallback       |
        +------------+-------------+
                     |
                     v
        +------------+-------------+
        |  Route Handler Function  |
        |  (C function pointer)    |
        +------------+-------------+
                     |
                     v
        +------------+-------------+
        |   HTTP Response Builder  |
        +--------------------------+


---

## 🚀 Features

### ✔ Manual HTTP Parsing
- Parses method, path, version  
- Parses headers: `Host`, `Content-Length`, `Connection`  
- Reads request body for POST

### ✔ Routing Support

| Path  | Method | Description                      |
|-------|---------|----------------------------------|
| `/`   | GET     | Returns a simple HTML page       |
| `/json` | GET   | Returns a JSON response          |
| `/time` | GET   | Returns current server time      |
| `/echo` | POST  | Echoes back POST JSON body       |

---

## 🛠 Build

Compile using GCC:


gcc server.c -o httpserver


▶️ Run the Server

./httpserver


🧪 Testing the Server

GET request

curl http://localhost:8080/


JSON endpoint

curl http://localhost:8080/json


POST request

curl -X POST http://localhost:8080/echo -d '{"name":"vijay"}'


📊 Performance Testing
Load Testing with wrk

The server was benchmarked using wrk, a high-performance HTTP load generator.

The following command was used:

wrk -t8 -c1000 -d30s http://localhost:8080/


Explanation:

-t8 → Use 8 worker threads

-c1000 → Maintain 1000 open TCP connections

-d30s → Run the test for 30 seconds


✔ Test Results

Running 30s test @ http://localhost:8080/
  8 threads and 1000 connections

  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     28.10ms   6.59ms   55.06ms   79.16%
    Req/Sec      4.45k    0.94k     5.85k    62.75%

  1,063,364 requests in 30.03s, 130.82MB read
  Requests/sec: 35404.80
  Transfer/sec: 4.36MB


📌 Interpretation

The server successfully handled 1,000+ concurrent clients.

Served more than 1 million requests in 30 seconds.

Achieved 35,404 requests per second, which is excellent for a multithreaded C server.

Average latency was 28 ms, with a maximum of 55 ms under heavy load.



🔮 Future Enhancements

1. Routing Table / Router Module
Add a proper router with path matching, method-based routing (GET, POST…), and dynamic routes.

2. HTTP/1.1 Keep-Alive Support
Reduce latency and improve throughput by keeping connections open for multiple requests.

3. Thread Pool Instead of Per-Thread Model
Replace one-thread-per-connection with a reusable pool to support thousands of clients more efficiently.

4. Non-Blocking I/O (epoll/kqueue) Version
Build a high-performance event-driven server like Nginx for handling 10K+ concurrent clients.

5. Static File Serving Module
Add ability to serve files (HTML, CSS, JS, images) from a public directory with caching headers.
 




📄 Notes

This project is for learning how HTTP works internally.

Everything is implemented manually using raw sockets and C.

No third-party libraries or frameworks are used.



