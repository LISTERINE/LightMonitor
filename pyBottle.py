#!/usr/bin/python
""" Install pybottle. Create user and add them to the dialout(20) group.
Open a screen su to the user in the dialout group. run the script and
detach the screen."""
from subprocess import call
from socket import socket, gethostbyname, gethostname, AF_INET, SOCK_STREAM
from time import sleep, localtime, strftime




def time_between(minTime, maxTime):
    curTime = int(strftime("%H", localtime()))
    return (curTime <= minTime or curTime >= maxTime)

def timer():
    if time_between(8, 16):
        call(["br", "--on=1,2"])
    else:
        call(["br", "--off=1,2"])

def signal(command):
    command = "br "+command
    call(command.split())


server = socket(AF_INET, SOCK_STREAM)
server.bind(("", 5000))
server.listen(5)

while 1:

    print "Waiting for data"
    client = server.accept()
    data = client[0].recv(1024)
    signal(data)


server.close()
