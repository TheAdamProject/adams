import matplotlib.pyplot as plt
import numpy as np
import re, sys

def read(path):
    L = []
    with open(path) as f:
        for l in f:
            l = l.strip()
            g = re.match("\[LOG\]: \(#guesses: (\d+) \|~10\^\d\|\)\t\(recovered: ([.\d]+)\%\)", l)
            if g:
                L.append((float(g.groups()[0]), float(g.groups()[1])))
    return np.array(L)

if __name__ == '__main__':
    try:
        adams = sys.argv[1]
        std = sys.argv[2]
        outfig = sys.argv[3]
    except:
        print("USAGE] path_to_adams_log path_to_standard_log path_output_fig")
        sys.exit(1)

    fig, ax = plt.subplots(1, 1)

    def p(w, label, c):
        ax.plot(w[:,0], w[:,1], label=label, c=c)
        ax.scatter(w[-1,0], w[-1,1],c=c)
  
    adams = read(adams)
    std = read(std)
    
    p(adams, 'ADaMs attack', 'red')
    p(std, 'standard attack', 'black')
    ax.grid()
    ax.legend()
    
    fig.savefig(outfig)

