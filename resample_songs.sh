#!/bin/bash  
#insstall ffmpeg and then run this script to convert all audio files in the inputs folder to mono, 40kHz, and apply a highpass filter at 250Hz twice. The converted files will be saved in the converted folder as .wav files.

cd /Users/boot_ssd/Desktop/Stuff/School/Sem_5/EECS_373/Songs/inputs

for f in *; do
    ffmpeg -i "$f" -ac 1 -ar 40000 -af "highpass=f=250,highpass=f=250" "../converted/${f%.*}.wav"
done