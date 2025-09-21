import os
import json
import tempfile
import subprocess
import sqlite3
from pocketsphinx import Decoder, Jsgf, Config
from gtts import gTTS
from fuzzywuzzy import process
from StudentReceiver import StudentReceiver
import time
import re
from UpdateDic import UpdateDic


def normalize(text):
    return re.sub(r"[^\w\s]", "", text).strip().lower()



#Record the audio using the record_audio from StudentReceiver first
#then use in this function recognize audio to transform to text ->
# -> we have the question ; then search the response for the question in the
#corresponding table; here it gets a little bit tricky; we can
#use to find key words in the question such as "curs/course" and 
#"lab/laborator" to find out if the corresponding question is either
# generala/serie/grupa and query the corresponding table or query all
# tables which would be less efficient but more certain. I will use the
#first option
def getResponseFromDB(receiver,conn,wavfile): 
    cursor = conn.cursor()
    response_text=""
    question_text=""
    question_text=receiver.recognize_audio(wavfile)
    
    print(f"[DEBUG] Recognized text: '{question_text}'")
    if not question_text or question_text.strip() == "":
        print("[DEBUG] No speech recognized")
        return None
    
    if("lab" in question_text or "laborator" in question_text):
        cursor.execute("SELECT intrebare,raspuns FROM group_questions ")
    elif("curs" in question_text):
        cursor.execute("SELECT intrebare,raspuns FROM series_questions ")
    else:
        cursor.execute("SELECT intrebare,raspuns FROM general_questions")
    rows=cursor.fetchall()
    if rows:
        norm_q = normalize(question_text)
        
        # Build normalization map
        norm_map = {normalize(q): (q, a) for q, a in rows}
        
        # Do fuzzy match
        best, score = process.extractOne(norm_q, list(norm_map.keys()))
        print(f"[DEBUG] Best match: '{best}' (score={score})")
        
        if score > 70:
            response_text = norm_map[best][1]
    
        return response_text
        
def speak_response(text):
    """Function to handle TTS with proper error handling"""
    try:
        print(f"[DEBUG] Attempting to speak: '{text}'")
        tts = gTTS(text=text, lang="ro")
        tts.save("response.mp3")
        
        # Check if file was actually created
        if os.path.exists("response.mp3"):
            print("[DEBUG] response.mp3 created successfully")
            result = os.system("mpg123 response.mp3")
            if result != 0:
                print(f"[WARNING] mpg123 returned non-zero exit code: {result}")
            
            # Clean up file
            try:
                os.remove("response.mp3")
                print("[DEBUG] response.mp3 removed successfully")
            except OSError as e:
                print(f"[WARNING] Could not remove response.mp3: {e}")
        else:
            print("[ERROR] response.mp3 was not created by gTTS")
            
    except Exception as e:
        print(f"[ERROR] TTS Error: {e}")
        print("[INFO] Falling back to text output only")

def interactionTTS(MAX_IDLE,receiver,studentName,conn):
    last_interaction=time.time()
    prompted=False
    while True:
        if time.time()-last_interaction > MAX_IDLE:
            speak_response("Sesiunea s-a incheiat din lipsa de activitate")
            break
        print(f"Hello, {studentName}")
        if prompted==False:
            speak_response(f"Buna, {studentName},cu ce te pot ajuta ?")
            prompted=True
        
        # If we reach here, student is recognized and verified
        # Record audio and get response to their question
        print("[INFO] Recording audio for question...")
        wav = receiver.record_audio(duration=5)
        response_text = getResponseFromDB(receiver,conn, wav)
        if response_text and response_text.lower() in ("exit","stop") :
            speak_response("La revedere")
            break
        # Fallback if no response found
        if not response_text or response_text.strip() == "":
            response_text = "Ne pare rău, nu am putut să înțeleg întrebarea. Vă rugăm să încercați din nou."
            time.sleep(3)
        speak_response(response_text)
        
        
        last_interaction=time.time()




# -------------------------------
# Configure PocketSphinx
# -------------------------------



config = Config()
config.set_string("-hmm", "/usr/share/pocketsphinx/model/en-us/en-us")  # ⚠️ replace with Romanian model if available
config.set_string("-dict", "ro.dic")
decoder = Decoder(config)


# Load JSGF grammar
jsgf = Jsgf("questions.jsgf")

# Use the correct public rule from your JSGF
rule = jsgf.get_rule("questions.question")
fsg = jsgf.build_fsg(rule, decoder.get_logmath(), 7.5)

decoder.add_fsg("questions", fsg)
decoder.set_search("questions")
conn=sqlite3.connect("students_db.db")
updateDic=UpdateDic("ro.dic","students_db.db")
updateDic.addPhoneticTranslation(conn)




# -------------------------------
# Main Loop
# -------------------------------

receiver = StudentReceiver(decoder)
print("[INFO] Ready. Speak into the microphone... (Ctrl+C to quit)")

try:
    while True:
        conn = sqlite3.connect("students_db.db")
        cursor = conn.cursor()
        
        # Get student name from face recognition
        studentName = receiver.start_listening()  # Uncomment this for real use
        #studentName = "TestUser"  # Comment this out for real use
        
        print(f"Received student: {studentName}")
        
        # Check if student is recognized
        if studentName == "Unknown" or not studentName or studentName.strip() == "":
            response_text = "Nu te-am putut identifica. Te rog încearcă din nou!"
            speak_response(response_text)
            conn.close()  # Close connection before continuing
            continue  # Continue the loop to try again
        
        # Check if student exists in database
        cursor.execute("SELECT id FROM students WHERE nume = ?", (studentName,))
        verification = cursor.fetchone()
        
        if verification is None:  # Student not in database
            response_text = f"Nu te-am putut identifica în baza de date, {studentName}. Te rog încearcă din nou!"
            speak_response(response_text)
            conn.close()
            continue  # Continue the loop to try again
        else:
            MAX_IDLE=90
            interactionTTS(MAX_IDLE,receiver,studentName,conn)
           
        conn.close()  # Close connection after processing
        
except KeyboardInterrupt:
    print("\n[INFO] Exiting.")
except Exception as e:
    print(f"[ERROR] Unexpected error: {e}")
finally:
    conn.close()

