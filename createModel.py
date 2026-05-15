import torch
from sentence_transformers import SentenceTransformer
import torch.onnx

# 1. Load the model
model = SentenceTransformer('all-MiniLM-L6-v2')
model.eval() # Set to evaluation mode

# 2. Create dummy input
# We need to ensure these are on the same device as the model
dummy_input = model.tokenizer("This is a test sentence for DSA", 
                              return_tensors="pt", 
                              padding='max_length', 
                              max_length=128, 
                              truncation=True)

# 3. Export the FULL model pipeline
# We wrap the model to ensure the output is the pooled sentence embedding
class OnnxModel(torch.nn.Module):
    def __init__(self, model):
        super().__init__()
        self.model = model

    def forward(self, input_ids, attention_mask):
        # This executes both the transformer and the pooling layer
        outputs = self.model({'input_ids': input_ids, 'attention_mask': attention_mask})
        return outputs['sentence_embedding']

onnx_model = OnnxModel(model)

print("Exporting full pipeline to ONNX...")
torch.onnx.export(
    onnx_model,
    (dummy_input['input_ids'], dummy_input['attention_mask']),
    "semantic_model.onnx",
    export_params=True,
    opset_version=12,
    do_constant_folding=True,
    input_names=['input_ids', 'attention_mask'],
    output_names=['embeddings'],
    dynamic_axes={
        'input_ids': {0: 'batch_size', 1: 'sequence_length'},
        'attention_mask': {0: 'batch_size', 1: 'sequence_length'},
        'embeddings': {0: 'batch_size'}
    }
)

print("Done! You now have a pooled sentence embedding model.")