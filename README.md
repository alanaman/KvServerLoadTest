# KvServer

## Overview
KvServer is a high-performance, thread-safe key-value store built with modern C++17. It supports concurrent HTTP requests for accessing and manipulating data in a PostgreSQL database, backed by a sharded in-memory cache for low-latency lookups. The server is designed to handle high-throughput workloads and provides functionality to access, update, and delete keys efficiently.

---

## Architecture

### Key Components:
1. **HTTP Server:**
   - Uses the `httplib` library to handle HTTP requests.
   - Routes include:
     - `GET /key/{id}`: Fetches a key from the cache or database.
     - `PUT /key/{id}`: Updates or inserts a key-value pair.
     - `DELETE /key/{id}`: Deletes a key from the database and cache.
   - Multi-threaded request handling is supported using thread pools.

2. **PostgreSQL Database Integration:**
   - SQL queries are managed using the `sqlpp11` ORM for compile-time query validation.
   - The database includes a single table `key_value` with:
     - `key` (integer): Primary key.
     - `value` (text): Associated value.

3. **Sharded Cache:**
   - Implemented as an LRU (Least Recently Used) in-memory cache.
   - Sharding ensures high concurrency by dividing the cache into independent partitions, each protected by its own mutex, allowing threads to operate on different shards in parallel.

4. **Connection Pool:**
   - Database connections are optimized using a thread-safe connection pool, minimizing connection creation overhead.

---

## Build Instructions

### Prerequisites
- CMake (v3.15 or higher)
- A C++17 compatible compiler
- Docker (optional, to use the provided Docker setup)
- PostgreSQL (locally or via Docker Compose)

### Steps to Build and Run the Application

1. **Clone the Repository:**
   ```bash
   git clone https://github.com/alanaman/KvServerLoadTest.git
   cd KvServerLoadTest
   ```

2. **Build with CMake:**
   ```bash
   mkdir build
   cd build
   cmake ..
   make
   ```

3. **Run the Application:**
   ```bash
   ./server/my_app [num_threads]
   # Replace [num_threads] with the desired thread count for the thread pool.
   ```

Alternatively, you can use the Docker-based setup for building and running the application.

---

## Setting Up with Docker Compose

1. **Start the Services:**
   - Ensure Docker is installed and running.
   ```bash
   docker-compose up -d
   ```

2. **Attach to the Server:**
   - The server should be running and accessible at `http://localhost:8000`.

3. **Stop the Services:**
   ```bash
   docker-compose down
   ```

---

## HTTP Endpoints

### 1. `GET /key/{id}`
- Fetches the value for a given key.
- **Path Parameters:** 
  - `{id}`: The key to fetch (integer).
- **Responses:**
  - `200 OK`: Value found in cache or database.
  - `404 Not Found`: Key does not exist.
  - `400 Bad Request`: Invalid key format.

### Example:
```bash
curl http://localhost:8000/key/1
```

---

### 2. `PUT /key/{id}`
- Inserts or updates a key-value pair.
- **Path Parameters:**
  - `{id}`: The key to insert or update (integer).
- **Request Body:**
  - The value to associate with the key (plain text).
- **Responses:**
  - `200 OK`: Key-value pair updated.
  - `400 Bad Request`: Invalid key format.

### Example:
```bash
curl -X PUT http://localhost:8000/key/1 -d "example_value"
```

---

### 3. `DELETE /key/{id}`
- Deletes a key-value pair.
- **Path Parameters:**
  - `{id}`: The key to delete (integer).
- **Responses:**
  - `200 OK`: Key found and deleted.
  - `404 Not Found`: Key does not exist.
  - `400 Bad Request`: Invalid key format.

### Example:
```bash
curl -X DELETE http://localhost:8000/key/1
```

---

## Environment Variables

The application requires the following environment variables for configuration:
- `DATABASE_HOST`: Database server hostname (default: `localhost`)
- `DATABASE_NAME`: Name of the PostgreSQL database (`kv_db`)
- `DATABASE_USER`: Username for the database (`kv_app`)
- `DATABASE_PASSWORD`: Password for the database (`mysecretpassword`)

---