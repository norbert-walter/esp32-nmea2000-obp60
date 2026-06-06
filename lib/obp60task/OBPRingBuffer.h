#pragma once
#include "FreeRTOS.h"
#include "GwSynchronized.h"
#include <vector>
#include <WString.h>

template <typename T>
struct PSRAMAllocator {
    using value_type = T;

    PSRAMAllocator() = default;

    template <class U>
    constexpr PSRAMAllocator(const PSRAMAllocator<U>&) noexcept { }

    T* allocate(std::size_t n)
    {
        void* ptr = heap_caps_malloc(n * sizeof(T), MALLOC_CAP_SPIRAM);
        if (!ptr) {
            return nullptr;
        } else {
            return static_cast<T*>(ptr);
        }
    }

    void deallocate(T* p, std::size_t) noexcept
    {
        heap_caps_free(p);
    }
};

template <class T, class U>
bool operator==(const PSRAMAllocator<T>&, const PSRAMAllocator<U>&) { return true; }

template <class T, class U>
bool operator!=(const PSRAMAllocator<T>&, const PSRAMAllocator<U>&) { return false; }

template <typename T>
class RingBuffer {
private:
    std::vector<T, PSRAMAllocator<T>> buffer; // THE buffer vector, allocated in PSRAM
    size_t capacity;
    size_t head; // Points to the next insertion position
    size_t first; // Points to the first (oldest) valid element
    size_t last; // Points to the last (newest) valid element
    size_t count; // Number of valid elements currently in buffer
    bool is_Full; // Indicates that all buffer elements are used and ringing is in use
    T MIN_VAL; // lowest possible value of buffer of type <T>
    T MAX_VAL; // highest possible value of buffer of type <T> -> indicates invalid value in buffer
    double dblMIN_VAL, dblMAX_VAL; // MIN_VAL, MAX_VAL in double format
    mutable SemaphoreHandle_t bufLocker;

    // metadata for buffer
    String dataName; // Name of boat data in buffer
    String dataFmt; // Format of boat data in buffer
    int updFreq; // Update frequency in milliseconds
    double mltplr; // Multiplier which transforms original <double> value into buffer type format
    double smallest; // Value range of buffer: smallest value; needs to be => MIN_VAL
    double largest; // Value range of buffer: biggest value; needs to be < MAX_VAL, since MAX_VAL indicates invalid entries

    void initCommon();

public:
    RingBuffer();
    RingBuffer(size_t size);
    void setMetaData(String name, String format, int updateFrequency, double multiplier, double minValue, double maxValue); // Set meta data for buffer
    void setFormat(String format); // Specify format of buffer
    bool getMetaData(String& name, String& format, int& updateFrequency, double& multiplier, double& minValue, double& maxValue); // Get meta data of buffer
    bool getMetaData(String& name, String& format);
    String getName() const; // Get buffer name
    String getFormat() const; // Get buffer data format
    int getUpdFreq() const; // Get buffer update frequency
    void add(const double& value); // Add a new value to  buffer
    double get(size_t index) const; // Get value at specific position (0-based index from oldest to newest)
    double getFirst() const; // Get the first (oldest) value in buffer
    double getLast() const; // Get the last (newest) value in buffer
    double getMin() const; // Get the lowest value in buffer
    double getMin(size_t amount) const; // Get minimum value of the last <amount> values of buffer
    double getMax() const; // Get the highest value in buffer
    double getMax(size_t amount) const; // Get maximum value of the last <amount> values of buffer
    double getMid() const; // Get mid value between <min> and <max> value in buffer
    double getMid(size_t amount) const; // Get mid value between <min> and <max> value of the last <amount> values of buffer
    double getMedian() const; // Get the median value in buffer
    double getMedian(size_t amount) const; // Get the median value of the last <amount> values of buffer
    size_t getCapacity() const; // Get the buffer capacity (maximum size)
    size_t getCurrentSize() const; // Get the current number of elements in buffer
    size_t getFirstIdx() const; // Get the index of oldest value in buffer
    size_t getLastIdx() const; // Get the index of newest value in buffer
    bool isEmpty() const; // Check if buffer is empty
    bool isFull() const; // Check if buffer is full
    double getMinVal() const; // Get lowest possible value for buffer
    double getMaxVal() const; // Get highest possible value for buffer; used for unset/invalid buffer data
    void clear(); // Clear buffer
    void resize(size_t size); // Delete buffer and set new size
    double operator[](size_t index) const; // Operator[] for convenient access (same as get())
    std::vector<double> getAllValues() const; // Get all current values in native buffer format as a vector
    std::vector<double> getAllValues(size_t amount) const; // Get last <amount> values in native buffer format as a vector
};

#include "OBPRingBuffer.tpp"