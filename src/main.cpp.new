#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <algorithm>
#include <unordered_map>

// Include your two engines
#include "engine/searcher.h"
#include "engine/semantic_engine.h"
#include "os/indexer.h"

// --- HYBRID RANKING CONSTANTS ---
const float WEIGHT_SEMANTIC = 0.7f; // Importance of AI context
const float WEIGHT_KEYWORD  = 0.3f; // Importance of exact character matching

struct IndexedFile {
    std::string path;     // Full path for display
    std::string name;     // Filename only, used for AI embedding
    std::vector<float> vector;
};

struct RankedResult {
    std::string path;
    float hybridScore;
    bool keywordMatch;

    // Overload the < operator to sort highest scores to the top
    bool operator<(const RankedResult& other) const {
        return hybridScore > other.hybridScore; 
    }
};

int main() {
    std::cout << "--- SEARCH ENGINE INITIALIZATION ---\n";
    
    // 1. Initialize Engines
    DataOrientedTrie engine;
    std::cout << "[System] Booting OpenVINO Semantic Engine...\n";
    SemanticEngine aiEngine("openvino_model");
    if (!aiEngine.isReady()) {
        std::cerr << "[Warning] Semantic AI Engine failed to load. Running in keyword-only mode.\n";
    }
    
    std::vector<IndexedFile> semanticDatabase;

    // 2. Crawl the directory (Feeds the Trie)
    std::wstring searchTarget = L"C:\\Users\\Avik's Laptop\\Downloads"; 
    std::cout << "Crawling: " << std::string(searchTarget.begin(), searchTarget.end()) << " ...\n";
    crawl_directory(searchTarget, engine);

    // 3. Serialize the Trie memory arrays to SSD
    engine.saveToDisk("search_index.bin");

    // 4. Generate AI Embeddings for the Semantic Graph
    // First, count how many files we need to vectorize
    int totalFiles = 0;
    for (const auto& node : engine.nodeArena) {
        if (node.isEndOfFile && node.filePathIndex != -1) totalFiles++;
    }
    std::cout << "Generating AI Vector Graph for " << totalFiles << " indexed files...\n";

    // We efficiently loop through the Trie's contiguous memory to find all valid file paths
    int processed = 0;
    for (const auto& node : engine.nodeArena) {
        if (node.isEndOfFile && node.filePathIndex != -1) {
            std::string filePath = engine.getFilePath(node);
            
            // Extract just the filename for AI embedding (strip the path noise)
            std::string fileName = filePath;
            size_t lastSlash = filePath.find_last_of("\\/");
            if (lastSlash != std::string::npos) {
                fileName = filePath.substr(lastSlash + 1);
            }
            
            // Push the FILENAME (not full path) into the AI model
            std::vector<float> embedding = aiEngine.getEmbedding(fileName);
            semanticDatabase.push_back({filePath, fileName, embedding});

            processed++;
            if (processed % 50 == 0 || processed == totalFiles) {
                std::cout << "\r  [Vectorizing] " << processed << " / " << totalFiles << " files..." << std::flush;
            }
        }
    }

    std::cout << "\nIndex built. " << semanticDatabase.size() << " files vectorized. Ready for queries.\n";
    std::cout << "------------------------------------\n";

    std::string userInput;
    int allowedMistakes = 2; // How forgiving the fuzzy search is

    // 5. The REPL (Read-Eval-Print Loop)
    while (true) {
        std::cout << "\nSearch > ";
        
        // Use getline to allow multi-word searches with spaces
        std::getline(std::cin, userInput);

        // Exit conditions
        if (userInput == "exit" || userInput == "quit") {
            std::cout << "Shutting down engine...\n";
            break; 
        }

        if (userInput.empty()) continue; 

        // Start the high-resolution timer
        auto start_time = std::chrono::high_resolution_clock::now();

        // --- STEP A: FIRE KEYWORD TRIE ---
        // Your existing engine returns pairs of <budget_left, path>
        std::vector<std::pair<int, std::string>> keywordHits = engine.search(userInput, allowedMistakes);
        
        // Hash them for instant lookup during hybrid blending
        std::unordered_map<std::string, int> fastKeywordMap;
        for (const auto& hit : keywordHits) {
            fastKeywordMap[hit.second] = hit.first; // Store the remaining budget
        }

        // --- STEP B: FIRE SEMANTIC AI ---
        std::vector<float> queryVector = aiEngine.getEmbedding(userInput);

        // --- STEP C: HYBRID BLENDING ---
        std::vector<RankedResult> finalResults;
        finalResults.reserve(semanticDatabase.size());

        for (const auto& file : semanticDatabase) {
            // 1. Calculate Vector Distance (Hardware Accelerated SIMD)
            float semScore = aiEngine.calculateSimilarity(queryVector, file.vector);
            
            // 2. Check Trie Match
            bool keyMatch = fastKeywordMap.count(file.path) > 0;
            
            // Give a boost based on how perfect the spelling was (remaining budget)
            float keyScore = 0.0f;
            if (keyMatch) {
                int budgetRemaining = fastKeywordMap[file.path];
                // 1.0 base score for matching + bonus for fewer typos
                keyScore = 1.0f + ((float)budgetRemaining / allowedMistakes);
            }

            // 3. Alpha-Blend the Scores
            float finalScore = (WEIGHT_SEMANTIC * semScore) + (WEIGHT_KEYWORD * keyScore);

            finalResults.push_back({file.path, finalScore, keyMatch});
        }

        // --- STEP D: SORT AND RENDER ---
        std::sort(finalResults.begin(), finalResults.end());

        // Stop the timer
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        // Filter out noise: only keep results above a meaningful threshold
        const float SCORE_THRESHOLD = 0.05f;
        std::vector<RankedResult> filteredResults;
        for (const auto& res : finalResults) {
            if (res.hybridScore > SCORE_THRESHOLD) {
                filteredResults.push_back(res);
            }
        }

        if (filteredResults.empty()) {
            std::cout << " No matches found.\n";
        } else {
            std::cout << " Returned " << filteredResults.size() << " matches in " << duration.count() << " ms:\n";
            
            int limit = std::min((int)filteredResults.size(), 20);
            for (int i = 0; i < limit; ++i) {
                const auto& res = filteredResults[i];
                
                // Add a visual tag showing WHERE the match came from
                std::string tag = res.keywordMatch ? "[Trie+AI]  " : "[Semantic] ";
                
                // Show the score for transparency
                printf("  %s (%.2f) -> %s\n", tag.c_str(), res.hybridScore, res.path.c_str());
            }
            if ((int)filteredResults.size() > 20) {
                std::cout << "  -> (...and " << (filteredResults.size() - 20) << " more)\n";
            }
        }
    }

    return 0;
}