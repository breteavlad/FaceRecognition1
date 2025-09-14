# 🚀 Project Setup Guide

Follow these steps to set up the environment, TTS system, and networking components.

---

## 1️⃣ Install **FaceRecognizer**

1. Clone the repository:
   
   git clone <repo_url>
Create and enter the build directory:

mkdir build && cd build
Run CMake with Tesseract and Leptonica paths:


cmake .. \
  -DTesseract_DIR=/home/vlad/tesseract-install/lib/cmake/tesseract \
  -DLeptonica_DIR=/home/vlad/leptonica-install/lib/cmake/leptonica
Build with verbose output (to debug errors):


make VERBOSE=1

2️⃣ Set Up TTS Speaker

Create a virtual environment:


python -m venv TTSvenv
Activate the environment:


source TTSvenv/bin/activate
Install required packages (e.g., PocketSphinx):


pip install pocketsphinx
Run the TTS script:


python TTS.py

3️⃣ Configure TCP Server & Client

- Move TCPserver → inside the TTS folder

- Move TCPclient → to the administrator’s laptop

- The TCP client allows the administrator to update the database via a GUI.

4️⃣ Hardware Setup

🔊 Connect a speaker

🎤 Connect a microphone

📦 Both should be plugged into the Raspberry Pi

Additional setups:
- CAD design for the microphone and the raspberry pi
- wiring for