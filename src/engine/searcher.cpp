#include "searcher.h"
#include <cstdio>
#include <iostream>

DataOrientedTrie::DataOrientedTrie() {
    // Initialize the root node
    nodeArena.push_back({0, 0, false, -1});
}

void DataOrientedTrie::insertPointerNode(const std::string& key, const std::string& fullPath) {
    int currentNodeIndex = 0;
    
    for (char c : key) {
        int nextNodeIndex = -1;
        
        // Find existing edge
        for (int i = 0; i < nodeArena[currentNodeIndex].edgeCount; i++) {
            if (edgeArena[nodeArena[currentNodeIndex].firstEdgeIndex + i].letter == c) {
                nextNodeIndex = edgeArena[nodeArena[currentNodeIndex].firstEdgeIndex + i].targetNodeIndex;
                break;
            }
        }
        
        if (nextNodeIndex == -1) {
            // Create new node
            nextNodeIndex = nodeArena.size();
            nodeArena.push_back({0, 0, false, -1});
            
            // Relocate edges to the end of edgeArena to keep them contiguous
            int oldEdgeCount = nodeArena[currentNodeIndex].edgeCount;
            int oldFirstEdge = nodeArena[currentNodeIndex].firstEdgeIndex;
            int newFirstEdge = edgeArena.size();
            
            for (int i = 0; i < oldEdgeCount; i++) {
                edgeArena.push_back(edgeArena[oldFirstEdge + i]);
            }
            // Add the new edge
            edgeArena.push_back({c, nextNodeIndex});
            
            nodeArena[currentNodeIndex].firstEdgeIndex = newFirstEdge;
            nodeArena[currentNodeIndex].edgeCount++;
        }
        
        currentNodeIndex = nextNodeIndex;
    }
    
    nodeArena[currentNodeIndex].isEndOfFile = true;
    nodeArena[currentNodeIndex].filePathIndex = addStringToStringArena(fullPath);
}

int DataOrientedTrie::addStringToStringArena(const std::string& filePath) {
    int startingOffset = stringArena.size();
    for (char c : filePath) {
        stringArena.push_back(c);
    }
    stringArena.push_back('\0'); 
    return startingOffset;
}

std::string DataOrientedTrie::getFilePath(const FlatNode& node) {
    if (!node.isEndOfFile || node.filePathIndex == -1) return "";
    return std::string(&stringArena[node.filePathIndex]);
}

void DataOrientedTrie::saveToDisk(const std::string& filename) {
    FILE* file = fopen(filename.c_str(), "wb");
    if (!file) return;

    size_t nodeCount = nodeArena.size();
    size_t edgeCount = edgeArena.size();
    size_t stringBytes = stringArena.size();
    
    fwrite(&nodeCount, sizeof(size_t), 1, file);
    fwrite(&edgeCount, sizeof(size_t), 1, file);
    fwrite(&stringBytes, sizeof(size_t), 1, file);

    fwrite(nodeArena.data(), sizeof(FlatNode), nodeCount, file);
    fwrite(edgeArena.data(), sizeof(Edge), edgeCount, file);
    fwrite(stringArena.data(), sizeof(char), stringBytes, file);

    fclose(file);
    std::cout << "[Engine] Successfully serialized graph to disk.\n";
}


void DataOrientedTrie::searchRecursive(int nodeIndex, const std::string& query, int queryIndex, int budget) {
    // Base Case 1: Out of budget
    if (budget < 0) return;

    // Memoization: Pack the array index and string index into a 64-bit integer
    uint64_t stateKey = ((uint64_t)nodeIndex << 32) | (uint32_t)queryIndex;
    if (memo.find(stateKey) != memo.end() && memo[stateKey] >= budget) {
        return; // We've been here with equal or better budget. Prune!
    }
    memo[stateKey] = budget;

    // Grab the current state from our contiguous array
    FlatNode& currentNode = nodeArena[nodeIndex];

    // Base Case 2: We reached the end of the query and found a valid file
    if (queryIndex == query.length() && currentNode.isEndOfFile) {
        results.push_back({budget,getFilePath(currentNode)});
    }

    // --- OPERATION 1: DELETION ---
    // User typed an extra character. Move query forward, stay on same node index.
    if (queryIndex < query.length()) {
        searchRecursive(nodeIndex, query, queryIndex + 1, budget - 1);
    }

    // --- EXPLORE CHILDREN (Data-Oriented Loop) ---
    // Instead of chasing pointers, we loop through a contiguous slice of the edge array.
    // This is where the massive performance gain over std::unordered_map happens.
    for (int i = 0; i < currentNode.edgeCount; i++) {
        
        // Find the specific edge in the arena
        Edge& edge = edgeArena[currentNode.firstEdgeIndex + i];

        if (queryIndex < query.length()) {
            if (edge.letter == query[queryIndex]) {
                // PERFECT MATCH
                searchRecursive(edge.targetNodeIndex, query, queryIndex + 1, budget);
            } else {
                // SUBSTITUTION
                searchRecursive(edge.targetNodeIndex, query, queryIndex + 1, budget - 1);
            }
        }

        // --- OPERATION 3: INSERTION ---
        // Missed a key. Move to target node, stay at current query index.
        searchRecursive(edge.targetNodeIndex, query, queryIndex, budget - 1);
    }
}

std::vector<std::pair<int,std::string>> DataOrientedTrie::search(const std::string& query, int maxMistakes) {
    results.clear();
    memo.clear();
    
    // Start the recursion at nodeIndex 0 (the root)
    searchRecursive(0, query, 0, maxMistakes);
    
    return results;
}