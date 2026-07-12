// Arduino Moving Average Library
// https://github.com/JChristensen/movingAvg
// Copyright (C) 2018 by Jack Christensen and licensed under
// GNU GPL v3.0, https://www.gnu.org/licenses/gpl.html

// Extended to template class for handling of multiple data types

#ifndef MOVINGAVG_H_INCLUDED
#define MOVINGAVG_H_INCLUDED

template <typename T>
class movingAvg
{
    public:
        movingAvg(int interval)
            : m_interval{interval}, m_nbrReadings{0}, m_sum{0}, m_next{0}, m_readings{nullptr} {}
        ~movingAvg() { delete[] m_readings; }
        void begin();
        T reading(T newReading);
        T getAvg();
        T getAvg(int nPoints);
        int getCount() { return m_nbrReadings; }
        void reset();
        T* getReadings() { return m_readings; }
        T to2PI(T a);

    private:
        int m_interval;     // number of data points for the moving average
        int m_nbrReadings;  // number of readings
        // Sum type adapts to T: long for integers, T for floating point
        using SumType = typename std::conditional<std::is_floating_point<T>::value, T, long>::type;
        SumType m_sum;
        int m_next;         // index to the next reading
        T* m_readings;      // pointer to the dynamically allocated interval array
};

// moving average for angle type of data (wind, course, rotation)
// makes sense for data types double and float; angle in radians [0..2pi]
template <typename T>
class movingAvgAngle
{
    public:
        movingAvgAngle(int interval)
            : m_interval{interval}, m_nbrReadings{0}, m_sumSin{0}, m_sumCos{0}, m_next{0}, m_buffer{nullptr} {}
        ~movingAvgAngle() { delete[] m_buffer; }
        void begin();
        T reading(T newReading);
        T getAvg();
        T getAvg(int nPoints);
        int getCount() { return m_nbrReadings; }
        void reset();
        T* getReadings() { return m_buffer; }
        T to2PI(T a);

    private:
        int m_interval;            // number of data points for the moving average
        int m_nbrReadings;         // number of readings
        double m_sumSin, m_sumCos; // sum for angle values should always be double for precision reasons, regardless of class type
        int m_next;                // index to the next reading
        T* m_buffer;               // pointer to the dynamically allocated interval array
};

// Include the implementation to satisfy template instantiation requirements
#include "movingAvg.tpp"

#endif
