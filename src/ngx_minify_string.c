#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include "ngx_minify_string.h"

static void *ngx_palloc_block(ngx_pool_t *pool, size_t size)
{
    u_char *m;
    size_t psize;
    ngx_pool_t *p, *new;

    psize = (size_t)(pool->d.end - (u_char *)pool);

    m = ngx_memalign(NGX_POOL_ALIGNMENT, psize, pool->log);
    if (m == NULL)
    {
        return NULL;
    }

    new = (ngx_pool_t *)m;

    new->d.end = m + psize;
    new->d.next = NULL;
    new->d.failed = 0;

    m += sizeof(ngx_pool_data_t);
    m = ngx_align_ptr(m, NGX_ALIGNMENT);
    new->d.last = m + size;

    for (p = pool->current; p->d.next; p = p->d.next)
    {
        if (p->d.failed++ > 4)
        {
            pool->current = p->d.next;
        }
    }

    p->d.next = new;

    return m;
}

static ngx_inline void *ngx_palloc_small(ngx_pool_t *pool, size_t size, ngx_uint_t align)
{
    u_char *m;
    ngx_pool_t *p;

    p = pool->current;

    do
    {
        m = p->d.last;

        if (align)
        {
            m = ngx_align_ptr(m, NGX_ALIGNMENT);
        }

        if ((size_t)(p->d.end - m) >= size)
        {
            p->d.last = m + size;

            return m;
        }

        p = p->d.next;

    } while (p);

    return ngx_palloc_block(pool, size);
}

static void *minify_palloc_large(ngx_pool_t *pool, size_t size)
{
    void *p;
    ngx_uint_t n;
    ngx_pool_large_t *large;

    p = ngx_alloc(size, pool->log);
    if (p == NULL)
    {
        return NULL;
    }

    n = 0;

    for (large = pool->large; large; large = large->next)
    {
        if (large->alloc == NULL)
        {
            large->alloc = p;
            return p;
        }

        if (n++ > 3)
        {
            break;
        }
    }

    large = ngx_palloc_small(pool, sizeof(ngx_pool_large_t), 1);
    if (large == NULL)
    {
        ngx_free(p);
        return NULL;
    }

    large->alloc = p;
    large->next = pool->large;
    pool->large = large;

    return p;
}

void *minify_pcalloc(ngx_pool_t *pool, size_t size)
{
    void *p = minify_palloc_large(pool, size);
    if (p)
    {
        ngx_memzero(p, size);
    }
    return p;
}

/* 求字符串的长度 */
int mystrlen(u_char *p)
{
    if (p == NULL)
    {
        return -1; //失败，
    }
    int length = 0;
    while (*p != '\0') //字符串终止条件
    {
        length++; //长度自增
        p++;      //指针不断向前
    }
    return length;
}

/*字符串拷贝 */
u_char *mystrcpy(u_char *dest, const u_char *source) // const限定不被意外修改
{
    if (dest == NULL || source == NULL)
    {
        return NULL; //为空没有必要干活了
    }
    u_char *destbak = dest;
    while (*source != '\0') //一直拷贝
    {
        *dest = *source; //赋值字符
        source++;
        dest++; //指针不断向前，字符挨个赋值
    }
    *dest = '\0';   //结尾
    return destbak; //返回地址
}

/*字符串拼接*/
u_char *mystrcat(u_char *dest, const u_char *source)
{
    if (dest == NULL || source == NULL)
    {
        return NULL; //失败
    }
    else
    {
        u_char *destbak = dest; //保留地址
        while (*dest != '\0')
        {
            dest++; //指针向前移动
        }
        //从尾部开始拷贝
        while (*source != '\0') //循环被被拷贝的字符串
        {
            *dest = *source; //字符串赋值
            dest++;
            source++;
        }
        *dest = '\0'; //结尾
        return destbak;
    }
}

u_char *mystrchr(u_char *dest, u_char ch)
{
    if (dest == NULL)
    {
        return NULL;
    }
    while (*dest != '\0')
    {
        if (*dest == ch)
        {
            return dest; //找到返回地址
        }
        dest++;
    }
    return NULL; //返回
}

u_char *mystrstr(u_char *dest, u_char *findstr)
{
    if (dest == NULL || findstr == NULL)
    {
        return NULL;
    }
    u_char *destbak = dest;
    u_char *p = NULL; //保存找到的地址
    while (*destbak != '\0')
    {
        int flag = 1; //假定是相等
        u_char *findstrbak = findstr;
        u_char *nowdestbak = destbak;
        while (*findstrbak != '\0')
        {
            if (*nowdestbak != '\0')
            {
                if (*findstrbak != *nowdestbak) //有一个不等
                {
                    flag = 0; //赋值为0代表不等
                }
                nowdestbak++;
                findstrbak++;
            }
            else
            {
                flag = 0; //设置标识
                break;
            }
        }
        if (flag == 1)
        {
            p = destbak; //当前位置
            return p;
        }
        destbak++;
    }
    return NULL;
}

void init(mystring *string)
{
    string->p = NULL;
    string->reallength = 0; //初始化结构体字符串
}

void initwithlength(ngx_pool_t *pool, mystring *string, int length)
{
    // string->p =(char *) malloc(sizeof(char)*length);//分配内存
    string->p = (u_char *)minify_pcalloc(pool, length * sizeof(u_char)); //分配内存并清零
    string->reallength = length;                                         //长度
}

void initwithstring(ngx_pool_t *pool, mystring *string, u_char *copystring)
{
    int length = strlen((char *)copystring) + 1;
    //获取字符串长度
    string->p = (u_char *)minify_pcalloc(pool, length * sizeof(u_char)); //分配内存
    mystrcpy(string->p, copystring);                                     //拷贝字符串
    string->reallength = length;                                         //设置长度
}

void backaddchar(mystring *string, u_char ch)
{
    if (mystrlen(string->p) + 1 == string->reallength) //意味着满了
    {
        //重新分配内存
        string->p = realloc(string->p, string->reallength + 1);
        string->reallength += 1;
        string->p[string->reallength - 2] = ch;
        string->p[string->reallength - 1] = '\0';
    }
    else
    {
        int nowlength = mystrlen(string->p); //求出当前长度
        string->p[nowlength] = ch;
        string->p[nowlength + 1] = '\0'; //字符的增加
    }
}

void backaddstring(mystring *string, u_char *str)
{
    int nowmystringlength = mystrlen(string->p);                      //获取当前长度
    int addstringlength = mystrlen(str);                              //要增加的长度
    if (nowmystringlength + addstringlength + 1 > string->reallength) //判定是否越界
    {
        int needaddlength = nowmystringlength + addstringlength + 1 - (string->reallength);
        // printf("%d",needaddlength);
        string->p = (u_char *)realloc(string->p, string->reallength + needaddlength); //增加字符串长度
        mystrcat(string->p, str);                                                     //拷贝字符串
        string->reallength += needaddlength;                                          //增加长度
    }
    else
    {
        mystrcat(string->p, str); //拷贝字符串
    }
}

void printfstring(mystring *string)
{
    printf("\nstring=%s", string->p); //打印字符串
}

void run(mystring *string)
{
    system(string->p); //执行指令
}

u_char *findfirstchar(mystring *string, chu_charar ch)
{
    u_char *p = mystrchr(string->p, ch); //查找
    return p;
}

u_char *findfirststring(mystring *string, u_char *str)
{
    u_char *pres = mystrstr(string->p, str);
    return pres; //返回地址
}

int deletefirstchar(mystring *string, const u_char ch)
{
    u_char *p = mystrchr(string->p, ch); //查找
    if (p == NULL)
    {
        return 0;
    }
    else
    {
        u_char *pnext = p + 1;
        while (*pnext != '\0')
        {
            *p = *pnext; //删除一个字符整体向前移动
            p++;
            pnext++;
        }
        *p = '\0'; //字符串要有结尾

        return 1;
    }
}

int deletefirststring(mystring *string, u_char *const str)

{

    u_char *pres = mystrstr(string->p, str); //查找

    if (pres == NULL)

    {

        return 0;
    }

    else

    {

        int length = mystrlen(str); //求字符串长度

        u_char *pnext = pres + length; //下一个字符

        while (*pnext != '\0')

        {

            *pres = *pnext; //删除一个字符整体向前移动

            pres++;

            pnext++;
        }

        *pres = '\0'; //字符串要有结尾

        return 1;
    }
}

void addchar(mystring *string, u_char ch, u_char *pos)

{

    if (pos == NULL || string == NULL)

    {

        return;
    }

    if (mystrlen(string->p) + 1 == string->reallength) //意味着满了

    {

        //重新分配内存

        string->p = realloc(string->p, string->reallength + 1);

        string->reallength += 1;

        int nowlength = mystrlen(string->p); //求出当前长度

        int movelength = mystrlen(pos); //求出现在要移动的长度

        for (int i = nowlength; i > nowlength - movelength; i--) //移动

        {

            string->p[i] = string->p[i - 1]; //轮询
        }

        string->p[nowlength - movelength] = ch; //插入

        string->p[nowlength + 1] = '\0'; //结尾
    }

    else

    {

        int nowlength = mystrlen(string->p); //求出当前长度

        int movelength = mystrlen(pos); //求出现在要移动的长度

        for (int i = nowlength; i > nowlength - movelength; i--) //移动

        {

            string->p[i] = string->p[i - 1]; //轮询
        }

        string->p[nowlength - movelength] = ch; //插入

        string->p[nowlength + 1] = '\0'; //结尾
    }
}

void addstring(mystring *string, u_char *str, u_char *pos) //任意增加字符串

{

    if (pos == NULL || string == NULL)

    {

        return;
    }

    int nowmystringlength = mystrlen(string->p); //获取当前长度

    int addstringlength = mystrlen(str); //要增加的长度

    if (nowmystringlength + addstringlength + 1 > string->reallength) //判定是否越界

    {

        int needaddlength = nowmystringlength + addstringlength + 1 - (string->reallength);

        // printf("%d",needaddlength);

        string->p = (u_char *)realloc(string->p, string->reallength + needaddlength); //增加字符串长度

        string->reallength += needaddlength; //增加长度

        //先移动，再拷贝

        int nowlength = mystrlen(string->p); //求出当前长度

        int movelength = mystrlen(pos); //求出现在要移动的长度

        int insertlength = strlen(str); //要求出插入的长度

        for (int i = nowlength; i >= nowlength - movelength; i--)

        {

            string->p[i + insertlength] = string->p[i]; //字符移动
        }

        for (int j = 0; j < insertlength; j++)

        {

            string->p[nowlength - movelength + j] = str[j]; //赋值拷贝
        }
    }

    else
    {
        int nowlength = mystrlen(string->p); //求出当前长度
        int movelength = mystrlen(pos);      //求出现在要移动的长度
        int insertlength = strlen(str);      //要求出插入的长度
        for (int i = nowlength; i >= nowlength - movelength; i--)
        {
            string->p[i + insertlength] = string->p[i]; //字符移动
        }
        for (int j = 0; j < insertlength; j++)
        {
            string->p[nowlength - movelength + j] = str[j]; //赋值拷贝
        }
    }
}

void changefirstchar(mystring *string, const u_char oldchar, const newchar) //改变字符
{

    u_char *pstr = string->p;

    while (*pstr != '\0')
    {
        if (*pstr == oldchar) //查找
        {
            *pstr = newchar; //赋值
            return;
        }
        pstr++;
    }
}

void changefirststring(mystring *string, u_char *const oldstring, u_char *const newstring) //改变字符串
{
    u_char *pfind = findfirststring(string, oldstring); //找到位置
    if (pfind != NULL)
    {
        deletefirststring(string, oldstring); //删除
        addstring(string, newstring, pfind);  //插入
    }
}