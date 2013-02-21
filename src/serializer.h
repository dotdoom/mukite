#ifndef SERIALIZER_H
#define SERIALIZER_H

#define SERIALIZE_BASE(value) \
	fwrite(&(value), sizeof(value), 1, output)

#define DESERIALIZE_BASE(value) \
	fread(&(value), sizeof(value), 1, input)

#endif
