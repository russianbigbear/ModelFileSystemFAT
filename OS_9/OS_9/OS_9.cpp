#include <algorithm>
#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <ctime>
using namespace std;

#define SIZE 1048576 //размер диска 1 Мб

/*
Класс "Диск" с файловой системой FAT16
_disk_pointer - указатель на начало диска
_FAT1 - указатель на 1 таблицу размещения файлов
_FAT2 - указатель на 2 таблицу размещения файлов
_root_directory - указатель на корневой каталог
_data_area - указатель на область данных
_place_indicator - указатель на кластер в котором находимся в процессе работы (0 - корневой каталог)
*/

class disk {
private:
	//системная область
	unsigned char* _disk_pointer;				// равно 1 байту
	unsigned __int16* _FAT1;					// равно 2 байтам
	unsigned __int16* _FAT2;					// равно 2 байтам
	unsigned char* _root_directory;				// равно 1 байту

	//область данных
	unsigned char* _data_area;					// равно 1 байту

	//дополнительный переменная
	unsigned __int16 _place_indicator = 0;		//равен 2 байтам 

public:

	/*конструктор класса "Диск" (инициализация диска)*/
	disk() {
		_disk_pointer = (unsigned char*)calloc(SIZE, sizeof(unsigned char));	//выделение диску 1 Мб
		_FAT1 = (unsigned __int16*)(_disk_pointer + 512);						//указатель FAT1 ставим в конце загрузочного сектора
		_FAT2 = (unsigned __int16*)(_FAT1 + 256);								//указатель FAT2 ставим в конце FAT1
		_root_directory = (unsigned char*)(_FAT2 + 256);						//указатель на корневой каталог ставим в конце FAT2
		_data_area = (unsigned char*)(_root_directory + 16384);					//указатель на область данных ставим в конце корневого каталога


		//записываем в FAT1 и FAT2 дескриптор таблицы и заполнитель
		*(_FAT1) = 0xFFF8;
		*(_FAT1 + 1) = 0xFFFF;
		*(_FAT2) = 0xFFF8;
		*(_FAT2 + 1) = 0xFFFF;

	}

	/*копирование 256 строк FAT1 в FAT2 (так как FAT2 копия FAT1)*/
	void copy() {
		for (int i = 0; i < 256; i++)
			*(_FAT2 + i) = *(_FAT1 + i);
	}

	/*разложение введеного названия на имя и расширение*/
	void name_separation(string str, string& name, string& expansion) {

		if (str == ".." || str == ".") {
			name = "..";
			return;
		}

		bool f_flag = false;	//флаг файла

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

	/*сравнение имен создаваемой директории(файла) и существующих записей*/
	bool compare_names(vector<unsigned char> name, unsigned char* pointer) {
		for (int i = 0; i < 11; i++)
			if (name[i] != *(pointer + i))
				return 1;

		return 0;
	}

	/*автозаполнение файла и возврат размера*/
	int auto_fail_creater(unsigned __int16 cluster) {

		srand(time(NULL));
		int size_file = (rand() % rand() % 16384); // размер, до 16384 байт (16Кб)
		int copy_size = size_file;

		while (size_file > 0) {
			for (int i = 0; i < 2048 && i < size_file; i++)
				*(_data_area + i + (cluster - 2) * 2048) = (rand()%33 + 126);
			
			//выделяем новый кластер объёмом 2048 байт, если файл не записан
			if (size_file - 2048 > 0) {
				size_file -= 2048;

				for (int i = 2; i < 512; i++)
					if (*(_FAT1 + i) == 0) {
						*(_FAT1 + i) = 0xffff; //пишем, что она конечная в цепочке
						*(_FAT1 + cluster) = i;
						cluster = i;
						copy();					// копируем FAT1

						//Очищаем соответствующий кластер в области данных
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

	/*запись содержимого текущего каталога*/
	void directory_files(vector<unsigned char*>& all_directory_files) {

		unsigned __int16 tmp = _place_indicator;		//временный указатель на позицию (текущий кластер)
		bool flag_next = false;							//флаг наличия продолжения

		//просматриваем записи в корневом каталоге, заносим непустые и не удаленные
		if (_place_indicator == 0) {
			for (int i = 0; i < 16384; i += 32)
				if (*(_root_directory + i) != 0 && *(_root_directory + i) != 0xE5)
					all_directory_files.push_back(_root_directory + i);
		}
		else {
			do {
				//Проверяем 32 байтные записи текущего каталога, заносим непустые и не удаленные
				for (int i = 0; i < 1024; i += 32)
					if ( *(_data_area + i + (tmp - 2) * 2048) != 0 && *(_data_area + i + (tmp - 2) * 2048) != 0xE5)
						all_directory_files.push_back(_data_area + i + (tmp - 2) * 2048);

				//Проверяем по таблице FAT1 есть ли продолжение у директории (смотрим непустые записи продолжения, если есть)
				if (*(_FAT1 + tmp) != 0xffff){
					tmp = *(_FAT1 + tmp);		//временный указатель на позицию равен продолжению
					flag_next = true;			
				}
				else flag_next = false;

			} while (flag_next);
		}
	}

	/*нахождение места для записи*/
	unsigned char* searh_free_space() {

		unsigned __int16 tmp = _place_indicator;	//временный указатель на позицию (текущий кластер)
		bool flag_next = false;						//флаг наличия продолжения

		//Если находимся в корневом каталоге, возвращаем указатель на пустое место или на удаленую запись
		if (_place_indicator == 0) {
			for (int i = 0; i < 16384; i += 32) {
				if (*(_root_directory + i) == 0)
					return _root_directory + i;

				if (*(_root_directory + i) == 0xE5) {
					for (int j = 0; j < 32; j++)
						*(_root_directory + i + j) = 0; //чистим имя в корневом каталоге

					return _root_directory + i;	
				}
			}

			cout << "The disk is full! Delete unnecessary files." << endl;;
			Sleep(1500);
			return 0;
		}
		else {
			do {
				//возвращаем указатель на пустую запись или на удаленую запись в текущем кластере
				for (int i = 0; i < 2048; i += 32) {
					if (*(_data_area + i + (tmp - 2) * 2048) == 0 )
						return (_data_area + i + (tmp - 2) * 2048);

					if (*(_root_directory + i) == 0xE5)
						return (_data_area + i + (tmp - 2) * 2048);
				}

				//Проверяем по таблице FAT1 есть ли продолжение
				if (*(_FAT1 + tmp) != 0xffff) {
					tmp = *(_FAT1 + tmp);		//временный указатель на позицию равен продолжению
					flag_next = true;
				}
				else flag_next = false;

			} while (flag_next);

		}

		unsigned __int16 free_cluster = 0; //номер свободного кластера

		//Проходим по таблице FAT1 и ищем пустой кластер(если не нашлось свободного места для записи, во всей цепочке кластеров)
		for (int i = 2; i < 512; i++)
			if (*(_FAT1 + i) == 0) {
				*(_FAT1 + i) = 0xffff; //пишем, что она конечная в цепочке
				copy(); // копируем FAT1
				free_cluster = i;

				//Очищаем соответствующий кластер в области данных
				for (int j = 0; j < 2048; j++)
					*(_data_area + (i - 2) * 2048 + j) = 0;

				break;
			}

		//добавляем свободный кластер в цепочку, иначе выводим предупреждение
		if (free_cluster != 0) {
			*(_FAT1 + tmp) = free_cluster;
			copy();

			//Возращаем ссылку на начало нового кластера
			return (_data_area + (free_cluster - 2) * 2048);

		}
		else {
			cout << "The disk is full! Delete unnecessary files." << endl;;
			Sleep(1500);
			return 0;
		}
		return 0;
	}

	/*создание директории*/
	int make_directory(vector<unsigned char> name) {

		unsigned char* free_space;					 //свободное для записи место 
		vector<unsigned char*> all_directory_files;  //вектор указателей на непустые записи
		directory_files(all_directory_files);	

		//проверяем на наличие похожего каталога
		for (unsigned i = 0; i < all_directory_files.size(); i++)
			if (!compare_names(name, all_directory_files[i])) {
				cout << "Directory already exists!" << endl; 
				return 0;
			}

		//зануление 32 байт в корневом каталоге
		free_space = searh_free_space();
		for (int i = 0; i < 32; i++)
			*(free_space + i) = 0;

		//заполнение имени
		for (unsigned i = 0; i < 11; i++)
			*(free_space + i) = name[i];

		//ставим метку каталога
		*(free_space + 11) = 0x10;

		//записываем доли секунды создания директории
		time_t date_create = time(NULL);
		*((int*)(free_space + 14)) = (int)date_create;

		//Проходим по таблице FAT1 и ищем пустой кластер для записи
		unsigned __int16 free_cluster = 0; //номер свободного кластера
		unsigned char* tmp_free_cluster = 0; //временный номер свободного кластера
		bool flag_not_free_clusters = false; //флаг наличия кластеров для записи

		for (int i = 2; i < 512; i++)
			if (*(_FAT1 + i) == 0) {
				*(_FAT1 + i) = 0xffff; //пишем, что она конечная в цепочке
				copy(); // копируем FAT1
				free_cluster = i;
				flag_not_free_clusters = true;

				//Очищаем соответствующий кластер в области данных
				for (int j = 0; j < 2048; j++)
					*(_data_area + (i - 2) * 2048 + j) = 0;

				break;
			}
			
		if (!flag_not_free_clusters) return -1;  //выходим, если нет свободных кластеров

		//заносим младшее слово первого кластера
		*((unsigned __int16*)(free_space + 26)) = free_cluster;

		//ставим 0 размер каталога
		*(free_space + 28) = 0x00;

		//пишем в каталоге файлы "." - сам каталог и ".." - корневой каталог
		tmp_free_cluster = _data_area + (free_cluster - 2) * 2048;

		for (int i = 0; i < 11; i++) *(tmp_free_cluster + i) = 0x20;		//запробеливаем имя
		*tmp_free_cluster = 0x2e;											//имя "."
		*(tmp_free_cluster + 11) = 0x10;									//ставим метку каталога
		*((int*)(tmp_free_cluster + 14)) = (int)date_create;				//время создание каталога
		*((unsigned _int16*)(tmp_free_cluster + 26)) = free_cluster;		//номер кластера, в котором находится

		tmp_free_cluster += 32;
		for (int i = 0; i < 11; i++) *(tmp_free_cluster + i) = 0x20;		//запробеливаем имя
		*tmp_free_cluster = 0x2e; *(tmp_free_cluster + 1) = 0x2e;			//имя ".."
		*(tmp_free_cluster + 11) = 0x10;									//ставим метку каталога
		*((int*)(tmp_free_cluster + 0xe)) = (int)date_create;		//время создание каталога
		*((unsigned short int*)(tmp_free_cluster + 26)) = _place_indicator; //номер коневого каталога, в котором находится

		return 1;

	}

	/*создание файла*/
	int make_file(vector<unsigned char> name) {

		unsigned char* free_space;					 //свободное для записи место 
		vector<unsigned char*> all_directory_files;  //вектор указателей на непустые записи
		directory_files(all_directory_files);

		//проверяем на наличие похожего файла
		for (unsigned int i = 0; i < all_directory_files.size(); i++)
			if (!compare_names(name, all_directory_files[i])) {
				cout << "File already exists!" << endl;
				return 0;
			}

		//зануление 32 байт в корневом каталоге
		free_space = searh_free_space();
		for (int i = 0; i < 32; i++)
			*(free_space + i) = 0;

		//заполнение имени
		for (unsigned i = 0; i < 11; i++)
			*(free_space + i) = name[i];

		//записываем время создания
		*((int*)(free_space + 14)) = (int)time(NULL);

		//Проходим по таблице FAT1 и ищем пустой кластер для записи
		unsigned __int16 free_cluster = 0; //номер свободного кластера
		bool flag_not_free_clusters = false; //флаг наличия кластеров для записи

		for (int i = 2; i < 512; i++)
			if (*(_FAT1 + i) == 0) {
				*(_FAT1 + i) = 0xffff; //пишем, что она конечная в цепочке
				copy();		// копируем FAT1
				free_cluster = i;
				flag_not_free_clusters = true;

				//Очищаем соответствующий кластер в области данных
				for (int j = 0; j < 2048; j++)
					*(_data_area + (i - 2) * 2048 + j) = 0;

				break;
			}
			
		if (!flag_not_free_clusters) return -1; //выходим, если нет свободных кластеров

		//заносим младшее слово первого кластера
		*((unsigned __int16*)(free_space + 26)) = free_cluster;

		//заполнение размера файла
		*((int*)(free_space + 28)) = auto_fail_creater(free_cluster);

		return 1;
	}

	/*создание файла или директории*/
	void make_a_directory_or_file(string str) {
		string n;						//имя файла/директории 
		string e;						//разрешение файла
		bool f_file = false;			//флаг файла

		name_separation(str, n, e); // разделение имени на составляющие

		//задаём флаг, если создаваемый объект файл
		if (e.size() > 0)
			f_file = true;

		vector<unsigned char> name;

		//создание имени (файла, директории)
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

		//создание файла иди директории
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

	/*переход в другую директорию*/
	int open_directory(vector<unsigned char> name) {
		bool f_find = false;						 //флаг поиска
		unsigned char* find_direction_or_file;		 //найденнный файл или директория
		vector<unsigned char*> all_directory_files;  //вектор указателей на непустые записи
		directory_files(all_directory_files);

		//проверяем на наличие похожего каталога
		for (unsigned i = 0; i < all_directory_files.size(); i++)
			if (!compare_names(name, all_directory_files[i])) {
				find_direction_or_file = all_directory_files[i];
				f_find = true;  break;
			}

		if (!f_find) return -1;

		_place_indicator = *((unsigned __int16*)(find_direction_or_file + 26)); //поменяли текущее местоположение (текущий кластер)

		return 1;

	}

	/*открытие файла*/
	int open_file(vector<unsigned char> name) {
		do {
			bool f_find = false;						 //флаг поиска
			unsigned char* find_direction_or_file;		 //найденнный файл или директория
			vector<unsigned char*> all_directory_files;  //вектор указателей на непустые записи
			directory_files(all_directory_files);

			//проверяем на наличие похожего каталога
			for (unsigned i = 0; i < all_directory_files.size(); i++)
				if (!compare_names(name, all_directory_files[i])) {
					find_direction_or_file = all_directory_files[i];
					f_find = true;  break;
				}

			if (!f_find) return -1; //если не нашли, выходим

			int file_size = *((int*)(find_direction_or_file + 28)); // размер файла
			unsigned __int16 first_cluster = *((unsigned __int16*)(find_direction_or_file + 26)); //первых кластер с данными

			//выводим файл
			system("cls");

			//выводим имя
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

	/*открытие директории или файла*/
	int open_a_directory_or_file(string str){
		string n;						//имя файла/директории 
		string e;						//разрешение файла
		bool f_file = false;			//флаг файла

		name_separation(str, n, e); // разделение имени на составляющие

									//задаём флаг, если создаваемый объект файл
		if (e.size() > 0)
			f_file = true;

		vector<unsigned char> name;

		//создание имени (файла, директории)
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

		//открытие файла или директории
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

	/*удаление директории или файла*/
	void delete_a_directory_or_file(string str) {
		string n;						//имя файла/директории 
		string e;						//разрешение файла

		bool f_file = false;			//флаг файла
		bool f_find = false;			//флаг поиска

		unsigned char* find_direction_or_file;		 //найденнный файл или директория
		vector<unsigned char*> all_directory_files;  //вектор указателей на непустые записи
		directory_files(all_directory_files);

		name_separation(str, n, e); // разделение имени на составляющие

		//задаём флаг, если создаваемый объект файл
		if (e.size() > 0)
			f_file = true;

		vector<unsigned char> name;

		//создание имени (файла, директории)
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

		//удаление файла или директории
		if (n == ".." || n == ".") {
			cout << "The file to be deleted is the system file!" << endl;
			Sleep(1500);
			return;
		}
		else {
			//проверяем на наличие похожего каталога или файла
			for (unsigned i = 0; i < all_directory_files.size(); i++)
				if (!compare_names(name, all_directory_files[i])) {
					find_direction_or_file = all_directory_files[i];
					f_find = true;
					break;
				}

			// выходим если файл не найден
			if (!f_find) {
				cout << "Directory (file) for deletion not found!" << endl;
					Sleep(1500);
					return;
			}

		DO:
			//просматриваем, на чтобы не удалялись системные файлы
			if (*find_direction_or_file == '.') {
				cout << "The file to be deleted is the system file!" << endl;
				Sleep(1500);
			}
				
			//проверяем не удален ли файл ранее, иначе удаляем
			if (*find_direction_or_file != 0xE5 )
				*find_direction_or_file = 0xE5;
			else return;

			//ставим флаг не файл, если удаляемый объект директория
			if (*(find_direction_or_file + 11) == 0x10 || *(find_direction_or_file + 11) == 0x04)
				f_file = false;

			unsigned __int16 cluster = *((unsigned __int16*)(find_direction_or_file + 26));
			goto DL;

		DL:
			//если директория, то переходим на проверку объекта и удаление
			if (!f_file) {
				for (int i = 0; i < 2048; i += 32) {
					cluster = *((unsigned char*)(_data_area + i + (cluster - 2) * 2048));
					goto DO;
				}
			}

			//если есть следующий кластер то удаляем кластер
			if (*(_FAT1 + cluster) != 0xFFFF) {
				cluster = *(_FAT1 + cluster);
				goto DL;
			}

			*(_FAT1 + cluster) = 0;
			copy();

		}
		
	}

	/*смена атрибута скрытности для файла или директории*/
	void hide_show_the_directory_or_file(string str, bool operation) {
		string n;						//имя файла/директории 
		string e;						//разрешение файла

		bool f_file = false;			//флаг файла
		bool f_find = false;			//флаг поиска

		unsigned char* find_direction_or_file;		 //найденнный файл или директория
		vector<unsigned char*> all_directory_files;  //вектор указателей на непустые записи
		directory_files(all_directory_files);

		name_separation(str, n, e); // разделение имени на составляющие

		//задаём флаг, если создаваемый объект файл
		if (e.size() > 0)
			f_file = true;

		vector<unsigned char> name;

		//создание имени (файла, директории)
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

		//проверяем на наличие похожего каталога или файла
		for (unsigned i = 0; i < all_directory_files.size(); i++)
			if (!compare_names(name, all_directory_files[i])) {
				find_direction_or_file = all_directory_files[i];
				f_find = true;
				break;
			}

		// выходим если файл не найден
		if (!f_find ) {
			cout << "Directory (file) for attribute change not found!" << endl;
			Sleep(1500);
			return;
		}

		//скрываем файл
		if(f_file && operation ){
			*(find_direction_or_file + 11) = 0x01;
			return;
		}

		//показываем файл
		if (f_file && !operation) {
			*(find_direction_or_file + 11) = 0x00;
			return;
		}

		//скрываем директорию
		if (!f_file && operation && (n != "." || n != "..")) {
			*(find_direction_or_file + 11) = 0x04;
			return;
		}

		//показываем директорию
		if (!f_file && !operation && (n != "." || n != "..")) {
			*(find_direction_or_file + 11) = 0x10;
			return;
		}

	}

	/*вывод на экран*/
	void display_file_manager(vector<string> way) {
		system("cls");

		//верхняя часть графического интерфейса
		cout << (char)0xC9;
		for (int i = 0; i < 54; i++) cout << (char)0xCD;
		cout << (char)0xBB;
		cout << endl;

		cout << (char)0xBA << "                  File manager FAT16                  " << (char)0xBA << endl;

		cout << (char)0xCC;
		for (int i = 0; i < 54; i++) cout << (char)0xCD;
		cout << (char)0xB9;
		cout << endl;

		vector<unsigned char*> all_directory_files;  //вектор указателей на непустые записи
		directory_files(all_directory_files);

		//просматриваем количество записей
		if (all_directory_files.size() == 0)
			cout << (char)0xBA << "                  The disk is empty.                  " << (char)0xBA << endl;
		else
			//выводим все файлы, кроме скрытых
			for (unsigned i = 0; i < all_directory_files.size(); i++) {
				int name_size = 0;
				int size_digits = 0;

				//пропускаем вывод директории или файла, если он скрыт
				if (*(all_directory_files[i] + 11) == 0x01 || *(all_directory_files[i] + 11) == 0x04 ) continue;

				cout << (char)0xBA << " "; //табличный элемент

				//выводим имя без пробелов
				for (int j = 0; j < 8 && *(all_directory_files[i] + j) != 0x20; j++) {
					cout << *(all_directory_files[i] + j);
					name_size++;
				}

				//если это файл
				if (*(all_directory_files[i] + 8) != 0x20) {
					cout << ".";
					name_size++;
					for (int j = 0; j < 3 && *(all_directory_files[i] + j + 8) != 0x20; j++) {
						cout << *(all_directory_files[i] + j + 8);
						name_size++;
					}
				}

				//дополняем пробелами до длины 12 (нужно для красивого вывода)
				for (int j = name_size; j <= 12; j++)
					cout << " ";

				//выводим время создания
				char* str = asctime(localtime(&*(time_t*)(all_directory_files[i] + 14)));
				cout << "   ";
				for (int j = 0; *(str + j) != '\n'; j++)
					cout << *(str + j);

				//выводим размер		
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

	/*файловый менеджер*/
	void file_manager(disk& C) {
		vector<string> way;
		way.push_back("C:/");

		while(1) {
			display_file_manager(way);
			string str;
			cin >> str;

			/*создание файла/директории*/
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

			/*выход из программы*/
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