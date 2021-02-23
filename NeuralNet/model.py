import tensorflow as tf
from tensorflow import keras as k
import os
import architecture
import numpy as np
from utils import predictionFromProbability
import losses

LOSS_SCALE = 100
PAD = 0


def make_model(hparams, DICT_SIZE, CLASS_NUM, MAX_LEN):

    # get arch
    arch_id = hparams['arch']
    arch = architecture.archMap(arch_id)

    x = tf.keras.layers.Input(MAX_LEN, dtype=tf.int32)
    
    if arch_id >= 10:
        print("Transformer!!!")
        mask_pad = tf.cast( tf.logical_not( tf.math.equal(x, PAD) ), tf.float32)
        max_len = x.shape[-1]
        x_pos = tf.keras.layers.Input(max_len, dtype=tf.int32)
        inputs = [x, x_pos]
        logits, pre_fc = arch(x, x_pos, mask_pad, DICT_SIZE, CLASS_NUM, hparams)
    else:
        inputs = x
        logits, pre_fc = arch(x, DICT_SIZE, CLASS_NUM, hparams)
        
    p = tf.nn.sigmoid(logits)
    
    prediction = predictionFromProbability(p)
    model = tf.keras.Model(inputs=inputs, outputs=[logits, p, prediction, pre_fc])

    return model


    
def make_loss_classification(features, logits, pre_fc, labels, w_class, hparams):
    y = labels
    
    if 'focal_gamma' in hparams and 'focal_alpha' in hparams:
        print("Focal loss")
        _y = tf.cast(y, tf.float32)
        _loss = losses.focalSigmoidCrossEntropy(logits, _y, gamma=hparams['focal_gamma'], alpha=hparams['focal_alpha'])
    else:
        _loss = tf.compat.v1.losses.sigmoid_cross_entropy(y, logits, reduction=tf.losses.Reduction.NONE)
    
    if hparams['alpha'] != 1:
        alpha = hparams['alpha']
        zero_mask = tf.cast( tf.equal(y, 0), tf.float32) * alpha
        
        #if 'w_class' in hparams and hparams['w_class']:
        #    print("With w_class")
        #    one_mask = tf.cast(y, tf.float32) * w_class
        #else:
        one_mask = tf.cast(y, tf.float32)
            
        w = zero_mask + one_mask
        _loss = _loss * w

    loss = tf.reduce_sum(_loss, 1)
    loss = tf.reduce_mean(_loss)

    return loss * LOSS_SCALE

def make_train_predict(hparams, optimizer, w_class, DICT_SIZE, CLASS_NUM, MAX_LEN):
    
    f = make_model(hparams, DICT_SIZE, CLASS_NUM, MAX_LEN)
    arch_id = hparams['arch']
    batch_size = hparams['batch_size']
    transformer = arch_id >= 10
    
    x_pos = np.arange(MAX_LEN)
    x_pos = np.tile(x_pos, (batch_size, 1))
    x_pos = tf.convert_to_tensor(x_pos)
    
    print(f.summary())
    
    @tf.function
    def train_step(data):
        features, labels = data
        with tf.GradientTape() as tape:
            # forward
            if transformer:                
                logits, p, prediction, pre_fc = f([features, x_pos], training=True)
            else:
                logits, p, prediction, pre_fc = f(features, training=True)
            
            loss = make_loss_classification(features, logits, pre_fc, labels, w_class, hparams)
            embeddings = [pre_fc]
           
            
        gradients = tape.gradient(loss, f.trainable_variables)
        optimizer.apply_gradients(zip(gradients, f.trainable_variables))

        return loss, p, prediction, embeddings

    @tf.function
    def predict_step(data):
        features = data[0]
        # forward
        if transformer:                
            logits, p, prediction, pre_fc = f([features, x_pos], training=False)
        else:
            logits, p, prediction, pre_fc = f(features, training=False)
        
        return p, prediction
    
    return f, train_step, predict_step