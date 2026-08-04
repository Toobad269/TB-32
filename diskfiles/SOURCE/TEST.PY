# Ein Python-Programm, das auf dem TB-32 laeuft
print("Hello from Python on the TB-32!")

x = 7
y = 6
print("7 * 6 =", x * y)

name = "Colin"
print("Hi " + name + ", this is real Python syntax.")

total = 0
for i in range(1, 11):
    total += i
print("Sum 1..10 =", total)

def fak(n):
    if n <= 1:
        return 1
    return n * fak(n - 1)

print("10! =", fak(10))

zahlen = [3, 1, 4, 1, 5, 9, 2, 6]
print("List:", zahlen)
print("Length:", len(zahlen))

groesste = 0
for z in zahlen:
    if z > groesste:
        groesste = z
print("Largest:", groesste)

i = 0
while i < 5:
    if i == 3:
        i += 1
        continue
    print("count", i)
    i += 1
