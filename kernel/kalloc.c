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

void addpageref(void *pa){
    if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP) return;
    acquire(&pageref_lock);
    pageref[(uint64)pa/PGSIZE]++;
    release(&pageref_lock);
}


int pagecheck(pagetable_t p,uint64 va){
  if(va >= MAXVA) return 0;
  pte_t *pte = walk(p, va, 0);
  if(pte == 0) return 0;
  if((*pte & PTE_V) == 0) return 0;  
  return 1;
}
int cowcheck(pagetable_t p,uint64 va){
  if(va >= MAXVA) return 0;
  return ((*walk(p, va, 0) & PTE_C)  ? 1 : 0);
}

pte_t* resolvecowpage(pagetable_t p, uint64 va){
  pte_t *pte = walk(p, va, 0);
  uint flags = PTE_FLAGS(*pte);
  flags &= ~PTE_C;
  flags |= PTE_W;
  // flags &= ~PTE_V;
  uint64 pa = PTE2PA(*pte);
  char *mem = kalloc();
  if(mem == 0) return 0;
  
  memmove(mem, (char*)pa, PGSIZE);
  kfree((void*)PGROUNDDOWN(pa));

  if(mappages(p, va, PGSIZE, (uint64)mem, flags) != 0) {
    kfree(mem);
    return 0;
  }
  return walk(p, va, 0);    
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
    addpageref(r);
  }
  return (void*)r;
}
