#!/bin/bash
ffmpeg -f s16le -ac 2 -ar 48000 -i raw.pcm wav.wav
