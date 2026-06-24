# Вклад в Riffle

[🇬🇧 English](CONTRIBUTING.md) · **🇷🇺 Русский**

Спасибо за интерес к Riffle! Этот документ объясняет, как предлагать изменения и чего мы ждём
от вклада.

## Кодекс поведения

Будьте уважительны и конструктивны. Мы придерживаемся духа
[Contributor Covenant](https://www.contributor-covenant.org/). Любые формы харассмента
недопустимы.

## Прежде чем начать

1. **Прочитайте спеку.** [`docs/riffle.md`](docs/riffle.md) — источник истины по поведению,
   типам данных, машине состояний и публичному контракту. Изменения должны оставаться
   согласованными с ней — либо обновляйте её в том же pull request.
2. **Сначала откройте issue** для всего нетривиального (новые фичи, изменения API,
   зависимости). Это исключает дублирование работы и позволяет заранее согласовать подход.
3. Хорошие первые задачи: тесты, документация, мелкие багфиксы, бенчмарки.

## Настройка окружения

```bash
# Зависимости (Debian/Ubuntu). Arrow/Parquet — из APT-репозитория Apache Arrow:
sudo apt-get update
sudo apt-get install -y -V ca-certificates lsb-release wget
wget https://apache.jfrog.io/artifactory/arrow/$(lsb_release --id --short | tr 'A-Z' 'a-z')/apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb
sudo apt-get install -y -V ./apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb
sudo apt-get update
sudo apt-get install -y -V build-essential cmake ninja-build \
    libarrow-dev libparquet-dev libzstd-dev libsnappy-dev \
    libsimdjson-dev libgtest-dev

# Сборка и тесты через task-runner
just build
just test
```

Мы используем [`just`](https://github.com/casey/just) как task-runner — выполните `just`, чтобы
увидеть список задач.

## Правила кода

- **Язык:** C++23 (используется `std::expected`).
- **Стиль: functional core, imperative shell.** Предпочитайте свободные функции
  классам-оркестраторам; моделируйте данные как неизменяемые value-структуры, создаваемые
  фабриками `make_*`. Мутабельность допустима **только** на горячем пути (буферы чтения,
  аккумуляторы колонок) и должна быть изолирована. Если стиль конфликтует с пропускной
  способностью на горячем пути — **приоритет за производительностью**.
- **Никаких «магических чисел».** Каждый настраиваемый параметр — именованная константа
  (см. раздел *Константы* в спеке).
- **Ошибки:** восстановимые отказы возвращают `std::expected<T, std::string>`; нарушения
  инвариантов в фабриках `make_*` бросают `std::invalid_argument`.
- **Форматирование и линт:** запускайте `just fmt` (clang-format) и `just lint` (clang-tidy)
  перед push. CI проверяет форматирование закреплённой версией **clang-format 18.1.8**
  (`pip install clang-format==18.1.8`) для детерминизма; clang-tidy — информационный.

## Тесты

- Каждая новая функция или поведение требует тестов. Где разумно, применяйте TDD: сначала
  падающий тест, затем реализация.
- Тесты должны быть детерминированными и независимыми от внешних сервисов.
- Перед открытием pull request прогоняйте весь набор локально через `just test`.

## Сообщения коммитов

- Чёткие императивные заголовки: `Add zstd codec option`, а не `added stuff`.
- Ссылайтесь на issue, где уместно: `Fix line splitting at buffer boundary (#42)`.
- Один коммит — одно логическое изменение.

## Pull request

1. Сделайте форк и создайте тематическую ветку от `main`.
2. Внесите изменение с тестами и обновлённой документацией/спекой, если нужно.
3. Убедитесь, что `just build`, `just test`, `just fmt` и `just lint` проходят — CI прогоняет
   те же проверки на каждый push и pull request (см.
   [`.github/workflows/ci.yml`](.github/workflows/ci.yml)).
4. Откройте pull request с описанием **что** изменилось и **почему**. Свяжите с issue.
5. Мейнтейнер проведёт ревью; отвечайте на замечания и держите ветку актуальной.

Внося вклад, вы соглашаетесь, что он лицензируется на условиях
[MIT License](LICENSE).
