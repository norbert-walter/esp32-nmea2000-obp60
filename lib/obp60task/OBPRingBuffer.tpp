#include <algorithm>
#include <limits>
#include <cmath>

template <typename T>
RingBuffer<T>::RingBuffer()
    : capacity(0)
    , head(0)
    , first(0)
    , last(0)
    , count(0)
    , is_Full(false)
{
    initCommon();
    // <buffer> stays empty
}

template <typename T>
RingBuffer<T>::RingBuffer(size_t size)
    : capacity(size)
    , head(0)
    , first(0)
    , last(0)
    , count(0)
    , is_Full(false)
{
    initCommon();

    buffer.reserve(size);
    buffer.resize(size, NUMLIMIT_HIGH); // NUMLIMIT_HIGH indicate invalid values
}

template <typename T>
void RingBuffer<T>::initCommon()
{
    NUMLIMIT_LOW = std::numeric_limits<T>::lowest();
    NUMLIMIT_HIGH = std::numeric_limits<T>::max();
    dataName = "";
    dataFmt = "";
    updFreq = -1;
    mltplr = 1;
    BUFMIN_VAL = static_cast<double>(NUMLIMIT_LOW);
    BUFMAX_VAL = static_cast<double>(NUMLIMIT_HIGH);
    lowest = BUFMIN_VAL;
    highest = BUFMAX_VAL;
    bufLocker = xSemaphoreCreateMutex();
}

// Specify meta data of buffer content
template <typename T>
void RingBuffer<T>::setMetaData(String name, String format, int updateFrequency, double multiplier, double minValue, double maxValue)
{
    GWSYNCHRONIZED(&bufLocker);
    dataName = name;
    dataFmt = format;
    updFreq = updateFrequency;
    mltplr = multiplier;
    BUFMIN_VAL = static_cast<double>(NUMLIMIT_LOW) / mltplr; // lowest possible buffer value; converted to external view
    BUFMAX_VAL = static_cast<double>(NUMLIMIT_HIGH) / mltplr; // highest possible buffer value; converted to external view
    lowest = std::max(BUFMIN_VAL, minValue); // low value range, set by user
    highest = std::min(std::nextafter(BUFMAX_VAL, -std::numeric_limits<double>::infinity()), maxValue); // high value range, set by user; maximum is 1 tick lower than BUFMAX_VAL
}

// Specify format of buffer content
template <typename T>
void RingBuffer<T>::setFormat(String format)
{
    GWSYNCHRONIZED(&bufLocker);
    dataFmt = format;
}

// Get meta data of buffer content
template <typename T>
bool RingBuffer<T>::getMetaData(String& name, String& format, int& updateFrequency, double& multiplier, double& minValue, double& maxValue)
{
    if (dataName == "" || dataFmt == "" || updFreq == -1) {
        return false; // Meta data not set
    }

    GWSYNCHRONIZED(&bufLocker);
    name = dataName;
    format = dataFmt;
    updateFrequency = updFreq;
    multiplier = mltplr;
    minValue = lowest;
    maxValue = highest;
    return true;
}

// Get meta data of buffer content
template <typename T>
bool RingBuffer<T>::getMetaData(String& name, String& format)
{
    if (dataName == "" || dataFmt == "") {
        return false; // Meta data not set
    }

    GWSYNCHRONIZED(&bufLocker);
    name = dataName;
    format = dataFmt;
    return true;
}

// Get buffer name
template <typename T>
String RingBuffer<T>::getName() const
{
    return dataName;
}

// Get buffer data format
template <typename T>
String RingBuffer<T>::getFormat() const
{
    return dataFmt;
}

// Get buffer update frequency
template <typename T>
int RingBuffer<T>::getUpdFreq() const
{
    return updFreq;
}

// Add a new value to buffer
template <typename T>
void RingBuffer<T>::add(const double& value)
{
    GWSYNCHRONIZED(&bufLocker);
    if (value < lowest || value > highest) {
        buffer[head] = NUMLIMIT_HIGH; // Store maximum buffer value if data value is out of range
    } else {
        buffer[head] = static_cast<T>(std::round(value * mltplr));
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
    // Serial.printf("Ringbuffer: value %.3f, multiplier: %.1f, buffer: %d\n", value, mltplr, buffer[head]);
    head = (head + 1) % capacity;
}

// Get value at specific position (0-based index from oldest to newest)
template <typename T>
double RingBuffer<T>::get(size_t index) const
{
    GWSYNCHRONIZED(&bufLocker);
    if (isEmpty() || index < 0 || index >= count) {
        return BUFMAX_VAL;
    }

    size_t realIndex = (first + index) % capacity;
    return static_cast<double>(buffer[realIndex] / mltplr); // is BUFMAX_VAL if value is invalid
}

// Operator[] for convenient access (same as get())
template <typename T>
double RingBuffer<T>::operator[](size_t index) const
{
    return get(index);
}

// Get the first (oldest) value in the buffer
template <typename T>
double RingBuffer<T>::getFirst() const
{
    if (isEmpty()) {
        return BUFMAX_VAL;
    }
    return get(0);
}

// Get the last (newest) value in the buffer
template <typename T>
double RingBuffer<T>::getLast() const
{
    if (isEmpty()) {
        return BUFMAX_VAL;
    }
    return get(count - 1);
}

// Get the lowest value in the buffer
template <typename T>
double RingBuffer<T>::getMin() const
{
    return getMin(getCurrentSize());
}

// Get minimum value of the last <amount> values of buffer
template <typename T>
double RingBuffer<T>::getMin(size_t amount) const
{
    if (isEmpty() || amount <= 0) {
        return BUFMAX_VAL;
    }
    if (amount > count)
        amount = count;

    double minVal = BUFMAX_VAL;
    double value;
    for (size_t i = 0; i < amount; i++) {
        value = get(count - 1 - i);
        if (value < minVal && value != BUFMAX_VAL) {
            minVal = value;
        }
    }
    return minVal;
}

// Get the highest value in the buffer
template <typename T>
double RingBuffer<T>::getMax() const
{
    return getMax(getCurrentSize());
}

// Get maximum value of the last <amount> values of buffer
template <typename T>
double RingBuffer<T>::getMax(size_t amount) const
{
    if (isEmpty() || amount <= 0) {
        return BUFMAX_VAL;
    }
    if (amount > count)
        amount = count;

    double maxVal = BUFMIN_VAL;
    double value;
    for (size_t i = 0; i < amount; i++) {
        value = get(count - 1 - i);
        if (value > maxVal && value != BUFMAX_VAL) {
            maxVal = value;
        }
    }
    if (maxVal == BUFMIN_VAL) { // no change of initial value -> buffer has only invalid values (BUFMAX_VAL)
        maxVal = BUFMAX_VAL;
    }
    return maxVal;
}

// Get mid value between <min> and <max> value in the buffer
template <typename T>
double RingBuffer<T>::getMid() const
{
    return getMid(getCurrentSize());
}

// Get mid value between <min> and <max> value of the last <amount> values of buffer
template <typename T>
double RingBuffer<T>::getMid(size_t amount) const
{
    if (isEmpty() || amount <= 0) {
        return BUFMAX_VAL;
    }

    if (amount > count)
        amount = count;

    return (getMin(amount) + getMax(amount)) / 2;
}

// Get the median value in the buffer
template <typename T>
double RingBuffer<T>::getMedian() const
{
    return getMedian(getCurrentSize());
}

// Get the median value of the last <amount> values of buffer
template <typename T>
double RingBuffer<T>::getMedian(size_t amount) const
{
    if (isEmpty() || amount <= 0) {
        return BUFMAX_VAL;
    }
    if (amount > count)
        amount = count;

    // Create a temporary vector with current valid elements
    std::vector<double> temp;
    temp.reserve(amount);

    for (size_t i = 0; i < amount; i++) {
        temp.push_back(get(count - 1 - i));
    }

    // Sort to find median
    std::sort(temp.begin(), temp.end());

    if (temp[0] == BUFMAX_VAL) { // 1st element of sorted vector is already BUFMAX_VAL -> only invalid entries in buffer, so we return invalid value
       return BUFMAX_VAL;
    }

    if (amount % 2 == 1) {
        // Odd number of elements
        return temp[amount / 2];
    } else {
        // Even number of elements - return average of middle two
        return (temp[amount / 2 - 1] + temp[amount / 2]) / 2;
    }
}

// Get mid value of circle (degree) values of buffer
template <typename T>
double RingBuffer<T>::getCircularMid() const
{
    return getCircularMid(getCurrentSize());
}

// Get mid value of circle (degree) values of the last <amount> values of buffer
template <typename T>
double RingBuffer<T>::getCircularMid(size_t amount) const
{
    if (isEmpty() || amount <= 0) {
        return BUFMAX_VAL;
    }
    if (amount > count)
        amount = count;

    std::vector<double> a;
    // Create a temporary vector with current valid elements
    std::vector<double> temp;
    temp.reserve(amount);

    for (size_t i = 0; i < amount; i++) {
        temp.push_back(get(count - 1 - i));
    }

    // Sort to find largest gap
    std::sort(temp.begin(), temp.end());

    if (temp[0] == BUFMAX_VAL) { // 1st element of sorted vector is already BUFMAX_VAL -> only invalid entries in buffer, so we return invalid value
       return BUFMAX_VAL;
    }

    // Find the largest gap
    double largestGap = BUFMIN_VAL;
    std::size_t gapIndex = 0;

    for (std::size_t i = 0; i < temp.size(); ++i)
    {
        std::size_t next = (i + 1) % temp.size();

        double gap;
        if (next == 0)
            gap = (temp[0] + M_TWOPI) - temp[i];
        else
            gap = temp[next] - temp[i];

        if (gap > largestGap)
        {
            largestGap = gap;
            gapIndex = i;
        }
    }

    double start = temp[(gapIndex + 1) % temp.size()]; // Start of occupied arc = first angle after largest gap
    double arcWidth = M_TWOPI - largestGap; // Width of occupied arc
    arcWidth = start + arcWidth / 2.0;
    arcWidth = fmod(arcWidth, M_TWOPI);
    if (arcWidth < 0.0) {
        arcWidth += M_TWOPI;
    }

    return arcWidth; // Midpoint of occupied arc
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

// Get lowest possible value for buffer
template <typename T>
double RingBuffer<T>::getMinVal() const
{
    return BUFMIN_VAL;
}

// Get highest possible value for buffer; used for unset/invalid buffer data
template <typename T>
double RingBuffer<T>::getMaxVal() const
{
    return BUFMAX_VAL;
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

// Delete buffer and set new size
template <typename T>
void RingBuffer<T>::resize(size_t newSize)
{
    GWSYNCHRONIZED(&bufLocker);
    capacity = newSize;
    head = 0;
    first = 0;
    last = 0;
    count = 0;
    is_Full = false;

    buffer.clear();
    buffer.reserve(newSize);
    buffer.resize(newSize, NUMLIMIT_HIGH);
}

// Get all current values in native buffer format as a vector
template <typename T>
std::vector<double> RingBuffer<T>::getAllValues() const
{
    return getAllValues(getCurrentSize());
}

// Get last <amount> values in native buffer format as a vector
template <typename T>
std::vector<double> RingBuffer<T>::getAllValues(size_t amount) const
{
    std::vector<double> result;

    if (isEmpty() || amount <= 0) {
        return result;
    }
    if (amount > count)
        amount = count;

    result.reserve(amount);

    for (size_t i = 0; i < amount; i++) {
        result.push_back(get(count - 1 - i));
    }

    return result;
}