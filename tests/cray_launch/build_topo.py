import os
import sys
import re
NODEFILE=sys.argv[1]
TOPODEF=sys.argv[2]
FE=sys.argv[3]
DEFSYMBOL="@"

f = open(NODEFILE, "r")
nodes = f.readlines()
f.close()

nodeMap = {}

in_format = DEFSYMBOL + "{}" + DEFSYMBOL
nodeMap[DEFSYMBOL+"FE"+DEFSYMBOL] = FE

for x in range(0,len(nodes)):
	nodeMap[in_format.format("NODE"+str(x))] = nodes[x].split(":")[0]

print nodeMap

f = open(TOPODEF, "r")
out = open(TOPODEF + ".top", "w")
for x in f.readlines():
	outStr = x
	try:
		replaceText = re.search("@[A-Z0-9]*@", x).group(0)
		if replaceText not in nodeMap:
			print "Could not find: " + replaceText
			exit(-1)
		outStr = outStr.replace(replaceText, nodeMap[replaceText])
		print outStr[:-1]
	except:
		pass
	out.write(outStr)
f.close()
out.close()



