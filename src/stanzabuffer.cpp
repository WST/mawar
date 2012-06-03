
#include <stanzabuffer.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "config.h"

#ifdef DUMP_IO
#include <string>
#endif

/**
* Конструктор буфера
* @param fdmax число поддерживаемых дескрипторов
* @param bufsize размер буфера
*/
StanzaBuffer::StanzaBuffer(size_t fdmax, size_t bufsize)
{
	fd_max = fdmax;
	fds = new fd_info_t[fd_max];
	fd_info_t *fb, *fd_limit = fds + fd_max;
	for(fb = fds; fb < fd_limit; fb++)
	{
		fb->size = 0;
		fb->offset = 0;
		fb->quota = 0;
		fb->first = 0;
		fb->last = 0;
	}
	
	free = size = bufsize;
	buffer = new block_t[size];
	stack = 0;
	block_t *block, *limit = buffer + size;
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
* @param size требуемый размер в байтах
* @return список блоков или NULL если невозможно выделить запрощенный размер
*/
StanzaBuffer::block_t* StanzaBuffer::allocBlocks(size_t size)
{
	// размер в блоках
	size_t count = (size + STANZABUFFER_BLOCKSIZE - 1) / STANZABUFFER_BLOCKSIZE;
	
	if ( count == 0 ) return 0;
	
	if ( mutex.lock() )
	{
		block_t *block = 0;
		if ( count <= free )
		{
			block = stack;
			if ( count > 1 ) for(size_t i = 0; i < (count-1); i++) stack = stack->next;
			block_t *last = stack;
			stack = stack->next;
			last->next = 0;
			free -= count;
		}
		mutex.unlock();
		
		return block;
	}
	
	return 0;
}

/**
* Освободить цепочку блоков
* @param top цепочка блоков
*/
void StanzaBuffer::freeBlocks(block_t *top)
{
	if ( top == 0 ) return;
	block_t *last = top;
	size_t count = 1;
	while ( last->next ) { count++; last = last->next; }
	while ( 1 )
	{
		if ( mutex.lock() )
		{
			last->next = stack;
			stack = top;
			free += count;
			mutex.unlock();
			return;
		}
	}
}

/**
* Включить компрессию
*/
bool StanzaBuffer::enableCompression(int fd, fd_info_t *fb)
{
	if ( ! fb->compression )
	{
		z_stream *strm = &(fb->strm);
		memset(strm, 0, sizeof(fb->strm));
		int status = deflateInit(strm, CONFIG_ZLIB_LEVEL);
		if ( status != Z_OK )
		{
			(void)deflateEnd(strm);
			return false;
		}
		
		fb->compression = true;
	}
	return true;
}

/**
* Отключить компрессию
*/
bool StanzaBuffer::disableCompression(int fd, fd_info_t *fb)
{
	if ( fb->compression )
	{
		z_stream *strm = &(fb->strm);
		(void)deflateEnd(strm);
		
		fb->compression = false;
	}
	
	return true;
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
* Вернуть флаг компрессии
* @param fd файловый дескриптор
* @return TRUE - компрессия включена, FALSE - компрессия отключена
*/
bool StanzaBuffer::getCompression(int fd)
{
	return ( fd >= 0 && fd < fd_max ) ? fds[fd].compression : false;
}

/**
* Включить/отключить компрессию
* @param fd файловый дескриптор
* @param state TRUE - включить компрессию, FALSE - отключить компрессию
* @return TRUE - операция успешна, FALSE - операция прошла с ошибкой
*/
bool StanzaBuffer::setCompression(int fd, bool state)
{
	if ( fd >= 0 && fd < fd_max )
	{
		if ( state ) return enableCompression(fd, &fds[fd]);
		return disableCompression(fd, &fds[fd]);
	}
	
	return false;
}

/**
* Добавить данные в буфер (thread-unsafe)
*
* Если включена компрессия, то сначала сжать данные
*
* @param fd файловый дескриптор
* @param fb указатель на описание файлового буфера
* @param data указатель на данные
* @param len размер данных
* @return TRUE данные приняты, FALSE данные не приняты - нет места
*/
bool StanzaBuffer::put(int fd, fd_info_t *fb, const char *data, size_t len)
{
	char buf[STANZABUFFER_BLOCKSIZE];
	
	if ( fb->compression )
	{
		size_t out_len = 0;
		z_stream *strm = &(fb->strm);
		
		strm->next_in = (unsigned char*)data;
		strm->avail_in = len;
		
		
		while ( strm->avail_out == 0 )
		{
			strm->next_out = (unsigned char*)buf;
			strm->avail_out = sizeof(buf);
			
			deflate(strm, Z_PARTIAL_FLUSH);
			
			size_t have = sizeof(buf) - strm->avail_out;
			out_len += have;
			
			if ( ! putRaw(fd, fb, buf, have) ) return false;
			
		}
		
		strm->avail_out = 0;
		
		float ratio = static_cast<float>(len) / out_len;
		printf("compression ratio: %d / %d = %.2f\n", len, out_len, ratio);
		
		return true;
	}
	else
	{
		return putRaw(fd, fb, data, len);
	}
}

/**
* Добавить данные в буфер (thread-unsafe)
*
* Записать данные как есть без какой-либо обработки
*
* @param fd файловый дескриптор
* @param fb указатель на описание файлового буфера
* @param data указатель на данные
* @param len размер данных
* @return TRUE данные приняты, FALSE данные не приняты - нет места
*/
bool StanzaBuffer::putRaw(int fd, fd_info_t *fb, const char *data, size_t len)
{
	if ( fb->quota != 0 && (fb->size + len) > fb->quota )
	{
		// превышение квоты
		return false;
	}
	
	block_t *block;
	
	if ( fb->size > 0 )
	{
		// смещение к свободной части последнего блока или 0, если последний
		// блок заполнен полностью
		size_t offset = (fb->offset + fb->size) % STANZABUFFER_BLOCKSIZE;
		
		// размер свободной части последнего блока
		size_t rest = offset > 0 ? STANZABUFFER_BLOCKSIZE - offset : 0;
		
		// размер недостающей части, которую надо выделить из общего буфера
		size_t need = len - rest;
		
		if ( len <= rest ) rest = len;
		else
		{
			// выделить недостающие блоки
			block = allocBlocks(need);
			if ( block == 0 ) return false;
		}
		
		// если последний блок заполнен не полностью, то дописать в него
		if ( offset > 0 )
		{
			memcpy(fb->last->data + offset, data, rest);
			fb->size += rest;
			data += rest;
			len -= rest;
			if ( len == 0 ) return true;
		}
		
		fb->last->next = block;
		fb->last = block;
	}
	else // fb->size == 0
	{
		block = allocBlocks(len);
		if ( block == 0 )
		{
			// нет буфера
			return false;
		}
		
		fb->first = block;
		fb->offset = 0;
	}
	
	// записываем полные блоки
	while ( len >= STANZABUFFER_BLOCKSIZE )
	{
		memcpy(block->data, data, STANZABUFFER_BLOCKSIZE);
		data += STANZABUFFER_BLOCKSIZE;
		len -= STANZABUFFER_BLOCKSIZE;
		fb->size += STANZABUFFER_BLOCKSIZE;
		fb->last = block;
		block = block->next;
	}
	
	// если что-то осталось записываем частичный блок
	if ( len > 0 )
	{
		memcpy(block->data, data, len);
		fb->size += len;
		fb->last = block;
	}
	
	return true;
}

/**
* Добавить данные в буфер (thread-safe)
*
* @param fd файловый дескриптор в который надо записать
* @param data указатель на данные
* @param len размер данных
* @return TRUE данные приняты, FALSE данные не приняты - нет места
*/
bool StanzaBuffer::put(int fd, const char *data, size_t len)
{
	// проверяем корректность файлового дескриптора
	if ( fd < 0 || fd >= fd_max )
	{
		// плохой дескриптор
		fprintf(stderr, "StanzaBuffer[%d]: wrong descriptor\n", fd);
		return false;
	}
	
	// проверяем размер, зачем делать лишние движения если len = 0?
	if ( len == 0 ) return true;
	
#ifdef DUMP_IO
	std::string io_dump(data, len);
	fprintf(stdout, "DUMP WRITE[%d]: \033[22;34m%s\033[0m\n", fd, io_dump.c_str());
#endif
	
	// находим описание файлового буфера
	fd_info_t *fb = &fds[fd];
	
	if ( fb->mutex.lock() )
	{
		bool r = put(fd, fb, data, len);
		fb->mutex.unlock();
		return r;
	}
	
	return false;
}

/**
* Добавить данные в буфер как есть (thread-safe)
*
* Данные добавляются в буфер как есть без какой-либо
* обработки типа сжатия и шифрования
*
* @param fd файловый дескриптор в который надо записать
* @param data указатель на данные
* @param len размер данных
* @return TRUE данные приняты, FALSE данные не приняты - нет места
*/
bool StanzaBuffer::putRaw(int fd, const char *data, size_t len)
{
	// проверяем корректность файлового дескриптора
	if ( fd < 0 || fd >= fd_max )
	{
		// плохой дескриптор
		fprintf(stderr, "StanzaBuffer[%d]: wrong descriptor\n", fd);
		return false;
	}
	
	// проверяем размер, зачем делать лишние движения если len = 0?
	if ( len == 0 ) return true;
	
#ifdef DUMP_IO
	std::string io_dump(data, len);
	fprintf(stdout, "DUMP WRITE[%d]: \033[22;34m%s\033[0m\n", fd, io_dump.c_str());
#endif
	
	// находим описание файлового буфера
	fd_info_t *fb = &fds[fd];
	
	if ( fb->mutex.lock() )
	{
		bool r = putRaw(fd, fb, data, len);
		fb->mutex.unlock();
		return r;
	}
	
	return false;
}

/**
* Записать данные из буфера в файл/сокет
*
* @param fd файловый дескриптор
* @return TRUE буфер пуст, FALSE в буфере ещё есть данные
*/
bool StanzaBuffer::push(int fd)
{
	// проверяем корректность файлового дескриптора
	if ( fd < 0 || fd >= fd_max )
	{
		// плохой дескриптор
		fprintf(stderr, "StanzaBuffer[%d]: wrong descriptor\n", fd);
		return false;
	}
	
	// находим описание файлового буфера
	fd_info_t *fb = &fds[fd];
	
	if ( fb->mutex.lock() )
	{
		// список освободившихся блоков
		block_t *unused = 0;
		
		while ( fb->size > 0 )
		{
			// размер не записанной части блока
			size_t rest = STANZABUFFER_BLOCKSIZE - fb->offset;
			if ( rest > fb->size ) rest = fb->size;
			
			// попробовать записать
			ssize_t r = write(fd, fb->first->data + fb->offset, rest);
			if ( r <= 0 ) break;
			
			fb->size -= r;
			fb->offset += r;
			
			// если блок записан полностью,
			if ( r == rest )
			{
				// добавить его в список освободившихся
				block_t *block = fb->first;
				fb->first = block->next;
				fb->offset = 0;
				block->next = unused;
				unused = block;
			}
			else
			{
				// иначе пора прерваться и вернуться в epoll
				break;
			}
		}
		fb->mutex.unlock();
		freeBlocks(unused);
	}
	return fb->size <= 0;
}

/**
* Удалить блоки файлового дескриптора
*
* @param fd файловый дескриптор
*/
void StanzaBuffer::cleanup(int fd)
{
	// проверяем корректность файлового дескриптора
	if ( fd < 0 || fd >= fd_max )
	{
		// плохой дескриптор
		fprintf(stderr, "StanzaBuffer[%d]: wrong descriptor\n", fd);
		return;
	}
	
	fd_info_t *p = &fds[fd];
	disableCompression(fd, p);
	freeBlocks(p->first);
	p->size = 0;
	p->offset = 0;
	p->quota = 0;
	p->first = 0;
	p->last = 0;
}
