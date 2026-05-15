#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <algorithm>
#include <windows.h>
#include <unordered_map>
#include "../webview.h"

// Include your two engines
#include "engine/searcher.h"
#include "engine/semantic_engine.h"
#include "os/indexer.h"

// --- HYBRID RANKING CONSTANTS ---
const float WEIGHT_SEMANTIC = 0.7f; // Importance of AI context
const float WEIGHT_KEYWORD = 0.3f;  // Importance of exact character matching

struct IndexedFile
{
    std::string path; // Full path for display
    std::string name; // Filename only, used for AI embedding
    std::vector<float> vector;
};

// Helper to escape backslashes and quotes for JSON
std::string escape_json(const std::string &s)
{
    std::string escaped;
    for (char c : s)
    {
        if (c == '\\')
            escaped += "\\\\";
        else if (c == '"')
            escaped += "\\\"";
        else if (c == '\n')
            escaped += "\\n";
        else if (c == '\r')
            escaped += "\\r";
        else
            escaped += c;
    }
    return escaped;
}

struct SearchItem
{
    std::string title;
    std::string type;
};

struct RankedResult
{
    std::string path;
    float hybridScore;
    bool keywordMatch;

    // Overload the < operator to sort highest scores to the top
    bool operator<(const RankedResult &other) const
    {
        return hybridScore > other.hybridScore;
    }
};

void init(DataOrientedTrie &engine, SemanticEngine &aiEngine, std::vector<IndexedFile> &semanticDatabase)
{
    std::cout << "--- SEARCH ENGINE INITIALIZATION ---\n";

    // 1. Initialize Engines

    std::cout << "[System] Booting OpenVINO Semantic Engine...\n";

    if (!aiEngine.isReady())
    {
        std::cerr << "[Warning] Semantic AI Engine failed to load. Running in keyword-only mode.\n";
    }

    // 2. Crawl the directory (Feeds the Trie)
    std::wstring searchTarget = L"C:\\Users\\Avik's Laptop\\Downloads";
    std::cout << "Crawling: " << std::string(searchTarget.begin(), searchTarget.end()) << " ...\n";
    crawl_directory(searchTarget, engine);

    // 3. Serialize the Trie memory arrays to SSD
    engine.saveToDisk("search_index.bin");

    // 4. Generate AI Embeddings for the Semantic Graph
    // First, count how many files we need to vectorize
    int totalFiles = 0;
    for (const auto &node : engine.nodeArena)
    {
        if (node.isEndOfFile && node.filePathIndex != -1)
            totalFiles++;
    }
    std::cout << "Generating AI Vector Graph for " << totalFiles << " indexed files...\n";

    // We efficiently loop through the Trie's contiguous memory to find all valid file paths
    int processed = 0;
    for (const auto &node : engine.nodeArena)
    {
        if (node.isEndOfFile && node.filePathIndex != -1)
        {
            std::string filePath = engine.getFilePath(node);

            // Extract just the filename for AI embedding (strip the path noise)
            std::string fileName = filePath;
            size_t lastSlash = filePath.find_last_of("\\/");
            if (lastSlash != std::string::npos)
            {
                fileName = filePath.substr(lastSlash + 1);
            }

            // Push the FILENAME (not full path) into the AI model
            std::vector<float> embedding = aiEngine.getEmbedding(fileName);
            semanticDatabase.push_back({filePath, fileName, embedding});

            processed++;
            if (processed % 50 == 0 || processed == totalFiles)
            {
                std::cout << "\r  [Vectorizing] " << processed << " / " << totalFiles << " files..." << std::flush;
            }
        }
    }

    std::cout << "\nIndex built. " << semanticDatabase.size() << " files vectorized. Ready for queries.\n";
    std::cout << "------------------------------------\n";
}
int WINAPI WinMain(HINSTANCE /*hInst*/, HINSTANCE /*hPrevInst*/, LPSTR /*lpCmdLine*/, int /*nCmdShow*/)
{
    DataOrientedTrie engine;
    SemanticEngine aiEngine("openvino_model");
    std::vector<IndexedFile> semanticDatabase;
    init(engine, aiEngine, semanticDatabase);
    int allowedMistakes = 2;

    // Initialize the window
    webview::webview w(true, nullptr);
    w.set_title("Search Interface Engine");
    w.set_size(600, 450, WEBVIEW_HINT_NONE);

    HWND hwnd = (HWND) w.window().value();

    // 2. Strip the title bar, borders, and X button entirely
    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    style &= ~(WS_CAPTION | WS_THICKFRAME | WS_SYSMENU);
    SetWindowLongPtr(hwnd, GWL_STYLE, style | WS_POPUP);

    // 3. Force the window to float "Always on Top" of other apps
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);

    // 3. The Backend Logic Bridge
    w.bind("SearchBackend", [&](std::string req) -> std::string
           {
               // req arrives as a JSON array string like: ["query"]
               std::string query = "";
               if (req.length() > 4)
               {
                   query = req.substr(2, req.length() - 4);
               }

               // Convert query to lowercase for case-insensitive matching
               std::transform(query.begin(), query.end(), query.begin(), ::tolower);

               if (query == "exit" || query == "quit")
               {
                   std::cout << "Shutting down engine...\n";
                   w.terminate();
                   return "[]";
               }

               if (query.empty())
                   return "[]";
               std::vector<std::pair<int, std::string>> keywordHits = engine.search(query, allowedMistakes);

               std::unordered_map<std::string, int> fastKeywordMap;
               for (const auto &hit : keywordHits)
               {
                   fastKeywordMap[hit.second] = hit.first; // Store the remaining budget
               }

               // aiEngine
               std::vector<float> queryVector = aiEngine.getEmbedding(query);
               std::vector<RankedResult> finalResults;
               finalResults.reserve(semanticDatabase.size());

               for (const auto &file : semanticDatabase)
               {
                   // 1. Calculate Vector Distance (Hardware Accelerated SIMD)
                   float semScore = aiEngine.calculateSimilarity(queryVector, file.vector);

                   // 2. Check Trie Match
                   bool keyMatch = fastKeywordMap.count(file.path) > 0;

                   // Give a boost based on how perfect the spelling was (remaining budget)
                   float keyScore = 0.0f;
                   if (keyMatch)
                   {
                       int budgetRemaining = fastKeywordMap[file.path];
                       // 1.0 base score for matching + bonus for fewer typos
                       keyScore = 1.0f + ((float)budgetRemaining / allowedMistakes);
                   }

                   // 3. Alpha-Blend the Scores
                   float finalScore = (WEIGHT_SEMANTIC * semScore) + (WEIGHT_KEYWORD * keyScore);

                   finalResults.push_back({file.path, finalScore, keyMatch});
               }

               std::sort(finalResults.begin(), finalResults.end());
               const float SCORE_THRESHOLD = 0.05f;
               std::vector<RankedResult> filteredResults;
               for (const auto &res : finalResults)
               {
                   if (res.hybridScore > SCORE_THRESHOLD)
                   {
                       filteredResults.push_back(res);
                   }
               }

               std::string jsonResult = "[";
               bool first = true;

               if (filteredResults.empty())
               {
                   jsonResult += "{\"title\": \"No result Found\",\"type\":\"None\"}";
                   first = false;
               }
               else
               {
                   int limit = std::min((int)filteredResults.size(), 20);
                   for (int i = 0; i < limit; i++)
                   {
                       const auto &res = filteredResults[i];
                       std::string tag = res.keywordMatch ? "[Trie+AI]  " : "[Semantic] ";

                       if (!first)
                           jsonResult += ",";
                       jsonResult += "{\"title\": \"" + escape_json(res.path) + "\", \"type\": \"" + tag + "\"}";
                       first = false;
                   }
               }

               jsonResult += "]";

               return jsonResult; // Return the filtered JSON string back to the UI
           });

    // 4. The Frontend Payload
    w.set_html(R"html(
        <!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <style>
        :root {
            --bg-main: #1a1a1c;
            --bg-input: transparent;
            --border-color: #3e3e42;
            --text-primary: #ffffff;
            --text-muted: #8b949e;
            --accent-color: #58a6ff;
            --active-bg: rgba(88, 166, 255, 0.15);
        }

        * { 
            box-sizing: border-box; 
            margin: 0; 
            padding: 0; 
        }

        /* The body acts as the actual application window now */
        body {
            background-color: var(--bg-main);
            color: var(--text-primary);
            font-family: 'Segoe UI', -apple-system, BlinkMacSystemFont, Roboto, sans-serif;
            height: 100vh;
            overflow: hidden;
            /* The fake window border to replace the Win32 one we deleted */
            border: 1px solid var(--border-color); 
            display: flex;
            flex-direction: column;
        }

        .search-container {
            width: 100%;
            height: 100%;
            display: flex;
            flex-direction: column;
            padding: 12px;
        }

        #search-input {
            width: 100%;
            background-color: var(--bg-input);
            color: var(--text-primary);
            border: none;
            border-bottom: 1px solid rgba(255, 255, 255, 0.1);
            padding: 12px 16px;
            font-size: 26px;
            font-weight: 300;
            outline: none;
            transition: border-color 0.3s ease;
            flex-shrink: 0;
        }

        #search-input::placeholder { color: #666; }
        #search-input:focus { border-bottom-color: var(--accent-color); }

        #results-list {
            list-style: none;
            overflow-y: auto;
            margin-top: 12px;
            padding-right: 4px;
            flex-grow: 1;
        }

        .result-item {
            padding: 14px 18px;
            margin-bottom: 4px;
            border-radius: 8px;
            cursor: pointer;
            display: flex;
            justify-content: space-between;
            align-items: center;
            font-size: 15px;
            transition: all 0.1s ease;
            /* Entrance animation */
            opacity: 0;
            transform: translateY(-5px);
            animation: slideIn 0.2s ease forwards;
        }

        @keyframes slideIn {
            to { opacity: 1; transform: translateY(0); }
        }

        /* Keyboard selected and hover states */
        .result-item:hover, .result-item.active {
            background-color: var(--active-bg);
            border-left: 3px solid var(--accent-color);
            padding-left: 15px; /* Offset the border width so text doesn't jump */
        }

        .item-title { font-weight: 500; letter-spacing: 0.3px; }

        .item-type {
            font-size: 11px;
            text-transform: uppercase;
            letter-spacing: 0.5px;
            color: var(--text-muted);
            background-color: rgba(0,0,0,0.4);
            padding: 4px 10px;
            border-radius: 12px;
            border: 1px solid rgba(255, 255, 255, 0.05);
        }

        /* Invisible sleek scrollbar */
        ::-webkit-scrollbar { width: 6px; }
        ::-webkit-scrollbar-track { background: transparent; }
        ::-webkit-scrollbar-thumb { background: rgba(255,255,255,0.1); border-radius: 10px; }
        ::-webkit-scrollbar-thumb:hover { background: rgba(255,255,255,0.2); }
    </style>
</head>
<body>
    <div class="search-container">
        <input type="text" id="search-input" placeholder="Search system tools..." autofocus autocomplete="off" spellcheck="false">
        <ul id="results-list"></ul>
    </div>

    <script>
        const input = document.getElementById('search-input');
        const resultsList = document.getElementById('results-list');
        let selectedIndex = 0; // Tracks the current active item for keyboard navigation

        // 1. Render UI based on data from C++
        function renderResults(resultsArray) {
            resultsList.innerHTML = ''; 
            selectedIndex = 0; 
            
            resultsArray.forEach((item, index) => {
                const li = document.createElement('li');
                li.className = 'result-item';
                if (index === 0) li.classList.add('active'); 

                // Stagger the entrance animation
                li.style.animationDelay = `${index * 0.02}s`;

                li.innerHTML = `
                    <span class="item-title">${item.title}</span>
                    <span class="item-type">${item.type}</span>
                `;

                // Handle mouse clicking
                li.addEventListener('click', () => {
                    input.value = item.title;
                    resultsList.innerHTML = '';
                    // TODO: Send execute command back to C++ here
                });

                // Sync mouse hover with keyboard selection
                li.addEventListener('mouseenter', () => {
                    document.querySelectorAll('.result-item').forEach(el => el.classList.remove('active'));
                    li.classList.add('active');
                    selectedIndex = index;
                });

                resultsList.appendChild(li);
            });
        }

        // 2. Keyboard Navigation (Up, Down, Enter, Escape)
        input.addEventListener('keydown', (e) => {
            const items = document.querySelectorAll('.result-item');
            
            // Allow closing the launcher with Escape
            if (e.key === 'Escape') {
                // You can add w.bind("CloseLauncher", ...) in C++ to kill the app!
                // window.CloseLauncher("");
                input.value = "";
                resultsList.innerHTML = "";
                return;
            }

            if (items.length === 0) return;

            if (e.key === 'ArrowDown') {
                e.preventDefault(); 
                items[selectedIndex].classList.remove('active');
                selectedIndex = (selectedIndex + 1) % items.length;
                items[selectedIndex].classList.add('active');
                items[selectedIndex].scrollIntoView({ block: 'nearest' });
            } 
            else if (e.key === 'ArrowUp') {
                e.preventDefault();
                items[selectedIndex].classList.remove('active');
                selectedIndex = (selectedIndex - 1 + items.length) % items.length;
                items[selectedIndex].classList.add('active');
                items[selectedIndex].scrollIntoView({ block: 'nearest' });
            }
            else if (e.key === 'Enter') {
                e.preventDefault();
                input.value = items[selectedIndex].querySelector('.item-title').innerText;
                resultsList.innerHTML = '';
                // TODO: Send execute command back to C++ here
            }
        });

        // 3. Talk to C++ when user types
        input.addEventListener('input', async (e) => {
            let query = e.target.value;
            // The C++ bridge automatically parses the JSON for us
            let dataArray = await window.SearchBackend(query);
            renderResults(dataArray);
        });

        // 4. Initial load ping
        window.onload = async () => {
            input.focus();
            let initialData = await window.SearchBackend("");
            renderResults(initialData);
        };
    </script>
</body>
</html>
    )html");

    // 5. Start the engine loop
    w.run();
    return 0;
}