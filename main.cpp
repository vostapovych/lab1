#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <random>
#include <chrono>
#include <iomanip>
#include <cstdint>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;

const size_t MAX_WORD_LEN = 15;
const double MAX_LOAD = 1.0;
const size_t PRINT_LIMIT = 100;
const size_t INIT_CAPACITY = 1024;

// Запис у ланцюжку таблиці: рядок, 64-бітний хеш, кількість входжень
struct Record {
    string word;
    uint64_t hash_value;
    int count;
};

// Перевірка простоти з кроком 6k +- 1
bool is_prime(size_t n) {
    if (n <= 1) return false;
    if (n <= 3) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;
    for (size_t d = 5; d * d <= n; d += 6) {
        if (n % d == 0 || n % (d + 2) == 0) {
            return false;
        }
    }
    return true;
}

// Пошук наступного простого модуля для розміру таблиці
size_t next_prime(size_t n) {
    if (n <= 2) return 2;
    size_t cur = (n % 2 == 0) ? n + 1 : n;
    while (!is_prime(cur)) {
        cur += 2;
    }
    return cur;
}

// Поліноміальний хешер Рабіна-Карпа за схемою Горнера (за модулем 2^64)
class Hasher {
private:
    uint64_t base;

public:
    Hasher() {
        mt19937_64 rng(chrono::steady_clock::now().time_since_epoch().count());
        // p обирається непарним (НСД(p, 2^64) = 1) та p > max(код літери) = 122
        do {
            base = rng() | 1ULL;
        } while (base <= 122);
    }

    uint64_t compute(const string &s) const {
        uint64_t h = 0;
        for (char c : s) {
            h = h * base + (uint64_t)(unsigned char)c;
        }
        return h;
    }
};

// Хеш-таблиця з розв'язанням колізій методом ланцюгів
class HashTable {
private:
    vector<vector<Record>> chains;
    Hasher hasher;
    size_t total_elements;

    double load_factor() const {
        return (double)total_elements / (double)chains.size();
    }

    void rehash() {
        size_t new_size = next_prime(chains.size() * 2);
        vector<vector<Record>> new_chains(new_size);

        for (auto &chain : chains) {
            for (auto &record : chain) {
                size_t bucket_idx = record.hash_value % new_size;
                new_chains[bucket_idx].push_back(move(record));
            }
        }
        chains = move(new_chains);
    }

public:
    HashTable(size_t initial_size = INIT_CAPACITY) {
        chains.resize(next_prime(initial_size));
        total_elements = 0;
    }

    // Операція "+" (додавання рядка до мультимножини)
    void insert(const string &word) {
        uint64_t h = hasher.compute(word);
        size_t bucket_idx = h % chains.size();

        for (auto &record : chains[bucket_idx]) {
            if (record.hash_value == h && record.word == word) {
                record.count++;
                return;
            }
        }

        chains[bucket_idx].push_back({word, h, 1});
        total_elements++;

        if (load_factor() > MAX_LOAD) {
            rehash();
        }
    }

    // Операція "-" (вилучення однієї копії рядка)
    void remove(const string &word) {
        uint64_t h = hasher.compute(word);
        size_t bucket_idx = h % chains.size();
        auto &chain = chains[bucket_idx];

        for (size_t i = 0; i < chain.size(); ++i) {
            if (chain[i].hash_value == h && chain[i].word == word) {
                chain[i].count--;
                if (chain[i].count == 0) {
                    chain.erase(chain.begin() + i);
                    total_elements--;
                }
                return;
            }
        }
    }

    // Операція "?" (перевірка належності рядка)
    bool contains(const string &word) const {
        uint64_t h = hasher.compute(word);
        size_t bucket_idx = h % chains.size();

        for (const auto &record : chains[bucket_idx]) {
            if (record.hash_value == h && record.word == word) {
                return true;
            }
        }
        return false;
    }

    size_t size() const {
        return total_elements;
    }

    // Варіант 1: збір усіх рядків із кількістю повторень > 1
    vector<pair<string, int>> get_duplicates() const {
        vector<pair<string, int>> duplicates;
        for (const auto &chain : chains) {
            for (const auto &record : chain) {
                if (record.count > 1) {
                    duplicates.push_back({record.word, record.count});
                }
            }
        }
        sort(duplicates.begin(), duplicates.end());
        return duplicates;
    }
};

// Генерація випадкового латинського рядка довжиною 1..15
string generate_word(mt19937_64 &rng) {
    size_t len = (rng() % MAX_WORD_LEN) + 1;
    string word = "";
    word.reserve(len);
    for (size_t i = 0; i < len; ++i) {
        word += (char)('a' + (rng() % 26));
    }
    return word;
}

// Виведення списку повторюваних рядків
void print_duplicates(const HashTable &table, bool print_all) {
    auto duplicates = table.get_duplicates();
    cout << "\n=======================================================\n";
    cout << "           ПОВТОРЮВАНІ РЯДКИ (ДУБЛІКАТИ)              \n";
    cout << "=======================================================\n";
    cout << "Всього унікальних повторюваних слів: " << duplicates.size() << "\n";

    if (duplicates.empty()) {
        cout << "Повторюваних слів (з кількістю входжень > 1) не знайдено.\n";
        cout << "=======================================================\n";
        return;
    }

    size_t limit = print_all ? duplicates.size() : min(duplicates.size(), PRINT_LIMIT);
    cout << left << setw(20) << "Рядок" << setw(15) << "Кількість" << "\n";
    cout << "-------------------------------------------------------\n";
    for (size_t i = 0; i < limit; ++i) {
        cout << left << setw(20) << duplicates[i].first
             << setw(15) << duplicates[i].second << "\n";
    }

    if (!print_all && duplicates.size() > PRINT_LIMIT) {
        cout << "... ще " << (duplicates.size() - PRINT_LIMIT)
             << " слів приховано (введіть #all, щоб вивести всі).\n";
    }
    cout << "=======================================================\n";
}

// Запуск генератора операцій
void run_generator(HashTable &table, size_t dict_sz, size_t ops_cnt, uint64_t seed) {
    mt19937_64 rng(seed);

    vector<string> dict;
    dict.reserve(dict_sz);
    for (size_t i = 0; i < dict_sz; ++i) {
        dict.push_back(generate_word(rng));
    }

    size_t cnt_add = 0, cnt_del = 0, cnt_check = 0;
    size_t yes_ans = 0, no_ans = 0;

    auto start_time = chrono::steady_clock::now();

    for (size_t i = 0; i < ops_cnt; ++i) {
        size_t r = rng() % 100;
        const string &word = dict[rng() % dict_sz];

        if (r < 70) {
            table.insert(word);
            cnt_add++;
        } else if (r < 80) {
            table.remove(word);
            cnt_del++;
        } else {
            if (table.contains(word)) {
                yes_ans++;
            } else {
                no_ans++;
            }
            cnt_check++;
        }
    }

    auto end_time = chrono::steady_clock::now();
    double elapsed_ms = chrono::duration<double, milli>(end_time - start_time).count();
    double us_per_op = (elapsed_ms * 1000.0) / (double)ops_cnt;

    cout << "\n------------------ Результати тесту ------------------\n";
    cout << "Параметри: словник = " << dict_sz << " слів, операцій = " << ops_cnt << ", зерно = " << seed << "\n";
    cout << "Виконано операцій:\n";
    cout << "  [+] Додавання       : " << cnt_add << "\n";
    cout << "  [-] Вилучення       : " << cnt_del << "\n";
    cout << "  [?] Перевірка       : " << cnt_check
         << " (знайдено [yes]: " << yes_ans << ", не знайдено [no]: " << no_ans << ")\n";
    cout << "Час роботи:\n";
    cout << "  Загальний час       : " << fixed << setprecision(3) << elapsed_ms << " мс\n";
    cout << "  Середній на дію     : " << fixed << setprecision(3) << us_per_op << " мкс/оп\n";
    cout << "Стан таблиці:\n";
    cout << "  Унікальних слів     : " << table.size() << "\n";
    cout << "------------------------------------------------------\n";
}

// Стартове меню з виведенням в консоль
void print_help() {
    cout << "=====================================================================\n";
    cout << "   ЛАБОРАТОРНА РОБОТА № 2: МНОЖИНА РЯДКІВ ТА ПОЛІНОМІАЛЬНИЙ ХЕШ     \n";
    cout << "=====================================================================\n";
    cout << "ДОСТУПНІ ОПЕРАЦІЇ ТА КОМАНДИ:\n\n";
    cout << " 1. Операції над структурою даних:\n";
    cout << "    + <рядок>    - Додати рядок (наприклад: + apple)\n";
    cout << "    - <рядок>    - Вилучити рядок (наприклад: - apple)\n";
    cout << "    ? <рядок>    - Перевірити наявність (виводить yes / no)\n\n";
    cout << " 2. Завершення роботи та підсумок (Варіант 1):\n";
    cout << "    #            - Завершити та вивести до 100 дублікатів\n";
    cout << "    #all         - Завершити та вивести всі знайдені дублікати\n\n";
    cout << " 3. Автоматичне тестування продуктивності:\n";
    cout << "    gen <N_слів> <Q_операцій> [seed]\n";
    cout << "                 - Виконати Q операцій над словником із N слів\n";
    cout << "                   (Приклад: gen 100000 100000 42)\n";
    cout << "    bench        - Автоматичний тест для 10^4, 10^5 та 10^6 операцій\n";
    cout << "    help         - Показати це меню знову\n";
    cout << "=====================================================================\n" << flush;
}

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    // Прискорення вводу/виводу, але зі збереженням автоматичного скидання буфера перед вводом
    ios_base::sync_with_stdio(false);
    cin.tie(&cout);

    print_help();

    HashTable table;
    string line;

    // Виводимо запрошення перед першою командою
    cout << "Введіть команду > " << flush;

    while (getline(cin, line)) {
        if (line.empty()) {
            cout << "Введіть команду > " << flush;
            continue;
        }

        stringstream ss(line);
        string cmd;
        ss >> cmd;

        if (cmd == "#" || cmd == "#all") {
            print_duplicates(table, cmd == "#all");
            break;
        }

        if (cmd == "help") {
            print_help();
        } else if (cmd == "+") {
            string word;
            if (ss >> word) {
                table.insert(word);
            } else {
                cout << "Помилка: вкажіть слово після '+'\n";
            }
        } else if (cmd == "-") {
            string word;
            if (ss >> word) {
                table.remove(word);
            } else {
                cout << "Помилка: вкажіть слово після '-'\n";
            }
        } else if (cmd == "?") {
            string word;
            if (ss >> word) {
                cout << (table.contains(word) ? "yes\n" : "no\n");
            } else {
                cout << "Помилка: вкажіть слово після '?'\n";
            }
        } else if (cmd == "gen") {
            size_t dict_sz = 0, ops_cnt = 0;
            if (ss >> dict_sz >> ops_cnt) {
                if (dict_sz == 0 || ops_cnt == 0) {
                    cout << "Помилка: розмір словника та кількість операцій мають бути > 0\n";
                } else {
                    uint64_t seed = 42;
                    uint64_t custom_seed;
                    if (ss >> custom_seed) {
                        seed = custom_seed;
                    }
                    run_generator(table, dict_sz, ops_cnt, seed);
                }
            } else {
                cout << "Формат: gen <розмір_словника> <кількість_операцій> [seed]\n";
            }
        } else if (cmd == "bench") {
            cout << "\n=======================================================\n";
            cout << "       ЗАПУСК БЕНЧМАРКУ ДЛЯ ЗВІТУ (10^4, 10^5, 10^6)   \n";
            cout << "=======================================================\n";
            HashTable t1, t2, t3;
            cout << "\n>>> Тест 1: 10^4 операцій";
            run_generator(t1, 10000, 10000, 42);
            cout << "\n>>> Тест 2: 10^5 операцій";
            run_generator(t2, 100000, 100000, 42);
            cout << "\n>>> Тест 3: 10^6 операцій";
            run_generator(t3, 1000000, 1000000, 42);
        } else {
            cout << "Невідома команда: '" << cmd << "'. Введіть 'help' для перегляду списку команд.\n";
        }

        // Запрошення до наступної команди зі скиданням буфера
        cout << "\nВведіть команду > " << flush;
    }

    return 0;
}