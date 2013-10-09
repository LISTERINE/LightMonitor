#!/usr/bin/python
""" Install pybottle. Create user and add them to the dialout(20) group.
Open a screen su to the user in the dialout group. run the script and
detach the screen."""
from subprocess import call
from socket import socket, gethostbyname, gethostname, AF_INET, SOCK_STREAM
from time import sleep, localtime, strftime


def get_time():
    return  int(strftime("%H", localtime()))

def time_between(minTime, maxTime):
    curTime = get_time()
    return (curTime <= minTime or curTime >= maxTime)

def power_signal(state):
    print state, get_time()
    if state == "home" and time_between(8, 16):
        call(["br", "--on=1,2"])
    elif state == "away":
        call(["br", "--off=1,2"])




server = socket(AF_INET, SOCK_STREAM)
server.bind(("10.0.0.111", 5000))
server.listen(1)

while 1:

    print "Waiting for data"
    client = server.accept()
    data = client[0].recv(1024)
    power_signal(data)


server.close()
