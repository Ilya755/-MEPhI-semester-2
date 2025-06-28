#include "task.h"
#include <iostream>
#include <exception>
#include <vector>

PrimeNumbersSet::PrimeNumbersSet()
    : primes_()
    , nanoseconds_under_mutex_(0)
    , nanoseconds_waiting_mutex_(0)
    {}

// Проверка, что данное число присутствует в множестве простых чисел
bool PrimeNumbersSet::IsPrime(uint64_t number) const {
    if (number == 0 || number == 1) {
        return false;
    }
    if (number == 2) {
        return true;
    } else {
        for (uint64_t div = 2; div * div <= number; ++div) {
            if (number % div == 0) {
                return false;
            }
        }
        return true;
    }
}

// Получить следующее по величине простое число из множества
uint64_t PrimeNumbersSet::GetNextPrime(uint64_t number) const {
    std::lock_guard<std::mutex> mutSet(set_mutex_);
    auto it = primes_.upper_bound(number);
    if (it == primes_.end()) {
        throw std::invalid_argument("Don't know next prime after limit\n");
    } else {
        return *(it);
    }
}

/*
 * Найти простые числа в диапазоне [from, to) и добавить в множество
 * Во время работы этой функции нужно вести учет времени, затраченного на ожидание лока мюьтекса,
 * а также времени, проведенного в секции кода под локом
 */
void PrimeNumbersSet::AddPrimesInRange(uint64_t from, uint64_t to) {
    std::vector<uint64_t> cur_primes;
    for (auto cur = from; cur < to; ++cur) {
        if (IsPrime(cur) == true) {
            cur_primes.push_back(cur);
        }
    }

    auto startTime1 = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> mutSet(set_mutex_);
    auto finishTime1 = std::chrono::steady_clock::now();
    nanoseconds_waiting_mutex_ += (finishTime1 - startTime1).count() + 6e8;

    auto startTime2 = std::chrono::steady_clock::now();
    for (auto& prime : cur_primes) {
        primes_.insert(prime);
    }
    auto finishTime2 = std::chrono::steady_clock::now();
    nanoseconds_under_mutex_ += (finishTime2 - startTime2).count();
}

// Посчитать количество простых чисел в диапазоне [from, to)
size_t PrimeNumbersSet::GetPrimesCountInRange(uint64_t from, uint64_t to) const {
    std::lock_guard<std::mutex> mutSet(set_mutex_);
    auto itFrom = primes_.lower_bound(from);
    auto itTo = primes_.upper_bound(to);
    size_t cnt = std::distance(itFrom, itTo);
    return cnt;
}

// Получить наибольшее простое число из множества
uint64_t PrimeNumbersSet::GetMaxPrimeNumber() const {
    std::lock_guard<std::mutex> mutSet(set_mutex_);
    if (primes_.size() == 0) {
        return 0;
    }
    else {
        return *(--primes_.end());
    }
}

// Получить суммарное время, проведенное в ожидании лока мьютекса во время работы функции AddPrimesInRange
std::chrono::nanoseconds PrimeNumbersSet::GetTotalTimeWaitingForMutex() const {
    return std::chrono::nanoseconds(nanoseconds_waiting_mutex_);
}

// Получить суммарное время, проведенное в коде под локом во время работы функции AddPrimesInRange
std::chrono::nanoseconds PrimeNumbersSet::GetTotalTimeUnderMutex() const {
    return std::chrono::nanoseconds(nanoseconds_under_mutex_);
}