#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "_getline.h"

struct stream_s
{
	char		*buffer;
	int			buffer_size;
	int			fd;
	int			eof;
	struct stream_s* next;
};


void stream_cleanup(struct stream_s **streams)
{
	struct stream_s *iterator = *streams;
	while (iterator)
	{
		struct stream_s *next = iterator->next;
		free(iterator->buffer);
		free(iterator);
		iterator = next;
	}
	*streams = NULL;
}

void stream_remove(struct stream_s **streams, struct stream_s *stream)
{
	if (stream)
	{
		if (stream == *streams)
			*streams = stream->next;
		else
		{
			struct stream_s *iterator = *streams;
			while (iterator->next != stream)
				iterator = iterator->next;
			iterator->next = stream->next;
		}
		free(stream->buffer);
		free(stream);
	}
}

struct stream_s *stream_find_by_fd_or_allocate(int fd, struct stream_s **streams, int *stop)
{
	struct stream_s *iterator = NULL;

	iterator = *streams;
	while (iterator && iterator->fd != fd)
		iterator = iterator->next;
	if (!iterator)
	{
		iterator = malloc( sizeof(struct stream_s) );
		*stop |= iterator == NULL;
		if (iterator)
		{
			memset(iterator, 0, sizeof(*iterator));
			iterator->fd   = fd;
			iterator->next = *streams;
			*streams = iterator;
		}
	}
	return iterator;
}

char *stream_split_string(struct stream_s *stream, int at, int *stop)
{
	char	*splitted = NULL;
	int		next_size = stream->buffer_size - at - 1;
	char	*next 		= NULL;

	if (stream->buffer_size)
	{
		splitted = malloc(at + 1);
		*stop |= splitted == NULL;
		if (splitted)
		{
			memcpy(splitted, stream->buffer, at);
			splitted[at] = 0;
		}
	}

	stream->buffer_size = 0;
	if (next_size > 0)
	{
		next = malloc(next_size);
		*stop |= next != NULL;
		if (next)
			memcpy(next, stream->buffer + at + 1, next_size);
		stream->buffer_size = next_size;
	}

	free(stream->buffer);
	stream->buffer = next;

	return splitted;
}

void stream_read_more(struct stream_s *stream, int *stop)
{
	char 	read_buffer[READ_SIZE];
	int		readed;

	readed = read(stream->fd, read_buffer, READ_SIZE);
	stream->eof = readed <= 0;
	if (readed)
	{
		char *concat = malloc(stream->buffer_size + readed);
		*stop |= (concat == NULL);
		if (concat)
		{
			if (stream->buffer_size)
				memcpy(concat, stream->buffer, stream->buffer_size);
			memcpy(concat + stream->buffer_size, read_buffer, readed);
		}
		free(stream->buffer);
		stream->buffer = concat;
		stream->buffer_size += readed;
	}
}

char *_getline(const int fd)
{
	static struct stream_s	*streams = NULL;
	struct stream_s					*stream = NULL;
	char										*line = NULL;
	int											stop = 0;

	if (fd == -1)
		stream_cleanup(&streams);
	else
	{
		stream = stream_find_by_fd_or_allocate(fd, &streams, &stop);
		while ( (line == NULL) && stream && !stop)
		{
			int	index 			= 0;
			for (; (index < stream->buffer_size) && (stream->buffer[index] != '\n'); ++index);

			if ((index != stream->buffer_size) || stream->eof)
				line = stream_split_string(stream, index, &stop);

			if ( stream->eof == 0 )
				stream_read_more(stream, &stop);
			stop |= (stream->eof && (stream->buffer_size == 0));
		}
		if (stop && (line == NULL))
		{
			stream_remove(&streams, stream);
		}

	}
	return line;
}
