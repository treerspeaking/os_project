//#ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Memory management unit mm/mm.c
 */

// 20:20 _ 10/05/2023

#include "mm.h"
#include <stdlib.h>
#include <stdio.h>

/* 
 * init_pte - Initialize PTE entry
 */
int init_pte(uint32_t *pte,
             int pre,    // present
             int fpn,    // FPN
             int drt,    // dirty
             int swp,    // swap
             int swptyp, // swap type
             int swpoff) //swap offset
{
  if (pre != 0) {
    if (swp == 0) { // Non swap ~ page online
      if (fpn == 0) 
        return -1; // Invalid setting

      /* Valid setting with FPN */
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

      SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT); 
    } else { // page swapped
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

      SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT); 
      SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);
    }
  }

  return 0;   
}

/* 
 * pte_set_swap - Set PTE entry for swapped page
 * @pte    : target page table entry (PTE)
 * @swptyp : swap type
 * @swpoff : swap offset
 */
int pte_set_swap(uint32_t *pte, int swptyp, int swpoff)
{
  // SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
  CLRBIT(*pte, PAGING_PTE_PRESENT_MASK);
  SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);

  SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
  SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);

  return 0;
}

/* 
 * pte_set_swap - Set PTE entry for on-line page
 * @pte   : target page table entry (PTE)
 * @fpn   : frame page number (FPN)
 */
int pte_set_fpn(uint32_t *pte, int fpn)
{
  SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
  CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);

  SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT); 

  return 0;
}


/* 
 * vmap_page_range - map a range of page at aligned addresslist
 */
int vmap_page_range(struct pcb_t *caller, // process call
                                int addr, // start address which is aligned to pagesz
                               int pgnum, // num of mapping page
           struct framephy_struct *frames,// list of the mapped frames
              struct vm_rg_struct *ret_rg)// return mapped region, the real mapped fp
{                                         // no guarantee all given pages are mapped
  uint32_t * pte = malloc(sizeof(uint32_t));
  // int fpn;
  int pgit = 0;
  struct framephy_struct *fpit = malloc(sizeof(struct framephy_struct));
  // struct framephy_Struct *tmp = fpit;

  // for debugging
  // printf("\nvmap_page_range()\n");

  ret_rg->rg_start = ret_rg->rg_end = addr; // at least the very first space is usable

  fpit->fp_next = frames;
  fpit = frames;

  /* TODO map range of frame to address space 
   *      [addr, addr + pgnum*PAGING_PAGESZ]
   *      in page table caller->mm->pgd[]
   */
  int pgn = PAGING_PGN(addr); // the first expanded page at addr

  for (int pn = 0; pn < pgnum; pn++)
  {
    if (!fpit)
    {
      // for debugging
      // printf("vmap_page_range()    !fpit\n");
      break;
    }

    /* Map the frame in the list */
    pgit++;
    ret_rg->rg_end += PAGING_PAGESZ;

    // pthread_mutex_init(&caller->mm->mtx, NULL); pthread_mutex_lock(&caller->mm->mtx);
    pte_set_fpn(&caller->mm->pgd[pn + pgn], fpit->fpn);
    // pthread_mutex_unlock(&caller->mm->mtx); pthread_mutex_destroy(&caller->mm->mtx);

    enlist_pgn_node(&caller->mm->fifo_pgn, pn + pgn);

    fpit = fpit->fp_next;
  }

  // for debugging
  // printf("vmap_page_range():  pgit = %d    pgnum = %d\n\n", pgit , pgnum);

  // free (tmp);

  return 0;
}

/* 
 * alloc_pages_range - allocate req_pgnum of frame in ram
 * @caller    : caller
 * @req_pgnum : request page num
 * @frm_lst   : frame list
 */

int alloc_pages_range(struct pcb_t *caller, int req_pgnum, struct framephy_struct** frm_lst)
{
  int pgit, fpn;
  int check;
  // struct framephy_struct *newfp_str;

  // for debugging
  // printf("\nalloc_pages_range()\n");

  // Not enough RAM to alloc the required page num
  if ((req_pgnum * PAGING_PAGESZ) > caller->mram->maxsz)
    return -3000; 


  for(pgit = 0; pgit < req_pgnum; pgit++)
  {
    // pthread_mutex_init(&caller->mram->mtx, NULL); pthread_mutex_lock(&caller->mram->mtx);
    // check = MEMPHY_get_freefp(caller->mram, &fpn);
    // pthread_mutex_unlock(&caller->mram->mtx); pthread_mutex_destroy(&caller->mram->mtx);
    // --> Had added mutex into function MEMPHY_get_freefp
    
    if(MEMPHY_get_freefp(caller->mram, &fpn) == 0)
    {
      enlist_framephy_node(frm_lst, fpn);

    } else 
    {  // ERROR CODE of obtaining somes but not enough frames
      int vicpgn, vicfpn;
      uint32_t vicpte;

      if (find_victim_page(caller->mm, &vicpgn) != 0)
      {
        // for debugging
        printf("ERROR: Cannot find victim page in ram  -  alloc_pages_range\n");
        return -1;

      } else
      {
        vicpte = caller->mm->pgd[vicpgn];

        vicfpn = GETVAL(vicpte, PAGING_PTE_FPN_MASK, 0);
        // vicfpn = PAGING_FPN(vicpte); /*--> wrong built macro*/

        int dest_fpn;

        // pthread_mutex_init(&caller->active_mswp->mtx, NULL); pthread_mutex_lock(&caller->active_mswp->mtx);
        // check = MEMPHY_get_freefp(caller->active_mswp, &dest_fpn);
        // pthread_mutex_unlock(&caller->active_mswp->mtx); pthread_mutex_destroy(&caller->active_mswp->mtx);
        //--> Had added mutex into function MEMPHY_get_freefp

        if (MEMPHY_get_freefp(caller->active_mswp, &dest_fpn) != 0)
        {
          printf("ERROR: Cannot get free frame from physical memory  -  alloc_pages_range()\n");
          return -1;
        }
        
        // Swap victim frame to swap
        __swap_cp_page(caller->mram, vicfpn, caller->active_mswp, dest_fpn);

        /* Update page table */
        pte_set_swap(&caller->mm->pgd[vicpgn], 0, dest_fpn);
        
        pthread_mutex_lock(&caller->active_mswp->mtx);
        enlist_framephy_node(&caller->active_mswp->used_fp_list, dest_fpn);
        pthread_mutex_unlock(&caller->active_mswp->mtx);

        enlist_framephy_node(frm_lst, vicfpn);
      }
    } 
  }

  return 0;
}


/* 
 * vm_map_ram - do the mapping all vm are to ram storage device
 * @caller    : caller
 * @astart    : vm area start
 * @aend      : vm area end
 * @mapstart  : start mapping point
 * @incpgnum  : number of mapped page
 * @ret_rg    : returned region
 */
int vm_map_ram(struct pcb_t *caller, int astart, int aend, int mapstart, int incpgnum, struct vm_rg_struct *ret_rg)
{
  struct framephy_struct *frm_lst = NULL;
  int ret_alloc;

  /*@bksysnet: author provides a feasible solution of getting frames
   *FATAL logic in here, wrong behaviour if we have not enough page
   *i.e. we request 1000 frames meanwhile our RAM has size of 3 frames
   *Don't try to perform that case in this simple work, it will result
   *in endless procedure of swap-off to get frame and we have not provide 
   *duplicate control mechanism, keep it simple
   */
  ret_alloc = alloc_pages_range(caller, incpgnum, &frm_lst);

  if (ret_alloc < 0 && ret_alloc != -3000)
    return -1;

  /* Out of memory */
  if (ret_alloc == -3000) 
  {
#ifdef MMDBG
     printf("OOM: vm_map_ram out of memory \n");
#endif
     return -1;
  }

  /* it leaves the case of memory is enough but half in ram, half in swap
   * do the swaping all to swapper to get the all in ram */
  vmap_page_range(caller, mapstart, incpgnum, frm_lst, ret_rg);

  return 0;
}

/* Swap copy content page from source frame to destination frame 
 * @mpsrc  : source memphy
 * @srcfpn : source physical page number (FPN)
 * @mpdst  : destination memphy
 * @dstfpn : destination physical page number (FPN)
 **/
int __swap_cp_page(struct memphy_struct *mpsrc, int srcfpn,
                struct memphy_struct *mpdst, int dstfpn) 
{
  int cellidx;
  int addrsrc,addrdst;

  pthread_mutex_init(&mpsrc->mtx, NULL);pthread_mutex_lock(&mpsrc->mtx);
  pthread_mutex_init(&mpdst->mtx, NULL);pthread_mutex_lock(&mpdst->mtx);
  
  for(cellidx = 0; cellidx < PAGING_PAGESZ; cellidx++)
  {
    addrsrc = srcfpn * PAGING_PAGESZ + cellidx;
    addrdst = dstfpn * PAGING_PAGESZ + cellidx;

    BYTE data;
    MEMPHY_read(mpsrc, addrsrc, &data);
    MEMPHY_write(mpdst, addrdst, data);
  }

  pthread_mutex_unlock(&mpdst->mtx); pthread_mutex_destroy(&mpdst->mtx);
  pthread_mutex_unlock(&mpsrc->mtx); pthread_mutex_destroy(&mpsrc->mtx);

  return 0;
}

/*
 *Initialize a empty Memory Management instance
 * @mm:     self mm
 * @caller: mm owner
 */
int init_mm(struct mm_struct *mm, struct pcb_t *caller)
{
  struct vm_area_struct * vma = malloc(sizeof(struct vm_area_struct));

  mm->pgd = malloc(PAGING_MAX_PGN*sizeof(uint32_t));

  /* By default the owner comes with at least one vma */
  vma->vm_id = 1;
  vma->vm_start = 0;
  vma->vm_end = vma->vm_start;
  vma->sbrk = vma->vm_start;
  struct vm_rg_struct *first_rg = init_vm_rg(vma->vm_start, vma->vm_end);
  enlist_vm_rg_node(&vma->vm_freerg_list, first_rg);

  vma->vm_next = NULL;
  vma->vm_mm = mm; /*point back to vma owner */

  mm->mmap = vma;

  return 0;
}

struct vm_rg_struct* init_vm_rg(int rg_start, int rg_end)
{
  struct vm_rg_struct *rgnode = malloc(sizeof(struct vm_rg_struct));

  rgnode->rg_start = rg_start;
  rgnode->rg_end = rg_end;
  rgnode->rg_next = NULL;

  return rgnode;
}

int enlist_vm_rg_node(struct vm_rg_struct **rglist, struct vm_rg_struct* rgnode)
{
  // pthread_mutex_init(&(*rglist)->mtx, NULL); pthread_mutex_lock(&(*rglist)->mtx);
  rgnode->rg_next = *rglist;
  *rglist = rgnode;
  // pthread_mutex_unlock(&(*rglist)->mtx); pthread_mutex_destroy(&(*rglist)->mtx);

  return 0;
}

int enlist_pgn_node(struct pgn_t **plist, int pgn)
{
  struct pgn_t* pnode = malloc(sizeof(struct pgn_t));

  // pthread_mutex_init(&(*plist)->mtx, NULL); pthread_mutex_lock(&(*plist)->mtx);
  pnode->pgn = pgn;
  pnode->pg_next = *plist;
  *plist = pnode;
  // pthread_mutex_unlock(&(*plist)->mtx); pthread_mutex_destroy(&(*plist)->mtx);

  return 0;
}

int enlist_framephy_node(struct framephy_struct **framephylist, int fpn)
{
  struct framephy_struct* fpnode = malloc(sizeof(struct framephy_struct));
  
  // pthread_mutex_init(&(*framephylist)->mtx, NULL); pthread_mutex_lock(&(*framephylist)->mtx);
  fpnode->fpn = fpn;
  fpnode->fp_next = *framephylist;
  *framephylist = fpnode; 
  // pthread_mutex_unlock(&(*framephylist)->mtx); pthread_mutex_destroy(&(*framephylist)->mtx);

  return 0;
}


int delist_framephy_node(struct framephy_struct **framephylist, int fpn)
{
  struct framephy_struct* fpnode = *framephylist;

  // while(fpnode)
  // {
  //   if (fpnode->fpn == fpn)
  //   {
  //     delist_framephy_node_idx(framephylist, idx);
  //     return 0;
  //   }
  //
  //   idx++;
  //   fpnode = fpnode->fp_next;
  // }

  if (fpnode) return -1;

  if (fpnode && fpnode->fpn == fpn)
  {
    *framephylist = fpnode->fp_next;
    free(fpnode);

    return 0;
  }
  else while(fpnode)
  {
    if (fpnode->fp_next && fpnode->fp_next->fpn == fpn)
    {
      struct framephy_struct* tmp = fpnode->fp_next;
      fpnode->fp_next = tmp->fp_next;
      free(tmp);

      return 0;
    }

    fpnode = fpnode->fp_next;
  }

  return -1;
}


int print_list_fp(struct framephy_struct *ifp)
{
   struct framephy_struct *fp = ifp;
 
   printf("print_list_fp: ");
   if (fp == NULL) {printf("NULL list\n"); return -1;}
   printf("\n");
   while (fp != NULL )
   {
       printf("fp[%d]\n",fp->fpn);
       fp = fp->fp_next;
   }
   printf("\n");
   return 0;
}

int print_list_rg(struct vm_rg_struct *irg)
{
   struct vm_rg_struct *rg = irg;
 
   printf("print_list_rg: ");
   if (rg == NULL) {printf("NULL list\n"); return -1;}
   printf("\n");
   while (rg != NULL)
   {
       printf("rg[%ld->%ld]\n",rg->rg_start, rg->rg_end);
       rg = rg->rg_next;
   }
   printf("\n");
   return 0;
}

int print_list_vma(struct vm_area_struct *ivma)
{
   struct vm_area_struct *vma = ivma;
 
   printf("print_list_vma: ");
   if (vma == NULL) {printf("NULL list\n"); return -1;}
   printf("\n");
   while (vma != NULL )
   {
       printf("va[%ld->%ld]\n",vma->vm_start, vma->vm_end);
       vma = vma->vm_next;
   }
   printf("\n");
   return 0;
}

int print_list_pgn(struct pgn_t *ip)
{
   printf("print_list_pgn: ");
   if (ip == NULL) {printf("NULL list\n"); return -1;}
   printf("\n");
   while (ip != NULL )
   {
       printf("va[%d]-\n",ip->pgn);
       ip = ip->pg_next;
   }
   printf("n");
   return 0;
}

int print_pgtbl(struct pcb_t *caller, uint32_t start, uint32_t end)
{
  int pgn_start,pgn_end;
  int pgit;

  if (end == -1){
    pgn_start = 0;
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, 0);
    end = cur_vma->vm_end;
  }
  pgn_start = PAGING_PGN(start);
  pgn_end = PAGING_PGN(end);

  printf("print_pgtbl: %d - %d", start, end);
  if (caller == NULL) {printf("NULL caller\n"); return -1;}
    printf("\n");


  for(pgit = pgn_start; pgit < pgn_end; pgit++)
  {
     printf("%08ld: %08x\n", pgit * sizeof(uint32_t), caller->mm->pgd[pgit]);
  }

  return 0;
}

//#endif
