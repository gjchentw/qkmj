/* Server */
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <string.h>
#include <netdb.h>
#include <sys/errno.h>
#include <termio.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/file.h>
#include <pwd.h>
#include <ctype.h>
#include <math.h>
/*
#include <malloc.h>
*/

#define DEFAULT_GPS_PORT 7001
#define DEFAULT_GPS_IP "140.113.209.32"
#define MAX_PLAYER 300
#define ASK_MODE 1
#define CMD_MODE 2
#define RECORD_FILE "qkmj.rec"
#define INDEX_FILE "qkmj.inx"
#define TMP_FILE "qkmj.tmp"
#define NEWS_FILE "news.txt"
#define BADUSER_FILE "baduser.txt"
#define LOG_FILE "qkmj.log"
#define DEFAULT_MONEY 20000
#define ADMIN_PASSWORD "qkmj"

char *lookup();
char *genpasswd();
int checkpasswd();

/* Global variables */
char admin_passwd[20];
int log_flag[10];
long n1=0;
int timeup=0;
extern int errno;
extern char *sys_errlist[];
struct fd_set rfds,afds;
int login_limit;
char gps_ip[20];
int gps_port;
int log_level;
char number_map[20][5]
={"０","１","２","３","４","５","６","７","８","９"};
int gps_sockfd;
char climark[30];
struct player_info {
  int sockfd;
  int login;
  int admin; 
  unsigned int id;
  char name[25];
  char username[40];
  long money;
  int serv;
  int join;
  int type;
  char note[256];
  int input_mode;
  int prev_request;
  struct sockaddr_in addr;
  int port;
  char version[10];
  char challenge[20];
} player[MAX_PLAYER];
struct player_record {
  unsigned int id;
  char name[20];
  char password[15];
  long money;
  int level;
  unsigned int login_count;
  unsigned int game_count;
  long regist_time;
  long last_login_time;
  char last_login_from[60];
} record;
struct index_type {
  char name[20];
  unsigned int id;
} user_index;
struct index_type index_table[50000];
unsigned int index_num;
FILE *rec_fp,*inx_fp,*tmp_fp,*log_fp,*news_fp,*baduser_fp;
struct ask_mode_info {
  int question;
  int answer_ok;
  char *answer;
} ask;
struct friend_type {
  int num;
  char name[50][10];
} friend[MAX_PLAYER];
struct rlimit fd_limit;
int nfds;
char *crypt();
int authentication=0;
char auth_key[10]={0,0,0,0,0,0,0,0};

generate_random(range)
int range;
{
  double rand_num;
  double drand48();
  int rand_int;

  rand_num=drand48();
  rand_int=(int) (range*rand_num);
  return(rand_int);
}

client_auth(player_id)   /* Client authentication */
int player_id;
{
  int i;
  char msg_buf[80];

  if(strcmp(player[player_id].version,"095p3")<0 || 
     player[player_id].version[0]==0)
  {
    write_msg(player[player_id].sockfd,
              "101□□□ 請使用 QKMJ Ver 0.95p3 以上版本上線 □□□");
    write_msg(player[player_id].sockfd,
              "101請至 ftp.csie.nctu.edu.tw/pub/CSIE/contrib/qkmj 下取得新版");
    write_msg(player[player_id].sockfd,"010");
    return 0;
  }
  strcpy(msg_buf,"007");
  for(i=0;i<8;i++)
    player[player_id].challenge[i]=generate_random(254)+1;  
  player[player_id].challenge[i]=0;
  strcat(msg_buf,player[player_id].challenge);
  write_msg(player[player_id].sockfd,msg_buf);
  return 1;
}


make_key()
{
  int i;
  char *tmp_str;
  char msg_buf[80];

  strcpy(msg_buf,gps_ip);
  tmp_str=strtok(msg_buf,".");
  auth_key[0]=(auth_key[0] ^ atoi(tmp_str));
  for(i=1;i<4;i++)
  {
    tmp_str=strtok('\0',".");
    auth_key[i]=(auth_key[i] ^ atoi(tmp_str));
  }
  auth_key[4]=(auth_key[4] ^ (gps_port%256));
  auth_key[5]=(auth_key[5] ^ (gps_port/256));
}

sign(challenge,response)
char challenge[],response[];
{
  int i,j,len;
  char msg_buf[80];

  len=strlen(challenge);
  for(i=0;i<len;i++)
  {
    msg_buf[i]=(auth_key[i] ^ challenge[i]);
    if(msg_buf[i]==0)    /* prevent 0 from cutting off the string */
      msg_buf[i]=255;
  }  
  msg_buf[i]=0;
  strcpy(response, crypt(msg_buf,"aa")+2);
  response[8]=0;
}

err(errmsg,level)
char *errmsg;
int level;
{
  if(!log_flag[level])
    return;
  fclose(log_fp);
  log_fp=fopen(LOG_FILE,"a+");
  if(log_fp==NULL)
  {
    return -1;
  }
  if(log_flag[level]==2)
    printf("%s",errmsg);
  fprintf(log_fp,"%s",errmsg);
  fflush(log_fp);
}

int read_msg(fd,msg)
int fd;
char *msg;
{
  int n;
  char msg_buf[512];
  int read_code;

  n=0;
  if(Check_for_data(fd)==0)
  {
    return 2;
  }
  timeup=0;
  alarm(3);
  do
  {
    recheck:;
    read_code=read(fd,msg,1);
    if(read_code==-1)
    {
      if(errno!=EWOULDBLOCK)
      {
        alarm(0);
        return 0;
      }
      else if(timeup)
      {
        alarm(0);
        err("TIME UP!\n",1);
        return 0;
      }
      else
        goto recheck;
    }
    else if(read_code==0)
    {
      alarm(0);
      return 0;
    }
    else
    {
      n++;
    }
    if(n>300)
    {
      alarm(0);
      return 0;
    }
  } while(*msg++ != '\0');
  alarm(0);
  return 1;
}

write_msg(fd,msg)
int fd;
char *msg;
{
  int n;
  n=strlen(msg);
  if(write(fd,msg,n+1)<0)
    return -1;
}

display_msg(player_id,msg)
int player_id;
char *msg;
{
  char msg_buf[256];
    
  sprintf(msg_buf,"101%s",msg);
  write_msg(player[player_id].sockfd,msg_buf);
}

int Check_for_data (fd)
     int fd;
/* Checks the socket descriptor fd to see if any incoming data has
   arrived.  If yes, then returns 1.  If no, then returns 0.
   If an error, returns -1 and stores the error message in socket_error.
*/
{
  int status;                 /* return code from Select call. */
  struct fd_set wait_set;     /* A set representing the connections that
				 have been established. */
  struct timeval tm;          /* A timelimit of zero for polling for new
				 connections. */

  FD_ZERO (&wait_set);
  FD_SET (fd, &wait_set);

  tm.tv_sec = 0;
  tm.tv_usec = 0;
  status = select (FD_SETSIZE, &wait_set, (fd_set *) 0, (fd_set *) 0, &tm);

/*  if (status < 0)
    sprintf (socket_error, "Error in select: %s", sys_errlist[errno]); */

  return (status);
}

int convert_msg_id(player_id,msg)
int player_id;
char *msg;
{
   int i;
   char msg_buf[256];

if(strlen(msg)<3)
{
  sprintf(msg_buf,"Error msg: %s",msg);
  err(msg_buf,1);
  return 0;
}
   for(i=0;i<3;i++)
     if(msg[i]<'0' || msg[i]>'9')
     {
       sprintf(msg_buf,"%d",msg[i]);
       err(msg_buf,1);
     }
   return(msg[0]-'0')*100+(msg[1]-'0')*10+(msg[2]-'0');
}

list_player(fd)
int fd;
{
  int i;
  char msg_buf[256];
  int total_num=0;

  write_msg(fd, "101-------------    目前上線使用者    ---------------");
  strcpy(msg_buf,"101");
  for(i=1;i<MAX_PLAYER;i++)
  {
    if(player[i].login==2)
    {
      total_num++;
      if((strlen(msg_buf)+strlen(player[i].name))>50)
      {
        write_msg(fd,msg_buf);
        strcpy(msg_buf,"101");
      }
      strcat(msg_buf,player[i].name);
      strcat(msg_buf,"  ");
    }
  }
  if(strlen(msg_buf)>4)
    write_msg(fd,msg_buf);
  write_msg(fd, "101--------------------------------------------------");
  sprintf(msg_buf,"101共 %d 人",total_num);
  write_msg(fd,msg_buf);
}  

list_table(fd,mode)
int fd;
int mode;
{
  int i;
  char msg_buf[256];
  int total_num=0;

  write_msg(fd,"101桌長       人數  附註");
  write_msg(fd, "101--------------------------------------------------");
  for(i=1;i<MAX_PLAYER;i++)
  {
    if(player[i].login && player[i].serv>0)
    {
      if(mode==2 && player[i].serv>=4)
        continue;
      total_num++;
      sprintf(msg_buf,"101%-10s %3d   %s"
              ,player[i].name,player[i].serv,player[i].note);
      write_msg(fd,msg_buf);
    }
  }
  write_msg(fd, "101--------------------------------------------------");
  sprintf(msg_buf,"101共 %d 桌",total_num);
  write_msg(fd,msg_buf);
}

list_stat(player_id,name)
int player_id;
char *name;
{
  char msg_buf[256];
  char msg_buf1[256];
  char order_buf[30];
  int i;
  int total_num;
  int order;
  struct player_record tmp_rec;

  total_num=0;
  order=1;
  if(!read_user_name(name))
  {
    sprintf(msg_buf,"101*** 找不到 %s 這個人",name);
    write_msg(player[player_id].sockfd,msg_buf);
    return;
  }
  sprintf(msg_buf,"101◇名稱:%s  %s", record.name,record.last_login_from);
  sprintf(msg_buf1,"101◇金額:%ld 上線次數:%d 已玩局數:%d",
          record.money,record.login_count,record.game_count);
  write_msg(player[player_id].sockfd,msg_buf);
  write_msg(player[player_id].sockfd,msg_buf1);
}
  
who(player_id,name)
int player_id;
char *name;
{
  char msg_buf[256];
  int i;
  int serv_id;

  if(name[0]==0)
  {
    serv_id=player[player_id].serv;
  }
  else
  {
    for(i=1;i<MAX_PLAYER;i++)
      if(player[i].login && player[i].serv)
        if(strcmp(player[i].name,name)==0)
        {
          serv_id=i;
          goto found_serv;
        }
    write_msg(player[player_id].sockfd,"101*** 找不到此桌");
    return;
  }
  found_serv:;
  sprintf(msg_buf,"101%s  ",player[serv_id].name);
  write_msg(player[player_id].sockfd,"101----------------   此桌使用者   ------------------");
  for(i=1;i<MAX_PLAYER;i++)
    if(player[i].join==serv_id)
    {
      if(strlen(msg_buf)+strlen(player[i].name)>53)
      {
        write_msg(player[player_id].sockfd,msg_buf);
        strcpy(msg_buf,"101");
      }
      strcat(msg_buf,player[i].name);
      strcat(msg_buf,"  ");
    }
  if(strlen(msg_buf)>4)
    write_msg(player[player_id].sockfd,msg_buf);
  write_msg(player[player_id].sockfd,"101--------------------------------------------------");
}

lurker(fd)
int fd;
{
  int i,total_num=0;
  char msg_buf[256];

  strcpy(msg_buf,"101");
  write_msg(fd,"101-------------   目前閒置之使用者   ---------------");
  for(i=1;i<MAX_PLAYER;i++)
    if(player[i].login==2 && (player[i].join==0 && player[i].serv==0))
    {
      total_num++;
      if((strlen(msg_buf)+strlen(player[i].name))>53)
      {
        write_msg(fd,msg_buf);
        strcpy(msg_buf,"101");
      }
      strcat(msg_buf,player[i].name);
      strcat(msg_buf,"  ");
    }
    if(strlen(msg_buf)>4)
      write_msg(fd,msg_buf);
  write_msg(fd,"101--------------------------------------------------");
  sprintf(msg_buf,"101共 %d 人",total_num);
  write_msg(fd,msg_buf);
}

list_friend(player_id)
int player_id;
{
  int i,total_num,fd,id;
  char msg_buf[255];

  fd=player[player_id].sockfd;
  total_num=0;
  write_msg(fd,"101-------------   目前在線上的牌友   ---------------");
  strcpy(msg_buf,"101");
  for(i=0;i<friend[player_id].num;i++)
  {
    id=find_user_name(friend[player_id].name[i]);
    if(id<=0)
      continue;
    if(player[id].login==2)
    {
      total_num++;
      if((strlen(msg_buf)+strlen(friend[player_id].name[i]))>53)
      {
        write_msg(fd,msg_buf);
        strcpy(msg_buf,"101");
      }
      strcat(msg_buf,friend[player_id].name[i]);
      strcat(msg_buf,"  ");
    }
  }
  if(strlen(msg_buf)>4)
    write_msg(fd,msg_buf);
  write_msg(fd,"101--------------------------------------------------");
  sprintf(msg_buf,"101共 %d 人",total_num);
  write_msg(fd,msg_buf);
}

add_friend(player_id,name)
int player_id;
char *name;
{
  if(friend[player_id].num>49)
    return;
  strcpy(friend[player_id].name[friend[player_id].num++],name);
}

find_user(fd,name)
int fd;
char *name;
{
  int i;
  char msg_buf[256];
  int id;
  char *ctime();
  char last_login_time[256];

  if(!read_user_name(name))
  {
    sprintf(msg_buf,"101*** 沒有 %s 這個人",name);
    write_msg(fd,msg_buf);
    return;
  }
  strcpy(last_login_time,ctime(&record.last_login_time));
  last_login_time[strlen(last_login_time)-1]=0;
  id=find_user_name(name);
  if(id>0)
  {
    if(player[id].login==2)
    {
      if(player[id].join==0 && player[id].serv==0)
      {
        sprintf(msg_buf,"101◇%s 閒置中",name);
        write_msg(fd,msg_buf); 
      }
      if(player[id].join)
      {
        sprintf(msg_buf,"101◇%s 在 %s 桌內",name,player[player[id].join].name); 
        write_msg(fd,msg_buf);
      }
      if(player[id].serv)
      {
        sprintf(msg_buf,"101◇%s 在 %s 桌內",name,player[id].name);
        write_msg(fd,msg_buf);
      }
      sprintf(msg_buf,"101◇進入系統時間: %s",last_login_time);
      write_msg(fd,msg_buf);
      return;
    }
  }
  else
  {
    sprintf(msg_buf,"101◇%s 目前不在線上",name);
    write_msg(fd,msg_buf);  
    sprintf(msg_buf,"101◇上次連線時間: %s",last_login_time);
    write_msg(fd,msg_buf);
  }
}

broadcast(player_id,msg)
int player_id;
char *msg;
{
  int i;
  char msg_buf[256];

  sprintf(msg_buf,"101%%%s%% %s",player[player_id].name,msg);
  for(i=1;i<MAX_PLAYER;i++)
    if(player[i].login==2)
      write_msg(player[i].sockfd,msg_buf);
}

send_msg(player_id,msg)
int player_id;
char *msg;
{
  char *str1,*str2; 
  int i;
  char msg_buf[256];
  
  str1=strtok(msg," ");
  str2=msg+strlen(str1)+1;
  if(!read_user_name(str1))
  {
    sprintf(msg_buf,"101*** 找不到 %s 這個人",str1);
    write_msg(player[player_id].sockfd,msg_buf);
    return;
  }
  for(i=1;i<MAX_PLAYER;i++)
    if(player[i].login==2 && strcmp(player[i].name,str1)==0)
    {
      sprintf(msg_buf,"101*%s* %s",player[player_id].name,str2);
      write_msg(player[i].sockfd,msg_buf);
      return;
    }
   sprintf(msg_buf,"101*** %s 目前不在線上",str1);
   write_msg(player[player_id].sockfd,msg_buf);
}

invite(player_id,name)
int player_id;
char *name;
{
  int i;
  char msg_buf[256];

  if(!read_user_name(name))
  {
    sprintf(msg_buf,"101*** 找不到 %s 這個人",name);
    write_msg(player[player_id].sockfd,msg_buf);
    return;
  }
  for(i=1;i<MAX_PLAYER;i++)
    if(player[i].login==2 && strcmp(player[i].name,name)==0)
    {
      sprintf(msg_buf,"101*** %s 邀請你加入 %s",player[player_id].name,
              (player[player_id].join==0) ? 
              player[player_id].name : player[player[player_id].join].name);
      write_msg(player[i].sockfd,msg_buf);
      return;
    }
  sprintf(msg_buf,"101*** %s 目前不在線上",name);
  write_msg(player[player_id].sockfd,msg_buf);
}

init_socket()
{
  struct sockaddr_in serv_addr;
  int on=1;

  printf("Starting QKMJ server on %s %d......\n",gps_ip,gps_port);
  /* open a TCP socket for internet stream socket */
  if( (gps_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    err("Server: cannot open stream socket",1);
  /* bind our local address */
  bzero((char *)&serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(gps_port);
  setsockopt(gps_sockfd,SOL_SOCKET,SO_REUSEADDR,(char *) &on,sizeof(on));
  if( bind(gps_sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) <0)
  {
    printf("server: cannot bind local address\n");
    exit(1);
  }
  listen(gps_sockfd, 10);
  printf("Listen for client...\n");
}

char *lookup(cli_addrp)
struct sockaddr_in *cli_addrp;
{
  struct hostent *hp;
  char *hostname;

  hostname=inet_ntoa(cli_addrp->sin_addr);
  hp=gethostbyaddr((char *) &cli_addrp->sin_addr,
                   sizeof(struct in_addr),cli_addrp->sin_family);
  if(hp)
    hostname=hp->h_name;
  return hostname;
}

int refresh_index()
{
  struct stat status;

  fclose(inx_fp);
  inx_fp=fopen(INDEX_FILE,"r+b");
  if(inx_fp!=NULL)
  {
    stat(INDEX_FILE,&status);
    index_num=status.st_size/sizeof(user_index);
    rewind(inx_fp);
    fread(&index_table[0],sizeof(user_index),index_num,inx_fp);
  }
  else
  {
    index_num=0;
    err("Can't open index file!\n",2);
  }
  fflush(inx_fp);
}

int search_index(name)
char *name;
{
  int left,right,mid;

  left=0;
  right=index_num-1;
  while(left<=right)
  {
    mid=(left+right)/2;
    if(strcmp(name,index_table[mid].name)==0)
    {
      read_user_id(index_table[mid].id);
      if(strcmp(record.name,name)!=0)
        return 2;  /* pointing wrong */
      else
        return 0;  /* found */
    }
    else if(strcmp(name,index_table[mid].name)<0)
      right=mid-1;
    else
      left=mid+1;
  }
  return 1;  /* No such name */
}

int sequencial_read(name)
char *name;
{
  struct player_record tmp_rec;

  fclose(rec_fp);
  rec_fp=fopen(RECORD_FILE,"r+b");
  if(rec_fp!=NULL)
  {
    rewind(rec_fp);
    while(!feof(rec_fp) && fread(&tmp_rec,sizeof(tmp_rec),1,rec_fp))
    {
      if(strcmp(name,tmp_rec.name)==0)
      {
        record=tmp_rec;
        return 1;
      }
    }
  }
  return 0;
}

int read_user_name(name)
char *name;
{
  struct player_record tmp_rec;
  char msg_buf[256];
  int use_index=0;

  if(inx_fp==NULL || strlen(name)==0)
  {
    if(sequencial_read(name))
      return 1;
    else
      return 0;
  }
  use_index=search_index(name);
  if(use_index==0)
    return 1;
  if(use_index==1)  /* No such name in index file */
  {
    if(sequencial_read(name))
    {
      sprintf(msg_buf,"Refresh index for %s\n",name);
      err(msg_buf,2);
      refresh_index();
      return 1;
    }
    else
      return 0;
  }
  if(use_index==2)  /* Index file pointing wrong */
  {
/*
    sprintf(msg_buf,"Index pointing wrong for %s\n",name);
    err(msg_buf,2);
*/
    if(sequencial_read(name))
      return 1;
    else
      return 0;
  }
}

int read_user_id(id)
unsigned int id;
{
  char msg_buf[256];

  fclose(rec_fp);
  rec_fp=fopen(RECORD_FILE,"r+b");
  if(rec_fp==NULL)
  {
    sprintf(msg_buf,"(read_user_id) Cannot open file!\n");
    err(msg_buf,1);
    return;
  }
  rewind(rec_fp);
  fseek(rec_fp,sizeof(record)*id,0);
  fread(&record,sizeof(record),1,rec_fp);
  fflush(rec_fp);
}

add_user(player_id,name,passwd)
int player_id;
char *name;
char *passwd;
{
  long time();
  struct stat status;
  unsigned int index_count, total_index,i;
  char msg_buf[80];

  if(rec_fp)
  {
    stat(RECORD_FILE,&status);
  }
  else
    status.st_size=0;
  if((i=read_user_name(""))==0)
    record.id=status.st_size/sizeof(record);
  strcpy(record.name,name);
  strcpy(record.password,genpasswd(passwd));
  record.money=DEFAULT_MONEY;
  record.level=0;
  record.login_count=1;
  record.game_count=0;
  time(&record.regist_time);
  record.last_login_time=record.regist_time;
  record.last_login_from[0]=0;
  if(player[player_id].username[0]!=0)
  {
    sprintf(record.last_login_from,"%s@",player[player_id].username);
  }
  strcat(record.last_login_from,lookup(&player[player_id].addr));
  if(check_user(player_id))
  {
    write_record();
    /* Should lock now! */
    refresh_index();
    /* First check if there's already such a name in the index */
    fclose(inx_fp);
    inx_fp=fopen(INDEX_FILE,"r+b");
    if(inx_fp!=NULL)
    {
      rewind(inx_fp);
      if(index_num==0)  /* index file is empty now */
      {
        index_num++;
        strcpy(index_table[0].name,name);
        index_table[0].id=record.id;
        fwrite(&index_table[0],sizeof(user_index),1,inx_fp);
        fflush(inx_fp);
        err("New index file\n",2);
        return 1;
      }
      index_count=0;
      while(1)
      {
        /* The order of the name is greater than the current index? */
        if(strcmp(name,index_table[index_count].name)>0)
        {
          index_count++; 
          /* No more indices? */
          if(index_count==index_num)
          {
            index_num++;
            strcpy(index_table[index_count].name,name);
            index_table[index_count].id=record.id;
            fseek(inx_fp,sizeof(user_index)*index_count,0);
            fwrite(&index_table[index_count],sizeof(user_index),1,inx_fp);
            fflush(inx_fp);
            return 1;
          }
        }
        else if(strcmp(name,index_table[index_count].name)<0)
        {
          if(index_count==0)
          {
            sprintf(msg_buf,"First index for %s < %s\n",name,
                    index_table[index_count].name);
            err(msg_buf,2);
          }
          for(i=index_num;i>index_count;i--)
          {
            index_table[i]=index_table[i-1];
          }
          strcpy(index_table[index_count].name,name);
          index_table[index_count].id=record.id;
          fseek(inx_fp,sizeof(user_index)*index_count,0);
          fwrite(&index_table[index_count],sizeof(user_index),
                  (index_num-index_count+1),inx_fp);
          fflush(inx_fp);
          index_num++;
          return 1;
        }
        /* If this is the name, just replace it */
        else  
        {
          index_table[index_count].id=record.id;
          fseek(inx_fp,sizeof(user_index)*index_count,0);
          fwrite(&index_table[index_count],sizeof(user_index),1,inx_fp);
          fflush(inx_fp);
          return 1;
        }
      }  
    }    
    return 1;
  }
  else
    return 0;
}

check_user(player_id)
int player_id;
{
  char msg_buf[256];
  char msg_buf1[256];
  char from[256];
  char email[256];

  fclose(baduser_fp);
  baduser_fp=fopen(BADUSER_FILE,"r+");
  if(baduser_fp==NULL)
  {
    return 1;
  }
  strcpy(from,lookup(&player[player_id].addr));
  sprintf(email,"%s@",player[player_id].username);
  strcat(email,from);
  rewind(baduser_fp);
  while(fgets(msg_buf,80,baduser_fp)!=NULL)
  {
    msg_buf[strlen(msg_buf)-1]=0;
    if(msg_buf[0]==0)
      continue;
    if(strcmp(email,msg_buf)==0 || 
       strcmp(player[player_id].username,msg_buf)==0)
    {
      display_msg(player_id,
        "你已被限制進入, 有問題請 mail 給 sywu@csie.nctu.edu.tw");
      fflush(baduser_fp);
      return 0;
    }
    if(strstr(from,msg_buf))
    {
      sprintf(msg_buf1,"QKMJ server 目前不接受由 (%s) 連上之使用者.",msg_buf);
      display_msg(player_id,msg_buf1);
      fflush(baduser_fp);
      return 0;
    }
    strcpy(from,inet_ntoa(player[player_id].addr.sin_addr));
    if(strstr(from,msg_buf))
    {
      sprintf(msg_buf1,"QKMJ server 目前不接受由 (%s) 連上之使用者.",msg_buf);
      display_msg(player_id,msg_buf1);
      fflush(baduser_fp);
      return 0;
    }
  }
  fflush(baduser_fp);
  return 1;
}

write_record()
{
  char msg_buf[256];

  fclose(rec_fp);
  rec_fp=fopen(RECORD_FILE,"r+b");
  if(rec_fp==NULL)
  {
    sprintf(msg_buf,"(write_record) Cannot open file!");
    err(msg_buf,1);
    return;
  }
  fseek(rec_fp,sizeof(record)*record.id,0);
  fwrite(&record,sizeof(record),1,rec_fp);
  fflush(rec_fp);
}
  
print_news(fd,fp)
int fd;
FILE *fp;
{
  char msg[256];
  char msg_buf[256];
  char news_buf[8192];

  if(fp==NULL)
  {
    return;
  }
  rewind(fp);
  strcpy(news_buf,"102");
  while(fgets(msg,80,fp)!=NULL)
  {
    msg[strlen(msg)]=0;
    strcat(news_buf,msg);
  }
  write_msg(fd,news_buf);
  fflush(fp);
}

welcome_user(player_id)
int player_id;
{
  char msg_buf[256];
  int fd;
  int total_num=0;
  int online_num=0;
  int i;
  struct player_record tmp_rec;

  fd=player[player_id].sockfd;
  fclose(news_fp);
  news_fp=fopen(NEWS_FILE,"r+");
  print_news(player[player_id].sockfd,news_fp);
  read_user_name(player[player_id].name);
  if(record.money<20000 && record.game_count>=16)
  {
    record.money=20000;
    write_msg(fd,"101運氣不太好是嗎? 將你的金額提升為 20000, 好好加油!");
    write_record();
  }
  player[player_id].id=record.id;
  player[player_id].money=record.money;
  player[player_id].login=2;
  player[player_id].admin=0;
  player[player_id].serv=0;
  player[player_id].join=0;
  player[player_id].note[0]=0;
  for(i=1;i<MAX_PLAYER;i++)
  {
    if(player[i].login==2)
      online_num++;
  }
  sprintf(msg_buf,"101◇目前上線人數: %d 人       註冊人數: %d 人",online_num,
                   index_num);
  write_msg(player[player_id].sockfd,msg_buf);
  list_stat(player_id,player[player_id].name); 
  write_msg(player[player_id].sockfd,"003");
  sprintf(msg_buf,"120%5d%ld",player[player_id].id,
          player[player_id].money);
  write_msg(player[player_id].sockfd,msg_buf);
  player[player_id].input_mode=CMD_MODE;
}

int find_user_name(name)
char *name;
{
  int i;

  for(i=1;i<MAX_PLAYER;i++)
  {
    if(strcmp(player[i].name,name)==0)
      return i;
  }
  return -1;
}

init_variable()
{
  int i;

  login_limit=240;
  strcpy(admin_passwd,ADMIN_PASSWORD);
  for(i=0;i<MAX_PLAYER;i++)
  {
    player[i].admin=0;
    player[i].login=0;
    player[i].serv=0;
    player[i].join=0;
    player[i].type=16;
    player[i].note[0]=0;
    player[i].username[0]=0;
    friend[i].num=0;
  }
  log_flag[1]=0;
  log_flag[2]=1;
  make_key();
}

gps_processing()
{
  int alen;
  int fd;
  int player_id;
  int player_num=0;
  int i,j;
  int msg_id;
  int read_code;
  char tmp_buf[256];
  char msg_buf[256];
  char msg_buf1[256];
  unsigned char buf[256];
  struct timeval timeout;
  struct hostent *hp;
  long time();
  int id;
  struct timeval tm;
  long current_time;
  struct tm *tim;

log_level=0;
  /* Preopen all necessary files */
  rec_fp=fopen(RECORD_FILE,"a+b");
  inx_fp=fopen(INDEX_FILE,"a+b");
  news_fp=fopen(NEWS_FILE,"a+");
  baduser_fp=fopen(BADUSER_FILE,"a+");
  log_fp=fopen(LOG_FILE,"a+");

  refresh_index();
  FD_ZERO(&afds);
  FD_SET(gps_sockfd,&afds);
  bcopy((char *) &afds,(char *) &rfds,sizeof(rfds));
  tm.tv_sec=0;
  tm.tv_usec=0;

  /* Waiting for connections */
  for(;;)
  {
    bcopy((char *) &afds,(char *) &rfds,sizeof(rfds));
    if(select(nfds,&rfds,(fd_set *)0, (fd_set *)0, 0) <0)
    { 
      sprintf(msg_buf,"select: %d %s\n",errno,sys_errlist[errno]);
      err(msg_buf,1);
      continue;
    }
    if(FD_ISSET(gps_sockfd,&rfds))
    {
      for(player_num=1;player_num<MAX_PLAYER;player_num++)
        if(!player[player_num].login) break;
      if(player_num==MAX_PLAYER-1)
        err("Too many users",1);
      player_id=player_num;
      alen=sizeof(player[player_num].addr);
      player[player_id].sockfd=accept(gps_sockfd, (struct sockaddr *)
                                     &player[player_num].addr,&alen);
      FD_SET(player[player_id].sockfd,&afds);
      fcntl(player[player_id].sockfd,F_SETFL,FNDELAY);
      player[player_id].login=1;
      strcpy(climark,lookup(&player[player_id].addr));
      sprintf(msg_buf,"Connect with %s\n",climark);
      err(msg_buf,1);
      time(&current_time);
      tim=localtime(&current_time);
      if(player_id>login_limit)
      {
        if(strcmp(climark,"ccsun34")!=0)
        {
        write_msg(player[player_id].sockfd,"101*** 對不起,目前使用人數超過上限, 請稍後再進來.");
        close_id(player_id);
        }
      }
    }
    for(player_id=1;player_id<MAX_PLAYER;player_id++)
    {
    if(player[player_id].login)
      if(FD_ISSET(player[player_id].sockfd,&rfds))
      {
        /* Processing the player's information */
        read_code=read_msg(player[player_id].sockfd,buf);
        if(!read_code)
        {
          close_id(player_id); 
        }
        else if(read_code==1)
        {
        msg_id=convert_msg_id(player_id,buf);
        switch(msg_id)
        {
          case 99:   /* get username */
            buf[15]=0;
            strcpy(player[player_id].username,buf+3);
            break;
          case 100:  /* check version */
            *(buf+8)=0;
            strcpy(player[player_id].version,buf+3);
            break;
          case 101:  /* user login */
            buf[13]=0;
            strcpy(player[player_id].name,buf+3);
            for(i=0;i<strlen(buf)-3;i++)
            {
              if(buf[3+i]<=32 && buf[3+i]!=0)
              {
                write_msg(player[player_id].sockfd,"101Invalid username!");
                close_id(player_id);
                break;
              }
            }
            for(i=1;i<MAX_PLAYER;i++)
            {
              if(player[i].login==2 && strcmp(player[i].name,buf+3)==0)
              {
                write_msg(player[player_id].sockfd,"006");
                goto multi_login;
              }
            }
            if(read_user_name(player[player_id].name))
            {
              write_msg(player[player_id].sockfd,"002");
            }
            else
            {
              write_msg(player[player_id].sockfd,"005");
            }
            multi_login:;
            break;
          case 102:  /* Check password */
            if(read_user_name(player[player_id].name))
            {
              *(buf+11)=0;
              if(checkpasswd(record.password,buf+3))
              {
                for(i=1;i<MAX_PLAYER;i++)
                {
                  if(player[i].login==2 &&
                    strcmp(player[i].name,player[player_id].name)==0)
                  {
                     close_id(i);
                     break;
                  }
                }
                time(&record.last_login_time);
                record.last_login_from[0]=0;
                if(player[player_id].username[0]!=0)
                {
                  sprintf(record.last_login_from,"%s@",
                          player[player_id].username);
                }
                strcat(record.last_login_from,
                       lookup(&player[player_id].addr));
                record.login_count++;
                write_record();
                if(check_user(player_id))
                {
                  if(authentication)
                  {
                    if(!client_auth(player_id))
                      close_id(player_id);
                  }
                  else
                    welcome_user(player_id);
                }
                else
                  close_id(player_id);
              }
              else
              {
                write_msg(player[player_id].sockfd,"004");
              }
            }
            break;
          case 103:  /* Create new account */
            *(buf+11)=0;
            if(!add_user(player_id,player[player_id].name,buf+3))
            { 
              close_id(player_id);
              break;
            }
            if(authentication)
            {
              if(!client_auth(player_id))
                close_id(player_id);
            }
            else
              welcome_user(player_id);
            break;
          case 104:  /* Change password */
            *(buf+11)=0;
            read_user_name(player[player_id].name);
            strcpy(record.password,genpasswd(buf+3));
            write_record();
            break;
          case 110:  /* check client authentication response */
            sign(player[player_id].challenge,msg_buf);
            if(strcmp(buf+3,msg_buf)==0)
              welcome_user(player_id);
            else
            {
              sprintf(msg_buf,"Failed: %s@%s %s\n",player[player_id].username,
                        lookup(&player[player_id].addr),buf+3);
              err(msg_buf,2);
              write_msg(player[player_id].sockfd,"101*** 版本驗證失敗!");
              write_msg(player[player_id].sockfd,
              "101請至 ftp.csie.nctu.edu.tw/pub/CSIE/contrib/qkmj 下取得新版");
              close_id(player_id);
            }
            break;
          case 2:
            list_player(player[player_id].sockfd);
            break;
          case 3:
            list_table(player[player_id].sockfd,1);
            break;
          case 4:
            strcpy(player[player_id].note,buf+3);
            break;
          case 5:
            list_stat(player_id,buf+3);
            break;
          case 6:
            who(player_id,buf+3);
            break;
          case 7:
            if(player[player_id].admin==0)
              break;
            broadcast(player_id,buf+3);
            break;
          case 8:
            invite(player_id,buf+3);
            break;
          case 9:
            send_msg(player_id,buf+3);
            break;
          case 10:
            lurker(player[player_id].sockfd);
            break;
          case 11:
            /*  Check for table server  */
            if(player[player_id].login<2)
            {
              sprintf(msg_buf,"%s %s %s\n",buf,player[player_id].name
                 ,inet_ntoa(player[player_id].addr.sin_addr));
              err(msg_buf,2);
              break;
            }
            if(player[player_id].join!=0)
            {
              sprintf(msg_buf,"101*** 你已經在 %s 桌了!",
                      player[player[player_id].join].name);
              write_msg(player[player_id].sockfd,msg_buf);
              break;
            }
            for(i=1;i<MAX_PLAYER;i++)
            {
              if(player[i].login==2 && player[i].serv)
              {
                /* Find the name of table server */
                if(strcmp(player[i].name,buf+3)==0)
                {
                  if(player[i].serv>=4)
                  {
                    write_msg(player[player_id].sockfd,"101*** 此桌人數已滿!");
                    goto full;
                  }
                  sprintf(msg_buf,"120%5d%ld",player[player_id].id,
                          player[player_id].money);
                  write_msg(player[player_id].sockfd,msg_buf);
                  sprintf(msg_buf,"01100%s %d",
                     inet_ntoa(player[i].addr.sin_addr),player[i].port);
                  write_msg(player[player_id].sockfd,msg_buf);
                  player[player_id].join=i;
                  player[player_id].serv=0;
                  player[i].serv++;
                  break;
                }
              }
            }
            if(i==MAX_PLAYER)
              write_msg(player[player_id].sockfd,"0111");
            full:;
            break;
          case 12:
            if(player[player_id].login<2)
            {
              sprintf(msg_buf,"%s %s %s\n",buf,player[player_id].name
                 ,inet_ntoa(player[player_id].addr.sin_addr));
              err(msg_buf,2);
              break;
            }
            open_table:;
            player[player_id].port=atoi(buf+3);
            if(player[player_id].join)
            {
              if(player[player[player_id].join].serv>0)
                player[player[player_id].join].serv--;
              player[player_id].join=0;
            }
            /* clear all client */
            for(i=1;i<MAX_PLAYER;i++)
            {
              if(player[i].join==player_id)
                player[i].join=0;
            } 
            player[player_id].serv=1;
            sprintf(msg_buf,"120%5d%ld",player[player_id].id,
                    player[player_id].money);
            write_msg(player[player_id].sockfd,msg_buf);
            break;
          case 13:
            list_table(player[player_id].sockfd,2);
            break;
          case 20:
            if(player[player_id].login<2)
            {
              sprintf(msg_buf,"%s %s %s\n",buf,player[player_id].name
                 ,inet_ntoa(player[player_id].addr.sin_addr));
              err(msg_buf,2);
              break; 
            }
            strcpy(msg_buf,buf+3);
            *(msg_buf+5)=0;
            id=atoi(msg_buf);
            read_user_id(id);
            record.money=atol(buf+8);
            record.game_count++;
            write_record();
            for(i=1;i<MAX_PLAYER;i++)
            {
              if(player[i].login==2 && player[i].id==id)
              {
                player[i].money=record.money;
                break;
              }
            }
            break;
          case 21:  /* FIND */
            find_user(player[player_id].sockfd,buf+3);
            break;
          case 22:  /* FRIEND */
            list_friend(player_id);
            break;
          case 23:  /* Add Friend */
            add_friend(player_id,buf+3);
            break;
          case 30:  /* Admin */
            if(player[player_id].login<2)
            {
              sprintf(msg_buf,"%s %s %s\n",buf,player[player_id].name
                 ,inet_ntoa(player[player_id].addr.sin_addr));
              err(msg_buf,2);
              break;
            }
            *(buf+11)=0;
            if(strcmp(admin_passwd,buf+3)==0)
            {
              player[player_id].admin=1;
              write_msg(player[player_id].sockfd,"0301");
            }
            else
              write_msg(player[player_id].sockfd,"0300");
            break;
          case 111:
            break;
          case 200:
            close_id(player_id);
            break;
          case 202: /* kill user */
            if(player[player_id].admin==0)
              break; 
            id=find_user_name(buf+3);
            if(id>=0)
            {
              write_msg(player[id].sockfd,"200");
              close_id(id);
            }
            break;
          case 205:
            if(player[player_id].serv)
            {
              /* clear all client */
              for(i=1;i<MAX_PLAYER;i++)
              {
                if(player[i].join==player_id)
                  player[i].join=0;
              }
              player[player_id].serv=0;
              player[player_id].join=0;
            }
            else if(player[player_id].join)
            {
              if(player[player[player_id].join].serv>0)
                player[player[player_id].join].serv--;
              player[player_id].join=0;
            }
            break;
          case 300:  /* Suicide */
            suicide(player_id);
            break;
          case 310:
            break;
          case 311:
            break;
          case 400:  /* error log */
            sprintf(msg_buf,"(%s) %s\n",player[player_id].name,buf+3);
            err(msg_buf,2);
            break;
          case 500:
            if(player[player_id].admin==0)
              break; 
            shutdown_server();
            break;
          default:
            sprintf(msg_buf,"### cmd=%d player_id=%d sockfd=%d ###\n",msg_id,player_id,player[player_id].sockfd);
            err(msg_buf,1);
/* 
            close_connection(player_id);
            sprintf(msg_buf,"Connection to %s error, closed it\n",
                    lookup(&player[player_id].addr));
            err(msg_buf,1);
*/
            break;
        }
        buf[0]='\0';
        }
        else if(read_code==2)
        {
          sprintf(msg_buf,"No input data from %s",
                  lookup(&player[player_id].addr));
          err(msg_buf,1);
        }
      }
    }
  }
}

suicide(player_id)
int player_id;
{
  read_user_name(player[player_id].name);
  record.name[0]=0;
  write_record();
}

close_id(player_id)
int player_id;
{
   char msg_buf[256];

   close_connection(player_id);
   sprintf(msg_buf,"Connection to %s closed\n",
                   lookup(&player[player_id].addr));
   err(msg_buf,1);
}

close_connection(player_id)
int player_id;
{
  close(player[player_id].sockfd);
  FD_CLR(player[player_id].sockfd,&afds);
  if(player[player_id].join && player[player[player_id].join].serv)
    player[player[player_id].join].serv--;
  player[player_id].login=0;
  player[player_id].admin=0;
  player[player_id].serv=0;
  player[player_id].join=0;
  player[player_id].version[0]=0;
  player[player_id].note[0]=0;
  player[player_id].name[0]=0;
  player[player_id].username[0]=0;
  friend[player_id].num=0;
}

shutdown_server()
{
  int i;
  char msg_buf[256];

  for(i=1;i<MAX_PLAYER;i++)
  {
    if(player[i].login)
      shutdown(player[i].sockfd,2);   
  }
  sprintf(msg_buf,"QKMJ Server shutdown\n");
  err(msg_buf,1);
  exit(0);
}

void core_dump()
{
  err("CORE DUMP!\n",1);
  exit(0);
}

void bus_err()
{
  err("BUS ERROR!\n",1);
  exit(0);
}

void broken_pipe()
{
  err("Broken PIPE!!\n",1);
}

void time_out()
{
  timeup=1;
}

char *
genpasswd(pw)
char *pw ;
{
    char saltc[2] ;
    long salt ;
    int i,c ;
    static char pwbuf[14] ;
    long time() ;

    if(strlen(pw) == 0)
        return "" ;
    time(&salt) ;
    salt = 9 * getpid() ;
#ifndef lint
    saltc[0] = salt & 077 ;
    saltc[1] = (salt>>6) & 077 ;
#endif
    for(i=0;i<2;i++) {
        c = saltc[i] + '.' ;
        if(c > '9')
          c += 7 ;
        if(c > 'Z')
          c += 6 ;
        saltc[i] = c ;
    }
    strcpy(pwbuf, pw) ;
    return crypt(pwbuf, saltc) ;
}

int
checkpasswd(passwd, test)
char *passwd, *test ;
{
    static char pwbuf[14] ;
    char *pw ;

    strncpy(pwbuf,test,14) ;
    pw = crypt(pwbuf, passwd) ;
    return (!strcmp(pw, passwd)) ;
}

main(argc, argv)
int 	argc;
char 	*argv[];
{
  int i;
  long time(),seed;
  struct hostent *hp;
  struct in_addr myhostaddr;

  seed=time((long int *) 0);
  srand48(seed);
  /* Set fd to be the maximum number */
  getrlimit(RLIMIT_NOFILE,&fd_limit);
  fd_limit.rlim_cur=fd_limit.rlim_max;
  nfds=fd_limit.rlim_cur;
  setrlimit(RLIMIT_NOFILE,&fd_limit);
  signal(SIGSEGV,core_dump);
  signal(SIGBUS,bus_err);
  signal(SIGPIPE,broken_pipe);
  signal(SIGALRM, time_out);
  if(argc<2)
    gps_port=DEFAULT_GPS_PORT;
  else
  {
    gps_port=atoi(argv[1]);
  }
  gethostname(gps_ip,sizeof(gps_ip));
  hp=gethostbyname(gps_ip);
  bcopy(hp->h_addr,(char *)&myhostaddr,sizeof(myhostaddr));
  strcpy(gps_ip,inet_ntoa(myhostaddr));
  init_socket();
  init_variable();
  gps_processing();
}

#ifdef SVR4
bzero(b,len)
char *b;
int len;
{
  int i;

  for(i=0;i<len;i++)
    b[i]=0;
}

bcopy(b1,b2,len)
char *b1;
char *b2;
int len;
{
  int i;

  for(i=0;i<len;i++)
    b2[i]=b1[i];
}
#endif

