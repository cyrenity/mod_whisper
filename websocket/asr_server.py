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

# Load model
model = whisper.load_model("base")


def process_chunk(message):
    if type(message) is str and 'uuid' in message:
        return None, False
    elif type(message) is str and 'grammar' in message:
        return message, False
    elif type(message) is str and 'eof' in message:
        return None, True
    else:
        audio = np.frombuffer(message, np.int16)
        return audio, False    
    
    
async def recognize(websocket):
    global args
    global pool
    full_audio_bytes = np.array([])
    prompt_grammar = ""

    loop = asyncio.get_running_loop()

    logging.info('Connection from %s', websocket.remote_address);

    while True:
        message = await websocket.recv()
        response, stop = await loop.run_in_executor(pool, process_chunk, message)
    
        if type(response) == str:
            print('text response', response)
            if 'grammar' in response:
                grammar = json.loads(response)
                prompt_grammar = grammar['grammar']
            
        if type(response) == np.ndarray:
            full_audio_bytes = np.append(full_audio_bytes, response)
            print('response', response)
            
        
        if stop: 
            full_audio_bytes = whisper.pad_or_trim(full_audio_bytes)

            # make log-Mel spectrogram and move to the same device as the model
            mel = whisper.log_mel_spectrogram(full_audio_bytes.astype(np.float32)*(1/32768.0)).to(model.device)

            # detect the spoken language
            _, probs = model.detect_language(mel)
            print(f"Detected language: {max(probs, key=probs.get)}")

            # decode the audio
            options = whisper.DecodingOptions(language="en", fp16 = False, prompt=prompt_grammar)
            result = whisper.decode(model, mel, options)
            print(f"Result: {result.text}")
            
            await websocket.send(result.text)
            full_audio_bytes = np.array([])
            #break
    


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
    args.port = int(os.environ.get('WHISPER_SERVER_PORT', 2700))
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
