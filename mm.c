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

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "bestteam",
    /* First member's full name */
    "LRC",
    /* First member's email address */
    "1987477513@qq.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
/* ���Ͻ��ж��� */
#define ALIGNMENT 8
#define ALIGN(size) ((((size) + (ALIGNMENT-1)) / (ALIGNMENT)) * (ALIGNMENT))

#define WSIZE     4
#define DSIZE     8

/* ÿ����չ�ѵĿ��С��ϵͳ���á���ʱ��������һ����չһ��飬Ȼ����������һ��飩 */
#define INITCHUNKSIZE (1<<6)
#define CHUNKSIZE (1<<12)

#define LISTMAX     16

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

#define PACK(size, alloc) ((size) | (alloc))

/* �����ָ�����ڵ��ڴ渳ֵʱҪע������ת����������о��� */
#define GET(p)            (*(unsigned int *)(p))
#define PUT(p, val)       (*(unsigned int *)(p) = (val))

#define SET_PTR(p, ptr) (*(unsigned int *)(p) = (unsigned int)(ptr))

#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(ptr) ((char *)(ptr) - WSIZE)
#define FTRP(ptr) ((char *)(ptr) + GET_SIZE(HDRP(ptr)) - DSIZE)

#define NEXT_BLKP(ptr) ((char *)(ptr) + GET_SIZE((char *)(ptr) - WSIZE))
#define PREV_BLKP(ptr) ((char *)(ptr) - GET_SIZE((char *)(ptr) - DSIZE))

#define PRED_PTR(ptr) ((char *)(ptr))
#define SUCC_PTR(ptr) ((char *)(ptr) + WSIZE)

#define PRED(ptr) (*(char **)(ptr))
#define SUCC(ptr) (*(char **)(SUCC_PTR(ptr)))
/* ������б� */
void *segregated_free_lists[LISTMAX];
/* ��չ�� */
static void *extend_heap(size_t size);
/* �ϲ����ڵ�Free block */
static void *coalesce(void *ptr);
/* ��prt��ָ���free block����allocate size��С�Ŀ飬���ʣ�µĿռ����2*DWSIZE�������������Free list */
static void *place(void *ptr, size_t size);
/* ��ptr��ָ���free block���뵽������б��� */
static void insert_node(void *ptr, size_t size);
/* ��ptr��ָ��Ŀ�ӷ�����б���ɾ�� */
static void delete_node(void *ptr);
static void *extend_heap(size_t size)
{
	void *ptr;
	/* �ڴ���� */
	size = ALIGN(size);
	/* ϵͳ���á�sbrk����չ�� */
	if ((ptr = mem_sbrk(size)) == (void *)-1)
		return NULL;

	/* ���øո���չ��free���ͷ��β */
	PUT(HDRP(ptr), PACK(size, 0));
	PUT(FTRP(ptr), PACK(size, 0));
	/* ע��������ǶѵĽ�β�����Ի�Ҫ����һ�½�β */
	PUT(HDRP(NEXT_BLKP(ptr)), PACK(0, 1));
	/* ���úú�����뵽������б��� */
	insert_node(ptr, size);
	/* �������free���ǰ��Ҳ������һ��free�飬������Ҫ�ϲ� */
	return coalesce(ptr);
}
static void insert_node(void *ptr, size_t size)
{
	int listnumber = 0;
	void *search_ptr = NULL;
	void *insert_ptr = NULL;

	/* ͨ����Ĵ�С�ҵ���Ӧ���� */
	while ((listnumber < LISTMAX - 1) && (size > 1))
	{
		size >>= 1;
		listnumber++;
	}

	/* �ҵ���Ӧ�������ڸ����м���Ѱ�Ҷ�Ӧ�Ĳ���λ�ã��Դ˱������п���С��������� */
	search_ptr = segregated_free_lists[listnumber];
	while ((search_ptr != NULL) && (size > GET_SIZE(HDRP(search_ptr))))
	{
		insert_ptr = search_ptr;
		search_ptr = PRED(search_ptr);
	}

	/* ѭ������������� */
	if (search_ptr != NULL)
	{
		/* 1. ->xx->insert->xx ���м����*/
		if (insert_ptr != NULL)
		{
			SET_PTR(PRED_PTR(ptr), search_ptr);
			SET_PTR(SUCC_PTR(search_ptr), ptr);
			SET_PTR(SUCC_PTR(ptr), insert_ptr);
			SET_PTR(PRED_PTR(insert_ptr), ptr);
		}
		/* 2. [listnumber]->insert->xx �ڿ�ͷ���룬���Һ�����֮ǰ��free��*/
		else
		{
			SET_PTR(PRED_PTR(ptr), search_ptr);
			SET_PTR(SUCC_PTR(search_ptr), ptr);
			SET_PTR(SUCC_PTR(ptr), NULL);
			segregated_free_lists[listnumber] = ptr;
		}
	}
	else
	{
		if (insert_ptr != NULL)
		{ /* 3. ->xxxx->insert �ڽ�β����*/
			SET_PTR(PRED_PTR(ptr), NULL);
			SET_PTR(SUCC_PTR(ptr), insert_ptr);
			SET_PTR(PRED_PTR(insert_ptr), ptr);
		}
		else
		{ /* 4. [listnumber]->insert ����Ϊ�գ����ǵ�һ�β��� */
			SET_PTR(PRED_PTR(ptr), NULL);
			SET_PTR(SUCC_PTR(ptr), NULL);
			segregated_free_lists[listnumber] = ptr;
		}
	}
}
static void delete_node(void *ptr)
{
	int listnumber = 0;
	size_t size = GET_SIZE(HDRP(ptr));

	/* ͨ����Ĵ�С�ҵ���Ӧ���� */
	while ((listnumber < LISTMAX - 1) && (size > 1))
	{
		size >>= 1;
		listnumber++;
	}

	/* ������������������ֿ����� */
	if (PRED(ptr) != NULL)
	{
		/* 1. xxx-> ptr -> xxx */
		if (SUCC(ptr) != NULL)
		{
			SET_PTR(SUCC_PTR(PRED(ptr)), SUCC(ptr));
			SET_PTR(PRED_PTR(SUCC(ptr)), PRED(ptr));
		}
		/* 2. [listnumber] -> ptr -> xxx */
		else
		{
			SET_PTR(SUCC_PTR(PRED(ptr)), NULL);
			segregated_free_lists[listnumber] = PRED(ptr);
		}
	}
	else
	{
		/* 3. [listnumber] -> xxx -> ptr */
		if (SUCC(ptr) != NULL)
		{
			SET_PTR(PRED_PTR(SUCC(ptr)), NULL);
		}
		/* 4. [listnumber] -> ptr */
		else
		{
			segregated_free_lists[listnumber] = NULL;
		}
	}
}
static void *coalesce(void *ptr)
{
	_Bool is_prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(ptr)));
	_Bool is_next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
	size_t size = GET_SIZE(HDRP(ptr));
	/* ����ptr��ָ���ǰ�����ڿ����������Է�Ϊ���ֿ����� */
	/* ����ע�⵽�������ǵĺϲ���������ԣ������ܳ����������ڵ�free�� */
	/* 1.ǰ���Ϊallocated�飬�����ϲ���ֱ�ӷ��� */
	if (is_prev_alloc && is_next_alloc)
	{
		return ptr;
	}
	/* 2.ǰ��Ŀ���allocated�����Ǻ���Ŀ���free�ģ���ʱ������free��ϲ� */
	else if (is_prev_alloc && !is_next_alloc)
	{
		delete_node(ptr);
		delete_node(NEXT_BLKP(ptr));
		size += GET_SIZE(HDRP(NEXT_BLKP(ptr)));
		PUT(HDRP(ptr), PACK(size, 0));
		PUT(FTRP(ptr), PACK(size, 0));
	}
	/* 3.����Ŀ���allocated������ǰ��Ŀ���free�ģ���ʱ������free��ϲ� */
	else if (!is_prev_alloc && is_next_alloc)
	{
		delete_node(ptr);
		delete_node(PREV_BLKP(ptr));
		size += GET_SIZE(HDRP(PREV_BLKP(ptr)));
		PUT(FTRP(ptr), PACK(size, 0));
		PUT(HDRP(PREV_BLKP(ptr)), PACK(size, 0));
		ptr = PREV_BLKP(ptr);
	}
	/* 4.ǰ�������鶼��free�飬��ʱ��������ͬʱ�ϲ� */
	else
	{
		delete_node(ptr);
		delete_node(PREV_BLKP(ptr));
		delete_node(NEXT_BLKP(ptr));
		size += GET_SIZE(HDRP(PREV_BLKP(ptr))) + GET_SIZE(HDRP(NEXT_BLKP(ptr)));
		PUT(HDRP(PREV_BLKP(ptr)), PACK(size, 0));
		PUT(FTRP(NEXT_BLKP(ptr)), PACK(size, 0));
		ptr = PREV_BLKP(ptr);
	}

	/* ���ϲ��õ�free����뵽�������ӱ��� */
	insert_node(ptr, size);

	return ptr;
}
static void *place(void *ptr, size_t size)
{
	size_t ptr_size = GET_SIZE(HDRP(ptr));
	/* allocate size��С�Ŀռ��ʣ��Ĵ�С */
	size_t remainder = ptr_size - size;

	delete_node(ptr);

	/* ���ʣ��Ĵ�СС����С�飬�򲻷���ԭ�� */
	if (remainder < DSIZE * 2)
	{
		PUT(HDRP(ptr), PACK(ptr_size, 1));
		PUT(FTRP(ptr), PACK(ptr_size, 1));
	}

	/* �������ԭ�飬������Ҫע������һ���������binary-bal.rep��binary2-bal.rep�����֣���
	*  ���ÿ��allocate�Ŀ��С����С����С���������˳�����Ļ������ǵ�free�齫�ᱻ���𡱳��������ֽṹ��
	*  ����s����С�Ŀ飬B�����Ŀ�

 s      B      s       B     s      B      s     B
+--+----------+--+----------+-+-----------+-+---------+
|  |          |  |          | |           | |         |
|  |          |  |          | |           | |         |
|  |          |  |          | |           | |         |
+--+----------+--+----------+-+-----------+-+---------+

	*  ����������ûʲô���⣬��������������free��ʱ���ǰ��ա�С����С���󡰵�˳�����ͷŵĻ��ͻ���֡�external fragmentation��
	*  ���統���򽫴�Ŀ�ȫ���ͷ��ˣ���С�Ŀ�������allocated��

 s             s             s             s
+--+----------+--+----------+-+-----------+-+---------+
|  |          |  |          | |           | |         |
|  |   Free   |  |   Free   | |   Free    | |   Free  |
|  |          |  |          | |           | |         |
+--+----------+--+----------+-+-----------+-+---------+

	*  ������ʹ�����кܶ�free�Ĵ�����ʹ�ã������������ǲ��������ģ����ǲ��ܽ����Ǻϲ��������һ������һ����СΪB+1��allocate����
	*  ���Ǿͻ���Ҫ����ȥ��һ��Free��
	*  ����෴��������Ǹ���allocate��Ĵ�С��С�Ŀ���������ĵط������ﵽ�����������ĵط���

 s  s  s  s  s  s      B            B           B
+--+--+--+--+--+--+----------+------------+-----------+
|  |  |  |  |  |  |          |            |           |
|  |  |  |  |  |  |          |            |           |
|  |  |  |  |  |  |          |            |           |
+--+--+--+--+--+--+----------+------------+-----------+

	*  ������ʹ���������ͷ�s����B������Ҳ�ܹ��ϲ�free�飬�������external fragmentation
	*  �����С������ж��Ǹ���binary-bal.rep��binary2-bal.rep�������ļ����õģ���������96�����ܹ��ﵽ����ֵ
	*
	*/
	else if (size >= 96)
	{
		PUT(HDRP(ptr), PACK(remainder, 0));
		PUT(FTRP(ptr), PACK(remainder, 0));
		PUT(HDRP(NEXT_BLKP(ptr)), PACK(size, 1));
		PUT(FTRP(NEXT_BLKP(ptr)), PACK(size, 1));
		insert_node(ptr, remainder);
		return NEXT_BLKP(ptr);
	}

	else
	{
		PUT(HDRP(ptr), PACK(size, 1));
		PUT(FTRP(ptr), PACK(size, 1));
		PUT(HDRP(NEXT_BLKP(ptr)), PACK(remainder, 0));
		PUT(FTRP(NEXT_BLKP(ptr)), PACK(remainder, 0));
		insert_node(NEXT_BLKP(ptr), remainder);
	}
	return ptr;
}
/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
	int listnumber;
	char *heap;

	/* ��ʼ������������� */
	for (listnumber = 0; listnumber < LISTMAX; listnumber++)
	{
		segregated_free_lists[listnumber] = NULL;
	}

	/* ��ʼ���� */
	if ((long)(heap = mem_sbrk(4 * WSIZE)) == -1)
		return -1;

	/* ����Ľṹ�μ���������ġ��ѵ���ʼ�ͽ����ṹ�� */
	PUT(heap, 0);
	PUT(heap + (1 * WSIZE), PACK(DSIZE, 1));
	PUT(heap + (2 * WSIZE), PACK(DSIZE, 1));
	PUT(heap + (3 * WSIZE), PACK(0, 1));

	/* ��չ�� */
	if (extend_heap(INITCHUNKSIZE) == NULL)
		return -1;

	return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
	if (size == 0)
		return NULL;
	/* �ڴ���� */
	if (size <= DSIZE)
	{
		size = 2 * DSIZE;
	}
	else
	{
		size = ALIGN(size + DSIZE);
	}


	int listnumber = 0;
	size_t searchsize = size;
	void *ptr = NULL;

	while (listnumber < LISTMAX)
	{
		/* Ѱ�Ҷ�Ӧ�� */
		if (((searchsize <= 1) && (segregated_free_lists[listnumber] != NULL)))
		{
			ptr = segregated_free_lists[listnumber];
			/* �ڸ���Ѱ�Ҵ�С���ʵ�free�� */
			while ((ptr != NULL) && ((size > GET_SIZE(HDRP(ptr)))))
			{
				ptr = PRED(ptr);
			}
			/* �ҵ���Ӧ��free�� */
			if (ptr != NULL)
				break;
		}

		searchsize >>= 1;
		listnumber++;
	}

	/* û���ҵ����ʵ�free�飬��չ�� */
	if (ptr == NULL)
	{
		if ((ptr = extend_heap(MAX(size, CHUNKSIZE))) == NULL)
			return NULL;
	}

	/* ��free����allocate size��С�Ŀ� */
	ptr = place(ptr, size);

	return ptr;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
	size_t size = GET_SIZE(HDRP(ptr));

	PUT(HDRP(ptr), PACK(size, 0));
	PUT(FTRP(ptr), PACK(size, 0));

	/* �������������� */
	insert_node(ptr, size);
	/* ע��ϲ� */
	coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
	void *new_block = ptr;
	int remainder;

	if (size == 0)
		return NULL;

	/* �ڴ���� */
	if (size <= DSIZE)
	{
		size = 2 * DSIZE;
	}
	else
	{
		size = ALIGN(size + DSIZE);
	}

	/* ���sizeС��ԭ����Ĵ�С��ֱ�ӷ���ԭ���Ŀ� */
	if ((remainder = GET_SIZE(HDRP(ptr)) - size) >= 0)
	{
		return ptr;
	}
	/* �����ȼ���ַ������һ�����Ƿ�Ϊfree����߸ÿ��ǶѵĽ����飬��Ϊ����Ҫ�������������ڵ�free�飬�Դ˼�С��external fragmentation�� */
	else if (!GET_ALLOC(HDRP(NEXT_BLKP(ptr))) || !GET_SIZE(HDRP(NEXT_BLKP(ptr))))
	{
		/* ��ʹ���Ϻ���������ַ�ϵ�free��ռ�Ҳ��������Ҫ��չ�� */
		if ((remainder = GET_SIZE(HDRP(ptr)) + GET_SIZE(HDRP(NEXT_BLKP(ptr))) - size) < 0)
		{
			if (extend_heap(MAX(-remainder, CHUNKSIZE)) == NULL)
				return NULL;
			remainder += MAX(-remainder, CHUNKSIZE);
		}

		/* ɾ���ո����õ�free�鲢�����¿��ͷβ */
		delete_node(NEXT_BLKP(ptr));
		PUT(HDRP(ptr), PACK(size + remainder, 1));
		PUT(FTRP(ptr), PACK(size + remainder, 1));
	}
	/* û�п������õ�����free�飬����size����ԭ���Ŀ飬��ʱֻ�������µĲ�������free�顢����ԭ�����ݡ��ͷ�ԭ�� */
	else
	{
		new_block = mm_malloc(size);
		memcpy(new_block, ptr, GET_SIZE(HDRP(ptr)));
		mm_free(ptr);
	}

	return new_block;
}














