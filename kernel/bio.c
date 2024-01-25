// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

struct hashbucket {
  struct spinlock lock;
  struct buf buckethead;
};
struct {
  // struct spinlock lock;
  struct buf buf[NBUF];
  struct hashbucket bufbucket[NBUCKET];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head;
} bcache;

int inline getbucketid(uint blockno) {
  return blockno % NBUCKET;
}

void
binit(void)
{
  struct buf *b;
  char tmpname[10];
  for(int i = 0; i < NBUCKET; i++) {
    snprintf(tmpname, 10, "bcache_%d", i);
    initlock(&bcache.bufbucket[i].lock, tmpname);
    bcache.bufbucket[i].buckethead.next = &bcache.bufbucket[i].buckethead;
    bcache.bufbucket[i].buckethead.prev = &bcache.bufbucket[i].buckethead;
  }
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next=bcache.bufbucket[0].buckethead.next;
    b->prev=&bcache.bufbucket[0].buckethead;
    initsleeplock(&b->lock, "buffer");
    bcache.bufbucket[0].buckethead.next->prev=b;
    bcache.bufbucket[0].buckethead.next=b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int bucketid = getbucketid(blockno);
  acquire(&bcache.bufbucket[bucketid].lock);
  for(b = bcache.bufbucket[bucketid].buckethead.next; b != &bcache.bufbucket[bucketid].buckethead; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      acquire(&tickslock);
      b->lru = ticks;
      release(&tickslock);
      release(&bcache.bufbucket[bucketid].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // not cache
  b=0;
  struct buf *replace=0;
  int num=0;
  for(int now=bucketid; num<NBUCKET; now=(now+1)%NBUCKET,num++) {
    if(now != bucketid){
      if(!holding(&bcache.bufbucket[now].lock)) acquire(&bcache.bufbucket[now].lock);
      else continue;
    }
   
    for(b = bcache.bufbucket[now].buckethead.next; b != &bcache.bufbucket[now].buckethead; b = b->next){
      if(b->refcnt == 0 && (replace == 0 || b->lru < replace->lru)){
        replace = b;
      }
    }
    if(replace){
      if(now != bucketid){
        replace->next->prev = replace->prev;
        replace->prev->next = replace->next;
        replace->next = bcache.bufbucket[bucketid].buckethead.next;
        replace->prev = &bcache.bufbucket[bucketid].buckethead;
        bcache.bufbucket[bucketid].buckethead.next->prev = replace;
        bcache.bufbucket[bucketid].buckethead.next = replace;
        release(&bcache.bufbucket[now].lock);        
      }
      replace->dev = dev;
      replace->blockno = blockno;
      replace->valid = 0;
      replace->refcnt = 1;

      acquire(&tickslock);
      replace->lru = ticks;
      release(&tickslock);

      release(&bcache.bufbucket[bucketid].lock);
      acquiresleep(&replace->lock);
      return replace;
    } else{
      if(now != bucketid){
        release(&bcache.bufbucket[now].lock);
      }
    }
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  int bucketid = getbucketid(b->blockno);
  releasesleep(&b->lock);

  acquire(&bcache.bufbucket[bucketid].lock);
  b->refcnt--;
  acquire(&tickslock);
  b->lru = ticks;
  release(&tickslock);

  release(&bcache.bufbucket[bucketid].lock);
}

void
bpin(struct buf *b) {
  int bucketid = getbucketid(b->blockno);
  acquire(&bcache.bufbucket[bucketid].lock);
  b->refcnt++;
  release(&bcache.bufbucket[bucketid].lock);
}

void
bunpin(struct buf *b) {
  int bucketid = getbucketid(b->blockno);
  acquire(&bcache.bufbucket[bucketid].lock);
  b->refcnt--;
  release(&bcache.bufbucket[bucketid].lock);
}


