import os
import glob
import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.patches import Patch

# ---------------------------
# Настройки
# ---------------------------

PAGES_COUNT = 17   # <-- твое число страниц
READER_LIMIT = 5   # сколько reader логов брать
WRITER_LIMIT = 5   # сколько writer логов брать

# Цвета состояний
STATE_COLORS = {
    'WAIT_READ_BEGIN': '#f4a261',
    'READING': '#2a9d8f',
    'RELEASE_READ_BEGIN': '#457b9d',
    'IDLE': '#bdbdbd',
    'WAIT_WRITE_BEGIN': '#e76f51',
    'WRITING': '#d62828',
    'RELEASE_WRITE_BEGIN': '#6a4c93',
}

# ---------------------------
# Чтение логов
# ---------------------------

def load_log_file(filename, proc_type):
    df = pd.read_csv(filename, sep=';')
    df['process_type'] = proc_type
    df['filename'] = os.path.basename(filename)
    return df

reader_files = [
    "reader_11628.log",
    "reader_14712.log",
    "reader_1988.log",
    "reader_21488.log",
    "reader_24840.log",
]

writer_files = [
    "writer_10932.log",
    "writer_11236.log",
    "writer_14108.log",
    "writer_15852.log",
    "writer_25224.log",
]

if len(reader_files) < READER_LIMIT or len(writer_files) < WRITER_LIMIT:
    print("Недостаточно log-файлов.")
    print("Найдено reader:", len(reader_files))
    print("Найдено writer:", len(writer_files))
    exit(1)

reader_dfs = [load_log_file(f, 'reader') for f in reader_files]
writer_dfs = [load_log_file(f, 'writer') for f in writer_files]

all_df = pd.concat(reader_dfs + writer_dfs, ignore_index=True)

# Выделяем номер страницы из поля info, например "page=12"
all_df['page'] = all_df['info'].str.extract(r'page=(\d+)').astype(float)

# Сортировка
all_df = all_df.sort_values(['pid', 'time']).reset_index(drop=True)

# Нормируем время, чтобы на графике было от нуля
t0 = all_df['time'].min()
all_df['time_rel'] = all_df['time'] - t0

# ---------------------------
# Построение графика состояний процессов
# ---------------------------

selected_pids = []
labels = []

# сначала 5 readers
for df in reader_dfs:
    pid = int(df.iloc[0]['pid'])
    selected_pids.append(pid)
    labels.append(f"Reader {pid}")

# потом 5 writers
for df in writer_dfs:
    pid = int(df.iloc[0]['pid'])
    selected_pids.append(pid)
    labels.append(f"Writer {pid}")

fig, ax = plt.subplots(figsize=(16, 8))

for i, pid in enumerate(selected_pids):
    proc_df = all_df[all_df['pid'] == pid].sort_values('time_rel').reset_index(drop=True)

    # Рисуем интервалы между текущим и следующим состоянием
    for j in range(len(proc_df) - 1):
        start = proc_df.loc[j, 'time_rel']
        end = proc_df.loc[j + 1, 'time_rel']
        state = proc_df.loc[j, 'state']
        color = STATE_COLORS.get(state, '#000000')

        ax.barh(
            y=i,
            width=end - start,
            left=start,
            height=0.7,
            color=color,
            edgecolor='black'
        )

    # Последнее состояние — короткий хвост
    if len(proc_df) > 0:
        start = proc_df.iloc[-1]['time_rel']
        state = proc_df.iloc[-1]['state']
        color = STATE_COLORS.get(state, '#000000')

        ax.barh(
            y=i,
            width=100,
            left=start,
            height=0.7,
            color=color,
            edgecolor='black'
        )

ax.set_yticks(range(len(selected_pids)))
ax.set_yticklabels(labels)
ax.invert_yaxis()
ax.set_xlabel("Время, мс (относительно начала эксперимента)")
ax.set_title("Смена состояний процессов (5 читателей и 5 писателей)")
ax.grid(axis='x', linestyle='--', alpha=0.5)

legend_elements = [
    Patch(facecolor=STATE_COLORS['WAIT_READ_BEGIN'], edgecolor='black', label='WAIT_READ_BEGIN'),
    Patch(facecolor=STATE_COLORS['READING'], edgecolor='black', label='READING'),
    Patch(facecolor=STATE_COLORS['RELEASE_READ_BEGIN'], edgecolor='black', label='RELEASE_READ_BEGIN'),
    Patch(facecolor=STATE_COLORS['WAIT_WRITE_BEGIN'], edgecolor='black', label='WAIT_WRITE_BEGIN'),
    Patch(facecolor=STATE_COLORS['WRITING'], edgecolor='black', label='WRITING'),
    Patch(facecolor=STATE_COLORS['RELEASE_WRITE_BEGIN'], edgecolor='black', label='RELEASE_WRITE_BEGIN'),
    Patch(facecolor=STATE_COLORS['IDLE'], edgecolor='black', label='IDLE'),
]
ax.legend(handles=legend_elements, bbox_to_anchor=(1.02, 1), loc='upper left')

plt.tight_layout()
states_path = os.path.abspath("states_timeline.png")
plt.savefig(states_path, dpi=300)
plt.show()

# ---------------------------
# Построение heatmap занятости страниц
# ---------------------------

activity_df = all_df[all_df['state'].isin(['READING', 'WRITING'])].copy()
activity_df = activity_df.sort_values(['pid', 'time_rel']).reset_index(drop=True)

BIN_SIZE = 200  # мс
max_time = all_df['time_rel'].max()
num_bins = int(max_time // BIN_SIZE) + 2

# Матрица: страницы x временные интервалы
heatmap = [[0 for _ in range(num_bins)] for _ in range(PAGES_COUNT)]

for pid in activity_df['pid'].unique():
    proc_df = activity_df[activity_df['pid'] == pid].sort_values('time_rel').reset_index(drop=True)

    for j in range(len(proc_df) - 1):
        state = proc_df.loc[j, 'state']
        page = proc_df.loc[j, 'page']
        start = proc_df.loc[j, 'time_rel']
        end = proc_df.loc[j + 1, 'time_rel']

        if pd.isna(page):
            continue

        page = int(page)
        val = 1 if state == 'READING' else 2

        start_bin = int(start // BIN_SIZE)
        end_bin = int(end // BIN_SIZE)

        for b in range(start_bin, end_bin + 1):
            if 0 <= page < PAGES_COUNT and 0 <= b < num_bins:
                heatmap[page][b] = max(heatmap[page][b], val)

fig, ax = plt.subplots(figsize=(16, 8))
im = ax.imshow(heatmap, aspect='auto', origin='lower', interpolation='nearest', cmap='coolwarm')

ax.set_xlabel(f"Временные интервалы по {BIN_SIZE} мс")
ax.set_ylabel("Номер страницы")
ax.set_title("Занятость страниц во времени (heatmap)\n0 — пусто, 1 — чтение, 2 — запись")

ax.set_yticks(range(PAGES_COUNT))
ax.set_yticklabels(range(PAGES_COUNT))

cbar = plt.colorbar(im, ax=ax)
cbar.set_ticks([0, 1, 2])
cbar.set_ticklabels(['Нет активности', 'Чтение', 'Запись'])

plt.tight_layout()
heatmap_path = os.path.abspath("pages_heatmap.png")
plt.savefig(heatmap_path, dpi=300)
plt.show()

# ---------------------------
# Вывод путей
# ---------------------------

print("Текущая рабочая папка:")
print(os.getcwd())

print("\nГрафики сохранены:")
print(states_path)
print(heatmap_path)