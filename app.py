from flask import Flask, request, jsonify
import os
import google.generativeai as genai
from pydub import AudioSegment

# ==============================
# ⚙️ Cấu hình
# ==============================
genai.configure(api_key="AIzaSyAxxLYwVLvPgpLi4oWWzfrWmLamfI3fSEw")
model = genai.GenerativeModel("gemini-2.5-flash")
UPLOAD_FOLDER = "media"
os.makedirs(UPLOAD_FOLDER, exist_ok=True)
PROMPT = "Transcribe this audio to Vietnamese text only. No explanation."

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

        # ✅ Truy cập an toàn phần text
        text = ""
        if hasattr(response, "candidates") and response.candidates:
            parts = response.candidates[0].content.parts
            if parts and hasattr(parts[0], "text"):
                text = parts[0].text.strip().lower()

        print("Transcription:", text)

        # ⚠️ Nếu rỗng hoặc không có tiếng
        if not text:
            return "0"

        # 🔍 Kiểm tra từ khóa bạo lực
        violence_words = ["giết", "đánh", "chém", "bắn", "máu", "chết", "đấm", "cướp", "bạo lực"]
        violence_flag = 1 if any(word in text for word in violence_words) else 0

        return "0" if violence_flag == 0 else "1"
    except Exception as e:
        return jsonify({"error": str(e)}), 500

# ==============================
# 🖥️ Chạy server
# ==============================
if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)
