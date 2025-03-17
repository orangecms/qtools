#include "include.h"

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//* Чтение флеша модема в файл
//
//

// флаги способов обработки бедблоков
enum { BAD_UNDEF, BAD_FILL, BAD_SKIP, BAD_IGNORE, BAD_DISABLE };

// Формты читаемых данных
enum { RF_AUTO, RF_STANDART, RF_LINUX, RF_YAFFS };

int bad_processing_flag = BAD_UNDEF;
unsigned char *blockbuf;

// Структура для сохранения списка ошибок чтения

struct {
  int page;
  int sector;
  int errcode;
} errlist[1000];

int errcount;
int qflag = 0;

//********************************************************************************
//* Загрузка блока в блочный буфер
//*
//*  возвращает 0, если блок дефектный, или 1, если он нормально прочитался
//* cwsize - размер читаемого сектора (включая ООВ, если надо)
//********************************************************************************
unsigned int load_block(int blk, int cwsize) {

  int pg, sec;
  int oldudsize, cfg0;
  unsigned int cfgecctemp;
  int status;

  errcount = 0;
  if (bad_processing_flag == BAD_DISABLE)
    hardware_bad_off();
  else if (bad_processing_flag != BAD_IGNORE) {
    if (check_block(blk)) {
      // обнаружен бедблок
      memset(blockbuf, 0xbb,
             cwsize * spp * ppb); // заполняем блочный буфер заполнителем
      return 0; // возвращаем признак бедблока
    }
  }
  // хороший блок, или нам насрать на бедблоки - читаем блок

  // устанавливаем udsize на размер читаемого участка
  cfg0 = mempeek(nand_cfg0);
  // oldudsize=get_udsize();
  // set_udsize(cwsize);
  // set_sparesize(0);

  nand_reset();
  if (cwsize > (sectorsize + 4))
    mempoke(nand_cmd, 0x34); // чтение data+ecc+spare без коррекции
  else
    mempoke(nand_cmd, 0x33); // чтение data с коррекцией

  bch_reset();
  for (pg = 0; pg < ppb; pg++) {
    setaddr(blk, pg);
    for (sec = 0; sec < spp; sec++) {
      mempoke(nand_exec, 0x1);
      nandwait();
      status = check_ecc_status();
      if (status != 0) {
        //     printf("\n--- blk %x  pg %i  sec  %i err
        //     %i---\n",blk,pg,sec,check_ecc_status());
        errlist[errcount].page = pg;
        errlist[errcount].sector = sec;
        errlist[errcount].errcode = status;
        errcount++;
      }

      memread(blockbuf + (pg * spp + sec) * cwsize, sector_buf, cwsize);
      //   dump(blockbuf+(pg*spp+sec)*cwsize,cwsize,0);
    }
  }
  if (bad_processing_flag == BAD_DISABLE)
    hardware_bad_on();
  // set_udsize(oldudsize);
  mempoke(nand_cfg0, cfg0);
  return 1; // заебись - блок прочитан
}

//***************************************
//* Чтение блока данных в выходной файл
//***************************************
unsigned int read_block(int block, int cwsize, FILE *out) {

  unsigned int okflag = 0;

  okflag = load_block(block, cwsize);
  if (okflag || (bad_processing_flag != BAD_SKIP)) {
    // блок прочитался, или не прочитался, но мы бедблоки пропускаем
    fwrite(blockbuf, 1, cwsize * spp * ppb, out); // записываем его в файл
  }
  return !okflag;
}

//********************************************************************************
//* Чтение блока данных для разделов с защищенным spare (516-байтовые секторы)
//*   yaffsmode=0 - чтение данных,  1 - чтение данных и тега yaffs2
//********************************************************************************
unsigned int read_block_ext(int block, FILE *out, int yaffsmode) {
  unsigned int page, sec;
  unsigned int okflag;
  unsigned char *pgoffset;
  unsigned char *udoffset;
  unsigned char extbuf[2048]; // буфер для сборки псевдо-OOB

  okflag = load_block(block, sectorsize + 4);
  if (!okflag && (bad_processing_flag == BAD_SKIP))
    return 1; // обнаружен бедблок

  // цикл по страницам
  for (page = 0; page < ppb; page++) {
    pgoffset = blockbuf +
               page * spp * (sectorsize + 4); // смещение до текущей страницы
    // по секторам
    for (sec = 0; sec < spp; sec++) {
      udoffset =
          pgoffset + sec * (sectorsize + 4); // смещение до текущего сектора
      //   printf("\n page %i  sector %i\n",page,sec);
      if (sec != (spp - 1)) {
        // Для непоследних секторов
        fwrite(udoffset, 1, sectorsize + 4, out); // Тело сектора + 4 байта OBB
        //     dump(udoffset,sectorsize+4,udoffset-blockbuf);
      } else {
        // для последнего сектора
        fwrite(udoffset, 1, sectorsize - 4 * (spp - 1),
               out); // Тело сектора - хвост oob
        //     dump(udoffset,sectorsize-4*(spp-1),udoffset-blockbuf);
      }
    }

    // Чтение образов тега yafs2
    if (yaffsmode) {
      memset(extbuf, 0xff, oobsize);
      memcpy(extbuf,
             pgoffset + (sectorsize + 4) * (spp - 1) +
                 (sectorsize - 4 * (spp - 1)),
             16);
      //    printf("\n page %i oob\n",page);
      //    dump(pgoffset+(sectorsize+4)*(spp-1)+(sectorsize-4*(spp-1)),16,pgoffset+(sectorsize+4)*(spp-1)+(sectorsize-4*(spp-1))-blockbuf);
      fwrite(extbuf, 1, oobsize, out);
    }
  }

  return !okflag;
}

//*************************************************************
//* Чтение блока данных для нефайловых линуксовых разделов
//*************************************************************
unsigned int read_block_resequence(int block, FILE *out) {
  return read_block_ext(block, out, 0);
}

//*************************************************************
//* Чтение блока данных для файловых yaffs2 разделов
//*************************************************************
unsigned int read_block_yaffs(int block, FILE *out) {
  return read_block_ext(block, out, 1);
}

//****************************************
//* Вывод на экран списка ошибок чтения
//****************************************
void show_errlist() {

  int i;

  if (qflag || (errcount == 0))
    return; // ошибок не было
  for (i = 0; i < errcount; i++) {
    if (errlist[i].errcode == -1)
      printf("\n!   Страница %d  сектор %d: некорректируемая ошибка чтения",
             errlist[i].page, errlist[i].sector);
    else
      printf("\n!   Страница %d  сектор %d: скорректировано бит: %d",
             errlist[i].page, errlist[i].sector, errlist[i].errcode);
  }
  printf("\n");
}

//*****************************
//* чтение сырого флеша
//*****************************
void read_raw(int start, int len, int cwsize, FILE *out, unsigned int rflag) {

  int block;
  unsigned int badflag;

  printf("\n Чтение блоков %08x - %08x", start, start + len - 1);
  printf("\n Формат данных: %u+%i\n", sectorsize, cwsize - sectorsize);
  // главыный цикл
  // по блокам
  for (block = start; block < (start + len); block++) {
    printf("\r Блок: %08x", block);
    fflush(stdout);
    switch (rflag) {
    case RF_AUTO:
    case RF_STANDART:
      badflag = read_block(block, cwsize, out);
      break;

    case RF_LINUX:
      badflag = read_block_resequence(block, out);
      break;

    case RF_YAFFS:
      badflag = read_block_yaffs(block, out);
      break;
    }
    show_errlist();
    if (badflag != 0)
      printf(" - Badblock\n");
  }
  printf("\n");
}

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
void main(int argc, char *argv[]) {

  unsigned char filename[300] = {0};
  unsigned int i, block, filepos, lastpos;
  unsigned char c;
  unsigned int start = 0, len = 0, opt;
  unsigned int partlist[60]; // список разделов, разрешенных для чтения
  unsigned int
      cwsize; // размер порции данных, читаемых из секторного буфера за один раз
  FILE *out;
  int partflag = 0; // 0 - сырой флеш, 1 - работа с таблицец разделов
  int eccflag = 0;  // 1 - отключить ECC,  0 - включить
  int partnumber =
      -1; // флаг выбра раздела для чтения, -1 - все разделы, 1 - по списку
  int rflag = RF_AUTO; // формат разделов: 0 - авто, 1 - стандартный, 2 -
                       // линуксокитайский, 3- yaffs2
  int listmode = 0; // 1- вывод карты разделов
  int truncflag = 0; //  1 - отрезать все FF от конца раздела
  int xflag = 0; //
  unsigned int badflag;

  int forced_oobsize = -1;

  // Источник таблицы разделов. По умолчанию - раздел MIBIB на флешке
  char ptable_file[200] = "@";

#ifndef WIN32
  char devname[50] = "/dev/ttyUSB0";
#else
  char devname[50] = "";
#endif

  memset(partlist, 0,
         sizeof(partlist)); // очищаем список разрешенных к чтению разделов

  while ((opt = getopt(argc, argv, "hp:b:l:o:xs:ef:mtk:r:z:u:q")) != -1) {
    switch (opt) {
    case 'h':

      printf(
          "\n Утилита предназначена для чтения образа флеш-памяти через модифицированный загрузчик\n\
 Допустимы следующие ключи:\n\n\
-p <tty> - последовательный порт для общения с загрузчиком\n\
-e - отключить коррекцию ECC при чтении\n\
-x - читать полный сектор - данные+oob (без ключа - только данные)\n\n\
-k # - код чипсета (-kl - получить список кодов)\n\
-z # - размер OOB на страницу, в байтах (перекрывает автоопределенный размер)\n\
-q   - запретить вывод списка ошибок чтения\n\
-u <x> - определяет режим обработки дефектных блоков:\n\
   -uf - заполнить образ дефектного блока в выходном файле байтом 0xbb\n\
   -us - пропустить дефектные блоки при чтении\n\
   -ui - игнорировать маркер дефектного блока (читать как читается)\n\
   -ux - отключить аппаратный контроль дефекных блоков\n");
      printf(
          "\n * Для режима неформатированного чтения и проверки дефектных блоков:\n\
-b <blk> - начальный номер читаемого блока (по умолчанию 0)\n\
-l <num> - число читаемых блоков, 0 - до конца флешки\n\
-o <file> - имя выходного файла (по умолчанию qflash.bin)(только для режима чтения)\n\n\
 * Для режима чтения разделов:\n\n\
-s <file> - взять карту разделов из файла\n\
-s @ - взять карту разделов из флеша (по умолчанию для -f и -m)\n\
-s - - взять карту разделов из файла ptable/current-r.bin\n\
-m   - вывести полную карту разделов и завершить работу\n\
-f n - читать только раздел c номером n (может быть указан несколько раз для формирования списка разделов)\n\
-f * - читать все разделы\n\
-t - отрезать все FF за последним значимым байтом раздела\n\
-r <x> - формат данных:\n\
    -ra - (по умолчанию и только для режима разделов) автоопределение формата по атрибуту раздела\n\
    -rs - стандартный формат (512-байтные блоки)\n\
    -rl - линуксовый формат (516-байтные блоки, кроме последнего на странице)\n\
    -ry - формат yaffs2 (страница+тег)\n\
");
      return;

    case 'k':
      define_chipset(optarg);
      break;

    case 'p':
      strcpy(devname, optarg);
      break;

    case 'e':
      eccflag = 1;
      break;

    case 'o':
      strcpy(filename, optarg);
      break;

    case 'b':
      sscanf(optarg, "%x", &start);
      break;

    case 'l':
      sscanf(optarg, "%x", &len);
      break;

    case 'z':
      sscanf(optarg, "%u", &forced_oobsize);
      break;

    case 'u':
      if (bad_processing_flag != BAD_UNDEF) {
        printf("\n Дублированный ключ u - ошибка\n");
        return;
      }
      switch (*optarg) {
      case 'f':
        bad_processing_flag = BAD_FILL;
        break;
      case 'i':
        bad_processing_flag = BAD_IGNORE;
        break;
      case 's':
        bad_processing_flag = BAD_SKIP;
        break;
      case 'x':
        bad_processing_flag = BAD_DISABLE;
        break;
      default:
        printf("\n Недопустимое значение ключа u\n");
        return;
      }
      break;

    case 'r':
      switch (*optarg) {
      case 'a':
        rflag = RF_AUTO; // авто
        break;
      case 's':
        rflag = RF_STANDART; // стандартный
        break;
      case 'l':
        rflag = RF_LINUX; // линуксовый
        break;
      case 'y':
        rflag = RF_YAFFS;
        break;
      default:
        printf("\n Недопустимое значение ключа r\n");
        return;
      }
      break;

    case 'x':
      xflag = 1;
      break;

    case 's':
      partflag = 1;
      strcpy(ptable_file, optarg);
      break;

    case 'm':
      partflag = 1; // форсируем режим разделов
      listmode = 1;
      break;

    case 'f':
      partflag = 1; // форсируем режим разделов
      if (optarg[0] == '*') {
        // все разделы
        partnumber = -1;
        break;
      }
      // построение списка разделов
      partnumber = 1;
      sscanf(optarg, "%u", &i);
      partlist[i] = 1;
      break;

    case 't':
      truncflag = 1;
      break;

    case 'q':
      qflag = 1;
      break;

    case '?':
    case ':':
      return;
    }
  }

  // Проверяем на запуск без ключей
  if ((start == 0) && (len == 0) && !xflag && !partflag) {
    printf("\n Не указан ни один ключ режима работы\n");
    return;
  }

  // Определяем значение ключа -u по умолчанию
  if (bad_processing_flag == BAD_UNDEF) {
    if (partflag == 0)
      bad_processing_flag = BAD_FILL; // для чтения диапазона блоков
    else
      bad_processing_flag = BAD_SKIP; // для чтения разделов
  }

  if ((truncflag == 1) && (xflag == 1)) {
    printf("\nКлючи -t и -x несовместимы\n");
    return;
  }

  // Настройка параметров порта и флешки
  // В режиме вывода карты разделов из файла вся эта настройка пропускается
  //----------------------------------------------------------------------------
  if (!(listmode && ptable_file[0] != '@')) {

#ifdef WIN32
    if (*devname == '\0') {
      printf("\n - Последовательный порт не задан\n");
      return;
    }
#endif

    if (!open_port(devname)) {
#ifndef WIN32
      printf("\n - Последовательный порт %s не открывается\n", devname);
#else
      printf("\n - Последовательный порт COM%s не открывается\n", devname);
#endif
      return;
    }
    // загружаем параметры флешки
    hello(0);
    // разпределяем память под блочный буфер
    blockbuf = (unsigned char *)malloc(300000);

    if (forced_oobsize != -1) {
      printf("\n! Используется размер OOB %d вместо %d\n", forced_oobsize,
             oobsize);
      oobsize = forced_oobsize;
    }

    cwsize = sectorsize;
    if (xflag)
      cwsize += oobsize / spp; // наращиваем размер codeword на размер порции
                               // OOB на каждый сектор
    mempoke(nand_ecc_cfg,
            mempeek(nand_ecc_cfg) & 0xfffffffe | eccflag);         // ECC on/off
    mempoke(nand_cfg1, mempeek(nand_cfg1) & 0xfffffffe | eccflag); // ECC on/off

    mempoke(nand_cmd, 1); // Сброс всех операций контроллера
    mempoke(nand_exec, 0x1);
    nandwait();

    // устанавливаем код команды
    mempoke(nand_cmd, 0x34); // чтение data+ecc+spare

    // чистим секторный буфер
    for (i = 0; i < cwsize; i += 4)
      mempoke(sector_buf + i, 0xffffffff);
  }

  //###################################################
  // Режим чтения сырого флеша
  //###################################################

  if (partflag == 0) {

    if (len == 0)
      len = maxblock - start; //  до конца флешки
    if (filename[0] == 0) {
      switch (rflag) {
      case RF_AUTO:
      case RF_STANDART:
        strcpy(filename, "qrflash.bin");
        break;
      case RF_LINUX:
        strcpy(filename, "qrflash.oob");
        break;
      case RF_YAFFS:
        strcpy(filename, "qrflash.yaffs");
        break;
      }
    }
    out = fopen(filename, "wb");
    if (out == 0) {
      printf("\n Ошибка открытия выходного файла %s", filename);
      return;
    }
    read_raw(start, len, cwsize, out, rflag);
    fclose(out);
    return;
  }

  //###################################################
  // Режим чтения по таблице разделов
  //###################################################

  // загружаем таблицу разделов
  if (!load_ptable(ptable_file)) {
    printf("\n Таблица разделов не найдена. Завершаем работу.\n");
    return;
  }

  // проверяем, загрузилась ли таблица
  if (!validpart) {
    printf("\nТаблица разделов не найдена или повреждена\n");
    return;
  }

  // Режим просмотра таблицы разделов
  if (listmode) {
    list_ptable();
    return;
  }

  if ((partnumber != -1) && (partnumber >= fptable.numparts)) {
    printf("\nНедопустимый номер раздела: %i, всего разделов %u\n", partnumber,
           fptable.numparts);
    return;
  }

  print_ptable_head();

  // Главный цикл - по разделам
  for (i = 0; i < fptable.numparts; i++) {

    // Читаем раздел

    // Если не задан режим всех разделов - проверяем, выбран ли именно этот
    // раздел
    if ((partnumber == -1) || (partlist[i] == 1)) {
      // Выводим описание раздела
      show_part(i);
      // формируем имя файла в зависимости от формата
      if (rflag == RF_YAFFS)
        sprintf(filename, "%02u-%s.yaffs2", i, part_name(i));
      else if (cwsize == sectorsize)
        sprintf(filename, "%02u-%s.bin", i, part_name(i));
      else
        sprintf(filename, "%02u-%s.oob", i, part_name(i));
      // заменяем двоеточие на минус в имени файла
      if (filename[4] == ':')
        filename[4] = '-';
      // открываем выходной файл
      out = fopen(filename, "wb");
      if (out == 0) {
        printf("\n Ошибка открытия выходного файла %s\n", filename);
        return;
      }
      // Цикл по блокам раздела
      for (block = part_start(i); block < (part_start(i) + part_len(i));
           block++) {
        printf("\r * R: блок %06x [start+%03x] (%i%%)", block,
               block - part_start(i),
               (block - part_start(i) + 1) * 100 / part_len(i));
        fflush(stdout);

        //	  Собственно чтение блока
        switch (rflag) {
        case RF_AUTO: // автовыбор формата
          if ((fptable.part[i].attr2 != 1) || (cwsize > (sectorsize + 4)))
            // сырое чтение или чтение неизварщенных разделов
            badflag = read_block(block, cwsize, out);
          else
            // чтение китайсколинуксовых разделов
            badflag = read_block_resequence(block, out);
          break;

        case RF_STANDART: // стандартный формат
          badflag = read_block(block, cwsize, out);
          break;

        case RF_LINUX: // китайсколинуксовый формат
          badflag = read_block_resequence(block, out);
          break;

        case RF_YAFFS: // образ файловых разделов
          badflag = read_block_yaffs(block, out);
          break;
        }
        // выводим список найденных ошибок
        show_errlist();
        if (badflag != 0) {
          printf(" - дефектный блок");
          if (bad_processing_flag == BAD_SKIP)
            printf(", пропускаем");
          if (bad_processing_flag == BAD_IGNORE)
            printf(", читаем как есть");
          if (bad_processing_flag == BAD_FILL)
            printf(", отмечаем в выходном файле");
          printf("\n");
        }
      } // конец цикла по блокам

      fclose(out);
      // Обрезка всех FF хвоста
      if (truncflag) {
        out = fopen(filename, "r+b"); // переоткрываем выходной файл
        fseek(out, 0, SEEK_SET); // перематываем файл на начало
        lastpos = 0;
        for (filepos = 0;; filepos++) {
          c = fgetc(out);
          if (feof(out))
            break;
          if (c != 0xff)
            lastpos = filepos; // нашли значимый байт
        }
#ifndef WIN32
        ftruncate(fileno(out), lastpos + 1); // обрезаем файл
#else
        _chsize(_fileno(out), lastpos + 1); // обрезаем файл
#endif
        fclose(out);
      }
    } // проверка выбора раздела
  }   // цикл по разделам

  // восстанавливаем ЕСС
  mempoke(nand_ecc_cfg, mempeek(nand_ecc_cfg) & 0xfffffffe); // ECC on BCH
  mempoke(nand_cfg1, mempeek(nand_cfg1) & 0xfffffffe);       // ECC on R-S

  printf("\n");
}
