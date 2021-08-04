#!/usr/bin/python3

# CONSTANT##############
MIN_LEN = 5
MAX_LEN = 16
ENCODING = 'utf-8'
DEFAULT_HOST = '127.0.0.1'
CMPATH_C = './charmap.pickle'
CMPATH_BPE = './BPE/BPEmap.pickle'
CDPATH_BPE = './BPE/codes.txt'


DEFAULT_PORT=56789
DEFAULT_CLIENT=1
########################
import tensorflow as tf
import os, pickle, sys
import numpy as np
import argparse
import socket
import traceback
import tempfile
from ctypes import *
from functools import partial


def prepareXi_char(X, CM, MAX_LEN, vocab_size):
    f = lambda x: CM[x] if x in CM else vocab_size
    Xi = np.zeros((len(X), MAX_LEN), np.int32)    
    for i, x in enumerate(X):
        xi = list(map(f, x))
        Xi[i, :len(xi)] = xi            
    return Xi

def prepareXi_BPE(BPE, X, CM, MAX_LEN, CMm):
    f = lambda x: CM[x] if x in CM else CMm
    Xi = np.zeros((len(X), MAX_LEN), np.uint8)    
    for i, x in enumerate(X):
        x = BPE.segment(x).split(' ')
        xi = list(map(f, x))
        Xi[i, :len(xi)] = xi            
    return Xi


def init(modelpath):
    gpus = tf.config.experimental.list_physical_devices('GPU') 
    if gpus:
        tf.config.experimental.set_memory_growth(gpus[0], True)
    # load model
    model = tf.keras.models.load_model(modelpath, compile=False)
    
    if 'BPE' in modelpath:
        from subword_nmt import apply_bpe

        # load charmap
        with open(CMPATH_BPE, 'rb') as f:
             CM = pickle.load(f)

        codes = open(CDPATH_BPE)
        BPE = apply_bpe.BPE(codes)
        codes.close()
        
        prepare = partial(prepareXi_BPE, BPE)
    else:
        # load charmap
        with open(CMPATH_C, 'rb') as f:
            CM = pickle.load(f)
        prepare = prepareXi_char
            
            
    vocab_size = max(CM.values()) + 1
    
    # warm-up
    model(np.zeros((1, MAX_LEN)))
    
    return model, CM, vocab_size, prepare

def serve_batch(words, prepare, float16=False, floatSingle=True):
    x = prepare(words, CM, MAX_LEN, vocab_size)
    p = model(x, training=False)[1].numpy()
    # cast to float16
    if float16:
        p = p.astype(np.float16)
    return p


if __name__ == '__main__':
    
    parser=argparse.ArgumentParser(description="This daemon waits for a batch of words and returns their probabilitiess")
    parser.add_argument('-model', type=str, required=True, help='tensorflow model dump')
    parser.add_argument('-host', type=str, default=DEFAULT_HOST, help='binding host')
    parser.add_argument('-port', type=int, default=DEFAULT_PORT, help='binding port')
    parser.add_argument('-client', type=int, default=DEFAULT_CLIENT, help='max number of clients')
    args = parser.parse_args()
        
    model, CM, vocab_size, prepare = init(args.model)

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind((args.host,args.port))
        s.listen(args.client)
        conn,addr= s.accept()
        with conn:
            while(True):
                try:
                    dataS=conn.recv(sizeof(c_size_t))
                    if len(dataS) == 0:
                         break
                    datalen=c_uint.from_buffer_copy(dataS)
                    data=conn.recv(datalen.value,socket.MSG_WAITALL)
                    if not data:
                        break
                        
                    try:
                        words=data.decode(ENCODING).split('\t')
                    except:
                        print(data)
                        
                    p = serve_batch(words, prepare)
                    response = conn.sendall(p)
                    
                except Exception as e:
                    traceback.print_exc()
                    print("Exception {} - closing connection".format(e))
                    break
            conn.close()
        s.close()
