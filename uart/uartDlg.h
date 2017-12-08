
// uartDlg.h : ͷ�ļ�
//

#pragma once
#include "mscomm1.h"
#include "afxwin.h"
#include "afxcmn.h"
#include "crc16.h"
#define WM_SENDFILE_TO_LS1X_MESSAGE		WM_USER + 101

// CuartDlg �Ի���
class CuartDlg : public CDialogEx
{
// ����
public:
	CuartDlg(CWnd* pParent = NULL);	// ��׼���캯��

// �Ի�������
	enum { IDD = IDD_UART_DIALOG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV ֧��

// ʵ��
protected:
	HICON m_hIcon;
	// ���ɵ���Ϣӳ�亯��
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
	DECLARE_EVENTSINK_MAP()

public:
	void Get_Parg(void);//��ȡ����
	void bin_to_char(char * dest_file_name,char * target_file_name);
	//void write_log_into_file(char * filename,CString data);

	LRESULT send_one_datas(WPARAM wParam,LPARAM lParam);//�ҵ���Ϣӳ����Ӧ���� WPARAM wParam,LPARAM lParam
	
	
	void debug_show(char leixing,CString show_what);//������ʾ����
	BOOL m_reset_into_pmon_flag;
	afx_msg void OnBnClickedCheck3();
	CWinThread *get_into_pmon_mode_pThread; //�߳�ָ��

	char get_board_arg_ret[512];
	char check_borad_ret_str[512];
	bool get_board_arg_thread_sta;//�߳�״̬
	CWinThread *get_board_arg_thread; //�߳�ָ��
	bool get_board_arg(char *p);//��ȡ���廷������


	CMscomm1 m_mscomm;
	CString m_EditSend;
	CString m_EditReceive;
	bool uart_opened;
	afx_msg void OnBnClickedButtonOpen();


	afx_msg void OnBnClickedButtonSend();
	//DECLARE_EVENTSINK_MAP()
	void OnCommMscomm1();
	void CuartDlg::clear_uart_recv_buf(void);//���ڽ��ջ������
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

	LRESULT sendfile_to_ls1x_one_datas(WPARAM wParam,LPARAM lParam);//�ҵ���Ϣӳ����Ӧ���� WPARAM wParam,LPARAM lParam
	afx_msg void OnBnClickedButtonSend4();
	bool send_pmon_flag;//���͵���PMON�ļ� С�İ�

	int debug_show_lines;
	BOOL m_add_enter;
	afx_msg void OnBnClickedButtonRunPro();
	afx_msg void OnBnClickedButtonBoardReset();

	afx_msg void OnBnClickedButtonBoardReset3();
	afx_msg void OnBnClickedButtonBoardReset4();


	void uart_argc_get_from_config_file(char *filename);
	void update_uart_config_file(char *filename);
	afx_msg void OnEnChangeMfceditbrowse1();
	
	BOOL m_reset_pmon_flag;// ��� Ӳ����λʱ������pmon���

	// �����ļ�������
	CProgressCtrl m_send_file_progress;
	UINT m_progress_start;
	UINT m_progress_end;
	UINT m_progress_step;
	afx_msg void OnBnClickedButtonUpdataPmon();
	afx_msg void OnBnClickedButtonClearUserSpace();
};

