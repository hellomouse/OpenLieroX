#!/usr/bin/python -u

import sys

f = open("pwn0meter.txt","r")
w = open("pwn0meter.html","w")
#w = sys.stdout
w.write("<HEAD><TITLE>Pwn0meter</TITLE></HEAD><BODY><H2>Pwn0meter</H2><BR>\n")

killers = {}
deaders = {}

for l in f.readlines():
	l = l.strip()
	if l == "":
		continue
	try:
		( time, deader, killer ) = l.split("\t")
	except:
		continue
	if killer.find("[CPU]") >= 0:
		continue
	if killer.find("The Third") >= 0:
		continue
	if not killer in killers.keys():
		killers[killer] = []
	if not deader in deaders.keys():
		deaders[deader] = []
	killers[killer].append(deader)
	deaders[deader].append(killer)
f.close()

#print killers

sorted = killers.keys()
def sortFunc(s1, s2):
	kills1 = len(killers[s1]) - killers[s1].count(s1)
	kills2 = len(killers[s2]) - killers[s2].count(s2)
	if kills1 < kills2: return 1
	if kills1 > kills2: return -1
	try:
		deaths1 = len(deaders[s1])
	except:
		deaths1 = 0
	try:
		deaths2 = len(deaders[s2])
	except:
		deaths2 = 0
	if deaths1 < deaths2: return -1
	if deaths1 > deaths2: return 1
	return 0
	
sorted.sort(cmp=sortFunc)

for k in sorted:
	kills = len(killers[k])
	try:
		deaths = len(deaders[k])
	except:
		deatsh = 0
	suicides = killers[k].count(k)
	kills -= suicides
	deaths -= suicides
	w.write("<B>[B]%s[/B]</B>: %i kills %i deaths %i suicides, killed:" % ( k, kills, deaths, suicides ))
	# Ugly killer sorting
	killedMax = {}
	for f in killers[k]:
		if not killers[k].count(f) in killedMax.keys():
			killedMax[killers[k].count(f)] = []
		if killedMax[killers[k].count(f)].count(f) == 0:
			killedMax[killers[k].count(f)].append(f)
	killedMax1 = killedMax.keys()
	killedMax1.sort(reverse=True)
	count = 0
	for f in killedMax1:
		for f1 in killedMax[f]:
			if f1 == k: # Don't write suicides
				continue
			count += 1
			if count >= 5:
				break
			if count != 1:
				w.write(",")
			w.write(" %s - %i" % ( f1, f ) )
	w.write("<BR>\n")

w.write("</BODY>\n")
w.close()
