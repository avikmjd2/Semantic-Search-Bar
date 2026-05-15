#include "semantic_engine.h"
#include <iostream>
#include <numeric>
#include <cmath>

SemanticEngine::SemanticEngine(const std::string& model_dir) {
    try {
        // 1. Load the OpenVINO Tokenizers Extension dynamically.
        // This MUST be in the same folder as your compiled .exe. 
        core.add_extension("openvino_tokenizers.dll");

        // 2. Load the Tokenizer model (string -> input_ids, attention_mask)
        std::string tokenizer_path = model_dir + "/openvino_tokenizer.xml";
        std::cout << "[Semantic Engine] Loading tokenizer from: " << tokenizer_path << "\n";
        auto tokenizer_model = core.read_model(tokenizer_path);
        compiled_tokenizer = core.compile_model(tokenizer_model, "CPU");
        tokenizer_request = compiled_tokenizer.create_infer_request();

        // 3. Load the Transformer model (input_ids, attention_mask -> hidden states)
        std::string model_path = model_dir + "/openvino_model.xml";
        std::cout << "[Semantic Engine] Loading transformer from: " << model_path << "\n";
        auto transformer_model = core.read_model(model_path);

        // Attempt NPU first, then fall back to CPU
        std::cout << "[Semantic Engine] Compiling AI model for Intel hardware...\n";
        try {
            compiled_model = core.compile_model(transformer_model, "NPU");
            std::cout << "[Semantic Engine] OpenVINO NPU initialized successfully.\n";
        } catch (...) {
            std::cout << "[Semantic Engine] NPU unavailable. Falling back to CPU execution.\n";
            compiled_model = core.compile_model(transformer_model, "CPU");
        }

        // 4. Create the reusable inference request
        model_request = compiled_model.create_infer_request();
        
        // 5. Print model I/O info for debugging
        std::cout << "[Semantic Engine] Tokenizer outputs:\n";
        for (size_t i = 0; i < compiled_tokenizer.outputs().size(); ++i) {
            auto output = compiled_tokenizer.output(i);
            std::cout << "  [" << i << "] name=\"" << output.get_any_name() 
                      << "\" type=" << output.get_element_type() << "\n";
        }
        std::cout << "[Semantic Engine] Transformer inputs:\n";
        for (size_t i = 0; i < compiled_model.inputs().size(); ++i) {
            auto input = compiled_model.input(i);
            std::cout << "  [" << i << "] name=\"" << input.get_any_name() << "\"\n";
        }
        std::cout << "[Semantic Engine] Transformer outputs:\n";
        for (size_t i = 0; i < compiled_model.outputs().size(); ++i) {
            auto output = compiled_model.output(i);
            std::cout << "  [" << i << "] name=\"" << output.get_any_name()
                      << "\" shape=" << output.get_partial_shape() << "\n";
        }

        initialized = true;

        // 6. Self-test: run a quick embedding to verify the pipeline works
        std::cout << "[Semantic Engine] Running self-test...\n";
        auto testVec = getEmbedding("test document");
        float norm = 0;
        for (float v : testVec) norm += v * v;
        if (norm > 0.1f) {
            std::cout << "[Semantic Engine] Self-test PASSED (norm=" << norm << ")\n";
        } else {
            std::cerr << "[Semantic Engine] Self-test FAILED (norm=" << norm << ") - embeddings are zero!\n";
            initialized = false;
        }

    } catch (const ov::Exception& e) {
        std::cerr << "[Semantic Engine Error] " << e.what() << "\n";
    } catch (const std::exception& e) {
        std::cerr << "[Semantic Engine Error] " << e.what() << "\n";
    }
}

std::vector<float> SemanticEngine::meanPooling(const float* hidden_states, const int64_t* attention_mask,
                                                int seq_len, int hidden_size) {
    // Mean pooling: average the hidden states, weighted by attention mask
    // hidden_states shape: [1, seq_len, hidden_size]
    // attention_mask shape: [1, seq_len]
    std::vector<float> result(hidden_size, 0.0f);
    float mask_sum = 0.0f;

    for (int s = 0; s < seq_len; ++s) {
        float mask_val = static_cast<float>(attention_mask[s]);
        mask_sum += mask_val;
        for (int h = 0; h < hidden_size; ++h) {
            result[h] += hidden_states[s * hidden_size + h] * mask_val;
        }
    }

    // Avoid division by zero
    if (mask_sum > 0.0f) {
        for (int h = 0; h < hidden_size; ++h) {
            result[h] /= mask_sum;
        }
    }

    // L2 normalize the embedding (cosine similarity becomes dot product)
    float norm = 0.0f;
    for (int h = 0; h < hidden_size; ++h) {
        norm += result[h] * result[h];
    }
    norm = std::sqrt(norm);
    if (norm > 0.0f) {
        for (int h = 0; h < hidden_size; ++h) {
            result[h] /= norm;
        }
    }

    return result;
}

std::vector<float> SemanticEngine::getEmbedding(const std::string& text) {
    if (!initialized) return std::vector<float>(embedding_dim, 0.0f);

    try {
        // --- STAGE 1: TOKENIZER ---
        // Feed the raw string into the tokenizer model
        ov::Tensor input_tensor(ov::element::string, ov::Shape{1});
        input_tensor.data<std::string>()[0] = text;

        tokenizer_request.set_input_tensor(input_tensor);
        tokenizer_request.infer();

        // The tokenizer outputs: [0]=input_ids, [1]=token_type_ids, [2]=attention_mask
        ov::Tensor input_ids_tensor = tokenizer_request.get_output_tensor(0);
        ov::Tensor token_type_ids_tensor = tokenizer_request.get_output_tensor(1);
        ov::Tensor attention_mask_tensor = tokenizer_request.get_output_tensor(2);

        // --- STAGE 2: TRANSFORMER ---
        // Feed all three tokenizer outputs directly into the transformer
        model_request.set_tensor("input_ids", input_ids_tensor);
        model_request.set_tensor("attention_mask", attention_mask_tensor);
        model_request.set_tensor("token_type_ids", token_type_ids_tensor);

        model_request.infer();

        // --- STAGE 3: MEAN POOLING ---
        // The transformer outputs the last hidden state: [1, seq_len, 384]
        ov::Tensor output_tensor = model_request.get_output_tensor(0);
        ov::Shape output_shape = output_tensor.get_shape();

        int seq_len = static_cast<int>(output_shape[1]);
        int hidden_size = static_cast<int>(output_shape[2]);

        const float* hidden_states = output_tensor.data<float>();
        const int64_t* attention_mask = attention_mask_tensor.data<int64_t>();

        return meanPooling(hidden_states, attention_mask, seq_len, hidden_size);

    } catch (const std::exception& e) {
        // Print only the first error to avoid flooding the console
        static bool firstError = true;
        if (firstError) {
            std::cerr << "[Semantic Engine] INFERENCE ERROR: " << e.what() << "\n";
            firstError = false;
        }
        return std::vector<float>(embedding_dim, 0.0f);
    }
}

float SemanticEngine::calculateSimilarity(const std::vector<float>& vecA, const std::vector<float>& vecB) {
    float dot = 0.0f;
    
    // The compiler will unroll and vectorize this using SIMD / AVX2 registers.
    for (size_t i = 0; i < embedding_dim; ++i) {
        dot += vecA[i] * vecB[i];
    }
    
    return dot;
}