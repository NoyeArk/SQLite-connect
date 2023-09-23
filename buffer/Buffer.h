#pragma once

#include <torch/torch.h>
#include <future>
#include <atomic>
#include <thread>
#include <memory>

struct Message {
    torch::Tensor inputs;
    std::promise<std::pair<torch::Tensor, torch::Tensor> > outputs;
};

class LocklessBuffer
{
private:
    std::unique_ptr<Message[]> buffer_;
    const size_t capacity_;
    std::atomic<size_t> current_index_;
    std::atomic<size_t> current_index_read;
    std::atomic<size_t> current_index_write;
    std::atomic<size_t> current_writing_count_;
    std::atomic<bool> y;

public:
    LocklessBuffer(size_t capacity);
    ~LocklessBuffer();
    size_t capacity() const;
    size_t current_index() const;
    size_t get_current_index_read() const;
    size_t get_current_index_write() const;
    size_t get_data_num() const;
    void update_current_index_read();
    size_t current_writing_count() const;
    bool push(Message&);
    void finish_writing();
    bool is_ready() const;
    bool is_empty() const;
    Message* data();
};