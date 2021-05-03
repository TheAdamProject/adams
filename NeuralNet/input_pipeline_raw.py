import tensorflow as tf
import numpy as np
import myPickle, os, glob
import math
import h5py
import tensorflow_probability as tfp

CMNAME = '../../charmap.pickle'
buffer_size = 10000
ENCODING = 'ascii'


def readX_strict(path, encoding='iso-8859-1', MIN_LEN=0, MAX_LEN=0):
    X = []
    with open(path, encoding=encoding) as f:
        X = [x.strip() for x in f]
    return X

def readRULE(path):
    with open(path) as f:
        raw = [x.strip() for x in f]
        raw = [x for x in raw if x and x[0] != '#']
    return raw

def prepareXi(X, CM, MAX_LEN, CMm):
    f = lambda x: CM[x] if x in CM else CMm
    Xi = np.zeros((len(X), MAX_LEN), np.uint8)    
    for i, x in enumerate(X):
        xi = list(map(f, x))
        try:
            Xi[i, :len(xi)] = xi       
        except:
            ...
    return Xi

def read_index(path, n):
    tot_byte = 4 * n
    with open(path, 'rb') as f:
        b = f.read(tot_byte)
        index = np.frombuffer(b, dtype=np.uint32)
    return index

getAll = lambda home, s: sorted( glob.glob( os.path.join(home, s) ), key=lambda x: int(x.split('_')[-1]) )

def makeIterInput(home, batch_size, MAX_LEN=16, buffer_size=buffer_size, for_prediction=False):
    
    CMPATH = os.path.join(home, CMNAME)     
    CM = myPickle.load(CMPATH)
    CMm = max(CM.values()) + 1
    DICT_SIZE = len(CM)
    
    rpath = os.path.join(home, 'rules.txt') 
    rules = readRULE(rpath)
    CLASS_NUM = len(rules)
    print("RULES_NUMBER: ", CLASS_NUM)
    
    def G(*args):
        # for each chunk
        for xpath, index_path, hits_path in zip( getAll(home, 'X.txt_*'), getAll(home, 'index_hits*'), getAll(home, 'hits*') ):
            # read passwords chunk
            X = readX_strict(xpath, encoding=ENCODING)
            # parse plaintext password 
            Xi = prepareXi(X, CM, MAX_LEN, CMm)
            
            # read index array
            index = read_index(index_path, len(Xi))

            with open(hits_path, 'rb') as f:
                # for each password in the chunk
                for x, nm in zip(Xi, index):
                    # get hitting rules and create dense label
                    tot_byte = nm * 4
                    b = f.read(tot_byte)
                    rhits = np.frombuffer(b, dtype=np.uint32)
                    y = np.zeros(CLASS_NUM, dtype=np.int32)
                    y[rhits] = 1
                    
                    if len(x) > MAX_LEN:
                        continue
                                        
                    yield x, y
            
    dataset = tf.data.Dataset.from_generator(G, (tf.int32, tf.int32) , ((MAX_LEN,), (CLASS_NUM,)))
        
    if not for_prediction:
        dataset = dataset.shuffle(buffer_size)
        
    dataset = dataset.batch(batch_size, drop_remainder=True)
    dataset = dataset.prefetch(buffer_size=buffer_size)
    
    return dataset, CM, CLASS_NUM
