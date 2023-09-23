#include "Buffer.h"

LocklessBuffer::LocklessBuffer(size_t capacity)
    : buffer_(std::make_unique<Message[]>(capacity)), capacity_(capacity), current_index_read(0), current_index_write(0), current_writing_count_(0)
{
}

LocklessBuffer::~LocklessBuffer() {
    buffer_.reset();
}

//返回缓冲区的容量大小
size_t LocklessBuffer::capacity() const
{
    return capacity_;
}


//返回当前正在写入数据的索引位置
size_t LocklessBuffer::current_index() const
{
    return std::min(current_index_.load(std::memory_order_relaxed), capacity_);
}

size_t LocklessBuffer::get_data_num() const {
    size_t W_Sub_R = current_index_write.load(std::memory_order_acquire) - current_index_read.load(std::memory_order_acquire);
    //读索引<写索引 W-R
    return W_Sub_R > 0 ? W_Sub_R : capacity_ + W_Sub_R;
}

size_t LocklessBuffer::get_current_index_read() const {
    return current_index_read.load(std::memory_order_acquire);
}

size_t LocklessBuffer::get_current_index_write() const {
    return current_index_write.load(std::memory_order_acquire);
}

void LocklessBuffer::update_current_index_read() {
    std::cout << "------------------update read index-------------------" << std::endl;
    current_index_read.fetch_add(1, std::memory_order_acq_rel);
    std::cout << "-------current read index value：" << current_index_read.load(std::memory_order_acquire) << std::endl;
    // ring buffer
    if (current_index_read.load(std::memory_order_acquire) >= capacity_) {
        current_index_read.store(0, std::memory_order_release);
    }
}

//返回当前正在写入数据的线程数量
size_t LocklessBuffer::current_writing_count() const
{
    return current_writing_count_.load(std::memory_order_acquire);
}

//该函数用于获取一个可写入数据的槽位，相当于添加message
bool LocklessBuffer::push(Message& message)
{
    std::cout << "push------添加数据函数------push" << std::endl;
    std::cout << "push------写索引的大小：" << current_index_write.load(std::memory_order_acquire) << std::endl;
    std::cout << "push------读索引的大小：" << current_index_read.load(std::memory_order_acquire) << std::endl;
    if (current_index_write.load(std::memory_order_acquire) < capacity_ && current_index_write.load(std::memory_order_acquire) + 1 != current_index_read.load(std::memory_order_acquire))
    {
        y.store(false, std::memory_order_release);
        current_writing_count_.fetch_add(1, std::memory_order_release);
        std::cout << "push!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!当前有多少个线程正在写数据：" << current_writing_count_.load(std::memory_order_acquire) << std::endl;

        size_t index = current_index_write.load(std::memory_order_acquire);

        //返回得到的数据
        if (index < capacity_)
        {
            std::cout << "push-----成功返回数据指针" << std::endl;
            current_writing_count_.fetch_sub(1, std::memory_order_release);
            std::cout << "push-----添加完数据减1后写线程的数量：" << current_writing_count_.load(std::memory_order_acquire) << std::endl;
            buffer_[index] = std::move(message);
            std::cout << "=========================最新写入的数据对应的下标为：" << index << std::endl;
            std::cout << "=========================最新写入的数据为：" << buffer_[index].inputs << std::endl;
            std::atomic_thread_fence(std::memory_order_release); //释放栅栏
            y.store(true, std::memory_order_release);// 2 在栅栏后存储y
        }

        // update write index
        current_index_write.fetch_add(1, std::memory_order_release);
        std::cout << "push----写入一个数据后更新后写索引的大小：" << current_index_write.load(std::memory_order_acquire) << std::endl;

        // 判断写索引是否到达缓冲区尽头
        if (current_index_write.load(std::memory_order_acquire) >= capacity_ && current_index_read.load(std::memory_order_acquire) != 0) {
            current_index_write.store(0, std::memory_order_release);
        }
        return true;
    }
    //说明此时缓冲区已满
    std::cout << "push-----缓冲区已满" << std::endl;
    return false;
}

//通知缓冲区完成写入
void LocklessBuffer::finish_writing()
{
    current_writing_count_.fetch_sub(1, std::memory_order_release);
}

//检查缓冲区是否准备好进行读取
//条件1：不能有线程进行写操作
//条件2：当前
bool LocklessBuffer::is_ready() const
{
    std::cout << "is_ready------判断是否可以读------is_ready，当前正在写入的线程数：" << current_writing_count() << std::endl;
    return current_index_read.load(std::memory_order_acquire) < capacity_ && current_writing_count() == 0;
}

bool LocklessBuffer::is_empty() const
{
    while (!y.load(std::memory_order_acquire));// 3 在#2写入前， 持续等待
    std::atomic_thread_fence(std::memory_order_acquire); //获得栅栏
    
    std::cout << "is_empyt------------这里判断缓冲区是否为空------------is_empyt" << std::endl;
    std::cout << "---------读索引" << current_index_read.load(std::memory_order_acquire) << std::endl;
    std::cout << "---------写索引" << current_index_write.load(std::memory_order_acquire) << std::endl;
    std::cout << "---------读索引和写索引是否想等？：" << (current_index_write.load(std::memory_order_acquire) == current_index_read.load(std::memory_order_acquire)) << std::endl;
    //const Message& element = buffer_[current_index_read];
    //std::cout << "#############" << element.inputs << std::endl;
    
    return current_index_write.load(std::memory_order_acquire) == current_index_read.load(std::memory_order_acquire);
}

//返回缓冲区的数据指针
Message* LocklessBuffer::data()
{
    return buffer_.get();
}

/*
所有的写操作必须用memory_order_acq_rel
*/
