// JCFS.cpp : 定义控制台应用程序的入口点。
//

// TODO：
// 以后可以改进的地方
// 1. 选择在一个父目录下添加fcb时，应该选择其磁盘上第一个空闲的fcb块作为新添加fcb的位置，以增大磁盘的利用率，本代码因为时间匆忙，不在细节上做过多的优化
// 2. fcb的data和time两个成员变量应该是记录一个文件的创建时间的（按理说应该还有文件的最新更新时间也要记录）因为实验要求里的逻辑无法验证这两个变量是否存在，为了缩短编程的时间，没有任何与这两个成员变量相关的操作
// 3. fcb的exname成员变量记录的是文件的后缀名，与2同理，实验要求里的逻辑无法验证这个变量是否存在（因为可以把文件名加上后缀名一起作为文件名），所以代码里目前也没有与这个成员变量相关的操作
// 4. main里面其实就是一个读入命令和参数，然后执行相关命令的过程，所以应该一次直接读入一行，然后判断里面的命令和参数是否合法，但自己的写法没有判断换行，有时会导致错误的发生
// 5. 代码应该尽量抽象，使用更多的c高级语法（虽然这个文件是cpp文件，但是里面按某种理由来说，应该不能出现任何c++语法），以降低后期的维护成本（当然我验收完就不会用这个代码了 :) ）

#include "stdafx.h"
#include "JCFS.h"

const char USERNAME[] = "jiachen";

unsigned char *myvhard;
useropen openfilelist[MAXOPENFILE];
int curdirid; // 指向用户打开文件表中的当前目录所在打开文件表项的位置
unsigned char* startp; // 记录虚拟磁盘上数据区开始位置

unsigned char buf[SIZE];
unsigned char *blockaddr[BLOCKNUM];
block0 initblock;
fat fat1[BLOCKNUM], fat2[BLOCKNUM];

int main() {
  int fd;
  char command[DIRLEN << 1];

  startsys();
  printf("%s %s: ", USERNAME, openfilelist[curdirid].dir);
  while (~scanf("%s", command)) {
    if (!strcmp(command, "exit")) {
      break;
    }
    else if (!strcmp(command, "ls")) {
      my_ls();
    }
    else if (!strcmp(command, "mkdir")) {
      scanf("%s", command);
      my_mkdir(command);
    }
    else if (!strcmp(command, "close")) {
      scanf("%d", &fd);
      my_close(fd);
    }
    else if (!strcmp(command, "open")) {
      scanf("%s", command);
      fd = my_open(command);
      if (0 <= fd && fd < MAXOPENFILE) {
        if (!openfilelist[fd].open_fcb.attribute) {
          my_close(fd);
          printf("%s is dirictory, please use cd command\n", command);
        }
        else {
          openfilelist[fd].count = 0;
          printf("%s is open, it\'s id is %d\n", openfilelist[fd].dir, fd);
        }
      }
    }
    else if (!strcmp(command, "cd")) {
      scanf("%s", command);
      my_cd(command);
    }
    else {
      printf("command %s : no such command\n", command);
    }

    printf("%s %s: ", USERNAME, openfilelist[curdirid].dir);
  }

  my_exitsys();
  return 0;
}

int strcmp(const char* a, const char *b, int len) {
  for (int i = 0; i < len; ++i) if (a[i] != b[i]) {
    if (a[i] < b[i]) return -1;
    else return 1;
  }
  return 0;
}

void fcb_init(fcb *new_fcb, const char* filename, unsigned short first, unsigned long length, unsigned char attribute) {
  strcpy(new_fcb->filename, filename);
  new_fcb->first = first;
  new_fcb->length = length;
  new_fcb->attribute = attribute;
}

void useropen_init(useropen *openfile, int dirno, int diroff, const char* dir) {
  openfile->dirno = dirno;
  openfile->diroff = diroff;
  strcpy(openfile->dir, dir);
  openfile->fcbstate = 0;
  openfile->topenfile = 1;
  openfile->count = openfile->open_fcb.length;
}

//int nextSubdir(char *dirname) {
//  static char store_dirname[dirlen];
//  static int flag = 0, beg;
//  if (!flag) {
//    flag = 1;
//    beg = 2;
//    strcpy(store_dirname, dirname);
//  }
//  if (!strcmp(store_dirname+beg, "")) {
//    flag = 0;
//    return 0;
//  }
//  int len = 0;
//  for (; store_dirname[beg] && store_dirname[beg] != '/'; ++beg) {
//    dirname[len++] = store_dirname[beg];
//  }
//  dirname[len] = '\0';
//  if (store_dirname[beg] == '/') ++beg;
//  return 1;
//}

void fatFree(int id) {
  if (fat1[id].id != END) fatFree(fat1[id].id);
  fat1[id].id = FREE;
}

int getFreeFatid() {
  for (int i = 5; i < BLOCKNUM; ++i) if (fat1[i].id == FREE) return i;
  return END;
}

int getFreeOpenlist() {
  for (int i = 0; i < MAXOPENFILE; ++i) if (!openfilelist[i].topenfile) return i;
  return -1;
}

int getNextFat(int id) {
  if (fat1[id].id == END) fat1[id].id = getFreeFatid();
  return fat1[id].id;
}

int read_ls(int fd, unsigned char *text, int len) {
  int tcount = openfilelist[fd].count;
  openfilelist[fd].count = 0;
  int ret = do_read(fd, text, len);
  openfilelist[fd].count = tcount;
  return ret;
}

void startsys() {
  // 各种变量初始化
  myvhard = (unsigned char*)malloc(SIZE);
  for (int i = 0; i < BLOCKNUM; ++i) blockaddr[i] = i * BLOCKSIZE + myvhard;
  for (int i = 0; i < MAXOPENFILE; ++i) openfilelist[i].topenfile = 0;

  // 准备读入 myfsys 文件信息
  FILE *fp = fopen("myfsys", "rb");
  char need_format = 0;

  // 判断是否需要格式化
  if (fp != NULL) {
    fread(buf, 1, SIZE, fp);
    memcpy(myvhard, buf, SIZE);
    memcpy(&initblock, blockaddr[0], BLOCKSIZE);
    if (strcmp(initblock.information, "10101010") != 0) need_format = 1;
    fclose(fp);
  }
  else {
    need_format = 1;
  }

  // 不需要格式化的话接着读入fat信息
  if (!need_format) {
    memcpy(fat1, blockaddr[1], BLOCKSIZE << 1);
    memcpy(fat2, blockaddr[3], BLOCKSIZE << 1);
  }
  else {
    printf("myfsys 文件系统不存在，现在开始创建文件系统\n");
    my_format();
  }

  // 把根目录fcb放入打开文件表中，设定当前目录为根目录
  startp = blockaddr[5];
  curdirid = 0;
  memcpy(&openfilelist[curdirid].open_fcb, blockaddr[5], sizeof fcb);
#ifdef DEBUG_INFO
  printf("starsys: %s\n", openfilelist[curdirid].open_fcb.filename);
#endif // DEBUG_INFO
  useropen_init(&openfilelist[curdirid], 5, 0, "~/");
}

int do_read(int fd, unsigned char *text, int len) {
  int ret = 0;
  char *buf = (char*)malloc(1024);
  if (buf == NULL) {
    SAYERROR;
    printf("do_read: malloc error\n");
    return -1;
  }

  int blockorder = openfilelist[fd].count >> 10;
  int blockoffset = openfilelist[fd].count % 1024;
  unsigned short id = openfilelist[fd].open_fcb.first;
  while (blockorder) {
    --blockorder;
    id = fat1[id].id;
  }

  int count = 0;
  while (len) {
    memcpy(buf, blockaddr[id], BLOCKSIZE);
    count = min(len, 1024 - blockoffset);
    memcpy(text + ret, buf + blockoffset, count);
    len -= count;
    ret += count;
    blockoffset = 0;
    id = fat1[id].id;
  }

  free(buf);
  return ret;
}

int fat_write(unsigned short id, unsigned char *text, int blockoffset, int len) {
  int ret = 0;
  char *buf = (char*)malloc(1024);
  if (buf == NULL) {
    SAYERROR;
    printf("fat_write: malloc error\n");
    return -1;
  }

  // 写之前先把磁盘长度扩充到所需大小
  int tlen = len;
  int toffset = blockoffset;
  unsigned short tid = id;
  while (tlen) {
    if (tlen <= 1024 - toffset) break;
    tlen -= (1024 - toffset);
    toffset = 0;
    id = getNextFat(id);
    if (id == END) {
      SAYERROR;
      printf("fat_write: no next fat\n");
      return -1;
    }
  }

  int count = 0;
  while (len) {
    memcpy(buf, blockaddr[id], BLOCKSIZE);
    count = min(len, 1024 - blockoffset);
    memcpy(buf + blockoffset, text + ret, count);
    memcpy(blockaddr[id], buf, BLOCKSIZE);
    len -= count;
    ret += count;
    blockoffset = 0;
    id = fat1[id].id;
  }

  free(buf);
  return ret;
}

int do_write(int fd, unsigned char *text, int len) {
  //// 覆盖写的话把fcb指向的磁盘长度置0
  fcb *fcbp = &openfilelist[fd].open_fcb;
  //if (wstyle == 'w') {
  //  fcbp->length = 0;
  //  fatFree(fat1[fcbp->first].id);
  //  openfilelist[fd].count = 0;
  //}

  int blockorder = openfilelist[fd].count >> 10;
  int blockoffset = openfilelist[fd].count % 1024;
  unsigned short id = openfilelist[fd].open_fcb.first;
  while (blockorder) {
    --blockorder;
    id = fat1[id].id;
  }

  int ret = fat_write(id, text, blockoffset, len);

  fcbp->length += ret;
  openfilelist[fd].fcbstate = 1;

  return ret;
}

void my_exitsys() {
  // 先关闭所有打开文件项
  for (int i = 0; i < MAXOPENFILE; ++i) my_close(i);
  
  memcpy(blockaddr[0], &initblock, sizeof initblock);
  memcpy(blockaddr[1], fat1, sizeof fat1);
  memcpy(blockaddr[3], fat2, sizeof fat2);
  FILE *fp = fopen("myfsys", "wb");

#ifdef DEBUG_SAVEFILE
  fwrite(myvhard, BLOCKSIZE, BLOCKNUM, fp);
#endif // DEBUG_SAVEFILE

  fwrite(myvhard, BLOCKSIZE, BLOCKNUM, fp);

  free(myvhard);
  fclose(fp);
}

void my_format() {
  strcpy(initblock.information, "10101010");
  initblock.root = 5;
  initblock.startblock = blockaddr[5];

  for (int i = 0; i < 5; ++i) fat1[i].id = END;
  for (int i = 5; i < BLOCKNUM; ++i) fat1[i].id = FREE;
  for (int i = 0; i < BLOCKNUM; ++i) fat2[i].id = fat1[i].id;

  fat1[5].id = END;
  fcb root;
  fcb_init(&root, ".", 5, 2 * sizeof fcb, 0);
#ifdef DEBUG_INFO
  printf("my_format %s\n", root.filename);
#endif // DEBUG_INFO
  memcpy(blockaddr[5], &root, sizeof fcb);
  strcpy(root.filename, "..");
#ifdef DEBUG_INFO
  printf("my_format %s\n", root.filename);
#endif // DEBUG_INFO
  memcpy(blockaddr[5] + sizeof fcb, &root, sizeof fcb);

  printf("初始化完成\n");
}

void my_close(int fd) {
  if (!(0 <= fd && fd < MAXOPENFILE)) {
    SAYERROR;
    printf("my_close: fd invaild\n");
    return;
  }
  if (openfilelist[fd].topenfile == 0) return;

  // 若内容有改变，把fcb内容写回父亲的磁盘块中
  useropen *file = &openfilelist[fd];
  if (file->fcbstate) fat_write(file->dirno, (unsigned char *)&file->open_fcb, file->diroff, sizeof fcb);

  openfilelist[fd].topenfile = 0;
  return;
}

int spiltDir(char dirs[DIRLEN][DIRLEN], char *filename) {
  int bg = 0; int ed = strlen(filename);
  if (filename[0] == '/') ++bg;
  if (filename[ed - 1] == '/') --ed;

  int ret = 0, tlen = 0;
  for (int i = bg; i < ed; ++i) {
    if (filename[i] == '/') {
      dirs[ret][tlen] = '\0';
      tlen = 0;
      ++ret;
    }
    else {
      dirs[ret][tlen++] = filename[i];
    }
  }
  dirs[ret][tlen] = '\0';

  return ret+1;
}

void popLastDir(char *dir) {
  int len = strlen(dir) - 1;
  while (dir[len - 1] != '/') --len;
  dir[len] = '\0';
}

void splitLastDir(char *dir, char new_dir[2][DIRLEN]) {
  int len = strlen(dir);
  int flag = -1;
  for (int i = 0; i < len; ++i) if (dir[i] == '/') flag = i;

  if (flag == -1) {
    SAYERROR;
    printf("splitLastDir: can\'t split %s\n", dir);
    return;
  }

  int tlen = 0;
  for (int i = 0; i < flag; ++i) {
    new_dir[0][tlen++] = dir[i];
  }
  new_dir[0][tlen] = '\0';
  tlen = 0;
  for (int i = flag + 1; i < len; ++i) {
    new_dir[1][tlen++] = dir[i];
  }
  new_dir[1][tlen] = '\0';
}

int moveNextDir(int fd, char *dirname) {
  useropen *file = &openfilelist[fd];
  
  // 从磁盘中读出当前目录的信息
  int read_size = read_ls(fd, buf, file->open_fcb.length);
  if (read_size == -1) {
    SAYERROR;
    printf("my_ls: read_ls error\n");
    return 0;
  }
  fcb dirfcb;
  int flag = -1;
  for (int i = 0; i < read_size; i += sizeof fcb) {
    memcpy(&dirfcb, buf + i, sizeof fcb);
    if (!strcmp(dirfcb.filename, dirname)) {
      flag = i;
      break;
    }
  }

  // 没有找到需要的文件
  if (flag == -1) return 0;

  // 找到的话就开始计算相关信息，改变对应打开文件项的值
  int blockorder = flag >> 10;
  int blockoffset = flag % 1024;
  unsigned short id = file->open_fcb.first;
  while (blockorder) {
    --blockorder;
    id = fat1[id].id;
  }
  file->dirno = id;
  file->diroff = blockoffset;
  file->fcbstate = 0;
  file->topenfile = 1;
  memcpy(&file->open_fcb, &dirfcb, sizeof fcb);
  file->count = dirfcb.length;

  // 如果是到当前目录，目录名不变
  if (!strcmp(dirname, ".")) {
    ;
  }
  // 如果是到上一级目录，当前目录是根目录的话就不变，否则弹一个路径
  else if (!strcmp(dirname, "..")) {
    if (strcmp(file->dir, "~/")) {
      popLastDir(file->dir);
    }
  }
  // 其它情况一律在当前路径后面加上文件或路径名
  else {
    strcat(file->dir, dirfcb.filename);
    // 如果打开的是一个路径，就在路径名后面加上"/"
    if (!file->open_fcb.attribute) strcat(file->dir, "/");
  }

  return 1;
}

int my_open(char *filename) {
  char dirs[DIRLEN][DIRLEN];
  int count = spiltDir(dirs, filename);

  // 生成当前目录的副本
  int fileid = getFreeOpenlist();
  if (fileid == -1) {
    SAYERROR;
    printf("my_open: openlist is full\n");
    return -1;
  }
  memcpy(&openfilelist[fileid], &openfilelist[curdirid], sizeof useropen);

  // 利用当前目录的副本不断找到下一个目录
  int flag = 0;
  for (int i = 0; i < count; ++i) {
    if (!moveNextDir(fileid, dirs[i])) {
      flag = 1;
      break;
    }
  }
  if (flag) {
    SAYERROR;
    printf("my_open: %s no such file or directory\n", filename);
    openfilelist[fileid].topenfile = 0;
    return -1;
  }

  return fileid;
}

void my_cd(char *dirname) {
  int fd = my_open(dirname);
  if (!(0 <= fd && fd < MAXOPENFILE)) return;
  if (openfilelist[fd].open_fcb.attribute) {
    my_close(fd);
    printf("%s is a file, please use open command\n", openfilelist[fd].dir);
    return;
  }

  // 得到的fd是文件夹的话，就把原来的目录关了,把现在的目录设为当前目录
  my_close(curdirid);
  curdirid = fd;
}

void my_ls() {
  // 从磁盘中读出当前目录的信息
  int read_size = read_ls(curdirid, buf, openfilelist[curdirid].open_fcb.length);
  if (read_size == -1) {
    SAYERROR;
    printf("my_ls: read_ls error\n");
    return;
  }
  fcb dirfcb;
  for (int i = 0; i < read_size; i += sizeof fcb) {
    memcpy(&dirfcb, buf + i, sizeof fcb);
    if (dirfcb.attribute) printf(" %s", dirfcb.filename);
    else printf(" " GRN "%s", dirfcb.filename);
  }
  printf("\n");
}

int my_touch(char *filename) {
  // 先打开file的上级目录，如果上级目录不存在就报错（至少自己电脑上的Ubuntu是这个逻辑）
  // 为了保证filename字符串里有'/'，在其前面加上'.'
  // 如果filename后面有‘/’，去掉
  int len = strlen(filename);
  if (filename[len - 1] == '/') filename[len - 1] = '\0';
  char newdir[DIRLEN] = "./";
  char split_dir[2][DIRLEN];
  strcat(newdir, filename);
  splitLastDir(newdir, split_dir);

  int pafd = my_open(split_dir[0]);
  if (!(0 <= pafd && pafd < MAXOPENFILE)) {
    SAYERROR;
    printf("my_creat: my_open error\n");
    return -1;
  }

  // 从磁盘中读出当前目录的信息，进行检查
  int read_size = read_ls(pafd, buf, openfilelist[pafd].open_fcb.length);
  if (read_size == -1) {
    SAYERROR;
    printf("my_touch: read_ls error\n");
    return -1;
  }
  fcb dirfcb;
  for (int i = 0; i < read_size; i += sizeof fcb) {
    memcpy(&dirfcb, buf + i, sizeof fcb);
    if (dirfcb.attribute) continue;
    if (!strcmp(dirfcb.filename, split_dir[1])) {
      printf("%s is already exit\n", split_dir[1]);
      return -1;
    }
  }

  // 利用空闲磁盘块创建文件
  int fatid = getFreeFatid();
  if (fatid == -1) {
    SAYERROR;
    printf("my_touch: no free fat\n");
    return;
  }
  fat1[fatid].id = END;

  // 写入父亲目录内存
  fcb_init(&dirfcb, split_dir[1], fatid, 2 * sizeof fcb, 0);
  memcpy(buf, &dirfcb, sizeof dirfcb);
  int write_size = do_write(pafd, buf, sizeof fcb);
  if (write_size == -1) {
    SAYERROR;
    printf("my_touch: do_write error\n");
    return;
  }
  openfilelist[pafd].count += write_size;


}

int my_creat(char *filename) {
  

  int creatfile()
}

void my_mkdir(char *dirname) {

  

  

  // 把"."和".."装入自己的磁盘
  strcpy(dirfcb.filename, ".");
  memcpy(blockaddr[fatid], &dirfcb, sizeof fcb);
  memcpy(&dirfcb, &openfilelist[curdirid].open_fcb, sizeof fcb);
  strcpy(dirfcb.filename, "..");
  memcpy(blockaddr[fatid] + sizeof fcb, &dirfcb, sizeof fcb);
}