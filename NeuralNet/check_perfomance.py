N = 500
BS = 4096 
MAX_LEN = 16

import sys, time
import tensorflow as tf
import numpy as np
import tqdm

if __name__ == '__main__':
    try:
        model = sys.argv[1]
    except:
        print("USAGE: MODEL.h5")
        sys.exit(1)

    model = tf.keras.models.load_model(model, compile=False)
    
    X = tf.random.uniform((BS, MAX_LEN), minval=0, maxval=100, dtype=tf.int32)
    
    # warm-up
    model(X, training=False)
    
    start = time.time()
    for i in tqdm.trange(N):
        out = model(X, training=False)[1]
        
    done = time.time()
    elapsed = done - start
    
    n_rules = out.shape[-1]
    tot_cs = N * BS * n_rules
    
    
    tp = tot_cs / elapsed
    print("%d c/s (Compatibility-scores per second)" % tp)
    


    