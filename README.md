# Introduction

## General Vision

LocalMarketplacePlatform is an application that facilitates local commerce through a concurrent client-server architecture. The system allows users to create, manage, and purchase products intuitively via the terminal using simple commands. By prioritizing ease of use and efficiency, the platform promotes direct interaction between local buyers and sellers.

## Project Objectives

The project aims to develop a reliable application that implements at least ten essential functionalities, including:
- Viewing, creating, and deleting product listings.
- Managing additional product details and real-time purchases.
- User management, including authentication, registration, and credential recovery.
- Viewing transaction history and facilitating searches by category.

# Applied Technologies

The application is implemented in the C programming language, including components optimized for resource management and support for multiple interactions. The use of mutexes allows simultaneous access to resources while ensuring data integrity in concurrent environments.

## Protocol and Memory Management

The server utilizes the TCP protocol for communication between clients and the server. Messages are managed through a multithreading architecture, where each client is served by a dedicated thread. Data persistence is ensured by SQLite3, an easily integrable and high-performance DBMS that manages user and transaction data.

## Concurrency Management

To prevent issues related to shared data between threads, mutexes have been implemented to ensure exclusive access to critical resources. This allows read and write operations to be managed without client interference.

