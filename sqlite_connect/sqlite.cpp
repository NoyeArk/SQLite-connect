#include "sqlite.h"

std::mutex Log::lock_;

void Sqlite::open(const char* file) {
    std::cout << "--------------开启数据库---------------" << std::endl;
    sqlite3_config(SQLITE_CONFIG_MULTITHREAD);
    int result = sqlite3_open_v2(file, &m_db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (result != SQLITE_OK)
        throw SqliteError(result);
}


void Sqlite::close() {
    //if (m_db) {
    //    sqlite3_close_v2(m_db);
    //    std::cout << "--------------关闭数据库---------------" << std::endl;
    //    m_db = NULL;
    //    //std::cout << "release database m_db" << std::endl;
    //}
    std::cout << "--------------解除数据库连接---------------" << std::endl;
    if (stmt) {
        //std::cout << "stmt：" << stmt << std::endl;
        //std::cout << "begin release stmt" << std::endl;
        //sqlite3_finalize(stmt);
        //std::cout << "21" << std::endl;
        stmt = NULL;
        //std::cout << "release database stmt" << std::endl;
    }
}


void Sqlite::simple_execute(const char* lpszSql) {
    int result = sqlite3_exec(m_db, lpszSql, NULL, NULL, NULL);
    if (result != SQLITE_OK)
        throw SqliteError(result);
}


void Sqlite::begin_transaction() {
    simple_execute("BEGIN TRANSACTION");
}


void Sqlite::commit() {
    simple_execute("COMMIT TRANSACTION");
}


void Sqlite::rollback() {
    simple_execute("ROLLBACK TRANSACTION");
}


void Sqlite::test_open(const char* file) {
    open(file);
    Log::log("db opened");
}


void Sqlite::test_close() {
    close();
    Log::log("db closed");
}


std::pair<torch::Tensor, float> Sqlite::test_read(int idx, uint64_t query) {

    try {
        std::pair<torch::Tensor, float> result = read(query);
        std::string msg = std::to_string(idx);
        msg += " db read finished.";
        Log::log(msg);
        return result;
    }
    catch (SqliteError& e) {
        std::string msg = std::to_string(idx);
        msg += " db read error: ";
        msg += e.what();
        Log::log(msg);
        exit(1);
    }
}

void Sqlite::test_write(int idx, InfoStruct& info) {
    try {
        write(info);
        std::string msg = std::to_string(idx);
        msg += " db write finished.";
        Log::log(msg);
    }
    catch (SqliteError& e) {
        std::string msg = std::to_string(idx);
        msg += " db write error: ";
        msg += e.what();
        Log::log(msg);
    }
}


std::pair<torch::Tensor, float> Sqlite::read(uint64_t key)
{
    std::pair<torch::Tensor, float> result;

    // 将key转换为text类型
    std::ostringstream oss;
    oss << key;
    std::string strKey = oss.str();

    std::string selectQuery = "SELECT value FROM hash WHERE key = '" + strKey + "';";

    int rc = sqlite3_prepare_v2(m_db, selectQuery.c_str(), -1, &stmt, 0);
    //std::cout << "unlock db successfully" << std::endl;
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << sqlite3_errmsg(m_db) << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        exit(2);
    }

    // 获取查询结果并解析value
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* value_str = sqlite3_column_text(stmt, 0);
        std::string strValue(reinterpret_cast<const char*>(value_str));
        result = parseValueFromDatabase(strValue);

        // 现在你可以使用result了，它是std::pair<torch::Tensor, float>类型的值
        torch::Tensor tensor = result.first;
        float floatValue = result.second;
    }

    return result;
}


// 解析数据库中的value为std::pair<torch::Tensor, float>类型
std::pair<torch::Tensor, float> Sqlite::parseValueFromDatabase(const std::string& value_str) {
    // 将 JSON 字符串转换为 JSON 对象
    json j = json::parse(value_str);

    // 解析JSON对象中的floatValue和tensor字段
    float floatValue = j["floatValue"];
    std::vector<float> tensorData = j["tensor"];

    // 将vector<float>转换为torch::Tensor
    torch::Tensor tensor = torch::from_blob(tensorData.data(), { (long long)(tensorData.size()) }, torch::kFloat32);

    return std::make_pair(tensor, floatValue);
}


void Sqlite::write(InfoStruct& info) {

    uint64_t key = info.key;
    uint64_t parentKey1 = info.parentKey1;
    std::pair<torch::Tensor, float> value = info.value;
    
    // 转化为字符串
    std::string keyText = std::to_string(key);
    std::string parentKey1Text = std::to_string(parentKey1);

    torch::Tensor tensor_data = (torch::Tensor)(value.first);

    // 将 Tensor 数据转换为 std::vector<float>
    std::vector<float> tensor_data_vector(tensor_data.data_ptr<float>(), tensor_data.data_ptr<float>() + tensor_data.numel());

    // 将数据转换为 JSON 格式
    json jsonData;
    jsonData["tensor"] = tensor_data_vector;
    jsonData["floatValue"] = value.second;
    std::string jsonString = jsonData.dump();


    // 构建插入数据的 SQL 语句
    std::string insertQuery = "INSERT INTO hash (key, value, parent1, stage, action) VALUES ( \
        '" + keyText + "', '" + jsonString + "', '" + parentKey1Text + "', '" + std::to_string(info.stage) + "', '" + std::to_string(info.action) + "' \
        );";
    //std::cout << insertQuery << std::endl;
    char* errMsg;

    int rc = sqlite3_exec(m_db, insertQuery.c_str(), nullptr, nullptr, &errMsg);
    std::thread::id threadId = std::this_thread::get_id();
    do {
        if (rc == SQLITE_BUSY) {
            std::cout << "[" << threadId << "] Database is busy, wait for a short time before trying again" << std::endl;
            std::cerr << "SQL语句执行出错: " << errMsg << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        else if (rc != SQLITE_OK) {
            std::cerr << "rc:" << rc << std::endl;
            std::cerr << "SQL语句执行出错: " << errMsg << std::endl;
            sqlite3_free(errMsg);
            sqlite3_close(m_db);
            exit(1);
        }
        else {
            std::cout << "-----insert a kv-----" << std::endl;
            break;
        }
    } while (rc != SQLITE_OK);
}