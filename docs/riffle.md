## Описание

`Riffle` (CLI-бинарь и namespace — `riffle`) — потоковый конвертер больших логов из формата
**JSON-lines** (по одному JSON-объекту на строку) в колоночные форматы: Apache **Parquet** и
собственный сырой columnar-дамп. Инструмент закрывает нишу между «погрепать лог глазами» и
«поднять Spark/pandas/DuckDB ради одной конвертации»: он превращает огромные `.jsonl`-файлы в
аналитический Parquet **на одном ноутбуке, в постоянной памяти**, не загружая весь вход в RAM.

Поставляется как публичная C++-библиотека (единая точка входа — свободная функция
`convert(const Config&) -> ConvertStats`) и как тонкая CLI-обёртка `riffle` поверх неё.
Язык — **C++23** (модель ошибок на `std::expected`).

> **Архитектура.** Ports & adapters + functional core / imperative shell. «Холодная» логика
> (конфиг, схема, инференс, разрешение конфликтов типов) — чистые функции над неизменяемыми
> value-структурами с фабриками `make_*`. Внешний мир за портами: `RowSink` (приём полей от
> парсера) и `Writer` (приёмник колоночных батчей). Парсинг — **push-модель**: парсер
> разворачивает JSON-объект и пушит поля напрямую в sink, без промежуточного объекта-строки.

## Оглавление

- [[#Описание]]
- [[#Оглавление]]
- [[#Быстрый старт]]
  - [[#Установка]]
  - [[#Запуск]]
  - [[#Проверка, что инструмент работает]]
- [[#Архитектурные требования и ограничения]]
  - [[#Требования]]
  - [[#Ограничения и Non-goals]]
- [[#Константы]]
- [[#Типы данных]]
  - [[#ColumnType]]
  - [[#CompressionCodec]]
  - [[#OnError]]
  - [[#TypeConflictPolicy]]
  - [[#OutputFormat]]
  - [[#PipelineState]]
- [[#Форматы данных]]
  - [[#CellValue]]
  - [[#ColumnSchema]]
  - [[#InferredSchema]]
  - [[#Config]]
  - [[#ColumnBuilder]]
  - [[#BatchBuilder и RecordBatch]]
  - [[#ParseError]]
  - [[#ConvertStats]]
- [[#Порты]]
  - [[#RowSink]]
  - [[#Writer]]
- [[#Машина состояний]]
- [[#Функции и классы]]
  - [[#Чтение: LineReader]]
  - [[#Парсинг: JsonParser]]
  - [[#Инференс схемы: InferenceSink]]
  - [[#resolve_type_conflict]]
  - [[#merge_override]]
  - [[#Колоночная сборка: BatchSink, build_batch, widen_column]]
  - [[#Запись: open_writer и backend'ы]]
  - [[#Timestamp: parse_timestamp_us]]
  - [[#JSON-схема: parse_schema_json]]
  - [[#Оркестрация: convert]]
  - [[#Наблюдаемость: emit_stats, exit_code]]
  - [[#Фабрики make_*]]
- [[#Формат columnar-raw]]
- [[#Публичный контракт]]
  - [[#C++ API]]
  - [[#CLI]]
    - [[#Флаги CLI]]
    - [[#Коды выхода]]
    - [[#Формат ошибок stderr]]

## Быстрый старт

### Установка

```bash
# Системные пакеты (Debian/Ubuntu)
apt-get update && apt-get install -y \
    build-essential cmake ninja-build \
    libarrow-dev libparquet-dev libzstd-dev libsnappy-dev \
    libgtest-dev

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

На Windows/MSYS2 (окружение UCRT64) те же зависимости ставятся как
`mingw-w64-ucrt-x86_64-{cmake,arrow,simdjson,gtest}`. Зависимости: **Arrow** + **Parquet**
(запись колоночного формата), **simdjson** (on-demand парсинг), **zstd**/**snappy** (кодеки),
**GoogleTest** (тесты). Рантайм Python/JVM не требуется.

### Запуск

```bash
riffle events.jsonl -o events.parquet

gunzip -c huge.jsonl.gz | riffle - -o out.parquet --compression zstd --batch-rows 100000

riffle 'logs/*.jsonl' -o merged.parquet --schema schema.json

riffle events.jsonl -o out.parquet --on-error collect --stats
```

### Проверка, что инструмент работает

```bash
printf '%s\n' \
  '{"ts":"2026-06-24T10:00:00Z","level":"info","code":200,"latency_ms":12.5}' \
  '{"ts":"2026-06-24T10:00:01Z","level":"error","code":500,"latency_ms":840.1}' \
  > sample.jsonl

riffle sample.jsonl -o sample.parquet --stats
# stderr: riffle: rows_read=2 rows_written=2 rows_skipped=0 bytes_out=... elapsed_ms=...

python -c "import pyarrow.parquet as pq; print(pq.read_table('sample.parquet').to_pandas())"
# Колонки ts(timestamp[us,UTC]), level(string), code(int64), latency_ms(double), 2 строки
```

## Архитектурные требования и ограничения

### Требования

| ID    | Требование              | Содержание                                                                                          |
| ----- | ----------------------- | --------------------------------------------------------------------------------------------------- |
| `R1`  | Пропускная способность  | Высокая на одном ядре за счёт SIMD-парсинга и батчей. Замерено — порядок **~115 МБ/с** входа end-to-end (см. бенчмарки в README); это конвертация со сжатием, а не сырой парсинг |
| `R2`  | Постоянная память       | Потребление RAM **O(1) от размера входа**: ограничено сэмплом инференса и одним батчем, не размером файла |
| `R3`  | Потоковость             | Однопроходная обработка; данные не материализуются целиком — читаются и пишутся скользящим окном    |
| `R4`  | Устойчивость к битым данным | Битая/невалидная строка не роняет процесс при `on_error ≠ abort`; учитывается в `ConvertStats`  |
| `R5`  | Корректность типов      | Колонка получает один Arrow-тип; конфликты разрешаются детерминированно по `TypeConflictPolicy`     |
| `R6`  | Наблюдаемость           | Любой прогон возвращает измеримый `ConvertStats`                                                     |
| `R7`  | Простота развёртывания  | Один статический бинарь без внешнего рантайма                                                        |

### Ограничения и Non-goals

| Ограничение / Non-goal       | Пояснение                                                                                  |
| ---------------------------- | ------------------------------------------------------------------------------------------ |
| Не СУБД и не query-движок    | Никакого SQL, фильтров, агрегаций, индексов — только конвертация формата                    |
| Не читает Parquet обратно    | Направление одностороннее: JSON-lines → columnar                                            |
| Не распределённая обработка  | Один процесс, одна машина                                                                   |
| Ограниченная вложенность     | Вложенные объекты разворачиваются в плоские колонки по пути (`a.b.c`) до `MAX_FLATTEN_DEPTH`; глубже — сериализуются как `string` |
| Не GUI                       | Только библиотека и CLI                                                                     |
| Многопоточность (опц.)       | По умолчанию один поток; `--threads N` распараллеливает парсинг/сборку с детерминированным выводом и ограниченной памятью |
| Вложенность (опц.)           | По умолчанию объекты разворачиваются в dotted-колонки; `--nested native` мапит объекты/массивы в нативные Parquet-struct/list |
| Без сетевого ввода/вывода    | Источники — локальные файлы и `stdin`; выход — локальный файл                               |
| Межбатчевое расширение типов | Авто-widen работает до первого сброса row-group; потоковый Parquet фиксирует схему после коммита |

## Константы

Определены в `include/riffle/constants.hpp`.

| Константа               | Тип      | Значение            | Где используется                                          |
| ----------------------- | -------- | ------------------- | --------------------------------------------------------- |
| `DEFAULT_BATCH_ROWS`    | `size_t` | `65536`             | Порог строк батча; дефолт `Config.batch_rows`             |
| `INFER_SAMPLE_ROWS`     | `size_t` | `10000`             | Размер сэмпла для инференса схемы                         |
| `MAX_FLATTEN_DEPTH`     | `int`    | `4`                 | Предел разворачивания вложенных объектов в `JsonParser`   |
| `EXIT_OK`               | `int`    | `0`                 | Успешная конвертация                                      |
| `EXIT_USAGE`            | `int`    | `2`                 | Ошибка аргументов CLI                                     |
| `EXIT_INPUT`            | `int`    | `3`                 | Ошибка чтения входа (зарезервирован)                      |
| `EXIT_DATA`             | `int`    | `4`                 | Конвертация завершилась в `ABORTED`                       |
| `EXIT_OUTPUT`           | `int`    | `5`                 | Ошибка записи выхода (зарезервирован)                     |

Также используются: `READ_BUFFER_BYTES` (буфер чтения `LineReader`), `MAX_LINE_BYTES` (лимит
длины строки; превышение → строка усекается и обрабатывается по `OnError`), `MAX_BATCH_BYTES`
(дефолт `Config.batch_bytes` — байтовый порог сброса батча). Зарезервирован (объявлен, пока не
используется): `DEFAULT_ROW_GROUP_BYTES` — задел под тюнинг размера row-group.

## Типы данных

### ColumnType

Логический тип колонки, в который маппится JSON-значение.

| Атрибут     | Значение      | Arrow-тип / назначение                          |
| ----------- | ------------- | ----------------------------------------------- |
| `INT64`     | `"int64"`     | `int64`                                         |
| `DOUBLE`    | `"double"`    | `double`                                        |
| `BOOL`      | `"bool"`      | `boolean`                                       |
| `STRING`    | `"string"`    | `utf8`; сюда же не разворачиваемые структуры     |
| `TIMESTAMP` | `"timestamp"` | ISO-8601 строка → `timestamp[us, UTC]`          |
| `NULLTYPE`  | `"null"`      | Колонка, в сэмпле встреченная только как `null` |

### CompressionCodec

| Атрибут  | Значение   | Назначение                                |
| -------- | ---------- | ----------------------------------------- |
| `NONE`   | `"none"`   | Без сжатия                                |
| `SNAPPY` | `"snappy"` | Быстрое сжатие                            |
| `ZSTD`   | `"zstd"`   | По умолчанию (`Config.compression`)       |

### OnError

Политика реакции на невалидную/нераспарсенную строку (→ `R4`).

| Атрибут   | Значение    | Назначение                                          |
| --------- | ----------- | --------------------------------------------------- |
| `SKIP`    | `"skip"`    | Пропустить, увеличить `rows_skipped`, продолжить    |
| `ABORT`   | `"abort"`   | Прервать конвертацию (`final_state = ABORTED`)      |
| `COLLECT` | `"collect"` | Пропустить и сохранить `ParseError` в `errors`      |

### TypeConflictPolicy

Разрешение ситуации, когда в одной колонке встречены значения разных типов (→ `R5`).

| Атрибут  | Значение   | Назначение                                                            |
| -------- | ---------- | --------------------------------------------------------------------- |
| `WIDEN`  | `"widen"`  | `int64`+`double`→`double`; иначе несовместимое → `string`             |
| `STRING` | `"string"` | Любой конфликт сводить к `string`                                     |
| `ERROR`  | `"error"`  | Конфликт типов — фатальная ошибка (всегда прерывает конвертацию)      |

### OutputFormat

| Атрибут        | Значение         | Назначение                                                |
| -------------- | ---------------- | --------------------------------------------------------- |
| `PARQUET`      | `"parquet"`      | Apache Parquet через Arrow (основной режим)               |
| `COLUMNAR_RAW` | `"columnar-raw"` | Собственный колоночный дамп (см. [[#Формат columnar-raw]]) |

### PipelineState

Терминальное состояние прогона в `ConvertStats.final_state`: `DONE` либо `ABORTED`.
Полное перечисление состояний модели пайплайна:

```cpp
enum class PipelineState { INIT, INFER_SCHEMA, CONVERT, FLUSH, FINALIZE, DONE, ABORTED };
```

## Форматы данных

> Value-структуры (`Config`, `ColumnSchema`, `InferredSchema`, `ParseError`, `ConvertStats`)
> неизменяемы и собираются фабриками `make_*` с проверкой инвариантов. `ColumnBuilder` /
> `BatchBuilder` живут на горячем пути и мутируются.

### CellValue

Одно JSON-скалярное значение; `std::monostate` означает null/отсутствие. Строки хранятся как
**`std::string_view`** в буфер парсера — без аллокации на горячем пути. View действителен
только во время вызова `RowSink::field()`; sink обязан скопировать строку в долговременное
хранилище немедленно (что и делают `InferenceSink`/`BatchSink`). Поэтому конструировать
`CellValue` со `string_view` из временной строки нельзя — только из живого буфера/литерала.

```cpp
using CellValue = std::variant<std::monostate, std::int64_t, double, bool, std::string_view>;
ColumnType column_type_of(const CellValue& value);  // monostate → NULLTYPE
```

### ColumnSchema

| Атрибут     | Тип          | Назначение                                                       |
| ----------- | ------------ | --------------------------------------------------------------- |
| `name`      | `string`     | Имя колонки в выходе (плоский путь, напр. `req.code`)            |
| `type`      | `ColumnType` | Логический тип колонки                                          |
| `nullable`  | `bool`       | Допускает ли `null` (по умолчанию `true`)                       |
| `json_path` | `string`     | Путь к значению в JSON; по умолчанию равен `name`               |

**Инварианты:** `name` непуст; `json_path` непуст (фабрика подставляет `name`).

### InferredSchema

| Атрибут        | Тип                    | Назначение                                     |
| -------------- | ---------------------- | ---------------------------------------------- |
| `columns`      | `vector<ColumnSchema>` | Колонки в порядке первого появления            |
| `sampled_rows` | `size_t`               | Сколько строк просмотрено при инференсе         |
| `had_conflicts`| `bool`                 | Зарезервировано под диагностику                |

**Инвариант:** имена колонок уникальны.

### Config

Параметры одного запуска. Поля и дефолты:

```cpp
struct Config {
    std::vector<std::string> inputs;          // пути или "-" (stdin)
    std::string              output_path;
    OutputFormat             output_format  = OutputFormat::PARQUET;
    InferredSchema           schema_override;  // пустая → инференс
    Projection               projection;       // --select / --exclude / --rename
    CompressionCodec         compression    = CompressionCodec::ZSTD;
    std::size_t              batch_rows      = DEFAULT_BATCH_ROWS;
    std::size_t              batch_bytes     = MAX_BATCH_BYTES;
    std::size_t              threads         = 1;     // >1 → parallel pipeline
    OnError                  on_error        = OnError::SKIP;
    TypeConflictPolicy       type_conflict   = TypeConflictPolicy::WIDEN;
    NestedMode               nested          = NestedMode::FLATTEN;  // native → struct/list
    bool                     emit_stats      = false;
    bool                     print_schema    = false;
};
```

**Инварианты:** `inputs` непуст; `output_path` непуст; `batch_rows ≥ 1`.

### ColumnBuilder

Горячий аккумулятор одной колонки. Значения копятся в нативные буферы без вызовов Arrow на
горячем пути; в Arrow-массив колонка конвертируется один раз за батч (`build_batch`).
Заполняется только буфер, соответствующий типу колонки: **`ints` хранит INT64, а также
TIMESTAMP (микросекунды) и BOOL (0/1)**, `doubles` — DOUBLE, `strings` — STRING.

```cpp
struct ColumnBuilder {
    ColumnSchema              schema;
    std::vector<std::int64_t> ints;
    std::vector<double>       doubles;
    std::vector<std::string>  strings;
    std::vector<std::uint8_t> valid;       // 1 = значение, 0 = null
    std::size_t               null_count = 0;
};
```

**Инвариант:** длины активного буфера и `valid` равны числу обработанных строк батча.

### BatchBuilder и RecordBatch

`BatchBuilder` — набор `ColumnBuilder` + счётчик строк `n_rows`. `RecordBatch` — замороженный
`arrow::RecordBatch` плюс `n_rows`, готовый к передаче в `Writer`.

### ParseError

| Атрибут   | Тип      | Назначение                                  |
| --------- | -------- | ------------------------------------------- |
| `line_no` | `size_t` | Номер строки во входе (1-based)             |
| `reason`  | `string` | Причина                                     |
| `raw`     | `string` | Сырой фрагмент строки                       |

**Инварианты:** `line_no ≥ 1`; `reason` непуст.

### ConvertStats

Итог прогона — единственный наблюдаемый результат `convert` (→ `R6`).

```cpp
struct ConvertStats {
    std::size_t             rows_read    = 0;
    std::size_t             rows_written = 0;
    std::size_t             rows_skipped = 0;
    std::size_t             bytes_in     = 0;   // зарезервировано
    std::size_t             bytes_out    = 0;   // зарезервировано
    std::uint64_t           elapsed_ms   = 0;
    PipelineState           final_state  = PipelineState::INIT;
    std::vector<ParseError> errors;             // заполняется при OnError::COLLECT
};
```

## Порты

### RowSink

Push-приёмник полей одной строки. Парсер вызывает `begin_row()`, затем `field()` для каждого
листа, затем `end_row()`. Это избавляет от промежуточного объекта-строки: sink пишет данные
сразу в цель (построители колонок или аккумулятор типов).

```cpp
class RowSink {
public:
    virtual void begin_row() = 0;
    virtual std::expected<void, std::string> field(std::string_view path, CellValue value) = 0;
    virtual std::expected<void, std::string> end_row() = 0;
};
```

Реализации: `InferenceSink` (инференс схемы) и `BatchSink` (сборка батча).

### Writer

Приёмник колоночных батчей и финализатор выходного файла.

```cpp
class Writer {
public:
    virtual std::expected<void, std::string> write(const RecordBatch& batch) = 0;
    virtual std::expected<void, std::string> finish() = 0;
};
```

Реализации: `ParquetWriter` (Arrow/Parquet) и `ColumnarRawWriter`. Выбор —
`open_writer(const Config&, const InferredSchema&)`.

## Машина состояний

Линейный поток с двумя терминальными состояниями:
`INIT → INFER_SCHEMA → CONVERT ↔ FLUSH → FINALIZE → DONE`, и `* → ABORTED` при фатальной
ошибке (ошибка чтения/записи, `OnError::ABORT` на битой строке, конфликт типов под
`TypeConflictPolicy::ERROR`). `convert` возвращает `final_state ∈ {DONE, ABORTED}`.

## Функции и классы

> Модель ошибок: восстановимые отказы — `std::expected<T, std::string>`; нарушение инварианта
> в фабриках `make_*` — исключение `std::invalid_argument`.

### Чтение: LineReader и ByteSource

`include/riffle/reader.hpp`, `include/riffle/input.hpp`. `LineReader` потоково выдаёт непустые
логические строки из порта `ByteSource`, толерантен к CRLF. Читает чанками по `READ_BUFFER_BYTES`
в **переиспользуемый внутренний буфер** и возвращает `std::optional<std::string_view>` в него —
без аллокации на строку (view действителен до следующего `next()`). Строка длиннее
`MAX_LINE_BYTES` **усекается** (остаток физической строки пропускается) — память ограничена даже
на патологическом входе; усечённая строка обычно не парсится как JSON и идёт по `OnError`.

`open_input(path)` создаёт `ByteSource` на базе Arrow IO: `-` → stdin, `*.gz`/`*.zst` —
**прозрачная распаковка** (Arrow-кодеки), иначе обычный файл. Тесты используют `StdByteSource`
поверх `std::istream`.

```cpp
std::optional<std::string_view> LineReader::next();
std::unique_ptr<ByteSource> open_input(const std::string& path);
```

### Многопоточность: convert(--threads)

При `threads == 1` — однопроходный потоковый путь (`run_single`). При `threads > 1` запускается
конвейер с ограниченной памятью: продюсер режет вход на чанки по `batch_rows` строк, N воркеров
парсят и строят `RecordBatch` (со схемой, **фиксированной из сэмпла** — widening отключён, чтобы
схемы чанков совпадали), а единый писатель сливает батчи **строго в порядке seq**. Это даёт
**детерминированный, byte-identical вывод** независимо от числа потоков и постоянную память
(число «в полёте» чанков ограничено семафором). Битые строки и политика `OnError` обрабатываются
по-чанково и сводятся в порядке seq. Замер: ~120 → ~380 МБ/с при 1 → 8 потоках (см. README).

### Нативная вложенность: --nested native

`include/riffle/nested.hpp`, `src/adapters/nested.cpp`. При `nested == NATIVE` (только формат
`parquet`) вместо разворачивания объектов в dotted-колонки строится **дерево типов** `NestedType`
(объект → struct, массив → list, скаляры → примитивы) и из него — нативная `arrow::Schema`.
`infer_nested_schema` парсит сэмпл через **simdjson DOM** и **унифицирует** типы по всем строкам
(int+double→double; несовпадение форм/скаляров → `string` с сериализацией значения). `NestedBuilder`
рекурсивно наполняет `arrow::RecordBatchBuilder` (Struct/List/scalar builders) по каждой строке и
сбрасывает `RecordBatch` каждые `batch_rows`. Битые/не-объектные строки идут по `OnError`.
Многопоточность с native не комбинируется (путь однопоточный); `--nested native` с `columnar-raw`
отвергается фабрикой `make_Config`.

### Парсинг: JsonParser

`include/riffle/json_parser.hpp`. Парсит одну JSON-строку через **simdjson on-demand** и пушит
плоские листовые поля в `RowSink`. Вложенные объекты разворачиваются до `MAX_FLATTEN_DEPTH`
(путь `a.b.c`), глубже — сериализуются в строку. Для плоских ключей путь передаётся как
`string_view` без аллокации. Внутренний `padded`-буфер переиспользуется между строками.

```cpp
std::expected<void, std::string> JsonParser::parse(std::string_view line, RowSink& sink);
```

Возвращает ошибку для невалидного JSON или не-объекта верхнего уровня (в этом случае
`begin_row()` не вызывается — частичной строки не возникает).

### Инференс схемы: InferenceSink

`include/riffle/schema.hpp`. `RowSink`, который по сэмплу наблюдает типы полей. Колонки
появляются в порядке первого появления; по каждому пути множество наблюдённых типов
сворачивается `resolve_type_conflict`. ISO-8601 строки распознаются как `TIMESTAMP`. Итог —
`schema()`.

### resolve_type_conflict

Сводит наблюдённые типы колонки к одному по политике. `NULLTYPE` игнорируется при наличии
другого типа; единственный тип → он; иначе по политике: `WIDEN` (`int`+`double`→`double`,
прочее→`string`), `STRING`, `ERROR` (возвращает `unexpected`).

```cpp
std::expected<ColumnType, std::string> resolve_type_conflict(std::span<const ColumnType> seen,
                                                             TypeConflictPolicy policy);
```

### merge_override

Накладывает явную схему на выведенную: одноимённая колонка получает тип/`nullable` из
override, отсутствующая — добавляется, прочие сохраняются. Проверяет уникальность имён.

### Колоночная сборка: BatchSink, build_batch, widen_column

`include/riffle/batch.hpp`. `BatchSink` — `RowSink`, пишущий каждое поле сразу в построитель
своей колонки. Колонка ищется по `json_path` за **O(1)** через `unordered_map` с прозрачным
хешированием `string_view` (для плоских ключей — без аллокации). После строки `end_row()`
заполняет `null` пропущенные колонки. Конфликт типов под `ERROR` помечается `fatal()` — такие
отказы прерывают конвертацию независимо от `OnError`.

```cpp
BatchBuilder make_batch_builder(const InferredSchema& schema);
std::expected<RecordBatch, std::string> build_batch(BatchBuilder& builder);     // bulk AppendValues + Finish
std::expected<void, std::string> widen_column(ColumnBuilder& column, ColumnType to);  // DOUBLE | STRING
```

`build_batch` конвертирует каждую колонку **одним** `AppendValues` в типизированный Arrow-builder
и сбрасывает нативные буферы. `widen_column` расширяет уже накопленный буфер (int→double либо
любой→string) — основа авто-widen за пределами сэмпла.

### Запись: open_writer и backend'ы

`include/riffle/writer.hpp`. `open_writer` диспетчит по `OutputFormat`. `ParquetWriter`
(`parquet::arrow::FileWriter`, кодек из `Config`) и `ColumnarRawWriter`. В `convert` writer
**открывается лениво на первом сбросе** — чтобы схема файла отражала любой widening,
случившийся до первого row-group.

### Timestamp: parse_timestamp_us

`include/riffle/timestamp.hpp`. Парсит ISO-8601 UTC (`YYYY-MM-DDTHH:MM:SS[.fff][Z]`) в
микросекунды от эпохи; `looks_like_timestamp` — предикат для инференса.

```cpp
std::optional<std::int64_t> parse_timestamp_us(std::string_view text);
bool looks_like_timestamp(std::string_view text);
```

### JSON-схема: parse_schema_json

`include/riffle/schema_json.hpp`. Парсит документ `{"columns":[{name,type,nullable?,json_path?},...]}`
в `InferredSchema`; `load_schema_file` читает его из файла. Используется флагом `--schema`.

### Оркестрация: convert

`include/riffle/convert.hpp`, реализация в `src/app/convert.cpp`. Свободная функция — единая
точка входа. Алгоритм: `ChainedLines` отдаёт сырые строки по всем входам; **фаза 1** буферит до
`INFER_SAMPLE_ROWS` строк и кормит ими `InferenceSink` → схема (+`merge_override`); **фаза 2**
создаёт `BatchSink` над `BatchBuilder` и прогоняет сэмпл-строки и остаток входа, пушя поля
прямо в колонки; по достижении `batch_rows` — `build_batch` + `Writer::write`; в конце —
`finish`. Битые строки идут через `on_bad_line` (политика `OnError`); фатальные ошибки полей
прерывают всегда.

```cpp
ConvertStats convert(const Config& cfg);
```

### Наблюдаемость: emit_stats, exit_code

`include/riffle/stats.hpp`. `emit_stats` печатает одну строку статистики в stderr (при
`Config.emit_stats`). `exit_code` отображает `final_state` в код выхода (`DONE`→`EXIT_OK`,
иначе `EXIT_DATA`).

### Фабрики make_*

`include/riffle/factories.hpp`. `make_Config`, `make_ColumnSchema`, `make_InferredSchema`,
`make_ParseError`, `make_ConvertStats` — конструируют value-структуры, проверяя инварианты
(иначе `std::invalid_argument`).

## Формат columnar-raw

Самоописываемый бинарный дамп (отладка/тесты), backend `ColumnarRawWriter`:

```
magic            : "RIFFLEC1"            (8 байт)
columns          : uint32                (число колонок)
  для каждой колонки:
    name_len     : uint32
    name         : name_len байт
    type         : uint8                 (значение ColumnType)
для каждого батча:
  rows           : uint32
  для каждой колонки, для каждой строки:
    null_flag    : uint8                 (1 = null, тогда значение опускается)
    value        : int64 | double(8) | uint8(bool) | (uint32 len + байты)(string)
```

## Публичный контракт

### C++ API

Единая точка входа — `convert`. Вспомогательные сущности (`JsonParser`, `InferenceSink`,
`BatchSink`, `open_writer`, фабрики) публичны для тестирования и переиспользования.

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

**Ошибки:** невалидный `Config` → `std::invalid_argument` из `make_Config`; ошибки
чтения/записи/данных → `final_state = ABORTED` (без исключения).

### CLI

`riffle <inputs...> -o <output> [флаги]`. Разбирает argv в `Config` (`parse_args`), вызывает
`convert`, печатает `ConvertStats` (при `--stats`) и возвращает код выхода.

#### Флаги CLI

| Флаг                | Аргумент                       | По умолчанию | Поле `Config`                       |
| ------------------- | ------------------------------ | ------------ | ----------------------------------- |
| `-o, --output`      | путь                           | обязателен   | `output_path`                       |
| `--format`          | `parquet` \| `columnar-raw`    | `parquet`    | `output_format`                     |
| `--schema`          | путь к JSON                    | нет          | `schema_override` (см. `parse_schema_json`) |
| `--compression`     | `none` \| `snappy` \| `zstd`   | `zstd`       | `compression`                       |
| `--batch-rows`      | целое                          | `65536`      | `batch_rows`                        |
| `--batch-bytes`     | целое                          | 256 МиБ      | `batch_bytes` (байтовый порог сброса батча) |
| `--threads`         | целое                          | `1`          | `threads` (число рабочих потоков)   |
| `--nested`          | `flatten` \| `native`          | `flatten`    | `nested` (развернуть или Parquet struct/list) |
| `--on-error`        | `skip` \| `abort` \| `collect` | `skip`       | `on_error`                          |
| `--type-conflict`   | `widen` \| `string` \| `error` | `widen`      | `type_conflict`                     |
| `--select`          | `col,col,...`                  | все          | `projection.select`                 |
| `--exclude`         | `col,col,...`                  | нет          | `projection.exclude`                |
| `--rename`          | `from=to,...`                  | нет          | `projection.rename`                 |
| `--print-schema`    | —                              | выкл.        | `print_schema` (печатает схему через `infer_schema`/`write_schema_json`) |
| `--stats`           | —                              | выкл.        | `emit_stats`                        |
| `-h, --help`        | —                              | —            | справка, выход `EXIT_OK`            |
| `--version`         | —                              | —            | версия, выход `EXIT_OK`            |
| `-` (позиционный)   | —                              | —            | элемент `inputs` = stdin            |

#### Коды выхода

| Код | Константа     | Условие                                            |
| --- | ------------- | -------------------------------------------------- |
| `0` | `EXIT_OK`     | `final_state == DONE`                              |
| `2` | `EXIT_USAGE`  | Ошибка разбора аргументов / отсутствует `-o`       |
| `4` | `EXIT_DATA`   | Конвертация завершилась в `ABORTED`                |

#### Формат ошибок stderr

`riffle: usage: <причина>` при ошибке аргументов. Сообщения собранных ошибок строк хранятся в
`ConvertStats.errors` (при `--on-error collect`).
