/*
 * key_talk.c
 *
 *  Created on: May 13, 2016
 *      Author: aliu
 */

#include "key_talk.h"

// Define system parameters
#define TIMEOUT_SEC 0
#define TIMEOUT_USEC 150000

// String Parsing Defines
#define DELIMITER ","

// Define commands
#define CMD_LANGUAGE "SW,ED,1\n"
#define CMD_GET_MEASUREMEMT_ALL "MA\n"

// Define responses
#define RESP_LANGUAGE "SW,ED\n"
#define RESP_GET_MM "MM"
#define RESP_GET_MA "MA"

/*********************************************
 * <Keyence TM-3000 I/O Functions>
 *********************************************/
// Non-Static Exposed Library Functions
int key_connect(char * port, speed_t baud_rate);
int get_measurement_single(const int fd, double m_out);
int get_measurement_all(const int fd, double * m_out);
// Static Library Back-end functions
static int _config_fd(const int fd, speed_t baud_rate);
static int _check_if_keyence(const int fd);
static int _write_port(const int fd, const char * cmd);
static int _read_port(const int fd, char * buff);
static int _create_MM_header(const int sel_out, char * buff);
static int _parse_MM(char * buff, double m_out);
static int _parse_MA(char * buff, double * m_out);
/*********************************************
 * </Keyence TM-3000 I/O Functions>
 *********************************************/


/*********************************************
 *  <Keyence Library Setting Functions>
 *********************************************/
// Non-Static Exposed Library Functions
void key_debug(bool _state);
/*********************************************
 *  </Keyence Library Setting Functions>
 *********************************************/

// define library-wide functional variables
static bool _debug = false;

void key_debug(bool _state){
	_debug = _state;
	if (_state == true){
		printf ("************************************************\n");
		printf ("* Debug Mode Initiated :: VERBOSE OUTPUT\n");
		printf ("************************************************\n");
	}

	printf("");
}

int key_connect(char * port, speed_t baud_rate){
	if (_debug)
		printf(" > CONNECTING TO TM-3000...\n");

	char path[] = "/dev/";
	int len_port = strlen(port);
	int len_path = strlen(path);

	char buff[len_port + len_path];
	bzero(buff);
	sprintf(buff, "%s%s", path, port);

	int fd = open(strcat(path, port), O_RDWR | O_NOCTTY | O_NDELAY | O_NONBLOCK);
	if (fd < 0){
		// TODO: Implement ERRNO checking
		return -1;
	}

	int err = _config_fd(fd, baud_rate);
	if (err == -1){
		if (_debug)
			printf(" >> ERROR in 'connect_tm_3000' :: PORT CONFIG FAILED\n");
		return -1;
	}

	err = _check_if_keyencge(fd);
	if (err < 0){
		if (_debug)
			printf(" >> ERROR in 'connect_tm_3000' :: FD DOES NOT POINT TO KEYENCE\n");
		return -1;
	}

	if (_debug)
		printf(" > TM-3000 CONNECTED\n");

	return fd;
}



static int _config_fd(const int fd, const speed_t baud_rate){

	if (_debug)
		printf(" >> SUB_FUNC '_config_fd()' called\n");

	struct termios oldconfig;
	struct termios newconfig;
	memset(&oldconfig, 0, sizeof(oldconfig));
	memset(&newconfig, 0, sizeof(newconfig));

	if (tcgetattr(fd, &newconfig) < 0){
		close(fd);
		if (_debug){
			printf(" >>> ERROR in '_config_fd()' :: tcgetattr()\n");
			printf(" >>> ERRNO: %d\n", errno);
		}
		return -1;
	} else
		memcpy(&oldconfig, &newconfig, sizeof(newconfig));

	fcntl(fd, F_SETFL, FNDELAY);
	cfsetspeed(&newconfig, baud_rate);
	newconfig.c_cflag |= (CREAD | CLOCAL);
	newconfig.c_cflag &= ~CRTSCTS;
	newconfig.c_lflag |= ICANON;
	newconfig.c_lflag &= ~(ECHO | ECHOE);
	newconfig.c_lflag &= ~PARENB;
	newconfig.c_lflag &= ~CSTOPB;
	newconfig.c_lflag &= ~CSIZE;
	newconfig.c_lflag |= CS8;

	if (tcsetattr(fd, TCSANOW, &newconfig) < 0){
		close(fd);
		if (_debug){
			printf(" >>> ERROR in '_config_fd()' :: tcsetattr()\n");
			printf(" >>> ERRNO: %d\n", errno);
		}
		return -1;
	} else
		tcflush(fd, TCIOFLUSH);

	if (_debug)
		printf(" >> SUB_FUNC '_config_fd()' finished\n");

	return fd;
}

static int _check_if_keyence(const int fd){
	char cmd[] = CMD_LANGUAGE;
	int size = strlen(cmd);

	if (_debug)
		printf(" >> SUB_FUNC '_check_if_keyence()' called\n");

	int err = _write_port(fd, cmd);
	if (err == -1){
		// TODO:
		return -1;
	}

	char buff[256];
	bzero(buff, sizeof(buff));
	err = _read_port(fd, buff);
	if (err == -1){
		switch(errno){

		}
		return -1;
	}
	if (strlen(buff) > 0){
		if (strstr(buff, RESP_LANGUAGE) == NULL){
			if (_debug){
				printf(" >>> ERROR in '_check_if_keyence' :: Unexpected Return\n");
				printf(" >>> RECV'D STRING: %s\n", buff);
			}
			return -1;
		}

		if (_debug)
			printf(" >> SUB_FUNC '_check_if_keyence()' finished\n");

		return 0;
	} else {
		if (_debug)
			printf(" >>> ERROR in '_check_if_keyence' :: No Response\n");

		return -1;
	}
}

static int _write_port(const int fd, const char * cmd){
	bool _would_block = true;
	int len = strlen(cmd);

	if (_debug)
		printf(" >> SUB_FUNC '_write_port()' called\n");

	while (_would_block){
		int err = write(fd, cmd, len);
		if (err < len){
			switch(errno){
			case EAGAIN:
				break;
			case EWOULDBLOCK:
				break;
			case EBADF:
				if (_debug){
					printf(" >>> ERROR in '_write_port()' :: 'write()' :: code: EBADF\n");
					printf(" >>> ACTION Please check FD!\n");
				}
				return -1;
			case EIO:
				if (_debug){
					printf(" >>> ERROR in '_write_port()' :: code: EIO\n");
					printf(" >>> ACTION Please check TM-3000 port!\n");
				}
				return -1;
			default:
				if (_debug){
					printf(" >>> ERROR in '_write_port()' :: 'write()' :: code: %d\n", errno);
				}
				return -1;
			}
		}
		_would_block = false;
	}
	if (_debug)
		printf(" >> SUB_FUNC '_write_port()' finished\n");

	return 0;
}

static int _read_port(const int fd, char * buff){
	fd_set rfds;
	struct timeval tv;

	if (_debug)
		printf(" >> SUB_FUNC '_read_port()' called\n");

	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);
	tv.tv_sec = TIMEOUT_SEC;
	tv.tv_usec = TIMEOUT_USEC;

	bool _would_block = true;
	int _timeout = 0;
	while (_would_block && _timeout < 10){
		int err = select(fd+1, &rfds, NULL, NULL, &tv);
		if (err < 0){
			if (errno == EBADF){
				if (_debug){
					printf(" >>> ERROR in '_read_port()' :: 'select()' :: code: EBADF\n");
					printf(" >>> ACTION Please check FD!\n");
				}
				return -1;
			}
			if (_debug){
				printf(" >>> ERROR in '_read_port()' :: 'select()' :: code: %d\n", errno);
			}
			return -1;
		} else if (err == 0){
			_timeout++;
		} else {
			int err = read(fd, buff, sizeof(buff));
			if (err < 0){
				switch(errno){
				case EAGAIN:
					break;
				case EWOULDBLOCK:
					break;
				case EBADF:
					if (_debug){
						printf(" >>> ERROR in '_read_port()' :: 'write()' :: code: EBADF\n");
						printf(" >>> ACTION Please check FD!\n");
					}
					return -1;
				case EIO:
					if (_debug){
						printf(" >>> ERROR in '_read_port()' :: code: EIO\n");
						printf(" >>> ACTION Please check TM-3000 port!\n");
					}
					return -1;
				default:
					if (_debug){
						printf(" >>> ERROR in '_read_port()' :: 'write()' :: code: %d\n", errno);
					}
					return -1;
				}
			}
			_would_block = false;
		}
	}

	if (_debug)
		printf(" >> SUB_FUNC '_read_port()' finished\n");

	return 0;
}

int get_measurement_single(const int fd, const int sel_out, double m_out){
	//TODO:
}

int get_measurement_all(const int fd, double * m_out){
	//TODO:
}

static int _create_MM_header(const int sel_out, char * buff){
	int len = strlen('MM,') + 16 + strlen("\n");

	if (sizeof(buff) < len){
		if (_debug)
			printf(" >>> ERROR in '_create_MM_header()' :: buff arg is of insufficient length\n");
		return -1;
	}
}

static int _parse_MM(char * buff, double m_out){
	if (_debug)
		printf(" >> SUB_FUNC '_parse_MM()' called\n");

	char * tok = strtok(buff, DELIMITER);
	if (tok != strstr(buff, RESP_GET_MM)){
		if (_debug)
			printf(" >>> ERROR in '_parse_MM()' :: Returned String is not of type 'MM'\n");
		return -1;
	}

	tok = strtok(NULL, DELIMITER);
	tok = strtok(NULL, DELIMITER);

	if (tok != NULL)
		m_out = atof(tok);
	if (_debug)
		printf(" >> SUB_FUNC '_parse_MM()' finished\n");
}

static int _parse_MA(char * buff, double * m_out){
	if (_debug)
		printf(" >> SUB_FUNC '_parse_MA()' called\n");

	char * tok = strtok(buff, DELIMITER);
	if (tok != strstr(buff, RESP_GET_MA)){
		if (_debug){
			printf(" >>> ERROR in '_parse_MA()' :: Returned String is not of type 'MM'\n");
		}
		return -1;
	}
	int _counter = 0;
	while (tok != NULL){
		tok = strtok(NULL, DELIMITER);
		if (tok != NULL){
			*(m_out + _counter) = atof(tok);
			_counter++;
		}
	}
	if (_debug)
		printf(" >> SUB_FUNC '_parse_MA()' finished\n");
	return 0;
}

/*
int read_measurement_selected(const int fd, double * out_buff, int out_num){
	uint16_t out_reg = 1;
	out_reg = out_reg << (out_num - 1);

	char cmd[256];
	char buff[1024];
	sprintf(cmd, "MM,%b\n", out_reg);
	int len = strlen(cmd);

	fd_set rfds;
	struct timeval tv;

	FD_ZEOR(&rfds);
	FD_SET(fd, &rfds);
	tv.tv_sec = TIMEOUT_SEC;
	tv.tv_usec = TIMEOUT_USEC;

	int err = write(fd, cmd, len);
	if (err == len){
		err = select(fd + 1, &rfds, NULL, NULL, &tv);
		if (err > 0){
			err = read(fd, buff, sizeof(buff));
			if (err > len){
				char * _init = strstr(buff, "MM,");
				if (_init != )
			}
		}
	}
}



