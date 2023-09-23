#pragma once

#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "sqlite3.h"

class SQLiteConnectionPool {
private:
    std::queue<sqlite3*> connectionQueue;
    std::mutex mtx;
    std::condition_variable cond;
    int maxConnections;
    int usedConnections;

public:
    SQLiteConnectionPool(int maxConns = 10) : maxConnections(maxConns), usedConnections(0) {
        // 初始化连接
        for (int i = 0; i < maxConns; i++) {
            sqlite3* db;
            if (sqlite3_open("F:\\database\\hash_table.db", &db) == SQLITE_OK) {
                connectionQueue.push(db);
            }
            else {
                std::cerr << "Failed to create SQLite connection." << std::endl;
            }
            char* errMsg;
            const char* setWALMode = "PRAGMA journal_mode=WAL;";
            if (sqlite3_exec(db, setWALMode, 0, 0, &errMsg) != SQLITE_OK) {
                std::cerr << "SQL error: " << errMsg << std::endl;
                sqlite3_free(errMsg);
                sqlite3_close(db);
                exit(1);
            }
        }
        std::cout << "connectionPool init ok" << std::endl;
    }

    ~SQLiteConnectionPool() {
        std::lock_guard<std::mutex> lock(mtx);
        while (!connectionQueue.empty()) {
            sqlite3_close(connectionQueue.front());
            connectionQueue.pop();
        }
    }

    sqlite3* acquireConnection() {
        std::unique_lock<std::mutex> lock(mtx);
        while (connectionQueue.empty()) {
            cond.wait(lock);
        }

        sqlite3* conn = connectionQueue.front();
        connectionQueue.pop();
        usedConnections++;
        return conn;
    }

    void releaseConnection(sqlite3* conn) {
        std::lock_guard<std::mutex> lock(mtx);
        connectionQueue.push(conn);
        usedConnections--;
        cond.notify_one();
    }

    int getUsedConnectionsCount() {
        std::lock_guard<std::mutex> lock(mtx);
        return usedConnections;
    }
};