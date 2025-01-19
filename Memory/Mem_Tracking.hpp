#include <iostream>
#include <cstdint>

struct AllocationMetrics {
    uint32_t total_allocated = 0;
    uint32_t total_freed = 0;

    void CurrentUsage() const {
        std::cout << "Metrics:\n" << "   Allocated: " << total_allocated << " bytes\n   Freed:     " << total_freed << " bytes\n"; 
    }
};

static auto s_metrics = AllocationMetrics(); 

auto operator new(size_t size) -> void* {
    s_metrics.total_allocated += size;

    return malloc(size);
}

auto operator delete(void *memory, size_t size) -> void {
    s_metrics.total_freed += size;
    free(memory);
}

auto operator delete(void *memory) -> void {
    free(memory);
}
