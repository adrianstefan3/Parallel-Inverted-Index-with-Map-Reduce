# Tema 1a
## Index inversat (Map-Reduce)
La inceputul codului inainte de functiile thread-urilor si functia main am declarat 3 structuri de date pentru a-mi facilita trimiterea argumentelor catre thread-uri:

- **inputFisiere**
    ```c
    struct inputFisiere {
        int id_fis;
        string nume_fisier;
    };
    ```
    - folosita pentru a retine numele fisierului si indicele acestuia in fisierul de intrare primit ca argument.

- **inputMapper**
    ```c
    struct inputMapper {
        int id_thread;
        vector<map<string, set<int>>> *map_lists;
        vector<inputFisiere> fisiere;
        int *indexFisier;
        pthread_mutex_t *map_mutex;
        int n_fis;
        pthread_barrier_t *bariera;
    };
    ```
    - folosita drept argument pentru thread-urile de tip map, unde:
        - id_thread reprezinta id-ul thread-ului;
        - *map_lists reprezinta un array de dictionare comun ("global"), cate unul pentru fiecare thread mapper, in care acestia retin asocieri dintre cuvintele citite de ei si indexii fisierelor de unde le-au citit (reprezinta listele partiale create de mappers);
        - fisiere reprezinta array-ul de fisiere care trebuie citite;
        - *indexFisier reprezinta indexul fisierului care nu a fost citit inca si care urmeaza in array-ul de fisiere (prin care simulez o coada de fisiere necitite);
        - *map_mutex reprezinta mutex-ul pe care il folosesc pentru a nu avea conditii de cursa in momentul in care iau index-ul curent pentru fisier si il si incrementez;
        - n_fis reprezinta numarul de fisiere care trebuie citite;
        - *bariera reprezinta o bariera intre mappers si reducers aceasta se deblocheaza in momentul in care atat mappers cat si reducers au ajuns la ea, astfel permitand reducerilor sa "lucreze" deabia dupa ce mappers si-au terminat "munca";

- **inputReducer**
    ```c
    struct inputReducer {
        int id_thread;
        vector<map<string, set<int>>> *map_lists;
        int *indexLitera;
        pthread_mutex_t *reduce_mutex;
        pthread_barrier_t *bariera;
    };
    ```
    - folosita drept argument pentru thread-urile de tip reduce, unde:
        - id_thread reprezinta id-ul thread-ului;
        - *map_lists reprezinta array-ul cu "listele" partiale create de thread-urile mappers (asocieri intre cuvinte si indexi);
        - *indexLitera reprezinta indexul literei curente care nu a fost procesata (adica pentru care nu s-au cautat cuvintele care incep cu aceasta);
        - *reduce_mutex reprezinta mutex-ul pe care il folosesc pentru a nu avea conditii de cursa in momentul in care iau index-ul curent pentru litera si il si incrementez;
        - *bariera reprezinta o bariera intre mappers si reducers aceasta se deblocheaza in momentul in care atat mappers cat si reducers au ajuns la ea, astfel permitand reducerilor sa "lucreze" deabia dupa ce mappers si-au terminat "munca";

## Main
In functia main am inceput prin a lua **argumentele din linia de comanda** si prin a deschide fisierul de intrare. Apoi am citit denumirile fisierelor si le-am stocat intr-un **vector cu elemente de tipul structurii inputFisiere**. Dupa aceea am declarat un **array de thread-uri (mappers + reducers)**, doua mutex-uri si o **bariera pentru sincronizarea** task-urile intre map si reduce. Am creat thread-urile si le-am pornit. Pentru thread-uri am avut doua functii diferite (sincronizate cu acea bariera):
- ### mapFunc(void *arg)
    In interiorul functiei **simulez o coada** cu ajutorul vectorului de fisiere, astfel: fiecare thread are acces in structura lui la un indice de fisier "global" partajat intre thread-uri (este transmis prin referinta). In acest sens, cat timp indicele nu depaseste numarul de fisiere de intrare acesta **se asigneaza unui thread** pe rand si se incrementeaza folosind un **mutex pentru a nu avea race conditions**. Astfel munca de "map" este **impartita in mod dinamic** intre thread-uri, deoarece odata ce unul termina de procesat un fisiere il ia pe urmatorul (cel corespunzator index-ului curent). Odata ce un thread are un index valid, deschide fisierul, citeste cuvant cu cuvant si stocheaza intr-un dictionar propriu asocieri intre cuvant si indexul fisierului. Pentru ca un thread poate gasi de mai multe ori acelasi cuvant in acelasi fisier sau in fisiere diferite am folosit un set pentru a nu pastra duplicate si pentru a retine toti indexii valizi. Dupa ce citesc un fisier si stochez asocierile cuvant-indice, inchid fisierul si reiau procesul pana cand toate fisierele au fost procesate. Inainte de iesirea din thread astept terminarea tuturor thread-urilor de tip "map" si pornirea thread-urilor de tip "reduce".
- ### reduceFunc(void *arg)
    La inceputul functiei folosesc aceeasi bariera pentru a astepta terminarea tuturor thread-urilor "map". In interiorul acestei functii am procedat aproximativ la fel ca in cazul functie pentru mappers, doar ca acum in loc de fisiere **impart cate o litera in mod dinamic la cate un reducer**. In acest sens, toti reducers au acces la un index de litera "global" si la fel ca functia anterioara cat timp **indexul meu + litera 'a'** este mai mic sau egal decat litera 'z' aloc cate un index pt cate un thread si il **incrementez folosind la fel un mutex pentru a nu aparea race conditions**. Odata ce un thread reducer are un indice/litera valida **creez un dictionar de asocieri intre cuvintele care incep cu acea litera si lista indicilor fisierelor in care se afla** (la fel ca mai sus folosesc set si reuniune de set-uri pentru adaugarea noilor indexi in lista unui cuvant). Odata ce termin de procesat o litera, sortez dictionarul literei descrescator dupa lungimea set-ului de indexi, iar la egalitate, in ordine lexicografica dupa cuvant (cheie). Apoi deschid un fisier cu numele **"\<litera\>.txt"** si scriu asocierile dintre cuvant si indexi, pe cate un rand, in fisier. Repet procesul pana cand toate literele au fost procesate si pana cand toate fisierele au fost create sau scrise.

### La finalul main-ului distrug cele doua mutex-uri si bariera.