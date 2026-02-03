from gtts import gTTS
import os
# playsound simple playback (may have platform issues

text = "Vui lòng giữ trật tự, không được cãi nhau"
tts = gTTS(text=text, lang='vi')
filename = "009.mp3"
tts.save(filename)