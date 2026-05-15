#pragma once
#include <vector>
#include <string>
#include <openvino/openvino.hpp>
// No tokenizer header needed! The magic happens at runtime.

class SemanticEngine {
private:
    ov::Core core;

    // Stage 1: Tokenizer (string -> token IDs)
    ov::CompiledModel compiled_tokenizer;
    ov::InferRequest tokenizer_request;

    // Stage 2: Transformer (token IDs -> embeddings)
    ov::CompiledModel compiled_model;
    ov::InferRequest model_request;
    
    const int64_t embedding_dim = 384;
    bool initialized = false;

    // Mean pooling helper (collapses sequence dimension using attention mask)
    std::vector<float> meanPooling(const float* hidden_states, const int64_t* attention_mask,
                                   int seq_len, int hidden_size);

public:
    // Expects the DIRECTORY containing both openvino_tokenizer.xml and openvino_model.xml
    SemanticEngine(const std::string& model_dir);

    bool isReady() const { return initialized; }
    std::vector<float> getEmbedding(const std::string& text);
    float calculateSimilarity(const std::vector<float>& vecA, const std::vector<float>& vecB);
};