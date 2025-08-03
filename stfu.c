#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>

// Константы для оптимизации
#define MAX_CMD_SIZE 1024
#define MAX_BUFFER_SIZE 2048
#define MAX_QUOTE_SIZE 768
#define TERMINAL_MIN_WIDTH 20
#define DEFAULT_TERMINAL_WIDTH 80
#define MAX_APIS 3
#define MAX_QUOTES 10
#define URL_ENCODE_FACTOR 3

// Структура для переводов (более компактная)
typedef struct {
    const char* const usage;
    const char* const options;
    const char* const examples;
    const char* const home_desc;
    const char* const sudo_desc;
    const char* const help_desc;
    const char* const error_root;
    const char* const error_home_arg;
    const char* const error_unknown;
} translations_t;

// Оптимизированные переводы (const для размещения в read-only памяти)
static const translations_t translations[] = {
    // English (default)
    {"Usage: stfu [options] <command> [args...]", "Options:", "Examples:", 
     "Set custom HOME directory", "Execute as root (like sudo)", "Show this help",
     "Error: This program must be run as root or installed with SUID bit",
     "Error: --home requires a path argument", "I don't know what the problem is, you're on your own now."},
    // Russian
    {"Использование: stfu [опции] <команда> [аргументы...]", "Опции:", "Примеры:",
     "Установить пользовательский каталог HOME", "Выполнить как root (как sudo)", "Показать эту справку",
     "Ошибка: Эта программа должна запускаться от имени root или с SUID битом",
     "Ошибка: --home требует аргумент пути", "Я не знаю в чём проблема, теперь ты сам за себя."},
    // Ukrainian  
    {"Використання: stfu [опції] <команда> [аргументи...]", "Опції:", "Приклади:",
     "Встановити користувацький каталог HOME", "Виконати як root (як sudo)", "Показати цю довідку",
     "Помилка: Ця програма повинна запускатися від імені root або з SUID бітом",
     "Помилка: --home потребує аргумент шляху", "Я не знаю в чому проблема, тепер ти сам за себе."},
    // French
    {"Usage: stfu [options] <commande> [args...]", "Options:", "Exemples:",
     "Définir un répertoire HOME personnalisé", "Exécuter en tant que root (comme sudo)", "Afficher cette aide",
     "Erreur: Ce programme doit être exécuté en tant que root ou installé avec le bit SUID",
     "Erreur: --home nécessite un argument de chemin", "Je ne sais pas quel est le problème, tu te débrouilles maintenant."},
    // German
    {"Verwendung: stfu [optionen] <befehl> [args...]", "Optionen:", "Beispiele:",
     "Benutzerdefinierten HOME-Ordner festlegen", "Als root ausführen (wie sudo)", "Diese Hilfe anzeigen",
     "Fehler: Dieses Programm muss als root ausgeführt oder mit SUID-Bit installiert werden",
     "Fehler: --home benötigt ein Pfad-Argument", "Ich weiß nicht, was das Problem ist, jetzt bist du auf dich gestellt."},
    // Spanish
    {"Uso: stfu [opciones] <comando> [args...]", "Opciones:", "Ejemplos:",
     "Establecer directorio HOME personalizado", "Ejecutar como root (como sudo)", "Mostrar esta ayuda",
     "Error: Este programa debe ejecutarse como root o instalarse con bit SUID",
     "Error: --home requiere un argumento de ruta", "No sé cuál es el problema, ahora estás por tu cuenta."},
    // Finnish
    {"Käyttö: stfu [asetukset] <komento> [args...]", "Asetukset:", "Esimerkit:",
     "Aseta mukautettu HOME-hakemisto", "Suorita root-käyttäjänä (kuten sudo)", "Näytä tämä ohje",
     "Virhe: Tämä ohjelma on suoritettava root-käyttäjänä tai asennettava SUID-bitillä",
     "Virhe: --home vaatii polku-argumentin", "En tiedä mikä ongelma on, nyt olet omillasi."},
    // Italian
    {"Uso: stfu [opzioni] <comando> [args...]", "Opzioni:", "Esempi:",
     "Imposta directory HOME personalizzata", "Esegui come root (come sudo)", "Mostra questo aiuto",
     "Errore: Questo programma deve essere eseguito come root o installato con bit SUID",
     "Errore: --home richiede un argomento percorso", "Non so quale sia il problema, ora sei da solo."},
    // Bulgarian
    {"Употреба: stfu [опции] <команда> [args...]", "Опции:", "Примери:",
     "Задай потребителска HOME директория", "Изпълни като root (като sudo)", "Покажи тази помощ",
     "Грешка: Тази програма трябва да се стартира като root или да се инсталира с SUID бит",
     "Грешка: --home изисква аргумент за път", "Не знам какъв е проблемът, сега си сам."}
};

// Глобальные переменные (минимизированы)
static char *custom_home = NULL;
static const translations_t *t = &translations[0];

// Оптимизированный обработчик ошибок
static void __attribute__((noreturn)) error_handler(int sig) {
    (void)sig; // Избегаем предупреждения о неиспользуемом параметре
    puts(t->error_unknown);
    _exit(1);
}

// Быстрое получение ширины терминала
static inline int get_terminal_width(void) {
    FILE * const fp = popen("tput cols 2>/dev/null", "r");
    int width = DEFAULT_TERMINAL_WIDTH;
    
    if (__builtin_expect(fp != NULL, 1)) {
        if (fscanf(fp, "%d", &width) != 1) 
            width = DEFAULT_TERMINAL_WIDTH;
        pclose(fp);
    }
    
    return width > TERMINAL_MIN_WIDTH ? width : DEFAULT_TERMINAL_WIDTH;
}

// Оптимизированная установка локали
static inline void set_locale(void) {
    const char * const lang = getenv("LANG") ?: getenv("LC_ALL");
    if (__builtin_expect(lang == NULL, 0)) return;
    
    // Используем jump table для быстрого переключения
    static const struct {
        const char first, second;
        const translations_t *trans;
    } lang_map[] = {
        {'r', 'u', &translations[1]}, // Russian
        {'u', 'k', &translations[2]}, // Ukrainian  
        {'f', 'r', &translations[3]}, // French
        {'d', 'e', &translations[4]}, // German
        {'e', 's', &translations[5]}, // Spanish
        {'f', 'i', &translations[6]}, // Finnish
        {'i', 't', &translations[7]}, // Italian
        {'b', 'g', &translations[8]}  // Bulgarian
    };
    
    const char first = lang[0];
    const char second = lang[1];
    
    for (size_t i = 0; i < sizeof(lang_map)/sizeof(lang_map[0]); ++i) {
        if (__builtin_expect(lang_map[i].first == first && lang_map[i].second == second, 0)) {
            t = lang_map[i].trans;
            return;
        }
    }
}

// Оптимизированное URL-кодирование (только необходимые символы)
static char* url_encode_minimal(const char* const quote) {
    const size_t len = strlen(quote);
    char * const encoded = malloc(len * URL_ENCODE_FACTOR + 1);
    
    if (__builtin_expect(!encoded, 0)) return NULL;
    
    const char *src = quote;
    char *dst = encoded;
    
    while (*src) {
        switch (*src) {
            case ' ':
                *dst++ = '%'; *dst++ = '2'; *dst++ = '0';
                break;
            case '"':
                *dst++ = '%'; *dst++ = '2'; *dst++ = '2';
                break;
            default:
                *dst++ = *src;
        }
        ++src;
    }
    *dst = '\0';
    
    return encoded;
}

// Быстрый парсер JSON для API ответов
static char* extract_translation(const char* const buffer, const int api_idx) {
    const char *start, *end;
    
    if (api_idx == 0) {
        // Google API: [[["translated"
        start = strstr(buffer, "[[[\"");
        if (!start) return NULL;
        start += 4;
        end = strstr(start, "\",");
    } else {
        // MyMemory API: "translatedText":"..."
        start = strstr(buffer, "\"translatedText\":\"");
        if (!start) return NULL;
        start += 18;
        end = strstr(start, "\",");
    }
    
    if (!end || end <= start) return NULL;
    
    const size_t len = end - start;
    char * const result = malloc(len + 1);
    if (!result) return NULL;
    
    memcpy(result, start, len);
    result[len] = '\0';
    
    return result;
}

// Оптимизированный переводчик с fallback
static char* translate_quote(const char* const quote, const char* const target_lang) {
    if (strncmp(target_lang, "en", 2) == 0) return strdup(quote);
    
    char * const encoded_quote = url_encode_minimal(quote);
    if (!encoded_quote) return strdup(quote);
    
    // Оптимизированные API команды
    static const char* const api_templates[] = {
        "curl -s --max-time 4 --connect-timeout 2 'https://translate.googleapis.com/translate_a/single?client=gtx&sl=en&tl=%s&dt=t&q=%s' 2>/dev/null",
        "curl -s --max-time 4 --connect-timeout 2 'https://api.mymemory.translated.net/get?q=%s&langpair=en|%s' 2>/dev/null"
    };
    
    for (int api_idx = 0; api_idx < 2; ++api_idx) {
        char cmd[MAX_CMD_SIZE];
        
        const int ret = (api_idx == 0) 
            ? snprintf(cmd, sizeof(cmd), api_templates[api_idx], target_lang, encoded_quote)
            : snprintf(cmd, sizeof(cmd), api_templates[api_idx], encoded_quote, target_lang);
        
        if (ret >= (int)sizeof(cmd)) continue; // Команда слишком длинная
        
        FILE * const fp = popen(cmd, "r");
        if (!fp) continue;
        
        char buffer[MAX_BUFFER_SIZE];
        if (fgets(buffer, sizeof(buffer), fp)) {
            char * const translated = extract_translation(buffer, api_idx);
            pclose(fp);
            
            if (translated && strcmp(translated, quote) != 0) {
                free(encoded_quote);
                return translated;
            }
            free(translated);
        } else {
            pclose(fp);
        }
    }
    
    free(encoded_quote);
    return strdup(quote);
}

// Элегантное форматирование цитат
static inline void format_quote(const char* const quote, const char* const author, 
                                const char* const context, const char* const source) {
    const int term_width = get_terminal_width();
    
    // Жирная цитата с красивыми кавычками
    printf("\033[1m— \u201E%s\u201C\033[0m\n", quote);
    
    // Зелёный автор, выровненный по правому краю
    char author_text[128];
    const int author_len = snprintf(author_text, sizeof(author_text), "— %s", author);
    const int author_pos = (term_width > author_len) ? term_width - author_len : 0;
    
    printf("%*s\033[32m%s\033[0m\n\n", author_pos, "", author_text);
    
    // Серые контекст и источник
    printf("\033[90m%s\n%s\033[0m\n", context, source);
}

// Быстрая проверка сетевого соединения
static inline int check_network(void) {
    const int ret = system("ping -c 1 -W 1 8.8.8.8 >/dev/null 2>&1");
    return WIFEXITED(ret) && WEXITSTATUS(ret) == 0;
}

// Оптимизированный парсер JSON для цитат
static char* parse_quote_json(const char* const buffer) {
    const char * const results_start = strstr(buffer, "\"results\":[");
    if (!results_start) return NULL;
    
    // Быстрый поиск случайной цитаты
    srand(time(NULL) ^ getpid()); // Лучшая энтропия
    
    const char *current = results_start + 11;
    const char *quote_positions[MAX_QUOTES];
    int quote_count = 0;
    
    // Собираем позиции всех цитат
    while (quote_count < MAX_QUOTES) {
        const char * const quote_start = strstr(current, "{\"");
        if (!quote_start) break;
        quote_positions[quote_count++] = quote_start;
        current = quote_start + 2;
    }
    
    if (quote_count == 0) return NULL;
    
    // Выбираем случайную цитату
    const char * const selected_quote = quote_positions[rand() % quote_count];
    
    const char * const content_start = strstr(selected_quote, "\"content\":\"");
    const char * const author_start = strstr(selected_quote, "\"author\":\"");
    
    if (!content_start || !author_start) return NULL;
    
    const char *content_ptr = content_start + 11;
    const char * const content_end = strstr(content_ptr, "\",");
    
    const char *author_ptr = author_start + 10;
    const char * const author_end = strstr(author_ptr, "\",");
    
    if (!content_end || !author_end) return NULL;
    
    const size_t content_len = content_end - content_ptr;
    const size_t author_len = author_end - author_ptr;
    
    char * const result = malloc(MAX_QUOTE_SIZE);
    if (!result) return NULL;
    
    snprintf(result, MAX_QUOTE_SIZE, "%.*s|%.*s|Various speeches and writings|Quotable API", 
             (int)content_len, content_ptr, (int)author_len, author_ptr);
    
    return result;
}

// Получение онлайн цитаты с улучшенной обработкой ошибок
static char* get_online_quote(void) {
    if (!check_network()) return NULL;
    
    static const char* const apis[] = {
        "curl -s --max-time 3 --connect-timeout 1 'https://quotable.io/quotes?minLength=80&tags=technology,wisdom&limit=10' 2>/dev/null",
        "curl -s --max-time 3 --connect-timeout 1 'https://quotable.io/quotes?minLength=60&tags=science&limit=10' 2>/dev/null",
        "curl -s --max-time 3 --connect-timeout 1 'https://quotable.io/quotes?minLength=70&limit=10' 2>/dev/null"
    };
    
    srand(time(NULL) ^ getpid());
    const int api_index = rand() % MAX_APIS;
    
    FILE * const fp = popen(apis[api_index], "r");
    if (!fp) return NULL;
    
    char buffer[MAX_BUFFER_SIZE * 2]; // Больший буфер для JSON
    char *result = NULL;
    
    if (fgets(buffer, sizeof(buffer), fp)) {
        result = parse_quote_json(buffer);
    }
    
    pclose(fp);
    return result;
}

// Компактная функция показа случайной цитаты
static void show_random_quote(void) {
    static const char* const local_quotes[] = {
        "Free software is a matter of liberty, not price. To understand the concept, you should think of 'free' as in 'free speech,' not as in 'free beer'|Richard Stallman|GNU Project announcement, 1983|Free Software Foundation",
        "Most good programmers do programming not because they expect to get paid or get adulation by the public, but because it is fun to program|Linus Torvalds|Interview about Linux development, 1991|Linux Journal",
        "The use of COBOL cripples the mind; its teaching should, therefore, be regarded as a criminal offense|Edsger Dijkstra|How do we tell truths that might hurt?, 1975|ACM SIGPLAN Notices",
        "Programs must be written for people to read, and only incidentally for machines to execute. The source of the intellectual content is the key|Harold Abelson|Structure and Interpretation of Computer Programs, 1984|MIT Press",
        "Any fool can write code that a computer can understand. Good programmers write code that humans can understand. The real challenge is making it maintainable|Martin Fowler|Refactoring: Improving the Design of Existing Code, 1999|Addison-Wesley",
        "Debugging is twice as hard as writing the code in the first place. Therefore, if you write the code as cleverly as possible, you are not smart enough to debug it|Brian Kernighan|The Elements of Programming Style, 1974|McGraw-Hill",
        "The best way to get a project done faster is to start sooner. Time spent in planning and design saves exponentially more time during implementation|Jim Highsmith|Agile Project Management, 2004|Addison-Wesley",
        "Walking on water and developing software from a specification are easy if both are frozen. The challenge comes when requirements change|Edward V. Berard|Essays on Object-Oriented Software Engineering, 1993|Prentice Hall",
        "Intelligence is the ability to avoid doing work, yet getting the work done. This is the essence of good system design and automation|Linus Torvalds|Various interviews, 1990s|Linux community",
        "Perfection is achieved not when there is nothing more to add, but rather when there is nothing more to take away. Simplicity is the ultimate sophistication|Antoine de Saint-Exupery|Wind, Sand and Stars, 1939|Reynal & Hitchcock"
    };
    
    const char * const lang = getenv("LANG") ?: "en";
    char * const online_quote = get_online_quote();
    const char * const quote_data = online_quote ?: local_quotes[rand() % MAX_QUOTES];
    
    char * const copy = strdup(quote_data);
    if (__builtin_expect(!copy, 0)) {
        fputs("\033[1;31m[ERROR]\033[0m Memory allocation failed\n", stderr);
        free(online_quote);
        return;
    }
    
    char * const quote = strtok(copy, "|");
    char * const author = strtok(NULL, "|");
    char * const context = strtok(NULL, "|");
    char * const source = strtok(NULL, "|");
    
    if (__builtin_expect(quote && author && context && source, 1)) {
        if (strncmp(lang, "en", 2) != 0) {
            const char target_lang[3] = {lang[0], lang[1], '\0'};
            char * const translated_quote = translate_quote(quote, target_lang);
            format_quote(translated_quote, author, context, source);
            free(translated_quote);
        } else {
            format_quote(quote, author, context, source);
        }
    } else {
        fputs("\033[1;31m[ERROR]\033[0m Quote parsing failed\n", stderr);
    }
    
    free(copy);
    free(online_quote);
}

// Стильная справка
static inline void show_help(void) {
    show_random_quote();
    putchar('\n');
    
    const int term_width = get_terminal_width();
    for (int i = 0; i < term_width; ++i) putchar('-');
    
    printf("\n\n%s\n\n%s\n", t->usage, t->options);
    printf("  -H, --home <path>    %s\n", t->home_desc);
    printf("  -s, --sudo           %s\n", t->sudo_desc);
    printf("  -h, --help           %s\n\n", t->help_desc);
    printf("%s\n", t->examples);
    puts("  stfu firefox");
    puts("  stfu -H /tmp/safehome firefox");
    puts("  stfu -s firefox");
    puts("  stfu yay -S package");
    puts("  stfu code /etc/hosts");
}

// Оптимизированная генерация fake библиотеки
static void create_fake_lib(void) {
    // Оптимизированный код библиотеки в одной строке
    static const char fake_lib_code[] = 
        "#define _GNU_SOURCE\n"
        "#include <sys/types.h>\n#include <unistd.h>\n#include <pwd.h>\n"
        "#include <stdlib.h>\n#include <string.h>\n#include <dlfcn.h>\n"
        "uid_t getuid(void){return 1000;}uid_t geteuid(void){return 1000;}"
        "gid_t getgid(void){return 1000;}gid_t getegid(void){return 1000;}"
        "struct passwd*getpwuid(uid_t u){static struct passwd p={\"user\",\"x\",1000,1000,\"Regular User\",\"/home/user\",\"/bin/bash\"};"
        "char*h=getenv(\"STFU_CUSTOM_HOME\");if(h)p.pw_dir=h;return&p;}"
        "char*getlogin(void){return\"user\";}"
        "int access(const char*p,int m){static int(*r)(const char*,int)=0;"
        "if(!r)r=dlsym(RTLD_NEXT,\"access\");"
        "if(p&&strstr(p,\"/snap\")&&strstr(p,\"firefox\"))return-1;"
        "return r(p,m);}";
    
    // Атомарная запись файла
    const int fd = open("/tmp/stfu_fake.c", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (__builtin_expect(fd == -1, 0)) {
        puts(t->error_unknown);
        _exit(1);
    }
    
    const ssize_t written = write(fd, fake_lib_code, sizeof(fake_lib_code) - 1);
    close(fd);
    
    if (__builtin_expect(written != sizeof(fake_lib_code) - 1, 0)) {
        puts(t->error_unknown);
        _exit(1);
    }
    
    // Быстрая компиляция с оптимизацией
    if (__builtin_expect(system("gcc -shared -fPIC -O2 -ldl /tmp/stfu_fake.c -o /tmp/stfu_fake.so 2>/dev/null") != 0, 0)) {
        puts(t->error_unknown);
        _exit(1);
    }
}

// Быстрая очистка
static inline void cleanup(void) {
    unlink("/tmp/stfu_fake.c");
    unlink("/tmp/stfu_fake.so");
}

// Оптимизированный main
int main(int argc, char *argv[]) {
    // Быстрая инициализация
    set_locale();
    
    // Установка обработчиков сигналов
    signal(SIGABRT, error_handler);
    signal(SIGSEGV, error_handler);
    signal(SIGFPE, error_handler);
    
    int arg_start = 1;
    int sudo_mode = 0;
    
    // Оптимизированный парсинг аргументов
    while (arg_start < argc && argv[arg_start][0] == '-') {
        const char * const arg = argv[arg_start];
        
        if (__builtin_expect(strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0, 0)) {
            show_help();
            return 0;
        } else if (strcmp(arg, "-H") == 0 || strcmp(arg, "--home") == 0) {
            if (__builtin_expect(++arg_start >= argc, 0)) {
                puts(t->error_home_arg);
                return 1;
            }
            custom_home = argv[arg_start++];
        } else if (strcmp(arg, "-s") == 0 || strcmp(arg, "--sudo") == 0) {
            sudo_mode = 1;
            ++arg_start;
        } else {
            ++arg_start;
            break;
        }
    }
    
    if (__builtin_expect(arg_start >= argc, 0)) {
        show_help();
        return 0;
    }
    
    // Обработка sudo режима
    if (sudo_mode && getuid() != 0) {
        char ** const sudo_args = malloc((argc + 5) * sizeof(char*));
        if (__builtin_expect(!sudo_args, 0)) {
            puts(t->error_unknown);
            return 1;
        }
        
        int i = 0;
        sudo_args[i++] = "sudo";
        sudo_args[i++] = argv[0];
        
        // Копируем аргументы, исключая -s
        for (int j = 1; j < arg_start; ++j) {
            if (strcmp(argv[j], "-s") != 0 && strcmp(argv[j], "--sudo") != 0) {
                sudo_args[i++] = argv[j];
            }
        }
        
        // Копируем команду
        for (int j = arg_start; j < argc; ++j) {
            sudo_args[i++] = argv[j];
        }
        sudo_args[i] = NULL;
        
        execvp("sudo", sudo_args);
        puts(t->error_unknown);
        return 1;
    }
    
    // Проверка прав root
    if (!sudo_mode && getuid() != 0) {
        if (geteuid() == 0) {
            if (__builtin_expect(setuid(0) != 0, 0)) {
                puts(t->error_unknown);
                return 1;
            }
        } else {
            puts(t->error_root);
            return 1;
        }
    }
    
    // Создаем fake библиотеку
    create_fake_lib();
    atexit(cleanup);
    setenv("LD_PRELOAD", "/tmp/stfu_fake.so", 1);
    
    // Настройка HOME
    if (custom_home) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "mkdir -p '%s' 2>/dev/null", custom_home);
        if (system(cmd)) {} // Намеренно игнорируем результат mkdir
        setenv("HOME", custom_home, 1);
        setenv("STFU_CUSTOM_HOME", custom_home, 1);
    } else if (strstr(argv[arg_start], "firefox")) {
        setenv("HOME", access("/home/user", F_OK) ? "/tmp" : "/home/user", 1);
    }
    
    // Очистка sudo переменных
    unsetenv("SUDO_USER");
    unsetenv("SUDO_UID");
    unsetenv("SUDO_GID");
    unsetenv("SUDO_COMMAND");
    
    // Специальная обработка Firefox
    if (strstr(argv[arg_start], "firefox")) {
        char ** const new_argv = malloc((argc - arg_start + 2) * sizeof(char*));
        if (__builtin_expect(!new_argv, 0)) {
            puts(t->error_unknown);
            return 1;
        }
        
        new_argv[0] = argv[arg_start];
        new_argv[1] = "--no-sandbox";
        for (int i = arg_start + 1; i < argc; ++i) {
            new_argv[i - arg_start + 1] = argv[i];
        }
        new_argv[argc - arg_start + 1] = NULL;
        
        setenv("MOZ_DISABLE_CONTENT_SANDBOX", "1", 1);
        setenv("MOZ_DISABLE_GMP_SANDBOX", "1", 1);
        
        execvp(new_argv[0], new_argv);
    } else {
        execvp(argv[arg_start], &argv[arg_start]);
    }
    
    puts(t->error_unknown);
    return 1;
}
