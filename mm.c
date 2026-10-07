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

static char *search_pos = NULL;     // 함수 호출 사이에 다음 탐색 시작 주소 유지 (지역변수면 함수 소멸하며 사라지기 때문에 함수 밖 선언)

static void *find_fit(size_t block_size)
{
    char *heap_start = mem_heap_lo();       // 힙 시작 주소
    char *start_pos = search_pos;           // 이번 탐색 시작 위치 저장
    char *cur_pos = start_pos;              // 저장된 블록부터 탐색

    while (GET_SIZE(HDRP(cur_pos)) != 0)    // 시작 위치부터 에필로그까지 탐색
    {
        if (GET_ALLOC(HDRP(cur_pos)) == 0 &&
            GET_SIZE(HDRP(cur_pos)) >= block_size)
        {
            search_pos = cur_pos;          // 다음 탐색 위치 갱신
            return cur_pos;                // 적합한 블록 주소 반환
        }

        cur_pos = NEXT_BLKP(cur_pos);      // 다음 블록으로 이동
    }                                      // 첫 번째 반복문 종료

    cur_pos = heap_start + WSIZE + DSIZE + WSIZE; // 첫 일반 블록으로 돌아감

    while (cur_pos != start_pos &&
           GET_SIZE(HDRP(cur_pos)) != 0)    // 처음부터 시작 위치 직전까지 탐색
    {
        if (GET_ALLOC(HDRP(cur_pos)) == 0 &&
            GET_SIZE(HDRP(cur_pos)) >= block_size)
        {
            search_pos = cur_pos;          // 다음 탐색 위치 갱신
            return cur_pos;                // 적합한 블록 주소 반환
        }

        cur_pos = NEXT_BLKP(cur_pos);      // 다음 블록으로 이동
    }

    return NULL;                           // 한 바퀴 탐색했지만 적합한 블록 없음
}

static void place(void *ptr, size_t block_size) {
    size_t cur_size = GET_SIZE(HDRP(ptr));
    size_t remaining_size = cur_size - block_size;      // 할당 후 남는 크기

    if (remaining_size >= 16) {
        PUT(HDRP(ptr), PACK(block_size, 1));        // 앞쪽 할당 블록 헤더 기록
        PUT(FTRP(ptr), PACK(block_size, 1));        // 푸터 기록

        char *remaning_pos = (char *)ptr + block_size;      // 뒤쪽 가용 블록의 페이로드 주소

        PUT(HDRP(remaning_pos), PACK(remaining_size, 0));        // 남은 가용 블록 헤더 기록
        PUT(FTRP(remaning_pos), PACK(remaining_size, 0));        // 남은 가용 블록 푸터 기록
    } else {
        PUT(HDRP(ptr), PACK(cur_size, 1));        // 원래 전체 크기 유지하고 헤더 할당 상태 변경
        PUT(FTRP(ptr), PACK(cur_size, 1));        // 푸터에도 동일한 크기와 할당 상태 기록
    }
}

static void *coalesce(void *ptr) {

    size_t cur_size = GET_SIZE(HDRP(ptr));

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
        return ptr;
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

        // 추가: 병합 결과는 이전 블록에서 시작하므로 반환할 주소 변경
        ptr = prev_pos;
    } else {
        // 이전/현재/다음 모두 크기 합산
        size_t merge_size = GET_SIZE(HDRP(prev_pos)) + cur_size + GET_SIZE(HDRP(next_pos));
        
        char *merge_footer = FTRP(next_pos);

        PUT(HDRP(prev_pos), PACK(merge_size, 0));
        PUT(merge_footer, PACK(merge_size, 0));

        // 추가: 병합 결과는 이전 블록에서 시작하므로 반환할 주소 변경
        ptr = prev_pos;
    }

    if (search_pos >= (char *)ptr && search_pos < NEXT_BLKP(ptr)) {     // 저장된 탐색 위치가 병합 결과 블록 내부에 있으면 시작 주소로 보정
        search_pos = (char *)ptr;       // 병합된 블록의 페이로드 시작 주소
    }

    return ptr;
}

static void *extend_heap(size_t words) {
    char *cur_po;
    size_t size;

    size = (words%2) ? (words+1) * WSIZE : words * WSIZE;
    if ((long)(cur_po = mem_sbrk(size)) == -1) {
        return NULL;
    }

    PUT(HDRP(cur_po), PACK(size, 0));
    PUT(FTRP(cur_po), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(cur_po)), PACK(0, 1));

    return coalesce(cur_po);
    
}   


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
    search_pos = heap_start + WSIZE + DSIZE + WSIZE;

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
    if (size == 0) {
        return NULL;
    }

    size_t block_size = ALIGN(size + DSIZE);        // 헤더와 푸터 포함 전체 크기 8배수로 올림
    char *cur_pos = find_fit(block_size);

    if (cur_pos == NULL) {      // 실패한 경우에만 힙 확장
        size_t extend_size = MAX(block_size, CHUNKSIZE);        // 필요한 크기와 기본 확장 크기 중 큰 값 선택
        cur_pos = extend_heap(extend_size / WSIZE);     // 워드 단위로 확장하고 병합 결과 주소를 받음

        if (cur_pos == NULL) {      // 확장 실패 시 NULL 반환
            return NULL;
        }
    }
    place(cur_pos, block_size);     // 선택한 블록을 할당하고 필요하면 분할
    return cur_pos;     // 사용자에게 페이로드 주소 반환
} 

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{

    if (ptr == 0)
    {
        return;
    }
    
    size_t cur_size = GET_SIZE(HDRP(ptr));      // 현재 블록의 전체 크기를 헤더에서 읽기

    PUT(HDRP(ptr), PACK(cur_size, 0));      // 현재 블록의 헤더 기존 크기와 가용 상태 갱신
    PUT(FTRP(ptr), PACK(cur_size, 0));      // 현재 블록의 푸터도 기존 크기와 가용 상태로 갱신

    coalesce(ptr);

}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    if (ptr == NULL) {
        return mm_malloc(size);
    }
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }
    
    size_t cur_size = GET_SIZE(HDRP(ptr));      // 기존 블록의 전체 크기는 주소가 아니라 바이트 수니까 size_t로 저장
    size_t use_size = cur_size - DSIZE;     // 헤더랑 푸터 제외한 기존 페이로드 용량 계산  정렬용 공간도 포함하므로 원래 요청 크기와는 다를 수 있음

    if (size <= use_size) {     // 기존 페이로드 용량으로 새 요청을 수용할 수 있으면 재할당과 복사 생략
        return ptr;
    }

    size_t block_size = ALIGN(size + DSIZE);        // 새 요청에 필요한 전체 블록 크기
    char *next_po = NEXT_BLKP(ptr);       // 다음 블록의 페이로드 주소
    
    size_t next_size = GET_SIZE(HDRP(next_po));     // 다음 블록 전체 크기(에필로그면 0)
    size_t available_size = cur_size;       // 현재 위치에서 확보할 수 있는 전체 크기
    int heap_end = (next_size == 0);        // 현재 블록 바로 뒤가 에필로그인지 확인

    if (GET_ALLOC(HDRP(next_po)) == 0) {
        available_size += next_size;        // 다음 가용 블록까지 합산
        heap_end = (GET_SIZE(HDRP(NEXT_BLKP(ptr))) == 0);        // 다음 가용 블록 뒤가 에필로그인지 확인
    }

    if (heap_end && available_size < block_size) {
        size_t extend_size = block_size - available_size;       // 요청을 충족하는 데 부족한 사이즈 계산
        extend_size = MAX(extend_size, 2*DSIZE);        // 새 가용 블록은 최소 16바이트 확보
        
        if (extend_heap(extend_size / WSIZE) != NULL) {
            next_po = NEXT_BLKP(ptr);       // 확장과 병합 후 현재 블록 바로 뒤의 가용 블록 주소 다시 확인
        }
        
    }
    
    

    if (GET_ALLOC(HDRP(next_po)) == 0) {
        size_t merge_size = GET_SIZE(HDRP(ptr)) + GET_SIZE(HDRP(next_po));      // 두 블록의 전체 크기 합산
        if (merge_size >= block_size) {
            char *merge_footer = FTRP(next_po);     // 헤더 변경 전에 합쳐진 영역의 마지막 푸터 저장

            PUT(HDRP(ptr), PACK(merge_size, 1));        // 현재 블록 헤더를 합산 크기로 갱신
            PUT(merge_footer, PACK(merge_size, 1));        // 합쳐진 영역 푸터 갱신

            if (search_pos >= (char *)ptr && search_pos < NEXT_BLKP(ptr)) {     // 탐색 위치가 흡수된 영역 안에 있으면 현재 블록 시작으로 보정
                search_pos = (char *)ptr;       // 유효한 페이로드 시작 주소로 변경
            }
            
            place(ptr, block_size);     // 필요한 크기만 할당하고 충분한 여유 공간은 분할
            return ptr;     // 데이터 이동 없이 기존 페이로드 주소 반환
        }
    }

    //      앞쪽까지도 확인하는 코드이나 현재 코드에서는 오히려 util 성능 하락 (66점)
    char *prev_po = PREV_BLKP(ptr);        // 이전 블록의 페이로드 주소 구하기

    if (GET_ALLOC(HDRP(prev_po)) == 0) {
        size_t merge_size = GET_SIZE(HDRP(prev_po)) + cur_size;     // 이전 + 현재 전체 크기
        char *merge_footer = FTRP(ptr);     // 병합 영역의 푸터는 현재 블록의 푸터

        if (merge_size < block_size && GET_ALLOC(HDRP(next_po)) == 0) {        // 크기가 부족하고 다음 블록도 가용이면 포함
            merge_size += GET_SIZE(HDRP(NEXT_BLKP(ptr)));       // 다음 블록 크기까지 합산
            merge_footer = FTRP(next_po);       // 병합 영역 끝을 다음 블록 푸터로 변경
        }

        if (merge_size >= block_size) {     // 합산 크기가 block_size 이상이면
            memmove(prev_po, ptr, use_size);        // 겹칠 수 있는 기존 데이터들 먼저 앞쪽으로 이동

            PUT(HDRP(prev_po), PACK(merge_size, 1));
            PUT(merge_footer, PACK(merge_size, 1));

            place(prev_po, block_size);     // 필요한 크기를 할당하고 남은 공간 분할

            if (merge_size - block_size >= 16) {        // 분할로 가용 블록 생겼으면
                coalesce(NEXT_BLKP(prev_po));       // 남겨 둔 뒤쪽 가용 블록과 인접할 수 있으므로 병합
            }
                return prev_po;     // 이동한 데이터의 새 페이로드 주소 반환
        }
    }
    
    char *cur_pos = mm_malloc(size);        // 요청 크기만큼 새 블록을 할당하고 페이로드 주소 받음
    if (cur_pos == NULL) {
        return NULL;
    }
    
    size_t copy_size = use_size;        // 기존 페이로드 용량과 새 요청 크기 중 작은 값을 복사 크기로 결정하고
    if (size < copy_size) {
        copy_size = size;
    }
    
    memcpy(cur_pos, ptr, copy_size);        // memcpy로 기존 페이로드에서 새 페이로드로 데이터 복사
    mm_free(ptr);       // 복사 끝나면 기존 블록 해제

    return cur_pos;     // 데이터를 복사한 새 블록의 페이로드 주소 반환

}
