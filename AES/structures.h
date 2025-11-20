#ifndef STRUCTURES_H
#define STRUCTURES_H

extern unsigned char s[256];
extern unsigned char inv_s[256];
extern unsigned char mul2[256];
extern unsigned char mul3[256];
extern unsigned char mul9[256];
extern unsigned char mul11[256];
extern unsigned char mul13[256];
extern unsigned char mul14[256];
extern unsigned char rcon[256];

void AddRoundKey(unsigned char* state, unsigned char* roundKey);
void KeyExpansionCore(unsigned char* in, unsigned char i);
void KeyExpansion(unsigned char* inputKey, unsigned char* expandedKeys);

#endif