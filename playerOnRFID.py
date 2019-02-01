#!/usr/bin/env python3

# Lecture d'un pipe fifo pour envoyer des videos avec omxplayer
#
#
#    Copyright 2018 Daniel Perron.
#
#
#    playerOnRFID.py is free software: you can redistribute it and/or modify
#    it under the terms of the GNU Lesser General Public License as published
#    by the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    playerOnRFID is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU Lesser General Public License for more details.
#
#    You should have received a copy of the GNU Lesser General Public License
#    along with playerOnRFID.  If not, see <http://www.gnu.org/licenses/>.

import subprocess
import time
import sys
import errno
import random

proc = None


# Lire un fichier contenant des videos a jouer au hasard
def randomFile(fichierListe):
    global proc
    myVideos = []
    try:
        myfile = open(fichierListe, 'r')
        myVideos = myfile.read().splitlines()
        myfile.close()
    except IOError:
        pass
    count = len(myVideos)
    if proc is not None:
        try:
            proc.stdin.write(b'q')
        except IOError:
            pass
    time.sleep(0.1)
    proc = None
    if count > 0:
        proc = subprocess.Popen(['/usr/bin/omxplayer',
                                 '--display', '0',
                                 myVideos[random.randrange(0, count)]],
                                stdout=subprocess.PIPE,
                                stdin=subprocess.PIPE)


# Lire le video  sous le folder /home/pi/video
def showVideo(video):
    global proc
    if proc is not None:
        try:
            proc.stdin.write(b'q')
        except IOError as e:
            if e.errno == errno.EPIPE:
                pass
        time.sleep(0.1)
    subfolder = ""
    if video.find("http:") < 0:
        subfolder = "/home/pi/video/"
    proc = subprocess.Popen(['/usr/bin/omxplayer', '--display',
                             '0', subfolder+video],
                            stdout=subprocess.PIPE,
                            stdin=subprocess.PIPE)

# Boucle principale
# Attendre une commande provenant du fifo et l'executer
while True:
    try:
        rfid = open('/myfifo/fifoRFID', 'r')
        cmd = rfid.readline()
    except IOError as e:
        if e.errno == errno.EPIPE:
            proc = None
        continue
    print(cmd)
    rfid.close()

    if len(cmd) > 0:
        cmdSplit = cmd.split()

        if cmdSplit[0] == 'random':
            randomFile(cmdSplit[1])
        elif cmd == 'pause':
            if proc is not None:
                try:
                    proc.stdin.write(b' ')
                    proc.stdin.flush()
                except IOError as e:
                    if e.errno == errno.EPIPE:
                        pass
        elif cmd == 'seek30s':
            if proc is not None:
                try:
                    print("seek 30s")
                    proc.stdin.write('\x1b[C')
                    proc.stdin.flush()
                except IOError as e:
                    if e.errno == errno.EPIPE:
                        pass
        elif cmd == 'quit':
            if proc is not None:
                try:
                    proc.stdin.write(b'q')
                    proc.stdin.flush()
                except IOError as e:
                    if e.errno == errno.EPIPE:
                        pass
            time.sleep(0.1)
        else:
            showVideo(cmd)

# Sortie de boucle. forcer omxplayer a quitter
if proc is not None:
    proc.stdin.write('q')
    proc.stdin.flush()
