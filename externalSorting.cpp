#include <fstream>
#include <iostream>
#include <iomanip>
#include <conio.h>
#include <Windows.h>
#include <ctime>
#include <string>
#define MEMSZ 16
using namespace std;

unsigned string_count = 0;

struct merge_pair {
	unsigned elem, str;
	merge_pair() {}
	merge_pair(unsigned e, unsigned st = 99) {
		elem = e; str = st;
	}
	bool operator<(const merge_pair& another) const {
		return elem < another.elem;
	}
	bool operator<=(const merge_pair& another) const {
		return elem <= another.elem;
	}
};

ostream& operator<<(ostream& os, const merge_pair& mp) {
	os << mp.elem;
	return os;
}

template<typename T>
class heap {
	T *begin, *end;
	unsigned size;
	T left(unsigned idx) {
		unsigned res = 2 * idx + 1;
		if (begin + res >= end) return T(RAND_MAX + 1);
		else return begin[res];
	}
	T right(unsigned idx) {
		unsigned res = 2 * idx + 2;
		if (begin + res >= end) return T(RAND_MAX + 1);
		else return begin[res];
	}
	void heapify(unsigned posit) {
		if (begin[posit] <= left(posit) && begin[posit] <= right(posit));
		else {
			if (left(posit) < right(posit)) {
				swap(begin[posit], begin[2 * posit + 1]);
				heapify(2 * posit + 1);
			}
			else {
				swap(begin[posit], begin[2 * posit + 2]);
				heapify(2 * posit + 2);
			}
		}
	}
	void all_heapify(unsigned last_idx) {
		if (last_idx == 0) last_idx = (size - 2) / 2;
		end = begin + size;
		for (int i = last_idx; i >= 0; i--)
			heapify(i);
	}
public:
	heap(unsigned sz = MEMSZ) {
		size = sz;
		begin = new T[size];
		end = begin + size;
		for (unsigned* i = begin; i < end; i++)
			*i = T(RAND_MAX + 1);
	}
	heap(ifstream* ifs) {
		size = string_count;
		begin = new T[size];
		end = begin + size;
		for (unsigned i = 0; i < string_count; i++) {
			unsigned tmp; ifs[i] >> tmp;
			begin[i] = T(tmp, i);
		}
		all_heapify(0);
	}
	~heap() {
		delete[] begin;
		begin = end = NULL;
	}
	T pop_push(unsigned num) {
		T tnum(num);
		T res = *begin;
		if (tnum > res) *begin = tnum;
		else {
			end--;
			*begin = *end;
			*end = tnum;
			if (end == begin) {
				end = begin + size;
				all_heapify(0);
			}
		}
		heapify(0);
		return res;
	}
	void out() {
		all_heapify(0); string_count++;
		ofstream file(to_string(string_count) + ".txt");
		for (unsigned i = 0; i < size; i++) {
			T res = pop_push(RAND_MAX + 1);
			file << ' ' << res;
			cout << res << endl;
		}
		file.close();
	}
	void merge(ifstream* ifs) {
		ofstream file("output.txt");
		for (unsigned cnt = 1; begin->elem <= RAND_MAX; cnt++) {
			file << setw(6) << *begin;
			if (cnt % MEMSZ == 0) file << endl;
			if (!ifs[begin->str].eof()) ifs[begin->str] >> begin->elem;
			else begin->elem = RAND_MAX + 1;
			heapify(0);
		}
		file.close();
	}
};

void init() {
	srand(time(0));
	ofstream file("input.txt");
	for (unsigned i = 0; i < MEMSZ; i++) {
		for (unsigned j = 0; j < MEMSZ; j++)
			file << setw(6) << rand();
		file << endl;
	}
	file.close();
	cout << "初始化完毕！" << endl;
}

CRITICAL_SECTION cs_read, cs_write;
HANDLE event_ifull, event_iempty, event_ofull, event_oempty;
HANDLE thread_read_file, thread_mem_deal, thread_write_file;
unsigned ibuf[MEMSZ], obuf[MEMSZ];
heap<unsigned> hbuf;

void read_from_file() {
	ifstream file("input.txt"); unsigned* posit = ibuf;
	while (!file.eof()) {
		WaitForSingleObject(event_iempty, INFINITE);
		cout << "读文件进程启动！";
		if (posit - ibuf == MEMSZ) {
			SetEvent(event_ifull);
			ResetEvent(event_iempty);
			posit = ibuf;
			cout << endl;
		}
		else {
			EnterCriticalSection(&cs_read);
			file >> *posit;
			cout << *posit << endl;
			posit++;
			LeaveCriticalSection(&cs_read);
		}
	}
	file.close();
	cout << "读文件进程结束！" << endl;
}

void mem_deal() {
	unsigned i = 0;
	while (WaitForSingleObject(event_ifull, 500) == WAIT_OBJECT_0 &&
		WaitForSingleObject(event_oempty, 500) == WAIT_OBJECT_0) {
		cout << "处理内存进程启动！";
		if (i == MEMSZ) {
			SetEvent(event_iempty); SetEvent(event_ofull);
			ResetEvent(event_ifull); ResetEvent(event_oempty);
			i = 0; cout << endl;
		}
		else {
			EnterCriticalSection(&cs_read);
			EnterCriticalSection(&cs_write);
			unsigned temp = hbuf.pop_push(ibuf[i]);
			obuf[i] = temp;
			cout << obuf[i] << endl;
			i++;
			LeaveCriticalSection(&cs_write);
			LeaveCriticalSection(&cs_read);	
		}
	}
	cout << "处理内存进程结束！" << endl;
}

void write_to_file() {
	unsigned* posit = obuf;
	unsigned last = 0;
	ofstream file(to_string(string_count) + ".txt", ios::app);
	while (WaitForSingleObject(event_ofull, 500) == WAIT_OBJECT_0) {
		cout << "写文件进程启动！";
		if (posit - obuf == MEMSZ) {
			SetEvent(event_oempty);
			ResetEvent(event_ofull);
			posit = obuf; 
			cout << endl;
		}
		else {
			EnterCriticalSection(&cs_write);
			if (*posit <= RAND_MAX) {
				if (*posit >= last) {
					file << ' ' << *posit;
					cout << *posit << endl;
				}
				else {
					file.close();
					string_count++;
					file = ofstream(to_string(string_count) + ".txt", ios::app);
					file << ' ' << *posit;
					cout << *posit << endl;
				}
				last = *posit;
			}
			posit++;
			LeaveCriticalSection(&cs_write);	
		}
	}
	file.close();
	cout << "写文件进程结束！" << endl;
}

int main()
{
	cout << "数据源：input.txt" << endl;
	cout << "是否初始化数据？按Y是，按N否，按其它退出程序：";
	char t = _getche(); cout << endl;
	switch (t) {
		case 'Y': init(); break;
		case 'N': break;
		default: exit(0);
	}

	InitializeCriticalSection(&cs_read);
	InitializeCriticalSection(&cs_write);
	event_iempty = CreateEvent(NULL, true, true, NULL);
	event_ifull = CreateEvent(NULL, true, false, NULL);
	event_oempty = CreateEvent(NULL, true, true, NULL);
	event_ofull = CreateEvent(NULL, true, false, NULL);

	thread_read_file = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) read_from_file, NULL, 0, false);
	thread_mem_deal = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) mem_deal, NULL, 0, false);
	thread_write_file = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) write_to_file, NULL, 0, false);

	WaitForSingleObject(thread_write_file, INFINITE);
	hbuf.out();

	cout << "顺串产生完毕（0、1、2... .txt）！开始归并..." << endl;

	string_count++;
	ifstream* strings = new ifstream[string_count];
	for (unsigned i = 0; i < string_count; i++)
		strings[i] = ifstream(to_string(i) + ".txt");
	heap<merge_pair> merge_heap(strings);
	merge_heap.merge(strings);
	cout << "归并结束！请在output.txt中检查结果..." << endl;
	system("pause");
	return 0;
}
