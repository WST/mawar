#ifndef MAWAR_STANZABUFFER_H
#define MAWAR_STANZABUFFER_H

#include <sys/types.h>
#include <nanosoft/mutex.h>

/**
* Буфер станз
*/
class StanzaBuffer
{
private:
	/**
	* Структура описывающая один блок буфера
	*/
	struct block_t
	{
		/**
		* Ссылка на следующий блок
		*/
		block_t *next;
		
		/**
		* Данные блока
		*/
		char data[1024];
	};
	
	/**
	* Структура описывающая файловый дескриптор
	*/
	struct fd_info_t
	{
		/**
		* Размер буферизованных данных
		*/
		size_t size;
		
		/**
		* Смещение в блоке к началу не записанных данных
		*/
		size_t offset;
		
		/**
		* Размер квоты для файлового дескриптора
		*/
		size_t quota;
		
		/**
		* Мьютекс для сихронизации файлового буфера
		*/
		nanosoft::Mutex mutex;
		
		/**
		* Указатель на первый блок данных
		*/
		block_t *data;
	};
	
	/**
	* Мьютекс для сихронизации файлового буфера
	*/
	nanosoft::Mutex mutex;
	
	/**
	* Размер буфера в блоках
	*/
	size_t size;
	
	/**
	* Буфер
	*/
	block_t *buffer;
	
	/**
	* Стек свободных блоков
	*/
	block_t *stack;
	
	/**
	* Ограничение на число файловых дескрипторов
	*/
	int fd_max;
	
	/**
	* Таблица файловых дескрипторов
	*
	* хранит указатели на первый блок данных дескриптора или NULL
	* если нет буферизованных данных
	*/
	fd_info_t *fds;
	
	/**
	* Выделить цепочку блоков достаточную для буферизации указаного размера
	*/
	block_t* allocBlocks(size_t size);
	
	/**
	* Освободить цепочку блоков
	*/
	void freeBlocks(block_t *top);
public:
	/**
	* Конструктор буфера
	* @param fdmax число поддерживаемых дескрипторов
	* @param bufsize размер буфера
	*/
	StanzaBuffer(size_t fdmax, size_t bufsize);
	
	/**
	* Деструктор буфера
	*/
	~StanzaBuffer();
	
	/**
	* Вернуть размер буферизованных данных
	* @param fd файловый дескриптор
	* @return размер буферизованных данных
	*/
	size_t getBufferedSize(int fd);
	
	/**
	* Вернуть квоту файлового дескриптора
	* @param fd файловый дескриптор
	* @return размер квоты
	*/
	size_t getQuota(int fd);
	
	/**
	* Установить квоту буфер файлового дескриптора
	* @param fd файловый дескриптор
	* @param quota размер квоты
	* @return TRUE квота установлена, FALSE квота не установлена
	*/
	bool setQuota(int fd, size_t quota);
	
	/**
	* Добавить данные в буфер
	*
	* @param fd файловый дескриптор в который надо записать
	* @param data указатель на данные
	* @param len размер данных
	* @return TRUE данные приняты, FALSE данные не приняты - нет места
	*/
	bool put(int fd, const char *data, size_t len);
	
	/**
	* Записать данные из буфера в файл/сокет
	*
	* @param fd файловый дескриптор
	* @return TRUE буфер пуст, FALSE в буфере ещё есть данные
	*/
	bool push(int fd);
	
	/**
	* Удалить блоки файлового дескриптора
	*
	* @param fd файловый дескриптор
	*/
	void cleanup(int fd);
};

#endif // MAWAR_STANZABUFFER_H
