#include <algorithm>
#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <ctime>
using namespace std;

#define SIZE 1048576 //������ ����� 1 ��

/*
����� "����" � �������� �������� FAT16
_disk_pointer - ��������� �� ������ �����
_FAT1 - ��������� �� 1 ������� ���������� ������
_FAT2 - ��������� �� 2 ������� ���������� ������
_root_directory - ��������� �� �������� �������
_data_area - ��������� �� ������� ������
_place_indicator - ��������� �� ������� � ������� ��������� � �������� ������ (0 - �������� �������)
*/

class disk {
private:
	//��������� �������
	unsigned char* _disk_pointer;				// ����� 1 �����
	unsigned __int16* _FAT1;					// ����� 2 ������
	unsigned __int16* _FAT2;					// ����� 2 ������
	unsigned char* _root_directory;				// ����� 1 �����

	//������� ������
	unsigned char* _data_area;					// ����� 1 �����

	//�������������� ����������
	unsigned __int16 _place_indicator = 0;		//����� 2 ������ 

public:

	/*����������� ������ "����" (������������� �����)*/
	disk() {
		_disk_pointer = (unsigned char*)calloc(SIZE, sizeof(unsigned char));	//��������� ����� 1 ��
		_FAT1 = (unsigned __int16*)(_disk_pointer + 512);						//��������� FAT1 ������ � ����� ������������ �������
		_FAT2 = (unsigned __int16*)(_FAT1 + 256);								//��������� FAT2 ������ � ����� FAT1
		_root_directory = (unsigned char*)(_FAT2 + 256);						//��������� �� �������� ������� ������ � ����� FAT2
		_data_area = (unsigned char*)(_root_directory + 16384);					//��������� �� ������� ������ ������ � ����� ��������� ��������


		//���������� � FAT1 � FAT2 ���������� ������� � �����������
		*(_FAT1) = 0xFFF8;
		*(_FAT1 + 1) = 0xFFFF;
		*(_FAT2) = 0xFFF8;
		*(_FAT2 + 1) = 0xFFFF;

	}

	/*����������� 256 ����� FAT1 � FAT2 (��� ��� FAT2 ����� FAT1)*/
	void copy() {
		for (int i = 0; i < 256; i++)
			*(_FAT2 + i) = *(_FAT1 + i);
	}

	/*���������� ��������� �������� �� ��� � ����������*/
	void name_separation(string str, string& name, string& expansion) {

		if (str == ".." || str == ".") {
			name = "..";
			return;
		}

		bool f_flag = false;	//���� �����

		for (unsigned i = 0; i < str.size(); i++)
			if (str[i] == '.') {
				f_flag = true;
				break;
			}
			else name += str[i];

		if (f_flag)
			for (unsigned i = name.size() + 1; i < str.size(); i++)
				expansion += str[i];
	}

	/*��������� ���� ����������� ����������(�����) � ������������ �������*/
	bool compare_names(vector<unsigned char> name, unsigned char* pointer) {
		for (int i = 0; i < 11; i++)
			if (name[i] != *(pointer + i))
				return 1;

		return 0;
	}

	/*�������������� ����� � ������� �������*/
	int auto_fail_creater(unsigned __int16 cluster) {

		srand(time(NULL));
		int size_file = (rand() % rand() % 16384); // ������, �� 16384 ���� (16��)
		int copy_size = size_file;

		while (size_file > 0) {
			for (int i = 0; i < 2048 && i < size_file; i++)
				*(_data_area + i + (cluster - 2) * 2048) = (rand()%33 + 126);
			
			//�������� ����� ������� ������� 2048 ����, ���� ���� �� �������
			if (size_file - 2048 > 0) {
				size_file -= 2048;

				for (int i = 2; i < 512; i++)
					if (*(_FAT1 + i) == 0) {
						*(_FAT1 + i) = 0xffff; //�����, ��� ��� �������� � �������
						*(_FAT1 + cluster) = i;
						cluster = i;
						copy();					// �������� FAT1

						//������� ��������������� ������� � ������� ������
						for (int j = 0; j < 2048; j++)
							*(_data_area + (i - 2) * 2048 + j) = 0;

						break;
					}
					else
						return 0;


			}
			else return copy_size;
		}

		return copy_size;
	}

	/*������ ����������� �������� ��������*/
	void directory_files(vector<unsigned char*>& all_directory_files) {

		unsigned __int16 tmp = _place_indicator;		//��������� ��������� �� ������� (������� �������)
		bool flag_next = false;							//���� ������� �����������

		//������������� ������ � �������� ��������, ������� �������� � �� ���������
		if (_place_indicator == 0) {
			for (int i = 0; i < 16384; i += 32)
				if (*(_root_directory + i) != 0 && *(_root_directory + i) != 0xE5)
					all_directory_files.push_back(_root_directory + i);
		}
		else {
			do {
				//��������� 32 ������� ������ �������� ��������, ������� �������� � �� ���������
				for (int i = 0; i < 1024; i += 32)
					if ( *(_data_area + i + (tmp - 2) * 2048) != 0 && *(_data_area + i + (tmp - 2) * 2048) != 0xE5)
						all_directory_files.push_back(_data_area + i + (tmp - 2) * 2048);

				//��������� �� ������� FAT1 ���� �� ����������� � ���������� (������� �������� ������ �����������, ���� ����)
				if (*(_FAT1 + tmp) != 0xffff){
					tmp = *(_FAT1 + tmp);		//��������� ��������� �� ������� ����� �����������
					flag_next = true;			
				}
				else flag_next = false;

			} while (flag_next);
		}
	}

	/*���������� ����� ��� ������*/
	unsigned char* searh_free_space() {

		unsigned __int16 tmp = _place_indicator;	//��������� ��������� �� ������� (������� �������)
		bool flag_next = false;						//���� ������� �����������

		//���� ��������� � �������� ��������, ���������� ��������� �� ������ ����� ��� �� �������� ������
		if (_place_indicator == 0) {
			for (int i = 0; i < 16384; i += 32) {
				if (*(_root_directory + i) == 0)
					return _root_directory + i;

				if (*(_root_directory + i) == 0xE5) {
					for (int j = 0; j < 32; j++)
						*(_root_directory + i + j) = 0; //������ ��� � �������� ��������

					return _root_directory + i;	
				}
			}

			cout << "The disk is full! Delete unnecessary files." << endl;;
			Sleep(1500);
			return 0;
		}
		else {
			do {
				//���������� ��������� �� ������ ������ ��� �� �������� ������ � ������� ��������
				for (int i = 0; i < 2048; i += 32) {
					if (*(_data_area + i + (tmp - 2) * 2048) == 0 )
						return (_data_area + i + (tmp - 2) * 2048);

					if (*(_root_directory + i) == 0xE5)
						return (_data_area + i + (tmp - 2) * 2048);
				}

				//��������� �� ������� FAT1 ���� �� �����������
				if (*(_FAT1 + tmp) != 0xffff) {
					tmp = *(_FAT1 + tmp);		//��������� ��������� �� ������� ����� �����������
					flag_next = true;
				}
				else flag_next = false;

			} while (flag_next);

		}

		unsigned __int16 free_cluster = 0; //����� ���������� ��������

		//�������� �� ������� FAT1 � ���� ������ �������(���� �� ������� ���������� ����� ��� ������, �� ���� ������� ���������)
		for (int i = 2; i < 512; i++)
			if (*(_FAT1 + i) == 0) {
				*(_FAT1 + i) = 0xffff; //�����, ��� ��� �������� � �������
				copy(); // �������� FAT1
				free_cluster = i;

				//������� ��������������� ������� � ������� ������
				for (int j = 0; j < 2048; j++)
					*(_data_area + (i - 2) * 2048 + j) = 0;

				break;
			}

		//��������� ��������� ������� � �������, ����� ������� ��������������
		if (free_cluster != 0) {
			*(_FAT1 + tmp) = free_cluster;
			copy();

			//��������� ������ �� ������ ������ ��������
			return (_data_area + (free_cluster - 2) * 2048);

		}
		else {
			cout << "The disk is full! Delete unnecessary files." << endl;;
			Sleep(1500);
			return 0;
		}
		return 0;
	}

	/*�������� ����������*/
	int make_directory(vector<unsigned char> name) {

		unsigned char* free_space;					 //��������� ��� ������ ����� 
		vector<unsigned char*> all_directory_files;  //������ ���������� �� �������� ������
		directory_files(all_directory_files);	

		//��������� �� ������� �������� ��������
		for (unsigned i = 0; i < all_directory_files.size(); i++)
			if (!compare_names(name, all_directory_files[i])) {
				cout << "Directory already exists!" << endl; 
				return 0;
			}

		//��������� 32 ���� � �������� ��������
		free_space = searh_free_space();
		for (int i = 0; i < 32; i++)
			*(free_space + i) = 0;

		//���������� �����
		for (unsigned i = 0; i < 11; i++)
			*(free_space + i) = name[i];

		//������ ����� ��������
		*(free_space + 11) = 0x10;

		//���������� ���� ������� �������� ����������
		time_t date_create = time(NULL);
		*((int*)(free_space + 14)) = (int)date_create;

		//�������� �� ������� FAT1 � ���� ������ ������� ��� ������
		unsigned __int16 free_cluster = 0; //����� ���������� ��������
		unsigned char* tmp_free_cluster = 0; //��������� ����� ���������� ��������
		bool flag_not_free_clusters = false; //���� ������� ��������� ��� ������

		for (int i = 2; i < 512; i++)
			if (*(_FAT1 + i) == 0) {
				*(_FAT1 + i) = 0xffff; //�����, ��� ��� �������� � �������
				copy(); // �������� FAT1
				free_cluster = i;
				flag_not_free_clusters = true;

				//������� ��������������� ������� � ������� ������
				for (int j = 0; j < 2048; j++)
					*(_data_area + (i - 2) * 2048 + j) = 0;

				break;
			}
			
		if (!flag_not_free_clusters) return -1;  //�������, ���� ��� ��������� ���������

		//������� ������� ����� ������� ��������
		*((unsigned __int16*)(free_space + 26)) = free_cluster;

		//������ 0 ������ ��������
		*(free_space + 28) = 0x00;

		//����� � �������� ����� "." - ��� ������� � ".." - �������� �������
		tmp_free_cluster = _data_area + (free_cluster - 2) * 2048;

		for (int i = 0; i < 11; i++) *(tmp_free_cluster + i) = 0x20;		//������������� ���
		*tmp_free_cluster = 0x2e;											//��� "."
		*(tmp_free_cluster + 11) = 0x10;									//������ ����� ��������
		*((int*)(tmp_free_cluster + 14)) = (int)date_create;				//����� �������� ��������
		*((unsigned _int16*)(tmp_free_cluster + 26)) = free_cluster;		//����� ��������, � ������� ���������

		tmp_free_cluster += 32;
		for (int i = 0; i < 11; i++) *(tmp_free_cluster + i) = 0x20;		//������������� ���
		*tmp_free_cluster = 0x2e; *(tmp_free_cluster + 1) = 0x2e;			//��� ".."
		*(tmp_free_cluster + 11) = 0x10;									//������ ����� ��������
		*((int*)(tmp_free_cluster + 0xe)) = (int)date_create;		//����� �������� ��������
		*((unsigned short int*)(tmp_free_cluster + 26)) = _place_indicator; //����� �������� ��������, � ������� ���������

		return 1;

	}

	/*�������� �����*/
	int make_file(vector<unsigned char> name) {

		unsigned char* free_space;					 //��������� ��� ������ ����� 
		vector<unsigned char*> all_directory_files;  //������ ���������� �� �������� ������
		directory_files(all_directory_files);

		//��������� �� ������� �������� �����
		for (unsigned int i = 0; i < all_directory_files.size(); i++)
			if (!compare_names(name, all_directory_files[i])) {
				cout << "File already exists!" << endl;
				return 0;
			}

		//��������� 32 ���� � �������� ��������
		free_space = searh_free_space();
		for (int i = 0; i < 32; i++)
			*(free_space + i) = 0;

		//���������� �����
		for (unsigned i = 0; i < 11; i++)
			*(free_space + i) = name[i];

		//���������� ����� ��������
		*((int*)(free_space + 14)) = (int)time(NULL);

		//�������� �� ������� FAT1 � ���� ������ ������� ��� ������
		unsigned __int16 free_cluster = 0; //����� ���������� ��������
		bool flag_not_free_clusters = false; //���� ������� ��������� ��� ������

		for (int i = 2; i < 512; i++)
			if (*(_FAT1 + i) == 0) {
				*(_FAT1 + i) = 0xffff; //�����, ��� ��� �������� � �������
				copy();		// �������� FAT1
				free_cluster = i;
				flag_not_free_clusters = true;

				//������� ��������������� ������� � ������� ������
				for (int j = 0; j < 2048; j++)
					*(_data_area + (i - 2) * 2048 + j) = 0;

				break;
			}
			
		if (!flag_not_free_clusters) return -1; //�������, ���� ��� ��������� ���������

		//������� ������� ����� ������� ��������
		*((unsigned __int16*)(free_space + 26)) = free_cluster;

		//���������� ������� �����
		*((int*)(free_space + 28)) = auto_fail_creater(free_cluster);

		return 1;
	}

	/*�������� ����� ��� ����������*/
	void make_a_directory_or_file(string str) {
		string n;						//��� �����/���������� 
		string e;						//���������� �����
		bool f_file = false;			//���� �����

		name_separation(str, n, e); // ���������� ����� �� ������������

		//����� ����, ���� ����������� ������ ����
		if (e.size() > 0)
			f_file = true;

		vector<unsigned char> name;

		//�������� ����� (�����, ����������)
		for (int i = 0; i < 11; i++) 
			name.push_back(0x20);
		
		if (f_file) {
			for (unsigned i = 0; i < n.size(); i++)
				if(i < 8) 
					name[i] = n[i];
			for (int i = 0; i < 3 && i != e.size(); i++)
				name[i + 8] = e[i];
		}
		else {
			for (unsigned i = 0; i < n.size(); i++)
				if (i < 8)
					name[i] = n[i];
		}

		//�������� ����� ��� ����������
		if (f_file == true) {
			if (make_file(name) != 1) {
				cout << "Error operation!" << endl;
				Sleep(2000);
			}
		}
		else {
			if (make_directory(name) != 1) {
				cout << "Error operation!" << endl;
				Sleep(2000);
			}
		}

	}

	/*������� � ������ ����������*/
	int open_directory(vector<unsigned char> name) {
		bool f_find = false;						 //���� ������
		unsigned char* find_direction_or_file;		 //���������� ���� ��� ����������
		vector<unsigned char*> all_directory_files;  //������ ���������� �� �������� ������
		directory_files(all_directory_files);

		//��������� �� ������� �������� ��������
		for (unsigned i = 0; i < all_directory_files.size(); i++)
			if (!compare_names(name, all_directory_files[i])) {
				find_direction_or_file = all_directory_files[i];
				f_find = true;  break;
			}

		if (!f_find) return -1;

		_place_indicator = *((unsigned __int16*)(find_direction_or_file + 26)); //�������� ������� �������������� (������� �������)

		return 1;

	}

	/*�������� �����*/
	int open_file(vector<unsigned char> name) {
		do {
			bool f_find = false;						 //���� ������
			unsigned char* find_direction_or_file;		 //���������� ���� ��� ����������
			vector<unsigned char*> all_directory_files;  //������ ���������� �� �������� ������
			directory_files(all_directory_files);

			//��������� �� ������� �������� ��������
			for (unsigned i = 0; i < all_directory_files.size(); i++)
				if (!compare_names(name, all_directory_files[i])) {
					find_direction_or_file = all_directory_files[i];
					f_find = true;  break;
				}

			if (!f_find) return -1; //���� �� �����, �������

			int file_size = *((int*)(find_direction_or_file + 28)); // ������ �����
			unsigned __int16 first_cluster = *((unsigned __int16*)(find_direction_or_file + 26)); //������ ������� � �������

			//������� ����
			system("cls");

			//������� ���
			cout << "File: ";
			for (int j = 0; j < 8 && *(find_direction_or_file + j) != 0x20; j++)
				cout << *(find_direction_or_file + j);

			cout << ".";
			for (int j = 0; j < 3 && *(find_direction_or_file + j + 8) != 0x20; j++)
				cout << *(find_direction_or_file + j + 8);

			cout << endl;

			while (file_size >= 0) {
				for (int i = 0; i < 2048; i++)
					if (i < file_size)
						cout << (_data_area + i + (first_cluster - 2) * 2048);
					else
						break;

				file_size -= 2048;

				if (file_size >= 0)
					first_cluster = *(_FAT1 + first_cluster);
			}
			cout << endl;

		} while (getchar() != 'e');
	
		return 1;
	}

	/*�������� ���������� ��� �����*/
	int open_a_directory_or_file(string str){
		string n;						//��� �����/���������� 
		string e;						//���������� �����
		bool f_file = false;			//���� �����

		name_separation(str, n, e); // ���������� ����� �� ������������

									//����� ����, ���� ����������� ������ ����
		if (e.size() > 0)
			f_file = true;

		vector<unsigned char> name;

		//�������� ����� (�����, ����������)
		for (int i = 0; i < 11; i++)
			name.push_back(0x20);

		if (f_file) {
			for (unsigned i = 0; i < n.size(); i++)
				if (i < 8)
					name[i] = n[i];
			for (int i = 0; i < 3 && i != e.size(); i++)
				name[i + 8] = e[i];
		}
		else {
			for (unsigned i = 0; i < n.size(); i++)
				if (i < 8)
					name[i] = n[i];
		}

		if (n == ".") {
			return -1;
		}

		//�������� ����� ��� ����������
		if (n == ".." ) {
			if (open_directory(name) != 1) {
				cout << "Error operation!" << endl;
				Sleep(2000);
				return -1;
			}
			else
				return 0;
		}
		else {
			if (f_file == true) {
				if (open_file(name) != 1) {
					cout << "Error operation!" << endl;
					Sleep(2000);
					return -1;
				}
				else
					return 0;
			}
			else {
				if (open_directory(name) != 1) {
					cout << "Error operation!" << endl;
					Sleep(2000);
					return -1;
				}
				else
					return 1;
			}
		}
	}

	/*�������� ���������� ��� �����*/
	void delete_a_directory_or_file(string str) {
		string n;						//��� �����/���������� 
		string e;						//���������� �����

		bool f_file = false;			//���� �����
		bool f_find = false;			//���� ������

		unsigned char* find_direction_or_file;		 //���������� ���� ��� ����������
		vector<unsigned char*> all_directory_files;  //������ ���������� �� �������� ������
		directory_files(all_directory_files);

		name_separation(str, n, e); // ���������� ����� �� ������������

		//����� ����, ���� ����������� ������ ����
		if (e.size() > 0)
			f_file = true;

		vector<unsigned char> name;

		//�������� ����� (�����, ����������)
		for (int i = 0; i < 11; i++)
			name.push_back(0x20);

		if (f_file) {
			for (unsigned i = 0; i < n.size(); i++)
				if (i < 8)
					name[i] = n[i];
			for (int i = 0; i < 3 && i != e.size(); i++)
				name[i + 8] = e[i];
		}
		else {
			for (unsigned i = 0; i < n.size(); i++)
				if (i < 8)
					name[i] = n[i];
		}

		//�������� ����� ��� ����������
		if (n == ".." || n == ".") {
			cout << "The file to be deleted is the system file!" << endl;
			Sleep(1500);
			return;
		}
		else {
			//��������� �� ������� �������� �������� ��� �����
			for (unsigned i = 0; i < all_directory_files.size(); i++)
				if (!compare_names(name, all_directory_files[i])) {
					find_direction_or_file = all_directory_files[i];
					f_find = true;
					break;
				}

			// ������� ���� ���� �� ������
			if (!f_find) {
				cout << "Directory (file) for deletion not found!" << endl;
					Sleep(1500);
					return;
			}

		DO:
			//�������������, �� ����� �� ��������� ��������� �����
			if (*find_direction_or_file == '.') {
				cout << "The file to be deleted is the system file!" << endl;
				Sleep(1500);
			}
				
			//��������� �� ������ �� ���� �����, ����� �������
			if (*find_direction_or_file != 0xE5 )
				*find_direction_or_file = 0xE5;
			else return;

			//������ ���� �� ����, ���� ��������� ������ ����������
			if (*(find_direction_or_file + 11) == 0x10 || *(find_direction_or_file + 11) == 0x04)
				f_file = false;

			unsigned __int16 cluster = *((unsigned __int16*)(find_direction_or_file + 26));
			goto DL;

		DL:
			//���� ����������, �� ��������� �� �������� ������� � ��������
			if (!f_file) {
				for (int i = 0; i < 2048; i += 32) {
					cluster = *((unsigned char*)(_data_area + i + (cluster - 2) * 2048));
					goto DO;
				}
			}

			//���� ���� ��������� ������� �� ������� �������
			if (*(_FAT1 + cluster) != 0xFFFF) {
				cluster = *(_FAT1 + cluster);
				goto DL;
			}

			*(_FAT1 + cluster) = 0;
			copy();

		}
		
	}

	/*����� �������� ���������� ��� ����� ��� ����������*/
	void hide_show_the_directory_or_file(string str, bool operation) {
		string n;						//��� �����/���������� 
		string e;						//���������� �����

		bool f_file = false;			//���� �����
		bool f_find = false;			//���� ������

		unsigned char* find_direction_or_file;		 //���������� ���� ��� ����������
		vector<unsigned char*> all_directory_files;  //������ ���������� �� �������� ������
		directory_files(all_directory_files);

		name_separation(str, n, e); // ���������� ����� �� ������������

		//����� ����, ���� ����������� ������ ����
		if (e.size() > 0)
			f_file = true;

		vector<unsigned char> name;

		//�������� ����� (�����, ����������)
		for (int i = 0; i < 11; i++)
			name.push_back(0x20);

		if (f_file) {
			for (unsigned i = 0; i < n.size(); i++)
				if (i < 8)
					name[i] = n[i];
			for (int i = 0; i < 3 && i != e.size(); i++)
				name[i + 8] = e[i];
		}
		else {
			for (unsigned i = 0; i < n.size(); i++)
				if (i < 8)
					name[i] = n[i];
		}

		//��������� �� ������� �������� �������� ��� �����
		for (unsigned i = 0; i < all_directory_files.size(); i++)
			if (!compare_names(name, all_directory_files[i])) {
				find_direction_or_file = all_directory_files[i];
				f_find = true;
				break;
			}

		// ������� ���� ���� �� ������
		if (!f_find ) {
			cout << "Directory (file) for attribute change not found!" << endl;
			Sleep(1500);
			return;
		}

		//�������� ����
		if(f_file && operation ){
			*(find_direction_or_file + 11) = 0x01;
			return;
		}

		//���������� ����
		if (f_file && !operation) {
			*(find_direction_or_file + 11) = 0x00;
			return;
		}

		//�������� ����������
		if (!f_file && operation && (n != "." || n != "..")) {
			*(find_direction_or_file + 11) = 0x04;
			return;
		}

		//���������� ����������
		if (!f_file && !operation && (n != "." || n != "..")) {
			*(find_direction_or_file + 11) = 0x10;
			return;
		}

	}

	/*����� �� �����*/
	void display_file_manager(vector<string> way) {
		system("cls");

		//������� ����� ������������ ����������
		cout << (char)0xC9;
		for (int i = 0; i < 54; i++) cout << (char)0xCD;
		cout << (char)0xBB;
		cout << endl;

		cout << (char)0xBA << "                  File manager FAT16                  " << (char)0xBA << endl;

		cout << (char)0xCC;
		for (int i = 0; i < 54; i++) cout << (char)0xCD;
		cout << (char)0xB9;
		cout << endl;

		vector<unsigned char*> all_directory_files;  //������ ���������� �� �������� ������
		directory_files(all_directory_files);

		//������������� ���������� �������
		if (all_directory_files.size() == 0)
			cout << (char)0xBA << "                  The disk is empty.                  " << (char)0xBA << endl;
		else
			//������� ��� �����, ����� �������
			for (unsigned i = 0; i < all_directory_files.size(); i++) {
				int name_size = 0;
				int size_digits = 0;

				//���������� ����� ���������� ��� �����, ���� �� �����
				if (*(all_directory_files[i] + 11) == 0x01 || *(all_directory_files[i] + 11) == 0x04 ) continue;

				cout << (char)0xBA << " "; //��������� �������

				//������� ��� ��� ��������
				for (int j = 0; j < 8 && *(all_directory_files[i] + j) != 0x20; j++) {
					cout << *(all_directory_files[i] + j);
					name_size++;
				}

				//���� ��� ����
				if (*(all_directory_files[i] + 8) != 0x20) {
					cout << ".";
					name_size++;
					for (int j = 0; j < 3 && *(all_directory_files[i] + j + 8) != 0x20; j++) {
						cout << *(all_directory_files[i] + j + 8);
						name_size++;
					}
				}

				//��������� ��������� �� ����� 12 (����� ��� ��������� ������)
				for (int j = name_size; j <= 12; j++)
					cout << " ";

				//������� ����� ��������
				char* str = asctime(localtime(&*(time_t*)(all_directory_files[i] + 14)));
				cout << "   ";
				for (int j = 0; *(str + j) != '\n'; j++)
					cout << *(str + j);

				//������� ������		
				cout << " " << *((int*)(all_directory_files[i] + 28)) << " bytes ";
				CONSOLE_SCREEN_BUFFER_INFO bi;
				COORD coord;
				GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &bi);
				coord.X = 55;
				coord.Y = bi.dwCursorPosition.Y;
				SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
				
				cout << (char)0xBA << endl;

			}

		cout << (char)0xC8;
		for (int i = 0; i < 54; i++) cout << (char)0xCD;
		cout << (char)0xBC;
		cout << endl << endl;
		
		for (unsigned i = 0; i < way.size(); i++)
			cout << way[i];
		cout << ">";

	}

	/*�������� ��������*/
	void file_manager(disk& C) {
		vector<string> way;
		way.push_back("C:/");

		while(1) {
			display_file_manager(way);
			string str;
			cin >> str;

			/*�������� �����/����������*/
			if (str == "make") {
				string name;
				cin >> name;
				make_a_directory_or_file(name);
			}

			if (str == "open") {
				string name;
				cin >> name;

				if (name == ".." || name == ".") {
					if (open_a_directory_or_file(name) == 0)
						way.pop_back();
				}
				else
					if (open_a_directory_or_file(name) == 1)
						way.push_back(name + "/");
						
			}

			if (str == "delete") {
				string name;
				cin >> name;
				delete_a_directory_or_file(name);
			}

			if (str == "hide") {
				string name;
				cin >> name;
				hide_show_the_directory_or_file(name, true);
			}

			if (str == "show") {
				string name;
				cin >> name;
				hide_show_the_directory_or_file(name, false);
			}

			/*����� �� ���������*/
			if (str == "close")
				break;

		}
	}

};

int main() {
	disk C;
	C.file_manager(C);
	return 0;

}