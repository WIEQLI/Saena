import subprocess
import sys

if len(sys.argv) != 3:
    print "usage: <#processors> <final matrix grid size>"

# for x in range(3, int(sys.argv[1])+1):
#     print(x)

# args = ("bin/bar", "-c", "somefile.xml", "-d", "text.txt", "-r", "aString", "-f", "anotherString")
#Or just:
#args = "bin/bar -c somefile.xml -d text.txt -r aString -f anotherString".split()
# args = "./Saena 3 3 3"

for x in range(3, int(sys.argv[2])):
    args = ("mpirun", "-np", sys.argv[1], "./Saena", str(x))
    popen = subprocess.Popen(args, stdout=subprocess.PIPE)
    popen.wait()
    output = popen.stdout.read()
    print output
    print "\n\n================================================================="
    print "================================================================="
    print "=================================================================\n\n"
