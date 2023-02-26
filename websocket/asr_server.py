#!/usr/bin/env python3

import json
import os
import sys
import asyncio
import pathlib
import websockets
import concurrent.futures
import logging
import time

import wave
import math
import random
import struct

import whisper

model = whisper.load_model("base")



samplerate = 44100


def process_chunk(message):
    print(message[:20], type(message))

    test_file=open('/tmp/raw.bin','ab')

    if type(message) is str:
        test_file.close()
        time.sleep(1)
        return "Final Response", True
    else:
        if type(message) is bytes:
            test_file.write(message)
        return "Partial Response ...", False    
    
    
async def recognize(websocket, path):
    global args
    global pool

    loop = asyncio.get_running_loop()
    rec = None
    phrase_list = None
    sample_rate = args.sample_rate
    show_words = args.show_words
    max_alternatives = args.max_alternatives

    logging.info('Connection from %s', websocket.remote_address);

    while True:
        message = await websocket.recv()
            
        response, stop = await loop.run_in_executor(pool, process_chunk, message)
        print('response', response)
        await websocket.send(response)
        
        if stop: break
    


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

    pool = concurrent.futures.ThreadPoolExecutor((os.cpu_count() or 1))

    async with websockets.serve(recognize, args.interface, args.port):
        await asyncio.Future()


if __name__ == '__main__':
    asyncio.run(start())

