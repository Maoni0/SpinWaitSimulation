// Adopted from https://github.com/umezawatakeshi/GetLogicalProcessorInformationEx/blob/master/GetLogicalProcessorInformationEx.cc
#include <Windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


class GroupProcNo
{
	uint16_t m_groupProc;

public:

	static const uint16_t NoGroup = 0;

	GroupProcNo(uint16_t groupProc) : m_groupProc(groupProc)
	{
	}

	GroupProcNo(uint16_t group, uint16_t procIndex) : m_groupProc((group << 6) | procIndex)
	{
		assert(group <= 0x3ff);
		assert(procIndex <= 0x3f);
	}

	uint16_t GetGroup() { return m_groupProc >> 6; }
	uint16_t GetProcIndex() { return m_groupProc & 0x3f; }
	uint16_t GetCombinedValue() { return m_groupProc; }
};


void GetProcessorInfo(int* processorCount, int* processorGroupCount)
{
	*processorGroupCount = 1;
	*processorCount = 1;
	SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX* pBuffer;
	DWORD cbBuffer;
	char dummy[256] = {};

	pBuffer = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)dummy;
	cbBuffer = 1;
	if (GetLogicalProcessorInformationEx(RelationAll, pBuffer, &cbBuffer))
	{
		//printf("GetLogicalProcessorInformationEx returned nothing successfully.\n");
		*processorCount = 0;
		return;
	}
	if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
	{
		printf("GetLogicalProcessorInformationEx returned error (1). GetLastError() = %u\n", GetLastError());
		*processorCount = 1;
		return;
	}

	pBuffer = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)malloc(cbBuffer);
	if (!GetLogicalProcessorInformationEx(RelationAll, pBuffer, &cbBuffer))
	{
		printf("GetLogicalProcessorInformationEx returned error (2). GetLastError() = %u\n", GetLastError());
		*processorCount = 2;
		return;
	}

	//printf("GetLogicalProcessorInformationEx returned %u byte data.\n", cbBuffer);

	char* pCur = (char*)pBuffer;
	char* pEnd = pCur + cbBuffer;
	for (int idx = 0; pCur < pEnd; pCur += ((SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)pCur)->Size, ++idx)
	{
		pBuffer = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)pCur;
		if (pBuffer->Relationship == RelationGroup)
		{
			GROUP_RELATIONSHIP& info = pBuffer->Group;
			int activeGroupCount = info.ActiveGroupCount;
			*processorGroupCount = activeGroupCount;
			int activeProcessorCount = 0;
			for (int i = 0; i < activeGroupCount; i++)
			{
				PROCESSOR_GROUP_INFO& ginfo = info.GroupInfo[i];
				activeProcessorCount += ginfo.ActiveProcessorCount;
			}
			*processorCount = activeProcessorCount;
			return;
		}
	}
}

/// <summary>
/// Hard affinitize the threads to the processors.
/// </summary>
/// <param name="processorCount"></param>
/// <param name="isMultiCpuGroup"></param>
/// <param name="threadHandles"></param>
void SetThreadAffinity(int processorCount, bool isMultiCpuGroup, std::vector<HANDLE>& threadHandles)
{
	assert(threadHandles.size() == processorCount);

	for (int procNo = 0; procNo < processorCount; procNo++)
	{
		GroupProcNo groupProcNo(procNo);

		if (isMultiCpuGroup)
		{
			GROUP_AFFINITY ga;
			ga.Group = (WORD)groupProcNo.GetGroup();
			ga.Reserved[0] = 0; // reserve must be filled with zero
			ga.Reserved[1] = 0; // otherwise call may fail
			ga.Reserved[2] = 0;
			ga.Mask = (size_t)1 << groupProcNo.GetProcIndex();
			BOOL result = SetThreadGroupAffinity(threadHandles[procNo], &ga, nullptr);
			if (result == 0)
			{
				printf("SetThreadGroupAffinity returned 0 for processor %d. GetLastError() = %u\n", procNo, GetLastError());
				return;
			}
		}
		else
		{
			DWORD_PTR result = SetThreadAffinityMask(threadHandles[procNo], (DWORD_PTR)1 << groupProcNo.GetProcIndex());
			if (result == 0)
			{
				printf("SetThreadGroupAffinity returned 0 for processor %d. GetLastError() = %u\n", procNo, GetLastError());
				return;
			}
			else
			{
				//printf("affinitizing thread %d with index %d\n", procNo, groupProcNo.GetProcIndex());
			}
		}
	}
}