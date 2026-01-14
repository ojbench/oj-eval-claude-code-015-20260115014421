#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdint>

using namespace std;

const string DATA_FILE = "storage.db";

struct IndexInfo {
    vector<long> offsets;  // File offsets where values for this index are stored
};

class FileStorage {
private:
    fstream dataFile;
    unordered_map<string, IndexInfo> indexMap;

    // Write entry to file and return its offset
    long writeEntry(const string& index, int value) {
        dataFile.seekp(0, ios::end);
        long offset = dataFile.tellp();

        uint32_t indexLen = index.length();
        dataFile.write(reinterpret_cast<const char*>(&indexLen), sizeof(indexLen));
        dataFile.write(index.c_str(), indexLen);
        dataFile.write(reinterpret_cast<const char*>(&value), sizeof(value));

        dataFile.flush();
        return offset;
    }

    // Read entry from file at given offset
    bool readEntry(long offset, string& index, int& value) {
        dataFile.seekg(offset);
        if (!dataFile.good()) return false;

        uint32_t indexLen;
        dataFile.read(reinterpret_cast<char*>(&indexLen), sizeof(indexLen));
        if (!dataFile.good() || indexLen > 256) return false;

        index.resize(indexLen);
        dataFile.read(&index[0], indexLen);
        if (!dataFile.good()) return false;

        dataFile.read(reinterpret_cast<char*>(&value), sizeof(value));
        return dataFile.good();
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

            indexMap[index].offsets.push_back(offset);
        }

        dataFile.clear(); // Clear flags
    }

    // Insert entry
    void insert(const string& index, int value) {
        // Check if (index, value) already exists
        auto it = indexMap.find(index);
        if (it != indexMap.end()) {
            // Check all offsets for this index
            for (long offset : it->second.offsets) {
                string existingIndex;
                int existingValue;
                if (readEntry(offset, existingIndex, existingValue)) {
                    if (existingValue == value) {
                        // Already exists, do nothing
                        return;
                    }
                }
            }
        }

        // Write new entry
        long offset = writeEntry(index, value);
        indexMap[index].offsets.push_back(offset);
    }

    // Delete entry
    void remove(const string& index, int value) {
        auto it = indexMap.find(index);
        if (it == indexMap.end()) {
            return; // Index doesn't exist
        }

        // Find and remove the offset with matching value
        auto& offsets = it->second.offsets;
        for (size_t i = 0; i < offsets.size(); i++) {
            string existingIndex;
            int existingValue;
            if (readEntry(offsets[i], existingIndex, existingValue)) {
                if (existingValue == value) {
                    // Found it, remove from vector
                    offsets.erase(offsets.begin() + i);
                    if (offsets.empty()) {
                        indexMap.erase(it);
                    }
                    return;
                }
            }
        }
    }

    // Find all values for an index
    void find(const string& index) {
        auto it = indexMap.find(index);
        if (it == indexMap.end() || it->second.offsets.empty()) {
            cout << "null" << endl;
            return;
        }

        // Read all values
        vector<int> values;
        for (long offset : it->second.offsets) {
            string existingIndex;
            int value;
            if (readEntry(offset, existingIndex, value)) {
                values.push_back(value);
            }
        }

        // Sort and output
        sort(values.begin(), values.end());
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
