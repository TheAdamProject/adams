import tensorflow as tf

def archMap(arch_id):
    m = {
        0 : a0,
        1 : a1,
        2 : em0, 
        3 : lstm0,
        4 : em0_latent,
    }
    return m[arch_id]

def ResBlockDeepBNK(inputs, dim, ks=5, with_batch_norm=True, activation='relu'):
    print(activation)
    x = inputs
    
    dim_BNK = dim // 2
    
    if with_batch_norm:
        x = tf.keras.layers.BatchNormalization()(x)
        
    x = tf.keras.layers.Activation(activation)(x)
    x = tf.keras.layers.Conv1D(dim_BNK, 1, padding='same')(x)
    
    if with_batch_norm:
        x = tf.keras.layers.BatchNormalization()(x)
        
    x = tf.keras.layers.Activation(activation)(x)
    x = tf.keras.layers.Conv1D(dim_BNK, ks, padding='same')(x)
    
    if with_batch_norm:
        x = tf.keras.layers.BatchNormalization()(x)
        
    x = tf.keras.layers.Activation(activation)(x)
    x = tf.keras.layers.Conv1D(dim, 1, padding='same')(x)
    
    return inputs + (0.3*x)

def ResBlock(inputs, dim, ks=5, with_batch_norm=True, activation='relu'):
    print(activation)
    x = inputs
    
    if with_batch_norm:
        x = tf.keras.layers.BatchNormalization()(x)
        
    x = tf.keras.layers.Activation(activation)(x)
    x = tf.keras.layers.Conv1D(dim, ks, padding='same')(x)
    
    if with_batch_norm:
        x = tf.keras.layers.BatchNormalization()(x)
        
    x = tf.keras.layers.Activation(activation)(x)
    x = tf.keras.layers.Conv1D(dim, ks, padding='same')(x)
    return inputs + (0.3*x)

##################################################################

def a0(x, DICT_SIZE, CLASS_NUM, hparams, layer_dim=126, batch_norm=True):
    batch_norm = True
    x = tf.one_hot(x, DICT_SIZE)
    x = tf.keras.layers.Conv1D(layer_dim, 5, padding='same')(x)
    x = ResBlockDeepBNK(x, layer_dim, with_batch_norm=batch_norm)
    x = ResBlockDeepBNK(x, layer_dim, with_batch_norm=batch_norm)
    x = ResBlockDeepBNK(x, layer_dim, with_batch_norm=batch_norm)
    x = ResBlockDeepBNK(x, layer_dim, with_batch_norm=batch_norm)
    x = ResBlockDeepBNK(x, layer_dim, with_batch_norm=batch_norm)
    x = tf.keras.layers.Flatten()(x) 
    logits = tf.keras.layers.Dense(CLASS_NUM, name='logits')(x)
        
    return logits


def a1(x, DICT_SIZE, CLASS_NUM, hparams, layer_dim=128, batch_norm=True):
    batch_norm = True
    x = tf.one_hot(x, DICT_SIZE)
    x = tf.keras.layers.Conv1D(layer_dim, 5, padding='same')(x)
    x = ResBlockDeepBNK(x, layer_dim, with_batch_norm=batch_norm)
    x = ResBlockDeepBNK(x, layer_dim, with_batch_norm=batch_norm)
    x = ResBlockDeepBNK(x, layer_dim, with_batch_norm=batch_norm)
    x = ResBlockDeepBNK(x, layer_dim, with_batch_norm=batch_norm)
    x = ResBlockDeepBNK(x, layer_dim, with_batch_norm=batch_norm)
    x = ResBlockDeepBNK(x, layer_dim, with_batch_norm=batch_norm)
    x = ResBlockDeepBNK(x, layer_dim, with_batch_norm=batch_norm)
    x = ResBlockDeepBNK(x, layer_dim, with_batch_norm=batch_norm)
    x = tf.keras.layers.Flatten()(x) 
    logits = tf.keras.layers.Dense(CLASS_NUM, name='logits')(x)
        
    return logits


def em0(x, alphabet_size, CLASS_NUM, hparams):
    batch_norm = True
    emb_size = hparams['char_emb_size']
    depth = hparams['depth']
    layer_dim = hparams['layer_dim']
    bottleneck = hparams['bottleneck_fc']
    ks = hparams['conv_kernel_size']
    batch_norm = hparams['batch_norm']
    print('depth: ', depth, depth*3)
    print('conv_kernel_size: ', ks)
    activation = hparams['activation']
    
    x = tf.keras.layers.Embedding(alphabet_size+1, emb_size)(x)
    x = tf.keras.layers.Conv1D(layer_dim, ks, padding='same')(x)
    
    block_type = hparams['block_type']
    if block_type == 0:
        block = ResBlockDeepBNK
    elif block_type == 1:
        block = ResBlock
    else:
        print("Default_block")
        block = ResBlockDeepBNK
    
    for i in range(depth):
        x = block(x, layer_dim, ks=ks, with_batch_norm=batch_norm, activation=activation)
        
    if bottleneck > 1:
        print('bottleneck_fc: ', bottleneck)
        fnb = int(x.shape[-1] / bottleneck)
        x = tf.keras.layers.Conv1D(fnb, 3, padding='same')(x)
    
    pre_fc = tf.keras.layers.Flatten()(x) 
    
    if "dropout" in hparams:
        dropout = hparams['dropout']
        print("DROPOUT: %s" % dropout)
        x = tf.keras.layers.Dropout(dropout)(pre_fc)
    else:
        x = pre_fc
        
    print("pre_fc ", x.shape, "/", pre_fc.shape)
    logits = tf.keras.layers.Dense(CLASS_NUM, name='logits')(x)
        
    return logits, pre_fc


def lstm0(x, alphabet_size, CLASS_NUM, hparams):
    emb_size = hparams['char_emb_size']
    x = tf.keras.layers.Embedding(alphabet_size+1, emb_size)(x)
    x = tf.keras.layers.LSTM(1000, return_sequences=True)(x)
    x = tf.keras.layers.LSTM(1000)(x)
    x = tf.keras.layers.Dense(CLASS_NUM, activation=None)(x)
    return x


def em0_latent(x, alphabet_size, CLASS_NUM, latent_size, hparams, batch_norm=True):
    
    batch_norm = True
    emb_size = hparams['char_emb_size']
    depth = hparams['depth']
    layer_dim = hparams['layer_dim']
    print('depth: ', depth, depth*3)
    
    x = tf.keras.layers.Embedding(alphabet_size+1, emb_size)(x)
    x = tf.keras.layers.Conv1D(layer_dim, 5, padding='same')(x)
    
    block_type = hparams['block_type']
    if block_type == 0:
        block = ResBlockDeepBNK
    elif block_type == 1:
        block = ResBlock
    else:
        print("Default_block")
        block = ResBlockDeepBNK
    
    for i in range(depth):
        x = block(x, layer_dim, with_batch_norm=batch_norm)
    
    x = tf.keras.layers.Flatten()(x) 
    x = tf.keras.layers.Dense(latent_size, activation=None, name='latent_terminal')(x)
        
    return x
