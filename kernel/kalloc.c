// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct spinlock pageref_lock;
uint64 pageref[PHYSTOP/PGSIZE];
void clearpageref(int id){
  acquire(&pageref_lock);
  pageref[id] = 0;
  release(&pageref_lock);
}

void addpageref(int id){
  if(id < 0 || id >= PHYSTOP/PGSIZE) return;
  acquire(&pageref_lock);
  pageref[id]++;
  release(&pageref_lock);
}

int getpageref(int id){
  if(id < 0 || id >= PHYSTOP/PGSIZE) return -1;
  acquire(&pageref_lock);
  int ret = pageref[id];
  release(&pageref_lock);
  return ret;
}

int cowpage(pagetable_t pagetable, uint64 va) {
  if(va >= MAXVA)
    return -1;
  pte_t* pte = walk(pagetable, va, 0);
  if(pte == 0)
    return -1;
  if((*pte & PTE_V) == 0)
    return -1;
  return (*pte & PTE_C ? 0 : -1);
}

void* cowalloc(pagetable_t pagetable, uint64 va) {
  if(va % PGSIZE != 0)
    return 0;

  uint64 pa = walkaddr(pagetable, va);  // 获取对应的物理地址
  if(pa == 0)
    return 0;

  pte_t* pte = walk(pagetable, va, 0);  // 获取对应的PTE

  if(getpageref(pa/PGSIZE) == 1) {
    // 只剩一个进程对此物理地址存在引用
    // 则直接修改对应的PTE即可
    *pte |= PTE_W;
    *pte &= ~PTE_C;
    return (void*)pa;
  } else {
    // 多个进程对物理内存存在引用
    // 需要分配新的页面，并拷贝旧页面的内容
    char* mem = kalloc();
    if(mem == 0)
      return 0;

    // 复制旧页面内容到新页
    memmove(mem, (char*)pa, PGSIZE);

    // 清除PTE_V，否则在mappagges中会判定为remap
    *pte &= ~PTE_V;

    // 为新页面添加映射
    if(mappages(pagetable, va, PGSIZE, (uint64)mem, (PTE_FLAGS(*pte) | PTE_W) & ~PTE_C) != 0) {
      kfree(mem);
      *pte |= PTE_V;
      return 0;
    }

    // 将原来的物理内存引用计数减1
    kfree((char*)PGROUNDDOWN(pa));
    return mem;
  }
}

int pagecheck(pagetable_t p,int va){
  if(va >= MAXVA) return 0;
  pte_t *pte = walk(p, va, 0);
  if(pte == 0) return 0;
  if((*pte & PTE_V) == 0) return 0;  
  return 1;
}
int cowcheck(pagetable_t p,int va){
  return ((*walk(p, va, 0) & PTE_C) != 0);
}
int kaddrefcnt(void* pa) {
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    return -1;
  addpageref((uint64)pa/PGSIZE);
  return 0;
}
pte_t* resolvecowpage(pagetable_t pagetable, uint64 va){
  // pte_t *pte = walk(p, va, 0);
  // uint flags = PTE_FLAGS(*pte);
  // flags &= ~PTE_C;
  // flags |= PTE_W;
  // // flags &= ~PTE_V;
  // uint64 pa = PTE2PA(*pte);
  // char *mem = kalloc();
  // if(mem == 0) return 0;
  
  // memmove(mem, (char*)pa, PGSIZE);
  // kfree((void*)PGROUNDDOWN(pa));

  // if(mappages(p, va, PGSIZE, (uint64)mem, flags) != 0) {
  //   kfree(mem);
  //   return 0;
  // }
  // return walk(p, va, 0);    
   if(va % PGSIZE != 0)
    return 0;

  uint64 pa = walkaddr(pagetable, va);  // 获取对应的物理地址
  if(pa == 0)
    return 0;

    

  pte_t* pte = walk(pagetable, va, 0);  // 获取对应的PTE

   if(getpageref((uint64)pa/PGSIZE) == 1) {
    // 只剩一个进程对此物理地址存在引用
    // 则直接修改对应的PTE即可
    *pte |= PTE_W;
    *pte &= ~PTE_C;
    return pte;
  } else{
    // 多个进程对物理内存存在引用
    // 需要分配新的页面，并拷贝旧页面的内容
    char* mem = kalloc();
    if(mem == 0)
      return 0;

    // 复制旧页面内容到新页
    memmove(mem, (char*)pa, PGSIZE);

    // 清除PTE_V，否则在mappagges中会判定为remap
    *pte &= ~PTE_V;

    // 为新页面添加映射
    if(mappages(pagetable, va, PGSIZE, (uint64)mem, (PTE_FLAGS(*pte) | PTE_W) & ~PTE_C) != 0) {
      kfree(mem);
      *pte |= PTE_V;
      return 0;
    }

    // 将原来的物理内存引用计数减1
    kfree((char*)PGROUNDDOWN(pa));
    return pte;
  }
}
void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&pageref_lock, "pageref");
  for(int i=0;i<PHYSTOP/PGSIZE;i++) pageref[i] = 1;
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
  acquire(&pageref_lock);
  pageref[(uint64)pa/PGSIZE]--;
  if(pageref[(uint64)pa/PGSIZE] <0) panic("kfree:ref<0!");
  if(pageref[(uint64)pa/PGSIZE] == 0){
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);

    r = (struct run*)pa;

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  }
  release(&pageref_lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r){
    memset((char*)r, 5, PGSIZE); // fill with junk
    clearpageref((uint64)r/PGSIZE);
    addpageref((uint64)r/PGSIZE);
  }
  return (void*)r;
}
