#ifndef _ssh_ftp_h_
#define _ssh_ftp_h_

typedef struct Attrib Attrib;

/* File attributes */
struct Attrib {
	u_int32_t	flags;
	u_int64_t	size;
	u_int32_t	uid;
	u_int32_t	gid;
	u_int32_t	perm;
	u_int32_t	atime;
	u_int32_t	mtime;
};

#include "buffer.h"
#include "sftp-common.h"

/* version */
#define	SSH2_FILEXFER_VERSION		3

/* client to server */
#define SSH2_FXP_INIT			1
#define SSH2_FXP_OPEN			3
#define SSH2_FXP_CLOSE			4
#define SSH2_FXP_READ			5
#define SSH2_FXP_WRITE			6
#define SSH2_FXP_LSTAT			7
#define SSH2_FXP_FSTAT			8
#define SSH2_FXP_SETSTAT		9
#define SSH2_FXP_FSETSTAT		10
#define SSH2_FXP_OPENDIR		11
#define SSH2_FXP_READDIR		12
#define SSH2_FXP_REMOVE			13
#define SSH2_FXP_MKDIR			14
#define SSH2_FXP_RMDIR			15
#define SSH2_FXP_REALPATH		16
#define SSH2_FXP_STAT			17
#define SSH2_FXP_RENAME			18
#define SSH2_FXP_READLINK		19
#define SSH2_FXP_SYMLINK		20

/* server to client */
#define SSH2_FXP_VERSION		2
#define SSH2_FXP_STATUS			101
#define SSH2_FXP_HANDLE			102
#define SSH2_FXP_DATA			103
#define SSH2_FXP_NAME			104
#define SSH2_FXP_ATTRS			105

#define SSH2_FXP_EXTENDED		200
#define SSH2_FXP_EXTENDED_REPLY		201

/* attributes */
#define SSH2_FILEXFER_ATTR_SIZE		0x00000001
#define SSH2_FILEXFER_ATTR_UIDGID	0x00000002
#define SSH2_FILEXFER_ATTR_PERMISSIONS	0x00000004
#define SSH2_FILEXFER_ATTR_ACMODTIME	0x00000008
#define SSH2_FILEXFER_ATTR_EXTENDED	0x80000000

/* portable open modes */
#define SSH2_FXF_READ			0x00000001
#define SSH2_FXF_WRITE			0x00000002
#define SSH2_FXF_APPEND			0x00000004
#define SSH2_FXF_CREAT			0x00000008
#define SSH2_FXF_TRUNC			0x00000010
#define SSH2_FXF_EXCL			0x00000020

/* status messages */
#define SSH2_FX_OK			0
#define SSH2_FX_EOF			1
#define SSH2_FX_NO_SUCH_FILE		2
#define SSH2_FX_PERMISSION_DENIED	3
#define SSH2_FX_FAILURE			4
#define SSH2_FX_BAD_MESSAGE		5
#define SSH2_FX_NO_CONNECTION		6
#define SSH2_FX_CONNECTION_LOST		7
#define SSH2_FX_OP_UNSUPPORTED		8
#define SSH2_FX_MAX			8

typedef struct SFTP_DIRENT SFTP_DIRENT;

struct SFTP_DIRENT {
	char *filename;
	char *longname;
	Attrib a;
};

int ssh_connect(char **args, int *in, int *out, pid_t *sshpid);
char **ssh_make_args(char ***args, char *add_arg);
int ssh_cmd(Buffer *m);
int ssh_reply(Buffer *m);
int ssh_init(void);
char *ssh_realpath(char *path);
Attrib *ssh_stat(const char *path);
Attrib *do_lstat(const char *path);
Attrib *do_fstat(char *handle, u_int handle_len);
int ssh_setstat(const char *path, Attrib *a);
char *ssh_get_handle(u_int expected_id, u_int *len);
int ssh_readdir(char *path, SFTP_DIRENT ***dir);
void ssh_free_dirents(SFTP_DIRENT **s);
int ssh_close(char *handle, u_int handle_len);
Attrib *ssh_get_decode_stat(u_int expected_id);
void ssh_send_string_request(u_int id, u_int code, const char *s, u_int len);
void ssh_send_string_attrs_request(u_int id, u_int code, const char *s,
								   u_int len, Attrib *a);
u_int ssh_get_status(int expected_id);
int ssh_recv_binary(const char *remote_path, FILE *local_fp,
					ftp_transfer_func hookf, u_int64_t offset);
int ssh_send_binary(const char *remote_path, FILE *local_fp,
					ftp_transfer_func hookf, u_int64_t offset);
char *ssh_readlink(char *path);

#endif
