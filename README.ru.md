# Riffle

[🇬🇧 English](README.md) · **🇷🇺 Русский**

> Потоковый конвертер **JSON-lines → Parquet** для больших логов — на C++, быстрый и с
> постоянным потреблением памяти.

`riffle` превращает терабайтные `.jsonl`-файлы в готовый к аналитике **Apache Parquet** на
одной машине, на скорости диска, **не загружая вход в RAM**. Он закрывает нишу между
«погрепать лог глазами» и «поднять Spark/pandas/DuckDB ради одной конвертации».

Название — двойная метафора: *riffle* это и быстрая перетасовка колоды (реструктуризация
строк в колонки), и перекат воды по камням (потоковая обработка).

---

## Зачем Riffle

- **Производительность прежде всего** — SIMD-парсинг JSON
  ([simdjson](https://github.com/simdjson/simdjson)) и колоночная сборка батчами нацелены на
  порядок **≥ 1 ГБ/с** на одном ядре.
- **Постоянная память** — однопроходная потоковая обработка ограниченными батчами; расход
  RAM **O(1)** от размера входа, а не от размера файла.
- **Zero-config** — схема колонок **выводится** из данных, с опциональным явным override
  через `--schema`, если инференс ошибся.
- **Один статический бинарь** — без рантайма Python/JVM. Чисто встраивается в пайплайны и
  контейнеры.

Полный дизайн — в [`docs/riffle.md`](docs/riffle.md).

## Статус

⚠️ **Ранняя стадия.** Спецификация ([`docs/riffle.md`](docs/riffle.md)) готова; реализация в
процессе. API и флаги могут меняться.

## Быстрый старт

### Установка

```bash
# Системные пакеты (Debian/Ubuntu)
apt-get update && apt-get install -y \
    build-essential cmake git \
    libarrow-dev libparquet-dev \
    libzstd-dev libsnappy-dev

# simdjson (header-only) как submodule
git clone https://github.com/simdjson/simdjson third_party/simdjson

# Сборка
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### Использование

```bash
# Файл -> Parquet (схема выводится автоматически)
riffle events.jsonl -o events.parquet

# Поток из stdin, сжатие zstd, батчи по 100k строк
gunzip -c huge.jsonl.gz | riffle - -o out.parquet --compression zstd --batch-rows 100000

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
| `--on-error`        | `skip` \| `abort` \| `collect` | `skip`       | Политика битых строк                    |
| `--type-conflict`   | `widen` \| `string` \| `error` | `widen`      | Разрешение конфликта типов колонки      |
| `--stats`           | —                              | выкл.        | Печать статистики конвертации в stderr  |
| `-h, --help`        | —                              | —            | Показать справку                        |

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
