
// uartDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "uart.h"
#include "uartDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif
#define uchar unsigned char
#define u16 unsigned short
#define Data_Size 512
#define Send_Buf Data_Size	//发送文件时，每次从文件中读取的数据大小32*8bit
#define HARD_SEND_BUF 4096
#define HARD_REC_BUF 2048*10

#define START_SEND_FILE_CMD "~~"//"LS1X Receive OK."
#define file_send_size 1000//每次发送的文件数据帧大小 务必是8的倍数
bool sendfile_to_ls1x_flag=0;
FILE *rfp=NULL;

#define uart_config_file_name "program_config.txt" //默认的程序配置文件名
#define default_comx 8 //默认的串口号
#define default_baudrate 115200  //默认的串口速度
#define default_data_num 8  //默认的数据 位
#define default_data_stop 1  //默认的停止位
#define default_data_check 'n'  //默认的校验位
#define default_start_addr "1"  //开始刷写位置从1开始
#define default_file_browse_path "D:\\gzrom.bin"  //开始刷写位置

int comx=default_comx;
int baudrate=default_baudrate;
int data_num=default_data_num;
int data_stop=default_data_stop;
char data_check=default_data_check;

struct Uart_Rec_Data{
	char	rData[HARD_REC_BUF+100];//串口接收缓存
	int		count_now;					//当前数据存的位置
	int		count_old;					//上一次接收数据时的count值

	bool	get_new_data;			//接收到新数据了

	bool	send_busy;			//发送忙中
	bool sending_file_flag;

	FILE *rfp;
	int read_times;

};
struct Uart_Rec_Data uart_received_cache;//串口缓存

CuartDlg *global_dlg_pt=NULL;

void CuartDlg::debug_show(char leixing,CString show_what)//调试显示函数
{
	CString temp;
	temp.Format(_T("<%03d>:"),debug_show_lines);
	if(leixing==1)
	{
		debug_show_lines=1;
		temp.Format(_T("<%03d>:"),debug_show_lines);
		m_EditReceive=temp+show_what;
	}
	else if(leixing==0)
	{
		//m_EditReceive=m_EditReceive+temp+show_what+_T("\r\n");
		m_EditReceive=m_EditReceive+show_what+_T("\r\n");
		debug_show_lines++;
	}

	SetDlgItemText(IDC_EDIT2,m_EditReceive);
	m_EditReceive_Ctr.LineScroll(m_EditReceive_Ctr.GetLineCount());//自动滚动到最后一行 m_EditReceive_Ctr这个是编辑框的控制变量

	/*if(debug_show_lines>150)
	{
		debug_show_lines=0;
		m_EditReceive=_T("");
	}
	*/
}


/*
void CuartDlg::write_log_into_file(char * filename,CString data)
{
	FILE *wfp=fopen(filename,"a+");//写入log
	if(NULL==wfp)
	{
		m_EditReceive+="打开日志文件失败，将没有log记录.\r\n";
		UpdateData(false);
		return;
	}
	char write_log_str[100];
	strncpy(write_log_str,data,data.GetLength()+1);
	fwrite(write_log_str,sizeof(char),strlen(write_log_str),wfp);
	fclose(wfp);
}
*/
void CuartDlg::bin_to_char(char * dest_file_name,char * target_file_name)
{
	char showstr[100];
	FILE *wfp = fopen(target_file_name,"w+");
	char rData[10]="";//读取缓存
	char wData[10]="";
	char tempx[20]="";
	CString cstempx;
	int read_count=0,times=0;
	long pos=0,pos_end;
	FILE *rfp = fopen(dest_file_name,"rb");
	if(NULL == rfp)
	{
		sprintf(showstr,"打开文件%s失败,转换失败\n",dest_file_name);
		cstempx.Format(_T("%s"),showstr);
		m_EditReceive+=cstempx;  
		UpdateData(false); //更新编辑框内容
		m_EditReceive_Ctr.LineScroll(m_EditReceive_Ctr.GetLineCount());//自动滚动到最后一行 m_EditReceive_Ctr这个是编辑框的控制变量
		return;
	}

	fseek(rfp,0L,SEEK_END);
	pos_end=ftell(rfp);
	fseek(rfp,0L,SEEK_SET);
	while(ftell(rfp)<pos_end)
	{
		memset(rData,0,10);
		memset(tempx,0,20);
		read_count=fread(rData,sizeof(char),1,rfp);//读取文件内容
		sprintf(tempx,"%02x",(unsigned char)rData[0]);//读出hex的值变成字符
		wData[0]=tempx[0];
		wData[1]=tempx[1];
		//wData[2]=tempx[2];
		fwrite(wData,sizeof(char),2,wfp);
		times++;
	}
	fclose(rfp);
	fclose(wfp);
	m_EditReceive+="\r\n转换结束\r\n";
	UpdateData(false); //更新编辑框内容 
	m_EditReceive_Ctr.LineScroll(m_EditReceive_Ctr.GetLineCount());//自动滚动到最后一行 m_EditReceive_Ctr这个是编辑框的控制变量
}




//从cache中查找命令 原理：从cache的结尾前面len范围中查找，若找到返回1，否则返回0 如(helloxxyyzzcmdxxx)查找cmd
bool find_cmd_from_cache(struct Uart_Rec_Data * data,char * cmd,int len)
{
	if(data->get_new_data==1)//有新数据进来了
		data->get_new_data=0;//标记处理
	else
		return 0;

	int i=data->count_now-1-len*2;//由于从缓存结尾附近找 从这里开始
	int j=0;
	if(i<0)
		i=0;
	for(;data->rData[i]!='\0';i++)//没到结尾
	{
		for(j=0;cmd[j]!='\0';j++)
		{
			if(cmd[j]==data->rData[i+j])//如果相同 比对下一个
				continue;
			else
				break;
		}
		if(cmd[j]=='\0')//成功比对到结尾了
			return 1;
	}
	return 0;
}
char * cstring_to_char(char *desc,CString & cstring)
{
	CString cstmp=cstring;
	LPTSTR pp=cstmp.GetBuffer(0);//双char
	cstmp.ReleaseBuffer(); 
	UINT lens=cstmp.GetLength()+1;
	char * pBuff = new char[lens*2+1];
	WideCharToMultiByte(CP_ACP, 0, pp, -1, pBuff, wcslen(pp)*2, 0, 0);//双char转成单CHAR
	pBuff[wcslen(pp)*2] ='\0';
	strncpy(desc,pBuff,strlen(pBuff)+1);
	delete pBuff;
	return desc;
}
wchar_t * char_to_wchar(char *p,wchar_t *wchar,int nLen)
{
	MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, p, -1, wchar, nLen );
	return wchar;
}
CString char_to_cstring(char *source,UINT len)
{
	wchar_t* wchar=new wchar_t[len*2];
	char_to_wchar(source,wchar,len*2);
	CString tempwchar;
	tempwchar.Format(_T("%s"),wchar);
	delete wchar;
	return tempwchar;
}

void strcpyxy(char * dest,char * sources,unsigned int from,unsigned int len)
{
	int i,j;
	for(i=from,j=0;j<len;i++,j++)
	{
		dest[j]=sources[i];
	}
}
void strcatxy(char * dest,char * sources,unsigned int from,unsigned int len)
{
	int i,j;
	for(i=from,j=0;j<len;i++,j++)
	{
		dest[i]=sources[j];
	}
}
bool find_cmd_str(int from,char * src,UINT src_len,char * cmd,UINT cmd_len)
{
	int i=from;//由于从src结尾附近找 从这里开始
	int j=0;
	if(i<0)//指令过长
		return 0;
	for(;i<src_len;i++)//没到结尾
	{

		for(j=0;j<cmd_len;j++)
		{
			if(cmd[j]==src[i+j])//如果相同 比对下一个
				continue;
			else
				break;
		}
		if(cmd[j]=='\0')//成功比对到结尾了
			return 1;
	}
	return 0;
}
int find_str_from_cache(int from,char * src,UINT src_len,char * cmd,UINT cmd_len)
{
	int i=from;//由于从src结尾附近找 从这里开始
	int j=0;
	if(i<0)//指令过长
		return 0;
	for(;i<src_len;i++)//没到结尾
	{
		for(j=0;j<cmd_len;j++)
		{
			if(cmd[j]==src[i+j])//如果相同 比对下一个
				continue;
			else
				break;
		}
		if(cmd[j]=='\0')//成功比对到结尾了
			return i;//返回当前找到的字符串开始位置
	}
	return -1;
}

long read_all_config_data(FILE *wfp,char *buf,int *buf_len)
{
	if(wfp==NULL||buf==NULL)
		return 0;
	fseek(wfp,0,SEEK_SET);
	char readtemp[512];
	memset(readtemp,0,512);
	int readed_size=0;

	memset(buf,0,*buf_len);
	long buf_userd=0;
	while((readed_size=fread(readtemp,1,512,wfp))!=0)
	{
		if(readed_size>*buf_len-buf_userd)//空间不够了
		{
			*buf_len=*buf_len+readed_size+1024;
			buf=(char *)realloc(buf,*buf_len);
		}
		strcatxy(buf,readtemp,buf_userd,readed_size);
		buf_userd+=readed_size;
	}
	return buf_userd;//返回读取的总大小
}
void set_data_write_into_file(char *filename,char *name,char*str_value)
{
	FILE *wfp=fopen(filename,"rb+");
	if(NULL==wfp)
	{
		wfp=fopen(filename,"wb");//新建聊天记录文件
		if(NULL==wfp)
		{
			global_dlg_pt->debug_show(0,_T("新建配置文件失败."));
			return;
		}
		else//新建聊天文件成功
		{
			fclose(wfp);
			wfp=fopen(filename,"rb+");
			if(NULL==wfp)
			{
				global_dlg_pt->debug_show(0,_T("新建聊天记录文件成功后，用rb+方式打开失败."));
				return;
			}
		}
	}
	int buf_used=0;
	int total_space_size=1024;
	char*read_all_data=(char*)malloc(total_space_size);
	buf_used=read_all_config_data(wfp,read_all_data,&total_space_size);

	char*pt=read_all_data+buf_used;
	memset(pt,0,total_space_size-buf_used);//剩余的空间清零

	int start_finded_pos=-1,end_finded_pos=-1;
	start_finded_pos=find_str_from_cache(0,read_all_data,buf_used,name,strlen(name));
	if(start_finded_pos!=-1)
	{
		end_finded_pos=find_str_from_cache(start_finded_pos,read_all_data,buf_used,";\r\n",3);
		if(end_finded_pos!=-1)
		{
			pt=read_all_data+end_finded_pos+2;
			char * data_temp=(char*)malloc(buf_used-end_finded_pos-2+100);
			memset(data_temp,0,buf_used-end_finded_pos-2+100);
			strcpyxy(data_temp,pt,0,buf_used-end_finded_pos-2);
			memset(pt,0,buf_used-end_finded_pos-1-1);
			pt=read_all_data+start_finded_pos;
			memset(pt,0,buf_used-start_finded_pos);
			char *now_name_value=(char*)malloc(strlen(name)+strlen(str_value)+10);
			memset(now_name_value,0,strlen(name)+strlen(str_value)+10);
			strcpy(now_name_value,name);
			strcat(now_name_value,"=");
			strcat(now_name_value,str_value);
			strcat(now_name_value,";\r\n");
			strcatxy(read_all_data,now_name_value,start_finded_pos,strlen(now_name_value));
			strcatxy(read_all_data,data_temp,start_finded_pos+strlen(now_name_value),strlen(data_temp));
			free(now_name_value);
			free(data_temp);
		}
	}
	else//没有
	{
		char *now_name_value=(char*)malloc(strlen(name)+strlen(str_value)+10);
		memset(now_name_value,0,strlen(name)+strlen(str_value)+10);
		strcpy(now_name_value,name);
		strcat(now_name_value,"=");
		strcat(now_name_value,str_value);
		strcat(now_name_value,";\r\n");
		strcatxy(read_all_data,now_name_value,buf_used,strlen(now_name_value));
		buf_used=buf_used+strlen(now_name_value);
		free(now_name_value);
	}
	fseek(wfp,0,SEEK_SET);
	fwrite(read_all_data,1,strlen(read_all_data),wfp);
	fclose(wfp);
	free(read_all_data);
}
bool get_config_from_set_file(char*filename,char*name,char*value)
{
	bool ret=0;
	FILE *wfp=fopen(filename,"rb");
	if(NULL==wfp)
	{
		wfp=fopen(filename,"wb+");//新建
		if(NULL==wfp)//新建失败啦
			return 0;
		fclose(wfp);
		return 0;
	}
	fseek(wfp,0,SEEK_SET);

	int buf_used=0;
	int total_space_size=1024;
	char*read_all_data=(char*)malloc(total_space_size);
	buf_used=read_all_config_data(wfp,read_all_data,&total_space_size);

	char *name_temp=(char*)malloc(strlen(name)+10);
	memset(name_temp,0,strlen(name)+10);
	strcat(name_temp,name);
	strcat(name_temp,"=");

	long finded_pos_start=-1,finded_pos_end=-1;
	finded_pos_start=find_str_from_cache(0,read_all_data,buf_used,name_temp,strlen(name_temp));
	if(finded_pos_start!=-1)
	{
		finded_pos_end=find_str_from_cache(finded_pos_start,read_all_data,buf_used,";\r\n",3);
		if(finded_pos_end!=-1)
		{
			strcpyxy(value,read_all_data+finded_pos_start+strlen(name_temp),0,finded_pos_end-finded_pos_start-strlen(name_temp));
			ret=1;
		}
		else
			ret=0;
	}
	else
		ret=0;

	free(read_all_data);
	free(name_temp);
	return ret;
}

// 用于应用程序“关于”菜单项的 CAboutDlg 对话框
class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 对话框数据
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CuartDlg 对话框
CuartDlg::CuartDlg(CWnd* pParent /*=NULL*/) //初始化？？
	: CDialogEx(CuartDlg::IDD, pParent)
	, m_EditSend(_T(""))
	, m_EditReceive(_T(""))
	//, History_Cmd(_T(""))
	, Com_X(_T(""))
	, Baud_Rate(_T(""))
	, Data_Bit_Num(_T(""))
	, Data_Stop_Bit_Num(_T(""))
	, Data_Check_Bit(_T(""))
	, File_browse_path(_T(""))
	, m_add_enter(FALSE)
	, m_reset_into_pmon_flag(FALSE)
	//, m_default_load_pro_flag(FALSE)
	, m_reset_pmon_flag(FALSE)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CuartDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_MSCOMM1, m_mscomm);
	DDX_Text(pDX, IDC_EDIT1, m_EditSend);
	DDX_Text(pDX, IDC_EDIT2, m_EditReceive);
	DDX_Control(pDX, IDC_EDIT2, m_EditReceive_Ctr);
	//DDX_Text(pDX, IDC_EDIT3, History_Cmd);
	//DDX_Control(pDX, IDC_EDIT3, History_Cmd_Ctr);
	DDX_Text(pDX, IDC_EDIT4, Com_X);
	DDX_Text(pDX, IDC_EDIT5, Baud_Rate);

	DDX_Text(pDX, IDC_EDIT6, Data_Bit_Num);
	DDX_Text(pDX, IDC_EDIT7, Data_Stop_Bit_Num);
	DDX_Text(pDX, IDC_EDIT8, Data_Check_Bit);
	//DDX_Text(pDX, IDC_EDIT10, Start_addr);
	DDX_Text(pDX, IDC_MFCEDITBROWSE1, File_browse_path);
	DDX_Check(pDX, IDC_CHECK2, m_add_enter);
	DDX_Check(pDX, IDC_CHECK3, m_reset_into_pmon_flag);
	//DDX_Check(pDX, IDC_CHECK4, m_default_load_pro_flag);
	DDX_Check(pDX, IDC_CHECK4, m_reset_pmon_flag);
	DDX_Control(pDX, IDC_PROGRESS1, m_send_file_progress);
}

BEGIN_MESSAGE_MAP(CuartDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()

	ON_BN_CLICKED(IDC_BUTTON_OPEN, &CuartDlg::OnBnClickedButtonOpen)
	ON_BN_CLICKED(IDC_BUTTON_SEND, &CuartDlg::OnBnClickedButtonSend)
	ON_BN_CLICKED(IDC_BUTTON_CLEAR, &CuartDlg::OnBnClickedButtonClear)
	ON_BN_CLICKED(IDC_CHECK1, &CuartDlg::OnBnClickedCheck1)
	ON_BN_CLICKED(IDC_BUTTON_SendfileStop, &CuartDlg::OnBnClickedButtonSendfilestop)

	ON_MESSAGE(WM_SENDFILE_TO_LS1X_MESSAGE,sendfile_to_ls1x_one_datas) //消息映射给响应函数OnMyMessage

	ON_BN_CLICKED(IDC_BUTTON_SEND4, &CuartDlg::OnBnClickedButtonSend4)
	ON_BN_CLICKED(IDC_BUTTON_RUN_PRO, &CuartDlg::OnBnClickedButtonRunPro)
	ON_BN_CLICKED(IDC_BUTTON_BOARD_RESET, &CuartDlg::OnBnClickedButtonBoardReset)
	ON_BN_CLICKED(IDC_CHECK3, &CuartDlg::OnBnClickedCheck3)
	ON_BN_CLICKED(IDC_BUTTON_BOARD_RESET3, &CuartDlg::OnBnClickedButtonBoardReset3)
	ON_BN_CLICKED(IDC_BUTTON_BOARD_RESET4, &CuartDlg::OnBnClickedButtonBoardReset4)
	ON_EN_CHANGE(IDC_MFCEDITBROWSE1, &CuartDlg::OnEnChangeMfceditbrowse1)
	ON_BN_CLICKED(IDC_BUTTON_UPDATA_PMON, &CuartDlg::OnBnClickedButtonUpdataPmon)
	ON_BN_CLICKED(IDC_BUTTON_CLEAR_USER_SPACE, &CuartDlg::OnBnClickedButtonClearUserSpace)
END_MESSAGE_MAP()

// CuartDlg 消息处理程序
void CuartDlg::uart_argc_get_from_config_file(char *filename)
{
	bool ret=0;
	char arg_value[512];

	memset(arg_value,0,512);
	ret=get_config_from_set_file(filename,"Com_Num",arg_value);
	if(ret)
		Com_X=char_to_cstring(arg_value,512);
	else
		Com_X.Format(_T("%d"),default_comx);

	memset(arg_value,0,512);
	ret=get_config_from_set_file(filename,"Baud_Rate",arg_value);
	if(ret)
		Baud_Rate=char_to_cstring(arg_value,512);
	else
		Baud_Rate.Format(_T("%d"),default_baudrate);

	memset(arg_value,0,512);
	ret=get_config_from_set_file(filename,"Data_Bit_Num",arg_value);
	if(ret)
		Data_Bit_Num=char_to_cstring(arg_value,512);
	else
		Data_Bit_Num.Format(_T("%d"),default_data_num);

	memset(arg_value,0,512);
	ret=get_config_from_set_file(filename,"Data_Stop_Bit_Num",arg_value);
	if(ret)
		Data_Stop_Bit_Num=char_to_cstring(arg_value,512);
	else
		Data_Stop_Bit_Num.Format(_T("%d"),default_data_stop);

	memset(arg_value,0,512);
	ret=get_config_from_set_file(filename,"Data_Check_Bit",arg_value);
	if(ret)
		Data_Check_Bit.Format(_T("%c"),arg_value[0]);
	else
		Data_Check_Bit.Format(_T("%c"),default_data_check);

	memset(arg_value,0,512);
	ret=get_config_from_set_file(filename,"File_browse_path",arg_value);
	if(ret)
		File_browse_path=char_to_cstring(arg_value,512);
	else
		File_browse_path=char_to_cstring(default_file_browse_path,strlen(default_file_browse_path));


	comx=default_comx;
	baudrate=default_baudrate;
	data_num=default_data_num;
	data_stop=default_data_stop;
	data_check=default_data_check;
}

void CuartDlg::update_uart_config_file(char *filename)
{
	char arg_value[512];
	char*pt=arg_value;

	UpdateData(TRUE);

	memset(arg_value,0,512);
	pt=cstring_to_char(pt,Com_X);
	set_data_write_into_file(filename,"Com_Num",pt);

	memset(arg_value,0,512);
	pt=cstring_to_char(pt,Baud_Rate);
	set_data_write_into_file(filename,"Baud_Rate",pt);

	memset(arg_value,0,512);
	pt=cstring_to_char(pt,Data_Bit_Num);
	set_data_write_into_file(filename,"Data_Bit_Num",pt);

	memset(arg_value,0,512);
	pt=cstring_to_char(pt,Data_Stop_Bit_Num);
	set_data_write_into_file(filename,"Data_Stop_Bit_Num",pt);

	memset(arg_value,0,512);
	pt=cstring_to_char(pt,Data_Check_Bit);
	set_data_write_into_file(filename,"Data_Check_Bit",pt);

	memset(arg_value,0,512);
	pt=cstring_to_char(pt,File_browse_path);
	set_data_write_into_file(filename,"File_browse_path",pt);
}

BOOL CuartDlg::OnInitDialog() //窗口初始化
{
	CDialogEx::OnInitDialog();
	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 设置此对话框的图标。当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	global_dlg_pt=this;

	debug_show_lines=0;
	//ShowWindow(SW_MAXIMIZE);//窗口打开自动最大化

	//从配置文件获取串口配置参数

	clear_uart_recv_buf();
	uart_received_cache.get_new_data=0;
	uart_received_cache.sending_file_flag=0;
	uart_received_cache.send_busy=0;


	uart_opened=0;
	m_add_enter=1;
	m_reset_into_pmon_flag=0;
	get_into_pmon_mode_pThread=NULL;
	//m_default_load_pro_flag=0;


	get_board_arg_thread_sta=0;
	get_board_arg_thread=NULL;
	memset(get_board_arg_ret,0,512);


	m_reset_pmon_flag=0;
	send_pmon_flag=0;
	uart_argc_get_from_config_file(uart_config_file_name);
	UpdateData(false);//default_baudrate
	update_uart_config_file(uart_config_file_name);
	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}



void CuartDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)//打开about对话框
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CuartDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CuartDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}





void CuartDlg::Get_Parg(void)//获取设置参数 需要放到主类目录下uartDlg.h下
{
	UpdateData(true); //读取编辑框内容  
	comx=_ttoi(Com_X);//CSting字符串转int
	baudrate=_ttoi(Baud_Rate);
	data_num=_ttoi(Data_Bit_Num);
	data_stop=_ttoi(Data_Stop_Bit_Num);

	///char tempx[3];
	char * pp=new char[3];
	cstring_to_char(pp,Data_Check_Bit);	
	//strncpy(tempx,Data_Check_Bit,2);//CSting转换为char *
	data_check=pp[0];
	delete pp;
}

void CuartDlg::OnBnClickedButtonOpen()
{
	update_uart_config_file(uart_config_file_name);
	if(uart_opened==0)//如果串口没有打开
	{
		CString cs_temp;
		Get_Parg();//获取参数
		cs_temp.Format(_T("%d,%c,%d,%d"),baudrate,data_check,data_num,data_stop);

		if(m_mscomm.get_PortOpen()) //如果串口是打开的，则行关闭串口   
			m_mscomm.put_PortOpen(FALSE);     
		m_mscomm.put_CommPort(comx); //选择COM1  4
		m_mscomm.put_InBufferSize(HARD_REC_BUF); //接收缓冲区  1024
		m_mscomm.put_OutBufferSize(HARD_SEND_BUF);//发送缓冲区  1024 
		m_mscomm.put_InputLen(0);//设置当前接收区数据长度为0,表示全部读取  
		m_mscomm.put_InputMode(1);//输入方式1以二进制方式读写数据  0  文本
		m_mscomm.put_RThreshold(1);//接收缓冲区有1个及1个以上字符时，将引发接收数据的OnComm事件   
		m_mscomm.put_Settings(cs_temp);//波特率9600无检验位，8个数据位，1个停止位  "19200,n,8,1" 

		if(!m_mscomm.get_PortOpen())//如果串口没有打开则打开  
		{   
			m_mscomm.put_PortOpen(TRUE);//打开串口  
			cs_temp.Format(_T("串口%d打开成功"),comx);
			debug_show(0,cs_temp);
		}  
		else  
		{   
			m_mscomm.put_OutBufferCount(0); 
			cs_temp.Format(_T("串口%d打开失败"),comx);
			debug_show(0,cs_temp);
		} 
		UpdateData(false);
		SetDlgItemText(IDC_BUTTON_OPEN, _T("关闭串口"));
		uart_opened=1;
	}
	else
	{
		m_mscomm.put_PortOpen(FALSE);//关闭串口  
		CString show_temp;
		show_temp.Format(_T("串口%d已关闭\r\n"),comx);
		debug_show(0,show_temp);
		SetDlgItemText(IDC_BUTTON_OPEN, _T("打开串口"));
		uart_opened=0;
	}
}

void CuartDlg::OnBnClickedButtonSend()
{
	// TODO: 在此添加控件通知处理程序代码
	UpdateData(true); //读取编辑框内容  
	if(m_add_enter==0)
		m_mscomm.put_Output(COleVariant(m_EditSend));//发送数据
	else
	{
		m_EditSend=m_EditSend+_T("\n");
		m_mscomm.put_Output(COleVariant(m_EditSend));//发送数据
	}
	m_EditSend.Empty();
	/*
	CString temp=_T("\r\n"); //换行   
	m_EditReceive+=temp;
	History_Cmd+=m_EditSend;//存入历史指令编辑框
	History_Cmd+=temp;

	UpdateData(false); //更新编辑框内容 
	History_Cmd_Ctr.LineScroll(History_Cmd_Ctr.GetLineCount());//自动滚动到最后一行 m_EditReceive_Ctr这个是编辑框的控制变量
	*/
}

void DelayUs(int uDelay)  
{  
  
    LARGE_INTEGER litmp;  
    LONGLONG QPart1,QPart2;  
  
    double dfMinus,dfFreq,dfTim;  
      
    /* 
        Pointer to a variable that the function sets, in counts per second, to the current performance-counter frequency.  
        If the installed hardware does not support a high-resolution performance counter,  
        the value passed back through this pointer can be zero.  
 
    */  
    QueryPerformanceFrequency(&litmp);  
  
    dfFreq = (double)litmp.QuadPart;  
  
    /* 
        Pointer to a variable that the function sets, in counts, to the current performance-counter value.  
    */  
    QueryPerformanceCounter(&litmp);  
  
    QPart1 = litmp.QuadPart;  
    do  
    {  
           QueryPerformanceCounter(&litmp);  
        QPart2 = litmp.QuadPart;  
        dfMinus = (double)(QPart2-QPart1);  
        dfTim = dfMinus/dfFreq;  
     }while(dfTim<0.000001 * uDelay);  
}


LRESULT CuartDlg::sendfile_to_ls1x_one_datas(WPARAM wParam,LPARAM lParam)//消息响应函数
{
	char rData[file_send_size+3]="";//读取缓存 数据+crc16+1
	char send_temp[file_send_size*2+6];
	char *pt=send_temp;
	int read_count=0,index=0;
	int	 send_temp_counter=0;

	CString showtemp;
	//debug_show(0,_T("sendfile_to_ls1x_one_datas()"));
	uart_received_cache.send_busy=1;//发送忙
	{
		memset(rData,0,file_send_size+1);
		memset(pt,0,file_send_size*2+1);
		//debug_show(0,_T("sendfile_to_ls1x_one_datas()1"));
		read_count=fread(rData,sizeof(char),file_send_size,rfp);//读取文件内容
		//showtemp.Format(_T("read_count:%d"),read_count);
		//debug_show(0,showtemp);
		for(index=0;index<read_count;index++)
		{
			sprintf(pt+send_temp_counter,"%02x",(unsigned char)rData[index]);//读出hex的值变成字符
			send_temp_counter+=2;
		}
		//添加crc16计算值
		unsigned short crc_value=0xffff;
		crc_value=comp_crc16((unsigned char*)rData,read_count);
		sprintf(pt+send_temp_counter,"%04x",(unsigned short)crc_value);//读出hex的值变成字符
		send_temp_counter+=4;

		//showtemp.Format(_T("neirong:%s"),pt);
		//debug_show(0,showtemp);
		if(read_count<file_send_size)//应该发送结束了
		{
			fclose(rfp);
			rfp=NULL;
			(pt+send_temp_counter)[0]='e';
			(pt+send_temp_counter)[1]='n';
			(pt+send_temp_counter)[2]='d';
			(pt+send_temp_counter)[3]='@';
			sendfile_to_ls1x_flag=0;//结束标志
			send_pmon_flag=0;
		}
		CString cs_send =char_to_cstring(pt,file_send_size*2+6);
		m_mscomm.put_Output(COleVariant(cs_send));

		m_progress_step+=1;
		m_send_file_progress.SetPos(m_progress_step);
		clear_uart_recv_buf();//必须清空 不然会有风险

	}
	uart_received_cache.send_busy=0;//一次发送结束
	//debug_show(0,_T("sendfile_to_ls1x_one_datas() end"));

	return 0;
}

void CuartDlg::OnBnClickedButtonSendfilestop()//终止刷写线程
{
	// TODO: 在此添加控件通知处理程序代码
	sendfile_to_ls1x_flag=0;
	if(rfp!=NULL)
		fclose(rfp);
	rfp=NULL;
	AfxMessageBox(_T("从机若陷入死循环，请重新主板，本程序最好重启下")); //"串口4已关闭"
}

UINT send_file_thread(LPVOID pParam)//检查应答信号线程主函数 非类成员 创建线程的需要
{
	CuartDlg *pObj = (CuartDlg *)pParam;
	while(sendfile_to_ls1x_flag==1)//死循环检测从机串口发来的应答信号 若发现应答则发送消息给响应函数再次发送
	{
		if(uart_received_cache.send_busy==0&&find_cmd_from_cache(&uart_received_cache,START_SEND_FILE_CMD,strlen(START_SEND_FILE_CMD)))//不忙检测命令 并且查找命令成功
			pObj->SendMessage(WM_SENDFILE_TO_LS1X_MESSAGE,NULL,NULL);//发送消息给父进程send_one_datas函数 (LPARAM)&testsend
	}
	return 0;
}

void CuartDlg::OnBnClickedButtonSend4()
{
	if(uart_opened==0)
	{
		debug_show(0,_T("请先打开串口."));
		return;
	}
	if(sendfile_to_ls1x_flag)
	{
		debug_show(0,_T("正在发送，等待上一次发送完成."));
		return;
	}

	char showstr[100];
	char *dest_file_name=new char[512];//[512];//="d:\\hello";
	char rData[file_send_size+1]="";//读取缓存
	char wData[10]="";
	char tempx[20]="";
	CString cstempx;
	int read_count=0,times=0,index=0;
	long pos=0,pos_end;
	char send_temp[file_send_size*2+1];
	char *pt=send_temp;
	int	 send_temp_counter=0;
	memset(pt,0,file_send_size*2+1);


	UpdateData(true);
	debug_show(0,File_browse_path);
	cstring_to_char(dest_file_name,File_browse_path);
	//strncpy(dest_file_name,File_browse_path,512);//CSting转换为char *


	rfp = fopen(dest_file_name,"rb");
	if(NULL == rfp)
	{
		sprintf(showstr,"打开文件%s失败.\n",dest_file_name);
		cstempx.Format(_T("%s"),showstr);
		m_EditReceive+=cstempx;  
		UpdateData(false); //更新编辑框内容
		m_EditReceive_Ctr.LineScroll(m_EditReceive_Ctr.GetLineCount());//自动滚动到最后一行 m_EditReceive_Ctr这个是编辑框的控制变量
		return;
	}

	m_mscomm.put_Output(COleVariant(_T("recv_data_test_cmd\n")));
	DelayUs(1000000);

	fseek(rfp,0L,SEEK_END);
	pos_end=ftell(rfp);
	fseek(rfp,0L,SEEK_SET);

	//发送文件大小和每次发送的文件段大小 告知客户端
	char head_str[64];//第一次发送给客户端的文件信息
	char *pt_hread=head_str;
	memset(pt_hread,0,64);
	sprintf(pt_hread,"%08x",(unsigned int)pos_end);
	sprintf(pt_hread+8,"%08x",(unsigned int)file_send_size);
	if(send_pmon_flag)
		sprintf(pt_hread+16,"%08x",(unsigned int)3512);//发送的是PMON标记
	else
		sprintf(pt_hread+16,"%08x",(unsigned int)1234);//发送的是裸机程序标记

	CString cs_send =char_to_cstring(pt_hread,64);
	m_mscomm.put_Output(COleVariant(cs_send));
	//m_mscomm.put_Output(COleVariant(pt_hread));
	DelayUs(10000);

	m_progress_start=0;
	m_progress_end=pos_end/file_send_size+1;
	m_progress_step=1;
	m_send_file_progress.SetRange(m_progress_start,m_progress_end);

	//下面创建线程，配合发送文件
	CWinThread *m_pThread; //线程指针
	sendfile_to_ls1x_flag=1;//线程死循环条件
	m_pThread = AfxBeginThread(send_file_thread,(LPVOID)this);//创建线程

	delete dest_file_name;
}



BEGIN_EVENTSINK_MAP(CuartDlg, CDialogEx)
	ON_EVENT(CuartDlg, IDC_MSCOMM1, 1, CuartDlg::OnCommMscomm1, VTS_NONE)
END_EVENTSINK_MAP()


void CuartDlg::OnBnClickedButtonClear()
{
	// TODO: 在此添加控件通知处理程序代码
	UpdateData(true);
	m_EditReceive.Empty(); //发送后清空输入框 
	UpdateData(false); //更新编辑框内容
}

void CuartDlg::clear_uart_recv_buf(void)//串口接收缓存清空
{
	memset(uart_received_cache.rData,0,HARD_REC_BUF+100);
	uart_received_cache.count_now=0;
	uart_received_cache.count_old=0;
}

void CuartDlg::OnCommMscomm1()//串口接收程序
{
	// TODO: 在此处添加消息处理程序代码
	VARIANT variant_inp;    
	COleSafeArray safearray_inp;   
	long len,k;    
	byte rxdata[HARD_REC_BUF]; //设置 BYTE 数组   
	CString strtemp;    
	if(m_mscomm.get_CommEvent()==2) //值为 2 表示接收缓冲区内有字符   
	{ 
		variant_inp=m_mscomm.get_Input(); //读缓冲区消息    
		safearray_inp=variant_inp; //变量转换    
		len=safearray_inp.GetOneDimSize(); //得到有效的数据长度 

		uart_received_cache.count_old=uart_received_cache.count_now;//记录上一次计数值
		if((HARD_REC_BUF-uart_received_cache.count_now)<=len)//如果cache剩余空间不够 清零
			clear_uart_recv_buf();
		/*{
			memset(uart_received_cache.rData,0,HARD_REC_BUF+100);
			uart_received_cache.count_now=0;
			uart_received_cache.count_old=0;
		}*/

		for(k=0;k<len;k++)    
		{    
			safearray_inp.GetElement(&k,rxdata+k);
			strtemp.Format(_T("%c"),*(rxdata+k));   //CString格式转化 
			m_EditReceive+=strtemp;
			uart_received_cache.rData[uart_received_cache.count_now]=rxdata[k];
			uart_received_cache.count_now++;
		}

		uart_received_cache.rData[uart_received_cache.count_now]='\0';//结尾
		uart_received_cache.get_new_data=1;//接收到新数据了
	} 

	UpdateData(false); //更新编辑框内容
	m_EditReceive_Ctr.LineScroll(m_EditReceive_Ctr.GetLineCount());//自动滚动到最后一行 m_EditReceive_Ctr这个是编辑框的控制变量
}






void CuartDlg::OnBnClickedCheck1()
{
	// TODO: 在此添加控件通知处理程序代码
	UpdateData(true);
}


BOOL CuartDlg::PreTranslateMessage(MSG* pMsg)
{
	//屏蔽ESC关闭窗体/
	/*
	if(pMsg->message==WM_KEYDOWN && pMsg->wParam==VK_ESCAPE ) 
		return TRUE;

	//屏蔽回车关闭窗体,但会导致回车在窗体上失效.
	//if(pMsg->message==WM_KEYDOWN && pMsg->wParam==VK_RETURN && pMsg->wParam) 
	//	return TRUE;
	else
		return CDialog::PreTranslateMessage(pMsg);

	*/

	if   (pMsg->message   ==   WM_KEYDOWN)   
	{   
		if   (pMsg->wParam==VK_RETURN|| pMsg->wParam==VK_ESCAPE)   
			return TRUE;//截获消息 
	}   
	return CDialog::PreTranslateMessage(pMsg);
}

void CuartDlg::OnBnClickedButtonRunPro()
{
	if(uart_opened==0)
	{
		debug_show(0,_T("请先打开串口."));
		m_reset_into_pmon_flag=0;
		UpdateData(false);
		return;
	}
	
	CString run_cmd;
	run_cmd=_T("user_load\ng\n");
	m_mscomm.put_Output(COleVariant(run_cmd));//发送数据

}

#define  BORAD_INTO_PMON_ANSWERED "PMON>"
#define  BORAD_AUTO_LOAD_ANSWERED "Press any other key to"  
//#define  BORAD_AUTO_LOAD_ANSWERED "Press any other key to"
bool reboot_thread_run_flag=0;
UINT get_into_load_mode(LPVOID pParam)//检查应答信号线程主函数 非类成员 创建线程的需要
{
	CuartDlg *pObj = (CuartDlg *)pParam;
	int counterxx=0;
	CString run_cmd;
	run_cmd=_T("\n");
	while(1)
	{
		run_cmd=_T("\n");
		pObj->m_mscomm.put_Output(COleVariant(run_cmd));//发送数据
		if(uart_received_cache.send_busy==0&&find_cmd_from_cache(&uart_received_cache,BORAD_INTO_PMON_ANSWERED,strlen(BORAD_INTO_PMON_ANSWERED)))//不忙检测命令 并且查找命令成功
		{
			pObj->debug_show(0,_T("\n准备重启中....."));
			break;
		}
		counterxx++;
		if(counterxx>100)
		{
			pObj->debug_show(0,_T("\n主板好像死循环或者死机，需要手动复位，若需要进入PMON，请勾选\"硬复位\"进入PMON模式."));
			reboot_thread_run_flag=0;
			pObj->clear_uart_recv_buf();
			return 0;
		}
	}
	run_cmd=_T("reboot\n");
	pObj->m_mscomm.put_Output(COleVariant(run_cmd));//发送重启命令

	run_cmd=_T(" ");
	while(reboot_thread_run_flag)//死循环检测从机串口发来的应答信号 若发现应答则发送消息给响应函数再次发送
	{
		if(pObj->m_reset_pmon_flag)//进入PMON模式
		{
			if(uart_received_cache.send_busy==0)
				pObj->m_mscomm.put_Output(COleVariant(run_cmd));//发送数据
			Sleep(1000);
			if(uart_received_cache.send_busy==0&&find_cmd_from_cache(&uart_received_cache,BORAD_INTO_PMON_ANSWERED,strlen(BORAD_INTO_PMON_ANSWERED)))//不忙检测命令 并且查找命令成功
			{
				reboot_thread_run_flag=0;
				pObj->debug_show(0,_T("\n重启完成，现在处于PMON模式"));
				//清空接收缓存
				pObj->clear_uart_recv_buf();
				break;
			}
		}
		else
		{
			Sleep(1000);
			if(uart_received_cache.send_busy==0&&find_str_from_cache(uart_received_cache.count_now-1024,uart_received_cache.rData,uart_received_cache.count_now,BORAD_AUTO_LOAD_ANSWERED,strlen(BORAD_AUTO_LOAD_ANSWERED)))//不忙检测命令 并且查找命令成功
			{
				reboot_thread_run_flag=0;
				pObj->debug_show(0,_T("\n重启完成.系统处于自动引导模式."));
				//清空接收缓存 这里其实还是存在风险 假如数据正好在间前后两次的中间怎么办
				pObj->clear_uart_recv_buf();
				break;
			}
		}
	}
	return 0;
}
void CuartDlg::OnBnClickedButtonBoardReset()
{
	if(uart_opened==0)
	{
		debug_show(0,_T("请先打开串口."));
		m_reset_into_pmon_flag=0;
		UpdateData(false);
		return;
	}
	CString run_cmd;
	run_cmd=_T("\n");
	m_mscomm.put_Output(COleVariant(run_cmd));//发送数据

	UpdateData(TRUE);
	if(m_reset_pmon_flag)
	{
		debug_show(0,_T("将进入引导模式."));
	}
	else
	{
		debug_show(0,_T("将进入自动启动模式."));
	}


	//这里需要创建一个线程用来检测主板是否重启成功
	CWinThread *m_pThread; //线程指针
	reboot_thread_run_flag=1;
	m_pThread = AfxBeginThread(get_into_load_mode,(LPVOID)this);//创建线程
}




#define HARD_RESET_INTO_PMON_CMD "        "
UINT hard_reset_get_into_pmon(LPVOID pParam)//检查应答信号线程主函数 非类成员 创建线程的需要
{
	CuartDlg *pObj = (CuartDlg *)pParam;
	CString run_cmd;
	run_cmd=char_to_cstring(HARD_RESET_INTO_PMON_CMD,strlen(HARD_RESET_INTO_PMON_CMD));
	while(pObj->m_reset_into_pmon_flag)
	{
		pObj->m_mscomm.put_Output(COleVariant(run_cmd));//发送数据
		if(uart_received_cache.send_busy==0&&find_cmd_from_cache(&uart_received_cache,HARD_RESET_INTO_PMON_CMD,strlen(HARD_RESET_INTO_PMON_CMD)))//不忙检测命令 并且查找命令成功
		{
			pObj->m_reset_into_pmon_flag=0;
			pObj->m_mscomm.put_Output(COleVariant(_T("\n\r\n")));
			//pObj->debug_show(0,_T("\r\n主板没有死机或者死循环，不能选中,若需要复位直接点击\"软复位\"按钮"));
			pObj->debug_show(0,_T("\r\n提示:主板处于PMON，不能选中\"硬复位\"进入PMON,若需要复位直接点击\"软复位\"按钮"));
			break;
		}
	}

	return 0;
}

void CuartDlg::OnBnClickedCheck3()
{
	// TODO: 在此添加控件通知处理程序代码
	UpdateData(true);//default_baudrate
	if(uart_opened==0)
	{
		debug_show(0,_T("请先打开串口."));
		m_reset_into_pmon_flag=0;
		UpdateData(false);
		return;
	}

	if(m_reset_into_pmon_flag)
	{
		get_into_pmon_mode_pThread=AfxBeginThread(hard_reset_get_into_pmon,(LPVOID)this);//创建线程
	}
	else
	{
		//debug_show(0,_T("000"));
	}

}


void CuartDlg::OnBnClickedButtonBoardReset3()
{
	if(uart_opened==0)
	{
		debug_show(0,_T("请先打开串口."));
		m_reset_into_pmon_flag=0;
		UpdateData(false);
		return;
	}
	m_mscomm.put_Output(COleVariant(_T("set user_pro yes\n")));
	debug_show(0,_T("重启后，程序将默认执行裸机程序."));
}


void CuartDlg::OnBnClickedButtonBoardReset4()
{
	// TODO: 在此添加控件通知处理程序代码
	if(uart_opened==0)
	{
		debug_show(0,_T("请先打开串口."));
		m_reset_into_pmon_flag=0;
		UpdateData(false);
		return;
	}
	m_mscomm.put_Output(COleVariant(_T("set user_pro no\n")));
	debug_show(0,_T("重启后，程序将进入默认引导模式，PMON设置正确的话，有可能引导linux."));
}



UINT get_board_arg_thread_run(LPVOID pParam)//检查应答信号线程主函数 非类成员 创建线程的需要
{
	CuartDlg *pObj = (CuartDlg *)pParam;
	while(pObj->get_board_arg_thread_sta)
	{
		if(uart_received_cache.send_busy==0&&find_cmd_from_cache(&uart_received_cache,pObj->check_borad_ret_str,strlen(pObj->check_borad_ret_str)))//不忙检测命令 并且查找命令成功
		{
			pObj->debug_show(0,_T("\r\n成功获取环境变量"));
			pObj->get_board_arg_thread_sta=0;
			break;
		}
	}

	return 0;
}

bool CuartDlg::get_board_arg(char *p)
{
	if(get_board_arg_thread_sta==1)
	{
		debug_show(0,_T("正在获取参数，稍等！"));
		return 0;
	}
	memset(get_board_arg_ret,0,512);
	get_board_arg_thread_sta=1;

	CString cs_send =char_to_cstring(p,strlen(p));
	m_mscomm.put_Output(COleVariant(cs_send));
	//m_mscomm.put_Output(COleVariant(p));//发送数据
	get_board_arg_thread=AfxBeginThread(get_board_arg_thread_run,(LPVOID)this);//创建线程
	return 1;
}

void CuartDlg::OnEnChangeMfceditbrowse1()
{
	// TODO:  如果该控件是 RICHEDIT 控件，它将不
	// 发送此通知，除非重写 CDialogEx::OnInitDialog()
	// 函数并调用 CRichEditCtrl().SetEventMask()，
	// 同时将 ENM_CHANGE 标志“或”运算到掩码中。

	// TODO:  在此添加控件通知处理程序代码
	update_uart_config_file(uart_config_file_name);
}


void CuartDlg::OnBnClickedButtonUpdataPmon()
{
	// TODO: 在此添加控件通知处理程序代码
	if(uart_opened==0)
	{
		debug_show(0,_T("请先打开串口."));
		return;
	}
	if(sendfile_to_ls1x_flag)
	{
		debug_show(0,_T("正在发送，等待上一次发送完成."));
		return;
	}

	UpdateData(true);
	CString pmon_file_path_cs=File_browse_path;
	CString show_warnning;
	show_warnning=show_warnning+_T("请确认当前路径是正确的、没有问题的PMON固件，一旦发送刷写出现问题主板将不能启动！！\r\n发送文件过程中可以打断关闭发送.一旦显示正在刷写请不要做任何操作.\r\n")
		+_T("当前要刷写的固件文件为:")+pmon_file_path_cs;


	if (MessageBox(show_warnning,_T("严重警告"), MB_OKCANCEL|MB_ICONWARNING) == IDOK)  
	{  
		if (MessageBox(show_warnning,_T("再次严重警告"), MB_OKCANCEL|MB_ICONWARNING) == IDOK)  
		{
			debug_show(0,_T("正在发送更新PMON."));
			send_pmon_flag=1;
			OnBnClickedButtonSend4();
		}
		else
		{
			debug_show(0,_T("取消发送更新PMON."));
			return;
		}
	} 
	else
	{
		debug_show(0,_T("取消发送更新PMON."));
		return;
	}

}


void CuartDlg::OnBnClickedButtonClearUserSpace()
{
	CString cs_send =_T("erase_user_space\n");
	m_mscomm.put_Output(COleVariant(cs_send));
}
