/* Harter Test fuer CC.TBX: Zeiger, Arrays, Globals, Logik, char */
int zaehler;
int werte[10];
char text[40];

int strlen2(char* s) {
    int n;
    n = 0;
    while (*s) { n = n + 1; s = s + 1; }
    return n;
}

int summe(int* feld, int anzahl) {
    int i; int s;
    s = 0;
    for (i = 0; i < anzahl; i++) s = s + feld[i];
    return s;
}

int main() {
    int i; int a; int b; int* p;
    char* q;

    cls();
    print("Pointer/array test\n\n");

    for (i = 0; i < 10; i++) werte[i] = i * i;
    print("Squares 0..9 sum = ");
    printn(summe(werte, 10));
    print("\n");

    a = 5; b = 12;
    p = &a;
    *p = *p + 37;
    print("a after pointer write = ");
    printn(a);
    print("\n");

    print("Logic: ");
    if (a == 42 && b > 10) print("and-ok ");
    if (a == 99 || b == 12) print("or-ok ");
    if (!(a < 10)) print("not-ok");
    print("\n");

    q = "Hello pointer world";
    print("strlen = ");
    printn(strlen2(q));
    print("\n");

    i = 0;
    while (i < 5) {
        text[i] = 65 + i;
        i++;
    }
    text[5] = 0;
    print("built string: ");
    print(text);
    print("\n");

    zaehler = 0;
    for (i = 0; i < 100; i++) {
        if (i % 7) continue;
        zaehler++;
        if (zaehler > 8) break;
    }
    print("multiples of 7 counted: ");
    printn(zaehler);
    print("\n\nAll tests done. Press a key.\n");
    getkey();
    return 0;
}
