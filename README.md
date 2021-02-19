# Adaptive, Dynamic Mangling rules (**ADaMs**) password guessing attack

Official repo for the *"Reducing Bias in Modeling Real-world Password Strength via Deep Learning and Dynamic Dictionaries"* to be presented at USENIX Security 2021.

- **Note:** The code reported in this repository is mainly aimed to reproduce the results in our paper and support security analysis in the academic context.  We are working on a complete re-implementation of the software that will be applicable in actual password recovery applications. Stay tuned.

# Run a ADaMs attack
...

## Pre-trained Models
Pre-trained models in Keras format for the Adaptive mangling rules procedure are available [here](https://drive.google.com/drive/folders/1b697kYxg1z3BAgn7R7fy9pX5lBHE6Et-?usp=sharing).

Otherwise, these can be automatically downloaded via the make file....

# Train your model (Adaptive Mangling Rules)

In order to train your model, you have to create a training set first.

## Creating a training set

The directory *MakeTrainingset* contains the necessary code to produce the training sets. The script based on MPI, thus a suitable implementation of MPI should be installed in your system.  

The code can be compile using the makefile into the directory e.g.,:

> cd MakeTrainingset; make 

This produces a directory *bin* containing a binary file *makeTrainingsetMPI* that you will run to produce your training set. 

The binary takes as input different mandatory parameters:

* **-w** the dictionary used to create the training set. Referred as *W* in the paper.
* **-l** the attacked set of passwords used to create the training set. Called $X_{A}$ in the paper.
* **-r** an hashcat rules-set e.g., *generated.rule*. 
* **-o** output directory, where to write the training set files.

....

# Acknowledgements
> "Standing on the shoulders of giants"


Our software is built on top of **hashcat-legacy**.
<p align="center">
  <img width="30%" height="30%" src="https://gwillem.gitlab.io/assets/img/hashcat.png">
</p>

