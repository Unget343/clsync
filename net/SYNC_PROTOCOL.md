# Синхронізація з C#-сервісом

`clsync` запускає `SyncManager` з двома ізольованими чергами: за замовчуванням два воркери для малих файлів і два — для великих. Стан сервісу перевіряється через переданий health URL; коли сервіс недоступний, нові мережеві задачі залишаються в черзі до відновлення.

```text
clsync <local-root> <health-url> <IPC command> [IPC command ...]
```

Команди, які передаються через наявний шар Reborn:

```text
DOWNLOAD <id> <http(s)-url> <relative-local-path> <expected-size|-> <sha256|->
UPLOAD   <id> <http(s)-url> <relative-local-path>
STOP
```

Відносний шлях завжди перевіряється щодо `local-root`; абсолютні шляхи, `..` і symlink-вихід за межі каталогу відхиляються. Завантаження пишеться до `.part`, перевіряє розмір і SHA-256 та лише тоді перейменовується у фінальний файл.

Налаштування: `CLSYNC_USER_AGENT`, `CLSYNC_SMALL_WORKERS`, `CLSYNC_LARGE_WORKERS`, `CLSYNC_LARGE_FILE_THRESHOLD`, `CLSYNC_MAX_RETRIES`, `CLSYNC_TIMEOUT_MS`, `CLSYNC_RETRY_DELAY_MS`, `CLSYNC_MONITOR_INTERVAL_MS`, `CLSYNC_SOCKET_PATH`.

Паралельні range-завантаження одного файлу свідомо не вмикаються без погодженого C# API для `Range` і метаданих частин; звичайне потокове завантаження великих файлів працює без збереження файлу в RAM.
