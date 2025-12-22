import requests

url = "http://127.0.0.1:5000/upload"
file_path = "baoluc.mp3"   # hoặc test.mp3

with open(file_path, "rb") as f:
    files = {
        "file": (file_path, f, "audio/wav")
    }
    response = requests.post(url, files=files)

print("Status code:", response.status_code)
print("Response:", response.text)
