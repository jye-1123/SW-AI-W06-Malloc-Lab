/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

// 기본 상수 및 매크로 정의
#define WSIZE 4         // 1워드 크기(헤더랑 푸터 각각 4바이트니까)
#define DSIZE 8         // 2워드 크기(8바이트 정렬과 헤더나 푸터 합산 크기같은 곳에 사용)
#define CHUNKSIZE (1<<12)   // 기본 힙 합장 크기(공간 부족할 때 기본적으로 4096바이트씩 확보)

#define MAX(x,y)    ((x)>(y) ? (x):(y))    // 나중에 힙 확장할 때 필요한 전체 블록 크기와 기본 확장 크기 중 큰 값을 선택할 때 쓸듯

#define PACK(size, alloc)   ((size)|(alloc))   // 블록 크기와 할당 상태를 하나의 값으로 결합하기

#define GET(p)      (*(unsigned int *)(p))                 // 주소 p에 저장된 값을 읽어옴 (여기에서는 저장된 값 0x19를 읽어옴)
#define PUT(p, val)     (*(unsigned int *)(p) = (val))     // 주소 p의 헤더 또는 푸터에 값을 기록 (예를 들어 크기24+할당상태1을 결합한 값 저장하고)

#define GET_SIZE(p)     (GET(p) & ~0x7)     // 블록 크기가 8배수라 하위 3비트는 항상 000. 그래서 AND 연산으로 사이즈가 기록된 공간을 구분할 수 있음
#define GET_ALLOC(p)     (GET(p) & 0x1)     // 가장 오른쪽 비트만 남기기

#define HDRP(bp)        ((char *)(bp) - WSIZE)      // bp는 페이로드의 시작이기 때문에 헤더크기가 4니까 WSIZE만큼 뺴면 헤더주소
#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)     // 페이로드 시작위치에서 블록사이즈만큼 더하고 헤더크기랑 푸터크기 각각4니까 DSIZE 뺴주면 푸터주소

#define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)   ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))   



/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

// void find_fit() {

// }

// void place() {

// }

// void coalesce() {
    
// }

// void extend_heap(size_t words) {
//     char *cur_po;
//     size_t size;

//     size = (words%2) ? (words+1) * WSIZE : words * WSIZE;
//     if ((long)(cur_po = mem_sbrk(size)) == -1)
//     {
//         return NULL;
//     }

//     PUT(HDRP(cur_po), PACK(size, 0));
//     PUT(FTRP(cur_po), PACK(size, 0));
//     PUT(HDRP(NEXT_BLKP(cur_po)), PACK(0, 1));

//     return coalesce(cur_po);
    
// }   


/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{   
    // 첫 힙 공간 할당하고 brk 옮기고 시작 주소 받자
    char *heap_start = mem_sbrk(4112);      // 이제 heap_start가 시작 주소 위치
    if (heap_start == (void *)-1){
        return -1;
    }
    // 정렬 공간 할당하자 4바이트
    PUT(heap_start, 0);
    // 정렬 공간 뒤 4바이트 옮긴 곳인 프롤로그 헤드 주소 위치 받자
    char *cur_pos = heap_start + WSIZE;
    // 프롤로그 헤더 4바이트 할당하자
    PUT(cur_pos, PACK(DSIZE, 1));   // 프롤로그 헤더 값들 넣고
    cur_pos = cur_pos + WSIZE;    // 프롤로그 푸터로 이동
    // 프롤로그 푸터 4바이트 할당하자
    PUT(cur_pos, PACK(DSIZE, 1));
    cur_pos = cur_pos + WSIZE;
    // 가용 블록 헤더 할당
    PUT(cur_pos, PACK(4096,0));
    // 가용 블록 푸터 이동해서 값 할당
    cur_pos += 4096 - WSIZE;
    PUT(cur_pos, PACK(4096, 0));
    // 에필로그 공간 할당하자 4바이트(헤더만 존재함)
    cur_pos += WSIZE;
    PUT(cur_pos, PACK(0, 1));       // 에필로그 실제 크기는 4바이트지만 헤더에 0바이트로 표현하고 할당상태로 기록

    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    // 예외 처리: 요청 크기가 0이면 NULL 반환하자
    if (size == 0)
    {
        return NULL;
    }
    // 페이로드 기준 헤더 푸터 고려해 ALIGN 활용해 8배수 크기로 맞춰주기
    size_t block_size = ALIGN(size + DSIZE);
    // 시작 주소 가져오기
    char *heap_start = mem_heap_lo();
    // 기준 주소 페이로드 위치 받아오기
    // 정렬공간 WSIZE + 프롤로그 DSIZE + 가용블록 헤더 WSIZE
    char *cur_pos = heap_start + WSIZE + DSIZE + WSIZE;    // 일반 블록의 페이로드 주소
    // 에필로그까지 블록 탐색해서 공간 찾자
    while (GET_SIZE(HDRP(cur_pos)) != 0)
    {   
        // 블록 순회하면서 할당가능한 블록인지 검사하고 block_size크기 이상인지도 확인하고
        if (GET_ALLOC(HDRP(cur_pos)) == 0 && GET_SIZE(HDRP(cur_pos)) >= block_size)
        {
            break;
        }
        else cur_pos = NEXT_BLKP(cur_pos);
    }

    size_t cur_size = GET_SIZE(HDRP(cur_pos));

     // 충분히 큰 가용 블록을 찾지 못한 경우 힙 확장
    if (cur_size == 0) 
    {
        // 확장할 크기 결정하기
        size_t extend_size = MAX(block_size, CHUNKSIZE);
        // mem_sbrk호출해서 실패검사하기
        char *new_pos = mem_sbrk(extend_size);    
        if (new_pos == (void *)-1){
            return NULL;
        }
        // 새 블록 페이로드 주소 탐색 포인터로 저장
        cur_pos = new_pos;
        // 기존 에필로그 위치를 새 가용 블록 헤더로 만들기
        PUT(HDRP(cur_pos), PACK(extend_size, 0));
        // 새 가용블록 푸터 만들기
        PUT(FTRP(cur_pos), PACK(extend_size, 0));
        //새 가용블록 뒤에 에필로그 새로 만들기
        char *epil_header = cur_pos + extend_size - WSIZE;
        PUT(epil_header, PACK(0, 1));
        // 확장으로 확보한 블록 전체 크기 저장
        cur_size = extend_size;
    }

    // 가용 사이즈에서 block_size뺸 만큼 일단 기록해두고
    size_t remaining_size = cur_size - block_size;
    if (remaining_size >= 16){  // 남는 공간이 최소크기 이상이면 분할하기
        // 할당 블록의 헤더에 크기와 할당 상태 기록
        PUT(HDRP(cur_pos), PACK(block_size, 1));
        // 변경된 헤더 크기를 기준으로 앞쪽 블록의 푸터를 찾아 같은 값 기록하기
        PUT(FTRP(cur_pos), PACK(block_size, 1));
        // 뒤쪽 남는 블록의 페이로드 주소 계산
        char *remaining_pos = cur_pos + block_size;
        // 뒤 가용 블록 헤더에 남은 크기랑 가용상태 기록
        PUT(HDRP(remaining_pos), PACK(remaining_size, 0));
        // 푸터도 똑같이 채워주기
        PUT(FTRP(remaining_pos), PACK(remaining_size, 0));
    }
    else{   // 남는 공간이 최소사이즈 보다 작으면 분할 X
        // 남는 공간 작으니까 원래 블록 전체 크기 유지하면서(분할x) 헤더에 값 할당
        PUT(HDRP(cur_pos), PACK(cur_size, 1));
        // 푸터에도 같은 크기와 할당 상태 기록
        PUT(FTRP(cur_pos), PACK(cur_size, 1));
    }
    // 사용자한테 할당 블록의 페이로드 주소 반환
    return cur_pos;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    // 인자인 ptr이 오염되지 않은 진짜 ptr인가?를 확인해보는 것도 좋을 듯

    // ptr이 NULL이면 종료
    if (ptr == 0)
    {
        return;
    }

    // 현재 블록의 전체 크기를 헤더에서 읽기
    size_t cur_size = GET_SIZE(HDRP(ptr));

    // 현재 블록의 헤더와 푸터에 기존 크기와 가용 상태 0을 기록하자
    PUT(HDRP(ptr), PACK(cur_size, 0));
    PUT(FTRP(ptr), PACK(cur_size, 0));      // 굳이 할필요 없음

    // 앞뒤 블록 할당 상태 확인하기
    char *prev_pos = PREV_BLKP(ptr);
    char *next_pos = NEXT_BLKP(ptr);

    // 병합 전에 앞뒤 블록 할당 상태 저장
    int prev_alloc = GET_ALLOC(HDRP(prev_pos));
    int next_alloc = GET_ALLOC(HDRP(next_pos));

    // 인접한 가용 블록이 있으면 병합
    if (prev_alloc == 1 && next_alloc == 1)
    {
        // 모두 할당이니까 종료
        return;
    } else if (prev_alloc == 1 && next_alloc == 0){
        // 현재 블록과 다음 가용 블록 전체 크기
        size_t merge_size = cur_size + GET_SIZE(HDRP(next_pos));
    
        // 현재 블록 헤더와 다음 블록 푸터 합산 크기 기록
        char *merge_footer = FTRP(next_pos);
        PUT(HDRP(ptr), PACK(merge_size, 0));
        PUT(merge_footer, PACK(merge_size, 0));
    } else if (prev_alloc == 0 && next_alloc == 1){
        // 이전 가용 블록이랑 현재 블록의 전체 크기 합산
        size_t merge_size = GET_SIZE(HDRP(prev_pos)) + cur_size;

        // 헤더 변경 전에 병합 영역 마지막 푸터 주소 저장
        char *merge_footer = FTRP(ptr);

        // 이전 블록 헤더랑 현재 블록 푸터 합산 크기 기록
        PUT(HDRP(prev_pos), PACK(merge_size, 0));
        PUT(merge_footer, PACK(merge_size, 0));
    } else{
        // 이전/현재/다음 모두 크기 합산
        size_t merge_size = GET_SIZE(HDRP(prev_pos)) + cur_size + GET_SIZE(HDRP(next_pos));
        
        char *merge_footer = FTRP(next_pos);

        PUT(HDRP(prev_pos), PACK(merge_size, 0));
        PUT(merge_footer, PACK(merge_size, 0));
    }
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    // ptr에 공간 없으니까 size만큼 공간 할당
    if (ptr == NULL)
    {
        return mm_malloc(size);
    }
    // 요청 크기가 0이면 기존 블록 해제하고 NULL 반환
    if (size == 0)
    {
        mm_free(ptr);
        return NULL;
    }
    
    // 기존 블록의 전체 크기는 주소가 아니라 바이트 수니까 size_t로 저장
    size_t cur_size = GET_SIZE(HDRP(ptr));
    // 헤더랑 푸터 제외한 기존 페이로드 용량 계산
    // 정렬용 공간도 포함하므로 원래 요청 크기와는 다를 수 있음
    size_t use_size = cur_size - DSIZE;

    // 요청 크기만큼 새 블록을 할당하고 페이로드 주소 받음
    char *cur_pos = mm_malloc(size);
    // 새 블록 할당 실패하면 기존 블록 유지하고 NULL반환
    if (cur_pos == NULL)
    {
        return NULL;
    }
    
    // 기존 페이로드 용량과 새 요청 크기 중 작은 값을 복사 크기로 결정하고
    size_t copy_size = use_size;
    if (size < copy_size)
    {
        copy_size = size;
    }
    
    // memcpy로 기존 페이로드에서 새 페이로드로 데이터 복사
    memcpy(cur_pos, ptr, copy_size);

    // 복사 끝나면 기존 블록 해제
    mm_free(ptr);

    // 데이터를 복사한 새 블록의 페이로드 주소 반환
    return cur_pos;
}
