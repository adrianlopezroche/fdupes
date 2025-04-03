#! /usr/bin/python3
#
# Author : Jérôme Bouat jerome<dot>bouat<at>laposte<dot>net
#
# This script is transforming the output of fdupes
# (with '--recurse --size' options)
# into a csv table with duplicates sorted by decreasing size.
#
# Next, a simple spreadsheet software enables filtering the columns :
# - size of each file
# - occurences of the duplicate file
# - possible saving with the duplicate file
# - duplicate number
# - path
#
# Examples of commands combinations :
#----------
# fdupes --recurse --size . | fdupes2table.py > duplicates.csv
#----------
# fdupes --recurse --size . > duplicates.txt
# cat duplicates.txt | fdupes2table.py > duplicates.csv
#----------
#
# Example with the output of "fdupes --recurse --size . " :
#----------------------
# 16265216 byte(null)each:
# ./titi.DOC
# ./toto.DOC
# 
# 5527 byte(null)each:
# ./titi.gif
# ./toto.gif
# 
# 560149 byte(null)each:
# ./titi.pdf
# ./toto.pdf
#----------------------
#
# Example of the output of this script with the previous example :
#----------------------
# max possible saving : 16 MB
#
# size (ko);occurences;possible saving (kB);duplicate;path
#
# 15884;2;15884;1;titi.DOC
# 15884;2;15884;1;toto.DOC
#
# 547;2;547;2;titi.pdf
# 547;2;547;2;toto.pdf
#
# 5;2;5;3;titi.gif
# 5;2;5;3;toto.gif
#----------------------
#

import sys, re

beginPath = re.compile('^./')

sizes = {}
paths = {}
dupNumber = 1
getSize = True
line = sys.stdin.readline()
while len(line) > 0 :
  if getSize :
    size=line.split()[0]
    if size.isdigit() :
      size = int(size)
      if not (size in sizes) :
        sizes[size] = []
      sizes[size].append(dupNumber)
      paths[dupNumber] = []
      getSize = False
    else :
      raise 'doesn\'t find the size of the duplicate files'
  elif len(line) == 1 :
    dupNumber += 1
    getSize = True
  else :
    line = line[:-1]
    line = beginPath.sub('', line)
    paths[dupNumber].append(line)
  line = sys.stdin.readline()

totalSaving = 0
for size in sizes.keys() :
  nbDupForSize = 0
  for dupNumber in sizes[size] :
    nbDupForSize += (len(paths[dupNumber]) - 1)
  totalSaving += size * nbDupForSize
totalSaving = round(totalSaving * 2**-20)
print('max possible saving :;%d MB'%(totalSaving))
print()

print('size (ko);occurrences;possible saving (kB);duplicate;path')
newDupNum=0
for size in sorted(sizes.keys(), reverse=True) :
  sizeKo = round(size * 2**-10)
  for dupNumber in sizes[size] :
    newDupNum += 1
    occurences = len(paths[dupNumber])
    print()
    possibleSavingKB = round((occurences - 1) * size * 2**-10)
    for path in sorted(paths[dupNumber]) :
      print('%d;%d;%d;%d;%s'%(sizeKo, occurences, possibleSavingKB, newDupNum, path))
