#pragma once
#include "GwSynchronized.h"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <vector>
#include "WString.h"

template <typename T>
class RingBuffer {
private:
    mutable SemaphoreHandle_t bufLocker;
    std::vector<T> buffer;
    size_t capacity;
    size_t head; // Points to the next insertion position
    size_t first; // Points to the first (oldest) valid element
    size_t last; // Points to the last (newest) valid element
    size_t count; // Number of valid elements currently in buffer
    bool is_Full; // Indicates that all buffer elements are used and ringing is in use
    T MIN_VAL; // lowest possible value of buffer
    T MAX_VAL; // highest possible value of buffer of type <T>

    // metadata for buffer
    String dataName; // Name of boat data in buffer
    String dataFmt; // Format of boat data in buffer
    int updFreq; // Update frequency in milliseconds
    T smallest; // Value range of buffer: smallest value
    T largest; // Value range of buffer: biggest value

public:
    RingBuffer(size_t size);
    void setMetaData(String name, String format, int updateFrequency, T minValue, T maxValue); // Set meta data for buffer
    bool getMetaData(String& name, String& format, int& updateFrequency, T& minValue, T& maxValue); // Get meta data of buffer
    String getName() const; // Get buffer name
    void add(const T& value); // Add a new value to  buffer
    T get(size_t index) const; // Get value at specific position (0-based index from oldest to newest)
    T getFirst() const; // Get the first (oldest) value in buffer
    T getLast() const; // Get the last (newest) value in buffer
    T getMin() const; // Get the lowest value in buffer
    T getMin(size_t amount) const; // Get minimum value of the last <amount> values of buffer
    T getMax() const; // Get the highest value in buffer
    T getMax(size_t amount) const; // Get maximum value of the last <amount> values of buffer
    T getMid() const; // Get mid value between <min> and <max> value in buffer
    T getMid(size_t amount) const; // Get mid value between <min> and <max> value of the last <amount> values of buffer
    T getMedian() const; // Get the median value in buffer
    T getMedian(size_t amount) const; // Get the median value of the last <amount> values of buffer
    size_t getCapacity() const; // Get the buffer capacity (maximum size)
    size_t getCurrentSize() const; // Get the current number of elements in buffer
    size_t getFirstIdx() const; // Get the index of oldest value in buffer
    size_t getLastIdx() const; // Get the index of newest value in buffer
    bool isEmpty() const; // Check if buffer is empty
    bool isFull() const; // Check if buffer is full
    T getMinVal() const; // Get lowest possible value for buffer; used for initialized buffer data
    T getMaxVal() const; // Get highest possible value for buffer
    void clear(); // Clear buffer
    T operator[](size_t index) const; // Operator[] for convenient access (same as get())
    std::vector<T> getAllValues() const; // Get all current values as a vector
};

#include "OBPRingBuffer.tpp"