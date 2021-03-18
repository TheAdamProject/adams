EVAL_SUBDIR = 'eval'
model_dir_home = './MODELS/'
SAVED_MODELS = './MODELS/SAVED_MODELS'
MAX_LEN = 16


import tensorflow as tf
from tensorflow import keras as k
import numpy as np
import gin
import tqdm
import sys, os, shutil
import myPickle, myOS
from trainer import Trainer
from input_pipeline_raw import makeIterInput
from model import make_train_predict

gpus = tf.config.experimental.list_physical_devices('GPU') 

ESTIMATE_ALPHA_ITER = 100
ESTIMATE_ALPHA_BS = 2048

def estimate_alpha(estimate_alpha_batch):
    alpha = 0
    i = 0
    print("ESTIMATING alpha.....")
    for _, y in tqdm.tqdm(estimate_alpha_batch.take(ESTIMATE_ALPHA_ITER)):
        y = tf.cast(y, tf.float32)
        local = tf.reduce_mean(y).numpy()
        alpha += local
        i += 1
    alpha = alpha / i
    return alpha

def saveModel(conf_name, f):
    name = f'{conf_name}.h5'
    output = os.path.join(SAVED_MODELS, name)

    f.save(output, save_format='h5', include_optimizer=True)

## Run ###################################################################################

@gin.configurable
def setup(
    model_path,
    train_home,
    test_homes,
    MIN_LEN,
    MAX_LEN,
    epochs,
    test_num_steps,
    test_batch_size,
    log_freq,
    test_for_epoch,
    BUFFER_SIZE,
    hparams):
    
    log_train = model_path
    log_test = os.path.join(model_path, EVAL_SUBDIR)
        
    print(log_train, log_test)
    print(hparams)
        
    batch_size = hparams['batch_size']
    
    # estimated alpha from train-set
    estimate_alpha_batch, _, _ =  makeIterInput(train_home, ESTIMATE_ALPHA_BS, MAX_LEN=MAX_LEN, buffer_size=1, for_prediction=False)
    alpha = estimate_alpha(estimate_alpha_batch)
    hparams['alpha'] = alpha
    print("\nEstimated: ", hparams['alpha'])
    ######################################################################
    
    
    train_batch, CM, CLASS_NUM =  makeIterInput(train_home, batch_size, MAX_LEN=MAX_LEN, buffer_size=BUFFER_SIZE)
    
    # load tests
    test_batchs = []
    for i, test_home in enumerate(test_homes):
        name = "validation%d" % i
        print("LOAD, ", name)
        test_batch, _, _ =  makeIterInput(test_home, test_batch_size, MAX_LEN=MAX_LEN, buffer_size=int(BUFFER_SIZE/(2 * len(test_homes))))
        test_batchs.append((name, test_batch))
    Ycount = None
    
    DICT_SIZE = np.array([i for i in CM.values()])[-1] + 2
    
    print('\n\n', train_home, '\n', test_homes, '\n')
        
    w_class = None
        
    lr = hparams['learning_rate']

    if 'LR_SCHED' in hparams:
        LR_SCHED = hparams['LR_SCHED']
        print("LR_SCHED",  LR_SCHED, lr)
        if LR_SCHED['TYPE'] == 0:
            lr = tf.keras.optimizers.schedules.ExponentialDecay(
                lr,
                LR_SCHED['STEP'],
                LR_SCHED['RATE'],
            )
        else:
            assert False
                
    optimizer = tf.keras.optimizers.Adam(lr)
    f, train_step, predict_step = make_train_predict(hparams, optimizer, w_class, DICT_SIZE, CLASS_NUM, MAX_LEN)
    
    model_mem_footprint = (f.count_params() * 4) // (10 ** 6)
    print("\t MODEL_MEM: %dMB" % model_mem_footprint)
    
    T = Trainer(
        f,
        train_step,
        predict_step,
        epochs,
        train_batch,
        test_batchs,
        optimizer,
        log_train,
        log_test,
        test_num_steps,
        log_freq,
        hparams)
    
    T()
    
    return f, T
    
if __name__ == '__main__':
    
    try:
        gin_conf = sys.argv[1]
    except:
        print("USAGE: CONF")
        sys.exit(1)
        
    gin.parse_config_file(gin_conf)
    
    conf_name = myOS.basenameNoExt(gin_conf, sep='.')  
    
    myOS.mkdir(model_dir_home)
    myOS.mkdir(SAVED_MODELS)
    
    model_path = os.path.join(model_dir_home, 'CHECKPOINTS')
    myOS.mkdir(model_path)
    model_path = os.path.join(model_path, conf_name)
    myOS.mkdir(model_path)
    print(model_path)
        
    # TRAIN
    f, T = setup(model_path)
    checkpoint = tf.train.Checkpoint(model=f)
    checkpoint.restore(T.ckpt_manager.latest_checkpoint)
    print ('Latest checkpoint restored!! Saving...')
    saveModel(conf_name, f)
    
    
                
