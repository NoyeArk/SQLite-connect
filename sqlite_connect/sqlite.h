#include <iostream>
#include <thread>
#include <vector>
#include <sstream>
#include <cstdint>
#include <unordered_map>
#include <string>
#include <torch/torch.h>
#include <torch/script.h>

#include "rwlock.h"
#include "sqlite3.h"
#include "nlohmann/json.hpp"


using json = nlohmann::json; 

struct InfoStruct {
    uint64_t key;
    uint64_t parentKey1;
    std::pair<torch::Tensor, float> value;

    //----------debug”√----------
    int stage;
    int action;

    void assign(uint64_t key_, uint64_t parentKey1_, std::pair<torch::Tensor, float> value_) {
        key = key_;
        parentKey1 = parentKey1_;
        value = value_;
    }

    void assign(uint64_t key_, uint64_t parentKey1_, std::pair<torch::Tensor, float> value_, int stage_, int action_) {
        key = key_;
        parentKey1 = parentKey1_;
        value = value_;

        stage = stage_;
        action = action_;
    }
};

class Log {
public:
    static std::mutex lock_;

    static void log(const std::string& msg) {
        std::lock_guard<std::mutex> _(lock_);
        std::cout << "[";
        std::cout << std::this_thread::get_id();
        std::cout << "] ";
        std::cout << msg;
        std::cout << std::endl;
    }
    
};

class SqliteError : public std::exception {
public:
    explicit SqliteError(int error_code) : m_errno(error_code) {
        m_errmsg = sqlite3_errstr(error_code);
    }
    explicit SqliteError(sqlite3* db) {
        m_errno = sqlite3_errcode(db);
        m_errmsg = sqlite3_errmsg(db);
    }
    SqliteError(const SqliteError& src) = default;
    virtual ~SqliteError() throw() { }
    virtual const char* what() const throw() override { return m_errmsg; }
    int code() const throw() { return m_errno; }
private:
    int m_errno;
    const char* m_errmsg;
};

class Sqlite {
public:
    sqlite3* m_db;
    sqlite3_stmt* stmt;


    Sqlite(sqlite3* db) { 
        m_db = db;
        Log::log("db opened");
    }
    ~Sqlite() { test_close(); }
    void open(const char* file);
    void close();
    void simple_execute(const char* lpszSql);
    void begin_transaction();
    void commit();
    void rollback();
    
    void test_open(const char* file);
    void test_close();

    std::pair<torch::Tensor, float> test_read(int idx, uint64_t query);
    std::pair<torch::Tensor, float> read(uint64_t key);
    std::pair<torch::Tensor, float> parseValueFromDatabase(const std::string& value_str);

    void test_write(int idx, InfoStruct&);
    void write(InfoStruct&);
    
};
