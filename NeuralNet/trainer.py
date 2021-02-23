import tensorflow as tf
from tensorflow import keras as k
import tensorflow_addons as tfa
from tensorboard.plugins.hparams import api as hp
import tqdm

patience = 3
LOG_TFB_INTERNAL_EVAL = False
AUC_THRS_NUM = 200

def flush_metric(iteration, metric, non_scalar=False, log=True):
    name = metric.name
    if non_scalar:
        value = metric.result()[0]
    else:
        value = metric.result()
    if log:
        tf.summary.scalar(name, value, step=iteration)
    metric.reset_states()
    return (name, value)

flatten = lambda x: tf.reshape(x, (-1, 1)) 

def makeNameTest(name, score):
    if name:
        return '%s_%s' % (score, name)
    else:
        return score

class Trainer:
    def __init__(self,
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
                 #test_freq,
                 hparams
                ):
        
        self.f = f
        self.train_step = train_step
        self.predict_step = predict_step
        self.epochs = epochs
        
        self.train_batch = train_batch
        self.test_batchs = test_batchs
        self.optimizer = optimizer
        self.log_freq = log_freq
        
        self.hparams = hparams
        
        self.test_num_steps = test_num_steps
        
        # early stopping
        self.top_score = 0
        self.countdown = patience
        
        self.train_summary_writer = train_summary_writer = tf.summary.create_file_writer(log_train)
        if log_test:
            self.test_summary_writer = test_summary_writer = tf.summary.create_file_writer(log_test)
        
        #check points
        checkpoint = tf.train.Checkpoint(
            optimizer=self.optimizer,
            model=self.f,
        )

        self.ckpt_manager = tf.train.CheckpointManager(checkpoint, log_train, max_to_keep=patience)

        if self.ckpt_manager.latest_checkpoint:
            checkpoint.restore(self.ckpt_manager.latest_checkpoint)
            print ('Latest checkpoint restored!!')


    def train(self, dataset):
    
    
        # create metrics
        loss_m = tf.keras.metrics.Mean(name='loss')
        acc_m = tf.keras.metrics.Accuracy(name='accuracy')
        recal_m = tf.keras.metrics.Recall(name='recall')
        precision_m = tf.keras.metrics.Precision(name='precision')

        for i in range(self.epochs):
            print(f'Epoch {i}')
            for data in tqdm.tqdm(dataset):
                x, y = data
                
                iteration = self.optimizer.iterations
                
                loss, p, prediction, embeddings = self.train_step(data)
                    
                _y = flatten(y)
                _prediction = flatten(prediction)
                _p = flatten(p)

                loss_m.update_state(loss)
                recal_m.update_state(_y, _prediction)
                precision_m.update_state(_y, _prediction)
                acc_m.update_state(_y, _prediction)

                if tf.equal(iteration % self.log_freq, 0):

                    flush_metric(iteration, loss_m)
                    flush_metric(iteration, acc_m)
                    flush_metric(iteration, recal_m)
                    flush_metric(iteration, precision_m)
                    

                    tf.summary.histogram("true", y, buckets=2, step=iteration)
                    tf.summary.histogram("prediction", prediction, buckets=2, step=iteration)

            with self.test_summary_writer.as_default():
                _auc, _recall, _pre = 0, 0, 0
                for name_test_batch, test_batch in self.test_batchs:
                    print("TESTING: %s" % name_test_batch)
                    __auc, __recall, __pre = self.test(test_batch, name_test_batch, i+1, 0)
                    _auc += __auc / len(self.test_batchs)
                    _recall += __recall / len(self.test_batchs)
                    _pre += __pre / len(self.test_batchs)
                
                tf.summary.scalar("avg_pre_plus_recall", _pre + _recall, step=i+1)
                tf.summary.scalar("avg_auc", _auc, step=i+1)
                tf.summary.scalar("avg_recall", _recall, step=i+1)
                tf.summary.scalar("avg_pre", _pre, step=i+1)
                
                if self.early_stopping(_auc):
                    print(f"Early-stop epoch-{i}")
                    break

                
                    
    def test(self, dataset, name, epoch, num_steps=0):
        # create metrics
        #loss_m = tf.keras.metrics.Mean(name='loss')
        #acc_m = tf.keras.metrics.Accuracy(name='accuracy')
        recal_m = tf.keras.metrics.Recall(name=makeNameTest(name, 'recall'))
        precision_m = tf.keras.metrics.Precision(name=makeNameTest(name, 'precision'))
        #f2_m = tfa.metrics.FBetaScore(1, None, 2., .5, name='f2')
        
        auc_m = tf.keras.metrics.AUC(name=makeNameTest(name, 'auc'), curve='PR', num_thresholds=AUC_THRS_NUM)
        #PR_m = tf.keras.metrics.AUC(curve='PR', name='PR')
        
        efficiency_m = tf.keras.metrics.Mean(name=makeNameTest(name, 'efficiency'))

        #if num_steps:
        #    dataset = dataset.take(num_steps)

        print("Testing")
        for data in tqdm.tqdm(dataset.take(self.test_num_steps)):
            x, y = data
                
            p, prediction = self.predict_step(data)

            _y = flatten(y)
            _prediction = flatten(prediction)
            _p = flatten(p)
            
            #loss_m.update_state(loss)
            recal_m.update_state(_y, _prediction)
            precision_m.update_state(_y, _prediction)
            
            # for efficiency reasons; auc just on a half of the output
            #sub_n = int(len(_y.shape) * .5)
            #acc_m.update_state(_y, _prediction)
            #f2_m.update_state(_y, _p)
            
            # number of guesses  
            efficiency_m.update_state( tf.reduce_sum(prediction) / (y.shape[0] * y.shape[1]) )
            
            # slow op
            #sub_n = int(len(_y.shape) * .5)
            # _y[:sub_n], _prediction[:sub_n]
            auc_m.update_state(y, p)
            #PR_m.update_state(y, p)

        iteration = self.optimizer.iterations

        #flush_metric(iteration, loss_m)
            
        metrics = []
        metrics += [flush_metric(epoch, auc_m, log=LOG_TFB_INTERNAL_EVAL)]
        #metrics += [flush_metric(iteration, acc_m)]
        metrics += [flush_metric(iteration, recal_m, log=LOG_TFB_INTERNAL_EVAL)]
        metrics += [flush_metric(iteration, precision_m, log=LOG_TFB_INTERNAL_EVAL)]
        #metrics += [flush_metric(iteration, f2_m, True)]

        
        metrics +=[flush_metric(iteration, efficiency_m, log=LOG_TFB_INTERNAL_EVAL)]

        if LOG_TFB_INTERNAL_EVAL:
            tf.summary.histogram("true", y, buckets=2, step=iteration)
            tf.summary.histogram("prediction", prediction, buckets=2, step=iteration)
        
        for name, value in metrics:
            print(f'{name} : {value.numpy()} -|_ ', sep='')
            print('\n')
        
        return metrics[0][-1].numpy(), metrics[1][-1].numpy(),  metrics[2][-1].numpy()
    
    
    def early_stopping(self, test_score):
        print(test_score)
        if test_score > self.top_score:
            print("New Best", test_score)
            self.top_score = test_score
            self.countdown = patience
            
            # Save checkpoint
            ckpt_save_path = self.ckpt_manager.save()
            print ('Saving checkpoint at {}'.format(ckpt_save_path))
            
            return False
        
        self.countdown -= 1
        if self.countdown == 0:
            print("UTB", test_score, self.top_score)
            return True


    def __call__(self):
        with self.train_summary_writer.as_default():
            self.train(self.train_batch)
            if 'LR_SCHED' in self.hparams:
                self.hparams.pop('LR_SCHED')
            hp.hparams(self.hparams)
            
           
