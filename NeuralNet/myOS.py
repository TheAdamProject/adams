import os, sys

def basename(path):
    path = os.path.normpath(path)
    return os.path.basename(path)

def splitpath(path):
    path = os.path.normpath(path)
    return path.split(os.path.sep)

def basenameNoExt(path, sep='.'):
    name = basename(path)
    return name.split(sep)[0]

def mkdir(path):
    try:
        os.mkdir(path)
    except FileExistsError:
        ...