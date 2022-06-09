// Copyright 2019 Mikhail Kazhamiaka
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <array>
#include <mutex>
#include <vector>
#include <condition_variable>
#include <unordered_map>
#include <unordered_set>

#include <iostream>

typedef struct {
    std::unordered_map<uint64_t, size_t> readers;
    std::unordered_set<uint64_t> writers;
    std::mutex mtx;
    std::condition_variable read_cv;
    std::condition_variable write_cv;
} LockTableEntry;

template<size_t SIZE>
class LockTable {
public:
    LockTable() {}

    /**
     * 等待直至可以合法读映射到该哈希槽上的键，并上读锁
     * 1. 等待直至该哈希槽上的所有写任务完成（写计数器归零）
     * 2. 添加读任务（该键上的读任务计数器 +1，注意该引用计数是细粒度的）
     * @param hash 键哈希（从而可以较粗粒度地管理键空间上的锁）
     */
    void readLock(uint64_t hash) {
        LockTableEntry &tableEntry = lockTable[hash % SIZE];
        std::unique_lock<std::mutex> l(tableEntry.mtx);
        tableEntry.read_cv.wait(l, [&tableEntry,&hash]{return tableEntry.writers.find(hash) == tableEntry.writers.end();});
        // 以下实际等效于 tableEntry.readers[hash]++;
        auto readerMapEntry = tableEntry.readers.find(hash);
        if (readerMapEntry == tableEntry.readers.end()) {
            tableEntry.readers[hash] = 1;
        } else {
            tableEntry.readers[hash]++;
        }
        //tableEntry.readers++;
    }

    /**
     * 解读锁，若读引用清零，则唤醒同哈希槽上的其他读写任务
     * @param hash 
     */
    void readUnlock(uint64_t hash) {
        LockTableEntry &tableEntry = lockTable[hash % SIZE];
        std::unique_lock<std::mutex> l(tableEntry.mtx);
        tableEntry.readers[hash]--;
        if (tableEntry.readers[hash] == 0) {
            tableEntry.readers.erase(hash);
            //l.unlock();
            tableEntry.read_cv.notify_all();
            tableEntry.write_cv.notify_all();
        }
        //tableEntry.readers--;
        //if (tableEntry.readers == 0) {
        //    l.unlock();
        //    tableEntry.write_cv.notify_all();
        //}
    }

    void readUnlock(std::vector<uint64_t> hashes) {
        std::unordered_set<uint64_t> unique_hashes;
        for (auto hash : hashes) {
            unique_hashes.insert(hash % SIZE);
        }

        for (auto hash : unique_hashes) {
            readUnlock(hash);
        }
    }

    /**
     * 等待直至该哈希槽上无其他读写任务，并上写锁
     * @param hash 
     */
    void writeLock(uint64_t hash) {
        //LogInfo("Obtaining write lock for " << hash << " (" << hash % SIZE << ")");
        LockTableEntry &tableEntry = lockTable[hash % SIZE];
        std::unique_lock<std::mutex> l(tableEntry.mtx);
        /*if (tableEntry.readers.find(hash) != tableEntry.readers.end()) {
            LogInfo("Cannot acquire write lock for " << hash << " (" << hash % SIZE << "), readers have it");
        } else if (tableEntry.writers.find(hash) != tableEntry.writers.end()) {
            LogInfo("Cannot acquire write lock for " << hash << " (" << hash % SIZE << "), writer has it");
        }*/
        tableEntry.write_cv.wait(l, [&tableEntry,&hash]{return tableEntry.writers.find(hash) == tableEntry.writers.end() && tableEntry.readers.find(hash) == tableEntry.readers.end();});
        tableEntry.writers.insert(hash);
        //LogInfo("Acquired write lock for " << hash << " (" << hash % SIZE << ")");
        //tableEntry.writers++;
    }

    void writeLock(std::vector<uint64_t> hashes) {
        std::unordered_set<uint64_t> unique_hashes;
        for (auto hash : hashes) {
            unique_hashes.insert(hash % SIZE);
        }

        for (auto hash : unique_hashes) {
            writeLock(hash);
        }
    }

    /**
     * 解写锁，放行同哈希槽上的其他读写任务
     * @param hash 
     */
    void writeUnlock(uint64_t hash) {
        LockTableEntry &tableEntry = lockTable[hash % SIZE];
        std::unique_lock<std::mutex> l(tableEntry.mtx);
        tableEntry.writers.erase(hash);
        //tableEntry.writers--;
        //LogInfo("Releasing write lock for " << hash << " (" << hash % SIZE << ")");
        //l.unlock();
        tableEntry.write_cv.notify_all();
        tableEntry.read_cv.notify_all();
    }

    void writeUnlock(std::vector<uint64_t> hashes) {
        std::unordered_set<uint64_t> unique_hashes;
        for (auto hash : hashes) {
            unique_hashes.insert(hash % SIZE);
        }

        for (auto hash : unique_hashes) {
            writeUnlock(hash);
        }
    }

private:
    std::array<LockTableEntry, SIZE> lockTable;
};