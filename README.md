# serwer-z-komunikacja
TO JEST PO PROSTU NA SOPY ZEBY UMIEC PROSZE NIE OCENIAC 
NIE POZDRAWIAM HEJTEROW DEAL WITH IT


L8: Agora Ateńska
Jest rok 440 p.n.e. Ateny przeżywają złoty wiek demokracji. Obywatele zbierają się na agorze aby wymieniać poglądy i ogłoszenia. Twoim zadaniem jest napisanie serwera TCP który umożliwia obywatelom przesyłanie wiadomości między sobą za pośrednictwem centralnego placu.
Ważne: Każda wiadomość kończy się znakiem \n. Żadna wiadomość nie przekracza MAX_MSG_LEN = 63 znaków (włącznie z \n).

Etap 1 — 3 pkt (~20 min)
Zaimplementuj przyjmowanie pojedynczego połączenia TCP.
Serwer przyjmuje jeden argument: numer portu.
./agora 8888
Po uruchomieniu serwer nasłuchuje połączeń. Po podłączeniu pierwszego klienta wypisuje:
A citizen has arrived at the agora!
Zamyka połączenie i kończy działanie.

Etap 2 — 7 pkt (~40 min)
Zaimplementuj obsługę wielu klientów z użyciem epoll.

Maksymalna liczba klientów: MAX_CLIENTS = 10
Po połączeniu klient wysyła swoje imię w pierwszej wiadomości (zakończone \n)
Po odebraniu imienia serwer odsyła: "Welcome to the agora, [imię]!\n"
Wypisz na stdout: "[imię] joined the agora"
Poprawnie obsługuj rozłączanie — wypisz: "[imię] left the agora" (jeśli klient nie zdążył podać imienia użyj "???")


Etap 3 — 6 pkt (~35 min)
Zaimplementuj rozsyłanie wiadomości.

Gdy serwer odbierze pełną wiadomość (zakończoną \n) od klienta który już podał imię, roześle ją do wszystkich pozostałych połączonych klientów w formacie: "[imię]: [treść]\n"
Wiadomości mogą przychodzić urwane — używaj bufora
Jeśli klient nie podał jeszcze imienia, wiadomości są ignorowane


Etap 4 — 4 pkt (~25 min)
Obsłuż sygnał SIGINT.
Po otrzymaniu SIGINT serwer:

Rozsyła do wszystkich klientów: "The agora is closing!\n"
Zamyka wszystkie połączenia
Wypisuje: "Citizens present: [imię1], [imię2], ..." (tylko ci którzy podali imię)
Zwalnia zasoby i kończy działanie
