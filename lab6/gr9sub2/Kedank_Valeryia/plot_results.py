import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

def plot_throughput(df):
    """Строит график пропускной способности (чтение и запись)."""
    plt.style.use('seaborn-v0_8-whitegrid')
    fig, ax = plt.subplots(1, 2, figsize=(14, 6), sharey=True)
    
    # Данные для чтения и записи
    read_data = df[df['test_type'] == 'read']
    write_data = df[df['test_type'] == 'write']

    # График для записи
    sns.barplot(x='file_size_mb', y='value_mb_s', hue='fs_type', data=write_data, ax=ax[0])
    ax[0].set_title('Пропускная способность (Запись)')
    ax[0].set_xlabel('Размер файла (MB)')
    ax[0].set_ylabel('Скорость (MB/s)')
    ax[0].legend(title='Тип ФС')

    # График для чтения
    sns.barplot(x='file_size_mb', y='value_mb_s', hue='fs_type', data=read_data, ax=ax[1])
    ax[1].set_title('Пропускная способность (Чтение)')
    ax[1].set_xlabel('Размер файла (MB)')
    ax[1].set_ylabel('') # Ось Y общая

    fig.suptitle('Сравнение пропускной способности FUSE и нативной ФС', fontsize=16)
    plt.tight_layout(rect=[0, 0, 1, 0.96])
    plt.savefig('throughput.png')
    print("График 'throughput.png' сохранен.")

def plot_iops_latency(df):
    """Строит графики IOPS и задержки."""
    plt.style.use('seaborn-v0_8-whitegrid')
    fig, axes = plt.subplots(1, 2, figsize=(14, 6))

    # График IOPS
    iops_data = df[df['test_type'] == 'iops']
    sns.barplot(x='fs_type', y='value_mb_s', data=iops_data, ax=axes[0])
    axes[0].set_title('IOPS (создание 1000 мелких файлов)')
    axes[0].set_xlabel('Тип ФС')
    axes[0].set_ylabel('Операций в секунду')

    # График задержки
    latency_data = df[df['test_type'].str.startswith('latency')]
    latency_data = latency_data.copy()
    latency_data['operation'] = latency_data['test_type'].str.replace('latency_', '')
    sns.barplot(x='operation', y='value_mb_s', hue='fs_type', data=latency_data, ax=axes[1])
    axes[1].set_title('Средняя задержка операций')
    axes[1].set_xlabel('Тип операции')
    axes[1].set_ylabel('Задержка (мс)')

    fig.suptitle('Сравнение IOPS и задержки', fontsize=16)
    plt.tight_layout(rect=[0, 0, 1, 0.96])
    plt.savefig('iops_latency.png')
    print("График 'iops_latency.png' сохранен.")


def main():
    try:
        df = pd.read_csv('results.csv')
    except FileNotFoundError:
        print("Ошибка: файл 'results.csv' не найден.")
        print("Пожалуйста, сначала запустите скрипт 'benchmark.sh' для сбора данных.")
        return

    # Преобразование типов для корректной сортировки и отображения
    df['value_mb_s'] = pd.to_numeric(df['value_mb_s'])
    df['file_size_mb'] = pd.to_numeric(df['file_size_mb'])

    plot_throughput(df)
    plot_iops_latency(df)

if __name__ == '__main__':
    main()
