//#define DEBUG_INFO
//#define DEBUG_DONT_SAVEFILE

#define FREE 0
#define DIRLEN 80
#define END 65535
#define SIZE 1024000
#define BLOCKNUM 1000
#define BLOCKSIZE 1024
#define MAXOPENFILE 10
#define ROOTBLOCKNUM 2

#define SAYERROR printf("ERROR: ")
#define max(X, Y) (((X) > (Y)) ? (X) : (Y))
#define min(X, Y) (((X) < (Y)) ? (X) : (Y))

// #define GRN   "\x1B[32m"
#define GRN   "dir:"
#define RESET "\x1B[0m"

typedef struct FCB {
  char free; // 此fcb是否已被删除，因为把一个fcb从磁盘块上删除是很费事的，所以选择利用fcb的free标号来标记其是否被删除
  char exname[3];
  char filename[DIRLEN];
  unsigned short time;
  unsigned short data;
  unsigned short first; // 文件起始盘块号
  unsigned long length; // 文件的实际长度
  unsigned char attribute; // 文件属性字段：为简单起见，我们只为文件设置了两种属性：值为 0 时表示目录文件，值为 1 时表示数据文件
} fcb;

typedef struct FAT {
  unsigned short id;
} fat;

// 对于文件夹fcb，其count永远等于其fcb的length，
// 只有文件fcb的count会根据打开方式的不同和读写方式的不同而不同
typedef struct USEROPEN {
  fcb open_fcb; // 文件的 FCB 中的内容
  int count; // 读写指针在文件中的位置
  int dirno; // 相应打开文件的目录项在父目录文件中的盘块号
  int diroff; // 相应打开文件的目录项在父目录文件的dirno盘块中的起始位置
  char fcbstate; // 是否修改了文件的 FCB 的内容，如果修改了置为 1，否则为 0
  char topenfile; // 表示该用户打开表项是否为空，若值为 0，表示为空，否则表示已被某打开文件占据
  char dir[DIRLEN]; // 打开文件的绝对路径名，这样方便快速检查出指定文件是否已经打开
} useropen;

typedef struct BLOCK0 {
  unsigned short root;
  char information[200];
  unsigned char *startblock;
} block0;

int my_write(int fd);
int my_open(char *filename);
int my_create(char *filename);
int my_read(int fd);
int do_read(int fd, unsigned char *text, int len);
// 直接针对fat内存块进行写入，属于do_read的子函数
int fat_write(unsigned short id, unsigned char * text, int blockoffset, int len);
// 对fd指向的打开文件的文件指针处填上text中len长度的字节，只会改变打开文件的fcb信息，而不会改变打开文件的文件指针信息
int do_write(int fd, unsigned char *text, int len);
void my_reload(int fd);
// 创建一个文件，然后再由my_mkdir和my_creat继续处理
int my_touch(char * filename, int attribute, int *rpafd);

void my_ls();
void startsys();
void my_format();
void my_exitsys();
void my_save(int fd);
void my_close(int fd);
void my_cd(char *dirname);
void my_rm(char *filename);
void my_mkdir(char *dirname);
void my_rmdir(char *dirname);

// 初始化打开文件项，需要先把fcb拷贝进openfile的fcb中，其文件指针默认为其所占磁盘大小
void useropen_init(useropen * openfile, int dirno, int diroff, const char * dir);
