#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <climits>

using namespace std;
using chrono::nanoseconds;
using chrono::duration_cast;
using chrono::high_resolution_clock;

// Лінійне виконання
void linearExecution(const vector<int>& data, int& count, int& maxValue) {
    count = 0;
    maxValue = INT_MIN;
    for (int value : data) {
        if (value % 5 == 0) {
            count++;
            if (value > maxValue) {
                maxValue = value;
            }
        }
    }
}

// Обробка секції з використанням м'ютексу
void processSectionWithMutex(int start, int end, const vector<int>& data, int& localCount, int& localMax, mutex& mtx) {
    int count = 0;
    int maxValue = INT_MIN;
    for (int i = start; i < end; ++i) {
        if (data[i] % 5 == 0) {
            count++;
            if (data[i] > maxValue) {
                maxValue = data[i];
            }
        }
    }
    lock_guard<mutex> lock(mtx);
    localCount += count;
    if (maxValue > localMax) {
        localMax = maxValue;
    }
}

// Паралельне виконання з використанням м'ютексу
void parallelWithMutex(const vector<int>& data, int& count, int& maxValue, int numThreads) {
    count = 0;
    maxValue = INT_MIN;
    mutex mtx;
    vector<thread> threads;
    int chunkSize = data.size() / numThreads;

    for (int t = 0; t < numThreads; ++t) {
        int start = t * chunkSize;
        int end = (t == numThreads - 1) ? data.size() : start + chunkSize;
        threads.emplace_back(processSectionWithMutex, start, end, cref(data), ref(count), ref(maxValue), ref(mtx));
    }

    for (auto& th : threads) {
        if (th.joinable()) {
            th.join();
        }
    }
}

// Обробка секції з використанням CAS
void processSectionWithCAS(int start, int end, const vector<int>& data, atomic<int>& atomicCount, atomic<int>& atomicMax) {
    int localCount = 0;
    int localMax = INT_MIN;
    for (int i = start; i < end; ++i) {
        if (data[i] % 5 == 0) {
            localCount++;
            if (data[i] > localMax) {
                localMax = data[i];
            }
        }
    }
    atomicCount.fetch_add(localCount, memory_order_relaxed);

    int currentMax = atomicMax.load(memory_order_relaxed);
    while (localMax > currentMax && !atomicMax.compare_exchange_weak(currentMax, localMax, memory_order_relaxed)) {
        currentMax = atomicMax.load(memory_order_relaxed);
    }
}

// Паралельне виконання з використанням CAS
void parallelWithCAS(const vector<int>& data, int& count, int& maxValue, int numThreads) {
    atomic<int> atomicCount(0);
    atomic<int> atomicMax(INT_MIN);
    vector<thread> threads;
    int chunkSize = data.size() / numThreads;

    for (int t = 0; t < numThreads; ++t) {
        int start = t * chunkSize;
        int end = (t == numThreads - 1) ? data.size() : start + chunkSize;
        threads.emplace_back(processSectionWithCAS, start, end, cref(data), ref(atomicCount), ref(atomicMax));
    }

    for (auto& th : threads) {
        if (th.joinable()) {
            th.join();
        }
    }

    count = atomicCount.load();
    maxValue = atomicMax.load();
}

int main() {
    vector<int> matrixSizes = {10000, 1000000, 100000000, 2000000000};
    vector<int> threadCounts = {8, 16, 32, 64, 128, 256};

    cout << "\nTest Results:\n";
    cout << "Matrix Size\tThreads\tMode\tTime (seconds)\tCount\tMax Value\n";

    for (int matrixSize : matrixSizes) {
        vector<int> data(matrixSize);
        srand(static_cast<unsigned>(time(nullptr)));
        for (int i = 0; i < matrixSize; ++i) {
            data[i] = rand() % 1001; // Генерація чисел від 0 до 1000
        }

        // Лінійне виконання
        int count = 0;
        int maxValue = INT_MIN;
        auto start = high_resolution_clock::now();
        linearExecution(data, count, maxValue);
        auto end = high_resolution_clock::now();
        double elapsed = duration_cast<nanoseconds>(end - start).count() * 1e-9;
        cout << matrixSize << "\t\t-\tLinear\t" << fixed << setprecision(6) << elapsed << "\t" << count << "\t" << maxValue << endl;

        // Паралельне виконання з м'ютексом
        for (int numThreads : threadCounts) {
            count = 0;
            maxValue = INT_MIN;
            start = high_resolution_clock::now();
            parallelWithMutex(data, count, maxValue, numThreads);
            end = high_resolution_clock::now();
            elapsed = duration_cast<nanoseconds>(end - start).count() * 1e-9;
            cout << matrixSize << "\t\t" << numThreads << "\tMutex\t" << fixed << setprecision(6) << elapsed << "\t" << count << "\t" << maxValue << endl;
        }

        // Паралельне виконання з CAS
        for (int numThreads : threadCounts) {
            count = 0;
            maxValue = INT_MIN;
            start = high_resolution_clock::now();
            parallelWithCAS(data, count, maxValue, numThreads);
            end = high_resolution_clock::now();
            elapsed = duration_cast<nanoseconds>(end - start).count() * 1e-9;
            cout << matrixSize << "\t\t" << numThreads << "\tCAS\t" << fixed << setprecision(6) << elapsed << "\t" << count << "\t" << maxValue << endl;
        }

        cout << endl;
    }

    return 0;
}