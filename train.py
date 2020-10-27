#! /usr/bin/python3
import pandas as pd
import numpy as np
import math
import json
from datetime import datetime
from datetime import timedelta
import mysql.connector
from sklearn import linear_model

connection = mysql.connector.connect(host='localhost',
                                    database='meteo',
                                    user='daj_slowaka',
                                    password='walczaka')
query = "select timestamp,temperature from JKM WHERE timestamp LIKE CONCAT(DATE_FORMAT(DATE_SUB(NOW(),INTERVAL 1 HOUR),'%Y-%m-%d %H'),'%')"
cursor = connection.cursor()
cursor.execute(query)
last = pd.DataFrame(cursor.fetchall())
last.columns = ['timestamp','temp']
#last.index = last.index + 1
#last.sort_index(inplace=True)
last['date']=pd.to_datetime(last.timestamp,errors='coerce',dayfirst=True)
last['temp'] = pd.to_numeric(last['temp'])
last['day'] = last['date'].dt.day
last['month'] = last['date'].dt.month
last['hour'] = last['date'].dt.hour
last['year'] = last['date'].dt.year
g = last.groupby(["year","month","day","hour"])
hourly_averages = g.aggregate({"temp":np.mean})
hourly_averages['temp'] = round(hourly_averages['temp'],2)
hourly_averages.to_csv("/home/pi/prognoza/temp.csv",sep=",")

df=pd.read_csv('/home/pi/prognoza/train.csv', delimiter = ',',encoding= 'unicode_escape')
df2=pd.read_csv('/home/pi/prognoza/temp.csv', delimiter = ',',encoding= 'unicode_escape')
mean=pd.read_csv('/home/pi/prognoza/mean.csv', delimiter = ',',encoding= 'unicode_escape')
df2 = pd.merge(df2,mean,how="left")
df=df.append(df2)
last_temp = df.tail(24)
df.to_csv("/home/pi/prognoza/train.csv",sep=",",index=False)
df['day_temp'] = df.temp.shift(-24)
df['grad_temp'] = df.temp.shift(72)
#df['2day_temp'] = df.temp.shift(-48)\n,
X_test = df.tail(24)
del X_test['day_temp']
df = df.dropna()
X_train = df
y_train = X_train['day_temp']
del X_train['day_temp']
regressor = linear_model.LinearRegression()
regressor.fit(X_train, y_train)
prediction = regressor.predict(X_test)
X_test['predict'] = np.array(prediction)
#X_test['predict'] = round(X_test['predict'],2)
del X_test['temp']
del X_test['mean']
del X_test['grad_temp']
X_test['day'] +=1
prognoza=pd.read_csv('/home/pi/prognoza/last_prognoza.csv', delimiter = ',',encoding= 'unicode_escape')
prog = prognoza.tail(47)
prog = prog.iloc[:24]
#print(prog)
#print(last_temp)
real = last_temp['temp'].to_numpy()
pred = prog['predict'].to_numpy()
prednomean = prog['predictNoCorrection'].to_numpy()
err = real - pred
mean = err.mean()
absmean = abs(err)
abs_mean = absmean.mean()
errnomean = real - prednomean
absnomean = abs(errnomean)
abs_meannomean = absnomean.mean()
prog24nomean = X_test['predict'].iloc[23]
prog24nomean = round(prog24nomean,2)
#print(mean)
#print(X_test)
if mean <= -2:
	mean = -mean
	X_test['predict'] -= math.log(mean,2)*0.8 + 1
	mean = -mean
elif mean > -2 and mean < -1:
	mean = -mean
	X_test['predict'] -= math.log(mean,2)*0.7 + 1
	mean = -mean
elif mean >= -1 and mean <= 1:
	X_test['predict'] += mean
elif mean > 1 and mean < 2:
	X_test['predict'] += math.log(mean,2)*0.7 + 1
elif mean >= 2:
	X_test['predict'] += math.log(mean,2)*0.8 + 1
#print(X_test['predict'])
X_test['predict'] = round(X_test['predict'],2)
prognoza = prognoza.append(X_test.iloc[23])
prognoza.iloc[-1, prognoza.columns.get_loc('error')] = abs_mean
prognoza.iloc[-1, prognoza.columns.get_loc('predictNoCorrection')] = prog24nomean
prognoza.iloc[-1, prognoza.columns.get_loc('errorNoCorrection')] = abs_meannomean
prognoza.to_csv("/home/pi/prognoza/last_prognoza.csv",sep=",",index=False)
temp = prognoza.tail(50)
his = temp.iloc[:27]
#print(his)
his.to_json('/var/www/html/last24hours.json',orient='records')
prognoza.tail(24).to_json('/var/www/html/prognoza.json',orient='records')
#X_test.to_csv('prognoza.csv', sep=',', index=False)
