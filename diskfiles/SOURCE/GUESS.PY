# Zahlenraten -- zeigt input() und while
print("Guess the number between 1 and 100!")
geheim = (ticks() * 7) % 100 + 1
versuche = 0
while True:
    antwort = input("Your guess: ")
    zahl = int(antwort)
    versuche += 1
    if zahl < geheim:
        print("Too low")
    elif zahl > geheim:
        print("Too high")
    else:
        print("Correct! You needed", versuche, "tries.")
        break
