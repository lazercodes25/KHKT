from flask import Flask, request, jsonify
import os
import google.generativeai as genai
import json
import re
# ==============================
# ⚙️ Cấu hình
# ==============================
# genai.configure(api_key="AIzaSyCTH0SHERh58RqH29PWFTvOAWiHYYAQoCg")
genai.configure(api_key="AIzaSyCTH0SHERh58RqH29PWFTvOAWiHYYAQoCg")
model = genai.GenerativeModel("gemini-2.5-flash")
UPLOAD_FOLDER = "media"
os.makedirs(UPLOAD_FOLDER, exist_ok=True)
PROMPT = """
Analyze this audio and respond in Vietnamese ONLY.

Tasks:
1. Transcribe spoken words to Vietnamese text (if any).
2. Detect the following:
   - Loud or abnormally high volume
   - Sudden impact / collision sounds
   - Screaming or shouting
   - Violence expressed through spoken words
Response Format:
No text before or after JSON.
Do NOT include explanations, comments, or markdown.
Return STRICT JSON ONLY.

JSON schema:
{
  "text": "<Vietnamese transcription or empty string>",
  "loud": 0 or 1,
  "impact": 0 or 1,
  "scream": 0 or 1,
  "violence_words": 0 or 1
}
"""
def extract_json(text):
    match = re.search(r"\{[\s\S]*\}", text)
    if not match:
        return None
    return match.group(0)

# ==============================
# 🚀 Flask App
# ==============================
app = Flask(__name__)

@app.route("/upload", methods=["POST"])
def upload_audio():
    if "file" not in request.files:
        return jsonify({"error": "No file uploaded"}), 400

    file = request.files["file"]
    if file.filename == "":
        return jsonify({"error": "No filename"}), 400

    # Lưu file
    file_path = os.path.join(UPLOAD_FOLDER, file.filename)
    file.save(file_path)

    # MIME type
    ext = os.path.splitext(file.filename)[1].lower()
    mime = "audio/wav" if ext == ".wav" else "audio/mp3"

    audio_data = {
        "mime_type": mime,
        "data": open(file_path, "rb").read()
    }

    try:
        response = model.generate_content([
            audio_data,
            PROMPT
        ])
        raw = response.text.strip()
        json_str = extract_json(raw)
        print("AI Response:", json_str)
        try:
            data = json.loads(json_str)
        except json.JSONDecodeError:
            return jsonify({"error": "Invalid AI JSON"}), 500
        
        return jsonify(data)
        
    except Exception as e:
        return jsonify({"error": str(e)}), 500

# ==============================
# 🖥️ Chạy server
# ==============================
if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)