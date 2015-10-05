
#include "mm_alloc.h"
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#define ASSERT assert
#define align4(x) (((((x)-1)>>2)<<2)+4)
#define S_BLOCK_SIZE sizeof ( struct s_block )
// mutex to protect allocation of memory
pthread_mutex_t mm_mutex = PTHREAD_MUTEX_INITIALIZER;
#define TRUE 1
#define FALSE 0
s_block_ptr heap_ptr = NULL; /* beginning point of heap*/
s_block_ptr last; /* last visited block */
s_block_ptr
find_block (s_block_ptr *last , size_t size ){
s_block_ptr p;
pthread_mutex_lock( &mm_mutex );
for(p =heap_ptr; p != NULL; p = p->next){
*last = p;
pthread_mutex_unlock( &mm_mutex );
if(p && !(p->free && p->size >= size ))
return p;
pthread_mutex_unlock( &mm_mutex );
}
pthread_mutex_unlock( &mm_mutex );
return NULL;
}
/* Invoke sbrk to extend break point */
s_block_ptr
extend_heap (s_block_ptr last , size_t size){
/* Current break is address of the new block */
s_block_ptr new_block = (s_block_ptr)sbrk(size);
pthread_mutex_lock( &mm_mutex );
if (sbrk( S_BLOCK_SIZE + size) < 0)
return (NULL );
new_block->size = size;
new_block->next = NULL;
new_block->prev = last;
new_block->ptr = new_block->data;
new_block->free = TRUE;
pthread_mutex_unlock( &mm_mutex );
/* heap is empty */
if (last){
last ->next = new_block;
new_block->free =FALSE;
}
pthread_mutex_unlock( &mm_mutex );
return new_block;
}
/* fusing block with neighbors */
s_block_ptr
fusion_block( s_block_ptr b){
ASSERT(b->free == TRUE);
/* If b's next block exists and b's block is also free,
* then merge b and b's next block. */
if (b->next && b->next ->free ){
b->size =b->size + S_BLOCK_SIZE + b->next ->size;
if (b->next->next)
b->next->next->prev = b;
b->next = b->next->next;
}
return (b);
}
void
split_block( s_block_ptr b, size_t new_size){
s_block_ptr new_block;
pthread_mutex_lock( &mm_mutex );
if(new_size > 0){
new_block = ( s_block_ptr )(b->data + new_size);
new_block->size = b->size - new_size- S_BLOCK_SIZE ;
new_block ->next = b->next;
new_block ->prev = b;
new_block ->free =TRUE;
new_block->ptr = new_block ->data;
b->size = new_size;
b->next = new_block;
pthread_mutex_lock( &mm_mutex );
if (new_block->next)
new_block->next ->prev = new_block;
b->next = new_block;
pthread_mutex_lock( &mm_mutex );
}
pthread_mutex_lock( &mm_mutex );
}
static void
copy_block ( s_block_ptr s , s_block_ptr data){
int *sd ,* ddata ;
size_t j;
sd = s ->ptr;
ddata = data ->ptr;
pthread_mutex_lock( &mm_mutex );
for (j=0; (j*4)<s->size && (j*4)<data ->size; j++){
ddata [j] = sd [j];
pthread_mutex_unlock( &mm_mutex );
}
}
void *
mm_malloc ( size_t size ){
s_block_ptr b,last;
size_t s;
/* Align the requested size */
s = align4 (size );
pthread_mutex_lock( &mm_mutex );
if (heap_ptr == NULL) {
last = heap_ptr;
b = find_block (&last ,s);
pthread_mutex_unlock( &mm_mutex );
if (b) {
/* NEED TO SPLIT*/
if ((b->size - s) >= ( S_BLOCK_SIZE + 4))
split_block (b,s);
pthread_mutex_unlock( &mm_mutex );
b->free =FALSE;
} else {
/* No fitting block THEN the heap needs to be extended*/
b = extend_heap (last ,s);
if (!b)
return (NULL );
}
pthread_mutex_unlock( &mm_mutex );
}
else {
b = extend_heap (NULL ,s);
if (!b)
return (NULL );
heap_ptr = b;
}
return (b->data);
}
s_block_ptr
get_block (void *p){
/* If p is a valid block address, convert it to a s_block_ptr */
char *temp;
temp =(char*)p;
return (s_block_ptr)(temp -= S_BLOCK_SIZE );
}
void *base =NULL;
int is_valid_addr (void *p){
if (base){
if (p>base && p< sbrk (0)){
return (p == ( get_block (p))->ptr );
}
}
return FALSE;
}
void *
mm_realloc (void *p, size_t size){
size_t s;
s_block_ptr pb, new;
void *newp;
/* exclude some special conditions */
if (!p)
pthread_mutex_lock( &mm_mutex );
return ( mm_malloc (size ));
pthread_mutex_unlock( &mm_mutex );
if ( is_valid_addr (p)){
s = align4 (size );
pb = get_block (p);
if (pb->size >= s){
/* If space is enough, try to split current block */
if (pb->size - s >= ( S_BLOCK_SIZE + 4))
split_block (pb,s);
}
else{
/* Try fusion with next if possible */
if (pb->next && pb->next ->free
&& (pb->size + S_BLOCK_SIZE + pb->next ->size) >= s){
fusion_block(pb);
if (pb->size - s >= ( S_BLOCK_SIZE + 4))
split_block (pb,s);
}
else
{
pthread_mutex_lock( &mm_mutex );
/* no way to get enough space on current node, have to malloc new memory */
newp = mm_malloc (s);
pthread_mutex_unlock( &mm_mutex );
if (!newp)
return (NULL );
new = get_block (newp );
/*copy the data*/
copy_block (pb,new );
free(p);
return (newp);
}
}
return (p);
}
return (NULL);
}
void
mm_free(void *p){
s_block_ptr b;
/* If the pointer ptr is valid and get a non-NULL block address */
pthread_mutex_lock(&mm_mutex);
if(is_valid_addr (p)){
b = get_block (p);
b->free = TRUE;
pthread_mutex_unlock( &mm_mutex );
/* Step backward and fusion two blocks */
if(b->prev && b->prev ->free)
b = fusion_block(b->prev );
pthread_mutex_unlock( &mm_mutex );
/* Also try fusion with the next block */
if (b->next)
fusion_block(b);
else{
/* free the end of the heap */
if (b->prev)
b->prev->next = NULL;
/* Else release the end of heap */
else
b->prev->next = NULL;
brk(b);
}
}
pthread_mutex_unlock( &mm_mutex );



