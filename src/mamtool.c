#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>

#include "uscsilib.h"

static const char *default_tape = "/dev/enrst0";

struct uscsi_dev dev;

#define CDB_OPCODE		0
#define CDB_RDATTR_SVCACTION	1 
#define CDB_RDATTR_ID_MSB	8
#define CDB_RDATTR_ID_LSB	9
#define CDB_RDATTR_ALLOCLEN_LSB	13

#define OP_READ_ATTRIBUTE	0x8C

#define RDATTR_LISTHEAD_LEN	4	/* attribute list has 4 byte header */
#define RDATTR_ATTRHEAD_LEN	5	/* every attribute has 5 byte header */
#define RDATTR_HEADONLY_LEN	RDATTR_LISTHEAD_LEN+RDATTR_ATTRHEAD_LEN	

#define RDATTR_ATTRHEAD_ID_MSB		0
#define RDATTR_ATTRHEAD_ID_LSB		1
#define RDATTR_ATTRHEAD_FORMAT		2
#define RDATTR_ATTRHEAD_ATTRLEN_MSB	3
#define RDATTR_ATTRHEAD_ATTRLEN_LSB	4

#define ATTR_FORMAT_MASK		0x03
#define ATTR_RO_MASK			0x80

#define ATTR_TYPE_ASCII		0
#define ATTR_TYPE_BINARY	1
#define ATTR_TYPE_TEXT		2

struct mam_attribute {
	uint16_t id;
	uint16_t length;
	uint8_t type;
	bool ro;
	void *value;
};

static inline uint16_t
attribute_lenght_from_head(uint8_t *buf)
{
	return (buf[RDATTR_ATTRHEAD_ATTRLEN_MSB] << 8) 
	    | buf[RDATTR_ATTRHEAD_ATTRLEN_LSB];
}

static inline uint8_t
attribute_type_from_head(uint8_t *buf)
{
	return (buf[RDATTR_ATTRHEAD_FORMAT] & ATTR_FORMAT_MASK);
}

static inline bool
attribute_ro_from_head(uint8_t *buf)
{
	return (buf[RDATTR_ATTRHEAD_FORMAT] & ATTR_RO_MASK);
}

void
cdb_read_attribute(scsicmd *cmd, uint16_t id, uint16_t length)
{
	memset(*cmd, 0, SCSI_CMD_LEN);

	(*cmd)[CDB_OPCODE]		= OP_READ_ATTRIBUTE; 
	(*cmd)[CDB_RDATTR_ID_MSB]	= (id >> 8) & 0xff;
	(*cmd)[CDB_RDATTR_ID_LSB]	= id & 0xff;
	(*cmd)[CDB_RDATTR_ALLOCLEN_LSB]	= length;
}

int
mam_read_attribute_1(struct mam_attribute *ma, uint16_t id) 
{
	int error;
	uint8_t i;
	uint8_t buf[256];
	uint16_t attrlen;

	scsicmd cmd;

	memset(buf, 0, sizeof(buf));

	uscsilib_verbose = 1;

	cdb_read_attribute(&cmd, id, RDATTR_HEADONLY_LEN);

	error = uscsi_command(SCSI_READCMD, &dev, cmd, 16, &buf,
	    RDATTR_HEADONLY_LEN, 10000, NULL);
	
	if (error)
		return error;

	attrlen = attribute_lenght_from_head(&buf[RDATTR_LISTHEAD_LEN]);

	cdb_read_attribute(&cmd, id, RDATTR_HEADONLY_LEN+attrlen);

	error = uscsi_command(SCSI_READCMD, &dev, cmd, 16, &buf,
	    RDATTR_HEADONLY_LEN+attrlen, 10000, NULL);

	if (error)
		return error;

	for(i = 0; i < 255; i++) {
		printf("%x ", buf[i]);
	}
	printf("\n");

	return 0;

}

int
main(int argc, char *argv[])
{
	int error;

	struct uscsi_addr saddr;

	struct mam_attribute ma;

	dev.dev_name = strdup(default_tape);
	printf("Opening device %s\n", dev.dev_name);

	error = uscsi_open(&dev);
	if (error) {
		fprintf(stderr, "Device failed to open : %s\n",
		strerror(error));
		exit(1);
	}


	error = uscsi_check_for_scsi(&dev);
	if (error) {
		fprintf(stderr, "sorry, not a SCSI/ATAPI device : %s\n",
		    strerror(error));
		exit(1);
	}

	error = uscsi_identify(&dev, &saddr);
	if (error) {
		fprintf(stderr, "SCSI/ATAPI identify returned : %s\n",
		    strerror(error));
		exit(1);
	}

	printf("\nDevice identifies itself as : ");
	if (saddr.type == USCSI_TYPE_SCSI) {
		printf("SCSI   busnum = %d, target = %d, lun = %d\n",
		    saddr.addr.scsi.scbus, saddr.addr.scsi.target,
		    saddr.addr.scsi.lun);
	} else {
		printf("ATAPI  busnum = %d, drive = %d\n",
		    saddr.addr.atapi.atbus, saddr.addr.atapi.drive);
	}

	printf("\n");

	mam_read_attribute_1(&ma, 0x400);

	uscsi_close(&dev);
	return 0;

}

