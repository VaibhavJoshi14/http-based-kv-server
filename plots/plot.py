"""
- [avg throughput, avg response time ms]
[1, 2, 3, 4, 6]                //      10, 15, (fails at 25 due to low disk space)]
create_all [[93.20, 10.19] , [126.1, 16.17], [120.557, 22.86], [125.39, 20.32], [106.6, 55,7],        //       [115.3, 47.28], [116, 88.15]], 
read_all [[1078, 0.196], [2127, 0.29], [2155, 1.02], [2097, 1.61], [1949, 2.59]                   //       [2161, 1.92], [216.223, 4.69]]
rotate_all [[761.29, 1.026]. [1276, 1.13], [1393, 1.71],  [1316.19, 3.16], [1382, 3.8]            //       [1333, 6.64], [1362, 10.25]]



10 clients [72.96, 0.224, 61.46], [53.48, 0.016, 62.63], [50.61, 0.05, 52.79]
15 clients [50.6, 1.18, 54.99], [44.01, 0.033, 70.42], [50.22, 0.05, 70.68]

"""
import matplotlib.pyplot as plt

clients_count = [1, 2, 3, 4, 6]
"""
# average throughput
avg_throughput = [761.29, 1276, 1393, 1316.19, 1382]
plt.plot(clients_count, avg_throughput)
plt.title("avg throughput (rotate command)")
plt.xlabel("num clients")
plt.ylabel("avg throughput (req/s)")
plt.savefig('avg_thp_rotate.png')
plt.show()

#avg resp time
avg_throughput = [1.026, 1.13, 1.71, 3.16, 3.8]
plt.plot(clients_count, avg_throughput)
plt.title("avg response time (rotate command)")
plt.xlabel("num clients")
plt.ylabel("avg response time (ms)")
plt.savefig('avg_resptime_rotate.png')
plt.show()

"""
perc_cpu_util = [67, 95.5, 98.7, 99.56, 99.7]
perc_disk_util = [0.133, 0.066, 0.014, 0.004, 0.03]

plt.plot(clients_count, perc_cpu_util, label = "cpu utilization")
plt.plot(clients_count, perc_disk_util, label = "disk utilization")
plt.xlabel("num clients")
plt.ylabel("percentage utilization")
plt.legend(loc="upper right")
plt.title("utilization for rotate command at server")
plt.savefig('utilization for rotate command at server.png')
plt.show()

"""
<create read rotate>

database [%cpu, %disk, %ram]
1 client [49.6, 0.74, 60.4], [11.76, 0.15, 62.04], [10.5, 0.11, 61]
2 clients [67, 0.87, 63.27], [3.9, 0.07, 63.85], [3.66, 0.0066, 63.85]
3 clients [65.6, 1.07, 63.4], [9.75, 0.029, 63.75], [3.77, 0.014, 63.78]
4 clients [41.57, 0.8, 63.8], [51.97, 0.052, 63.94], [47.25, 0.004, 63.81]
6 clients [66.04, 1.15, 57.9], [27.48, 0.08, 59.10], [19.01, 0.03, 58.795]

10 clients [47.88, 0.004, 68.58], [30.01, 0.86, 74.34], [49.37, 0.018, 74.33]
15 clients [51.09, 1.16, 54.99], [44.01, 0.033, 70.42], [50.22, 00.54, 70.6]

server []
1 client [40, 0.737, 62.39], [64, 0.066, 62.69], [67, 0.113, 62.71]
2 clients [51.4, 0.87, 63.27], [89., 0.07, 63.85], [95.53, 0.0066, 63.85]
3 clients [53.69, 1.01, 63], [92.93,0.029, 63.75] , [98.71, 0.0146, 63.78] 
4 clients [54.43, 2.4, 63.8], [94.38, 0.05, 63.94], [99.56, 0.004, 63.8]
6 clients [59.22, 1.16, 57.95], [88.51, 0.08, 59.10], [99.7, 0.03, 58.7]
"""