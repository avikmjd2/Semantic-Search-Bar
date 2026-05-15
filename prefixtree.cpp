#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>

// 1. The State Machine Node
struct TrieNode {
    int id; // Unique ID for ultra-fast memoization hashing
    std::unordered_map<char, TrieNode*> children;
    bool isEndOfFile = false;
    std::string filePath = "";
    
    // Constructor assigns the ID automatically
    TrieNode(int nodeId) : id(nodeId) {}
};


// 2. The Search Engine Engine
class FuzzySearcher {
private:
    TrieNode* root;
    int nextNodeId = 0;
    
    // The "Memory": Tracks the highest budget we've had at any specific (Node + Index) state
    std::unordered_map<uint64_t, int> memo;
    
    // Stores our final answers
    std::vector<std::string> results;

    // The core recursive logic we built together
    void searchRecursive(TrieNode* node, const std::string& query, int index, int budget) {
        // Base Case 1: Out of mistakes or hit a dead end
        if (budget < 0 || node == nullptr) return;

        // --- THE MEMOIZATION TRICK ---
        // Shift the 32-bit Node ID to the left, and merge it with the 32-bit Index.
        // This creates a single 64-bit key that is incredibly fast to hash.
        uint64_t stateKey = ((uint64_t)node->id << 32) | (uint32_t)index;

        // If we've been to this exact state before with the SAME or MORE budget, prune this branch!
        if (memo.find(stateKey) != memo.end() && memo[stateKey] >= budget) {
            return;
        }
        // Otherwise, update our memory to reflect our current highest budget for this state
        memo[stateKey] = budget;


        // Base Case 2: We found a file!
        if (index == query.length() && node->isEndOfFile) {
            results.push_back(node->filePath);
            // We do NOT return here, because an "Insertion" mistake might reveal a longer file name.
        }

        // --- OPERATION 1: DELETION ---
        // User typed an extra character. Skip query character, stay at same Trie node.
        if (index < query.length()) {
            searchRecursive(node, query, index + 1, budget - 1);
        }

        // Explore all branches
        for (auto const& [edgeChar, nextNode] : node->children) {
            
            if (index < query.length()) {
                if (edgeChar == query[index]) {
                    // PERFECT MATCH: Move forward in both. Costs 0 budget.
                    searchRecursive(nextNode, query, index + 1, budget);
                } else {
                    // SUBSTITUTION: Move forward in both. Costs 1 budget.
                    searchRecursive(nextNode, query, index + 1, budget - 1);
                }
            }
            
            // --- OPERATION 3: INSERTION ---
            // User missed a character. Move forward in Trie, stay at query index. Costs 1 budget.
            searchRecursive(nextNode, query, index, budget - 1);
        }
    }

public:
    // Initialize the root node
    FuzzySearcher() { 
        root = new TrieNode(nextNodeId++); 
    }

    // Helper function to build the Trie (Simulates your background indexer)
    void insertFile(const std::string& filePath) {
        TrieNode* current = root;
        for (char c : filePath) {
            if (current->children.find(c) == current->children.end()) {
                current->children[c] = new TrieNode(nextNodeId++);
            }
            current = current->children[c];
        }
        current->isEndOfFile = true;
        current->filePath = filePath;
    }

    // The public wrapper function you requested!
    std::vector<std::string> search(const std::string& query, int maxMistakes) {
        results.clear(); // Wipe previous results
        memo.clear();    // Wipe memory for the new search
        
        // Kick off the recursion from the root
        searchRecursive(root, query, 0, maxMistakes);
        
        return results;
    }
};

int main() {
    FuzzySearcher engine;

    // 1. Index your files (This happens in the background in your real app)
    engine.insertFile("main_node.cpp");
    engine.insertFile("math_utils.h");
    engine.insertFile("make_new.py");
    engine.insertFile("matrix.c");

    // 2. The user types something messy
    std::string userInput = "mnd";
    int allowedTypos = 2; // Give them a budget of 2 mistakes

    std::cout << "Searching for: " << userInput << "\n\n";

    // 3. Run the search wrapper
    std::vector<std::string> foundFiles = engine.search(userInput, allowedTypos);

    for (const std::string& file : foundFiles) {
        std::cout << "Match found: " << file << "\n";
    }

    return 0;
}