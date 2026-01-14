#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdint>

using namespace std;

const string DATA_FILE = "storage.db";

struct IndexInfo {
    vector<pair<int, long>> entries;  // (value, offset) pairs sorted by value
};

class FileStorage {
private:
    fstream dataFile;
    unordered_map<string, IndexInfo> indexMap;

    // Write entry to file and return its offset
    long writeEntry(const string& index, int value) {
        dataFile.seekp(0, ios::end);
        long offset = dataFile.tellp();

        uint8_t deleted = 0;  // Not deleted
        uint32_t indexLen = index.length();
        dataFile.write(reinterpret_cast<const char*>(&deleted), sizeof(deleted));
        dataFile.write(reinterpret_cast<const char*>(&indexLen), sizeof(indexLen));
        dataFile.write(index.c_str(), indexLen);
        dataFile.write(reinterpret_cast<const char*>(&value), sizeof(value));

        dataFile.flush();
        return offset;
    }

    // Read entry from file at given offset, return false if deleted
    bool readEntry(long offset, string& index, int& value) {
        dataFile.seekg(offset);
        if (!dataFile.good()) return false;

        uint8_t deleted;
        dataFile.read(reinterpret_cast<char*>(&deleted), sizeof(deleted));
        if (!dataFile.good() || deleted) return false;  // Skip deleted entries

        uint32_t indexLen;
        dataFile.read(reinterpret_cast<char*>(&indexLen), sizeof(indexLen));
        if (!dataFile.good() || indexLen > 256) return false;

        index.resize(indexLen);
        dataFile.read(&index[0], indexLen);
        if (!dataFile.good()) return false;

        dataFile.read(reinterpret_cast<char*>(&value), sizeof(value));
        return dataFile.good();
    }

    // Mark entry at offset as deleted
    void markDeleted(long offset) {
        dataFile.seekp(offset);  // Seek to beginning of entry
        uint8_t deleted = 1;
        dataFile.write(reinterpret_cast<const char*>(&deleted), sizeof(deleted));
        dataFile.flush();
    }

public:
    FileStorage() {
        // Check if file exists
        ifstream testFile(DATA_FILE);
        bool fileExists = testFile.good();
        testFile.close();

        dataFile.open(DATA_FILE, ios::in | ios::out | ios::binary);

        if (!fileExists) {
            // Create new file
            dataFile.close();
            dataFile.open(DATA_FILE, ios::in | ios::out | ios::binary | ios::trunc);
        } else {
            // Load index from existing file
            rebuildIndex();
        }
    }

    ~FileStorage() {
        dataFile.close();
    }

    // Rebuild index map from file (for persistence)
    void rebuildIndex() {
        indexMap.clear();
        dataFile.seekg(0, ios::beg);

        while (dataFile.good()) {
            long offset = dataFile.tellg();

            uint8_t deleted;
            dataFile.read(reinterpret_cast<char*>(&deleted), sizeof(deleted));
            if (!dataFile.good() || dataFile.eof()) break;

            uint32_t indexLen;
            dataFile.read(reinterpret_cast<char*>(&indexLen), sizeof(indexLen));
            if (!dataFile.good() || dataFile.eof()) break;

            if (indexLen > 256) break; // Invalid entry

            string index;
            index.resize(indexLen);
            dataFile.read(&index[0], indexLen);
            if (!dataFile.good() || dataFile.eof()) break;

            int value;
            dataFile.read(reinterpret_cast<char*>(&value), sizeof(value));
            if (!dataFile.good() || dataFile.eof()) break;

            // Only add non-deleted entries
            if (!deleted) {
                indexMap[index].entries.push_back({value, offset});
            }
        }

        // Sort entries by value for binary search
        for (auto& entry : indexMap) {
            sort(entry.second.entries.begin(), entry.second.entries.end());
        }

        dataFile.clear(); // Clear flags
    }

    // Insert entry
    void insert(const string& index, int value) {
        // Check if (index, value) already exists using binary search
        auto it = indexMap.find(index);
        if (it != indexMap.end()) {
            auto& entries = it->second.entries;
            // Binary search for value
            auto found = lower_bound(entries.begin(), entries.end(), make_pair(value, 0L),
                [](const pair<int, long>& a, const pair<int, long>& b) {
                    return a.first < b.first;
                });
            if (found != entries.end() && found->first == value) {
                // Already exists, do nothing
                return;
            }

            // Write new entry
            long offset = writeEntry(index, value);
            // Insert in sorted order
            entries.insert(found, {value, offset});
        } else {
            // New index - write entry first
            long offset = writeEntry(index, value);
            indexMap[index].entries.push_back({value, offset});
        }
    }

    // Delete entry
    void remove(const string& index, int value) {
        auto it = indexMap.find(index);
        if (it == indexMap.end()) {
            return; // Index doesn't exist
        }

        // Find value using binary search
        auto& entries = it->second.entries;
        auto found = lower_bound(entries.begin(), entries.end(), make_pair(value, 0L),
            [](const pair<int, long>& a, const pair<int, long>& b) {
                return a.first < b.first;
            });

        if (found == entries.end() || found->first != value) {
            return; // Value doesn't exist
        }

        // Mark as deleted in file
        markDeleted(found->second);

        // Remove from memory
        entries.erase(found);
        if (entries.empty()) {
            indexMap.erase(it);
        }
    }

    // Find all values for an index
    void find(const string& index) {
        auto it = indexMap.find(index);
        if (it == indexMap.end() || it->second.entries.empty()) {
            cout << "null" << endl;
            return;
        }

        // Values are already sorted, just output them
        const auto& entries = it->second.entries;
        for (size_t i = 0; i < entries.size(); i++) {
            if (i > 0) cout << " ";
            cout << entries[i].first;
        }
        cout << endl;
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    FileStorage storage;

    int n;
    cin >> n;

    for (int i = 0; i < n; i++) {
        string command;
        cin >> command;

        if (command == "insert") {
            string index;
            int value;
            cin >> index >> value;
            storage.insert(index, value);
        } else if (command == "delete") {
            string index;
            int value;
            cin >> index >> value;
            storage.remove(index, value);
        } else if (command == "find") {
            string index;
            cin >> index;
            storage.find(index);
        }
    }

    return 0;
}
