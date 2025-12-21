from gtts import gTTS
import os
# playsound simple playback (may have platform issues

text = "Đã gửi yêu cầu kiểm tra vệ sinh"
tts = gTTS(text=text, lang='vi')
filename = "006.mp3"
tts.save(filename)