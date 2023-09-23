#include "ZobristHash.h"

// Use a constant instead of transposing the key depending on the side to play.
// This is more efficient, since I can more easily undo operations (so I can
// incrementally update the key).

static const uint64_t BLACK_TO_MOVE = 0x8913125CFB309AFC;   // Random number to XOR into black moves
static const uint64_t SELECT_STAGE = 0x7385925CFB389ABA;

void ZobristHash::InitializeZobristTable()
{

	for (int ii = 0; ii < BOARD_SIZE; ii++)
	{
		for (int jj = 0; jj < NUM_CHANNEL; jj++)
		{
			m_aZobristTable[ii][jj] = data[ii * 5 + jj];
		}
	}

}


ZobristHash::ZobristHash()
{
	InitializeZobristTable();
	//sqlite3* m_db;
	int result = sqlite3_open_v2("F:\\database\\hash_table.db", &m_db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
	if (result != SQLITE_OK)
		throw SqliteError(result);

}


ZobristHash::~ZobristHash()
{}


int ZobristHash::GetChessType(int piece) {
	//1 空   2 白皇后   3 黑皇后   4 障碍   5 选子
	if (piece == EMPTY) return 0;
	if (piece == WHITEQUEUE) return 1;
	if (piece == BLACKQUEUE) return 2;
	if (piece == STONE) return 3;
	return 4;
}

//
//uint64_t ZobristHash::CalculateZobristKey(int sideToMove, const int board[][10]) {
//	for (unsigned int ii = 0; ii < BOARD_SIZE / 10; ii++) {
//		for (unsigned int jj = 0; jj < BOARD_SIZE / 10; jj++) {
//			std::cout << board[ii][jj] << " ";
//		}
//		std::cout << std::endl;
//	}
//
//	uint64_t uZobristKey = 0;
//
//	int channel = 0;
//
//	for (unsigned int ii = 0; ii < BOARD_SIZE / 10; ii++) {
//		for (unsigned int jj = 0; jj < BOARD_SIZE / 10; jj++) {
//			if (EMPTY != board[ii][jj]) {
//
//				channel = GetChessType(board[ii][jj]);
//
//				uZobristKey ^= m_aZobristTable[ii * 10 + jj][4];
//			}
//		}
//	}
//
//	if (sideToMove == BLACKQUEUE)
//	{
//		uZobristKey ^= BLACK_TO_MOVE;
//	}
//
//	return uZobristKey;
//}


uint64_t ZobristHash::CalculateZobristKey(int sideToMove, const int board[][10], int select) {
	for (unsigned int ii = 0; ii < BOARD_SIZE / 10; ii++) {
		for (unsigned int jj = 0; jj < BOARD_SIZE / 10; jj++) {
			std::cout << board[ii][jj] << " ";
		}
		std::cout << std::endl;
	}

	uint64_t uZobristKey = 0;

	int channel = 0;

	for (unsigned int ii = 0; ii < BOARD_SIZE / 10; ii++) {
		for (unsigned int jj = 0; jj < BOARD_SIZE / 10; jj++) {
			if (EMPTY != board[ii][jj]) {

				channel = GetChessType(board[ii][jj]);

				uZobristKey ^= m_aZobristTable[ii*10+jj][4];
			}
		}
	}

	std::cout << "select:" << select << std::endl;
	if (select != -1) {
		std::cout << "进行选子^操作" << std::endl;
		uZobristKey ^= m_aZobristTable[select][channel];
	}

	if (sideToMove == BLACKQUEUE)
	{
		uZobristKey ^= BLACK_TO_MOVE;
	}

	return uZobristKey;
}


uint64_t ZobristHash::UpdateZobristKey(uint64_t oldKey, int piece, int action, int stage, int sideToMove) {

	uint64_t newKey = oldKey;
	
	//判断是否为选子阶段
	if (stage == 0) {
		newKey ^= m_aZobristTable[action][4];
	}
	else {

		// 得到当前channel
		int channel = GetChessType(piece);

		newKey ^= m_aZobristTable[action][channel];

	}
	//判断是否为黑方移动
	if (sideToMove == BLACKQUEUE)
	{
		newKey ^= BLACK_TO_MOVE;
	}
	//std::cout << "newKey:" << newKey << std::endl;
	return newKey;
}

void getDbState(sqlite3* m_db) {
	// 获取数据库状态
	int dbStatus = sqlite3_db_readonly(m_db, "main");
	if (dbStatus == 1) {
		std::cout << "Database is in read-only mode." << std::endl;
	}
	else if (dbStatus == 0) {
		std::cout << "Database is in read-write mode." << std::endl;
	}
	else if (dbStatus == 2) {
		std::cout << "Database is 实际上无法进行写入操作。这通常是由于其他进程或线程正在写入数据库而造成的." << std::endl;
	}
	else {
		std::cerr << "Failed to determine database status." << std::endl;
	}
}

void write_test() {
	sqlite3_config(SQLITE_CONFIG_MULTITHREAD);
	std::cout << "-------writeTest-------" << std::endl;
	
	getDbState(m_db);

	std::string insertQuery = "UPDATE hash SET value = 'write_test' WHERE key = 'test000';";
	//insertQuery = "insert into hash (key, value) values ('18:28', 'value');";
	std::cout << insertQuery << std::endl;
	char* errMsg;

	int rc = sqlite3_exec(m_db, insertQuery.c_str(), nullptr, nullptr, &errMsg);
	if (rc == SQLITE_BUSY) {
		std::cout << "Database is busy, wait for a short time before trying again" << std::endl;
		std::cerr << "SQL语句执行出错: " << errMsg << std::endl;
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	else if (rc != SQLITE_OK) {
		std::cerr << "rc:" << rc << std::endl;
		std::cerr << "SQL语句执行出错: " << errMsg << std::endl;
		sqlite3_free(errMsg);
		sqlite3_close(m_db);
	}
	else {
		std::cout << "-----insert a kv-----" << std::endl;
		exit(3);
	}
	sqlite3_close_v2(m_db);
}


std::pair<torch::Tensor, float> ZobristHash::read_thread(Sqlite* db, uint64_t query) {
	std::cout << "---------------------read-----------------------hashKey：" << query << std::endl;

	//writeTest();
	rwlock::LockRead _(lock_);

	std::thread::id threadId = std::this_thread::get_id();

	std::hash<std::thread::id> hasher;
	size_t hashValue = hasher(threadId);
	int intThreadId = static_cast<int>(hashValue);

	return db->test_read(intThreadId, query);
}


void ZobristHash::readTest() {
	std::string selectQuery = "SELECT value FROM hash WHERE key = 'test000';";
	sqlite3_stmt* stmt;

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
		std::cout << "###value:" << value_str << "###" << std::endl;
	}
}


void ZobristHash::writeTest() {
	std::string insertQuery = "UPDATE hash SET value = '09:29' WHERE key = 'test';";
	//insertQuery = "insert into hash (key, value) values ('20230822000', 'value');";
	std::cout << insertQuery << std::endl;
	char* errMsg;

	int rc = sqlite3_exec(m_db, insertQuery.c_str(), nullptr, nullptr, &errMsg);
	std::thread::id threadId = std::this_thread::get_id();
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
		//exit(3);
	}
	write_test();
}


// write thread
void ZobristHash::write_thread(Sqlite* db, InfoStruct& info) {

	rwlock::LockWrite _(lock_);

	std::thread::id threadId = std::this_thread::get_id();
	// Convert std::thread::id to integer using std::hash
	std::hash<std::thread::id> hasher;
	size_t hashValue = hasher(threadId);
	int intThreadId = static_cast<int>(hashValue);

	db->write(info);
}


void ZobristHash::write(uint64_t key, std::pair<torch::Tensor, float> value) {
	sqlite3_config(SQLITE_CONFIG_MULTITHREAD);
	std::cout << "---------------------write-----------------------" << std::endl;

	// 构建插入数据的 SQL 语句
	//std::string insertQuery = "INSERT INTO hash (key, value) VALUES ('" + strValue + "', '" + jsonString + "');";
	std::string insertQuery = "UPDATE hash SET value = '0822' WHERE key = 'test';";
	//insertQuery = "insert into hash (key, value) values ('20230822000', 'value');";
	std::cout << insertQuery << std::endl;
	char* errMsg;

	int rc = sqlite3_exec(m_db, insertQuery.c_str(), nullptr, nullptr, &errMsg);
	std::thread::id threadId = std::this_thread::get_id();
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
		exit(3);
	}
	//sqlite3_close_v2(m_db);
}

