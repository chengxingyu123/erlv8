// ���� ifdef ���Ǵ���ʹ�� DLL �������򵥵�
// ��ı�׼�������� DLL �е������ļ��������������϶���� ERLV8_EXPORTS
// ���ű���ġ���ʹ�ô� DLL ��
// �κ�������Ŀ�ϲ�Ӧ����˷��š�������Դ�ļ��а������ļ����κ�������Ŀ���Ὣ
// ERLV8_API ������Ϊ�Ǵ� DLL ����ģ����� DLL ���ô˺궨���
// ������Ϊ�Ǳ������ġ�
#ifdef ERLV8_EXPORTS
#define ERLV8_API __declspec(dllexport)
#else
#define ERLV8_API __declspec(dllimport)
#endif

// �����Ǵ� erlv8.dll ������
class ERLV8_API Cerlv8 {
public:
	Cerlv8(void);
	// TODO: �ڴ�������ķ�����
};

extern ERLV8_API int nerlv8;

ERLV8_API int fnerlv8(void);
