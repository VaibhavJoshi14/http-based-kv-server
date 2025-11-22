"""

- [avg throughput, avg response time ms]
[1, 2, 3, 4, 10, 15, (fails at 25 due to low disk space)]
create_all [[93.20, 10.19] , [126.1, 16.17], [120.557, 22.86], [125.39, 20.32], [115.3, 47.28], [116, 88.15]], 
read_all [[1078, 0.196], [2127, 0.29], [2155, 1.02], [2097, 1.61], [2161, 1.92], [216.223, 4.69]]
rotate_all [[761.29, 1.026]. [1276, 1.13], [1393, 1.71],  [1316.19, 3.16], [1333, 6.64], [1362, 10.25]]


<create read rotate delete>

database [%cpu, %disk, %ram]
1 client [36.41, 0.73, 62.3], [32.01, 0.025, 62.69], [30.21, 0.052, 62.71], [37.49,0.07, 62.57]
2 clients [41.63, 0.87, 63.27], [50.96, 0.07, 63.85], [46.41, 0.0066, 63.85], [49.73, 0.045, 63.72]
3 clients [42.45, 0.818, 63.4], [49.49, 0.008, 63.75], [46.79, 0.014, 63.78], [50.59, 0.0733, 63.95]
4 clients [41.57, 0.8, 63.8], [51.97, 0.052, 63.94], [47.25, 0.004, 63.81], [48.33, 0.18, 62.28]
10 clients [47.88, 0.004, 68.58], [30.01, 0.86, 74.34], [49.37, 0.018, 74.33], [48.1, 0.02, 74.23]
15 clients [51.09, 1.16, 54.99], [44.01, 0.033, 70.42], [50.22, 00.54, 70.6], [46.25, 0.04, 70.66]

server []
1 client [37.06, 0.737, 62.39], [33.32, 0.066, 62.69], [30.70, 0.01, 62.71], [36.49, 0.07, 62.57]
2 clients [41.44, 0.87, 63.27], [50.96, 0.07, 63.85], [46.41, 0.0066, 63.85], [49.72, 0.045, 63.75]
3 clients [42.88, 0.81, 63], [49.49,0.008, 63.75] , [49.797, 0.0146, 63.78]  , [50.59, 0.073, 63.95]
4 clients [41.30, 0.8, 63.8], [51.9, 0.05, 63.94], [47.2537, 0.004, 63.8], [48.33, 0.18, 62.28]
10 clients [41.1, 0.95, 61.46], [53.48, 0.016, 62.63], [50.61, 0.05, 52.79], [46.38, 0.06, 63.006]
15 clients [50.6, 1.18, 54.99], [44.01, 0.033, 70.42], [50.22, 0.05, 70.68], [46.25, 0.04, 70.66]


"""
import matplotlib.pyplot as plt

clients_count = [1, 2, 3, 4, 10, 15]
"""
# average throughput
avg_throughput = [761.29, 1276, 1393, 1316.19, 1333, 1362]
plt.plot(clients_count, avg_throughput)
plt.title("avg throughput (rotate command)")
plt.xlabel("num clients")
plt.ylabel("avg throughput (req/s)")
plt.savefig('avg_thp_rotate.png')
plt.show()

#avg resp time
avg_throughput = [1.026, 1.13, 1.71, 3.16, 6.64, 10.25]
plt.plot(clients_count, avg_throughput)
plt.title("avg response time (rotate command)")
plt.xlabel("num clients")
plt.ylabel("avg response time (ms)")
plt.savefig('avg_resptime_rotate.png')
plt.show()
"""

perc_cpu_util = [30.21, 46.21, 46.79, 47.25, 49.37, 50.22]
perc_disk_util = [0.052, 0.0066, 0.014, 0.004, 0.018, 0.054]
perc_ram_util = [62.3, 63.27, 63.4, 63, 74, 70]
plt.plot(clients_count, perc_cpu_util, label = "cpu utilization")
plt.plot(clients_count, perc_disk_util, label = "disk utilization")
plt.plot(clients_count, perc_ram_util, label = "ram utilization")
plt.xlabel("num clients")
plt.ylabel("percentage utilization")
plt.legend(loc="upper right")
plt.title("utilization for rotate command at database")
plt.savefig('utilization for rotate command at database.png')
plt.show()
