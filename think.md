1. Large files
   
   doubly-indirect  熟悉的感觉...

   按照提示一步一步来就可以了

```c
// fs.h
// 修改 NDIRECT  为 11
#define NDIRECT 11
// doubly-indirect 的下标
#define DINDIRECTIDX (NDIRECT + 1)
#define NINDIRECT (BSIZE / sizeof(uint))
// doubly-indirect  所能允许的页数
#define DINDIRECT (NINDIRECT * NINDIRECT)
// 最大文件也要记得更新
#define MAXFILE (NDIRECT + NINDIRECT + DINDIRECT)

// On-disk inode structure
struct dinode {
  short type;           // File type
  short major;          // Major device number (T_DEVICE only)
  short minor;          // Minor device number (T_DEVICE only)
  short nlink;          // Number of links to inode in file system
  uint size;            // Size of file (bytes)
  // 总体还是13  
  // 原来 12 直接 + 1 间接
  // 变为 11 直接 + 1 间接 + 1 双重间接
  uint addrs[NDIRECT+2];   // Data block addresses
};


// file.h
struct inode {
  uint dev;           // Device number
  uint inum;          // Inode number
  int ref;            // Reference count
  struct sleeplock lock; // protects everything below here
  int valid;          // inode has been read from disk?

  short type;         // copy of disk inode
  short major;
  short minor;
  short nlink;
  uint size;
  // 和dinode 一样 hint中说了
  uint addrs[NDIRECT+2];
};
```


```c
// brelse 和 bread 一个是释放对应空间的锁 一个返回带有锁的对应空间
// 一般是对应的 hint中也说了
// fs.c
static uint
bmap(struct inode *ip, uint bn)
{
  uint addr, *a;
  struct buf *bp;

  if(bn < NDIRECT){
    if((addr = ip->addrs[bn]) == 0){
      addr = balloc(ip->dev);
      if(addr == 0)
        return 0;
      ip->addrs[bn] = addr;
    }
    return addr;
  }
  bn -= NDIRECT;

  if(bn < NINDIRECT){
    // Load indirect block, allocating if necessary.
    if((addr = ip->addrs[NDIRECT]) == 0){
      addr = balloc(ip->dev);
      if(addr == 0)
        return 0;
      ip->addrs[NDIRECT] = addr;
    }
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;
    if((addr = a[bn]) == 0){
      addr = balloc(ip->dev);
      if(addr){
        a[bn] = addr;
        log_write(bp);
      }
    }
    brelse(bp);
    return addr;
  }
  // ... 前面的不改
  // 仿照前面的间接  不过又多了一层
  bn -= NINDIRECT;
  if(bn < DINDIRECT){
    if((addr = ip->addrs[DINDIRECTIDX]) == 0){
      addr = balloc(ip->dev);
      if(addr == 0)
        return 0;
      ip->addrs[DINDIRECTIDX] = addr;
    }
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;
    
    // 多的一层
    if((addr = a[bn/NINDIRECT]) == 0) {
      addr = balloc(ip->dev);
      if(addr == 0)
        return 0;
      a[bn/NINDIRECT] = addr;
      log_write(bp);
    }
    brelse(bp);

    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;
    if((addr = a[bn%NINDIRECT]) == 0){
      addr = balloc(ip->dev);
      if(addr == 0){
        return 0;
      }
      a[bn%NINDIRECT] = addr;
      log_write(bp);
    }
    brelse(bp);
    return addr;
  }
  panic("bmap: out of range");
}

// Truncate inode (discard contents).
// Caller must hold ip->lock.
void
itrunc(struct inode *ip)
{
  int i, j;
  struct buf *bp;
  uint *a;

  for(i = 0; i < NDIRECT; i++){
    if(ip->addrs[i]){
      bfree(ip->dev, ip->addrs[i]);
      ip->addrs[i] = 0;
    }
  }

  if(ip->addrs[NDIRECT]){
    bp = bread(ip->dev, ip->addrs[NDIRECT]);
    a = (uint*)bp->data;
    for(j = 0; j < NINDIRECT; j++){
      if(a[j])
        bfree(ip->dev, a[j]);
    }
    brelse(bp);
    bfree(ip->dev, ip->addrs[NDIRECT]);
    ip->addrs[NDIRECT] = 0;
  }

  // ... 前面的不改

  if(ip->addrs[DINDIRECTIDX]){
    struct buf *bp2;
    uint *a2;

    // 和前面比 多了一层遍历
    // 释放下一层的空间
    bp = bread(ip->dev, ip->addrs[DINDIRECTIDX]);
    a = (uint*)bp->data;
    for(j = 0; j < NINDIRECT; j++){
      if(a[j]){
        bp2 = bread(ip->dev, a[j]);
        a2 = (uint*)bp2->data;
        for(int k = 0; k < NINDIRECT; k++){
          if(a2[k]){
            bfree(ip->dev, a2[k]);
          }
        }
        brelse(bp2);
        bfree(ip->dev, a[j]);
        a[j] = 0;
      }
    }
    brelse(bp);
    bfree(ip->dev, ip->addrs[DINDIRECTIDX]);
    ip->addrs[DINDIRECTIDX] = 0;
  }

  ip->size = 0;
  iupdate(ip);
}
```

2. Symbolic links

(
  总感觉这个比第一个难  这个还参考了其他人的实现  
  不过也和我懒得看第8章有关系 上来直接做lab...  
  最近变懒了 但是还有事  emmm 还是抓紧查重改论文吧...
)

按照提示 把定义啥的都定义好

```c
// Makefile
UPROGS=\
	$U/_symlinktest\

// fcntl.h
#define O_NOFOLLOW   0x004

// stat.h
#define T_SYMLINK  4   // 只要不和前面重复就行

// syscall.c
extern uint64 sys_symlink(void);

static uint64 (*syscalls[])(void) = {
    // ...
  [SYS_symlink]   sys_symlink,
};

// syscall.h
#define SYS_symlink  22

// user.h
int symlink(char *, char *);

// usys.pl

entry("symlink");
```

根据提示主要是实现和修改两个函数 sys_symlink 和 sys_open
sys_symlink 是为了创建Symbolic links
sys_open 是为了打开Symbolic links 所对应的文件

```c
// sysfile.c

#define MAXLINKNUM  10


struct inode* find_symlink(struct inode* ip) {
  // seen 确认是否存在环
  uint seen[MAXLINKNUM];
  char target[MAXPATH];

  for(int i = 0; i < MAXLINKNUM; i++){
    seen[i] = ip->inum;
    if(readi(ip, 0, (uint64)target, 0, MAXPATH) <= 0){
      iunlockput(ip);
      return 0;
    }
    iunlockput(ip);
    
    if((ip = namei(target)) == 0){
        return 0;
    }

    for(int j = 0; j <= i; j++){
      if(ip->inum == seen[j]){
        return 0;
      }
    }

    // readi 必须持有对应的 lock
    // 在进入该函数之前 即 sys_open 中
    // 已经持有对应ip的lock
    ilock(ip);
    if(ip->type != T_SYMLINK){
        return ip;
    }
  }

  iunlockput(ip);
  return 0;
}


uint64
sys_open(void)
{
  // ....

  // hint 5： When a process specifies O_NOFOLLOW in the flags to open, open should open the symlink (and not follow the symbolic link).
  // 我的理解是打开当前文件本身 不是打开所指向的那个文件
  if(ip->type == T_SYMLINK && (omode & O_NOFOLLOW) == 0){
    if((ip = find_symlink(ip)) == 0){
      end_op();
      return -1;
    }
  }

  // ....
}

int
sys_symlink(void)
{
  char target[MAXPATH], path[MAXPATH];
  struct inode *ip;
  uint n;
  
  begin_op();
  if((n = argstr(0, target, MAXPATH)) < 0 || argstr(1, path, MAXPATH) < 0){
    end_op();
    return -1;
  }
  // 在获取到参数之后
  // 创建 inode 
  ip = create(path, T_SYMLINK, 0, 0);
  if(ip == 0){
    end_op();
    return -1;
  }
  
  // 通过目标 target(原本的文件) 向新创建的node写入数据
  if(writei(ip, 0, (uint64)target, 0, n) != n){
    iunlockput(ip);
    end_op();
    return -1;
  }

  iunlockput(ip);
  end_op();
  return 0;
}
```
