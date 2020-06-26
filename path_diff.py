import matplotlib
import matplotlib.pyplot as plt
import numpy as np
import csv
import os



def subcategorybar(X, vals, labels, width=0.8):
    n = len(vals)
    _X = np.arange(len(X))
    for i in range(n):
        plt.bar(_X - width/2. + i/float(n)*width, vals[i], 
                width=width/float(n), align="edge", label = labels[i])   
    plt.xticks(_X, X)

username = os.environ['USER']
num = int(input("How many files would you like to plot?\n"))
files = []

for i in range(num):
    path = input("Input folder name?\n")
    filename = input("Input file name (" + str(i+1) + "/" + str(num) + "). Note default = path_data.csv\n")
    if (filename == ''):
        filename = "path_data.csv"
    files.append("/home/"+username+"/Myhal_Simulation/simulated_runs/" + path + "/logs-" + path + "/"+ filename)

x_labels = []
y_data = []
labels = []

for filename in files:
    x = []
    y = []
    series = ""

    with open(filename,'r') as csvfile:
        plots = csv.reader(csvfile, delimiter=',')
        count = 0
        for row in plots:
            if (count < 2):
                if count == 0:
                    series = row[0]
                count+=1
                continue
            x.append(row[0])
            optimal_length = float(row[1])
            success = int(row[2])
            actual_length = 0
            if (success):
                actual_length = float(row[3])


            percent_diff = ((actual_length - optimal_length)/optimal_length)*100
            y.append(percent_diff)
            count+=1
    
    x_labels = x
    y_data.append(y)
    labels.append(series)
    #plt.bar(x,y, label= series)


#subcategorybar(x_labels , y_data, labels)

#plt.xlabel('Target Number')
#plt.ylabel('Difference From Optimal Path (%)')
#plt.title('Length Difference Between Actual Path and Optimal Path')
#plt.legend()

#plt.show()

x = np.arange(len(x_labels))
width = 0.8


fig,ax=plt.subplots()
n = len(y_data)

rects_list = []

for i in range(len(y_data)):
    c = "g"
    if (labels[i] == "Ground Truth Demon"):
        c = "r"
    elif (labels[i] == "No Demon"):
        c = "b"
    rects_list.append(ax.bar(x- width/2. + i/float(n)*width, y_data[i], width/float(n), align="edge",  label = labels[i], color = c))

ax.set_ylabel('Percent Deviation From Optimal Path (%)')   
ax.set_xlabel('Target Number')  
ax.set_title('Length Difference Between Actual Path and Optimal Path')
ax.set_xticks(x)
ax.set_xticklabels(x_labels)
ax.legend()

def autolabel(rects):
    """Attach a text label above each bar in *rects*, displaying its height."""
    for rect in rects:
        height = rect.get_height()
        ax.annotate('{}'.format(height),xy=(rect.get_x() + rect.get_width() / 2, height),
                    xytext=(0, 3),  # 3 points vertical offset
                    textcoords="offset points",
                    ha='center', va='bottom')

                    
for rects in rects_list:
    autolabel(rects)


fig.tight_layout()
plt.show()

