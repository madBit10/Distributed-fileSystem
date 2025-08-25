# 📂 Distributed File System  

A C-based **Distributed File System (DFS)** implemented using **TCP sockets**, designed to manage file storage, retrieval, and deletion across multiple servers with redundancy and fault tolerance.  

---

## 🚀 Features
- **Client–Server Architecture**  
  - `S25Client.c`: Handles user commands (upload, download, remove).  
  - `S1.c`, `S2.c`, `S3.c`, `S4.c`: Act as distributed storage servers.  

- **File Operations Supported**  
  - `UPLOAD` → Store file chunks across servers.  
  - `DOWN` → Retrieve file chunks and reassemble.  
  - `REMF` → Remove files from all servers.  

- **Fault Tolerance**  
  Even if one server fails, files can still be retrieved from remaining servers.  

- **Networking Concepts**  
  Built on **TCP sockets**, demonstrating real-world distributed computing principles.  

---

## 🛠️ Tech Stack
- **Language**: C  
- **Core Concepts**: Socket Programming, File I/O, Distributed Systems, TCP/IP  
- **Environment**: Linux/Unix  

---

## ⚙️ How It Works
1. **Start the Servers**  
   Run each of the server programs (`S1`, `S2`, `S3`, `S4`):  
   ```bash
   gcc S1.c -o S1 && ./S1
   gcc S2.c -o S2 && ./S2
   gcc S3.c -o S3 && ./S3
   gcc S4.c -o S4 && ./S4


2. **Compile the Servers and Client**
   -Use gcc to compile each server and the client
   ```bash
   gcc S1.c -o S1
   gcc S2.c -o S2
   gcc S3.c -o S3
   gcc S4.c -o S4
   gcc S25Client.c -o client

3. **Run the Servers**
   -In separate terminals, start each server
   ```bash
   ./S1 <S1_port> <S2_ip> <S2_port> <S3_ip> <S3_port> <S4_ip> <S4_port>
   ./S2 <S2_ip> <S2_port>
   ./S3 <S3_ip> <S3_port>
   ./S4 <S4_ip> <S4_port>

4. **Run the Client**
   -In a new terminal, start the client:
   ```bash
   ./S25client <client_ip> <S1_port>
