
import sys
s = "".join(sys.argv[1:]).replace(" ","").strip()

if s == "":
    print("reading from stdin until EOF")
    s = sys.stdin.read()
    for c in "\t\n ":
        s = s.replace(c,"").strip()

s = s.upper()

chars = list(s)
for i, c in enumerate(chars):
    if c not in "0123456789ABCDEF":
        chars[i] = "?"
s = "".join(chars)
del chars

size = int(len(s) / 2)
print("Array size: " + str(size))
print("{ ", end="")
for i in range(size):
    bytestr = s[i*2:i*2+2]
    if bytestr == "??":
        print("MASK", end="")
    else:
        print("0x" + bytestr, end="")
    if i < size - 1:
        print(", ", end="")
print(" }")
