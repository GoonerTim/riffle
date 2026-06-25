# Riffle

[🇬🇧 English](README.md) · **🇷🇺 Русский**

> Потоковый конвертер **JSON-lines → Parquet** для больших логов — на C++, быстрый и с
> постоянным потреблением памяти.

`riffle` превращает терабайтные `.jsonl`-файлы в готовый к аналитике **Apache Parquet** на
одной машине, на скорости диска, **не загружая вход в RAM**. Он закрывает нишу между
«погрепать лог глазами» и «поднять Spark/pandas/DuckDB ради одной конвертации».

---

## Зачем Riffle

- **Ограниченная память** — однопроходная потоковая обработка ограниченными батчами; расход RAM
  **не растёт** с размером файла. Это главная фича: конвертация файлов **больше объёма RAM**.
  Пик в один поток — плоские ~80 МБ; `--threads` добавляет небольшую ограниченную величину на воркера.
- **Масштабируется по ядрам** — SIMD-парсинг JSON
  ([simdjson](https://github.com/simdjson/simdjson)) и батчевые колоночные append держат ~115 МБ/с
  на ядре и **~410–550 МБ/с при `--threads 8`** — на уровне или выше DuckDB/PyArrow на бенч-машине —
  с **детерминированным byte-identical** выводом независимо от числа потоков.
- **Zero-config схема** — схема колонок **выводится** (включая ISO-8601 timestamps), с
  опциональным override `--schema` и проекцией колонок (`--select`/`--exclude`/`--rename`).
- **Реальные входы** — прозрачная распаковка **gzip/zstd** по расширению, а вложенный JSON либо
  разворачивается в dotted-колонки, либо мапится в **нативные Parquet-struct/list** (`--nested native`).
- **Один статический бинарь** — без рантайма Python/JVM; как CLI или C++-библиотека. Собирается и
  тестируется на Linux, macOS и Windows.

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
- **Сжатые логи** — указать Riffle прямо на `big.jsonl.gz`/`big.jsonl.zst` (распаковка прозрачна)
  или принять поток из pipe, передав `-` как вход.
- **Как библиотека** — встроить `riffle::convert` в C++-сервис, чтобы писать Parquet из JSON
  без тяжёлого дата-фреймворка.

## Бенчмарки

Конвертация одного и того же датасета JSON-lines в Parquet: Riffle против популярных
Python-однострочников (`duckdb`, `pyarrow.json`, `pandas`). Каждый инструмент замеряется **дважды** —
**в один поток** (по ядру на каждого) и **многопоточно** (все ядра) — для честного сравнения;
воспроизводится через `just bench` (3 прогона, лучшее wall-time, пиковый RSS через psutil). DuckDB
фиксируется через `SET threads`, PyArrow — через `ReadOptions(use_threads)`, Riffle — через
`--threads`; у pandas нет параллельного чтения JSON, поэтому он однопоточный в обоих случаях.

### Один поток — по ядру на каждого

![Скорость, один поток](docs/img/bench_throughput_single.png)
![Память, один поток](docs/img/bench_memory_single.png)

На одном ядре по скорости лидирует DuckDB (~190–230 МБ/с); Riffle (~115–120 МБ/с) — на уровне
PyArrow. Преимущество Riffle здесь — **память**: ~75–80 МБ независимо от входа, тогда как PyArrow
раздувается до 340 МБ / 720 МБ, а pandas — до 1.4 ГБ / 4.2 ГБ (DuckDB тоже экономен, ~87 МБ).

| Инструмент (1 ядро) | Скорость 1M | Скорость 3M | Память 1M | Память 3M |
| ------------------- | ----------- | ----------- | --------- | --------- |
| **riffle**          | 116 МБ/с    | 119 МБ/с    | **75 МБ** | **79 МБ** |
| duckdb              | **187 МБ/с**| **227 МБ/с**| 87 МБ     | 87 МБ     |
| pyarrow             | 113 МБ/с    | 127 МБ/с    | 341 МБ    | 721 МБ    |
| pandas              | 36 МБ/с     | 39 МБ/с     | 1442 МБ   | 4185 МБ   |

### Все ядра — многопоточно

![Скорость, многопоток](docs/img/bench_throughput_multi.png)
![Память, многопоток](docs/img/bench_memory_multi.png)

На всех ядрах Riffle (`--threads 8`) — **самый быстрый на наборе 1M** (386 против 340 у DuckDB и
251 у PyArrow) и **вровень с DuckDB на 3M** (547 против 569), заметно опережая PyArrow — и делает это
в **наименьшей памяти среди быстрых инструментов** (~180–245 МБ против 213–494 у DuckDB и 388–814 у
PyArrow), по-прежнему ограниченной и не растущей с размером входа.

| Инструмент (все ядра)       | Скорость 1M  | Скорость 3M  | Память 1M  | Память 3M  |
| --------------------------- | ------------ | ------------ | ---------- | ---------- |
| **riffle** (`--threads 8`)  | **386 МБ/с** | 547 МБ/с     | **181 МБ** | **244 МБ** |
| duckdb                      | 340 МБ/с     | **569 МБ/с** | 213 МБ     | 494 МБ     |
| pyarrow                     | 251 МБ/с     | 397 МБ/с     | 388 МБ     | 814 МБ     |
| pandas                      | 34 МБ/с      | 40 МБ/с      | 1441 МБ    | 4183 МБ    |

### Масштабирование по `--threads`

![Скорость от числа потоков](docs/img/bench_threads.png)

Скорость растёт с ядрами: **~117 → 205 → 308 → 369 МБ/с** при 1 → 2 → 4 → 8 потоках (датасет 120 МБ).
Вывод **byte-identical и детерминированный** при любом числе потоков (воркеры парсят+строят чанки;
единый писатель пишет батчи в порядке входа). Цена — память: каждый чанк «в полёте» добавляет
ограниченную величину, поэтому пик RSS умеренно растёт с `--threads`, но никогда — с размером входа.

**Вывод:** на одном ядре DuckDB и PyArrow парсят JSON быстрее Riffle. Но с `--threads` Riffle
**на уровне или быстрее их по пропускной способности**, оставаясь единственным инструментом с
**ограниченной памятью** — и поставляется одним бинарём без зависимостей. DuckDB отличен для
in-memory SQL-аналитики; а для конвертации **произвольно больших логов в Parquet с предсказуемой
памятью** (в один поток для минимального следа или во много — для максимальной скорости) Riffle
создан специально.

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

CI собирает и тестирует на **Linux, macOS и Windows** (каждый push и PR). К тегам-релизам
прикладываются **готовые бинарники** под каждую платформу (`riffle-linux-x86_64.tar.gz`,
`riffle-macos-arm64.tar.gz`, `riffle-windows-x86_64.zip`) на странице
[Releases](https://github.com/GoonerTim/riffle/releases) — они динамически слинкованы с Arrow,
поэтому для запуска поставь рантайм Arrow (ниже) или собери из исходников.

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

На **macOS** (Homebrew) и **Windows** (MSYS2 UCRT64) зависимости ставятся пакетным менеджером
платформы; команда сборки та же:

```bash
# macOS
brew install apache-arrow cmake ninja googletest

# Windows — в шелле MSYS2 UCRT64
pacman -S --needed mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,arrow,gtest}
```

Чтобы собрать только CLI без тестов (GoogleTest не нужен), добавь `-DRIFFLE_BUILD_TESTS=OFF`.

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
    // make_Config проверяет инварианты и кидает std::invalid_argument на плохом входе.
    riffle::Config cfg = riffle::make_Config({
        .inputs = {"events.jsonl"},
        .output_path = "events.parquet",
        .threads = 8,
    });
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

CI собирает и тестирует на **Linux, macOS и Windows** и прогоняет формат + clang-tidy на каждый
push и pull request; к тегам-релизам собираются бинарники под каждую платформу (см.
[`.github/workflows/`](.github/workflows/)).

## Контрибьютинг

Будем рады вкладу — см. [CONTRIBUTING.ru.md](CONTRIBUTING.ru.md)
([in English](CONTRIBUTING.md)).

## Лицензия

[MIT](LICENSE) © 2026 Riffle contributors.
