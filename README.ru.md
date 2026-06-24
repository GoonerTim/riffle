# Riffle

[🇬🇧 English](README.md) · **🇷🇺 Русский**

> Потоковый конвертер **JSON-lines → Parquet** для больших логов — на C++, быстрый и с
> постоянным потреблением памяти.

`riffle` превращает терабайтные `.jsonl`-файлы в готовый к аналитике **Apache Parquet** на
одной машине, на скорости диска, **не загружая вход в RAM**. Он закрывает нишу между
«погрепать лог глазами» и «поднять Spark/pandas/DuckDB ради одной конвертации».

---

## Зачем Riffle

- **Постоянная память** — однопроходная потоковая обработка ограниченными батчами; расход
  RAM **O(1)** от размера входа, а не от размера файла. Это главная фича: конвертация файлов
  **больше объёма RAM**.
- **Быстро на одном ядре** — SIMD-парсинг JSON
  ([simdjson](https://github.com/simdjson/simdjson)) on-demand и батчевые колоночные append
  держат **~115 МБ/с** входа на одном ядре.
- **Zero-config** — схема колонок **выводится** из данных, с опциональным явным override
  через `--schema`, если инференс ошибся.
- **Один статический бинарь** — без рантайма Python/JVM. Чисто встраивается в пайплайны и
  контейнеры.

Полный дизайн — в [`docs/riffle.md`](docs/riffle.md).

## Для кого

- **Дата- и бэкенд-инженеры**, которым нужны JSON-логи в колоночном хранилище для аналитики
  без поднятия Spark или Python-стека.
- **DevOps / SRE**, превращающие логи приложений, доступа или аудита (`.jsonl`) в Parquet для
  запросов через DuckDB, Spark, Trino/Athena или pandas.
- **Любой на ноутбуке или маленькой VM**, кому нужно сконвертировать файл **больше доступной
  RAM** — плоский профиль памяти Riffle именно для этого.
- **Авторы пайплайнов / контейнеров**, предпочитающие один бинарь без зависимостей рантайму
  Python/JVM в ETL-шаге.

## Применения

- **Архивация и аналитика логов** — конвертация NDJSON-логов приложений/доступа/аудита в
  компактный Parquet для дешёвого хранения и быстрых запросов.
- **Стадия ETL-загрузки** — быстрый шаг «JSON-lines → Parquet» с ограниченной памятью внутри
  большого пайплайна (cron, задача Airflow, шаг контейнера).
- **Landing в data lake** — нормализация разнородных JSON-событий в типизированный Parquet с
  выведенной схемой (или зафиксированной через `--schema`).
- **Разовая конвертация** — превратить огромный `.jsonl`-дамп в Parquet на ноутбуке, где
  pandas/pyarrow упрутся в OOM.
- **Поток из pipe** — `gunzip -c big.jsonl.gz | riffle - -o out.parquet`, конвертация на лету
  без временного файла.
- **Как библиотека** — встроить `riffle::convert` в C++-сервис, чтобы писать Parquet из JSON
  без тяжёлого дата-фреймворка.

## Бенчмарки

Конвертация одного и того же датасета JSON-lines в Parquet: Riffle против популярных
Python-однострочников (`duckdb`, `pyarrow.json`, `pandas`). Замерено на машине разработки;
воспроизводится через `just bench` (каждый инструмент — подпроцесс, 3 прогона, лучшее
wall-time, пиковый RSS через psutil).

### Пиковая память — ради чего Riffle и создан

![Сравнение по памяти](docs/img/bench_memory.png)

Riffle обрабатывает поток в **постоянной памяти**: пик держится на **~80 МБ и для входа 120 МБ,
и для 359 МБ**. Остальные грузят файл целиком (или большие промежуточные структуры): pandas —
до **4.2 ГБ на входе 359 МБ (~×12)**, pyarrow — ~810 МБ, duckdb — ~490 МБ и растёт с размером
входа. Эта плоская линия — причина, по которой Riffle конвертирует файлы **больше объёма RAM**
на ноутбуке, где остальные упираются в OOM.

### Пропускная способность — честная картина

![Сравнение по скорости](docs/img/bench_throughput.png)

На графике Riffle показан и однопоточным (`1t`), и многопоточным (`8t`); DuckDB и PyArrow тоже
используют все ядра. Однопоточный Riffle (~95–100 МБ/с) — в середине: быстрее pandas, медленнее
многоядерных инструментов, но с `--threads 8` достигает **~360–380 МБ/с**, на уровне или выше
DuckDB и PyArrow на этом железе — при доле их памяти. Парсинг использует **on-demand** API
simdjson со строковыми ячейками-`string_view` (без аллокации на поле), поля пишутся **напрямую
в построители колонок**, а append в Arrow **батчевые** по колонкам.

### Масштабирование по `--threads`

![Скорость от числа потоков](docs/img/bench_threads.png)

Однопоточная скорость — в середине, но конвертация распараллеливается по ядрам: с `--threads 8`
Riffle достигает **~380 МБ/с** на датасете 120 МБ — на уровне DuckDB/PyArrow — сохраняя ту же
плоскую ограниченную память и **byte-identical детерминированный вывод** (воркеры парсят+строят
чанки; единый писатель пишет батчи в порядке входа).

### Где Riffle выигрывает, а где нет

| Критерий                              | Riffle               | duckdb  | pyarrow | pandas |
| ------------------------------------- | -------------------- | ------- | ------- | ------ |
| Пик памяти, не растёт с размером входа | ✅ ~80 МБ, постоянно  | ⚠️ растёт | ❌ растёт | ❌ огромный |
| Конвертирует файлы больше RAM          | ✅                    | ⚠️       | ❌       | ❌      |
| Сырая скорость (1 поток)               | ⚠️ ~95 МБ/с           | ✅       | ✅       | ❌      |
| Сырая скорость (`--threads 8`)         | ✅ ~370 МБ/с          | ✅       | ✅       | ❌      |
| Один статический бинарь, без рантайма  | ✅                    | ❌ (lib) | ❌ (lib) | ❌ (lib) |

**Вывод:** нужна максимальная скорость на данных, помещающихся в память — DuckDB отличен. Нужно
конвертировать **произвольно большие логи в ограниченной памяти** одним бинарём без зависимостей —
это ровно то, для чего Riffle.

## Статус

🚧 **Рабочий MVP.** Конвертация JSON-lines → Parquet (и `columnar-raw`) работает end-to-end
(библиотека + CLI), написана test-first (100+ тестов). C++23. Схема выводится автоматически
(включая ISO-8601 timestamps); типы авто-расширяются за пределами сэмпла. Поддержаны: override
`--schema`, проекция колонок (`--select`/`--exclude`/`--rename`), прозрачный вход gzip/zstd,
многопоточная конвертация (`--threads`, детерминированный вывод) и вложенный JSON — либо
разворачивается в плоские dotted-колонки (по умолчанию), либо мапится в **нативные
Parquet-struct/list** (`--nested native`). Известное ограничение: при `--threads > 1` схема
фиксируется из сэмпла (без межбатчевого widening).

## Быстрый старт

### Установка

```bash
# Debian/Ubuntu: Arrow/Parquet ставятся из APT-репозитория Apache Arrow,
# а не из стандартных репозиториев Ubuntu.
sudo apt-get update
sudo apt-get install -y -V ca-certificates lsb-release wget
wget https://apache.jfrog.io/artifactory/arrow/$(lsb_release --id --short | tr 'A-Z' 'a-z')/apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb
sudo apt-get install -y -V ./apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb
sudo apt-get update

# Зависимости сборки
sudo apt-get install -y -V \
    build-essential cmake ninja-build \
    libarrow-dev libparquet-dev libzstd-dev libsnappy-dev \
    libgtest-dev

# Сборка
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### Использование

```bash
# Файл -> Parquet (схема выводится автоматически)
riffle events.jsonl -o events.parquet

# Вход .gz/.zst распаковывается прозрачно (по расширению)
riffle huge.jsonl.gz -o out.parquet

# Параллельная конвертация по ядрам (детерминированный вывод, ограниченная память)
riffle huge.jsonl -o out.parquet --threads 8

# Вложенный JSON в нативные Parquet struct/list вместо разворачивания
riffle events.jsonl -o out.parquet --nested native

# Несколько файлов по glob + явная схема
riffle 'logs/*.jsonl' -o merged.parquet --schema schema.json

# Не падать на битых строках, собрать их, печатать статистику в stderr
riffle events.jsonl -o out.parquet --on-error collect --stats
```

### Библиотека

```cpp
#include <riffle.hpp>

int main() {
    riffle::Config cfg = riffle::make_Config(
        /*inputs=*/{"events.jsonl"},
        /*output_path=*/"events.parquet");
    riffle::ConvertStats stats = riffle::convert(cfg);
    return stats.final_state == riffle::PipelineState::DONE ? 0 : 1;
}
```

## Флаги CLI

| Флаг                | Аргумент                       | По умолчанию | Действие                                |
| ------------------- | ------------------------------ | ------------ | --------------------------------------- |
| `-o, --output`      | путь                           | обязателен   | Выходной файл                           |
| `--format`          | `parquet` \| `columnar-raw`    | `parquet`    | Формат выхода                           |
| `--schema`          | путь к JSON                    | нет          | Явная схема, перекрывает инференс        |
| `--compression`     | `none` \| `snappy` \| `zstd`   | `zstd`       | Кодек Parquet                           |
| `--batch-rows`      | целое                          | `65536`      | Строк в батче                           |
| `--batch-bytes`     | целое                          | 256 МиБ      | Байтовый порог батча                    |
| `--threads`         | целое                          | `1`          | Параллельные рабочие потоки             |
| `--on-error`        | `skip` \| `abort` \| `collect` | `skip`       | Политика битых строк                    |
| `--type-conflict`   | `widen` \| `string` \| `error` | `widen`      | Разрешение конфликта типов колонки      |
| `--nested`          | `flatten` \| `native`          | `flatten`    | Развернуть вложенность или мапить в Parquet struct/list |
| `--select`          | `col,col,...`                  | все          | Оставить только эти колонки (в этом порядке) |
| `--exclude`         | `col,col,...`                  | нет          | Убрать эти колонки                      |
| `--rename`          | `from=to,...`                  | нет          | Переименовать выходные колонки          |
| `--print-schema`    | —                              | выкл.        | Напечатать выведенную схему как JSON и выйти |
| `--stats`           | —                              | выкл.        | Печать статистики конвертации в stderr  |
| `-h, --help`        | —                              | —            | Показать справку и выйти                |
| `--version`         | —                              | —            | Показать версию и выйти                 |

## Разработка

Репозиторий использует [`just`](https://github.com/casey/just) как task-runner:

```bash
just            # список задач
just build      # конфигурация + сборка (Release)
just test       # запуск тестов
just fmt        # форматирование (clang-format)
just lint       # статический анализ (clang-tidy)
```

CI прогоняет сборку, тесты, проверки форматирования и линт на каждый push и pull request
через GitHub Actions (см. [`.github/workflows/ci.yml`](.github/workflows/ci.yml)).

## Контрибьютинг

Будем рады вкладу — см. [CONTRIBUTING.ru.md](CONTRIBUTING.ru.md)
([in English](CONTRIBUTING.md)).

## Лицензия

[MIT](LICENSE) © 2026 Riffle contributors.
