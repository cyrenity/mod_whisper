#!/usr/bin/env python3

import numpy as np
import os
import sys
import asyncio
import websockets
import concurrent.futures
import logging
import os
import whisper
import json
from scipy.io import wavfile
import torchaudio
from speechbrain.pretrained import Tacotron2
from speechbrain.pretrained import HIFIGAN
import io


# Intialize TTS (tacotron2) and Vocoder (HiFIGAN)
tacotron2 = Tacotron2.from_hparams(source="speechbrain/tts-tacotron2-ljspeech", savedir="tmpdir_tts")
hifi_gan = HIFIGAN.from_hparams(source="speechbrain/tts-hifigan-ljspeech", savedir="tmpdir_vocoder")


def process_chunk(message):
    if type(message) is str:      
        mel_output, mel_length, alignment = tacotron2.encode_text(message)
        waveforms = hifi_gan.decode_batch(mel_output)
        
        audio_bytes = torchaudio.transforms.Resample(22050, 48000)(waveforms.squeeze(1))
        
        # Saving to bytes buffer
        buffer_ = io.BytesIO()
        torchaudio.save(buffer_, audio_bytes, 48000, format="wav", encoding="PCM_S", bits_per_sample=16)
        buffer_.seek(0)
        
        return buffer_.read(), False
    
async def recognize(websocket):
    global args
    global pool


    loop = asyncio.get_running_loop()

    logging.info('Connection from %s', websocket.remote_address);

    while True:
        message = await websocket.recv()
        print(message)
        response, stop = await loop.run_in_executor(pool, process_chunk, message)
        print('sending, response', len(response))
        await websocket.send(response)
    

async def start():

    global args
    global pool

    # Enable loging if needed
    #
    # logger = logging.getLogger('websockets')
    # logger.setLevel(logging.INFO)
    # logger.addHandler(logging.StreamHandler())
    logging.basicConfig(level=logging.INFO)

    args = type('', (), {})()

    args.interface = os.environ.get('WHISPER_SERVER_INTERFACE', '0.0.0.0')
    args.port = int(os.environ.get('WHISPER_SERVER_PORT', 2600))
    args.model_path = os.environ.get('WHISPER_MODEL_PATH', 'model')
    args.spk_model_path = os.environ.get('WHISPER_SPK_MODEL_PATH')
    args.sample_rate = float(os.environ.get('WHISPER_SAMPLE_RATE', 8000))
    args.max_alternatives = int(os.environ.get('WHISPER_ALTERNATIVES', 0))
    args.show_words = bool(os.environ.get('WHISPER_SHOW_WORDS', True))

    if len(sys.argv) > 1:
       args.model_path = sys.argv[1]

    # Gpu part, uncomment if WHISPER-api has gpu support
    #
    # from WHISPER import GpuInit, GpuInstantiate
    # GpuInit()
    # def thread_init():
    #     GpuInstantiate()
    # pool = concurrent.futures.ThreadPoolExecutor(initializer=thread_init)


    pool = concurrent.futures.ThreadPoolExecutor((os.cpu_count() or 1))

    async with websockets.serve(recognize, args.interface, args.port):
        await asyncio.Future()


if __name__ == '__main__':
    asyncio.run(start())
