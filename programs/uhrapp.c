/* ==========================================================================
   UHRAPP  --  ein BRAVES Programm, das gerne beim Start mitlaufen wuerde

   Der Gegenentwurf zu TAKT/SEUCHE: statt sich heimlich einzunisten, FRAGT es
   hoeflich. autostart_anmelden() legt nur einen Wunsch hin -- der TOOBAD
   DEFENDER poppt auf und fragt den Benutzer "Programm moechte bei jedem Start
   laufen -- erlauben?". Nur bei "Allow" wird der Autostart geschrieben.

   So soll es sein: jedes Programm DARF fragen, aber der Mensch entscheidet.
   ========================================================================== */
#include "proglib.c"

int main() {
    print("UHRAPP would like to run at every startup.\n");
    print("Asking the Defender for permission ...\n");
    autostart_anmelden("UHRAPP.TBX");
    print("Request sent. Check the desktop for the Defender prompt.\n");
    return 0;
}
