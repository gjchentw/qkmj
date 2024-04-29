/* Server */
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <string.h>

#define GPS_PORT 6666
#define MAX_PLAYER 300
#define ASK_MODE 1
#define CMD_MODE 2
#define DEFAULT_RECORD_FILE "qkmj.rec"
#define INDEX_FILE "qkmj.inx"
#define DEFAULT_MONEY 20000

char RECORD_FILE[30];
struct player_record {
  unsigned int id;
  char name[20];
  char password[15];
  long money;
  int level;
  int login_count;
  int game_count;
  long regist_time;
  long last_login_time;
  char last_login_from[60];
} record;
struct index_type {
  char name[20];
  unsigned int id;
} user_index;
struct index_type index_table[30000];
FILE *fp,*fp1;
long count;

int read_user_name(name)
char *name;
{
  struct player_record tmp_rec;

  if((fp=fopen(RECORD_FILE,"r+b"))==NULL)
  {
    printf("Cannot open file %s\n",RECORD_FILE);
    exit(1);
  }
  rewind(fp);
  while(!feof(fp) && fread(&tmp_rec,sizeof(tmp_rec),1,fp))
  {
    if(strcmp(name,tmp_rec.name)==0)
    {
      record=tmp_rec;
      fclose(fp);
      return 1;
    }
  }
  fclose(fp);
  return 0;
}

int read_user_id(id)
unsigned int id;
{
  if((fp=fopen(RECORD_FILE,"r+b"))==NULL)
    printf("Cannot open file!\n");
  rewind(fp);
  fseek(fp,sizeof(record)*id,0);
  fread(&record,sizeof(record),1,fp);
  fclose(fp);
}


char *read_index(id)
unsigned int id;
{
   if((fp1=fopen(INDEX_FILE,"r+b"))==NULL)
   {
     printf("Cannot open file!\n");
   }
  fseek(fp1,sizeof(user_index)*id,0);
  fread(&user_index,sizeof(user_index),1,fp1);
  fclose(fp1);
  return user_index.name;
}

create_index()
{
  struct index_type tmp_index;
  unsigned int index_num=0;
int i;

  if((fp=fopen(RECORD_FILE,"r+b"))==NULL)
  {
    printf("Cannot open file!");
    exit(1);
  }
/*
  if((fp1=fopen(INDEX_FILE,"w+b"))==NULL)
  {
    printf("Cannot open file!");
  }
*/
  rewind(fp);
  while(!feof(fp) && fread(&record,sizeof(record),1,fp))
  {
    if(record.name[0]==0)
    {
/*
      printf("id=%d name=%s",record.id,record.name);
      continue;
*/
    }
    index_table[record.id].id=record.id;
    strcpy(index_table[record.id].name,record.name);
/*
    strcpy(tmp_index.name,record.name);
    if(tmp_index.name[0]==0)
      continue;
    tmp_index.id=record.id;
    fwrite(&tmp_index,sizeof(tmp_index),1,fp1); 
*/
    index_num++;
  }  
  fclose(fp);
/*
  fclose(fp1);
*/
  /* Finished copying, now begin sorting */
  qsort(0,index_num-1);
  write_index(index_num);
/*
  fclose(fp1);
*/
}
  
write_index(num)
unsigned int num;
{
  if((fp1=fopen(INDEX_FILE,"w+b"))==NULL)
  {
    printf("Cannot open file!\n");
  }
/*
  fseek(fp1,sizeof(user_index)*i,0);
*/
  fwrite(&index_table[0],sizeof(user_index),num,fp1);
  fclose(fp1);
}

swap_index(i,j)
unsigned int i,j;
{
  struct index_type tmp_index;

/*
  read_index(i);
  tmp_index=user_index;
  read_index(j);
  write_index(i);
  user_index=tmp_index;
  write_index(j);
*/
  tmp_index=index_table[i];
  index_table[i]=index_table[j];
  index_table[j]=tmp_index;
}

qsort(left,right) 
int left,right;
{
  int i,j;
  struct index_type pivot,index1,index2;

/*
if(left%100==0)
printf("%d %d\n",left,right);
*/
  if(left<right)
  {
    /* Partition */
    pivot=index_table[left];
    i=left; j=right;
    while(i<j)
    {
/*
      read_index(i); index1=index;
*/
      while(i<right && strcmp(index_table[i].name,pivot.name)<=0)
      {
        i++;
/*
        read_index(i); index1=index;
*/
      }
/*
      read_index(j); index1=index;
*/
      while(j>left && strcmp(index_table[j].name,pivot.name)>0)
      {
        j--;
/*
        read_index(j); index1=index;
*/
      }
      if(i<j)
        swap_index(i,j);
    }
    swap_index(left,j);
    qsort(left,j-1);
    qsort(j+1,right);
  }
}

write_record()
{
  if((fp=fopen(RECORD_FILE,"r+b"))==NULL)
    printf("Cannot open file!");
  fseek(fp,sizeof(record)*record.id,0);
  fwrite(&record,sizeof(record),1,fp);
  fclose(fp);
}
  
print_record()
{
  char *ctime();
  struct player_record tmprec;
  char time1[40],time2[40];
  int player_num;
  int i;
  int id;
  char name[30];
  long money;
  int game;

  printf("(1) 以 id 查看特定使用者\n");
  printf("(2) 以名稱查看特定使用者\n");
  printf("(3) 查看所有使用者\n");
  printf("(4) 查看此金額以上的使用者\n");
  printf("(5) 查看此金額以下的使用者\n");
  printf("(6) 查看已玩此局數以上的使用者\n");
  printf("\n請輸入你的選擇:");
  scanf("%d",&i);
  switch(i)
  {
    case 1:
      printf("請輸入你要查看的 id:");
      scanf("%d",&id);
      if(id<0)
        return;
      break;
    case 2:
      printf("請輸入你要查看的名稱:");
gets(name);
      gets(name);
      break;
    case 3:
      break;
    case 4:
    case 5:
      printf("請輸入金額:");
      scanf("%D",&money);
      break;
    case 6:
      printf("請輸入局數:");
      scanf("%d",&game);
      break;
    default:
      return;
  }
  player_num=0;
  if((fp=fopen(RECORD_FILE,"rb"))==NULL)
    printf("Cannot open file!");
  while(!feof(fp) && fread(&tmprec,sizeof(tmprec),1,fp))
  {
    if(i==1)
    {
      if(id!=tmprec.id)
        continue;
    }
    if(i==2)
    {
      if(strcmp(name,tmprec.name)!=0 || name[0]==0)
        continue;
    }
    if(i==4)
    {
      if(tmprec.money<=money || tmprec.name[0]==0)
        continue;
    }
    if(i==5)
    {
      if(tmprec.money>=money || tmprec.name[0]==0)
        continue;
    }
    if(i==6)
    {
      if(tmprec.game_count<=game | tmprec.name[0]==0)
        continue;
    }
    printf("%d %10s %15s %ld %d %d %d  %s\n",tmprec.id,tmprec.name,
            tmprec.password, tmprec.money,tmprec.level,tmprec.login_count,
            tmprec.game_count,tmprec.last_login_from);
    strcpy(time1,ctime(&tmprec.regist_time));
    strcpy(time2,ctime(&tmprec.last_login_time));
    time1[strlen(time1)-1]=0;
    time2[strlen(time2)-1]=0;
    printf("              %s    %s\n",time1,time2);
    if(tmprec.name[0]!=0)
      player_num++;
  }
  printf("--------------------------------------------------------------\n");
  if(i==3)
    printf("共 %d 人註冊\n",player_num);
  fclose(fp);
}

modify_user()
{
  int i,id;
  char name[40];
  long money;

  printf("請輸入使用者代號:");
  scanf("%d",&id);
  if(id<0)
    return;
  read_user_id(id);
  printf("\n");
  printf("(1) 更改名稱\n");
  printf("(2) 重設密碼\n");
  printf("(3) 更改金額\n");
  printf("(4) 取消更改\n");
  printf("(5) 更改代號\n");
  printf("\n請輸入你的選擇:");
  scanf("%d",&i);
  printf("\n");
  switch(i)
  {
    case 1:
      printf("請輸入要更改的名稱:");
      gets(name);
   gets(name);
      strcpy(record.name,name);
      printf("改名為 %s\n",name);
      break;
    case 2:
      record.password[0]=0;
      printf("密碼已重設!\n");
      break;
    case 3:
      printf("請輸入要更改的金額:");
      scanf("%D",&money);
      record.money=money;
      printf("金額更改為 %ld\n",money);
      break;
    case 4:
      printf("Enter id:");
      scanf("%d",&i);
      if((fp=fopen(RECORD_FILE,"r+b"))==NULL)
        exit(1);
      fseek(fp,sizeof(record)*2,0);
      record.id=2;
      fwrite(&record,sizeof(record),1,fp);
      fclose(fp);
      return;
    default:
      return;
  }
  write_record();
}

main(argc,argv)
int argc;
char *argv[];
{
  int i,id;
  char name[20];
  struct stat status;

  if(argc<2)
  {
    strcpy(RECORD_FILE,DEFAULT_RECORD_FILE);
  }
  else
  {
    strcpy(RECORD_FILE,argv[1]);  
  }
  do
  {
    printf("\n");
    printf("(1) 列出所有使用者資料\n");
    printf("(2) 刪除使用者\n");
    printf("(3) 更改使用者資料\n");
    printf("(4) 建立 Index 檔\n");
    printf("(5) 離開\n");
    printf("請輸入你的選擇:");
    scanf("%d",&i);
    switch(i)
    {
      case 1:
        print_record();
        break;
      case 2:
        printf("請輸入使用者代號:");
        scanf("%d",&id);
        if(id<0)
          break;
        read_user_id(id);
        record.name[0]=0;
        write_record();
        break;
      case 3:
        modify_user();
        break;
      case 4:
        create_index();
        break;
      case 5:
        break;
    }
  } while(i!=5); 
}

