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
    vector<int> values;  // Sorted values for this index
};

class FileStorage {
private:
    fstream dataFile;
    unordered_map<string, IndexInfo> indexMap;

    // Write entry to file
    void writeEntry(const string& index, int value) {
        dataFile.seekp(0, ios::end);

        uint8_t deleted = 0;  // Not deleted
        uint32_t indexLen = index.length();
        dataFile.write(reinterpret_cast<const char*>(&deleted), sizeof(deleted));
        dataFile.write(reinterpret_cast<const char*>(&indexLen), sizeof(indexLen));
        dataFile.write(index.c_str(), indexLen);
        dataFile.write(reinterpret_cast<const char*>(&value), sizeof(value));

        dataFile.flush();
    }

    // Find and mark entry as deleted by scanning file
    void markDeleted(const string& targetIndex, int targetValue) {
        dataFile.seekg(0, ios::beg);

        while (dataFile.good()) {
            int32_t offset = dataFile.tellg();

            uint8_t deleted;
            dataFile.read(reinterpret_cast<char*>(&deleted), sizeof(deleted));
            if (!dataFile.good() || dataFile.eof()) break;

            uint32_t indexLen;
            dataFile.read(reinterpret_cast<char*>(&indexLen), sizeof(indexLen));
            if (!dataFile.good() || dataFile.eof()) break;

            if (indexLen > 256) break;

            string index;
            index.resize(indexLen);
            dataFile.read(&index[0], indexLen);
            if (!dataFile.good() || dataFile.eof()) break;

            int value;
            dataFile.read(reinterpret_cast<char*>(&value), sizeof(value));
            if (!dataFile.good() || dataFile.eof()) break;

            // Check if this is the entry to delete
            if (!deleted && index == targetIndex && value == targetValue) {
                dataFile.seekp(offset);
                uint8_t del = 1;
                dataFile.write(reinterpret_cast<const char*>(&del), sizeof(del));
                dataFile.flush();
                return;
            }
        }
        dataFile.clear();
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
            int32_t offset = dataFile.tellg();

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
                indexMap[index].values.push_back(value);
            }
        }

        // Sort values for binary search
        for (auto& entry : indexMap) {
            sort(entry.second.values.begin(), entry.second.values.end());
        }

        dataFile.clear(); // Clear flags
    }

    // Insert entry
    void insert(const string& index, int value) {
        // Check if (index, value) already exists using binary search
        auto it = indexMap.find(index);
        if (it != indexMap.end()) {
            auto& values = it->second.values;
            // Binary search for value
            auto found = lower_bound(values.begin(), values.end(), value);
            if (found != values.end() && *found == value) {
                // Already exists, do nothing
                return;
            }
            // Insert in sorted order
            values.insert(found, value);

            // Write new entry to file
            writeEntry(index, value);
        } else {
            // New index
            indexMap[index].values.push_back(value);
            writeEntry(index, value);
        }
    }

    // Delete entry
    void remove(const string& index, int value) {
        auto it = indexMap.find(index);
        if (it == indexMap.end()) {
            return; // Index doesn't exist
        }

        // Find value using binary search
        auto& values = it->second.values;
        auto found = lower_bound(values.begin(), values.end(), value);

        if (found == values.end() || *found != value) {
            return; // Value doesn't exist
        }

        // Mark as deleted in file (this scans the file)
        markDeleted(index, value);

        // Remove from memory
        values.erase(found);
        if (values.empty()) {
            indexMap.erase(it);
        }
    }

    // Find all values for an index
    void find(const string& index) {
        auto it = indexMap.find(index);
        if (it == indexMap.end() || it->second.values.empty()) {
            cout << "null" << endl;
            return;
        }

        // Values are already sorted, just output them
        const auto& values = it->second.values;
        for (size_t i = 0; i < values.size(); i++) {
            if (i > 0) cout << " ";
            cout << values[i];
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
