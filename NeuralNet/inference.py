import os, sys, tqdm, math, pickle
import numpy as np
import tensorflow as tf

CM_PATH = './HOME/DATASETs/charmap.pickle'

def prepareXi(X, CM, MAX_LEN):
    CMm = max(CM.values()) + 1

    f = lambda x: CM[x] if x in CM else CMm
    Xi = np.zeros((len(X), MAX_LEN), np.uint8)    
    for i, x in enumerate(X):
        xi = list(map(f, x))
        Xi[i, :len(xi)] = xi            
    return Xi

def loadModel(path):
    gpus = tf.config.experimental.list_physical_devices('GPU') 
    tf.config.experimental.set_memory_growth(gpus[0], True)
    return tf.keras.models.load_model(path, compile=False)

def inference(model, X, batch_size, MAX_LEN=16, CM_path=CM_PATH):
    
    with open(CM_path, 'rb') as f:
        CM = pickle.load(f)
    vocab_size = max(CM.values()) + 1
    
    Xi = prepareXi(X, CM, MAX_LEN)
    
    nbatch = math.ceil(len(Xi) / batch_size)
    
    for i in tqdm.trange(nbatch):
        xi = Xi[batch_size*i:batch_size*(i+1)]
        out = model(xi, training=False)[1].numpy()

        yield out
