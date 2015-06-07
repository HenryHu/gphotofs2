#ifndef __GPHOTOFS2_UTILS_H_
#define __GPHOTOFS2_UTILS_H_

#include <string>

int Now();
void Error(const std::string& msg);
void Warn(const std::string& msg);
void Debug(const std::string& msg);
off_t SizeToBlocks(off_t size);
int gpresultToErrno(int result);

#endif // __GPHOTOFS2_UTILS_H_
