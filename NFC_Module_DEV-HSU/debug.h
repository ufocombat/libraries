
#ifndef __DEBUG_H__
#define __DEBUG_H__

#define IS_DEBUG        1

#if IS_DEBUG
	#define DMSG(msg)       Serial.print(msg)
	#define DMSG_HEX(n)     Serial.print(' '); Serial.print(n, HEX)
#else
	#define DMSG(msg)
	#define DMSG_HEX(n)
#endif

#endif
