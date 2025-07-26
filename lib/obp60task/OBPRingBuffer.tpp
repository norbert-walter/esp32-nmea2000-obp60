#include "OBPRingBuffer.h"

template <typename T>
RingBuffer<T>::RingBuffer(size_t size)
    : capacity(size)
    , head(0)
    , first(0)
    , last(0)
    , count(0)
    , is_Full(false)
{
    bufLocker = xSemaphoreCreateMutex();

    if (size == 0) {
        // return false;
    }

    MIN_VAL = std::numeric_limits<T>::lowest();
    MAX_VAL = std::numeric_limits<T>::max();
    dataName = "";
    dataFmt = "";
    updFreq = -1;
    smallest = MIN_VAL;
    largest = MAX_VAL;

    buffer.resize(size, MIN_VAL);

    // return true;
}

// Specify meta data of buffer content
template <typename T>
void RingBuffer<T>::setMetaData(String name, String format, int updateFrequency, T minValue, T maxValue)
{
    GWSYNCHRONIZED(&bufLocker);
    dataName = name;
    dataFmt = format;
    updFreq = updateFrequency;
    smallest = std::max(MIN_VAL, minValue);
    largest = std::min(MAX_VAL, maxValue);
}

// Get meta data of buffer content
template <typename T>
bool RingBuffer<T>::getMetaData(String& name, String& format, int& updateFrequency, T& minValue, T& maxValue)
{
    if (dataName == "" || dataFmt == "" || updFreq == -1) {
        return false; // Meta data not set
    }

    GWSYNCHRONIZED(&bufLocker);
    name = dataName;
    format = dataFmt;
    updateFrequency = updFreq;
    minValue = smallest;
    maxValue = largest;
    return true;
}

// Get buffer name
template <typename T>
String RingBuffer<T>::getName() const
{
    return dataName;
}

// Add a new value to buffer
template <typename T>
void RingBuffer<T>::add(const T& value)
{
    GWSYNCHRONIZED(&bufLocker);
    if (value < smallest || value > largest) {
        buffer[head] = MIN_VAL; // Store MIN_VAL if value is out of range
    } else {
        buffer[head] = value;
    }
    last = head;

    if (is_Full) {
        first = (first + 1) % capacity; // Move pointer to oldest element when overwriting
    } else {
        count++;
        if (count == capacity) {
            is_Full = true;
        }
    }

    head = (head + 1) % capacity;
}

// Get value at specific position (0-based index from oldest to newest)
template <typename T>
T RingBuffer<T>::get(size_t index) const
{
    GWSYNCHRONIZED(&bufLocker);
    if (isEmpty() || index < 0 || index >= count) {
        return MIN_VAL;
    }

    size_t realIndex = (first + index) % capacity;
    return buffer[realIndex];
}

// Operator[] for convenient access (same as get())
template <typename T>
T RingBuffer<T>::operator[](size_t index) const
{
    return get(index);
}

// Get the first (oldest) value in the buffer
template <typename T>
T RingBuffer<T>::getFirst() const
{
    if (isEmpty()) {
        return MIN_VAL;
    }
    return get(0);
}

// Get the last (newest) value in the buffer
template <typename T>
T RingBuffer<T>::getLast() const
{
    if (isEmpty()) {
        return MIN_VAL;
    }
    return get(count - 1);
}

// Get the lowest value in the buffer
template <typename T>
T RingBuffer<T>::getMin() const
{
    if (isEmpty()) {
        return MIN_VAL;
    }

    T minVal = MAX_VAL;
    T value;
    for (size_t i = 0; i < count; i++) {
        value = get(i);
        if (value < minVal && value != MIN_VAL) {
            minVal = value;
        }
    }
    return minVal;
}

// Get minimum value of the last <amount> values of buffer
template <typename T>
T RingBuffer<T>::getMin(size_t amount) const
{
    if (isEmpty() || amount <= 0) {
        return MIN_VAL;
    }
    if (amount > count)
        amount = count;

    T minVal = MAX_VAL;
    T value;
    for (size_t i = 0; i < amount; i++) {
        value = get(count - 1 - i);
        if (value < minVal && value != MIN_VAL) {
            minVal = value;
        }
    }
    return minVal;
}

// Get the highest value in the buffer
template <typename T>
T RingBuffer<T>::getMax() const
{
    if (isEmpty()) {
        return MIN_VAL;
    }

    T maxVal = MIN_VAL;
    T value;
    for (size_t i = 0; i < count; i++) {
        value = get(i);
        if (value > maxVal && value != MIN_VAL) {
            maxVal = value;
        }
    }
    return maxVal;
}

// Get maximum value of the last <amount> values of buffer
template <typename T>
T RingBuffer<T>::getMax(size_t amount) const
{
    if (isEmpty() || amount <= 0) {
        return MIN_VAL;
    }
    if (amount > count)
        amount = count;

    T maxVal = MIN_VAL;
    T value;
    for (size_t i = 0; i < amount; i++) {
        value = get(count - 1 - i);
        if (value > maxVal && value != MIN_VAL) {
            maxVal = value;
        }
    }
    return maxVal;
}

// Get mid value between <min> and <max> value in the buffer
template <typename T>
T RingBuffer<T>::getMid() const
{
    if (isEmpty()) {
        return MIN_VAL;
    }

    return (getMin() + getMax()) / static_cast<T>(2);
}

// Get mid value between <min> and <max> value of the last <amount> values of buffer
template <typename T>
T RingBuffer<T>::getMid(size_t amount) const
{
    if (isEmpty() || amount <= 0) {
        return MIN_VAL;
    }

    if (amount > count)
        amount = count;

    return (getMin(amount) + getMax(amount)) / static_cast<T>(2);
}

// Get the median value in the buffer
template <typename T>
T RingBuffer<T>::getMedian() const
{
    if (isEmpty()) {
        return MIN_VAL;
    }

    // Create a temporary vector with current valid elements
    std::vector<T> temp;
    temp.reserve(count);

    for (size_t i = 0; i < count; i++) {
        temp.push_back(get(i));
    }

    // Sort to find median
    std::sort(temp.begin(), temp.end());

    if (count % 2 == 1) {
        // Odd number of elements
        return temp[count / 2];
    } else {
        // Even number of elements - return average of middle two
        // Note: For integer types, this truncates. For floating point, it's exact.
        return (temp[count / 2 - 1] + temp[count / 2]) / 2;
    }
}

// Get the median value of the last <amount> values of buffer
template <typename T>
T RingBuffer<T>::getMedian(size_t amount) const
{
    if (isEmpty() || amount <= 0) {
        return MIN_VAL;
    }
    if (amount > count)
        amount = count;

    // Create a temporary vector with current valid elements
    std::vector<T> temp;
    temp.reserve(amount);

    for (size_t i = 0; i < amount; i++) {
        temp.push_back(get(i));
    }

    // Sort to find median
    std::sort(temp.begin(), temp.end());

    if (amount % 2 == 1) {
        // Odd number of elements
        return temp[amount / 2];
    } else {
        // Even number of elements - return average of middle two
        // Note: For integer types, this truncates. For floating point, it's exact.
        return (temp[amount / 2 - 1] + temp[amount / 2]) / 2;
    }
}

// Get the buffer capacity (maximum size)
template <typename T>
size_t RingBuffer<T>::getCapacity() const
{
    return capacity;
}

// Get the current number of elements in the buffer
template <typename T>
size_t RingBuffer<T>::getCurrentSize() const
{
    return count;
}

// Get the first index of buffer
template <typename T>
size_t RingBuffer<T>::getFirstIdx() const
{
    return first;
}

// Get the last index of buffer
template <typename T>
size_t RingBuffer<T>::getLastIdx() const
{
    return last;
}

// Check if buffer is empty
template <typename T>
bool RingBuffer<T>::isEmpty() const
{
    return count == 0;
}

// Check if buffer is full
template <typename T>
bool RingBuffer<T>::isFull() const
{
    return is_Full;
}

// Get lowest possible value for buffer; used for non-set buffer data
template <typename T>
T RingBuffer<T>::getMinVal() const
{
    return MIN_VAL;
}

// Get highest possible value for buffer
template <typename T>
T RingBuffer<T>::getMaxVal() const
{
    return MAX_VAL;
}

// Clear buffer
template <typename T>
void RingBuffer<T>::clear()
{
    GWSYNCHRONIZED(&bufLocker);
    head = 0;
    first = 0;
    last = 0;
    count = 0;
    is_Full = false;
}

// Get all current values as a vector
template <typename T>
std::vector<T> RingBuffer<T>::getAllValues() const
{
    std::vector<T> result;
    result.reserve(count);

    for (size_t i = 0; i < count; i++) {
        result.push_back(get(i));
    }

    return result;
}