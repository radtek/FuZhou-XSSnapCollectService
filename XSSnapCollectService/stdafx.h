// stdafx.h : ��׼ϵͳ�����ļ��İ����ļ���
// ���Ǿ���ʹ�õ��������ĵ�
// �ض�����Ŀ�İ����ļ�
//

#ifndef stdafx_h

#ifdef __WINDOWS__

#define WIN32_LEAN_AND_MEAN		// �� Windows ͷ���ų�����ʹ�õ�����
#include "targetver.h"

#include <stdio.h>
#include <tchar.h>
#include <Windows.h>

#pragma comment(lib, "HPSocket.lib")


#pragma warning(disable:4996)
#pragma warning(disable:4244)
#pragma warning(disable:4267)
#pragma warning(disable:4018)
#pragma warning(disable:4800)
#pragma warning(disable:4267)
#pragma warning(disable:4003)
#pragma warning(disable:4099)

#endif

#define stdafx_h
#endif