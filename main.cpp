#include <iostream>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <pthread.h>
#include <algorithm>

using namespace std;

// Intrare in vectorul de fisiere
struct inputFisiere {
    int id_fis;
    string nume_fisier;
};

// Argument al unui thread de tip "mapper"
struct inputMapper {
    int id_thread;
    vector<map<string, set<int>>> *map_lists;
    vector<inputFisiere> fisiere;
    int *indexFisier;
    pthread_mutex_t *map_mutex;
    int n_fis;
    pthread_barrier_t *bariera;
};

// Argument al unui thread de tip "reduce"
struct inputReducer {
    int id_thread;
    vector<map<string, set<int>>> *map_lists;
    int *indexLitera;
    pthread_mutex_t *reduce_mutex;
    pthread_barrier_t *bariera;
};

// Functie de comparare descrescator dupa lungimea celui
// de-al doilea element dintr-o pereche si lexicografic
// dupa primul in caz de egalitate
bool fcmp(pair<string, set<int>> a, pair<string, set<int>> b) {
    if (a.second.size() != b.second.size()) {
        return a.second.size() > b.second.size();
    } else {
        return a.first < b.first;
    }
}

// Functia executata de mappers
void *mapFunc(void *arg) {
    inputMapper m = *((inputMapper *)arg);
    int index_fisier;

    // Simularea unei cozi de fisiere
    while (*(m.indexFisier) < m.n_fis) {
        pthread_mutex_lock(m.map_mutex);
        index_fisier = *(m.indexFisier);
        *(m.indexFisier) += 1;
        pthread_mutex_unlock(m.map_mutex);

        // Obtinere cale/nume fisier
        string nume_fisier = (m.fisiere)[index_fisier].nume_fisier;
        ifstream fin(nume_fisier);
        if (!fin.is_open()) {
            return NULL;
        }

        string cuv;
        // Referinta catre dictionarul de cuvinte-indexi alocat thread-ului curent
        map<string, set<int>> &map_t = (*m.map_lists)[m.id_thread];

        // Citire cuvinte pe rand
        while (fin >> cuv) {
            // Eliminare caractere non-alfabetice (.,'"?!-_ etc.)
            // si transformare litere mari in litere mici
            for (int i = 0; i < cuv.length(); i++) {
                if (!(cuv[i] >= 'a' && cuv[i] <= 'z')) {
                    if (cuv[i] >= 'A' && cuv[i] <= 'Z')
                        cuv[i] = cuv[i] + 32;
                    else {
                        cuv.erase(cuv.begin() + i);
                        i--;
                    }
                }
            }
            // Adaugare intrare in dictionar/adaugare
            // index nou pentru un cuvant deja existent
            map_t[cuv].insert(index_fisier);
        }

        fin.close();
    }
    // Bariera pentru sincronizarea intre thread-urile de tip "map" si "reduce"
    pthread_barrier_wait(m.bariera);
    pthread_exit(NULL);
}

// Functia executata de reducers
void *reduceFunc(void *arg) {
    inputReducer r = *(inputReducer*)arg;

    // Bariera pentru sincronizarea intre thread-urile de tip "map" si "reduce"
    pthread_barrier_wait(r.bariera);

    char litera = 'a';
    int index_litera;

    // Alocare dinamica a literelor catre thread-uri 
    while (litera + *(r.indexLitera) <= 'z') {
        pthread_mutex_lock(r.reduce_mutex);
        index_litera = *(r.indexLitera);
        *(r.indexLitera) += 1;
        pthread_mutex_unlock(r.reduce_mutex);

        // Dictionar auxiliar pentru cuvintele care incep cu litera 'a' + index_litera
        map<string, set<int>> cuvinte_litera;

        // Iterare prin toate dictionarele thread-urilor mappers
        // si adaugarea cuvintelor in dictionarul auxiliar pentru litera
        for (auto lista_partiala : (*r.map_lists)) {
            for (auto cuvant : lista_partiala) {
                if (cuvant.first[0] == litera + index_litera) {
                    if (cuvinte_litera.find(cuvant.first) == cuvinte_litera.end()) {
                        // Cuvantul curent nu se afla in dictionar
                        cuvinte_litera[cuvant.first] = cuvant.second;
                    } else {
                        // Cuvantul este deja in dictionar
                        // In acest sens fac reuniunea set-urilor
                        for (auto index : cuvant.second) {
                            cuvinte_litera[cuvant.first].insert(index);
                        }
                    }
                }
            }
        }

        // Vector pentru a retine intrarile si pentru a putea fi sortat
        // deoarece map-ul nu poate fi sortat
        vector<pair<string, set<int>>> lista_agregata;

        for (auto intrare : cuvinte_litera) {
            lista_agregata.push_back(intrare);
        }
        
        // Sortare
        sort(lista_agregata.begin(), lista_agregata.end(), fcmp);

        // Calea catre fisierul de tip "<litera = ('a' + index_litera)>.txt"
        char litera_fisier = litera + index_litera;
        string extensie = ".txt";
        string fisier_out = litera_fisier + extensie;
        ofstream fout(fisier_out);

        // Scriere in fisier a asocierilor cuvant-indexi
        for (auto cuv : lista_agregata) {
            fout << cuv.first << ": [";
            int i = 0;
            int max_len = cuv.second.size();
            for (auto index : cuv.second) {
                fout << index + 1;
                if (i < max_len - 1) {
                    fout << " ";
                }
                i++;
            }
            fout << "]" << endl;
        }
        fout.close();

    }
    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    if (argc < 4)
        return -1;

    // Stocare parametrii din linia de comanda
    const int n_mapper = atoi(argv[1]);
    const int n_reducer = atoi(argv[2]);
    string NumeFisier = argv[3];

    // Initializare structuri
    vector<map<string, set<int>>> map_lists(n_mapper);
    
    // Citire din fisierul de intrare
    ifstream fin(NumeFisier);
    string fis_temp;
    inputFisiere fis;

    getline(fin, fis_temp);
    int n_fis = stoi(fis_temp);
    vector<inputFisiere> fisiere(n_fis);

    for (int i = 0; i < n_fis; i++) {
        getline(fin, fis_temp);
        if (fis_temp.back() == '\n' || fis_temp.back() == '\r')
            fis_temp.pop_back();
        fis.id_fis = i;
        fis.nume_fisier = fis_temp;
        fisiere[i] = fis;
    }

    fin.close();

    // Initializare thread-uri si mutexi si bariera
    pthread_t map_reduce_th[n_mapper + n_reducer];
    pthread_barrier_t bariera;
    pthread_barrier_init(&bariera, NULL, n_mapper + n_reducer);

    int r;
    void *status;

    pthread_mutex_t map_mutex;
    pthread_mutex_t reduce_mutex;
    pthread_mutex_init(&map_mutex, NULL);
    pthread_mutex_init(&reduce_mutex, NULL);

    // Argumente pentru mapperi
    vector<inputMapper> args_mappers(n_mapper);
    vector<inputReducer> args_reducers(n_reducer);
    int indexFisier = 0;
    int indexLitera = 0;

    // Creare thread-uri
    for (int i = 0; i < n_mapper + n_reducer; i++) {
        if (i < n_mapper) {
            args_mappers[i].id_thread = i;
            args_mappers[i].fisiere = fisiere;
            args_mappers[i].indexFisier = &indexFisier;
            args_mappers[i].map_lists = &map_lists;
            args_mappers[i].map_mutex = &map_mutex;
            args_mappers[i].n_fis = n_fis;
            args_mappers[i].bariera = &bariera;
            r = pthread_create(&map_reduce_th[i], NULL, mapFunc, (void*)&args_mappers[i]);
            if (r) {
                exit(-1);
            }
        }
        else {
            args_reducers[i - n_mapper].id_thread = i;
            args_reducers[i - n_mapper].indexLitera = &indexLitera;
            args_reducers[i - n_mapper].map_lists = &map_lists;
            args_reducers[i - n_mapper].reduce_mutex = &reduce_mutex;
            args_reducers[i - n_mapper].bariera = &bariera;
            r = pthread_create(&map_reduce_th[i], NULL, reduceFunc, (void*)&args_reducers[i - n_mapper]);
            if (r) {
                exit(-1);
            }
        }
    }

    // Pornire thread-uri
    for (int i = 0; i < n_mapper + n_reducer; i++) {
        r = pthread_join(map_reduce_th[i], &status);
        if (r) {
            exit(-1);
        }
    }

    // Distrugere mutexi si bariere
    pthread_mutex_destroy(&map_mutex);
    pthread_mutex_destroy(&reduce_mutex);
    pthread_barrier_destroy(&bariera);
    
    return 0;
}