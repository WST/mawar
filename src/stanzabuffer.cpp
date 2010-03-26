
#include <stanzabuffer.h>
#include <string.h>
#include <stdio.h>

/**
* Конструктор буфера
* @param fdmax число поддерживаемых дескрипторов
* @param bufsize размер буфера
*/
StanzaBuffer::StanzaBuffer(size_t fdmax, size_t bufsize)
{
	fd_max = fdmax;
	fds = new fd_info_t[fd_max];
	fd_info_t *fd, *fd_limit = fds + fd_max;
	for(fd = fds; fd < fd_limit; fd++)
	{
		fd->size = 0;
		fd->offset = 0;
		fd->quota = 0;
		fd->data = 0;
	}
	
	size = bufsize;
	buffer = new block_t[size];
	stack = 0;
	block_t *block, *limit = block + size;
	for(block = buffer; block < limit; block++)
	{
		memset(block->data, 0, sizeof(block->data));
		block->next = stack;
		stack = block;
	}
}

/**
* Деструктор буфера
*/
StanzaBuffer::~StanzaBuffer()
{
	delete [] fds;
	delete [] buffer;
}

/**
* Выделить цепочку блоков достаточную для буферизации указаного размера
*/
StanzaBuffer::block_t* StanzaBuffer::allocBlocks(size_t size)
{
	// TODO
	return 0;
}

/**
* Освободить цепочку блоков
*/
void StanzaBuffer::freeBlocks(block_t *top)
{
	// TODO
}

/**
* Вернуть размер буферизованных данных
* @param fd файловый дескриптор
* @return размер буферизованных данных
*/
size_t StanzaBuffer::getBufferedSize(int fd)
{
	return ( fd >= 0 && fd < fd_max ) ? fds[fd].size : 0;
}

/**
* Вернуть квоту файлового дескриптора
* @param fd файловый дескриптор
* @return размер квоты
*/
size_t StanzaBuffer::getQuota(int fd)
{
	return ( fd >= 0 && fd < fd_max ) ? fds[fd].quota : 0;
}

/**
* Установить квоту буфер файлового дескриптора
* @param fd файловый дескриптор
* @param quota размер квоты
* @return TRUE квота установлена, FALSE квота не установлена
*/
bool StanzaBuffer::setQuota(int fd, size_t quota)
{
	if ( fd >= 0 && fd < fd_max )
	{
		fds[fd].quota = quota;
		return true;
	}
	return false;
}

/**
* Добавить данные в буфер
*
* @param fd файловый дескриптор в который надо записать
* @param data указатель на данные
* @param len размер данных
* @return TRUE данные приняты, FALSE данные не приняты - нет места
*/
bool StanzaBuffer::put(int fd, const char *data, size_t len)
{
	if ( fd < 0 || fd >= fd_max )
	{
		// плохой дескриптор
		printf("StanzaBuffer[%d]: wrong descriptor\n", fd);
		return false;
	}
	
	fd_info_t *p = &fds[fd];
	
	if ( p->quota != 0 && (p->size + len) > p->quota )
	{
		// превышение квоты
		printf("StanzaBuffer[%d]: quota exceed: %d > %d\n", (p->size + len), p->quota);
		return false;
	}
	
	// TODO
	return false;
}

/**
* Записать данные из буфера в файл/сокет
*
* @param fd файловый дескриптор
*/
bool StanzaBuffer::push(int fd)
{
	// TODO
	return true;
}

/**
* Удалить блоки файлового дескриптора
*
* @param fd файловый дескриптор
*/
void StanzaBuffer::cleanup(int fd)
{
	if ( fd >= 0 && fd < fd_max )
	{
		fd_info_t *p = &fds[fd];
		freeBlocks(p->data);
		p->size = 0;
		p->offset = 0;
		p->quota = 0;
		p->data = 0;
	}
}

