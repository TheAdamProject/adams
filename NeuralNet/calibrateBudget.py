import os, sys


import tensorflow as tf
import numpy as np

import input_pipeline_raw
import tqdm
import matplotlib.pyplot as plt

batch_size = 4096
BINS = 1000
PLOT = True
target_recalls = np.linspace(.85, .99, 15)
target_betas = np.linspace(.3, .99, 30)


if __name__ == '__main__':
    try:
        model_path = sys.argv[1]
        train_home = sys.argv[2]
    except:
        print("USAGE: model_keras trainingset_path")
        sys.exit(1)
    
    dataset, _, CLASS_NUM = input_pipeline_raw.makeIterInput(train_home, batch_size, for_prediction=False)
    model = tf.keras.models.load_model(model_path)
    
    
    pos = []
    guesses_per_beta = {beta:0 for beta in target_betas}
    tot_guesses = 0
    for x, y in tqdm.tqdm(dataset.take(10)):
        _p = model(x, training=False)[1].numpy()
        y = y.numpy()
        p = _p[y.astype(np.bool)]
        pos.append(p)

        tot_guesses += y.shape[0] * y.shape[1]

        for beta in target_betas:
            b = _p > beta
            guesses_per_beta[beta] += b.sum()

    pos = np.concatenate(pos)


    rec, beta = np.histogram(pos, bins=np.linspace(0, 1, BINS))
    rec = rec / pos.shape[0] 
    

    def getBestBeta(target_recall):
        c = 0.
        for i in range(len(beta)-1)[::-1]:
            c += rec[i] 
            if(c >= target_recall):
                return beta[i] 

    results = []
    for recall in target_recalls:
        bbeta = getBestBeta(recall)
        results.append([recall, bbeta])

    print("Recall\t \tBeta\tCost\n")
    for i, (r, b) in enumerate(results):
        c_beta = target_betas[(target_betas > b).argmax()]
        cost = guesses_per_beta[c_beta] / tot_guesses
        print("[%.2f]\t=\t%.3f\t%.3f (%.3f)" % (r, 1-b, cost, 1-c_beta))

        
    if PLOT:
        fig, ax = plt.subplots(1, 1, figsize=(17, 6))
        ax.plot(beta[1:], rec, c='black');
        top = rec.max()

        for r, b in results:
            y = np.linspace(0, top, 3)
            x = np.tile(b, 3)
            ax.plot(x, y, c='red');

        ax.set(xlim=(0, 1), ylim=(0, top), yticks=[], xticks=np.linspace(0, 1, 50))
        plt.xticks(rotation = 45)
        plt.grid()
        fig.savefig('calibrationBeta.png')