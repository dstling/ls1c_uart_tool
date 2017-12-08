
// uartDlg.h : 头文件
//

#pragma once
#include "mscomm1.h"
#include "afxwin.h"
#include "afxcmn.h"
#include "crc16.h"
#define WM_SENDFILE_TO_LS1X_MESSAGE		WM_USER + 101

// CuartDlg 对话框
class CuartDlg : public CDialogEx
{
// 构造
public:
	CuartDlg(CWnd* pParent = NULL);	// 标准构造函数

// 对话框数据
	enum { IDD = IDD_UART_DIALOG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持

// 实现
protected:
	HICON m_hIcon;
	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
	DECLARE_EVENTSINK_MAP()

public:
	void Get_Parg(void);//获取参数
	void bin_to_char(char * dest_file_name,char * target_file_name);
	//void write_log_into_file(char * filename,CString data);

	LRESULT send_one_datas(WPARAM wParam,LPARAM lParam);//我的消息映射响应函数 WPARAM wParam,LPARAM lParam
	
	
	void debug_show(char leixing,CString show_what);//调试显示函数
	BOOL m_reset_into_pmon_flag;
	afx_msg void OnBnClickedCheck3();
	CWinThread *get_into_pmon_mode_pThread; //线程指针

	char get_board_arg_ret[512];
	char check_borad_ret_str[512];
	bool get_board_arg_thread_sta;//线程状态
	CWinThread *get_board_arg_thread; //线程指针
	bool get_board_arg(char *p);//获取主板环境变量


	CMscomm1 m_mscomm;
	CString m_EditSend;
	CString m_EditReceive;
	bool uart_opened;
	afx_msg void OnBnClickedButtonOpen();


	afx_msg void OnBnClickedButtonSend();
	//DECLARE_EVENTSINK_MAP()
	void OnCommMscomm1();
	void CuartDlg::clear_uart_recv_buf(void);//串口接收缓存清空
	CEdit m_EditReceive_Ctr;
	afx_msg void OnBnClickedButtonClear();
	
	//CString History_Cmd;
	//CEdit History_Cmd_Ctr;
	//afx_msg void OnEnChangeEdit4();

	CString Com_X;
	CString Baud_Rate;
	CString Data_Bit_Num;
	CString Data_Stop_Bit_Num;
	CString Data_Check_Bit;
	CString File_browse_path;


	afx_msg void OnBnClickedCheck1();
	afx_msg void OnBnClickedButtonSendfilestop();
	virtual BOOL PreTranslateMessage(MSG* pMsg);

	LRESULT sendfile_to_ls1x_one_datas(WPARAM wParam,LPARAM lParam);//我的消息映射响应函数 WPARAM wParam,LPARAM lParam
	afx_msg void OnBnClickedButtonSend4();
	bool send_pmon_flag;//发送的是PMON文件 小心啊

	int debug_show_lines;
	BOOL m_add_enter;
	afx_msg void OnBnClickedButtonRunPro();
	afx_msg void OnBnClickedButtonBoardReset();

	afx_msg void OnBnClickedButtonBoardReset3();
	afx_msg void OnBnClickedButtonBoardReset4();


	void uart_argc_get_from_config_file(char *filename);
	void update_uart_config_file(char *filename);
	afx_msg void OnEnChangeMfceditbrowse1();
	
	BOOL m_reset_pmon_flag;// 软件 硬件复位时，进入pmon标记

	// 发送文件进度条
	CProgressCtrl m_send_file_progress;
	UINT m_progress_start;
	UINT m_progress_end;
	UINT m_progress_step;
	afx_msg void OnBnClickedButtonUpdataPmon();
	afx_msg void OnBnClickedButtonClearUserSpace();
};

