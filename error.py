#! /usr/bin/python3

import pandas as pd
from sklearn.metrics import mean_absolute_error
from sklearn.metrics import mean_squared_error
from math import sqrt

prognoza=pd.read_csv('last_prognoza.csv', delimiter = ',',encoding= 'unicode_escape')
temp=pd.read_csv('train.csv', delimiter = ',',encoding= 'unicode_escape')
x = prognoza.tail(1550)
real = temp.tail(1526)
prog = x.iloc[:1526]

print(prog.head(5)['day'],prog.head(5)['hour'])
print(real.head(5))
print(prog.tail(5)['day'],prog.tail(5)['hour'])
print(real.tail(5))

print(mean_absolute_error(real['temp'], prog['predict']))
print(sqrt(mean_squared_error(real['temp'], prog['predict'])))
print(mean_absolute_error(real['temp'], prog['predictNoCorrection']))
print(sqrt(mean_squared_error(real['temp'], prog['predictNoCorrection'])))
