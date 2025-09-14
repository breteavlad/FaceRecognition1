import os
import stat
import tempfile
import subprocess

class StudentReceiver:
    def __init__(self,decoder):
        self.decoder=decoder
        self.pipe_path = "/tmp/studentName_pipe"
        # Create pipe if it doesn't exist
        if not os.path.exists(self.pipe_path):
            os.mkfifo(self.pipe_path)
    
    def wait_for_student(self):
        #waiting to get student name
        with open(self.pipe_path, 'r') as pipe:
            student_name = pipe.readline().strip()
            print(f"[DEBUG] Raw from pipe: '{student_name}'")
            return student_name
    
    def start_listening(self):
        while True:
            student_name = self.wait_for_student()
            if student_name:
                print(f"Received student: {student_name}")
                return student_name
            return None
    
    # -------------------------------
    # Helper: Record microphone input
    # -------------------------------
    def record_audio(self,duration=5):
        tmpfile = tempfile.NamedTemporaryFile(suffix=".wav", delete=False)
        tmpfile.close()

        cmd_record = (
            f"arecord -D hw:3,0 -f S32_LE -r 48000 -c 2 -d {duration} | "
            f"sox -t wav - -c 1 -r 16000 -b 16 {tmpfile.name} gain 45"
        )

        print(f"[INFO] Recording {duration} seconds...")
        subprocess.run(cmd_record, shell=True, executable="/bin/bash")

        return tmpfile.name



    # -------------------------------
    # Helper: Recognize speech
    # -------------------------------
    def recognize_audio(self,wavfile):
        with open(wavfile, "rb") as f:
            self.decoder.start_utt()
            while True:
                buf = f.read(1024)
                if not buf:
                    break
                self.decoder.process_raw(buf, False, False)
            self.decoder.end_utt()

        hyp = self.decoder.hyp()
        return hyp.hypstr.lower() if hyp else ""
