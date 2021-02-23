import tensorflow as tf
import numpy as np
import math

def predictionFromProbability(p, threshold=.5):
    return tf.cast( (p > threshold), tf.int32)

def binarizeLabels(y):
    return tf.cast(y > 0, tf.int32)