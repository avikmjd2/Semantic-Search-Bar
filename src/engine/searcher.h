#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>


struct Edge 
{
    char letter;
    int targetNodeIndex;
};

struct FlatNode {
    int firstEdgeIndex;
    int edgeCount;
    bool isEndOfFile;
    int filePathIndex;
};

// The Engine Class
class DataOrientedTrie {

private:
    // --- THE SEARCH STATE ---
    std::unordered_map<uint64_t, int> memo;
    std::vector<std::pair<int,std::string>> results;

    // The core recursive logic, updated for array indices
    void searchRecursive(int nodeIndex, const std::string& query, int queryIndex, int budget);

public:
    std::vector<FlatNode> nodeArena;
    std::vector<Edge> edgeArena;
    std::vector<char> stringArena;

    DataOrientedTrie();

    // Builds the tree in RAM (Step 1)
    void insertPointerNode(const std::string& key, const std::string& fullPath);
    
    // Memory and Disk Management
    int addStringToStringArena(const std::string& filePath);
    std::string getFilePath(const FlatNode& node);
    void saveToDisk(const std::string& filename);
    std::vector<std::pair<int,std::string>> search(const std::string& query, int maxMistakes);
};