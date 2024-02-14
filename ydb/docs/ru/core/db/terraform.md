# Управление {{ ydb-short-name }} с помощью Terraform

С помощью [Terraform](https://www.terraform.io/) можно создавать, удалять и изменять следующие объекты внутри кластера {{ ydb-short-name }}:

* [таблицы](../concepts/datamodel/table.md);
* [индексы](../concepts/secondary_indexes.md) таблиц;
* [потоки изменений](../concepts/cdc.md) таблиц;
* [топики](../concepts/topic.md).

{% note info %}

На данный момент провайдер находится на стадии разработки и его функциональность будет расширяться

{% endnote %}

Чтобы начать работать с Terraform для {{ ydb-short-name }}, установите и настройте [Terraform provider for {{ ydb-short-name }}](https://github.com/ydb-platform/terraform-provider-ydb/).

{% note warning %}

Для работы с ресурсами Terraform, БД должна быть создана заранее.

{% endnote %}

## Настройка Terraform провайдера для работы с {{ ydb-short-name }} {#setup}

1. Нужно скачать [код провайдера](https://github.com/ydb-platform/terraform-provider-ydb/)
2. Собрать провайдер, выполнив `make local-build`
Провайдер установится в папку плагинов Terraform - `~/.terraform.d/plugins/terraform.storage.ydb.tech/...`
3. Настроить `~/.terraformrc`, добавив в секцию `provider_installation` (если такая секция уже есть, то в текущую) следующее содержание:

    ```tf
    direct {
      exclude = ["terraform.storage.ydb.tech/*/*"]
    }

    filesystem_mirror {
      path    = "/PATH_TO_HOME/.terraform.d/plugins"
      include = ["terraform.storage.ydb.tech/*/*"]
    }
    ```

4. Далее настраиваем сам {{ ydb-short-name }} провайдер для работы:

    ```tf
    terraform {
      required_providers {
        ydb = {
          source = "terraform.storage.ydb.tech/provider/ydb"
        }
      }
      required_version = ">= 0.13"
    }
    
    provider "ydb" {
      token = ""
    }
    ```

## Использование Terraform провайдера {{ ydb-short-name }} {#work-with-tf}

### Соединение с БД {#connection_string}

Для всех ресурсов, описывающих объекты схемы данных, необходимо задать реквизиты БД, в которой они размещаются. Для этого укажите один из двух аргументов:

* Строка соединения `connection_string` — выражение вида `grpc(s)://HOST:PORT/?database=/database/path`, где `grpc(s)://HOST:PORT/` эндпоинт, а `/database/path` — путь БД.
  Например, `grpcs://example.com:2135?database=/Root/testdb0`.
* `database_endpoint` - алиас к первому пункту `connection_string` используется при создании/изменении топиков.
* `token` - указывается токен доступа к БД, если он необходим.

Если на сервере {{ ydb-short-name }} не включена авторизация, то в конфиге БД [config.yaml](../deploy/configuration/config.md) нужно указать:

```yaml
...
pqconfig:
  require_credentials_in_new_protocol: false
  check_acl: false
```

если вы используете создание ресурсов `ydb_table_changefeed` или `ydb_topic`.

### Построчная таблица (поколоночные пока не доступны) {#ydb-table}

Для работы с таблицами используется ресурс `ydb_table`.

Пример:

```tf
  resource "ydb_table" "ydb_table" {
    path = "path/to/table"
    connection_string = "grpc(s)://HOST:PORT/?database=/database/path" # в примерах ниже мы поместим в переменную tf
    column {
      name = "a"
      type = "Utf8"
      not_null = true
    }
    column {
      name = "b"
      type = "Uint32"
      not_null = true
    }
    column {
      name = "c"
      type = String
      not_null = false
    }
    column {
      name = "f"
      type = "Utf8"
    }
    column {
      name = "e"
      type = "String"
    }
    column {
      name = "d"
      type = "Timestamp"
    }
    primary_key = ["b", "a"]
  }
```

Поддерживаются следующие аргументы:

* `path` — (обязательный) путь таблицы.
* `connection_string` — строка соединения.

* `column` — (обязательный) свойства колонки (см. аргумент [column](#column)).
* `family` — (необязательный) группа колонок (см. аргумент [family](#family)).
* `primary_key` — (обязательный) [первичный ключ](../yql/reference/syntax/create_table.md#columns) таблицы, содержит список колонок первичного ключа.
* `ttl` — (необязательный) TTL (см. аргумент [ttl](#ttl)).
* `attributes` — (необязательный) атрибуты таблицы.
* `partitioning_settings` — (необязательный) настройки партицирования (см. аргумент [partitioning_settings](#partitioning-settings)).
* `key_bloom_filter` — (необязательный) (true|false) использовать [фильтра Блума для первичного ключа](../concepts/datamodel/table.md#bloom-filter).
* `read_replicas_settings` — (необязательный) настройки [репликации](../concepts/datamodel/table.md#read_only_replicas).

#### column {#column}

Аргумент `column` описывает [свойства колонки](../yql/reference/syntax/create_table.md#columns) таблицы.

{% note warning %}

При помощи Terraform нельзя удалить колонку, можно только добавить. Чтобы удалить колонку, используйте средства {{ ydb-short-name }}, затем удалите колонку из описания ресурса. При попытке "прокатки" изменений колонок таблицы (смена типа, имени), Terraform не попытается их удалить, а попытается сделать update-in-place, но изменения применены не будут.

{% endnote %}

Пример:

```tf
column {
  name     = "column_name"
  type     = "Utf8"
  family   = "some_family"
  not_null = true
}
```

* `name` — (обязательный) имя колонки.
* `type` — (обязательный) [тип данных YQL](../yql/reference/types/primitive.html) колонки.
* `family` — (необязательный) свойства группы колонок (см. аргумент [family](#family)).
* `not_null` — (необязательный) колонка не может содержать `NULL`. Значение по умолчанию: `false`.

#### family {#family}

Аргумент `family` описывает [свойства группы колонок](../yql/reference/syntax/create_table.md#column-family).

* `name` — (обязательный) имя группы колонок.
* `data` — (обязательный) [тип устройства хранения](../yql/reference/syntax/create_table#column-family) для данных колонок этой группы.
* `compression` — (обязательный) [кодек сжатия данных](../yql/reference/syntax/create_table#column-family).

Пример:

```tf
family {
  name        = "my_family"
  data        = "ssd"
  compression = "lz4"
}
```

#### partitioning_settings {#partitioning-settings}

Аргумент `partitioning_settings` описывает [настройки партицирования](../concepts/datamodel/table.md#partitioning).

Пример:

```tf
partitioning_settings = {
  auto_partitioning_min_partitions_count = 5
  auto_partitioning_max_partitions_count = 8
  auto_partitioning_partition_size_mb    = 256
  auto_partitioning_by_load              = true
}
```

* `uniform_partitions` — (необязательный) количество [заранее аллоцированных партиций](../concepts/datamodel/table.md#uniform_partitions).
* `partition_at_keys` — (необязательный) [партицирование по первичному ключу](../concepts/datamodel/table.md#partition_at_keys).
* `auto_partitioning_min_partitions_count` — (необязательный) [минимально возможное количество партиций](../concepts/datamodel/table.md#auto_partitioning_min_partitions_count) при автопартицировании.
* `auto_partitioning_max_partitions_count` — (необязательный) [максимально возможное количество партиций](../concepts/datamodel/table.md#auto_partitioning_max_partitions_count) при автопартицировании.
* `auto_partitioning_partition_size_mb` — (необязательный) задание значения [автопартицирования по размеру](../concepts/datamodel/table.md#auto_partitioning_partition_size_mb).
* `auto_partitioning_by_size_enabled` — (необязательный) включение автопартиционирования по размеру (bool), по умолчанию - включено.
* `auto_partitioning_by_load` — (необязательный) включение [автопартицирования по нагрузке](../concepts/datamodel/table.md#auto_partitioning_by_load).

#### ttl {#ttl}

Аргумент `ttl` описывает настройки [Time To Live](../concepts/ttl.md).

{% note info %}

Изменение TTL производится через удаление и последующее создание.

{% endnote %}

Пример:

```tf
ttl {
  column_name     = "column_name"
  expire_interval = "PT1H" # 1 час
  unit            = "seconds" # для числовых типов колонок (non ISO 8601)
}
```

* `column_name` — (обязательный) имя колонки для TTL.
* `expire_interval` — (обязательный) интервал в формате [ISO 8601](https://ru.wikipedia.org/wiki/ISO_8601) (например P1D продолжительность длиной 1 сутки (24 часа)).
* `unit` — (необязательный) задается в случае, если колонка с ttl имеет числовой тип. Поддерживаемые значения:
  * `seconds`
  * `milliseconds`
  * `microseconds`
  * `nanoseconds`

### Вторичный индекс таблицы {#ydb-table-index}

Для работы с индексом таблицы используется ресурс `ydb_table_index`.

{% note info %}

Изменение индекса таблицы производится через удаление и последующее создание.

{% endnote %}

Пример:

```tf
resource "ydb_table_index" "ydb_table_index" {
  table_path        = "path/to/table"
  connection_string = "grpc(s)://HOST:PORT/?database=/database/path"
  name              = "my_index"
  type              = "global_sync" # "global_async"
  columns           = ["a", "b"]
  cover             = ["c"]
}
```

Поддерживаются следующие аргументы:

* `table_path` — путь таблицы. Указывается, если не задан `table_id`.
* `connection_string` — [строка соединения](#connection_string). Указывается, если не задан `table_id`.
* `table_id` - terraform-идентификатор таблицы. Указывается, если не задан `table_path` или `connection_string`.

* `name` — (обязательный) имя индекса.
* `type` — (обязательный) тип индекса [global_sync | global_async](../yql/reference/syntax/create_table#secondary_index).
* `columns` — (обязательный) колонки в индексе.
* `cover` — (обязательный) колонки для покрывающего индекса.

### Поток изменений таблицы {#ydb-table-changefeed}

Для работы с [потоком изменений](../concepts/cdc.md) таблицы используется ресурс `ydb_table_changefeed`.

{% note info %}

Изменение потока производится через удаление и последующее создание.

{% endnote %}

Пример:

```tf
resource "ydb_table_changefeed" "ydb_table_changefeed" {
  table_id = ydb_table.ydb_table.id
  name     = "changefeed"
  mode     = "NEW_IMAGE"
  format   = "JSON"
}
```

Поддерживаются следующие аргументы:

* `table_path` — путь таблицы. Указывается, если не задан `table_id`.
* `connection_string` — [строка соединения](#connection_string). Указывается, если не задан `table_id`.
* `table_id` — terraform-идентификатор таблицы. Указывается, если не задан `table_path` или `connection_string`.

* `name` — (обязательный) имя потока изменений.
* `mode` — (обязательный) режим работы [потока изменений](../yql/reference/syntax/alter_table#changefeed-options).
* `format` — (обязательный) формат [потока изменений](../yql/reference/syntax/alter_table#changefeed-options).
* `virtual_timestamps` — (необязательный) использование [виртуальных таймстеймпов](../concepts/cdc.md#virtual-timestamps).
* `retention_period` — (необязательный) время хранения данных в формате [ISO 8601](https://ru.wikipedia.org/wiki/ISO_8601).
* `consumer` — (необязательный) читатель потока изменений (см. аргумент [#consumer](#consumer)).

#### consumer {#consumer}

Аргумент `consumer` описывает [читателя](../best_practices/cdc#read) потока изменений.

* `name` — (обязательный) имя читателя.
* `supported_codecs` — (необязательный) поддерживаемые кодек данных.
* `starting_message_timestamp_ms` — (необязательный) временная метка в формате [UNIX timestamp](https://ru.wikipedia.org/wiki/Unix-время), с которой читатель начнет читать данные.

### Примеры использования {manage-examples}

#### Создание таблицы в существующей БД {#example-with-connection-string}

```tf
provider "ydb" {
  token = ""
}

variable "my_db_connection_string" {
  type = string
  default = "grpc(s)://HOST:PORT/?database=/database/path" # можно передавать другими способами в tf
}

resource "ydb_table" "ydb_table" {
  # Путь до таблицы
  path = "path/tf_table" # Создать таблицу по пути `path/tf_table`

  connection_string = var.my_db_connection_string

  column {
    name = "a"
    type = "Uint64"
    not_null = true
  }
  column {
    name     = "b"
    type     = "Uint32"
    not_null = true
  }
  column {
    name = "c"
    type = String
    not_null = false
  }
  column {
    name = "f"
    type = "Utf8"
  }
  column {
    name = "e"
    type = "String"
  }
  column {
    name = "d"
    type = "Timestamp"
  }
  # Первичный ключ
  primary_key = [
    "a", "b"
  ]
}
```

#### Создание таблицы, индекса и потока изменений {#example-with-table}

```tf
provider "ydb" {
  token = ""
}

resource "ydb_table" "ydb_table" {
  # Путь до таблицы
  path = "path/tf_table" # Создать таблицу по пути `path/tf_table`
  
  # ConnectionString до базы данных.
  connection_string = var.my_db_connection_string

  column {
    name = "a"
    type = "Uint64"
    not_null = true
  }
  column {
    name     = "b"
    type     = "Uint32"
    not_null = true
  }
  column {
    name = "c"
    type = "Utf8"
  }
  column {
    name = "f"
    type = "Utf8"
  }
  column {
    name = "e"
    type = "String"
  }
  column {
    name = "d"
    type = "Timestamp"
  }

  # Первичный ключ
  primary_key = [
    "a", "b"
  ]


  ttl {
    column_name     = "d"
    expire_interval = "PT5S"
  }

  partitioning_settings {
    auto_partitioning_by_load = false
    auto_partitioning_partition_size_mb    = 256
    auto_partitioning_min_partitions_count = 6
    auto_partitioning_max_partitions_count = 8
  }

  read_replicas_settings = "PER_AZ:1"

  key_bloom_filter = true # Default = false
}

resource "ydb_table_changefeed" "ydb_table_changefeed" {
  table_id = ydb_table.ydb_table.id
  name = "changefeed"
  mode = "NEW_IMAGE"
  format = "JSON"

  consumer {
    name = "test_consumer"
  }
}

resource "ydb_table_index" "ydb_table_index" {
  table_id = ydb_table.ydb_table.id
  name = "some_index"
  columns = ["c", "d"]
  cover = ["e"]
  type = "global_sync"
}
```

## Управление конфигурацией топиков {{ydb-short-name}} через Terraform

Описание [топиков](../concepts/topic.md)

Топик не может быть создан в корне БД, нужно указать хотя бы один каталог в имени топика.

### Описание ресурса `ydb_topic`

Пример:

```tf
resource "ydb_topic" "ydb_topic" {
  database_endpoint = var.my_db_connection_string
  name              = "test/test1"
  supported_codecs  = ["zstd"]

  consumer {
    name             = "test-consumer1"
    starting_message_timestamp_ms = 0
    supported_codecs = ["zstd","raw"]
  }

  consumer {
    name             = "test-consumer2"
    starting_message_timestamp_ms = 2000
    supported_codecs = ["zstd"]
  }

  consumer {
    name             = "test-consumer3"
    starting_message_timestamp_ms = 0
    supported_codecs = ["zstd"]
  }
}
```

Поддерживаются следующие аргументы:

* `name` - (обязательный) имя топика.
* `database_endpoint` - (обязательный) полный путь до базы данных, например: `"grpcs://example.com:2135/?database=/Root/testdb0"` (передаем в переменной tf - `var.my_db_connection_string`).
* `retention_period_ms` - время хранения данных, значение по умолчанию - `86400000` миллисекунд.
* `partitions_count` - количество партиций, значение по умолчанию - `2`.
* `supported_codecs` - поддерживаемые кодировки сжатия данных, значение по умолчанию - `"gzip", "raw", "zstd"` (всего 3 типа кодеков).
* `consumer` - читатели для топика.

Описание потребителя данных `consumer`:

* `name` - (обязательное) имя читателя.
* `supported_codecs` - поддерживаемые кодировки сжатия данных, по умолчанию - `"gzip", "raw", "zstd"` (всего 3 типа кодеков).
* `starting_message_timestamp_ms` - временная метка в формате UNIX timestamp, с которой читатель начнёт читать данные, по умаолчанию - 0, то есть с начала поставки.
