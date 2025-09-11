// Simple test for storage cleanup functionality
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

// Forward declarations to avoid full header includes
extern "C" {
    typedef struct cc_storage cc_storage;
    typedef void cc_storage_apply_func(void *, void *);
    
    cc_storage * cc_storage_construct(unsigned int size);
    cc_storage * cc_storage_construct_etc(unsigned int size,
                                         void (*constructor)(void *),
                                         void (*destructor)(void *));
    void cc_storage_destruct(cc_storage * storage);
    void * cc_storage_get(cc_storage * storage);
    void cc_storage_apply_to_all(cc_storage * storage, 
                                cc_storage_apply_func * func, 
                                void * closure);
}

std::atomic<int> constructor_count{0};
std::atomic<int> destructor_count{0};

void test_constructor(void* ptr) {
    int* data = static_cast<int*>(ptr);
    *data = 42;
    constructor_count++;
}

void test_destructor(void* ptr) {
    destructor_count++;
}

void simple_apply_func(void* data, void* closure) {
    std::atomic<int>* counter = static_cast<std::atomic<int>*>(closure);
    counter->fetch_add(1);
}

void thread_test(cc_storage* storage, int thread_id) {
    int* data = static_cast<int*>(cc_storage_get(storage));
    *data = thread_id;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

int main() {
    std::cout << "Simple Storage Test\n";
    std::cout << "==================\n\n";
    
    // Test basic storage functionality
    cc_storage* storage = cc_storage_construct_etc(sizeof(int), 
                                                  test_constructor, 
                                                  test_destructor);
    
    const int num_threads = 3;
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(thread_test, storage, i);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // Test applyToAll
    std::atomic<int> apply_count{0};
    cc_storage_apply_to_all(storage, simple_apply_func, &apply_count);
    
    std::cout << "Constructor calls: " << constructor_count.load() << std::endl;
    std::cout << "ApplyToAll count: " << apply_count.load() << std::endl;
    
    cc_storage_destruct(storage);
    
    std::cout << "Destructor calls: " << destructor_count.load() << std::endl;
    std::cout << "\n✓ Test completed successfully\n";
    
    return 0;
}