# pip install optimum[openvino] openvino-tokenizers sentence-transformers
from optimum.intel import OVModelForFeatureExtraction
from transformers import AutoTokenizer
import openvino as ov
from openvino_tokenizers import convert_tokenizer

model_id = "sentence-transformers/all-MiniLM-L6-v2"

# 1. Load Model and Tokenizer
tokenizer = AutoTokenizer.from_pretrained(model_id)
model = OVModelForFeatureExtraction.from_pretrained(model_id, export=True)

# 2. Convert the tokenizer to OpenVINO operations
ov_tokenizer = convert_tokenizer(tokenizer)

# 3. Save the OpenVINO IR files
model.save_pretrained("./openvino_model")
ov.save_model(ov_tokenizer, "./openvino_model/openvino_tokenizer.xml")

print("Done! You now have the .xml and .bin files for C++.")