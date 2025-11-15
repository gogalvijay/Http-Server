# Simple HTTP Server in C (Built From Scratch)

This project is a minimal HTTP server written entirely in C, without using any external libraries or frameworks.  
It implements low-level networking using POSIX sockets and includes a manually written HTTP request parser.

The goal is to understand how HTTP works internally — request parsing, routing, content serving, headers, and responses — by building everything step-by-step.

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







📄 Notes

This project is for learning how HTTP works internally.

Everything is implemented manually using raw sockets and C.

No third-party libraries or frameworks are used.



