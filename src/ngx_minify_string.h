typedef struct
{
    u_char *p;        //保存字符串首地址
    int reallength; //实际长度
} mystring;

void *minify_pcalloc(ngx_pool_t *pool, size_t size);
//原封不动初始化

void init(mystring *string);

//开辟长度，内存清零

void initwithlength(ngx_pool_t *pool, mystring *string, int length);

//初始化并拷贝字符串

void initwithstring(ngx_pool_t *pool, mystring *string, u_char *copystring);

//打印

void printfstring(mystring *string);

//增加字符

void backaddchar(mystring *string, u_char ch);

//增加字符串

void backaddstring(mystring *string, u_char *str);

//执行指令

void run(mystring *string);

//返回第一个找到的字符的地址

u_char *findfirstchar(mystring *string, u_char ch);

//返回第一个找到的字符串的地址

u_char *findfirststring(mystring *string, u_char *str);

//删除第一个找到的字符

int deletefirstchar(mystring *string, const u_char ch);

//删除第一个找到的字符串

int deletefirststring(mystring *string, u_char *const str);

//任意增加字符

void addchar(mystring *string, u_char ch, u_char *pos);

//任意增加字符串

void addstring(mystring *string, u_char *str, u_char *pos);

//改变字符

void changefirstchar(mystring *string, const u_char oldchar, const u_char newchar);

//改变字符串

void changefirststring(mystring *string, u_char *const oldstring, u_char *const newstring);